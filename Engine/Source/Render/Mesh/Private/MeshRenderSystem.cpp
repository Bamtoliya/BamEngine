#include "MeshRenderSystem.h"
#include "ComponentEnable.h"
#include "WorldComponents.h"
#include "RenderComponents.h" 
#include "SplineComponent.h"
#include "SplineMeshComponent.h"
#include "RenderCommands.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "ResidentMeshLODs.h"
#include "Material.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "PickingManager.h"
#include "ResourceManager.h"
#include "ResourceStreamer.h"
#include "RHI.h"
#include "RHITexture.h"
#include "Shader.h"
#include "StreamingDemand.h"
#include "FrustumCuller.h"
#include "SplineMeshDeformCache.h"
#include "AnimationSceneQuery.h"
#include "RaytracingSceneManager.h"
#include "MotionHistory.h"

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
        // 월드 행렬을 뺀 배치(점·간격·설정)가 바뀔 때마다 올린다. 인스턴스 번호가 가리키는 위치가 달라지므로 모션 벡터의 이전 변환을 쓰지 않게 한다.
        uint64 Revision = 0;

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

        const bool sameLayout = cache.Attempted &&
            cache.Points == spline.Points && cache.Closed == spline.Closed &&
            cache.Settings.Spacing == settings.Spacing &&
            cache.Settings.StartDistance == settings.StartDistance &&
            cache.Settings.InitialUp == settings.InitialUp &&
            cache.Settings.InstanceScale == settings.InstanceScale;
        const bool unchanged = sameLayout && cache.WorldMatrix == worldMatrix;

        if (!sameLayout)
            ++cache.Revision;

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
            command.motionStamp = MakeMotionStamp(&mesh, cache.Revision);
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
    vector<StreamingView> streamingViews; // views와 같은 순서
    const auto* backBuffer = renderer.GetRHI() ? renderer.GetRHI()->GetBackBuffer() : nullptr;

    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        if (!viewport.acceptsSceneSubmissions) continue;
        RenderPass* pass = viewport.renderPass;
        if (!pass) continue;

        const ERenderPassType passType = pass->GetPassType();
        const bool isShadowPass = (passType == ERenderPassType::Shadow);

        if (!isShadowPass && !viewport.hasCamera) continue;
        if (passType != ERenderPassType::Geometry && passType != ERenderPassType::Forward &&
            passType != ERenderPassType::ForwardTransparent && passType != ERenderPassType::Distortion && !isShadowPass)
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
                if (found == views.end())
                {
                    views.push_back(target.viewID);
                    StreamingView streamingView;
                    streamingView.CameraPosition = target.cameraPos;
                    // 원근 투영의 w 부호로 뷰 공간의 앞 방향(+Z 또는 -Z)을 정한다.
                    const bool forwardIsNegativeZ = viewport.lodCamera.projMatrix[2][3] < 0.0f;
                    streamingView.Forward = vec3(viewport.lodCamera.invViewMatrix[2]) * (forwardIsNegativeZ ? -1.0f : 1.0f);
                    streamingView.ProjectionScaleY = target.projY;
                    // 뷰포트는 창보다 크지 않으므로 창 높이를 쓰면 부족하게 올리지 않는다.
                    if (backBuffer) streamingView.ViewHeight = static_cast<f32>(backBuffer->GetHeight());
                    streamingView.Orthographic = viewport.lodCamera.projMatrix[3][3] == 1.0f;
                    streamingViews.push_back(streamingView);
                }
            }
            targets.push_back(target);
        }
    }

    if (targets.empty()) return;
    vector<int32> viewLODs(views.size(), -1);

    auto* streamer = ResourceStreamer::IsCreated() && ResourceStreamer::Get().HasEntries() ? &ResourceStreamer::Get() : nullptr;
    // UV를 그대로 쓰는 엔진 기본 셰이더인지. 다른 셰이더는 UV를 바꿀 수 있어 가장 자세한 밉을 요청한다.
    unordered_map<const Shader*, bool> plainUVShaders;
    const auto usesPlainUV = [&plainUVShaders](const MaterialInterface& material)
        {
            const Shader* shader = material.GetPixelShader(ERenderPassType::Geometry);
            const EBlendMode blendMode = material.GetBlendMode();
            if (!shader || (blendMode != EBlendMode::Opaque && blendMode != EBlendMode::Masked) || !material.GetShaderGraphPath().empty()) return false;
            const auto [found, inserted] = plainUVShaders.try_emplace(shader, false);
            if (inserted) found->second = ToLowerWStr(shader->GetKey()).ends_with(L"gbufferps");
            return found->second;
        };

    // 2. 씬 엔티티 순회 및 패스별 독립 LOD 메시 제출
    for (Scene* scene : activeScenes)
    {
        if (!scene) continue;

        auto& sceneWorld = scene->GetRegistry();
        // 다른 씬의 같은 엔티티 번호와 구분하기 위한 값. 0은 추적하지 않는 물체이므로 피한다.
        const uint32 motionSceneId = static_cast<uint32>(std::hash<const Scene*>{}(scene)) | 1u;
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

                    // 배치 인스턴스와 변형 메시도 월드 행렬만 움직이면 모션 벡터를 만든다. 정점이 변하는 변형 메시는
                    // 렌더러가 메시 주소와 GeometryVersion(ownedMesh)으로 형상이 바뀐 프레임을 가려낸다.
                    uint32 instanceIndex = 0;
                    for (StaticDrawCommand command : commands)
                    {
                        command.receiveDecals = meshRenderer.receiveDecals;
                        command.objectId = { motionSceneId, static_cast<uint32>(entt::to_integral(entity)), instanceIndex++ };
                        renderer.SubmitStatic(command, target.pass->GetID());
                    }
                }

                continue;
            }
            if (!baseMesh || !material) continue;

            auto* lodGroup = TryGetEnabledComponent<LODGroupComponent>(scene->GetRegistry(), entity);
            // 자세한 메시를 읽는 동안에는 준비된 LOD를 대신 그린다.
            const ResidentMeshLODs lodMeshes(baseMesh, lodGroup);
            const Mesh* boundsMesh = lodMeshes.Find(0);
            if (!boundsMesh)
            {
                // 대신 그릴 것이 없는 메시는 크기를 모르므로 가장 먼저 읽는다.
                if (streamer && baseMesh) streamer->RequestMesh(*baseMesh, 1.0f);
                continue;
            }

            const uint32 pickingId = PickingManager::Get().RegisterPickingEntity<StaticMeshRendererComponent>(scene, entity);

            // 정점에 배치가 구워진 메시도 올바르게 판정하도록 엔티티 위치가 아니라 경계 상자 중심에서 거리를 잰다.
            vec3 lodCenter(0.0f);
            float lodRadius = 0.0f;
            LODThresholds lodThresholds;
            if (lodGroup)
            {
                const mat4& worldMatrix = worldTransform.worldMatrix;
                const vec3 boundsMin = boundsMesh->GetMin();
                const vec3 boundsMax = boundsMesh->GetMax();
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
            if (streamer)
            {
                BoundingBox worldBounds;
                const bool hasBounds = BoundingBox(boundsMesh->GetMin(), boundsMesh->GetMax()).TryTransformToAABB(worldTransform.worldMatrix, worldBounds);
                const mat4& matrix = worldTransform.worldMatrix;
                const float scale = glm::max(glm::length(vec3(matrix[0])), glm::max(glm::length(vec3(matrix[1])), glm::length(vec3(matrix[2]))));
                const float uvDensity = hasBounds && usesPlainUV(*material) ? boundsMesh->GetUVDensity() : 0.0f;
                for (size_t viewIndex = 0; viewIndex < streamingViews.size(); ++viewIndex)
                {
                    const auto& streamingView = streamingViews[viewIndex];
                    if (!hasBounds)
                    {
                        streamer->RequestMesh(*baseMesh, 1.0f);
                        streamer->RequestMaterial(*material, 0.0f, 1.0f);
                        continue;
                    }
                    const vec3 center = worldBounds.Center();
                    const float coverage = ComputeScreenCoverage(glm::length(worldBounds.Extent()), streamingView.ProjectionScaleY,
                        streamingView.Orthographic ? 1.0f : glm::distance(streamingView.CameraPosition, center));
                    const float priority = ComputeStreamingPriority(streamingView, worldBounds, coverage);
                    // 준비된 LOD로 바꾸기 전의, 화면에 필요한 LOD를 요청한다.
                    const Mesh* wantedMesh = baseMesh;
                    if (lodGroup)
                    {
                        const float lodCoverage = ComputeScreenCoverage(lodRadius, streamingView.ProjectionScaleY, glm::distance(streamingView.CameraPosition, lodCenter));
                        wantedMesh = lodMeshes.Meshes[SelectLODIndex(lodCoverage, lodGroup->viewLODs.Get(views[viewIndex]), lodThresholds)];
                    }
                    streamer->RequestMesh(*wantedMesh, priority);
                    streamer->RequestMaterial(*material, ComputePixelsPerUV(streamingView, worldBounds, scale, uvDensity), priority);
                }
            }
            // 같은 뷰의 GBuffer·포워드·그림자 패스와 TLAS가 같은 LOD를 쓰도록 뷰마다 한 번만 고른다.
            const auto selectLOD = [&](const PassTarget& target) -> int32
                {
                    if (!target.hasLODCamera) return ResolveResidentLOD(0, lodMeshes.Mask);
                    const float coverage = ComputeScreenCoverage(lodRadius, target.projY, glm::distance(target.cameraPos, lodCenter));
                    if (target.viewIndex >= viewLODs.size())
                        return ResolveResidentLOD(SelectLODIndex(coverage, 0, lodThresholds), lodMeshes.Mask);
                    int32& selected = viewLODs[target.viewIndex];
                    if (selected < 0)
                    {
                        selected = ResolveResidentLOD(SelectLODIndex(coverage, lodGroup->viewLODs.Get(target.viewID), lodThresholds), lodMeshes.Mask);
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
                    activeMesh = lodMeshes.Meshes[selectLOD(target)];
                }

                StaticDrawCommand command;
                command.mesh = activeMesh;
                command.material = material;
                command.worldMatrix = worldTransform.worldMatrix;

                const BoundingBox localBounds(activeMesh->GetMin(), activeMesh->GetMax());
                BoundingBox worldBounds;
                if (localBounds.IsValid() && localBounds.TryTransformToAABB(command.worldMatrix, worldBounds))
                    command.worldBounds = worldBounds;

                command.pickingId = pickingId;
                command.receiveDecals = meshRenderer.receiveDecals;
                command.objectId = { motionSceneId, static_cast<uint32>(entt::to_integral(entity)) };
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
            command.receiveDecals = skinnedRenderer.receiveDecals;
            command.objectId = { motionSceneId, static_cast<uint32>(entt::to_integral(entity)) };

            for (const auto& target : targets)
            {
                if (target.pass->GetPassType() == ERenderPassType::Shadow && !skinnedRenderer.castShadow)
                    continue;
                if (!target.pass->IsAcceptsBlendMode(material->GetBlendMode()))
                    continue;
                SkinnedDrawCommand targetCommand = command;
                if (target.pass->GetPassType() == ERenderPassType::Shadow && isRaytraced(target.viewID))
                {
                    targetCommand.raytracedScene = scene;
                    targetCommand.raytracedViewID = target.viewID;
                    targetCommand.raytracedEntityIndex = static_cast<uint32>(entt::to_entity(entity));
                }
                renderer.SubmitSkinned(targetCommand, target.pass->GetID());
            }
        }
    }
}
