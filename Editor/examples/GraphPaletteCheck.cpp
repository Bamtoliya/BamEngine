#include "GraphNodePalette.h"
#include <iostream>
#include <stdexcept>

namespace
{

using namespace Engine;
using namespace Editor;

void Expect(bool condition, const char* message)
{
	if (!condition) throw std::runtime_error(message);
}

GraphNodeId Add(Graph& graph, const GraphRegistry& registry, const std::string& type)
{
	const auto result = graph.AddNode(registry, type);
	Expect(result.Succeeded(), "Cannot prepare node.");
	return result.Node;
}

bool Has(const std::vector<GraphNodeCandidate>& candidates, const std::string& type, const std::string& pin)
{
	return std::any_of(candidates.begin(), candidates.end(), [&](const GraphNodeCandidate& candidate)
	{
		return candidate.Type == type && candidate.Pin == pin;
	});
}

void CheckDirectionsAndCapacity(const GraphRegistry& registry)
{
	Graph graph;
	const auto value = Add(graph, registry, "Core.Float");
	const auto sum = Add(graph, registry, "Math.AddFloat");
	const GraphPinReference output{ value, "result" };
	auto candidates = FindGraphNodeCandidates(graph, registry, output);
	Expect(Has(candidates, "Math.AddFloat", "a") && Has(candidates, "Math.AddFloat", "b"),
		"Ambiguous compatible input pins must remain individually selectable.");
	Expect(!Has(candidates, "Math.AddFloat", "result") && !Has(candidates, "Core.Bool", "value"),
		"Wrong direction or type was offered.");
	Expect(!Has(candidates, "Shader.ColorOutput", "color"), "Wrong domain was offered.");
	Expect(graph.GetNodes().size() == 2 && graph.GetLinks().empty(), "Discovery mutated the document.");
	for (const auto& candidate : candidates)
	{
		auto copy = graph;
		const auto added = AddGraphNodeCandidate(copy, registry, candidate, output);
		Expect(added.Succeeded() && added.Link != 0, "An offered candidate cannot be connected.");
	}
	candidates = FindGraphNodeCandidates(graph, registry, { sum, "a" });
	Expect(Has(candidates, "Core.Float", "result"), "Dragging from an input did not offer producers.");
	const auto created = AddGraphNodeCandidate(graph, registry, { "Core.Float", "result" }, { sum, "a" });
	Expect(created.Succeeded() && created.Link != 0, "Reverse dragging could not create and connect.");
	Expect(graph.GetLinks().front().From == GraphPinReference{ created.Node, "result" } &&
		graph.GetLinks().front().To == GraphPinReference{ sum, "a" }, "Connection direction was reversed.");
	Expect(FindGraphNodeCandidates(graph, registry, { sum, "a" }).empty(), "Occupied data input was offered.");
	const auto occupied = AddGraphNodeCandidate(graph, registry, { "Core.Float", "result" }, { sum, "a" });
	Expect(occupied.Code == EGraphError::InputAlreadyConnected && graph.GetNodes().size() == 3,
		"A stale data candidate left a partial node or replaced the existing connection.");
	Expect(graph.Connect(registry, output, { sum, "b" }).Succeeded(), "Cannot prepare data fan-out.");
	Expect(!FindGraphNodeCandidates(graph, registry, output).empty(), "Data output fan-out was forbidden.");

	const auto entry = Add(graph, registry, "Event.Start");
	const auto branch = Add(graph, registry, "Flow.Branch");
	Expect(graph.Connect(registry, { entry, "exec" }, { branch, "exec" }).Succeeded(), "Cannot prepare flow.");
	Expect(FindGraphNodeCandidates(graph, registry, { entry, "exec" }).empty(), "Occupied flow output was offered.");
	const auto stale = AddGraphNodeCandidate(graph, registry, { "Flow.Branch", "exec" }, { entry, "exec" });
	Expect(stale.Code == EGraphError::ExecutionOutputAlreadyConnected && graph.GetNodes().size() == 5,
		"A stale flow candidate left a partial node or replaced the existing connection.");
	Expect(Has(FindGraphNodeCandidates(graph, registry, { branch, "exec" }), "Event.Start", "exec"),
		"Execution input merging should remain possible.");
}

void CheckObjectsAndDomains(GraphRegistry registry)
{
	using Direction = EGraphPinDirection;
	using Type = EGraphPinType;
	Expect(registry.Register({ "Test.Objects", { EGraphDomain::Logic }, {
		{ "out", Direction::Output, Type::Object, {}, "Actor" },
		{ "actor", Direction::Input, Type::Object, {}, "Actor" },
		{ "camera", Direction::Input, Type::Object, {}, "Camera" } } }).Succeeded(), "Cannot register object fixture.");
	Graph graph;
	const auto node = Add(graph, registry, "Test.Objects");
	const auto candidates = FindGraphNodeCandidates(graph, registry, { node, "out" });
	Expect(Has(candidates, "Test.Objects", "actor") && !Has(candidates, "Test.Objects", "camera"),
		"Object owner types were not matched exactly.");
	Graph shader(EGraphDomain::Shader);
	const auto color = Add(shader, registry, "Shader.ColorOutput");
	const auto sources = FindGraphNodeCandidates(shader, registry, { color, "color" });
	Expect(Has(sources, "Core.Float4", "result") && !Has(sources, "Core.Float", "result"),
		"Vector dimensions must match without implicit conversions.");
	const auto ordinary = FindGraphNodeCandidates(shader, registry);
	Expect(Has(ordinary, "Shader.ColorOutput", "") && !Has(ordinary, "Event.Start", ""),
		"Ordinary palette ignored the current domain.");
	const auto added = AddGraphNodeCandidate(shader, registry, { "Core.Float4", {} });
	Expect(added.Succeeded() && added.Node != 0 && added.Link == 0 && shader.GetNodes().size() == 2,
		"Ordinary palette creation should add a node without a connection.");
}

void CheckFailurePreservesDocument(const GraphRegistry& registry)
{
	Graph graph;
	const auto source = Add(graph, registry, "Core.Float");
	Expect(graph.SetInputValue(registry, { source, "value" }, 7.0f).Succeeded(), "Cannot prepare literal.");
	const auto unchanged = graph;
	const auto failed = AddGraphNodeCandidate(graph, registry, { "Core.Bool", "value" }, { source, "result" });
	Expect(failed.Code == EGraphError::TypeMismatch, "Mismatched candidate succeeded.");
	Expect(graph.GetNodes().size() == 1 && graph.GetLinks().empty() &&
		std::get<float>(graph.FindNode(source)->InputValues.at("value")) == 7.0f,
		"Failure left a partial node or data change.");
	Expect(!AddGraphNodeCandidate(graph, registry, { "Core.Float", "missing" }, { source, "result" }).Succeeded(),
		"Unknown destination pin succeeded.");
	Expect(!AddGraphNodeCandidate(graph, registry, { "Core.Float", "value" }, { source + 100, "result" }).Succeeded(),
		"Removed source succeeded.");
	Expect(FindGraphNodeCandidates(graph, registry, { source, "missing" }).empty(), "Invalid source offered all nodes.");
	auto control = unchanged;
	const auto expected = AddGraphNodeCandidate(control, registry, { "Math.AddFloat", "a" }, { source, "result" });
	const auto actual = AddGraphNodeCandidate(graph, registry, { "Math.AddFloat", "a" }, { source, "result" });
	Expect(actual.Succeeded() && actual.Node == expected.Node && actual.Link == expected.Link,
		"Failed attempts or discovery consumed document IDs.");
	Expect(graph.GetNodes().size() == 2 && graph.GetLinks().size() == 1, "Success did not commit both objects.");
}

void CheckSearch(const GraphRegistry& registry)
{
	auto definition = *registry.Find("Math.AddFloat");
	definition.DisplayName = "더하기";
	definition.Pins.front().DisplayName = "Left operand";
	const GraphNodeCandidate candidate{ definition.Type, "a" };
	Expect(MatchesGraphNodeSearch(definition, candidate, "  MATH addFLOAT  "), "Case-insensitive token search failed.");
	Expect(MatchesGraphNodeSearch(definition, candidate, "더하기 left"), "Display name or pin label search failed.");
	Expect(!MatchesGraphNodeSearch(definition, candidate, "Math missing"), "Search must require every word.");
	Expect(MatchesGraphNodeSearch(definition, candidate, "\t "), "Blank search hid nodes.");
	Expect(!MatchesGraphNodeSearch(definition, { definition.Type, "removed" }, "left"), "Stale pin search was unsafe.");
}

}

int main()
{
	try
	{
		GraphRegistry registry;
		Expect(registry.RegisterBuiltins().Succeeded(), "Cannot register builtins.");
		CheckDirectionsAndCapacity(registry);
		CheckObjectsAndDomains(registry);
		CheckFailurePreservesDocument(registry);
		CheckSearch(registry);
		std::cout << "Graph palette compatibility, search and atomic creation checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
