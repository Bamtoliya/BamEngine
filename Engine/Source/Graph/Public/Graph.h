#pragma once

#include "GraphData.h"
#include "GraphLink.h"
#include "GraphRegistry.h"
#include <span>

namespace Engine
{

// Owns document data. Node schemas belong to the registry supplied by the caller.
class ENGINE_API Graph
{
public:
	explicit Graph(EGraphDomain domain = EGraphDomain::Logic) : m_Domain(domain) {}

	EGraphDomain GetDomain() const { return m_Domain; }
	std::span<const GraphNode> GetNodes() const { return m_Nodes; }
	std::span<const GraphLink> GetLinks() const { return m_Links; }
	const GraphNode* FindNode(GraphNodeId id) const;

	[[nodiscard]] GraphDiagnostic AddNode(const GraphRegistry& registry, const std::string& type);
	[[nodiscard]] GraphDiagnostic RemoveNode(GraphNodeId id);
	[[nodiscard]] GraphDiagnostic CanConnect(const GraphRegistry& registry, const GraphPinReference& from,
		const GraphPinReference& to) const;
	[[nodiscard]] GraphDiagnostic Connect(const GraphRegistry& registry, GraphPinReference from, GraphPinReference to);
	[[nodiscard]] GraphDiagnostic Disconnect(GraphLinkId id);
	[[nodiscard]] GraphDiagnostic SetInputValue(const GraphRegistry& registry, GraphPinReference input, GraphValue value);
	[[nodiscard]] GraphDiagnostic ResetInputValue(const GraphRegistry& registry, GraphPinReference input);
	[[nodiscard]] std::vector<GraphDiagnostic> Validate(const GraphRegistry& registry) const;
	GraphData ExportData() const;
	[[nodiscard]] GraphDiagnostic ImportData(const GraphRegistry& registry, const GraphData& data);

private:
	GraphDiagnostic ResolvePin(const GraphRegistry& registry, const GraphPinReference& ref, const GraphPin*& pin) const;
	GraphDiagnostic CheckConnection(const GraphRegistry& registry, const GraphPinReference& from,
		const GraphPinReference& to, GraphLinkId ignoredLink = 0) const;
	bool HasDataPath(const GraphRegistry& registry, GraphNodeId from, GraphNodeId to) const;

	EGraphDomain m_Domain;
	GraphNodeId m_NextNodeId = 1;
	GraphLinkId m_NextLinkId = 1;
	std::vector<GraphNode> m_Nodes;
	std::vector<GraphLink> m_Links;
};

}
