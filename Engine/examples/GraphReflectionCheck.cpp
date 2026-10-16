#include "Graph.h"
#include "GraphReflection.h"
#include "GraphReflectionCheck.gen.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>

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

void Metadata(reflection::Metadata& metadata, std::string key, reflection::MetadataValue value)
{
	Expect(metadata.Add(std::move(key), std::move(value)), "Duplicate test metadata.");
}

reflection::FunctionInfo Function()
{
	auto function = reflection::MakeFunction<&graph_checks::Add>();
	function.Name = "Add";
	function.ReturnType = "float";
	function.Signature = "float(float, float)";
	function.Parameters = { { "left", "float" }, { "right", "float" } };
	Metadata(function.Metadata, "Graph.Pure", true);
	Metadata(function.Metadata, "Graph.Id", std::string("Check.Add"));
	return function;
}

void CheckGenerated()
{
	reflection::Registry reflection;
	Expect(reflection_generated::Register_GraphReflectionCheck(reflection), "Generated reflection failed.");
	const auto types = reflection.GetTypes();
	Expect(types.size() == 1 && types.front()->QualifiedName == "graph_checks::Counter", "Type enumeration failed.");
	const auto functions = reflection.GetFreeFunctions();
	Expect(!functions.empty(), "Free function enumeration failed.");
	Expect(std::is_sorted(functions.begin(), functions.end(), [](const auto& a, const auto& b)
	{
		return a.QualifiedName < b.QualifiedName;
	}), "Free function enumeration is not sorted.");
	const auto found = std::find_if(functions.begin(), functions.end(), [](const auto& entry)
	{
		return entry.QualifiedName == "graph_checks::Add";
	});
	Expect(found != functions.end() && found->Function->CanonicalReturnType == "float",
		"Return aliases were not resolved by the generator.");

	GraphRegistry graph;
	Accept(graph.RegisterBuiltins());
	Accept(RegisterReflectedGraphNodes(graph, reflection));
	const auto* add = graph.Find("Check.Add");
	Expect(add && add->DisplayName == "Add" && add->Category == "Check/Math" &&
		add->Tooltip == "Add two floats." && add->Pins.size() == 3, "Presentation or pure pins were lost.");
	Expect(!graph.FindPin("Check.Add", "exec") && !graph.FindPin("Check.Add", "target"),
		"Pure free function has flow pins.");
	const auto* a = graph.FindPin("Check.Add", "a");
	Expect(a && a->DisplayName == "Left" && std::get<float>(a->DefaultValue) == 0.0f,
		"Parameter metadata macros were not expanded correctly.");
	Expect(graph.FindPin("Check.Add", "right") && !graph.FindPin("Check.Add", "left"), "Stable pin key was not applied.");
	Expect(graph.FindPin("Check.Counter.Assign", "exec") && graph.FindPin("Check.Counter.Assign", "then") &&
		graph.FindPin("Check.Counter.Assign", "target"), "Callable member pins were not generated.");
	Expect(graph.FindPin("Check.Counter.Positive", "target") && !graph.FindPin("Check.Counter.Positive", "exec"),
		"Pure member pins were not generated.");
	Expect(!graph.FindPin("Check.Counter.Twice", "target"), "Static function acquired a target pin.");
	Expect(graph.Find("Check.Counter.Value.Get") && graph.Find("Check.Counter.Value.Set") &&
		graph.Find("Check.Counter.Enabled.Get") && !graph.Find("Check.Counter.Enabled.Set"), "Property policy was ignored.");
	const auto* get = graph.Find("Check.Counter.Value.Get");
	Expect(get->Reflection.Kind == EGraphReflectionKind::PropertyGet &&
		get->Reflection.Owner == "graph_checks::Counter" && get->Reflection.Member == "Value", "Property lookup was lost.");
	Expect(add->Reflection.Kind == EGraphReflectionKind::Function && add->Reflection.Member == "graph_checks::Add" &&
		!add->Reflection.Signature.empty(), "Function lookup was lost.");
	Expect(graph.Find("Math.AbsFloat") && graph.Find("Logic.Xor"), "Annotated engine library was not discovered.");
	const auto shader = graph.GetTypes(EGraphDomain::Shader);
	Expect(std::find(shader.begin(), shader.end(), "Check.Add") == shader.end(), "CPU function leaked into Shader.");
	const auto count = graph.GetTypes(EGraphDomain::Logic).size();
	Expect(!RegisterReflectedGraphNodes(graph, reflection).Succeeded(), "Duplicate reflection batch succeeded.");
	Expect(graph.GetTypes(EGraphDomain::Logic).size() == count, "Duplicate batch changed existing schemas.");

	Graph document;
	const auto node = document.AddNode(graph, "Check.Counter.Value.Get");
	Accept(node);
	const auto missing = document.Validate(graph);
	Expect(missing.size() == 1 && missing.front().Pin == "target", "An object target became optional.");
	Expect(!document.SetInputValue(graph, { node.Node, "target" }, std::int64_t{ 0 }).Succeeded(),
		"Object target accepted a literal value.");
	Accept(graph.Register({ "Check.Object", { EGraphDomain::Logic }, {
		{ "result", Direction::Output, Type::Object, {}, "graph_checks::Counter" } } }));
	Accept(graph.Register({ "Check.WrongObject", { EGraphDomain::Logic }, {
		{ "result", Direction::Output, Type::Object, {}, "Other" } } }));
	const auto object = document.AddNode(graph, "Check.Object");
	const auto wrong = document.AddNode(graph, "Check.WrongObject");
	Accept(object);
	Accept(wrong);
	Expect(document.CanConnect(graph, { wrong.Node, "result" }, { node.Node, "target" }).Code == EGraphError::TypeMismatch,
		"Different object owners were accepted.");
	Accept(document.Connect(graph, { object.Node, "result" }, { node.Node, "target" }));
	Expect(document.Validate(graph).empty(), "Matching object connection failed validation.");
}

