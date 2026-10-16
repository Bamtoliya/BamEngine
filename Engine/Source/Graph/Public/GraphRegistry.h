#pragma once

#include "Engine_API.h"
#include "GraphNode.h"

namespace Engine
{

// Schemas only: no UI pointers, execution callbacks or process-global registry.
class ENGINE_API GraphRegistry
{
public:
	[[nodiscard]] GraphDiagnostic Register(GraphNodeDefinition definition);
	[[nodiscard]] GraphDiagnostic RegisterBuiltins();
	const GraphNodeDefinition* Find(const std::string& type) const;
	const GraphPin* FindPin(const std::string& type, const std::string& pin) const;
	std::vector<std::string> GetTypes(EGraphDomain domain) const;

private:
	std::unordered_map<std::string, GraphNodeDefinition> m_Definitions;
};

}
