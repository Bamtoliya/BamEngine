#include "UIRenderSystem.h"
#include "ComponentEnable.h"
#include "UILayout.h"
#include "UIClipLayout.h"
#include "UIEventSystem.h"
#include "UIWidgetComponents.h"
#include <glm/gtc/matrix_transform.hpp>
#include "Renderer.h"
#include "RenderPass.h"
#include "ResourceManager.h"
#include "WorldComponents.h"
#include "Sprite.h"
#include "Material.h"

Engine::UIRenderSystem* Engine::UIRenderSystem::Create(void* arg)
{
    return new UIRenderSystem();
}

namespace
{
    // 시작 매니페스트는 엔진 리소스를 Engine:// URI 키로 등록한다. Client는 UI 머티리얼을 레거시 키로도 등록하므로 둘 다 찾는다.
    template<typename T>
    Engine::ResourceHandle<T> FindEngineResource(const wchar_t* uri, const wchar_t* legacyKey)
    {
        auto& resources = Engine::ResourceManager::Get();
        auto handle = resources.GetResourceHandle<T>(uri);
        return handle.IsValid() ? handle : resources.GetResourceHandle<T>(legacyKey);
    }

    bool HasWidgetDecorations(const entt::registry& world, entt::entity entity)
    {
        return world.any_of<Engine::UISliderComponent, Engine::UIToggleComponent, Engine::UIProgressBarComponent>(entity);
    }

    void SubmitWidgetDecorations(Engine::Renderer& renderer, Engine::RenderPass& pass, const entt::registry& world,
        const Engine::UIElementLayout& layout, glm::vec2 canvasSize,
        const Engine::ResourceHandle<Engine::MaterialInterface>& material, const Engine::ResourceHandle<Engine::Texture>& white)
    {
        const auto* texture = white.Get();
        if (!texture || !texture->GetRHITexture() || !material.Get() ||
            !pass.IsAcceptsBlendMode(material.Get()->GetBlendMode())) return;

        const auto tint = Engine::UIEventSystem::GetButtonTint(world, layout.entity);
        const auto drawRect = [&](glm::vec2 offset, glm::vec2 size, glm::vec4 color)
        {
            if (size.x <= 0.0f || size.y <= 0.0f || color.a <= 0.0f ||
                !glm::all(glm::greaterThanEqual(color, glm::vec4(0.0f))) ||
                !glm::all(glm::lessThanEqual(color, glm::vec4(1.0f)))) return;
            Engine::UIImageDrawCommand command;
            command.textureHandle = white;
            command.materialHandle = material;
            command.worldMatrix = glm::scale(glm::translate(layout.renderMatrix, glm::vec3(offset, 0.0f)),
                glm::vec3(size, 1.0f));
            command.color = color * tint;
            command.canvasSize = canvasSize;
            command.clipRect = layout.clipRect;
            command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
            command.ellipseMaskCount = layout.ellipseMaskCount;
            renderer.SubmitUIImage(command, pass.GetID());
        };

        if (const auto* slider = Engine::TryGetEnabledComponent<Engine::UISliderComponent>(world, layout.entity))
        {
            if (!Engine::UIWidgetRange::IsValid(*slider)) return;
            const float fraction = Engine::UIWidgetRange::Fraction(slider->minimum, slider->maximum, slider->value);
            const float handleWidth = std::clamp(layout.size.y * 0.70f / layout.size.x, 0.0f, 0.25f);
            const float handlePosition = std::clamp(fraction - 0.5f * handleWidth, 0.0f, 1.0f - handleWidth);
            drawRect({ 0.0f, 0.30f }, { fraction, 0.40f }, slider->fillColor);
            drawRect({ handlePosition, 0.08f }, { handleWidth, 0.84f }, slider->handleColor);
        }
        else if (const auto* toggle = Engine::TryGetEnabledComponent<Engine::UIToggleComponent>(world, layout.entity))
        {
            if (toggle->checked) drawRect({ 0.22f, 0.22f }, { 0.56f, 0.56f }, toggle->checkColor);
        }
        else if (const auto* progress = Engine::TryGetEnabledComponent<Engine::UIProgressBarComponent>(world, layout.entity))
        {
            if (!Engine::UIWidgetRange::IsValid(progress->minimum, progress->maximum, progress->value)) return;
            const float fraction = Engine::UIWidgetRange::Fraction(progress->minimum, progress->maximum, progress->value);
            drawRect({ 0.0f, 0.0f }, { fraction, 1.0f }, progress->fillColor);
        }
    }

