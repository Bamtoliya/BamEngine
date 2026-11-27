#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"

namespace Engine
{
	STRUCT(COMPONENT, CATEGORY("CATEGORY_CAMERA"))
	struct Camera2DRoomComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_ENABLED"), META("ComponentEnable", true))
		bool Enable = true;
		static constexpr auto ComponentEnableMember = &Camera2DRoomComponent::Enable;

		PROPERTY(EDITABLE, ALLOW_MISSING)
		vec2 roomMin{ -20.0f, -10.0f };

		PROPERTY(EDITABLE, ALLOW_MISSING)
		vec2 roomMax{ 20.0f, 10.0f };
	};
}
