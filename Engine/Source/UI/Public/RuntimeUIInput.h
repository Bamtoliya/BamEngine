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
        // The hosts provide output-relative coordinates, where (0, 0) to (1, 1) maps to each Canvas.
        bool Update(const std::vector<Scene*>& scenes, const UIPointerInput& input);
        void Clear();

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
