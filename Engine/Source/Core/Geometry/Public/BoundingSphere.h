#pragma once

#include "Engine_API.h"
#include "ReflectionMacro.h"

#include <cmath>

namespace Engine
{
    STRUCT(DISPLAY_NAME("STRUCT_BOUNDING_SPHERE"), CATEGORY("CATEGORY_BOUNDS"))
    struct ENGINE_API BoundingSphere
    {
        REFLECT_BODY()

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CENTER"))
        vec3 center{ 0.f };

        PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RADIUS"))
        f32 radius = 0.f;

        BoundingSphere() = default;
        BoundingSphere(const vec3& center, f32 radius) : center(center), radius(radius) {}

        bool IsValid() const
        {
            return std::isfinite(center.x) && std::isfinite(center.y) && std::isfinite(center.z) &&
                std::isfinite(radius) && radius >= 0.f;
        }

        bool Contains(const vec3& point) const
        {
            if (!IsValid() || !IsFinite(point))
                return false;

            const glm::dvec3 delta = glm::dvec3(point) - glm::dvec3(center);
            return glm::dot(delta, delta) <= static_cast<double>(radius) * radius;
        }
    };
}