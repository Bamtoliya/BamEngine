#include "MeshRenderSystem.h"
#include "ComponentEnable.h"
#include "WorldComponents.h"
#include "RenderComponents.h" 
#include "SplineComponent.h"
#include "SplineMeshComponent.h"
#include "RenderCommands.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "PickingManager.h"
#include "ResourceManager.h"
#include "FrustumCuller.h"
#include "SplineMeshDeformCache.h"
#include "AnimationSceneQuery.h"
#include "RaytracingSceneManager.h"

namespace
{
    using namespace Engine;

    struct SplineMeshRenderCache
    {
        std::vector<SplinePoint> Points;
        bool Closed = false;
        mat4 WorldMatrix{ 1.0f };
        SplineMeshComponent Settings;

        bool Attempted = false;
        bool Ready = false;

        std::vector<mat4> Matrices;
        std::vector<StaticDrawCommand> Commands;
    };

    struct SplineMeshSceneCache
    {
        std::unordered_map<entt::entity, SplineMeshRenderCache> Entries;
    };

    bool PrepareSplineMeshCommands(SplineMeshRenderCache& cache, const SplineComponent& spline,
        const SplineMeshComponent& settings, const mat4& worldMatrix,
        Mesh& mesh, MaterialInterface& material, uint32 pickingId)
    {
        cache.Commands.clear();

        if (!spline.IsValid() || mesh.IsDynamic() || mesh.IsSkinned())
            return false;

        const bool unchanged = cache.Attempted &&
            cache.Points == spline.Points && cache.Closed == spline.Closed &&
            cache.WorldMatrix == worldMatrix &&
            cache.Settings.Spacing == settings.Spacing &&
            cache.Settings.StartDistance == settings.StartDistance &&
            cache.Settings.InitialUp == settings.InitialUp &&
            cache.Settings.InstanceScale == settings.InstanceScale;

        if (!unchanged)
        {
            cache.Points = spline.Points;
            cache.Closed = spline.Closed;
            cache.WorldMatrix = worldMatrix;
            cache.Settings = settings;
            cache.Attempted = true;
            cache.Ready = settings.BuildMatrices(spline, worldMatrix, cache.Matrices);
        }

        if (!cache.Ready)
            return false;

        const BoundingBox localBounds(mesh.GetMin(), mesh.GetMax());
        cache.Commands.reserve(cache.Matrices.size());

        for (const auto& matrix : cache.Matrices)
        {
            BoundingBox worldBounds;
            if (!localBounds.TryTransformToAABB(matrix, worldBounds))
            {
                cache.Commands.clear();
                return false;
            }

            StaticDrawCommand command;
            command.mesh = &mesh;
            command.material = &material;
            command.worldMatrix = matrix;
            command.worldBounds = worldBounds;
            command.pickingId = pickingId;
            cache.Commands.push_back(command);
        }

        return true;
    }
}

EResult MeshRenderSystem::Initialize(void* arg)
{
	return EResult::Success;
}

void MeshRenderSystem::Free()
{
}

MeshRenderSystem* MeshRenderSystem::Create(void* arg)
{
	auto* instance = new MeshRenderSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}


void MeshRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
    auto& renderer = Renderer::Get();

    // 1. 활성 뷰포트/패스별 고유 카메라 정보(위치, ProjY) 추출 및 묶음 구성
    struct PassTarget
    {
        RenderPass* pass = nullptr;
        RenderPassID viewID = INVALID_PASS_ID;
        vec3 cameraPos = vec3(0.0f);
        float projY = 1.0f;
        bool hasLODCamera = false;
        size_t viewIndex = SIZE_MAX; // views의 인덱스. 뷰에 속하지 않는 패스는 SIZE_MAX
    };
    vector<PassTarget> targets;
    vector<RenderPassID> views;

    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        if (!viewport.acceptsSceneSubmissions) continue;
        RenderPass* pass = viewport.renderPass;
        if (!pass) continue;

        const ERenderPassType passType = pass->GetPassType();
        const bool isShadowPass = (passType == ERenderPassType::Shadow);

        if (!isShadowPass && !viewport.hasCamera) continue;
        if (passType != ERenderPassType::Geometry && passType != ERenderPassType::Forward &&
            passType != ERenderPassType::ForwardTransparent && !isShadowPass)
            continue;

        auto it = std::find_if(targets.begin(), targets.end(), [pass](const PassTarget& t) {
            return t.pass == pass;
            });
        if (it == targets.end())
        {
            PassTarget target;
            target.pass = pass;
            target.hasLODCamera = viewport.hasLODCamera;
            if (viewport.hasLODCamera)
            {
                target.cameraPos = viewport.lodCamera.cameraPosition;
                target.projY = glm::max(std::abs(viewport.lodCamera.projMatrix[1][1]), 0.1f);
                target.viewID = viewport.viewID;
            }
            if (target.viewID != INVALID_PASS_ID)
            {
                const auto found = std::find(views.begin(), views.end(), target.viewID);
                target.viewIndex = static_cast<size_t>(found - views.begin());
                if (found == views.end()) views.push_back(target.viewID);
            }
            targets.push_back(target);
        }
    }

    if (targets.empty()) return;
    vector<int32> viewLODs(views.size(), -1);

    // 2. 씬 엔티티 순회 및 패스별 독립 LOD 메시 제출
    for (Scene* scene : activeScenes)
    {
        if (!scene) continue;

        auto& sceneWorld = scene->GetRegistry();
        // TLAS를 빌드할 씬의 캐스터만 섀도맵 제외 후보로 표시한다. 함께 활성화된 다른 씬은 섀도맵에 남는다.
        const auto isRaytraced = [scene](RenderPassID viewID)
            {
                return viewID != INVALID_PASS_ID && RaytracingSceneManager::IsCreated() &&
                    RaytracingSceneManager::Get().IsSceneRequested(scene, viewID);
            };
        SplineMeshDeformCache::Prune(sceneWorld);
        auto& splineMeshes = sceneWorld.ctx().emplace<SplineMeshSceneCache>();

        std::erase_if(splineMeshes.Entries, [&sceneWorld](const auto& item)
            {
                return !sceneWorld.valid(item.first) ||
                    !sceneWorld.all_of<SplineComponent, SplineMeshComponent, StaticMeshRendererComponent,
                    WorldTransformComponent, FlagComponent>(item.first) ||
                    !IsComponentEnabled<SplineComponent, SplineMeshComponent, StaticMeshRendererComponent>(sceneWorld, item.first) ||
                    sceneWorld.get<SplineMeshComponent>(item.first).DeformToPath;
            });

        auto view = sceneWorld.view<StaticMeshRendererComponent, WorldTransformComponent, FlagComponent>();
        for (auto [entity, meshRenderer, worldTransform, flag] : view.each())
        {
            if (!IsComponentEnabled<StaticMeshRendererComponent>(sceneWorld, entity)) continue;
            if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() ||
                HasFlag(flag.effectiveFlags, EEntityFlag::Dead))
            {
                continue;
            }

            Mesh* baseMesh = meshRenderer.meshHandle.Get();
            MaterialInterface* material = meshRenderer.materialHandle.Get();
            if (auto* splineMesh = TryGetEnabledComponent<SplineMeshComponent>(sceneWorld, entity))
            {
                using BuildStatus = ESplineMeshBuildStatus;
                const auto finishBuild = [&](BuildStatus status, uint32 count = 0)
                    {
                        splineMesh->LastBuildStatus = status;
                        splineMesh->GeneratedCount = count;
                        splineMesh->HasBuildError = status != BuildStatus::Ready && status != BuildStatus::NoOutput;
                    };

                const auto* spline = TryGetEnabledComponent<SplineComponent>(sceneWorld, entity);
                if (!spline)
                {
                    finishBuild(BuildStatus::InvalidPath);
                    continue;
                }
                if (!baseMesh)
                {
                    finishBuild(BuildStatus::MissingSourceMesh);
                    continue;
                }
                if (!material)
                {
                    finishBuild(BuildStatus::MissingMaterial);
                    continue;
                }

                const uint32 pickingId = PickingManager::Get().RegisterPickingEntity<StaticMeshRendererComponent>(scene, entity);
                StaticDrawCommand deformedCommand;
                std::span<const StaticDrawCommand> commands;

                if (splineMesh->DeformToPath)
                {
                    auto instance = SplineMeshDeformCache::GetOrBuild(sceneWorld, entity, worldTransform.worldMatrix);
                    if (!instance.Geometry)
                    {
                        finishBuild(spline->IsValid() ? instance.BuildStatus : BuildStatus::InvalidPath);
                        continue;
                    }

                    deformedCommand.ownedMesh = std::move(instance.Geometry);
                    deformedCommand.mesh = deformedCommand.ownedMesh.get();
                    deformedCommand.material = material;
                    deformedCommand.worldMatrix = instance.WorldMatrix;
                    deformedCommand.worldBounds = instance.WorldBounds;
                    deformedCommand.pickingId = pickingId;
                    commands = std::span<const StaticDrawCommand>(&deformedCommand, 1);
                }
                else
                {
                    auto& cache = splineMeshes.Entries[entity];
                    if (!PrepareSplineMeshCommands(cache, *spline, *splineMesh, worldTransform.worldMatrix,*baseMesh, *material, pickingId))
                    {
                        finishBuild(spline->IsValid() ? BuildStatus::PlacementFailed : BuildStatus::InvalidPath);
                        continue;
                    }

                    commands = std::span<const StaticDrawCommand>(cache.Commands);
                }

                finishBuild(commands.empty() ? BuildStatus::NoOutput : BuildStatus::Ready, static_cast<uint32>(commands.size()));

                for (const auto& target : targets)
                {
                    if (target.pass->GetPassType() == ERenderPassType::Shadow && !meshRenderer.castShadow)
                        continue;

                    if (!target.pass->IsAcceptsBlendMode(material->GetBlendMode()))
                        continue;

                    for (const auto& command : commands)
                        renderer.SubmitStatic(command, target.pass->GetID());
                }

                continue;
            }
            if (!baseMesh || !material) continue;

            auto* lodGroup = TryGetEnabledComponent<LODGroupComponent>(scene->GetRegistry(), entity);
            const uint32 pickingId = PickingManager::Get().RegisterPickingEntity<StaticMeshRendererComponent>(scene, entity);

            // 정점에 배치가 구워진 메시도 올바르게 판정하도록 엔티티 위치가 아니라 경계 상자 중심에서 거리를 잰다.
            vec3 lodCenter(0.0f);
            float lodRadius = 0.0f;
            LODThresholds lodThresholds;
            if (lodGroup)
            {
                const mat4& worldMatrix = worldTransform.worldMatrix;
                const vec3 boundsMin = baseMesh->GetMin();
                const vec3 boundsMax = baseMesh->GetMax();
                const float maxScale = glm::max(glm::length(vec3(worldMatrix[0])),
                    glm::max(glm::length(vec3(worldMatrix[1])), glm::length(vec3(worldMatrix[2]))));
                lodCenter = vec3(worldMatrix * vec4((boundsMin + boundsMax) * 0.5f, 1.0f));
                lodRadius = glm::length(boundsMax - boundsMin) * 0.5f * maxScale;
                lodThresholds.LOD1Coverage = lodGroup->lod1ScreenCoverage;
                lodThresholds.LOD2Coverage = lodGroup->lod2ScreenCoverage;
                lodThresholds.HysteresisMargin = lodGroup->hysteresisMargin;
                lodThresholds.HasLOD1 = lodGroup->lod1Mesh.Get() != nullptr;
                lodThresholds.HasLOD2 = lodGroup->lod2Mesh.Get() != nullptr;
                std::fill(viewLODs.begin(), viewLODs.end(), -1);
            }
            // 같은 뷰의 GBuffer·포워드·그림자 패스와 TLAS가 같은 LOD를 쓰도록 뷰마다 한 번만 고른다.
            const auto selectLOD = [&](const PassTarget& target) -> int32
                {
                    if (!target.hasLODCamera) return 0;
                    const float coverage = ComputeScreenCoverage(lodRadius, target.projY, glm::distance(target.cameraPos, lodCenter));
                    if (target.viewIndex >= viewLODs.size())
                        return SelectLODIndex(coverage, 0, lodThresholds);
                    int32& selected = viewLODs[target.viewIndex];
                    if (selected < 0)
                    {
                        selected = SelectLODIndex(coverage, lodGroup->viewLODs.Get(target.viewID), lodThresholds);
                        lodGroup->viewLODs.Set(target.viewID, selected);
                    }
                    return selected;
                };

            for (const auto& target : targets)
            {
                if (target.pass->GetPassType() == ERenderPassType::Shadow && !meshRenderer.castShadow)
                    continue;

                if (!target.pass->IsAcceptsBlendMode(material->GetBlendMode()))
                    continue;

                Mesh* activeMesh = baseMesh;

                if (lodGroup)
                {
                    const int32 lod = selectLOD(target);
                    if (lod == 2) activeMesh = lodGroup->lod2Mesh.Get();
                    else if (lod == 1) activeMesh = lodGroup->lod1Mesh.Get();
                }

                StaticDrawCommand command;
                command.mesh = activeMesh;
                command.material = material;
                command.worldMatrix = worldTransform.worldMatrix;

                if (!activeMesh->IsDynamic())
                {
                    const BoundingBox localBounds(activeMesh->GetMin(), activeMesh->GetMax());
                    BoundingBox worldBounds;
                    if (localBounds.TryTransformToAABB(command.worldMatrix, worldBounds))
                        command.worldBounds = worldBounds;
                }

                command.pickingId = pickingId;
                if (target.pass->GetPassType() == ERenderPassType::Shadow && isRaytraced(target.viewID))
                {
                    command.raytracedScene = scene;
                    command.raytracedViewID = target.viewID;
                    command.raytracedEntityIndex = static_cast<uint32>(entt::to_entity(entity));
                }
                renderer.SubmitStatic(command, target.pass->GetID());
            }
        }

        // 3. 스킨드 메시(SkinnedMeshRendererComponent) 순회 및 렌더러 제출
        auto& world = scene->GetRegistry();

        auto skinnedView = world.view<SkinnedMeshRendererComponent, WorldTransformComponent, FlagComponent>();
        for (auto [entity, skinnedRenderer, worldTransform, flag] : skinnedView.each())
        {
            if (!IsComponentEnabled<SkinnedMeshRendererComponent>(world, entity)) continue;
            if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() ||
                HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;

            skinnedRenderer.UpdateTemporaryEffects(dt);

            Mesh* mesh = nullptr;
            MaterialInterface* material = nullptr;
            MaterialConstants constants{};
            skinnedRenderer.ResolveRenderState(mesh, material, constants);
            if (!mesh || !material) continue;

            // 자신 또는 부모(루트) 계층의 AnimatorComponent에서 계산된 본 행렬 목록 참조
            const auto animatorEntity = FindAnimatorEntityInAncestors(world, entity);
            const auto* animator = animatorEntity != entt::null ? TryGetEnabledComponent<AnimatorComponent>(world, animatorEntity) : nullptr;
            const std::vector<mat4>* boneMatrices = (animator && !animator->finalBoneMatrices.empty())
                ? &animator->finalBoneMatrices : nullptr;

            SkinnedDrawCommand command;
            command.mesh = mesh;
            command.material = material;
            command.materialConstants = constants;
            // 스킨드 메시 정점은 본 행렬에 의해 스켈레톤/애니메이터 루트 공간으로 변환되므로,
            // DrawCommand의 월드 행렬은 자식 메시 엔티티가 아닌 애니메이터(루트) 엔티티의 월드 행렬을 사용하여 이중 변환(Double Transform)을 방지합니다.
            mat4 renderWorldMatrix = worldTransform.worldMatrix;
            if (animatorEntity != entt::null)
            {
                if (const auto* animWorld = world.try_get<WorldTransformComponent>(animatorEntity))
                    renderWorldMatrix = animWorld->worldMatrix;
            }

            command.worldMatrix = renderWorldMatrix;
            command.boneMatrices = boneMatrices;
            command.pickingId = PickingManager::Get().RegisterPickingEntity<SkinnedMeshRendererComponent>(scene, entity);

            for (const auto& target : targets)
            {
                if (target.pass->GetPassType() == ERenderPassType::Shadow && !skinnedRenderer.castShadow)
                    continue;
                if (!target.pass->IsAcceptsBlendMode(material->GetBlendMode()))
                    continue;
                renderer.SubmitSkinned(command, target.pass->GetID());
            }
        }
    }
}
