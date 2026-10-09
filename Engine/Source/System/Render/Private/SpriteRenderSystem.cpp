#include "SpriteRenderSystem.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "Scene.h"


IMPLEMENT_SINGLETON(SpriteRenderSystem)

EResult SpriteRenderSystem::Initialize(void* arg)
{
	return EResult();
}

void SpriteRenderSystem::Free()
{
}

void SpriteRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
    auto& renderer = Renderer::Get();

    vector<RenderPass*> geometryPasses;

    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        RenderPass* pass = viewport.renderPass;

        if (!pass || !viewport.camera ||
            pass->GetPassType() != ERenderPassType::Geometry)
        {
            continue;
        }

        if (std::find(geometryPasses.begin(), geometryPasses.end(), pass)
            == geometryPasses.end())
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
            SpriteRendererComponent,
            WorldTransformComponent,
            FlagComponent>();

        for (auto [entity, spriteRenderer, worldTransform, flag] : view.each())
        {
            if (!HasFlag(flag.flags, EEntityFlag::Active) ||
                !HasFlag(flag.flags, EEntityFlag::Visible) ||
                HasFlag(flag.flags, EEntityFlag::Dead))
            {
                continue;
            }

            // 이번 단계에서는 Simple만 지원합니다.
            if (spriteRenderer.drawMode != ESpriteDrawMode::Simple)
                continue;

            Sprite* sprite = spriteRenderer.spriteHandle.Get();
            Mesh* mesh = spriteRenderer.meshHandle.Get();
            MaterialInterface* material = spriteRenderer.materialHandle.Get();

            if (!sprite || !mesh || !material)
                continue;

            Texture* texture = sprite->GetTexture();

            if (!texture || !texture->GetRHITexture())
                continue;

            vec4 uv = sprite->GetUVTransform();

            if (uv.z <= 0.f || uv.w <= 0.f)
                continue;

            uv.x += spriteRenderer.offset.x * uv.z;
            uv.y += spriteRenderer.offset.y * uv.w;
            uv.z *= spriteRenderer.tiling.x;
            uv.w *= spriteRenderer.tiling.y;

            if (spriteRenderer.flipX)
            {
                uv.x += uv.z;
                uv.z = -uv.z;
            }

            if (spriteRenderer.flipY)
            {
                uv.y += uv.w;
                uv.w = -uv.w;
            }

            SpriteDrawCommand command;
            command.mesh = mesh;
            command.texture = texture;
            command.material = material;
            command.worldMatrix = worldTransform.worldMatrix;
            command.color = spriteRenderer.color;
            command.uvTransform = uv;

            for (RenderPass* pass : geometryPasses)
            {
                if (pass->IsAcceptsBlendMode(material->GetBlendMode()))
                    renderer.SubmitSprite(command, pass->GetID());
            }
        }
    }
}