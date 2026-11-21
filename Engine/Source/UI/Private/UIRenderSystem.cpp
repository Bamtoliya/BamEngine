#include "UIRenderSystem.h"
#include "ComponentEnable.h"
#include "UILayout.h"
#include "UIEventSystem.h"
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

    void SubmitImage(Engine::Renderer& renderer, Engine::RenderPass& pass, const entt::registry& world,
        const Engine::UIElementLayout& layout, glm::vec2 canvasSize,
        const Engine::ResourceHandle<Engine::MaterialInterface>& defaultUIMat, const Engine::ResourceHandle<Engine::Texture>& white)
    {
        const auto* image = Engine::TryGetEnabledComponent<Engine::UIImageComponent>(world, layout.entity);
        if (!image || !Engine::UILayout::IsValid(*image) || image->color.a <= 0.0f) return;

        // UI 패스 전용 머티리얼 결정: 비어있거나 3D용 머티리얼(SpriteMaterial 등)이 지정된 경우 기본 UIMaterial로 안전 폴백
        Engine::ResourceHandle<Engine::MaterialInterface> effectiveMatHandle = image->materialHandle;
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
                for (const auto& layout : layouts)
                {
                    if (!layout.visible) continue;
                    SubmitImage(renderer, *pass, world, layout, canvasSize, defaultUIMat, white);

                    const auto* text = TryGetEnabledComponent<UITextComponent>(world, layout.entity);
                    if (text && UILayout::IsValid(*text) && text->color.a > 0.0f)
                        SubmitText(renderer, *pass, world, layout, *text, canvasSize, defaultUIMat);
                }
            }
        }
    }
    std::erase_if(m_TextCache, [](const auto& entry) { return !entry.second.submitted; });
}
