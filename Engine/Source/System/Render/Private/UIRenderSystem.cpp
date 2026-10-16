#include "UIRenderSystem.h"
#include "UILayout.h"
#include "UIEventSystem.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "ResourceManager.h"
#include "CoreComponents.h"
#include "Sprite.h"
#include "MaterialInterface.h"
#include <cmath>
#include <unordered_set>

void Engine::UIRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
    auto& renderer = Renderer::Get();
    std::unordered_set<RenderPassID> submitted;
    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        auto* pass = viewport.renderPass;
        if (!pass || pass->GetPassType() != ERenderPassType::UI || !submitted.insert(pass->GetID()).second) continue;
        for (Scene* scene : activeScenes)
        {
            if (!scene || (viewport.filterUICanvas && viewport.uiScene != scene)) continue;
            auto& world = scene->GetRegistry();
            std::vector<entt::entity> canvases;
            for (const auto root : world.view<CanvasComponent, IDComponent>())
                if (!viewport.filterUICanvas || root == viewport.uiCanvas) canvases.push_back(root);
            std::sort(canvases.begin(), canvases.end(), [&](auto left, auto right)
            {
                const auto leftOrder = UILayout::GetZIndex(world, left);
                const auto rightOrder = UILayout::GetZIndex(world, right);
                if (leftOrder != rightOrder) return leftOrder < rightOrder;
                return world.get<IDComponent>(left).id < world.get<IDComponent>(right).id;
            });
            for (const auto root : canvases)
            {
                std::vector<UIElementLayout> layouts;
                if (!UILayout::Build(world, root, layouts)) continue;
                const auto canvasSize = world.get<CanvasComponent>(root).referenceResolution;
                for (const auto& layout : layouts)
                {
                    const auto* image = world.try_get<UIImageComponent>(layout.entity);
                    if (!layout.visible || !image || !UILayout::IsValid(*image) || image->color.a <= 0.0f) continue;
                    auto* material = image->materialHandle.Get();
                    if (!material || !pass->IsAcceptsBlendMode(material->GetBlendMode())) continue;
                    Sprite* sprite = image->spriteHandle.Get();
                    // Missing assigned assets must not silently turn into a different image.
                    if (image->spriteHandle.GetRawHandle().IsValid() && !sprite) continue;
                    auto white = ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png");
                    Texture* texture = sprite ? sprite->GetTexture() : white.Get();
                    if (!texture || !texture->GetRHITexture()) continue;
                    UIImageDrawCommand command;
                    command.textureHandle = sprite ? sprite->GetTextureHandle() : white;
                    command.materialHandle = image->materialHandle;
                    command.worldMatrix = layout.renderMatrix;
                    command.color = image->color * UIEventSystem::GetButtonTint(world, layout.entity);
                    command.uvTransform = sprite ? sprite->GetUVTransform() : glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
                    const auto uv = command.uvTransform;
                    if (!std::isfinite(uv.x) || !std::isfinite(uv.y) || !std::isfinite(uv.z) ||
                        !std::isfinite(uv.w) || uv.z <= 0.0f || uv.w <= 0.0f) continue;
                    command.canvasSize = canvasSize;
                    renderer.SubmitUIImage(command, pass->GetID());
                }
            }
        }
    }
}
