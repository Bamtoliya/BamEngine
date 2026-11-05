#include "MeshRenderSystem.h"
#include "CoreComponents.h"
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
        vec3 cameraPos = vec3(0.0f);
        float projY = 1.0f;
        bool isPrimaryGeometry = false;
    };
    vector<PassTarget> targets;

    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        RenderPass* pass = viewport.renderPass;
        if (!pass) continue;

        const ERenderPassType passType = pass->GetPassType();
        const bool isShadowPass = (passType == ERenderPassType::Shadow);

        if (!isShadowPass && !viewport.hasCamera) continue;
        if (passType != ERenderPassType::Geometry && passType != ERenderPassType::Forward && !isShadowPass)
            continue;

        auto it = std::find_if(targets.begin(), targets.end(), [pass](const PassTarget& t) {
            return t.pass == pass;
            });
        if (it == targets.end())
        {
            PassTarget target;
            target.pass = pass;
            target.cameraPos = viewport.cameraBuffer.cameraPosition;
            target.projY = glm::max(std::abs(viewport.cameraBuffer.projMatrix[1][1]), 0.1f);
            target.isPrimaryGeometry = (passType == ERenderPassType::Geometry);
            targets.push_back(target);
        }
    }

    if (targets.empty()) return;

    // 2. 씬 엔티티 순회 및 패스별 독립 LOD 메시 제출
    for (Scene* scene : activeScenes)
    {
        if (!scene) continue;

        auto& sceneWorld = scene->GetRegistry();
        SplineMeshDeformCache::Prune(sceneWorld);
        auto& splineMeshes = sceneWorld.ctx().emplace<SplineMeshSceneCache>();

        std::erase_if(splineMeshes.Entries, [&sceneWorld](const auto& item)
            {
                return !sceneWorld.valid(item.first) ||
                    !sceneWorld.all_of<SplineComponent, SplineMeshComponent, StaticMeshRendererComponent,
                    WorldTransformComponent, FlagComponent>(item.first) ||
                    sceneWorld.get<SplineMeshComponent>(item.first).DeformToPath;
            });

        auto view = sceneWorld.view<StaticMeshRendererComponent, WorldTransformComponent, FlagComponent>();
        for (auto [entity, meshRenderer, worldTransform, flag] : view.each())
        {
            if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() ||
                HasFlag(flag.effectiveFlags, EEntityFlag::Dead))
            {
                continue;
            }

            Mesh* baseMesh = meshRenderer.meshHandle.Get();
            MaterialInterface* material = meshRenderer.materialHandle.Get();
            if (auto* splineMesh = sceneWorld.try_get<SplineMeshComponent>(entity))
            {
                using BuildStatus = ESplineMeshBuildStatus;
                const auto finishBuild = [&](BuildStatus status, uint32 count = 0)
                    {
                        splineMesh->LastBuildStatus = status;
                        splineMesh->GeneratedCount = count;
                        splineMesh->HasBuildError = status != BuildStatus::Ready && status != BuildStatus::NoOutput;
                    };

                const auto* spline = sceneWorld.try_get<SplineComponent>(entity);
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

                const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);
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

            auto* lodGroup = scene->GetRegistry().try_get<LODGroupComponent>(entity);
            const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

            for (const auto& target : targets)
            {
                if (target.pass->GetPassType() == ERenderPassType::Shadow && !meshRenderer.castShadow)
                    continue;

                if (!target.pass->IsAcceptsBlendMode(material->GetBlendMode()))
                    continue;

                Mesh* activeMesh = baseMesh;

                // LODGroupComponent가 있을 경우 해당 패스 카메라 시점 기준으로 독립 LOD 평가
                if (lodGroup)
                {
                    const vec3 objectPos = vec3(worldTransform.worldMatrix[3]);
                    const float distance = glm::max(glm::distance(target.cameraPos, objectPos), 0.001f);
                    const float maxScale = glm::max(glm::length(vec3(worldTransform.worldMatrix[0])),
                        glm::max(glm::length(vec3(worldTransform.worldMatrix[1])), glm::length(vec3(worldTransform.worldMatrix[2]))));
                    const float localRadius = glm::length(baseMesh->GetMin() - baseMesh->GetMax()) * 0.5f;
                    const float worldRadius = glm::max(localRadius * maxScale, 0.01f);
                    const float coverage = glm::clamp((worldRadius * target.projY) / distance, 0.0f, 1.0f);
                    const float margin = lodGroup->hysteresisMargin;
                    const int32 current = lodGroup->currentLODIndex;

                    float lod2Thresh = lodGroup->lod2ScreenCoverage;
                    if (current == 2) lod2Thresh += margin;
                    float lod1Thresh = lodGroup->lod1ScreenCoverage;
                    if (current == 1) lod1Thresh += (coverage < lod1Thresh ? -margin : margin);

                    int32 targetLOD = 0;
                    if (lodGroup->lod2Mesh.Get() && coverage <= lod2Thresh)
                        targetLOD = 2;
                    else if (lodGroup->lod1Mesh.Get() && coverage <= lod1Thresh)
                        targetLOD = 1;

                    // 주 에디터 메인 카메라에서만 컴포넌트의 영속 관찰 상태 갱신
                    if (target.isPrimaryGeometry) lodGroup->currentLODIndex = targetLOD;

                    if (targetLOD == 2 && lodGroup->lod2Mesh.Get())
                        activeMesh = lodGroup->lod2Mesh.Get();
                    else if (targetLOD == 1 && lodGroup->lod1Mesh.Get())
                        activeMesh = lodGroup->lod1Mesh.Get();
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
                renderer.SubmitStatic(command, target.pass->GetID());
            }
        }

        // 3. 스킨드 메시(SkinnedMeshRendererComponent) 순회 및 렌더러 제출
        auto& world = scene->GetRegistry();
        auto findAnimatorInAncestors = [&world](entt::entity startEntity) -> const AnimatorComponent*
            {
                entt::entity cur = startEntity;
                while (cur != entt::null && world.valid(cur))
                {
                    if (const auto* anim = world.try_get<AnimatorComponent>(cur)) return anim;
                    const auto* hierarchy = world.try_get<HierarchyComponent>(cur);
                    cur = hierarchy ? hierarchy->parent : entt::null;
                }
                return nullptr;
            };

        auto skinnedView = world.view<SkinnedMeshRendererComponent, WorldTransformComponent, FlagComponent>();
        for (auto [entity, skinnedRenderer, worldTransform, flag] : skinnedView.each())
        {
            if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() ||
                HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
            Mesh* mesh = skinnedRenderer.meshHandle.Get();
            MaterialInterface* material = skinnedRenderer.materialHandle.Get();
            if (!mesh || !material) continue;

            // 자신 또는 부모(루트) 계층의 AnimatorComponent에서 계산된 본 행렬 목록 참조
            const auto* animator = findAnimatorInAncestors(entity);
            const std::vector<mat4>* boneMatrices = (animator && !animator->finalBoneMatrices.empty())
                ? &animator->finalBoneMatrices : nullptr;

            SkinnedDrawCommand command;
            command.mesh = mesh;
            command.material = material;
            command.worldMatrix = worldTransform.worldMatrix;
            command.boneMatrices = boneMatrices;
            command.pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

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