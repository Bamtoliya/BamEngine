#pragma once

#include "GraphTypes.h"
#include <reflection/Annotations.h>

namespace Engine
{

struct GraphPin
{
	std::string Key;
	EGraphPinDirection Direction = EGraphPinDirection::Input;
	EGraphPinType Type = EGraphPinType::Float;
	GraphValue DefaultValue;
	// Object pins require an exact reflected owner name; they accept links, never literal values.
	std::string ObjectType;
	std::string DisplayName;
};

STRUCT()
struct GraphPinReference
{
	PROPERTY()
	GraphNodeId Node = 0;

	PROPERTY()
	std::string Pin;

	bool operator==(const GraphPinReference&) const = default;
};

}
