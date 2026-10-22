#include "Graph.h"
#include "GraphValue.h"
#include <unordered_set>
#include <utility>

const GraphNode* Graph::FindNode(GraphNodeId id) const
{
	const auto found = std::find_if(m_Nodes.begin(), m_Nodes.end(), [&](const GraphNode& node) { return node.Id == id; });
	return found == m_Nodes.end() ? nullptr : &*found;
}

GraphDiagnostic Graph::AddNode(const GraphRegistry& registry, const std::string& type)
{
	const auto* definition = registry.Find(type);
	if (!definition)
		return { EGraphError::InvalidDefinition, 0, {}, 0, "Unknown node type: " + type };
	if (std::find(definition->Domains.begin(), definition->Domains.end(), m_Domain) == definition->Domains.end())
		return { EGraphError::UnsupportedDomain, 0, {}, 0, "Node type does not support this graph domain: " + type };
	if (m_NextNodeId == 0)
		return { EGraphError::IdExhausted, 0, {}, 0, "Node ID space is exhausted." };
	const auto id = m_NextNodeId;
	m_Nodes.push_back({ id, type, {} });
	++m_NextNodeId;
	return { EGraphError::None, id, {}, 0, {} };
}

GraphDiagnostic Graph::RemoveNode(GraphNodeId id)
{
	if (!FindNode(id))
		return { EGraphError::NodeNotFound, id, {}, 0, "Node does not exist." };
	std::erase_if(m_Links, [&](const GraphLink& link) { return link.From.Node == id || link.To.Node == id; });
	std::erase_if(m_Nodes, [&](const GraphNode& node) { return node.Id == id; });
	return {};
}

GraphDiagnostic Graph::ResolvePin(const GraphRegistry& registry, const GraphPinReference& ref, const GraphPin*& pin) const
{
	pin = nullptr;
	const auto* node = FindNode(ref.Node);
	if (!node)
		return { EGraphError::NodeNotFound, ref.Node, ref.Pin, 0, "Node does not exist." };
	const auto* definition = registry.Find(node->Type);
	if (!definition)
		return { EGraphError::InvalidDefinition, ref.Node, ref.Pin, 0,
			"Node type is not registered: " + node->Type };
	if (std::find(definition->Domains.begin(), definition->Domains.end(), m_Domain) == definition->Domains.end())
		return { EGraphError::UnsupportedDomain, ref.Node, ref.Pin, 0, "Node type does not support this graph domain." };
	pin = registry.FindPin(node->Type, ref.Pin);
	if (!pin)
		return { EGraphError::PinNotFound, ref.Node, ref.Pin, 0, "Pin key does not exist in the node definition." };
	return {};
}

bool Graph::HasDataPath(const GraphRegistry& registry, GraphNodeId from, GraphNodeId to) const
{
	std::vector<GraphNodeId> pending{ from };
	std::unordered_set<GraphNodeId> visited;
	while (!pending.empty())
	{
		const auto current = pending.back();
		pending.pop_back();
		if (current == to) return true;
		if (!visited.insert(current).second) continue;
		for (const auto& link : m_Links)
		{
			if (link.From.Node != current) continue;
			const GraphPin* pin = nullptr;
			if (ResolvePin(registry, link.From, pin).Succeeded() && pin->Type != EGraphPinType::Execution)
				pending.push_back(link.To.Node);
		}
	}
	return false;
}

