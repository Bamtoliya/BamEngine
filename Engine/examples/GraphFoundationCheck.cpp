#include "Graph.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include "Adapters/GraphValueArchiveAdapter.h"
#include <archive/JsonArchive.h>
#include <string_view>
#include <vector>
#include "GraphFoundationCheck.gen.h"
#include <reflection_archive/ReflectionArchiveAdapter.h>
#include "GraphFile.h"
#include <chrono>
#include <fstream>
#include <iterator>
#include <memory>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

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

void CheckGraphData(const GraphRegistry& registry)
{
	Graph source;
	const auto number = Add(source, registry, "Core.Float");
	const auto sum = Add(source, registry, "Math.AddFloat");

	Accept(source.SetInputValue(registry, { number, "value" }, 2.0f));
	Accept(source.SetInputValue(registry, { sum, "b" }, 3.0f));
	Accept(source.Connect(registry, { number, "result" }, { sum, "a" }));

	const auto removedLink = source.Connect(registry, { number, "result" }, { sum, "b" });
	Accept(removedLink);
	Accept(source.Disconnect(removedLink.Link));

	const auto removedNode = Add(source, registry, "Core.Bool");
	Accept(source.RemoveNode(removedNode));

	const auto data = source.ExportData();
	Graph restored(EGraphDomain::Shader);
	Accept(restored.ImportData(registry, data));

	Expect(restored.GetDomain() == EGraphDomain::Logic, "Graph domain was not restored.");
	Expect(restored.GetNodes().size() == 2 && restored.GetLinks().size() == 1, "Topology was not restored.");
	Expect(restored.FindNode(number)->Type == "Core.Float", "Node ID or type was changed.");
	Expect(restored.FindNode(sum)->InputValues == source.FindNode(sum)->InputValues, "Input values changed.");
	Expect(restored.GetLinks().front().Id == data.Links.front().Id, "Link ID was changed.");
	Expect(restored.GetLinks().front().From == data.Links.front().From &&
		restored.GetLinks().front().To == data.Links.front().To, "Link endpoints changed.");

	auto invalid = data;
	invalid.Nodes.push_back(invalid.Nodes.front());
	Reject(restored.ImportData(registry, invalid), EGraphError::DuplicateNodeId);
	Expect(restored.GetNodes().size() == 2, "Rejected import changed the graph.");

	invalid = data;
	invalid.NextNodeId = number;
	Reject(restored.ImportData(registry, invalid), EGraphError::InvalidDocument);

	invalid = data;
	invalid.Links.front().To.Pin = "missing";
	Reject(restored.ImportData(registry, invalid), EGraphError::PinNotFound);
	Expect(restored.Validate(registry).empty(), "Rejected import damaged the graph.");

	Expect(Add(restored, registry, "Core.Bool") == data.NextNodeId, "Deleted node ID was reused.");
	const auto newLink = restored.Connect(registry, { number, "result" }, { sum, "b" });
	Accept(newLink);
	Expect(newLink.Link == data.NextLinkId, "Deleted link ID was reused.");

	Graph incomplete;
	Add(incomplete, registry, "Logic.FloatResult");
	Accept(restored.ImportData(registry, incomplete.ExportData()));
	const auto errors = restored.Validate(registry);
	Expect(errors.size() == 1 && errors.front().Code == EGraphError::MissingRequiredInput,
		"Incomplete graph import changed execution validation.");
}

