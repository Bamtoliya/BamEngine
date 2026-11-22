#pragma once

#include "ComponentAnnotations.h"
#include <string>

namespace Engine
{
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SCENE_PERSISTENCE"), CATEGORY("CATEGORY_CORE"))
	struct ScenePersistenceComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PERSIST_ACROSS_SCENE_LOADS"))
		bool persistAcrossSceneLoads = true;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_PERSISTENT_GROUP"))
		std::string persistentGroup = "Global";
	};
}
