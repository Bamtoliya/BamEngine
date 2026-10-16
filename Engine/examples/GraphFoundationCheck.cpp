#include "Graph.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

namespace
{

using namespace Engine;
using Direction = EGraphPinDirection;
using Type = EGraphPinType;

void Expect(bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

void Accept(const GraphDiagnostic& result)
{
	Expect(result.Succeeded(), result.Message);
}

void Reject(const GraphDiagnostic& result, EGraphError code)
{
	Expect(result.Code == code, "Unexpected diagnostic: " + result.Message);
}

GraphNodeId Add(Graph& graph, const GraphRegistry& registry, const std::string& type)
{
	const auto result = graph.AddNode(registry, type);
	Accept(result);
	return result.Node;
}

void CheckRegistry()
{
	GraphRegistry registry;
	Accept(registry.RegisterBuiltins());
	Reject(registry.RegisterBuiltins(), EGraphError::DuplicateType);
	Expect(registry.Find("Flow.Branch") != nullptr, "Duplicate registration damaged existing definitions.");
	const auto* add = registry.Find("Math.AddFloat");
	Expect(add && registry.FindPin(add->Type, "a"), "Math schema was not registered.");
	Expect(!registry.Find("Unknown") && !registry.FindPin(add->Type, "Unknown"), "Unknown lookup succeeded.");

	GraphNodeDefinition custom{ "User.Scale", { EGraphDomain::Logic, EGraphDomain::Shader }, {
		{ "input", Direction::Input, Type::Float, {} }, { "result", Direction::Output, Type::Float, {} } } };
	Accept(registry.Register(custom));
	const auto shaderTypes = registry.GetTypes(EGraphDomain::Shader);
	Expect(std::is_sorted(shaderTypes.begin(), shaderTypes.end()), "Node palette order is not stable.");
	Expect(std::find(shaderTypes.begin(), shaderTypes.end(), "User.Scale") != shaderTypes.end(),
		"User node is absent from the shader palette.");
	Expect(std::find(shaderTypes.begin(), shaderTypes.end(), "Flow.Branch") == shaderTypes.end(),
		"Logic-only node leaked into the shader palette.");
	const auto logicTypes = registry.GetTypes(EGraphDomain::Logic);
	Expect(std::find(logicTypes.begin(), logicTypes.end(), "Flow.Branch") != logicTypes.end() &&
		std::find(logicTypes.begin(), logicTypes.end(), "Shader.ColorOutput") == logicTypes.end(),
		"Logic palette did not filter node domains.");
	Expect(registry.GetTypes(static_cast<EGraphDomain>(99)).empty(), "Invalid palette domain returned node types.");
	Reject(registry.Register(custom), EGraphError::DuplicateType);
	custom.Type = "Bad.DuplicatePin";
	custom.Pins[1].Key = "input";
	Reject(registry.Register(custom), EGraphError::InvalidDefinition);
	custom.Type = "Bad.DefaultType";
	custom.Pins[1].Key = "result";
	custom.Pins[0].DefaultValue = true;
	Reject(registry.Register(custom), EGraphError::InvalidDefinition);
	custom.Type = "Bad.OutputDefault";
	custom.Pins[0].DefaultValue = 1.0f;
	custom.Pins[1].DefaultValue = 1.0f;
	Reject(registry.Register(custom), EGraphError::InvalidDefinition);
	custom.Type = "Bad.NonFinite";
	custom.Pins[1].DefaultValue = std::monostate{};
	custom.Pins[0].DefaultValue = std::numeric_limits<float>::infinity();
	Reject(registry.Register(custom), EGraphError::InvalidDefinition);
	Reject(registry.Register({ "Bad.ShaderExec", { EGraphDomain::Shader }, {
		{ "exec", Direction::Input, Type::Execution, {} } } }), EGraphError::InvalidDefinition);
	Reject(registry.Register({ "Bad.Domain", {}, {} }), EGraphError::InvalidDefinition);
	Expect(!registry.Find("Bad.NonFinite"), "Rejected schema was inserted.");

	GraphRegistry collision;
	Accept(collision.Register({ "Math.MultiplyFloat", { EGraphDomain::Logic }, {} }));
	Reject(collision.RegisterBuiltins(), EGraphError::DuplicateType);
	Expect(!collision.Find("Core.Float") && !collision.Find("Math.AddFloat") && collision.Find("Math.MultiplyFloat"),
		"Builtin registration left a partial set after a collision.");
}

void CheckFlowSchemas(const GraphRegistry& registry)
{
	const std::vector<std::string> flowTypes{ "Flow.Branch", "Flow.Sequence", "Flow.ForLoop", "Flow.WhileLoop",
		"Flow.DoOnce", "Flow.Gate", "Flow.FlipFlop" };
	Graph logic;
	Graph shader(EGraphDomain::Shader);
	for (const auto& type : flowTypes)
	{
		Add(logic, registry, type);
		Reject(shader.AddNode(registry, type), EGraphError::UnsupportedDomain);
	}
	Expect(logic.Validate(registry).empty(), "Flow defaults did not satisfy required data inputs.");
	Expect(registry.FindPin("Flow.Sequence", "then0") && registry.FindPin("Flow.Sequence", "then1"),
		"Sequence outputs are missing.");
	Expect(std::get<std::int64_t>(registry.FindPin("Flow.ForLoop", "first")->DefaultValue) == 0 &&
		std::get<std::int64_t>(registry.FindPin("Flow.ForLoop", "last")->DefaultValue) == 1 &&
		registry.FindPin("Flow.ForLoop", "index")->Type == Type::Int &&
		registry.FindPin("Flow.ForLoop", "completed"), "ForLoop contract changed.");
	Expect(registry.FindPin("Flow.DoOnce", "reset") && registry.FindPin("Flow.Gate", "toggle") &&
		registry.FindPin("Flow.FlipFlop", "reset"), "State control pins are missing.");
	Reject(shader.AddNode(registry, "Event.Update"), EGraphError::UnsupportedDomain);
	Add(shader, registry, "Logic.SelectFloat");
	Expect(shader.Validate(registry).empty(), "Pure selection is not available in Shader.");

	GraphRegistry invalid;
	Reject(invalid.Register({ "Object.NoType", { EGraphDomain::Logic }, {
		{ "target", Direction::Input, Type::Object, {} } } }), EGraphError::InvalidDefinition);
	Reject(invalid.Register({ "Object.Shader", { EGraphDomain::Shader }, {
		{ "target", Direction::Input, Type::Object, {}, "Owner" } } }), EGraphError::InvalidDefinition);
	Reject(invalid.Register({ "Object.Literal", { EGraphDomain::Logic }, {
		{ "target", Direction::Input, Type::Object, std::int64_t{ 0 }, "Owner" } } }), EGraphError::InvalidDefinition);
}

void CheckLogicDocument(const GraphRegistry& registry)
{
	Graph graph;
	const auto start = Add(graph, registry, "Event.Start");
	const auto number = Add(graph, registry, "Core.Float");
	const auto sum = Add(graph, registry, "Math.AddFloat");
	const auto greater = Add(graph, registry, "Math.GreaterFloat");
	const auto branch = Add(graph, registry, "Flow.Branch");
	const auto result = Add(graph, registry, "Logic.FloatResult");

	Accept(graph.SetInputValue(registry, { number, "value" }, 2.0f));
	Accept(graph.SetInputValue(registry, { sum, "b" }, 3.0f));
	Accept(graph.CanConnect(registry, { number, "result" }, { sum, "a" }));
	Expect(graph.GetLinks().empty(), "Connection preview changed the document.");
	Accept(graph.Connect(registry, { number, "result" }, { sum, "a" }));
	Accept(graph.Connect(registry, { sum, "result" }, { greater, "a" }));
	Accept(graph.Connect(registry, { greater, "result" }, { branch, "condition" }));
	const auto data = graph.Connect(registry, { sum, "result" }, { result, "value" });
	Accept(data);
	Accept(graph.Connect(registry, { start, "exec" }, { branch, "exec" }));
	Accept(graph.Connect(registry, { branch, "true" }, { result, "exec" }));
	Expect(graph.Validate(registry).empty(), "Valid logic document failed validation.");
	Expect(graph.GetNodes().size() == 6 && graph.GetLinks().size() == 6, "Unexpected document topology.");

	const auto before = graph.GetLinks().size();
	Reject(graph.Connect(registry, { sum, "result" }, { result, "value" }), EGraphError::DuplicateLink);
	Reject(graph.Connect(registry, { number, "result" }, { result, "value" }), EGraphError::InputAlreadyConnected);
	Reject(graph.Connect(registry, { sum, "a" }, { number, "value" }), EGraphError::InvalidDirection);
	Reject(graph.Connect(registry, { sum, "result" }, { number, "result" }), EGraphError::InvalidDirection);
	const auto mismatch = graph.Connect(registry, { sum, "result" }, { branch, "condition" });
	Reject(mismatch, EGraphError::TypeMismatch);
	Reject(graph.CanConnect(registry, { sum, "result" }, { branch, "condition" }), EGraphError::TypeMismatch);
	Expect(mismatch.Node == branch && mismatch.Pin == "condition", "Diagnostic lost its pin location.");
	Reject(graph.Connect(registry, { start, "exec" }, { result, "value" }), EGraphError::TypeMismatch);
	Reject(graph.Connect(registry, { start, "exec" }, { result, "exec" }), EGraphError::ExecutionOutputAlreadyConnected);
	Reject(graph.Connect(registry, { sum, "unknown" }, { result, "value" }), EGraphError::PinNotFound);
	Reject(graph.Connect(registry, { 0, "result" }, { result, "value" }), EGraphError::NodeNotFound);
	Expect(graph.GetLinks().size() == before, "Rejected connection changed the document.");

	Accept(graph.Disconnect(data.Link));
	Reject(graph.Disconnect(data.Link), EGraphError::LinkNotFound);
	const auto missing = graph.Validate(registry);
	Expect(missing.size() == 1 && missing[0].Code == EGraphError::MissingRequiredInput &&
		missing[0].Node == result && missing[0].Pin == "value", "Missing input was not located.");
	Accept(graph.SetInputValue(registry, { result, "value" }, 5.0f));
	Expect(graph.Validate(registry).empty(), "Input override did not satisfy required input.");
	Accept(graph.ResetInputValue(registry, { result, "value" }));
	Expect(!graph.Validate(registry).empty(), "Reset did not restore required input policy.");
	const auto replacement = graph.Connect(registry, { sum, "result" }, { result, "value" });
	Accept(replacement);
	Expect(replacement.Link > data.Link, "Deleted link IDs were reused.");

	Reject(graph.SetInputValue(registry, { number, "value" }, true), EGraphError::InvalidDefault);
	Reject(graph.SetInputValue(registry, { number, "value" }, std::int64_t{ 2 }), EGraphError::InvalidDefault);
	Reject(graph.SetInputValue(registry, { number, "value" }, std::numeric_limits<float>::quiet_NaN()),
		EGraphError::InvalidDefault);
	Reject(graph.SetInputValue(registry, { sum, "result" }, 1.0f), EGraphError::InvalidDirection);
	Reject(graph.ResetInputValue(registry, { start, "exec" }), EGraphError::InvalidDirection);
	Expect(std::get<float>(graph.FindNode(number)->InputValues.at("value")) == 2.0f,
		"Rejected value replaced an existing override.");

	// Definitions and defaults remain shared descriptions; document overrides and edits are independent.
	Graph copy = graph;
	Accept(copy.SetInputValue(registry, { number, "value" }, 9.0f));
	Accept(copy.RemoveNode(sum));
	Expect(graph.FindNode(sum) && graph.GetLinks().size() == before, "Copy edits changed the source document.");
	Accept(graph.RemoveNode(sum));
	Expect(!graph.FindNode(sum) && graph.GetLinks().size() == 3, "Node removal did not remove every attached link.");
	for (const auto& link : graph.GetLinks())
		Expect(link.From.Node != sum && link.To.Node != sum, "Deleted node still has an attached link.");
	Expect(Add(graph, registry, "Math.AddFloat") > result, "Deleted node IDs were reused.");
	Reject(graph.RemoveNode(sum), EGraphError::NodeNotFound);
	Accept(graph.ResetInputValue(registry, { number, "value" }));
	Expect(graph.FindNode(number)->InputValues.empty(), "Override reset failed.");
	Expect(std::get<float>(registry.FindPin("Core.Float", "value")->DefaultValue) == 0.0f,
		"Editing a document changed the registry default.");
}

void CheckCyclesAndDomains(const GraphRegistry& registry)
{
	Graph graph;
	const auto a = Add(graph, registry, "Math.AddFloat");
	const auto b = Add(graph, registry, "Math.AddFloat");
	const auto c = Add(graph, registry, "Math.AddFloat");
	Accept(graph.Connect(registry, { a, "result" }, { b, "a" }));
	Accept(graph.Connect(registry, { b, "result" }, { c, "a" }));
	Reject(graph.Connect(registry, { c, "result" }, { a, "a" }), EGraphError::DataCycle);
	Reject(graph.Connect(registry, { a, "result" }, { a, "b" }), EGraphError::DataCycle);
	Expect(graph.GetLinks().size() == 2 && graph.Validate(registry).empty(), "Rejected cycle changed the graph.");

	const auto branch = Add(graph, registry, "Flow.Branch");
	Accept(graph.Connect(registry, { branch, "true" }, { branch, "exec" }));
	Accept(graph.Connect(registry, { branch, "false" }, { branch, "exec" }));
	Expect(graph.Validate(registry).empty(), "Execution loop was confused with a data cycle.");
	// An execution loop is representable, but there is no interpreter in this foundation.

	Graph shader(EGraphDomain::Shader);
	Reject(shader.AddNode(registry, "Flow.Branch"), EGraphError::UnsupportedDomain);
	Reject(shader.AddNode(registry, "Unknown"), EGraphError::InvalidDefinition);
	Expect(shader.GetNodes().empty(), "Rejected node changed the shader graph.");
	const auto color = Add(shader, registry, "Shader.ColorOutput");
	Accept(shader.SetInputValue(registry, { color, "color" }, std::array<float, 4>{ 0.2f, 0.4f, 0.8f, 1.0f }));
	Expect(shader.Validate(registry).empty(), "Valid shader document failed validation.");
	const auto scalar = Add(shader, registry, "Core.Float");
	Reject(shader.Connect(registry, { scalar, "result" }, { color, "color" }), EGraphError::TypeMismatch);
	Reject(shader.SetInputValue(registry, { color, "color" }, std::array<float, 4>{
		0.0f, std::numeric_limits<float>::infinity(), 0.0f, 1.0f }), EGraphError::InvalidDefault);
	Reject(graph.AddNode(registry, "Shader.ColorOutput"), EGraphError::UnsupportedDomain);

	GraphRegistry empty;
	Expect(!shader.Validate(empty).empty(), "Registry mismatch was silently accepted.");
	Graph invalid(static_cast<EGraphDomain>(99));
	Reject(invalid.AddNode(registry, "Core.Float"), EGraphError::UnsupportedDomain);
	Expect(!invalid.Validate(registry).empty(), "Invalid graph domain passed validation.");

	GraphRegistry custom;
	Accept(custom.Register({ "User.RequiredInt", { EGraphDomain::Logic }, {
		{ "value", Direction::Input, Type::Int, {} } } }));
	Graph integer;
	const auto id = Add(integer, custom, "User.RequiredInt");
	Expect(integer.Validate(custom).size() == 1, "Custom required input was not checked.");
	Accept(integer.SetInputValue(custom, { id, "value" }, std::int64_t{ 9007199254740993LL }));
	Expect(integer.Validate(custom).empty(), "Custom node could not use the common graph API.");
	Expect(std::get<std::int64_t>(integer.FindNode(id)->InputValues.at("value")) == 9007199254740993LL,
		"Integer input lost precision.");
}

}

int main()
{
	try
	{
		CheckRegistry();
		Engine::GraphRegistry registry;
		Accept(registry.RegisterBuiltins());
		CheckFlowSchemas(registry);
		CheckLogicDocument(registry);
		CheckCyclesAndDomains(registry);
		std::cout << "Graph foundation checks passed. No graph execution or shader compilation was performed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Graph foundation check failed: " << error.what() << '\n';
		return 1;
	}
}
