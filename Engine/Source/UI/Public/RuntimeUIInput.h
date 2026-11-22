#pragma once

#include "UIEventSystem.h"
#include <memory>
#include <span>

namespace Engine
{
    // Read after the host samples input and before its next sample. Scene storage owns the events.
    ENGINE_API std::span<const UIPointerEvent> GetRuntimeUIPointerEvents(const Scene& scene);

    class ENGINE_API RuntimeUIInput final
    {
    public:
        RuntimeUIInput() = default;
        RuntimeUIInput(const RuntimeUIInput&) = delete;
        RuntimeUIInput& operator=(const RuntimeUIInput&) = delete;

        // The hosts provide output-relative coordinates, where (0, 0) to (1, 1) maps to each Canvas.
        bool Update(const std::vector<Scene*>& scenes, const UIPointerInput& input, bool* wheelConsumed = nullptr);
        void Clear();

        bool ForwardKeyInput(EUIKey key, UIKeyModifiers mods = {}, std::string* clipboardOut = nullptr, std::string_view clipboardIn = {});
        bool ForwardTextInput(std::string_view utf8Text);
        bool ForwardImeComposition(std::string_view composition, bool commit = false);
        bool HasFocusedTextInput() const;
        bool HasOpenDropdown() const;

    private:
        struct CanvasInput
        {
            Scene* scene;
            entt::entity canvas;
            uint64 id;
            std::unique_ptr<UIEventSystem> events;
        };

        bool UpdateCanvases(const UIPointerInput& input);
        std::vector<CanvasInput> m_Canvases;
        bool m_BlocksGameplayPress = false;
    };
}
