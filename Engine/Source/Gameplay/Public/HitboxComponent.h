#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"
#include <string>

namespace Engine
{
	STRUCT(COMPONENT, CATEGORY("CATEGORY_GAMEPLAY"))
	struct HitboxComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, RANGE(0.0f, 100000.0f))
		f32 damage = 10.0f;

		PROPERTY(EDITABLE, RANGE(0.0f, 10000.0f))
		f32 knockbackForce = 0.0f;

		PROPERTY(EDITABLE)
		std::string teamTag;

		PROPERTY(EDITABLE)
		bool destroyOnHit = false;

		PROPERTY(EDITABLE)
		bool active = true;
	};
}
