#pragma once

#include "System.h"
#include "ResourceHandle.h"
#include <map>

namespace Engine
{
    class MaterialInterface;
    class Texture;
    struct UIElementLayout;
    struct UITextComponent;
    class Renderer;
    class RenderPass;

    class ENGINE_API UIRenderSystem final : public ISystem
    {
    public:
        static UIRenderSystem* Create(void* arg = nullptr);
        void OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

        // UI 이미지가 머티리얼·스프라이트를 지정하지 않았을 때 쓰는 기본 리소스. 로드되지 않았으면 빈 핸들이다.
        static ResourceHandle<MaterialInterface> GetDefaultMaterial();
        static ResourceHandle<Texture> GetDefaultTexture();

    private:
        struct CachedText
        {
            ResourceHandle<Texture> texture;
            bool submitted = false;
        };

        void SubmitText(Renderer& renderer, RenderPass& pass, const entt::registry& world,
            const UIElementLayout& layout, const UITextComponent& text, const glm::vec2& canvasSize,
            const ResourceHandle<MaterialInterface>& material);
        std::map<std::pair<int, std::string>, CachedText> m_TextCache;
    };
}