    void SubmitScrollbar(Engine::Renderer& renderer, Engine::RenderPass& pass, const Engine::UIElementLayout& viewport,
        const Engine::UIScrollbarLayout& scrollbar, glm::vec2 canvasSize,
        const Engine::ResourceHandle<Engine::MaterialInterface>& material, const Engine::ResourceHandle<Engine::Texture>& white)
    {
        const auto* texture = white.Get();
        if (!texture || !texture->GetRHITexture() || !material.Get() ||
            !pass.IsAcceptsBlendMode(material.Get()->GetBlendMode())) return;

        Engine::UIImageDrawCommand command;
        command.textureHandle = white;
        command.materialHandle = material;
        command.canvasSize = canvasSize;
        command.clipRect = viewport.clipRect;
        command.ellipseMaskPlanes = viewport.ellipseMaskPlanes;
        command.ellipseMaskCount = viewport.ellipseMaskCount;
        const auto track = glm::translate(viewport.renderMatrix, glm::vec3(1.0f - scrollbar.width, 0.0f, 0.0f));
        command.worldMatrix = glm::scale(track, glm::vec3(scrollbar.width, 1.0f, 1.0f));
        command.color = { 0.10f, 0.12f, 0.15f, 0.90f };
        renderer.SubmitUIImage(command, pass.GetID());

        const auto thumb = glm::translate(track, glm::vec3(0.0f, scrollbar.thumbTop, 0.0f));
        command.worldMatrix = glm::scale(thumb, glm::vec3(scrollbar.width, scrollbar.thumbHeight, 1.0f));
        command.color = { 0.73f, 0.78f, 0.85f, 1.0f };
        renderer.SubmitUIImage(command, pass.GetID());
    }

