#pragma once

#include "Reflection/ComponentAnnotations.h"
#include <entt/entity/entity.hpp>
#include <vector>

namespace Engine
{
	// Scene archives translate these runtime handles to persistent entity IDs.
	STRUCT(COMPONENT, NOSERIALIZE, HIDDEN, DISPLAY_NAME("STRUCT_HIERARCHY"), CATEGORY("CATEGORY_CORE"))
	struct HierarchyComponent
	{
		REFLECT_BODY()
		entt::entity parent = entt::null;
		std::vector<entt::entity> children;
	};
}