void CheckGraphValueArchive()
{
	archive_reflection::PropertyCodecs codecs;
	Expect(archive_graph::RegisterValueCodec(codecs), "GraphValue codec registration failed.");
	Expect(!archive_graph::RegisterValueCodec(codecs), "Duplicate codec registration was accepted.");

	const std::vector<GraphValue> values{
		std::monostate{}, false, std::int64_t{ 9007199254740993LL },
		(std::numeric_limits<std::int64_t>::min)(), (std::numeric_limits<std::int64_t>::max)(),
		2.0f, std::array<float, 2>{ 1.0f, -2.0f },
		std::array<float, 3>{ 1.0f, 2.0f, 3.0f }, std::array<float, 4>{ 0.2f, 0.4f, 0.8f, 1.0f }
	};

	for (const auto& source : values)
	{
		archive::JsonArchive writer;
		const auto written = codecs.WriteValue(writer, "Item", reflection::ValueView::From(source));
		Expect(written && static_cast<bool>(*written), "Registered GraphValue writer failed.");

		std::string document;
		archive::JsonArchive reader;
		Expect(writer.Encode(document) && reader.Decode(document), "JSON conversion failed.");

		GraphValue restored = true;
		const auto read = codecs.ReadValue(reader, "Item", std::type_index(typeid(GraphValue)),
			[&](const reflection::ValueView& value) -> archive::ArchiveResult
			{
				const auto* decoded = value.Get<GraphValue>();
				if (!decoded) return { archive::ArchiveErrorCode::InvalidOperation, "Item" };
				restored = *decoded;
				return {};
			});

		Expect(read && static_cast<bool>(*read) && restored == source, "GraphValue type or value changed.");
	}

	const std::string_view invalidDocuments[]{
		R"({"Item":{"Type":"Unknown","Value":1}})",
		R"({"Item":{"Type":"Bool","Value":1}})",
		R"({"Item":{"Type":"Int","Value":1.5}})",
		R"({"Item":{"Type":"Int","Value":9223372036854775808}})",
		R"({"Item":{"Type":"Float3","Value":[1,2]}})",
		R"({"Item":{"Type":"Float","Value":1e39}})",
		R"({"Item":{"Type":"None","Value":0}})",
		R"({"Item":{"Value":2}})"
	};

	for (const auto document : invalidDocuments)
	{
		archive::JsonArchive reader;
		Expect(static_cast<bool>(reader.Decode(document)), "Invalid-value test JSON could not be parsed.");

		GraphValue restored = std::int64_t{ 42 };
		const auto result = archive_graph::ReadValue(reader, "Item", restored);
		Expect(!result && restored == GraphValue{ std::int64_t{ 42 } }, "Failed read changed the destination.");
	}

	archive::JsonArchive writer;
	Expect(static_cast<bool>(archive_graph::WriteValue(writer, "Item", GraphValue{ 3.0f })), "Initial write failed.");

	std::string before, after;
	Expect(static_cast<bool>(writer.Encode(before)), "Initial encoding failed.");
	const auto rejected = archive_graph::WriteValue(writer, "Item", GraphValue{ std::numeric_limits<float>::infinity() });
	Expect(!rejected, "Non-finite GraphValue was accepted.");
	Expect(writer.Encode(after) && before == after, "Rejected write changed the document.");
}
void AcceptArchive(const archive::ArchiveResult& result)
{
	Expect(static_cast<bool>(result), "Archive error " + std::to_string(static_cast<int>(result.Code)) +
		", field: " + result.Field);
}

bool SameGraphData(const GraphData& left, const GraphData& right)
{
	if (left.Version != right.Version || left.Domain != right.Domain ||
		left.NextNodeId != right.NextNodeId || left.NextLinkId != right.NextLinkId ||
		left.Nodes.size() != right.Nodes.size() || left.Links.size() != right.Links.size()) return false;

	const bool sameNodes = std::equal(left.Nodes.begin(), left.Nodes.end(), right.Nodes.begin(),
		[](const GraphNode& a, const GraphNode& b)
		{
			return a.Id == b.Id && a.Type == b.Type && a.InputValues == b.InputValues;
		});

	const bool sameLinks = std::equal(left.Links.begin(), left.Links.end(), right.Links.begin(),
		[](const GraphLink& a, const GraphLink& b)
		{
			return a.Id == b.Id && a.From == b.From && a.To == b.To;
		});

	return sameNodes && sameLinks;
}

void AcceptGraphFile(const GraphFileResult& result)
{
	Expect(result.Succeeded(), "Graph file stage=" + std::to_string(static_cast<int>(result.Stage)) +
		", archive field=" + result.ArchiveError.Field + ", graph=" + result.GraphError.Message +
		", system=" + result.SystemError.message());
}

struct GraphFileTestDirectory
{
	std::filesystem::path Path;

	~GraphFileTestDirectory()
	{
		std::error_code ignored;
		std::filesystem::remove(Path / L"그래프.json", ignored);
		std::filesystem::remove(Path / "bad.json", ignored);
		std::filesystem::remove(Path, ignored);
	}
};

std::string ReadGraphTestBytes(const std::filesystem::path& path)
{
	std::ifstream file(path, std::ios::binary);
	Expect(static_cast<bool>(file), "Cannot read the graph test file.");
	std::string bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	Expect(!file.bad(), "Graph test file reading failed.");
	return bytes;
}

