#include "MeshRenderSystem.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "Material.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "PickingManager.h"
#include "ResourceManager.h"
#include "FrustumCuller.h"

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
    vector<RenderPass*> targetPasses;

    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        RenderPass* pass = viewport.renderPass;
        if (!pass) continue;

        const ERenderPassType passType = pass->GetPassType();
        const bool isShadowPass = passType == ERenderPassType::Shadow;

        // Shadow 패스의 카메라는 Renderer에서 광원 카메라로 설정한다.
        if (!isShadowPass && !viewport.hasCamera) continue;

        if (passType != ERenderPassType::Geometry && passType != ERenderPassType::Forward && !isShadowPass)
            continue;

        if (std::find(targetPasses.begin(), targetPasses.end(), pass) == targetPasses.end())
            targetPasses.push_back(pass);
    }

    if (targetPasses.empty())
        return;

    // 주 뷰포트 카메라(Geometry 패스) 정보 추출
    vec3 cameraPos(0.0f);
    float projY = 1.0f; // 1.0 / tan(fovY / 2)
    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        if (viewport.hasCamera && viewport.renderPass &&
            viewport.renderPass->GetPassType() == ERenderPassType::Geometry)
        {
            cameraPos = viewport.cameraBuffer.cameraPosition;
            projY = glm::max(std::abs(viewport.cameraBuffer.projMatrix[1][1]), 0.1f);
            break;
        }
    }

    for (Scene* scene : activeScenes)
    {
        if (!scene) continue;

        auto view = scene->GetRegistry().view<StaticMeshRendererComponent, WorldTransformComponent, FlagComponent>();
        for (auto [entity, meshRenderer, worldTransform, flag] : view.each())
        {
            if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;

            Mesh* mesh = meshRenderer.meshHandle.Get();
            MaterialInterface* material = meshRenderer.materialHandle.Get();
            if (!mesh || !material) continue;

            // LODGroupComponent가 부착되어 있는 경우 화면 점유율 기반 LOD 메시 선택
            if (auto* lodGroup = scene->GetRegistry().try_get<LODGroupComponent>(entity))
            {
                const vec3 objectPos = vec3(worldTransform.worldMatrix[3]);
                const float distance = glm::max(glm::distance(cameraPos, objectPos), 0.001f);
                const float maxScale = glm::max(glm::length(vec3(worldTransform.worldMatrix[0])),
                    glm::max(glm::length(vec3(worldTransform.worldMatrix[1])), glm::length(vec3(worldTransform.worldMatrix[2]))));
                const float localRadius = glm::length(mesh->GetMin() - mesh->GetMax()) * 0.5f;
                const float worldRadius = glm::max(localRadius * maxScale, 0.01f);
                const float coverage = glm::clamp((worldRadius * projY) / distance, 0.0f, 1.0f);
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
                // 주 뷰포트 카메라 기준일 때만 영속 상태를 갱신
                lodGroup->currentLODIndex = targetLOD;
                if (targetLOD == 2 && lodGroup->lod2Mesh.Get())
                    mesh = lodGroup->lod2Mesh.Get();
                else if (targetLOD == 1 && lodGroup->lod1Mesh.Get())
                    mesh = lodGroup->lod1Mesh.Get();
            }

            StaticDrawCommand command;
            command.mesh = mesh;
            command.material = material;
            command.worldMatrix = worldTransform.worldMatrix;

            if (!mesh->IsDynamic())
            {
                const BoundingBox localBounds(mesh->GetMin(), mesh->GetMax());
                BoundingBox worldBounds;
                if (localBounds.TryTransformToAABB(command.worldMatrix, worldBounds))
                    command.worldBounds = worldBounds;
            }

            command.pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

            for (RenderPass* pass : targetPasses)
            {
                if (pass->GetPassType() == ERenderPassType::Shadow && !meshRenderer.castShadow)
                    continue;

                if (!pass->IsAcceptsBlendMode(material->GetBlendMode()))
                    continue;

                renderer.SubmitStatic(command, pass->GetID());
            }
        }
    }
}