#pragma once

#include "GraphData.h"
#include "GraphLink.h"
#include "GraphRegistry.h"
#include "GraphVariable.h"
#include <span>
#include <vector>
#include <string>

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
		std::span<const GraphVariable> GetVariables() const { return m_Variables; }
		const GraphNode* FindNode(GraphNodeId id) const;
		const GraphVariable* FindVariable(const std::string& name) const;

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

		// ── 사용자 정의 변수 관리 및 노드 등록 ──
		[[nodiscard]] GraphDiagnostic AddVariable(GraphRegistry& registry, GraphVariable variable);
		[[nodiscard]] GraphDiagnostic RemoveVariable(GraphRegistry& registry, const std::string& name);
		void RegisterVariableNodes(GraphRegistry& registry) const;
		struct ReconciliationReport
		{
			size_t RemovedLinks = 0;
			size_t RemovedValues = 0;
			size_t InjectedDefaults = 0;
		};
		ReconciliationReport Reconcile(const GraphRegistry& registry);

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
		std::vector<GraphVariable> m_Variables;
	};

}