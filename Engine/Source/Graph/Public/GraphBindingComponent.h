#pragma once

#include "ComponentAnnotations.h"
#include <string>

namespace Engine
{
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_GRAPH_BINDING"), CATEGORY("CATEGORY_CORE"))
	struct GraphBindingComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_GRAPH_FILE"))
		std::wstring graphFile;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"), META("ComponentEnable", true))
		bool enabled = true;
		static constexpr auto ComponentEnableMember = &GraphBindingComponent::enabled;
	};
}