    void SubmitImage(Engine::Renderer& renderer, Engine::RenderPass& pass, const entt::registry& world,
        const Engine::UIElementLayout& layout, glm::vec2 canvasSize,
        const Engine::ResourceHandle<Engine::MaterialInterface>& defaultUIMat, const Engine::ResourceHandle<Engine::Texture>& white)
    {
        const auto* image = Engine::TryGetEnabledComponent<Engine::UIImageComponent>(world, layout.entity);
        if (!image || !Engine::UILayout::IsValid(*image) || image->color.a <= 0.0f) return;

        // UI 패스 전용 머티리얼 결정: 비어있거나 3D용 머티리얼(SpriteMaterial 등)이 지정된 경우 기본 UIMaterial로 안전 폴백
        // Built-in UI shaders carry ellipse coverage; use the built-in material on masked images.
        Engine::ResourceHandle<Engine::MaterialInterface> effectiveMatHandle =
            layout.ellipseMaskCount > 0 ? defaultUIMat : image->materialHandle;
        Engine::MaterialInterface* material = effectiveMatHandle.Get();
        if (!material)
        {
            effectiveMatHandle = defaultUIMat;
            material = defaultUIMat.Get();
        }
        else
        {
            // UI 패스 셰이더(SV_VertexID 기반 UIVS)가 아닌 정점 버퍼 입력을 기대하는 VS가 지정된 경우 안전하게 UIMaterial로 대체
            Engine::Shader* vs = material->GetVertexShader(Engine::ERenderPassType::UI);
            if (!vs || (vs->GetKey().find(L"ui") == std::wstring::npos && vs->GetKey().find(L"UI") == std::wstring::npos))
            {
                effectiveMatHandle = defaultUIMat;
                material = defaultUIMat.Get();
            }
        }

        if (!material || !pass.IsAcceptsBlendMode(material->GetBlendMode())) return;
        Engine::Sprite* sprite = image->spriteHandle.Get();
        // Missing assigned assets must not silently turn into a different image.
        if (image->spriteHandle.GetRawHandle().IsValid() && !sprite) return;
        Engine::Texture* texture = sprite ? sprite->GetTexture() : white.Get();
        if (!texture || !texture->GetRHITexture()) return;
        Engine::UIImageDrawCommand command;
        command.textureHandle = sprite ? sprite->GetTextureHandle() : white;
        command.materialHandle = effectiveMatHandle;
        command.worldMatrix = layout.renderMatrix;
        command.color = image->color * Engine::UIEventSystem::GetButtonTint(world, layout.entity);
        command.uvTransform = sprite ? sprite->GetUVTransform() : glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        const auto uv = command.uvTransform;
        if (!std::isfinite(uv.x) || !std::isfinite(uv.y) || !std::isfinite(uv.z) ||
            !std::isfinite(uv.w) || uv.z <= 0.0f || uv.w <= 0.0f) return;
        command.canvasSize = canvasSize;
        command.clipRect = layout.clipRect;
        command.ellipseMaskPlanes = layout.ellipseMaskPlanes;
        command.ellipseMaskCount = layout.ellipseMaskCount;
        renderer.SubmitUIImage(command, pass.GetID());
    }
}

Engine::ResourceHandle<Engine::MaterialInterface> Engine::UIRenderSystem::GetDefaultMaterial()
{
    return FindEngineResource<MaterialInterface>(L"Engine://Material/UIMaterial.bammat.json", L"Resources/Material/UIMaterial.bammat.json");
}

Engine::ResourceHandle<Engine::Texture> Engine::UIRenderSystem::GetDefaultTexture()
{
    return FindEngineResource<Texture>(L"Engine://Texture/white1x1.png", L"Resources/Texture/white1x1.png");
}

void Engine::UIRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
    auto& renderer = Renderer::Get();
    const auto defaultUIMat = GetDefaultMaterial();
    const auto white = GetDefaultTexture();
    for (auto& [key, cached] : m_TextCache) cached.submitted = false;
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
                if (IsComponentEnabled<CanvasComponent>(world, root) &&
                    (!viewport.filterUICanvas || root == viewport.uiCanvas)) canvases.push_back(root);
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
                const auto overlays = UIClipLayout::GetScrollbarOverlays(world, layouts);
                auto overlay = overlays.begin();
                for (std::size_t index = 0; index < layouts.size(); ++index)
                {
                    const auto& layout = layouts[index];
                    if (layout.visible)
                    {
                        SubmitImage(renderer, *pass, world, layout, canvasSize, defaultUIMat, white);
                        if (HasWidgetDecorations(world, layout.entity))
                            SubmitWidgetDecorations(renderer, *pass, world, layout, canvasSize, defaultUIMat, white);

                        const auto* text = TryGetEnabledComponent<UITextComponent>(world, layout.entity);
                        if (text && UILayout::IsValid(*text) && text->color.a > 0.0f)
                            SubmitText(renderer, *pass, world, layout, *text, canvasSize, defaultUIMat);
                    }
                    while (overlay != overlays.end() && layouts[overlay->index].subtreeEnd == index + 1)
                    {
                        SubmitScrollbar(renderer, *pass, layouts[overlay->index], overlay->geometry, canvasSize, defaultUIMat, white);
                        ++overlay;
                    }
                }
            }
        }
    }
    std::erase_if(m_TextCache, [](const auto& entry) { return !entry.second.submitted; });
}