GraphDiagnostic Graph::CheckConnection(const GraphRegistry& registry, const GraphPinReference& from,
	const GraphPinReference& to, GraphLinkId ignoredLink) const
{
	const GraphPin* output = nullptr;
	const GraphPin* input = nullptr;
	auto result = ResolvePin(registry, from, output);
	if (!result.Succeeded()) return result;
	result = ResolvePin(registry, to, input);
	if (!result.Succeeded()) return result;
	if (output->Direction != EGraphPinDirection::Output)
		return { EGraphError::InvalidDirection, from.Node, from.Pin, 0, "Connection must start at an output pin." };
	if (input->Direction != EGraphPinDirection::Input)
		return { EGraphError::InvalidDirection, to.Node, to.Pin, 0, "Connection must end at an input pin." };
	if (output->Type != input->Type || output->ObjectType != input->ObjectType)
		return { EGraphError::TypeMismatch, to.Node, to.Pin, 0, "Pin types must match exactly; conversions are explicit." };
	const bool execution = output->Type == EGraphPinType::Execution;
	for (const auto& link : m_Links)
	{
		if (link.Id == ignoredLink) continue;
		if (link.From == from && link.To == to)
			return { EGraphError::DuplicateLink, to.Node, to.Pin, link.Id, "These pins are already connected." };
		if (!execution && link.To == to)
			return { EGraphError::InputAlreadyConnected, to.Node, to.Pin, link.Id, "Data inputs accept one source." };
		if (execution && link.From == from)
			return { EGraphError::ExecutionOutputAlreadyConnected, from.Node, from.Pin, link.Id,
				"Execution outputs accept one destination; use separate flow outputs for branching." };
	}
	if (!execution && HasDataPath(registry, to.Node, from.Node))
		return { EGraphError::DataCycle, to.Node, to.Pin, 0, "Data connections must be acyclic." };
	return {};
}

GraphDiagnostic Graph::CanConnect(const GraphRegistry& registry, const GraphPinReference& from,
	const GraphPinReference& to) const
{
	return CheckConnection(registry, from, to);
}

GraphDiagnostic Graph::Connect(const GraphRegistry& registry, GraphPinReference from, GraphPinReference to)
{
	auto result = CheckConnection(registry, from, to);
	if (!result.Succeeded()) return result;
	if (m_NextLinkId == 0)
		return { EGraphError::IdExhausted, to.Node, to.Pin, 0, "Link ID space is exhausted." };
	const auto id = m_NextLinkId;
	m_Links.push_back({ id, std::move(from), std::move(to) });
	++m_NextLinkId;
	return { EGraphError::None, 0, {}, id, {} };
}

GraphDiagnostic Graph::Disconnect(GraphLinkId id)
{
	const auto count = std::erase_if(m_Links, [&](const GraphLink& link) { return link.Id == id; });
	if (count == 0)
		return { EGraphError::LinkNotFound, 0, {}, id, "Link does not exist." };
	return {};
}

GraphDiagnostic Graph::SetInputValue(const GraphRegistry& registry, GraphPinReference input, GraphValue value)
{
	const GraphPin* pin = nullptr;
	auto result = ResolvePin(registry, input, pin);
	if (!result.Succeeded()) return result;
	if (pin->Direction != EGraphPinDirection::Input || pin->Type == EGraphPinType::Execution)
		return { EGraphError::InvalidDirection, input.Node, input.Pin, 0, "Only data inputs accept default values." };
	if (!GraphValueMatchesType(value, pin->Type) || !IsFiniteGraphValue(value))
		return { EGraphError::InvalidDefault, input.Node, input.Pin, 0, "Default value must be finite and exactly typed." };
	for (auto& node : m_Nodes)
	{
		if (node.Id == input.Node)
		{
			node.InputValues.insert_or_assign(input.Pin, std::move(value));
			break;
		}
	}
	return {};
}

GraphDiagnostic Graph::ResetInputValue(const GraphRegistry& registry, GraphPinReference input)
{
	const GraphPin* pin = nullptr;
	auto result = ResolvePin(registry, input, pin);
	if (!result.Succeeded()) return result;
	if (pin->Direction != EGraphPinDirection::Input || pin->Type == EGraphPinType::Execution)
		return { EGraphError::InvalidDirection, input.Node, input.Pin, 0, "Only data inputs have default values." };
	for (auto& node : m_Nodes)
		if (node.Id == input.Node) node.InputValues.erase(input.Pin);
	return {};
}

