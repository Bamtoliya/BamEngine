#pragma once

#include "GraphPin.h"
#include <unordered_map>
#include <vector>

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

struct GraphNode
{
	GraphNodeId Id = 0;
	std::string Type;
	std::unordered_map<std::string, GraphValue> InputValues;
};

}