void CheckRejection(reflection::FunctionInfo function)
{
	reflection::Registry reflection;
	Expect(reflection.RegisterFreeFunction("graph_checks::Bad", std::move(function)), "Test registration failed.");
	GraphRegistry graph;
	Accept(graph.RegisterBuiltins());
	const auto count = graph.GetTypes(EGraphDomain::Logic).size();
	const auto result = RegisterReflectedGraphNodes(graph, reflection);
	Expect(!result.Succeeded() && result.Message.find("graph_checks::Bad") != std::string::npos,
		"Invalid graph annotation was not diagnosed at its source.");
	Expect(!graph.Find("Check.Add") && graph.GetTypes(EGraphDomain::Logic).size() == count,
		"Failed batch changed the destination.");
}

void CheckFailures()
{
	auto function = Function();
	Metadata(function.Metadata, "Graph.Callable", true);
	CheckRejection(function);
	function = Function();
	Metadata(function.Metadata, "Graph.PinDefault.misspelled", 0.0);
	CheckRejection(function);
	function = Function();
	Metadata(function.Metadata, "Graph.PinDefault.left", std::int64_t{ 0 });
	CheckRejection(function);
	function = Function();
	Metadata(function.Metadata, "Graph.PinDefault.left", std::numeric_limits<double>::infinity());
	CheckRejection(function);
	function = Function();
	Metadata(function.Metadata, "Graph.PinKey.left", std::string("result"));
	CheckRejection(function);
	function = Function();
	function.Parameters.front().TypeName = "const float&";
	CheckRejection(function);
	function = Function();
	function.ReturnType = "void";
	CheckRejection(function);
	function = Function();
	function.Parameters.front().Name.clear();
	CheckRejection(function);
	function = Function();
	function.ReturnType = "float*";
	CheckRejection(function);
	function = reflection::FunctionInfo::DescribeOnly(reflection::InvocationBindingStatus::InaccessibleMember);
	function.Name = "Hidden";
	function.Signature = "float()";
	function.ReturnType = "float";
	Metadata(function.Metadata, "Graph.Pure", true);
	Metadata(function.Metadata, "Graph.Id", std::string("Check.Add"));
	CheckRejection(function);

	// A late failure rolls back earlier successful descriptor insertions in the same batch.
	reflection::Registry reflection;
	Expect(reflection_generated::Register_GraphReflectionCheck(reflection), "Generated registration failed.");
	GraphRegistry graph;
	Accept(graph.Register({ "Check.Counter.Value.Set", { EGraphDomain::Logic }, {} }));
	Expect(!RegisterReflectedGraphNodes(graph, reflection).Succeeded(), "Property collision succeeded.");
	Expect(graph.GetTypes(EGraphDomain::Logic).size() == 1 && !graph.Find("Check.Add") &&
		!graph.Find("Check.Counter.Value.Get"), "Late failure left partial nodes.");
}

void CheckPropertyFailures()
{
	enum class PropertyCase { MissingId, ConflictingPolicy, ConstWrite };
	for (auto scenario : { PropertyCase::MissingId, PropertyCase::ConflictingPolicy, PropertyCase::ConstWrite })
	{
		auto owner = reflection::TypeInfo::For<graph_checks::Counter>("graph_checks::Counter");
		auto property = reflection::MakeProperty<&graph_checks::Counter::Enabled>("Enabled", "const bool");
		if (scenario != PropertyCase::MissingId) Metadata(property.Metadata, "Graph.Id", std::string("Check.Enabled"));
		Metadata(property.Metadata, "Graph.ReadOnly", true);
		if (scenario == PropertyCase::ConflictingPolicy) Metadata(property.Metadata, "Graph.ReadWrite", true);
		if (scenario == PropertyCase::ConstWrite)
		{
			property.Metadata = {};
			Metadata(property.Metadata, "Graph.Id", std::string("Check.Enabled"));
			Metadata(property.Metadata, "Graph.ReadWrite", true);
		}
		owner.Properties.push_back(std::move(property));
		reflection::Registry reflection;
		Expect(reflection.Register(std::move(owner)), "Test type registration failed.");
		GraphRegistry graph;
		Expect(!RegisterReflectedGraphNodes(graph, reflection).Succeeded(), "Invalid property exposure succeeded.");
		Expect(graph.GetTypes(EGraphDomain::Logic).empty(), "Invalid property exposure left partial nodes.");
	}

	// Reflection visibility alone never opts a function into the graph palette.
	auto function = Function();
	function.Metadata = {};
	reflection::Registry reflection;
	Expect(reflection.RegisterFreeFunction("graph_checks::Unexposed", std::move(function)), "Test registration failed.");
	GraphRegistry graph;
	Accept(RegisterReflectedGraphNodes(graph, reflection));
	Expect(graph.GetTypes(EGraphDomain::Logic).empty(), "Unannotated function was exposed.");
}
}

int main()
{
	try
	{
		CheckGenerated();
		CheckFailures();
		CheckPropertyFailures();
		std::cout << "Graph reflection checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
