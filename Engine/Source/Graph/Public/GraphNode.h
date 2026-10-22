#pragma once

#include "GraphPin.h"
#include <unordered_map>
#include <vector>
#include <reflection/Annotations.h>

namespace Engine
{

enum class EGraphReflectionKind { None, Function, PropertyGet, PropertySet };

// Stable lookup information, not borrowed pointers into a reflection registry.
struct GraphReflectionSource
{
	EGraphReflectionKind Kind = EGraphReflectionKind::None;
	std::string Owner;
	std::string Member;
	std::string Signature;
};

struct GraphNodeDefinition
{
	std::string Type;
	std::vector<EGraphDomain> Domains;
	std::vector<GraphPin> Pins;
	std::string DisplayName;
	std::string Category;
	std::string Tooltip;
	GraphReflectionSource Reflection;
};

STRUCT()
struct GraphNode
{
	PROPERTY()
	GraphNodeId Id = 0;

	PROPERTY()
	std::string Type;

	PROPERTY()
	std::unordered_map<std::string, GraphValue> InputValues;
};

}
