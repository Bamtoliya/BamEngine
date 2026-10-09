#include "MeshRenderSystem.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Renderer.h"
#include "RenderPass.h"

IMPLEMENT_SINGLETON(MeshRenderSystem)

EResult MeshRenderSystem::Initialize(void* arg)
{
	return EResult::Success;
}

void MeshRenderSystem::Free()
{
}

void MeshRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
	TODO("MeshRenderSystem::OnSubmit 구현 필요");
	(void)registry;
	(void)activeScenes;
	(void)dt;

    auto& renderer = Renderer::Get();

    // 현재 뷰포트에 등록된 Geometry 패스만 수집합니다.
    vector<RenderPass*> geometryPasses;

    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        RenderPass* pass = viewport.renderPass;

        if (!pass || !viewport.camera)
            continue;

        if (pass->GetPassType() != ERenderPassType::Geometry)
            continue;

        // 같은 패스가 중복 등록되어도 명령은 한 번만 제출합니다.
        if (std::find(
            geometryPasses.begin(),
            geometryPasses.end(),
            pass) == geometryPasses.end())
        {
            geometryPasses.push_back(pass);
        }
    }

    if (geometryPasses.empty())
        return;

    for (Scene* scene : activeScenes)
    {
        if (!scene)
            continue;

        auto view = scene->GetRegistry().view<
            StaticMeshRendererComponent,
            WorldTransformComponent,
            FlagComponent>();

        for (auto [entity, meshRenderer, worldTransform, flag]
            : view.each())
        {
            (void)entity;

            if (!HasFlag(flag.flags, EEntityFlag::Active) ||
                !HasFlag(flag.flags, EEntityFlag::Visible) ||
                HasFlag(flag.flags, EEntityFlag::Dead))
            {
                continue;
            }

            Mesh* mesh = meshRenderer.meshHandle.Get();
            MaterialInterface* material =
                meshRenderer.materialHandle.Get();

            // 비동기 로딩 중이거나 리소스가 없으면 제출하지 않습니다.
            if (!mesh || !material)
                continue;

            StaticDrawCommand command;
            command.mesh = mesh;
            command.material = material;
            command.worldMatrix = worldTransform.worldMatrix;

            for (RenderPass* pass : geometryPasses)
            {
                if (!pass->IsAcceptsBlendMode(
                    material->GetBlendMode()))
                {
                    continue;
                }

                renderer.SubmitStatic(command, pass->GetID());
            }
        }
    }
}
