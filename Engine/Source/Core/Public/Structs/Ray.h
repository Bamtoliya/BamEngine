#pragma once
#include "Engine_API.h"
#include "ReflectionMacro.h"

namespace Engine
{
	STRUCT(DISPLAY_NAME("STRUCT_RAY"), CATEGORY("CATEGORY_RAY"))
	struct ENGINE_API Ray
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ORIGIN"))
		vec3 origin;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_DIRECTION"))
		vec3 direction;

		Ray() = default;
		Ray(const vec3& origin, const vec3& direction)
			: origin(origin), direction(direction) {
		}
	};
}