void CheckGraphFile(const GraphRegistry& registry)
{
	reflection::Registry types;
	Expect(reflection_generated::Register_GraphFoundationCheck(types), "Graph reflection registration failed.");
	const auto* type = types.FindType<GraphData>();
	Expect(type != nullptr, "GraphData reflection type is missing.");

	archive_reflection::PropertyCodecs codecs;
	Expect(archive_graph::RegisterValueCodec(codecs), "GraphValue codec registration failed.");

	const auto stamp = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	const auto directory = std::filesystem::current_path() / ("graph-file-check-" + stamp);
	Expect(std::filesystem::create_directory(directory), "Cannot reserve the graph file test directory.");
	GraphFileTestDirectory cleanup{ directory };

	const auto path = directory / L"그래프.json";
	const auto badPath = directory / "bad.json";

	Graph source;
	const auto number = Add(source, registry, "Core.Float");
	const auto sum = Add(source, registry, "Math.AddFloat");
	Accept(source.SetInputValue(registry, { number, "value" }, 2.0f));
	Accept(source.Connect(registry, { number, "result" }, { sum, "a" }));

	AcceptGraphFile(SaveGraphJson(path, source, registry, types, codecs));
	Graph restored(EGraphDomain::Shader);
	AcceptGraphFile(LoadGraphJson(path, restored, registry, types, codecs));
	Expect(SameGraphData(source.ExportData(), restored.ExportData()), "Graph file round-trip changed the data.");

	Accept(source.SetInputValue(registry, { number, "value" }, 9.0f));
	AcceptGraphFile(SaveGraphJson(path, source, registry, types, codecs));
	AcceptGraphFile(LoadGraphJson(path, restored, registry, types, codecs));
	Expect(SameGraphData(source.ExportData(), restored.ExportData()), "Existing graph file was not replaced.");

	const auto beforeBytes = ReadGraphTestBytes(path);
	const auto beforeGraph = restored.ExportData();

	Graph invalid(static_cast<EGraphDomain>(99));
	const auto invalidSave = SaveGraphJson(path, invalid, registry, types, codecs);
	Expect(!invalidSave.Succeeded() && invalidSave.Stage == EGraphFileStage::Validate &&
		ReadGraphTestBytes(path) == beforeBytes, "Invalid graph save changed the existing file.");

	reflection::Registry missingTypes;
	const auto missingType = SaveGraphJson(path, source, registry, missingTypes, codecs);
	Expect(!missingType.Succeeded() && missingType.Stage == EGraphFileStage::Serialize &&
		ReadGraphTestBytes(path) == beforeBytes, "Failed serialization changed the existing file.");

	auto writeBadFile = [&](std::string_view document)
		{
			std::ofstream file(badPath, std::ios::binary | std::ios::trunc);
			file.write(document.data(), static_cast<std::streamsize>(document.size()));
			file.close();
			Expect(static_cast<bool>(file), "Cannot write the invalid graph test file.");
		};

	writeBadFile("{");
	const auto malformed = LoadGraphJson(badPath, restored, registry, types, codecs);
	Expect(!malformed.Succeeded() && malformed.Stage == EGraphFileStage::Read &&
		SameGraphData(beforeGraph, restored.ExportData()), "Malformed JSON changed the graph.");

	writeBadFile(R"({"Graph":{}})");
	const auto missingField = LoadGraphJson(badPath, restored, registry, types, codecs);
	Expect(!missingField.Succeeded() && missingField.Stage == EGraphFileStage::Deserialize &&
		SameGraphData(beforeGraph, restored.ExportData()), "Missing fields changed the graph.");

	auto newer = source.ExportData();
	++newer.Version;
	archive::JsonArchive newerWriter;
	AcceptArchive(archive_reflection::WriteObject(
		newerWriter, "Graph", *type, reflection::ObjectView::From(newer), &types, &codecs));
	AcceptArchive(newerWriter.SaveToFile(badPath));

	const auto unsupported = LoadGraphJson(badPath, restored, registry, types, codecs);
	Expect(!unsupported.Succeeded() && unsupported.Stage == EGraphFileStage::Validate &&
		unsupported.GraphError.Code == EGraphError::UnsupportedVersion &&
		SameGraphData(beforeGraph, restored.ExportData()), "Unsupported version changed the graph.");

	const auto missingFile = LoadGraphJson(directory / "missing.json", restored, registry, types, codecs);
	Expect(!missingFile.Succeeded() && SameGraphData(beforeGraph, restored.ExportData()),
		"Missing file changed the graph.");

	const auto missingParent = SaveGraphJson(directory / "missing" / "graph.json", source, registry, types, codecs);
	Expect(!missingParent.Succeeded() && missingParent.Stage == EGraphFileStage::Write,
		"Saving into a missing parent directory succeeded.");

#ifdef _WIN32
	const HANDLE handle = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
		nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	Expect(handle != INVALID_HANDLE_VALUE, "Cannot lock the graph test file.");
	std::unique_ptr<void, decltype(&CloseHandle)> lock(handle, &CloseHandle);

	Accept(source.SetInputValue(registry, { number, "value" }, 13.0f));
	const auto blocked = SaveGraphJson(path, source, registry, types, codecs);
	lock.reset();

	Expect(!blocked.Succeeded() && blocked.Stage == EGraphFileStage::Replace &&
		ReadGraphTestBytes(path) == beforeBytes, "Failed replacement changed the existing file.");
#endif
}

