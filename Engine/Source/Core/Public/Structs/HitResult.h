#pragma once
#include "Engine_API.h"
#include "Reflection/ReflectionMacro.h"

namespace Engine
{
	STRUCT(DISPLAY_NAME("STRUCT_HIT_RESULT"), CATEGORY("CATEGORY_HIT_RESULT"))
	struct ENGINE_API HitResult
	{
		REFLECT_BODY()
	
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_POINT"))
		vec3 point;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_NORMAL"))
		vec3 normal;
	
		union {
			f32 distance;
			f32 denetrationDepth;
		};
	
		union
		{
			vec3 extents;
			vec3 barycentric;
		};
	
		void* userData;
		bool hasHit;
	
		void Reset()
		{
			point = vec3(0.f);
			normal = vec3(0.f);
			distance = 0.f;
			extents = vec3(0.f);
			userData = nullptr;
			hasHit = false;
		}
	};
}