#pragma once

#include "ComponentAnnotations.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>

namespace Engine
{
    ENUM()
    enum class ECanvasScaleMode
    {
        ConstantPixelSize,
        ScaleWithScreenSize,
        ConstantPhysicalSize
    };

    STRUCT(COMPONENT, CATEGORY("CATEGORY_UI"))
    struct CanvasScalerComponent
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE)
        ECanvasScaleMode scaleMode = ECanvasScaleMode::ConstantPixelSize;

        PROPERTY(EDITABLE)
        glm::vec2 referenceResolution{ 1920.0f, 1080.0f };

        PROPERTY(EDITABLE)
        float matchWidthOrHeight = 0.5f; // 0.0f = match width, 1.0f = match height

        PROPERTY(EDITABLE)
        float scaleFactor = 1.0f;

        PROPERTY(EDITABLE)
        float referencePixelsPerUnit = 100.0f;

        [[nodiscard]] float ComputeScaleFactor(glm::vec2 viewportSize) const
        {
            if (viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
            {
                return (scaleFactor > 0.0f && std::isfinite(scaleFactor)) ? scaleFactor : 1.0f;
            }

            switch (scaleMode)
            {
            case ECanvasScaleMode::ConstantPixelSize:
            case ECanvasScaleMode::ConstantPhysicalSize:
            {
                return (scaleFactor > 0.0f && std::isfinite(scaleFactor)) ? scaleFactor : 1.0f;
            }
            case ECanvasScaleMode::ScaleWithScreenSize:
            {
                if (referenceResolution.x <= 0.0f || referenceResolution.y <= 0.0f)
                {
                    return 1.0f;
                }

                const float widthRatio = viewportSize.x / referenceResolution.x;
                const float heightRatio = viewportSize.y / referenceResolution.y;
                if (widthRatio <= 0.0f || heightRatio <= 0.0f || !std::isfinite(widthRatio) || !std::isfinite(heightRatio))
                {
                    return 1.0f;
                }

                const float clampedMatch = std::clamp(matchWidthOrHeight, 0.0f, 1.0f);
                if (clampedMatch <= 0.0f)
                {
                    return widthRatio;
                }
                if (clampedMatch >= 1.0f)
                {
                    return heightRatio;
                }

                // Log-weighted matching: 2^(log2(widthRatio) * (1 - match) + log2(heightRatio) * match)
                const float logW = std::log2(widthRatio);
                const float logH = std::log2(heightRatio);
                const float logWeighted = logW * (1.0f - clampedMatch) + logH * clampedMatch;
                const float result = std::exp2(logWeighted);
                return (std::isfinite(result) && result > 0.0f) ? result : 1.0f;
            }
            default:
                return 1.0f;
            }
        }

        [[nodiscard]] glm::vec2 ComputeEffectiveDesignSize(glm::vec2 viewportSize) const
        {
            const float factor = ComputeScaleFactor(viewportSize);
            if (factor <= 0.0f || !std::isfinite(factor))
            {
                return viewportSize;
            }
            return viewportSize / factor;
        }
    };
}
