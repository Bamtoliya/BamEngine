#pragma once
#include "Engine_API.h"
#include "ReflectionMacro.h"

namespace Engine
{
	STRUCT(DISPLAY_NAME("STRUCT_CIRCLE"), CATEGORY("CATEGORY_CIRCLE"))
	struct ENGINE_API Circle
	{
		REFLECT_BODY()
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CENTER"), CATEGORY("CATEGORY_CIRCLE"))
		vec2 center{ 0.f };
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RADIUS"), CATEGORY("CATEGORY_CIRCLE"))
		f32 radius = 0.f;
		Circle() = default;
		Circle(const vec2& center, f32 radius) : center(center), radius(radius) {}
		bool IsValid() const
		{
			return std::isfinite(center.x) && std::isfinite(center.y) &&
				std::isfinite(radius) && radius >= 0.f;
		}
		bool Contains(const vec2& point) const
		{
			if (!IsValid() || !IsFinite(point))
				return false;
			const glm::dvec2 delta = glm::dvec2(point) - glm::dvec2(center);
			return glm::dot(delta, delta) <= static_cast<double>(radius) * radius;
		}
	};
}