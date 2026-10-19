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

void MeshRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
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

            StaticDrawCommand command;
            command.mesh = mesh;
            command.material = material;
            command.worldMatrix = worldTransform.worldMatrix;
            command.pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

            for (RenderPass* pass : targetPasses)
            {
                if (pass->GetPassType() == ERenderPassType::Shadow && !meshRenderer.castShadow)
                    continue;
                // 패스의 허용 BlendMode 검사:
                // GeometryPass -> Opaque/Masked (GBuffer)
                // ForwardPass  -> Forward (SkyMaterial)
                if (!pass->IsAcceptsBlendMode(material->GetBlendMode()))
                    continue;

                renderer.SubmitStatic(command, pass->GetID());
            }
        }
    }
}