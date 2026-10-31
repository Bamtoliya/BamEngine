#pragma once

#include "GraphTypes.h"
#include <string>
#include <reflection/Annotations.h>

namespace Engine
{

	STRUCT()
	struct GraphVariable
	{
		REFLECT_BODY()

		PROPERTY()
		std::string Name;

		PROPERTY()
		EGraphPinType Type = EGraphPinType::Float;

		PROPERTY()
		GraphValue DefaultValue = 0.0f;
	};

}