std::vector<GraphDiagnostic> Graph::Validate(const GraphRegistry& registry) const
{
	std::vector<GraphDiagnostic> errors;
	if (m_Domain != EGraphDomain::Logic && m_Domain != EGraphDomain::Shader)
		errors.push_back({ EGraphError::UnsupportedDomain, 0, {}, 0, "Invalid graph domain." });
	for (const auto& node : m_Nodes)
	{
		const auto* definition = registry.Find(node.Type);
		if (!definition)
		{
			errors.push_back({ EGraphError::InvalidDefinition, node.Id, {}, 0,
				"Unregistered node type: " + node.Type });
			continue;
		}
		if (std::find(definition->Domains.begin(), definition->Domains.end(), m_Domain) == definition->Domains.end())
			errors.push_back({ EGraphError::UnsupportedDomain, node.Id, {}, 0, "Unsupported node domain: " + node.Type });
		for (const auto& [key, value] : node.InputValues)
		{
			const auto* pin = registry.FindPin(node.Type, key);
			if (!pin || pin->Direction != EGraphPinDirection::Input ||
				!GraphValueMatchesType(value, pin->Type) || !IsFiniteGraphValue(value))
				errors.push_back({ EGraphError::InvalidDefault, node.Id, key, 0, "Invalid input override." });
		}
		for (const auto& pin : definition->Pins)
		{
			if (pin.Direction != EGraphPinDirection::Input || pin.Type == EGraphPinType::Execution ||
				!std::holds_alternative<std::monostate>(pin.DefaultValue) || node.InputValues.contains(pin.Key)) continue;
			const GraphPinReference ref{ node.Id, pin.Key };
			if (std::none_of(m_Links.begin(), m_Links.end(), [&](const GraphLink& link) { return link.To == ref; }))
				errors.push_back({ EGraphError::MissingRequiredInput, node.Id, pin.Key, 0, "Data input requires a link or value." });
		}
	}
	for (const auto& link : m_Links)
	{
		auto result = CheckConnection(registry, link.From, link.To, link.Id);
		if (!result.Succeeded())
		{
			result.Link = link.Id;
			errors.push_back(std::move(result));
		}
	}
	return errors;
}

GraphData Graph::ExportData() const
{
	return { GraphData::CurrentVersion, m_Domain, m_NextNodeId, m_NextLinkId, m_Nodes, m_Links };
}

GraphDiagnostic Graph::ImportData(const GraphRegistry& registry, const GraphData& data)
{
	if (data.Version != GraphData::CurrentVersion)
		return { EGraphError::UnsupportedVersion, 0, {}, 0, "Unsupported graph data version." };

	std::unordered_set<GraphNodeId> nodeIds;
	for (const auto& node : data.Nodes)
	{
		if (node.Id == 0)
			return { EGraphError::InvalidDocument, node.Id, {}, 0, "Node ID must be non-zero." };

		if (!nodeIds.insert(node.Id).second)
			return { EGraphError::DuplicateNodeId, node.Id, {}, 0, "Duplicate node ID." };

		if (data.NextNodeId != 0 && node.Id >= data.NextNodeId)
			return { EGraphError::InvalidDocument, node.Id, {}, 0, "Next node ID must exceed existing node IDs." };
	}

	std::unordered_set<GraphLinkId> linkIds;
	for (const auto& link : data.Links)
	{
		if (link.Id == 0)
			return { EGraphError::InvalidDocument, 0, {}, link.Id, "Link ID must be non-zero." };

		if (!linkIds.insert(link.Id).second)
			return { EGraphError::DuplicateLinkId, 0, {}, link.Id, "Duplicate link ID." };

		if (data.NextLinkId != 0 && link.Id >= data.NextLinkId)
			return { EGraphError::InvalidDocument, 0, {}, link.Id, "Next link ID must exceed existing link IDs." };
	}

	Graph candidate(data.Domain);
	candidate.m_NextNodeId = data.NextNodeId;
	candidate.m_NextLinkId = data.NextLinkId;
	candidate.m_Nodes = data.Nodes;
	candidate.m_Links = data.Links;

	for (const auto& diagnostic : candidate.Validate(registry))
	{
		if (diagnostic.Code != EGraphError::MissingRequiredInput)
			return diagnostic;
	}

	m_Nodes.swap(candidate.m_Nodes);
	m_Links.swap(candidate.m_Links);
	m_Domain = candidate.m_Domain;
	m_NextNodeId = candidate.m_NextNodeId;
	m_NextLinkId = candidate.m_NextLinkId;
	return {};
}