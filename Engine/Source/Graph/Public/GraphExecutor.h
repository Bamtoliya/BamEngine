#pragma once

#include "Graph.h"
#include <reflection/Registry.h>
#include <deque>
#include <unordered_set>

namespace Engine
{
enum class EGraphExecutionState { Idle, Paused, Running, Completed, Stopped, Failed };
enum class EGraphTraceKind { Enter, Leave, Link, Value, Iteration, Error };

struct GraphTraceEvent
{
	std::uint64_t Sequence = 0;
	EGraphTraceKind Kind = EGraphTraceKind::Enter;
	GraphNodeId Node = 0;
	std::string Pin;
	GraphLinkId Link = 0;
	GraphValue Value;
	std::string Message;
};

// Owns an execution snapshot and per-instance state. No Editor or scene pointers.
class ENGINE_API GraphExecutor
{
public:
	[[nodiscard]] GraphDiagnostic Load(const Graph& graph, const GraphRegistry& registry,
		const reflection::Registry* sourceRegistry = nullptr);
	[[nodiscard]] GraphDiagnostic Begin(GraphNodeId entry, float delta = 0.0f, std::size_t budget = 10000);
	void Resume();
	void Pause();
	void Stop();
	void Clear();
	void Step();
	void Tick(std::size_t maxTasks = 64);
	EGraphExecutionState GetState() const { return m_State; }
	const GraphDiagnostic& GetDiagnostic() const { return m_Diagnostic; }
	const std::deque<GraphTraceEvent>& GetTrace() const { return m_Trace; }
	const GraphValue& GetResult() const { return m_Result; }
	std::size_t GetRemainingBudget() const { return m_Budget; }

private:
	enum class TaskKind { Enter, Leave, ForNext, WhileNext };
	struct Task
	{
		TaskKind Kind = TaskKind::Enter;
		GraphNodeId Node = 0;
		std::string Pin;
		GraphLinkId Link = 0;
		std::int64_t Index = 0;
		std::int64_t Last = 0;
	};
	struct NodeState
	{
		bool Initialized = false;
		bool Open = false;
		bool NextA = true;
	};

	bool Consume(GraphNodeId node);
	GraphDiagnostic Fail(GraphNodeId node, std::string pin, std::string message,
		EGraphError code = EGraphError::ExecutionFailed);
	void Trace(EGraphTraceKind kind, GraphNodeId node, std::string pin = {}, GraphLinkId link = 0,
		GraphValue value = {}, std::string message = {});
	void PushOutput(GraphNodeId node, const std::string& pin);
	void Execute(const Task& task);
	GraphValue ReadInput(GraphNodeId node, const std::string& pin);
	GraphValue Evaluate(GraphNodeId node, const std::string& pin);
	GraphValue Invoke(GraphNodeId node);
	GraphValue Store(GraphNodeId node, const std::string& pin, GraphValue value);

	Graph m_Graph;
	GraphRegistry m_Registry;
	std::unordered_map<std::string, reflection::FunctionInfo> m_Functions;
	std::unordered_map<GraphNodeId, NodeState> m_NodeStates;
	std::unordered_map<GraphNodeId, std::unordered_map<std::string, GraphValue>> m_Outputs;
	std::unordered_set<GraphNodeId> m_Active;
	std::unordered_set<GraphNodeId> m_Evaluating;
	std::vector<Task> m_Pending;
	std::deque<GraphTraceEvent> m_Trace;
	GraphValue m_Result;
	GraphDiagnostic m_Diagnostic;
	EGraphExecutionState m_State = EGraphExecutionState::Idle;
	std::size_t m_Budget = 0;
	std::uint64_t m_Sequence = 0;
	bool m_Loaded = false;
	bool m_Started = false;
};
}
