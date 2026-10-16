#pragma once

#include "Graph.h"
#include <algorithm>
#include <string_view>
#include <utility>

namespace Editor
{

struct GraphNodeCandidate
{
	std::string Type;
	std::string Pin;
};

inline Engine::GraphDiagnostic AddGraphNodeCandidate(Engine::Graph& graph, const Engine::GraphRegistry& registry,
	const GraphNodeCandidate& candidate, const Engine::GraphPinReference& source = {})
{
	// Commit the node and its optional connection together. Failed choices leave IDs and data intact.
	auto pending = graph;
	auto result = pending.AddNode(registry, candidate.Type);
	if (!result.Succeeded()) return result;
	if (source.Node != 0)
	{
		const auto* node = graph.FindNode(source.Node);
		const auto* pin = node ? registry.FindPin(node->Type, source.Pin) : nullptr;
		if (!pin) return { Engine::EGraphError::PinNotFound, source.Node, source.Pin, 0, "GraphPanel.SourceUnavailable" };
		const Engine::GraphPinReference target{ result.Node, candidate.Pin };
		const bool output = pin->Direction == Engine::EGraphPinDirection::Output;
		auto connection = pending.Connect(registry, output ? source : target, output ? target : source);
		if (!connection.Succeeded()) return connection;
		result.Link = connection.Link;
	}
	else if (!candidate.Pin.empty())
	{
		return { Engine::EGraphError::PinNotFound, 0, candidate.Pin, 0, "GraphPanel.SourceRequired" };
	}
	graph = std::move(pending);
	return result;
}

inline std::vector<GraphNodeCandidate> FindGraphNodeCandidates(const Engine::Graph& graph,
	const Engine::GraphRegistry& registry, const Engine::GraphPinReference& source = {})
{
	std::vector<GraphNodeCandidate> candidates;
	const auto* node = graph.FindNode(source.Node);
	const auto* sourcePin = node ? registry.FindPin(node->Type, source.Pin) : nullptr;
	if (source.Node != 0 && !sourcePin) return candidates;
	for (const auto& type : registry.GetTypes(graph.GetDomain()))
	{
		if (!sourcePin)
		{
			candidates.push_back({ type, {} });
			continue;
		}
		const auto* definition = registry.Find(type);
		// Candidate discovery runs when opening the popup, never while typing or rendering its rows.
		auto pending = graph;
		const auto added = pending.AddNode(registry, type);
		if (!added.Succeeded()) continue;
		for (const auto& pin : definition->Pins)
		{
			if (pin.Direction == sourcePin->Direction || pin.Type != sourcePin->Type ||
				pin.ObjectType != sourcePin->ObjectType) continue;
			const Engine::GraphPinReference target{ added.Node, pin.Key };
			const bool output = sourcePin->Direction == Engine::EGraphPinDirection::Output;
			if (pending.CanConnect(registry, output ? source : target, output ? target : source).Succeeded())
				candidates.push_back({ type, pin.Key });
		}
	}
	return candidates;
}

inline bool MatchesGraphNodeSearch(const Engine::GraphNodeDefinition& definition,
	const GraphNodeCandidate& candidate, std::string_view search, std::string_view localizedText = {})
{
	const auto fold = [](unsigned char value)
	{
		return value >= 'A' && value <= 'Z' ? static_cast<unsigned char>(value + ('a' - 'A')) : value;
	};
	const auto contains = [&](std::string_view text, std::string_view word)
	{
		return std::search(text.begin(), text.end(), word.begin(), word.end(),
			[&](unsigned char a, unsigned char b) { return fold(a) == fold(b); }) != text.end();
	};
	const auto found = std::find_if(definition.Pins.begin(), definition.Pins.end(),
		[&](const Engine::GraphPin& value) { return value.Key == candidate.Pin; });
	const auto* pin = found == definition.Pins.end() ? nullptr : &*found;
	while (!search.empty())
	{
		const auto start = search.find_first_not_of(" \t\r\n");
		if (start == std::string_view::npos) break;
		search.remove_prefix(start);
		const auto end = search.find_first_of(" \t\r\n");
		const auto word = search.substr(0, end);
		if (!contains(definition.Type, word) && !contains(definition.DisplayName, word) &&
			!contains(definition.Category, word) && !contains(definition.Tooltip, word) && !contains(localizedText, word) &&
			!(pin && (contains(pin->Key, word) || contains(pin->DisplayName, word)))) return false;
		if (end == std::string_view::npos) break;
		search.remove_prefix(end);
	}
	return true;
}

}
