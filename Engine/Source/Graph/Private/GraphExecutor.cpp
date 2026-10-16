#include "GraphExecutor.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace Engine
{
namespace
{
bool SupportsBuiltin(const std::string& type)
{
	const char* types[]{ "Core.Bool", "Core.Int", "Core.Float", "Core.Float2", "Core.Float3", "Core.Float4",
		"Math.AddFloat", "Math.MultiplyFloat", "Math.GreaterFloat", "Math.LessFloat", "Logic.And", "Logic.Or",
		"Logic.Not", "Logic.SelectFloat", "Event.Start", "Event.Update", "Flow.Branch", "Flow.Sequence",
		"Flow.ForLoop", "Flow.WhileLoop", "Flow.DoOnce", "Flow.Gate", "Flow.FlipFlop", "Logic.FloatResult",
		"Logic.BoolResult", "Logic.IntResult", "Logic.Float2Result", "Logic.Float3Result", "Logic.Float4Result" };
	return std::find(std::begin(types), std::end(types), type) != std::end(types);
}
}

void GraphExecutor::Clear()
{
	m_Pending.clear();
	m_Active.clear();
	m_Evaluating.clear();
	m_Outputs.clear();
	m_NodeStates.clear();
	m_Functions.clear();
	m_Trace.clear();
	m_Graph = Graph{};
	m_Registry = GraphRegistry{};
	m_Result = {};
	m_Diagnostic = {};
	m_State = EGraphExecutionState::Idle;
	m_Budget = 0;
	m_Sequence = 0;
	m_Loaded = false;
	m_Started = false;
}

GraphDiagnostic GraphExecutor::Load(const Graph& graph, const GraphRegistry& registry,
	const reflection::Registry* sourceRegistry)
{
	Clear();
	if (graph.GetDomain() != EGraphDomain::Logic)
		return Fail(0, {}, "Only Logic graphs can execute on this interpreter.", EGraphError::UnsupportedExecution);
	const auto errors = graph.Validate(registry);
	if (!errors.empty())
	{
		const auto& error = errors.front();
		return Fail(error.Node, error.Pin, error.Message, error.Code);
	}
	m_Graph = graph;
	m_Registry = registry;
	for (const auto& node : graph.GetNodes())
	{
		const auto& definition = *registry.Find(node.Type);
		if (std::any_of(definition.Pins.begin(), definition.Pins.end(),
			[](const GraphPin& pin) { return pin.Type == EGraphPinType::Object; }))
			return Fail(node.Id, "target", "Object access requires a scene binding and is not supported yet.",
				EGraphError::UnsupportedExecution);
		const auto& source = definition.Reflection;
		if (source.Kind == EGraphReflectionKind::None)
		{
			if (!SupportsBuiltin(node.Type))
				return Fail(node.Id, {}, "No execution binding for node type: " + node.Type,
					EGraphError::UnsupportedExecution);
			continue;
		}
		if (source.Kind != EGraphReflectionKind::Function || !sourceRegistry)
			return Fail(node.Id, {}, "The reflected execution source is unavailable.", EGraphError::UnsupportedExecution);
		const reflection::FunctionInfo* function = nullptr;
		if (source.Owner.empty()) function = sourceRegistry->FindFreeFunction(source.Member, source.Signature);
		else if (const auto* owner = sourceRegistry->FindType(source.Owner))
			function = owner->FindFunction(source.Member, source.Signature);
		const auto* idValue = function ? function->Metadata.Find("Graph.Id") : nullptr;
		const auto* id = idValue ? std::get_if<std::string>(idValue) : nullptr;
		if (!function || !function->CanInvokeDynamicWithoutObject() || !id || *id != node.Type)
			return Fail(node.Id, {}, "No matching dynamic free/static function binding.", EGraphError::UnsupportedExecution);
		m_Functions.emplace(node.Type, *function);
	}
	m_Loaded = true;
	return {};
}

GraphDiagnostic GraphExecutor::Begin(GraphNodeId entry, float delta, std::size_t budget)
{
	if (!m_Loaded || m_State == EGraphExecutionState::Running || m_State == EGraphExecutionState::Paused)
		return { EGraphError::InvalidExecutionState, entry, {}, 0,
			"Load a graph and finish or stop the current event first." };
	const auto* node = m_Graph.FindNode(entry);
	if (!node || (node->Type != "Event.Start" && node->Type != "Event.Update"))
		return { EGraphError::InvalidExecutionState, entry, {}, 0, "Select a Start or Update event entry." };
	if (node->Type == "Event.Start" && m_Started)
		return { EGraphError::InvalidExecutionState, entry, {}, 0, "Start already ran in this instance. Reload to restart." };
	if (!std::isfinite(delta) || delta < 0.0f || budget == 0)
		return { EGraphError::InvalidExecutionState, entry, {}, 0, "Delta must be finite/non-negative and budget positive." };
	m_Pending.clear();
	m_Active.clear();
	m_Evaluating.clear();
	m_Outputs.clear();
	m_Result = {};
	m_Diagnostic = {};
	m_Budget = budget;
	m_State = EGraphExecutionState::Paused;
	if (node->Type == "Event.Start") m_Started = true;
	else Store(entry, "delta", delta);
	m_Pending.push_back({ TaskKind::Enter, entry });
	return {};
}

void GraphExecutor::Resume()
{
	if (m_State == EGraphExecutionState::Paused) m_State = EGraphExecutionState::Running;
}

void GraphExecutor::Pause()
{
	if (m_State == EGraphExecutionState::Running) m_State = EGraphExecutionState::Paused;
}

void GraphExecutor::Stop()
{
	m_Pending.clear();
	m_Active.clear();
	m_Evaluating.clear();
	if (m_Loaded) m_State = EGraphExecutionState::Stopped;
}

GraphDiagnostic GraphExecutor::Fail(GraphNodeId node, std::string pin, std::string message, EGraphError code)
{
	m_Diagnostic = { code, node, pin, 0, message };
	m_State = EGraphExecutionState::Failed;
	Trace(EGraphTraceKind::Error, node, std::move(pin), 0, {}, std::move(message));
	m_Pending.clear();
	m_Active.clear();
	return m_Diagnostic;
}

bool GraphExecutor::Consume(GraphNodeId node)
{
	if (m_Budget > 0) { --m_Budget; return true; }
	Fail(node, {}, "Execution budget exhausted.", EGraphError::ExecutionLimit);
	return false;
}

void GraphExecutor::Trace(EGraphTraceKind kind, GraphNodeId node, std::string pin, GraphLinkId link,
	GraphValue value, std::string message)
{
	constexpr std::size_t capacity = 2048;
	if (m_Trace.size() == capacity) m_Trace.pop_front();
	m_Trace.push_back({ ++m_Sequence, kind, node, std::move(pin), link, std::move(value), std::move(message) });
}

void GraphExecutor::PushOutput(GraphNodeId node, const std::string& pin)
{
	for (const auto& link : m_Graph.GetLinks())
		if (link.From == GraphPinReference{ node, pin })
		{
			m_Pending.push_back({ TaskKind::Enter, link.To.Node, link.To.Pin, link.Id });
			return;
		}
}

void GraphExecutor::Step()
{
	if (m_State != EGraphExecutionState::Paused && m_State != EGraphExecutionState::Running) return;
	if (m_Pending.empty()) { m_State = EGraphExecutionState::Completed; return; }
	const auto task = m_Pending.back();
	m_Pending.pop_back();
	if (!Consume(task.Node)) return;
	try { Execute(task); }
	catch (const std::exception& error)
	{
		if (m_State != EGraphExecutionState::Failed) Fail(task.Node, task.Pin, error.what());
	}
	catch (...)
	{
		if (m_State != EGraphExecutionState::Failed) Fail(task.Node, task.Pin, "Unknown execution error.");
	}
	if (m_State != EGraphExecutionState::Failed && m_Pending.empty()) m_State = EGraphExecutionState::Completed;
}

void GraphExecutor::Tick(std::size_t maxTasks)
{
	for (std::size_t i = 0; i < maxTasks && m_State == EGraphExecutionState::Running; ++i) Step();
}

void GraphExecutor::Execute(const Task& task)
{
	const auto* node = m_Graph.FindNode(task.Node);
	if (!node) throw std::runtime_error("Execution node is missing.");
	const auto id = node->Id;
	const auto& type = node->Type;
	if (task.Kind == TaskKind::Leave)
	{
		m_Active.erase(id);
		Trace(EGraphTraceKind::Leave, id);
		return;
	}
	if (task.Kind == TaskKind::ForNext)
	{
		m_Outputs[id].erase("index");
		if (task.Index >= task.Last) { PushOutput(id, "completed"); return; }
		Store(id, "index", task.Index);
		Trace(EGraphTraceKind::Iteration, id, "index", 0, task.Index);
		m_Pending.push_back({ TaskKind::ForNext, id, {}, 0, task.Index + 1, task.Last });
		PushOutput(id, "body");
		return;
	}
	if (task.Kind == TaskKind::WhileNext)
	{
		const bool condition = std::get<bool>(ReadInput(id, "condition"));
		Trace(EGraphTraceKind::Iteration, id, "condition", 0, condition);
		if (!condition) PushOutput(id, "completed");
		else { m_Pending.push_back(task); PushOutput(id, "body"); }
		return;
	}
	if (task.Link) Trace(EGraphTraceKind::Link, id, task.Pin, task.Link);
	Trace(EGraphTraceKind::Enter, id, task.Pin);
	if (!m_Active.insert(id).second) throw std::runtime_error("Re-entry into an active node is not supported.");
	m_Pending.push_back({ TaskKind::Leave, id });
	if (type == "Event.Start" || type == "Event.Update") PushOutput(id, "exec");
	else if (type == "Flow.Branch") PushOutput(id, std::get<bool>(ReadInput(id, "condition")) ? "true" : "false");
	else if (type == "Flow.Sequence") { PushOutput(id, "then1"); PushOutput(id, "then0"); }
	else if (type == "Flow.ForLoop")
	{
		const auto first = std::get<std::int64_t>(ReadInput(id, "first"));
		const auto last = std::get<std::int64_t>(ReadInput(id, "last"));
		m_Pending.push_back({ TaskKind::ForNext, id, {}, 0, first, last });
	}
	else if (type == "Flow.WhileLoop") m_Pending.push_back({ TaskKind::WhileNext, id });
	else if (type == "Flow.DoOnce")
	{
		auto& state = m_NodeStates[id];
		if (!state.Initialized)
		{
			state.Open = !std::get<bool>(ReadInput(id, "startClosed"));
			state.Initialized = true;
		}
		if (task.Pin == "reset") state.Open = true;
		else if (state.Open) { state.Open = false; PushOutput(id, "then"); }
	}
	else if (type == "Flow.Gate")
	{
		auto& state = m_NodeStates[id];
		if (!state.Initialized)
		{
			state.Open = std::get<bool>(ReadInput(id, "startOpen"));
			state.Initialized = true;
		}
		if (task.Pin == "open") state.Open = true;
		else if (task.Pin == "close") state.Open = false;
		else if (task.Pin == "toggle") state.Open = !state.Open;
		else if (task.Pin == "enter" && state.Open) PushOutput(id, "exit");
	}
	else if (type == "Flow.FlipFlop")
	{
		auto& state = m_NodeStates[id];
		if (task.Pin == "reset") { state.NextA = true; Store(id, "isA", false); }
		else
		{
			Store(id, "isA", state.NextA);
			PushOutput(id, state.NextA ? "a" : "b");
			state.NextA = !state.NextA;
		}
	}
	else if (type == "Logic.FloatResult" || type == "Logic.BoolResult" || type == "Logic.IntResult" ||
		type == "Logic.Float2Result" || type == "Logic.Float3Result" || type == "Logic.Float4Result")
		m_Result = Store(id, "value", ReadInput(id, "value"));
	else if (m_Functions.contains(type) && m_Registry.FindPin(type, "exec") &&
		m_Registry.FindPin(type, "exec")->Type == EGraphPinType::Execution)
	{
		auto value = Invoke(id);
		if (m_Registry.FindPin(type, "result")) Store(id, "result", std::move(value));
		PushOutput(id, "then");
	}
	else throw std::runtime_error("Execution is not implemented for node type: " + type);
}

}
