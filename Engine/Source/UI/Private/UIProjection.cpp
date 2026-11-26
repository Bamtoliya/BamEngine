#include "Engine_Includes.h"
#include "UIProjection.h"
#include "CameraComponent.h"

#include <cmath>

namespace Engine
{
    namespace UIProjectionDetail
    {
        constexpr f32 kMinClipW = 1e-6f;

        bool IsFiniteVec2(const glm::vec2& v) noexcept
        {
            return std::isfinite(v.x) && std::isfinite(v.y);
        }

        bool IsFiniteVec3(const glm::vec3& v) noexcept
        {
            return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
        }

        bool IsFiniteMat4(const glm::mat4& m) noexcept
        {
            for (int col = 0; col < 4; ++col)
            {
                for (int row = 0; row < 4; ++row)
                {
                    if (!std::isfinite(m[col][row]))
                        return false;
                }
            }
            return true;
        }

        UIWorldToScreenResult ProjectClipSpace(
            const glm::vec4& clipPos,
            bool hasKnownViewZ,
            f32 viewZ,
            const glm::vec2& viewportSize) noexcept
        {
            UIWorldToScreenResult result{};
            if (!IsFiniteVec2(viewportSize) || viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
                return result;

            if (!std::isfinite(clipPos.x) || !std::isfinite(clipPos.y) ||
                !std::isfinite(clipPos.z) || !std::isfinite(clipPos.w))
                return result;

            if (clipPos.w <= kMinClipW)
                return result;

            const glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
            if (!IsFiniteVec3(ndc))
                return result;

            // BamEngine은 GLM_FORCE_LEFT_HANDED + GLM_FORCE_DEPTH_ZERO_TO_ONE을 사용하며
            // UI/스크린 픽셀 좌표계는 좌상단 (0,0), Y-Down이다.
            const bool isInFront = hasKnownViewZ
                ? (viewZ > kMinClipW)
                : (ndc.z >= 0.0f || std::abs(clipPos.w - 1.0f) > 1e-4f);

            const glm::vec2 screenPos(
                (ndc.x * 0.5f + 0.5f) * viewportSize.x,
                (0.5f - ndc.y * 0.5f) * viewportSize.y);

            if (!IsFiniteVec2(screenPos))
                return result;

            result.screenPosition = screenPos;
            result.depth = ndc.z;
            result.isInFront = isInFront;
            result.isOnScreen = isInFront &&
                ndc.z >= 0.0f && ndc.z <= 1.0f &&
                screenPos.x >= 0.0f && screenPos.x <= viewportSize.x &&
                screenPos.y >= 0.0f && screenPos.y <= viewportSize.y;

            return result;
        }
    }

    UIWorldToScreenResult UIProjection::WorldToScreen(
        const glm::vec3& worldPosition,
        const glm::mat4& viewProjMatrix,
        const glm::vec2& viewportSize) noexcept
    {
        if (!UIProjectionDetail::IsFiniteVec3(worldPosition) ||
            !UIProjectionDetail::IsFiniteMat4(viewProjMatrix))
        {
            return {};
        }

        const glm::vec4 clipPos = viewProjMatrix * glm::vec4(worldPosition, 1.0f);
        return UIProjectionDetail::ProjectClipSpace(clipPos, false, 0.0f, viewportSize);
    }

    UIWorldToScreenResult UIProjection::WorldToScreen(
        const glm::vec3& worldPosition,
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const glm::vec2& viewportSize) noexcept
    {
        if (!UIProjectionDetail::IsFiniteVec3(worldPosition) ||
            !UIProjectionDetail::IsFiniteMat4(viewMatrix) ||
            !UIProjectionDetail::IsFiniteMat4(projMatrix))
        {
            return {};
        }

        const glm::vec4 viewPos = viewMatrix * glm::vec4(worldPosition, 1.0f);
        if (!std::isfinite(viewPos.z))
            return {};

        const glm::vec4 clipPos = projMatrix * viewPos;
        return UIProjectionDetail::ProjectClipSpace(clipPos, true, viewPos.z, viewportSize);
    }

    UIWorldToScreenResult UIProjection::WorldToScreen(
        const glm::vec3& worldPosition,
        const CameraComponent& camera,
        const glm::vec2& viewportSize) noexcept
    {
        const auto cameraBuffer = camera.GetCameraBuffer();
        if (!cameraBuffer.has_value())
            return {};

        return WorldToScreen(worldPosition, cameraBuffer->viewMatrix, cameraBuffer->projMatrix, viewportSize);
    }

    bool UIProjection::WorldToScreen(
        const glm::vec3& worldPosition,
        const CameraComponent& camera,
        const glm::vec2& viewportSize,
        glm::vec2& outScreenPosition,
        bool* outIsInFront) noexcept
    {
        const UIWorldToScreenResult result = WorldToScreen(worldPosition, camera, viewportSize);
        if (outIsInFront)
            *outIsInFront = result.isInFront;

        if (!result.isInFront)
            return false;

        outScreenPosition = result.screenPosition;
        return result.isOnScreen;
    }

    glm::vec2 UIProjection::ScreenToCanvas(
        const glm::vec2& screenPosition,
        const glm::vec2& viewportSize,
        const glm::vec2& canvasSize) noexcept
    {
        if (!UIProjectionDetail::IsFiniteVec2(screenPosition) ||
            !UIProjectionDetail::IsFiniteVec2(viewportSize) ||
            !UIProjectionDetail::IsFiniteVec2(canvasSize) ||
            viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
        {
            return glm::vec2(0.0f);
        }

        return (screenPosition / viewportSize) * canvasSize;
    }

    UIWorldToScreenResult UIProjection::WorldToCanvas(
        const glm::vec3& worldPosition,
        const CameraComponent& camera,
        const glm::vec2& viewportSize,
        const glm::vec2& canvasSize) noexcept
    {
        UIWorldToScreenResult result = WorldToScreen(worldPosition, camera, viewportSize);
        if (result.isInFront)
            result.screenPosition = ScreenToCanvas(result.screenPosition, viewportSize, canvasSize);
        return result;
    }

    UIWorldToScreenResult UIProjection::WorldToCanvas(
        const glm::vec3& worldPosition,
        const glm::mat4& viewMatrix,
        const glm::mat4& projMatrix,
        const glm::vec2& viewportSize,
        const glm::vec2& canvasSize) noexcept
    {
        UIWorldToScreenResult result = WorldToScreen(worldPosition, viewMatrix, projMatrix, viewportSize);
        if (result.isInFront)
            result.screenPosition = ScreenToCanvas(result.screenPosition, viewportSize, canvasSize);
        return result;
    }
}