void CheckGraphArchive(const GraphRegistry& registry)
{
	reflection::Registry types;
	Expect(reflection_generated::Register_GraphFoundationCheck(types), "Graph reflection registration failed.");

	const auto* type = types.FindType<GraphData>();
	Expect(type != nullptr, "GraphData reflection type is missing.");

	archive_reflection::PropertyCodecs codecs;
	Expect(archive_graph::RegisterValueCodec(codecs), "GraphValue codec registration failed.");

	Graph source;
	const auto number = Add(source, registry, "Core.Float");
	const auto sum = Add(source, registry, "Math.AddFloat");
	const auto integer = Add(source, registry, "Core.Int");

	Accept(source.SetInputValue(registry, { number, "value" }, 2.0f));
	Accept(source.SetInputValue(registry, { sum, "b" }, 3.0f));
	Accept(source.SetInputValue(registry, { integer, "value" }, std::int64_t{ 9007199254740993LL }));
	Accept(source.Connect(registry, { number, "result" }, { sum, "a" }));

	auto original = source.ExportData();
	original.NextNodeId = std::uint64_t{ 1 } << 63;
	original.NextLinkId = (std::numeric_limits<GraphLinkId>::max)();

	archive::JsonArchive writer;
	const auto written = archive_reflection::WriteObject(writer, "Graph", *type,
		reflection::ObjectView::From(original), &types, &codecs);
	AcceptArchive(written);

	std::string document;
	AcceptArchive(writer.Encode(document));

	archive::JsonArchive reader;
	AcceptArchive(reader.Decode(document));

	GraphData decoded;
	decoded.Domain = EGraphDomain::Shader;
	AcceptArchive(archive_reflection::ReadObject(reader, "Graph", *type, decoded, &types, &codecs));
	Expect(SameGraphData(original, decoded), "Graph JSON round-trip changed the data.");

	Graph restored(EGraphDomain::Shader);
	Accept(restored.ImportData(registry, decoded));
	Expect(SameGraphData(original, restored.ExportData()), "Graph import changed the decoded data.");

	auto invalidDocument = document;
	const std::string integerText = "9007199254740993";
	const auto position = invalidDocument.find(integerText);
	Expect(position != std::string::npos, "Large integer was not written as expected.");
	invalidDocument.replace(position, integerText.size(), "9223372036854775808");

	archive::JsonArchive invalidReader;
	AcceptArchive(invalidReader.Decode(invalidDocument));
	GraphData unchanged = decoded;
	const auto invalidRead = archive_reflection::ReadObject(
		invalidReader, "Graph", *type, unchanged, &types, &codecs);
	Expect(!invalidRead && invalidRead.Code == archive::ArchiveErrorCode::InvalidValue &&
		SameGraphData(decoded, unchanged), "Invalid nested value changed GraphData.");

	archive::JsonArchive missingReader;
	AcceptArchive(missingReader.Decode(R"({"Graph":{"Version":1,"Domain":1}})"));
	const auto missingRead = archive_reflection::ReadObject(
		missingReader, "Graph", *type, unchanged, &types, &codecs);
	Expect(!missingRead && missingRead.Code == archive::ArchiveErrorCode::MissingField &&
		SameGraphData(decoded, unchanged), "Missing required field changed GraphData.");

	auto newer = decoded;
	++newer.Version;
	Reject(restored.ImportData(registry, newer), EGraphError::UnsupportedVersion);
	Expect(SameGraphData(decoded, restored.ExportData()), "Rejected version changed the graph.");
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
		CheckGraphData(registry);
		CheckGraphValueArchive();
		CheckGraphArchive(registry);
		CheckGraphFile(registry);
		std::cout << "Graph foundation checks passed. No graph execution or shader compilation was performed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << "Graph foundation check failed: " << error.what() << '\n';
		return 1;
	}
}