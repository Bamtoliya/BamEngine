#pragma once

#include "Engine_Includes.h"

namespace Engine
{
    struct CameraComponent;

    struct UIWorldToScreenResult
    {
        glm::vec2 screenPosition{ 0.0f };
        f32 depth = 0.0f;
        bool isInFront = false;
        bool isOnScreen = false;
    };

    class ENGINE_API UIProjection
    {
    public:
        static UIWorldToScreenResult WorldToScreen(
            const glm::vec3& worldPosition,
            const glm::mat4& viewProjMatrix,
            const glm::vec2& viewportSize) noexcept;

        static UIWorldToScreenResult WorldToScreen(
            const glm::vec3& worldPosition,
            const glm::mat4& viewMatrix,
            const glm::mat4& projMatrix,
            const glm::vec2& viewportSize) noexcept;

        static UIWorldToScreenResult WorldToScreen(
            const glm::vec3& worldPosition,
            const CameraComponent& camera,
            const glm::vec2& viewportSize) noexcept;

        static bool WorldToScreen(
            const glm::vec3& worldPosition,
            const CameraComponent& camera,
            const glm::vec2& viewportSize,
            glm::vec2& outScreenPosition,
            bool* outIsInFront = nullptr) noexcept;

        static glm::vec2 ScreenToCanvas(
            const glm::vec2& screenPosition,
            const glm::vec2& viewportSize,
            const glm::vec2& canvasSize) noexcept;

        static UIWorldToScreenResult WorldToCanvas(
            const glm::vec3& worldPosition,
            const CameraComponent& camera,
            const glm::vec2& viewportSize,
            const glm::vec2& canvasSize) noexcept;

        static UIWorldToScreenResult WorldToCanvas(
            const glm::vec3& worldPosition,
            const glm::mat4& viewMatrix,
            const glm::mat4& projMatrix,
            const glm::vec2& viewportSize,
            const glm::vec2& canvasSize) noexcept;
    };
}
