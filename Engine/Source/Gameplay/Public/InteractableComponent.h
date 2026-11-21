#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"
#include <string>

namespace Engine
{
	STRUCT(COMPONENT, CATEGORY("CATEGORY_GAMEPLAY"))
	struct InteractableComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, RANGE(0.1f, 1000.0f))
		f32 interactRange = 2.5f;

		PROPERTY(EDITABLE)
		std::string interactTag;

		PROPERTY(EDITABLE)
		bool enabled = true;

		PROPERTY(EDITABLE)
		bool oneShot = false;

		PROPERTY(READONLY)
		bool consumed = false;

		bool CanInteract() const
		{
			return enabled && (!oneShot || !consumed);
		}

		bool TryConsume()
		{
			if (!CanInteract())
				return false;
			if (oneShot)
				consumed = true;
			return true;
		}
	};
}
