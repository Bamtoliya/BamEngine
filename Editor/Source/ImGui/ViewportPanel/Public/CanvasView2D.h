#pragma once

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

namespace Editor
{
    struct CanvasView2D
    {
        glm::vec2 viewportOrigin{ 0.0f };
        glm::vec2 viewportSize{ 1.0f };
        glm::vec2 center{ 960.0f, 540.0f };
        float zoom = 1.0f; // ImGui screen units per canvas pixel; independent of RT resolution.

        glm::vec2 ToScreen(const glm::vec2& point) const
        {
            return viewportOrigin + viewportSize * 0.5f + (point - center) * zoom;
        }
        glm::vec2 ToCanvas(const glm::vec2& screen) const
        {
            return center + (screen - viewportOrigin - viewportSize * 0.5f) / zoom;
        }
        void Fit(const glm::vec2& canvasSize)
        {
            if (canvasSize.x <= 0.0f || canvasSize.y <= 0.0f) return;
            const glm::vec2 available = glm::max(viewportSize - glm::vec2(80.0f), glm::vec2(1.0f));
            zoom = std::clamp(glm::min(available.x / canvasSize.x, available.y / canvasSize.y), 1.0f / 64.0f, 32.0f);
            center = canvasSize * 0.5f;
        }
        void ZoomAt(const glm::vec2& screen, float factor)
        {
            if (!std::isfinite(factor) || factor <= 0.0f) return;
            const auto anchor = ToCanvas(screen);
            zoom = std::clamp(zoom * factor, 1.0f / 64.0f, 32.0f);
            center += anchor - ToCanvas(screen);
        }
        void Pan(const glm::vec2& screenDelta) { center -= screenDelta / zoom; }
        static glm::vec2 Snap(const glm::vec2& point, float step)
        {
            if (!std::isfinite(step) || step <= 0.0f) return point;
            return glm::round(point / step) * step;
        }
    };
}
