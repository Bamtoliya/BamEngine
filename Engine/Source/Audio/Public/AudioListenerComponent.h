#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"

namespace Engine
{
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_AUDIO_LISTENER"), CATEGORY("CATEGORY_AUDIO"))
	struct AudioListenerComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_ACTIVE"), META("ComponentEnable", true))
		bool isActive = true;
		static constexpr auto ComponentEnableMember = &AudioListenerComponent::isActive;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_VOLUME"))
		f32 volume = 1.f;
	};
}