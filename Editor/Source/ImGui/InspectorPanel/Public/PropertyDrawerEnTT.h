#pragma once

#include "Editor_Includes.h"
#include <entt/entt.hpp>

BEGIN(Editor)
class PropertyDrawerEnTT
{
public:
	static bool DrawHeaderNode(entt::meta_any& instance, const entt::meta_type& type);
	static bool DrawPropertyTable(entt::meta_any& instance, const entt::meta_type& type);
	static bool DrawProperty(entt::meta_any& instance, const entt::meta_data& data);
};
END
