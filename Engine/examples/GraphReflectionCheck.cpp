#include "Graph.h"
#include "GraphReflection.h"
#include "GraphReflectionCheck.gen.h"
#include "ReflectionRegistry.h"
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

void CheckExpandedGameplayNodes()
{
	struct ReflectionScope
	{
		bool created = false;
		ReflectionScope()
		{
			if (!ReflectionRegistry::IsCreated())
			{
				Expect(ReflectionRegistry::Create() != nullptr, "Failed to create ReflectionRegistry.");
				created = true;
			}
		}
		~ReflectionScope()
		{
			if (created) ReflectionRegistry::Destroy();
		}
	} reflectionScope;

	const auto& engineReflection = ReflectionRegistry::Get().GetRegistry();
	GraphRegistry graph;
	Accept(graph.RegisterBuiltins());
	Accept(RegisterReflectedGraphNodes(graph, engineReflection));

	// 1. SaveGame Nodes
	const auto* writeCheckpoint = graph.Find("Save.WriteCheckpointSlot");
	Expect(writeCheckpoint != nullptr, "Save.WriteCheckpointSlot node missing.");
	Expect(writeCheckpoint->Category == "SaveGame", "Save.WriteCheckpointSlot wrong category.");
	Expect(graph.FindPin("Save.WriteCheckpointSlot", "exec") != nullptr, "Save.WriteCheckpointSlot missing exec pin.");
	Expect(graph.FindPin("Save.WriteCheckpointSlot", "then") != nullptr, "Save.WriteCheckpointSlot missing then pin.");
	const auto* slotNamePin = graph.FindPin("Save.WriteCheckpointSlot", "slotName");
	Expect(slotNamePin && slotNamePin->Type == Type::String, "Save.WriteCheckpointSlot slotName pin invalid.");
	const auto* checkpointIdPin = graph.FindPin("Save.WriteCheckpointSlot", "checkpointId");
	Expect(checkpointIdPin && checkpointIdPin->Type == Type::String, "Save.WriteCheckpointSlot checkpointId pin invalid.");
	const auto* writeResultPin = graph.FindPin("Save.WriteCheckpointSlot", "result");
	Expect(writeResultPin && writeResultPin->Type == Type::Bool && writeResultPin->Direction == Direction::Output,
		"Save.WriteCheckpointSlot result pin invalid.");

	const auto* loadCheckpoint = graph.Find("Save.LoadCheckpointSlot");
	Expect(loadCheckpoint != nullptr, "Save.LoadCheckpointSlot node missing.");
	Expect(graph.FindPin("Save.LoadCheckpointSlot", "slotName") != nullptr, "Save.LoadCheckpointSlot slotName pin missing.");
	const auto* loadResultPin = graph.FindPin("Save.LoadCheckpointSlot", "result");
	Expect(loadResultPin && loadResultPin->Type == Type::Bool, "Save.LoadCheckpointSlot result pin invalid.");

	const auto* hasSlot = graph.Find("Save.HasSlot");
	Expect(hasSlot != nullptr, "Save.HasSlot node missing.");
	Expect(!graph.FindPin("Save.HasSlot", "exec"), "Save.HasSlot is pure; should not have exec pin.");
	const auto* hasSlotResult = graph.FindPin("Save.HasSlot", "result");
	Expect(hasSlotResult && hasSlotResult->Type == Type::Bool, "Save.HasSlot result pin invalid.");

	const auto* deleteSlot = graph.Find("Save.DeleteSlot");
	Expect(deleteSlot != nullptr, "Save.DeleteSlot node missing.");
	Expect(graph.FindPin("Save.DeleteSlot", "exec") != nullptr, "Save.DeleteSlot missing exec pin.");
	const auto* deleteResult = graph.FindPin("Save.DeleteSlot", "result");
	Expect(deleteResult && deleteResult->Type == Type::Bool, "Save.DeleteSlot result pin invalid.");

	// 2. Navigation & AI Nodes
	const auto* setDest = graph.Find("Nav.SetDestination");
	Expect(setDest != nullptr, "Nav.SetDestination node missing.");
	Expect(setDest->Category == "Navigation", "Nav.SetDestination wrong category.");
	const auto* navEntityPin = graph.FindPin("Nav.SetDestination", "entityId");
	Expect(navEntityPin && navEntityPin->Type == Type::Int, "Nav.SetDestination entityId pin invalid.");
	const auto* navXPin = graph.FindPin("Nav.SetDestination", "x");
	Expect(navXPin && navXPin->Type == Type::Float, "Nav.SetDestination x pin invalid.");
	const auto* navYPin = graph.FindPin("Nav.SetDestination", "y");
	Expect(navYPin && navYPin->Type == Type::Float, "Nav.SetDestination y pin invalid.");
	const auto* navZPin = graph.FindPin("Nav.SetDestination", "z");
	Expect(navZPin && navZPin->Type == Type::Float, "Nav.SetDestination z pin invalid.");

	const auto* stopNav = graph.Find("Nav.Stop");
	Expect(stopNav != nullptr, "Nav.Stop node missing.");
	Expect(stopNav->Category == "Navigation", "Nav.Stop wrong category.");

	const auto* reachedNav = graph.Find("Nav.HasReachedDestination");
	Expect(reachedNav != nullptr, "Nav.HasReachedDestination node missing.");
	Expect(!graph.FindPin("Nav.HasReachedDestination", "exec"), "Nav.HasReachedDestination is pure.");
	const auto* reachedResult = graph.FindPin("Nav.HasReachedDestination", "result");
	Expect(reachedResult && reachedResult->Type == Type::Bool, "Nav.HasReachedDestination result pin invalid.");

	const auto* getTarget = graph.Find("AI.GetDetectedTarget");
	Expect(getTarget != nullptr, "AI.GetDetectedTarget node missing.");
	Expect(getTarget->Category == "AI", "AI.GetDetectedTarget wrong category.");
	Expect(!graph.FindPin("AI.GetDetectedTarget", "exec"), "AI.GetDetectedTarget is pure.");
	const auto* targetResult = graph.FindPin("AI.GetDetectedTarget", "result");
	Expect(targetResult && targetResult->Type == Type::Int, "AI.GetDetectedTarget result pin invalid.");

	const auto* hasLos = graph.Find("AI.HasLineOfSight");
	Expect(hasLos != nullptr, "AI.HasLineOfSight node missing.");
	Expect(hasLos->Category == "AI", "AI.HasLineOfSight wrong category.");
	Expect(!graph.FindPin("AI.HasLineOfSight", "exec"), "AI.HasLineOfSight is pure.");
	const auto* losResult = graph.FindPin("AI.HasLineOfSight", "result");
	Expect(losResult && losResult->Type == Type::Bool, "AI.HasLineOfSight result pin invalid.");

	// 3. AudioMixer Nodes
	const auto* setBusVol = graph.Find("Audio.SetBusVolume");
	Expect(setBusVol != nullptr, "Audio.SetBusVolume node missing.");
	Expect(setBusVol->Category == "Audio", "Audio.SetBusVolume wrong category.");
	const auto* busNamePin = graph.FindPin("Audio.SetBusVolume", "busName");
	Expect(busNamePin && busNamePin->Type == Type::String, "Audio.SetBusVolume busName pin invalid.");
	const auto* busVolPin = graph.FindPin("Audio.SetBusVolume", "volume");
	Expect(busVolPin && busVolPin->Type == Type::Float, "Audio.SetBusVolume volume pin invalid.");

	const auto* getBusVol = graph.Find("Audio.GetBusVolume");
	Expect(getBusVol != nullptr, "Audio.GetBusVolume node missing.");
	Expect(getBusVol->Category == "Audio", "Audio.GetBusVolume wrong category.");
	Expect(!graph.FindPin("Audio.GetBusVolume", "exec"), "Audio.GetBusVolume is pure.");
	const auto* getBusVolResult = graph.FindPin("Audio.GetBusVolume", "result");
	Expect(getBusVolResult && getBusVolResult->Type == Type::Float, "Audio.GetBusVolume result pin invalid.");

	const auto* setBusMuted = graph.Find("Audio.SetBusMuted");
	Expect(setBusMuted != nullptr, "Audio.SetBusMuted node missing.");
	const auto* mutedPin = graph.FindPin("Audio.SetBusMuted", "muted");
	Expect(mutedPin && mutedPin->Type == Type::Bool, "Audio.SetBusMuted muted pin invalid.");

	const auto* fadeBusVol = graph.Find("Audio.FadeBusVolume");
	Expect(fadeBusVol != nullptr, "Audio.FadeBusVolume node missing.");
	const auto* targetVolPin = graph.FindPin("Audio.FadeBusVolume", "targetVolume");
	Expect(targetVolPin && targetVolPin->Type == Type::Float, "Audio.FadeBusVolume targetVolume pin invalid.");
	const auto* durPin = graph.FindPin("Audio.FadeBusVolume", "duration");
	Expect(durPin && durPin->Type == Type::Float, "Audio.FadeBusVolume duration pin invalid.");

	// 4. SceneStreaming Nodes
	const auto* markPersistent = graph.Find("Scene.MarkDontDestroyOnLoad");
	Expect(markPersistent != nullptr, "Scene.MarkDontDestroyOnLoad node missing.");
	Expect(markPersistent->Category == "Scene", "Scene.MarkDontDestroyOnLoad wrong category.");
	const auto* sceneEntityPin = graph.FindPin("Scene.MarkDontDestroyOnLoad", "entityId");
	Expect(sceneEntityPin && sceneEntityPin->Type == Type::Int, "Scene.MarkDontDestroyOnLoad entityId pin invalid.");
	const auto* groupPin = graph.FindPin("Scene.MarkDontDestroyOnLoad", "group");
	Expect(groupPin && groupPin->Type == Type::String, "Scene.MarkDontDestroyOnLoad group pin invalid.");

	const auto* unloadAdditive = graph.Find("Scene.UnloadAdditive");
	Expect(unloadAdditive != nullptr, "Scene.UnloadAdditive node missing.");
	Expect(unloadAdditive->Category == "Scene", "Scene.UnloadAdditive wrong category.");
	const auto* unloadSubScenePin = graph.FindPin("Scene.UnloadAdditive", "subSceneName");
	Expect(unloadSubScenePin && unloadSubScenePin->Type == Type::String, "Scene.UnloadAdditive subSceneName pin invalid.");
	const auto* unloadResult = graph.FindPin("Scene.UnloadAdditive", "result");
	Expect(unloadResult && unloadResult->Type == Type::Bool, "Scene.UnloadAdditive result pin invalid.");

	// 5. Network RPC & Diagnostics Nodes
	const auto* sendServerRpc = graph.Find("Net.SendServerRpc");
	Expect(sendServerRpc != nullptr && sendServerRpc->Category == "Network", "Net.SendServerRpc node missing or wrong category.");
	Expect(graph.FindPin("Net.SendServerRpc", "exec") != nullptr, "Net.SendServerRpc missing exec pin.");
	const auto* rpcNetIdPin = graph.FindPin("Net.SendServerRpc", "netId");
	const auto* rpcNamePin = graph.FindPin("Net.SendServerRpc", "rpcName");
	const auto* rpcPayloadPin = graph.FindPin("Net.SendServerRpc", "payloadText");
	const auto* rpcResultPin = graph.FindPin("Net.SendServerRpc", "result");
	Expect(rpcNetIdPin && rpcNetIdPin->Type == Type::Int, "Net.SendServerRpc netId pin invalid.");
	Expect(rpcNamePin && rpcNamePin->Type == Type::String, "Net.SendServerRpc rpcName pin invalid.");
	Expect(rpcPayloadPin && rpcPayloadPin->Type == Type::String, "Net.SendServerRpc payloadText pin invalid.");
	Expect(rpcResultPin && rpcResultPin->Type == Type::Bool, "Net.SendServerRpc result pin invalid.");

	const auto* sendMulticastRpc = graph.Find("Net.SendMulticastRpc");
	Expect(sendMulticastRpc != nullptr && sendMulticastRpc->Category == "Network", "Net.SendMulticastRpc node missing.");
	Expect(graph.FindPin("Net.SendMulticastRpc", "exec") != nullptr, "Net.SendMulticastRpc missing exec pin.");

	const auto* isServer = graph.Find("Net.IsServer");
	Expect(isServer != nullptr && isServer->Category == "Network" && !graph.FindPin("Net.IsServer", "exec"),
		"Net.IsServer pure node invalid.");
	const auto* isClient = graph.Find("Net.IsClient");
	Expect(isClient != nullptr && isClient->Category == "Network" && !graph.FindPin("Net.IsClient", "exec"),
		"Net.IsClient pure node invalid.");
	const auto* simLatency = graph.Find("Net.GetSimulatedLatency");
	Expect(simLatency != nullptr && simLatency->Category == "Network" && !graph.FindPin("Net.GetSimulatedLatency", "exec"),
		"Net.GetSimulatedLatency pure node invalid.");
	const auto* simLatencyResult = graph.FindPin("Net.GetSimulatedLatency", "result");
	Expect(simLatencyResult && simLatencyResult->Type == Type::Float, "Net.GetSimulatedLatency result pin invalid.");

	// 6. Camera Sequencer Nodes
	const auto* camSeqPlay = graph.Find("CameraSequencer.Play");
	Expect(camSeqPlay != nullptr && camSeqPlay->Category == "Camera", "CameraSequencer.Play node missing.");
	Expect(graph.FindPin("CameraSequencer.Play", "exec") != nullptr, "CameraSequencer.Play missing exec pin.");
	const auto* camSeqStartPin = graph.FindPin("CameraSequencer.Play", "startTime");
	Expect(camSeqStartPin && camSeqStartPin->Type == Type::Float, "CameraSequencer.Play startTime pin invalid.");

	const auto* camSeqStop = graph.Find("CameraSequencer.Stop");
	Expect(camSeqStop != nullptr && camSeqStop->Category == "Camera", "CameraSequencer.Stop node missing.");
	const auto* camSeqPlaying = graph.Find("CameraSequencer.IsPlaying");
	Expect(camSeqPlaying != nullptr && camSeqPlaying->Category == "Camera" && !graph.FindPin("CameraSequencer.IsPlaying", "exec"),
		"CameraSequencer.IsPlaying pure node invalid.");

	// 7. PakArchive Mount Nodes
	const auto* pakMounted = graph.Find("Pak.IsMounted");
	Expect(pakMounted != nullptr && pakMounted->Category == "Resource" && !graph.FindPin("Pak.IsMounted", "exec"),
		"Pak.IsMounted pure node invalid.");
	const auto* pakHasEntry = graph.Find("Pak.HasEntry");
	Expect(pakHasEntry != nullptr && pakHasEntry->Category == "Resource" && !graph.FindPin("Pak.HasEntry", "exec"),
		"Pak.HasEntry pure node invalid.");
	const auto* pakUriPin = graph.FindPin("Pak.HasEntry", "uri");
	const auto* pakResultPin = graph.FindPin("Pak.HasEntry", "result");
	Expect(pakUriPin && pakUriPin->Type == Type::String, "Pak.HasEntry uri pin invalid.");
	Expect(pakResultPin && pakResultPin->Type == Type::Bool, "Pak.HasEntry result pin invalid.");
}
}

int main()
{
	try
	{
		CheckGenerated();
		CheckFailures();
		CheckPropertyFailures();
		CheckExpandedGameplayNodes();
		std::cout << "Graph reflection checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
