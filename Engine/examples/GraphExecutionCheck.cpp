#include "GraphExecutor.h"
#include "GraphReflection.h"
#include "GraphExecutionCheck.gen.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace
{
using namespace Engine;

void Expect(bool condition, const std::string& message)
{
	if (!condition) throw std::runtime_error(message);
}

void Accept(const GraphDiagnostic& result) { Expect(result.Succeeded(), result.Message); }

template<typename Function>
void RejectMath(Function function)
{
	bool rejected = false;
	try { function(); }
	catch (const std::exception&) { rejected = true; }
	Expect(rejected, "Invalid math input was accepted.");
}

GraphNodeId Add(Graph& graph, const GraphRegistry& registry, const std::string& type)
{
	const auto result = graph.AddNode(registry, type);
	Accept(result);
	return result.Node;
}

void Link(Graph& graph, const GraphRegistry& registry, GraphNodeId from, const std::string& output,
	GraphNodeId to, const std::string& input)
{
	Accept(graph.Connect(registry, { from, output }, { to, input }));
}

void Drain(GraphExecutor& executor)
{
	executor.Resume();
	for (int i = 0; i < 1000 && executor.GetState() == EGraphExecutionState::Running; ++i) executor.Tick(64);
	Expect(executor.GetState() != EGraphExecutionState::Running, "Interpreter did not yield or exhaust its budget.");
}

void Dispatch(GraphExecutor& executor, GraphNodeId entry)
{
	Accept(executor.Begin(entry));
	Drain(executor);
	Expect(executor.GetState() == EGraphExecutionState::Completed, executor.GetDiagnostic().Message);
}

void CheckMath()
{
	using namespace Engine::GraphMath;
	Expect(LerpFloat(2.0f, 6.0f, 0.25f) == 3.0f && LerpFloat(2.0f, 6.0f, 2.0f) == 10.0f,
		"Lerp or extrapolation failed.");
	Expect(ClampFloat(4.0f, 0.0f, 1.0f) == 1.0f && RemapFloat(5, 0, 10, -1, 1) == 0,
		"Clamp or Remap failed.");
	Expect(SmoothStepFloat(0, 1, 0.5f) == 0.5f && std::abs(SinFloat(RadiansFloat(90)) - 1.0f) < 0.00001f,
		"SmoothStep or angle conversion failed.");
	RejectMath([] { DivideFloat(1, 0); });
	RejectMath([] { SqrtFloat(-1); });
	RejectMath([] { LogFloat(0); });
	RejectMath([] { ClampFloat(0, 2, 1); });
	RejectMath([] { InverseLerpFloat(1, 1, 0); });
	RejectMath([] { PowFloat(-1, 0.5f); });
	RejectMath([] { AddInt(std::numeric_limits<std::int64_t>::max(), 1); });
	RejectMath([] { SubtractInt(std::numeric_limits<std::int64_t>::min(), 1); });
	RejectMath([] { MultiplyInt(std::numeric_limits<std::int64_t>::min(), -1); });
	RejectMath([] { DivideInt(std::numeric_limits<std::int64_t>::min(), -1); });
	RejectMath([] { FloatToInt(9223372036854775808.0f); });
	Expect(RemainderInt(std::numeric_limits<std::int64_t>::min(), -1) == 0 && MultiplyInt(-3, -4) == 12,
		"Defined Int boundary arithmetic failed.");
	Expect(FloatToInt(-1.75f) == -1 && FloatToInt(-9223372036854775808.0f) ==
		std::numeric_limits<std::int64_t>::min(), "Float-to-Int bounds or truncation failed.");
	using namespace Engine::GraphVector;
	Expect(CrossFloat3({ 1, 0, 0 }, { 0, 1, 0 }) == Float3{ 0, 0, 1 }, "Cross product failed.");
	Expect(DotFloat3({ 1, 2, 3 }, { 4, 5, 6 }) == 32, "Dot product failed.");
	Expect(NormalizeFloat3({ std::numeric_limits<float>::max(), 0, 0 }) == Float3{ 1, 0, 0 },
		"Normalization overflowed before division.");
	Expect(NormalizeFloat3({ std::numeric_limits<float>::denorm_min(), 0, 0 }) == Float3{ 1, 0, 0 },
		"Normalization underflowed before division.");
	RejectMath([] { NormalizeFloat3({ 0, 0, 0 }); });
}

void CheckNativeInvocation(const reflection::Registry& reflection)
{
	const reflection::FunctionInfo* divide = nullptr;
	const reflection::FunctionInfo* vector = nullptr;
	for (const auto& entry : reflection.GetFreeFunctions())
	{
		if (entry.Function->Name == "DivideFloat") divide = entry.Function;
		if (entry.Function->Name == "MakeFloat3") vector = entry.Function;
	}
	Expect(divide && vector && vector->CanInvokeDynamicWithoutObject(), "Array return was not bound by the generator.");
	std::vector<std::any> args{ 6.0f, 2.0f };
	const auto quotient = divide->InvokeDynamicWithoutObject(args);
	Expect(quotient.Succeeded() && std::any_cast<float>(quotient.Value) == 3.0f, "Dynamic invocation failed.");
	args[1] = 0.0f;
	Expect(divide->InvokeDynamicWithoutObject(args).Error == reflection::DynamicInvocationError::Exception,
		"Native math exception escaped the invocation result.");
	args[1] = true;
	Expect(divide->InvokeDynamicWithoutObject(args).Error == reflection::DynamicInvocationError::ArgumentType,
		"Dynamic invocation silently converted an argument.");
	args.pop_back();
	Expect(divide->InvokeDynamicWithoutObject(args).Error == reflection::DynamicInvocationError::ArgumentCount,
		"Dynamic invocation ignored argument count.");
	args = { 1.0f, 2.0f, 3.0f };
	const auto made = vector->InvokeDynamicWithoutObject(args);
	Expect(made.Succeeded() && std::any_cast<GraphVector::Float3>(made.Value) == GraphVector::Float3{ 1, 2, 3 },
		"Array result did not preserve ownership and type.");
}

void CheckMathGraph(const GraphRegistry& registry, const reflection::Registry& reflection)
{
	Graph graph;
	const auto start = Add(graph, registry, "Event.Start");
	const auto math = Add(graph, registry, "Math.LerpFloat");
	const auto branch = Add(graph, registry, "Flow.Branch");
	const auto compare = Add(graph, registry, "Math.GreaterFloat");
	const auto yes = Add(graph, registry, "Logic.FloatResult");
	const auto no = Add(graph, registry, "Logic.FloatResult");
	Accept(graph.SetInputValue(registry, { math, "a" }, 2.0f));
	Accept(graph.SetInputValue(registry, { math, "b" }, 6.0f));
	Accept(graph.SetInputValue(registry, { math, "t" }, 0.25f));
	Accept(graph.SetInputValue(registry, { no, "value" }, -1.0f));
	Link(graph, registry, start, "exec", branch, "exec");
	Link(graph, registry, math, "result", compare, "a");
	Link(graph, registry, compare, "result", branch, "condition");
	Link(graph, registry, branch, "true", yes, "exec");
	Link(graph, registry, branch, "false", no, "exec");
	Link(graph, registry, math, "result", yes, "value");
	GraphExecutor executor;
	Accept(executor.Load(graph, registry, &reflection));
	Accept(executor.Begin(start));
	executor.Step();
	Expect(executor.GetState() == EGraphExecutionState::Paused, "Step did not preserve the pause.");
	const auto traceSize = executor.GetTrace().size();
	executor.Tick(100);
	Expect(executor.GetTrace().size() == traceSize, "Tick advanced a paused interpreter.");
	Accept(graph.SetInputValue(registry, { math, "a" }, 100.0f));
	Drain(executor);
	Expect(executor.GetState() == EGraphExecutionState::Completed && std::get<float>(executor.GetResult()) == 3.0f,
		"Math graph, branch or snapshot isolation failed.");
	Expect(!executor.Begin(start).Succeeded(), "Start ran twice in one instance.");
	bool enteredYes = false, enteredNo = false, sawLink = false;
	std::uint64_t sequence = 0;
	for (const auto& event : executor.GetTrace())
	{
		Expect(event.Sequence > sequence, "Trace sequence was not monotonic.");
		sequence = event.Sequence;
		if (event.Kind == EGraphTraceKind::Enter && event.Node == yes) enteredYes = true;
		if (event.Kind == EGraphTraceKind::Enter && event.Node == no) enteredNo = true;
		if (event.Kind == EGraphTraceKind::Link && event.Link != 0) sawLink = true;
	}
	Expect(enteredYes && !enteredNo && sawLink, "Trace did not reflect the chosen execution path.");
}

void CheckLoops(const GraphRegistry& registry, const reflection::Registry& reflection)
{
	Graph graph;
	const auto start = Add(graph, registry, "Event.Start");
	const auto loop = Add(graph, registry, "Flow.ForLoop");
	const auto result = Add(graph, registry, "Logic.IntResult");
	Accept(graph.SetInputValue(registry, { loop, "last" }, std::int64_t{ 3 }));
	Link(graph, registry, start, "exec", loop, "exec");
	Link(graph, registry, loop, "body", result, "exec");
	Link(graph, registry, loop, "index", result, "value");
	GraphExecutor executor;
	Accept(executor.Load(graph, registry, &reflection));
	Dispatch(executor, start);
	std::vector<std::int64_t> indices;
	for (const auto& event : executor.GetTrace())
		if (event.Kind == EGraphTraceKind::Iteration && event.Node == loop)
			indices.push_back(std::get<std::int64_t>(event.Value));
	Expect(indices == std::vector<std::int64_t>{ 0, 1, 2 }, "ForLoop was not [first, last).");
	Expect(std::get<std::int64_t>(executor.GetResult()) == 2, "ForLoop body did not receive index.");
	const auto completed = Add(graph, registry, "Logic.IntResult");
	Link(graph, registry, loop, "completed", completed, "exec");
	Link(graph, registry, loop, "index", completed, "value");
	Accept(executor.Load(graph, registry, &reflection));
	Accept(executor.Begin(start));
	Drain(executor);
	Expect(executor.GetState() == EGraphExecutionState::Failed && executor.GetDiagnostic().Node == loop,
		"ForLoop index escaped its body context.");

	Graph whileGraph;
	const auto entry = Add(whileGraph, registry, "Event.Start");
	const auto repeat = Add(whileGraph, registry, "Flow.WhileLoop");
	const auto condition = Add(whileGraph, registry, "Check.HasWork");
	const auto pulse = Add(whileGraph, registry, "Check.Pulse");
	const auto count = Add(whileGraph, registry, "Check.ReadPulses");
	const auto output = Add(whileGraph, registry, "Logic.FloatResult");
	Link(whileGraph, registry, entry, "exec", repeat, "exec");
	Link(whileGraph, registry, condition, "result", repeat, "condition");
	Link(whileGraph, registry, repeat, "body", pulse, "exec");
	Link(whileGraph, registry, repeat, "completed", output, "exec");
	Link(whileGraph, registry, count, "result", output, "value");
	graph_checks::Pulses = 0;
	Accept(executor.Load(whileGraph, registry, &reflection));
	Dispatch(executor, entry);
	Expect(graph_checks::Pulses == 3 && std::get<float>(executor.GetResult()) == 3,
		"While condition was cached or callable body was not executed.");

	Graph infinite;
	const auto root = Add(infinite, registry, "Event.Start");
	const auto forever = Add(infinite, registry, "Flow.WhileLoop");
	Link(infinite, registry, root, "exec", forever, "exec");
	Accept(infinite.SetInputValue(registry, { forever, "condition" }, true));
	Accept(executor.Load(infinite, registry));
	Accept(executor.Begin(root, 0, 5000));
	Drain(executor);
	Expect(executor.GetDiagnostic().Code == EGraphError::ExecutionLimit && executor.GetTrace().size() == 2048 &&
		executor.GetTrace().front().Sequence > 1, "Budget or bounded trace retention failed.");
	const auto size = executor.GetTrace().size();
	executor.Tick();
	Expect(executor.GetTrace().size() == size, "Failed interpreter continued running.");
}

void CheckStateAndSequence(const GraphRegistry& registry)
{
	Graph graph;
	const auto entry = Add(graph, registry, "Event.Update");
	const auto reset = Add(graph, registry, "Event.Update");
	const auto once = Add(graph, registry, "Flow.DoOnce");
	const auto result = Add(graph, registry, "Logic.FloatResult");
	Link(graph, registry, entry, "exec", once, "exec");
	Link(graph, registry, reset, "exec", once, "reset");
	Link(graph, registry, once, "then", result, "exec");
	Accept(graph.SetInputValue(registry, { result, "value" }, 42.0f));
	GraphExecutor first, second;
	Accept(first.Load(graph, registry));
	Accept(second.Load(graph, registry));
	Dispatch(first, entry);
	Expect(std::get<float>(first.GetResult()) == 42, "DoOnce did not pass initially.");
	Dispatch(first, entry);
	Expect(std::holds_alternative<std::monostate>(first.GetResult()), "DoOnce state was lost between Update events.");
	Dispatch(second, entry);
	Expect(std::get<float>(second.GetResult()) == 42, "Instances shared DoOnce state.");
	Dispatch(first, reset);
	Dispatch(first, entry);
	Expect(std::get<float>(first.GetResult()) == 42, "DoOnce reset failed.");

	Graph sequence;
	const auto start = Add(sequence, registry, "Event.Start");
	const auto split = Add(sequence, registry, "Flow.Sequence");
	const auto a = Add(sequence, registry, "Logic.FloatResult");
	const auto b = Add(sequence, registry, "Logic.FloatResult");
	Link(sequence, registry, start, "exec", split, "exec");
	Link(sequence, registry, split, "then0", a, "exec");
	Link(sequence, registry, split, "then1", b, "exec");
	Accept(sequence.SetInputValue(registry, { a, "value" }, 1.0f));
	Accept(sequence.SetInputValue(registry, { b, "value" }, 2.0f));
	Accept(first.Load(sequence, registry));
	Dispatch(first, start);
	std::vector<GraphNodeId> order;
	for (const auto& event : first.GetTrace())
		if (event.Kind == EGraphTraceKind::Enter && (event.Node == a || event.Node == b)) order.push_back(event.Node);
	Expect(order == std::vector<GraphNodeId>{ a, b } && std::get<float>(first.GetResult()) == 2,
		"Sequence did not finish then0 before then1.");
}

void CheckGateFlipFlopAndStop(const GraphRegistry& registry)
{
	Graph gateGraph;
	const auto enter = Add(gateGraph, registry, "Event.Update");
	const auto close = Add(gateGraph, registry, "Event.Update");
	const auto toggle = Add(gateGraph, registry, "Event.Update");
	const auto gate = Add(gateGraph, registry, "Flow.Gate");
	const auto value = Add(gateGraph, registry, "Logic.FloatResult");
	Link(gateGraph, registry, enter, "exec", gate, "enter");
	Link(gateGraph, registry, close, "exec", gate, "close");
	Link(gateGraph, registry, toggle, "exec", gate, "toggle");
	Link(gateGraph, registry, gate, "exit", value, "exec");
	Accept(gateGraph.SetInputValue(registry, { value, "value" }, 1.0f));
	GraphExecutor executor;
	Accept(executor.Load(gateGraph, registry));
	Dispatch(executor, enter);
	Expect(std::get<float>(executor.GetResult()) == 1, "Gate was not initially open.");
	Dispatch(executor, close);
	Dispatch(executor, enter);
	Expect(std::holds_alternative<std::monostate>(executor.GetResult()), "Closed Gate passed execution.");
	Dispatch(executor, toggle);
	Dispatch(executor, enter);
	Expect(std::get<float>(executor.GetResult()) == 1, "Gate toggle did not reopen the gate.");

	Graph flipGraph;
	const auto entry = Add(flipGraph, registry, "Event.Update");
	const auto reset = Add(flipGraph, registry, "Event.Update");
	const auto flip = Add(flipGraph, registry, "Flow.FlipFlop");
	const auto a = Add(flipGraph, registry, "Logic.FloatResult");
	const auto b = Add(flipGraph, registry, "Logic.FloatResult");
	Link(flipGraph, registry, entry, "exec", flip, "exec");
	Link(flipGraph, registry, reset, "exec", flip, "reset");
	Link(flipGraph, registry, flip, "a", a, "exec");
	Link(flipGraph, registry, flip, "b", b, "exec");
	Accept(flipGraph.SetInputValue(registry, { a, "value" }, 1.0f));
	Accept(flipGraph.SetInputValue(registry, { b, "value" }, 2.0f));
	Accept(executor.Load(flipGraph, registry));
	Dispatch(executor, entry);
	Expect(std::get<float>(executor.GetResult()) == 1, "FlipFlop did not start at a.");
	Dispatch(executor, entry);
	Expect(std::get<float>(executor.GetResult()) == 2, "FlipFlop did not alternate to b.");
	Dispatch(executor, reset);
	Dispatch(executor, entry);
	Expect(std::get<float>(executor.GetResult()) == 1, "FlipFlop reset failed.");
	Accept(executor.Begin(entry));
	executor.Resume();
	executor.Stop();
	const auto size = executor.GetTrace().size();
	executor.Tick(100);
	Expect(executor.GetState() == EGraphExecutionState::Stopped && executor.GetTrace().size() == size,
		"Stop did not discard pending execution.");

	Graph recursive;
	const auto root = Add(recursive, registry, "Event.Start");
	const auto branch = Add(recursive, registry, "Flow.Branch");
	Accept(recursive.SetInputValue(registry, { branch, "condition" }, true));
	Link(recursive, registry, root, "exec", branch, "exec");
	Link(recursive, registry, branch, "true", branch, "exec");
	Accept(executor.Load(recursive, registry));
	Accept(executor.Begin(root));
	Drain(executor);
	Expect(executor.GetState() == EGraphExecutionState::Failed && executor.GetDiagnostic().Node == branch,
		"Re-entry into an active node was accepted.");
}

void CheckFailuresAndVectors(const GraphRegistry& registry, const reflection::Registry& reflection)
{
	Graph graph;
	const auto start = Add(graph, registry, "Event.Start");
	const auto math = Add(graph, registry, "Math.DivideFloat");
	const auto output = Add(graph, registry, "Logic.FloatResult");
	Link(graph, registry, start, "exec", output, "exec");
	Link(graph, registry, math, "result", output, "value");
	Accept(graph.SetInputValue(registry, { math, "b" }, 0.0f));
	GraphExecutor executor;
	Accept(executor.Load(graph, registry, &reflection));
	Accept(executor.Begin(start));
	Drain(executor);
	Expect(executor.GetState() == EGraphExecutionState::Failed && executor.GetDiagnostic().Node == math &&
		executor.GetDiagnostic().Pin == "result", "Native error lost the producing node and pin.");
	Expect(!executor.GetTrace().empty() && executor.GetTrace().back().Kind == EGraphTraceKind::Error,
		"Native error was absent from trace.");

	Graph vector;
	const auto root = Add(vector, registry, "Event.Start");
	const auto make = Add(vector, registry, "Math.MakeFloat3");
	const auto normalize = Add(vector, registry, "Math.NormalizeFloat3");
	const auto result = Add(vector, registry, "Logic.Float3Result");
	Accept(vector.SetInputValue(registry, { make, "x" }, 3.0f));
	Accept(vector.SetInputValue(registry, { make, "y" }, 4.0f));
	Link(vector, registry, root, "exec", result, "exec");
	Link(vector, registry, make, "result", normalize, "value");
	Link(vector, registry, normalize, "result", result, "value");
	Accept(executor.Load(vector, registry, &reflection));
	Dispatch(executor, root);
	const auto value = std::get<GraphVector::Float3>(executor.GetResult());
	Expect(std::abs(value[0] - 0.6f) < 0.00001f && std::abs(value[1] - 0.8f) < 0.00001f,
		"Reflected vector graph did not evaluate.");

	Graph nativeRange;
	const auto event = Add(nativeRange, registry, "Event.Start");
	const auto narrow = Add(nativeRange, registry, "Check.Int32");
	const auto sink = Add(nativeRange, registry, "Logic.IntResult");
	Accept(nativeRange.SetInputValue(registry, { narrow, "value" }, std::int64_t{ 1 } << 40));
	Link(nativeRange, registry, event, "exec", sink, "exec");
	Link(nativeRange, registry, narrow, "result", sink, "value");
	Accept(executor.Load(nativeRange, registry, &reflection));
	Accept(executor.Begin(event));
	Drain(executor);
	Expect(executor.GetState() == EGraphExecutionState::Failed && executor.GetDiagnostic().Node == narrow &&
		executor.GetDiagnostic().Pin == "value", "Native int32 range was not checked at the input pin.");
}
}

int main()
{
	try
	{
		reflection::Registry reflection;
		Expect(reflection_generated::Register_GraphExecutionCheck(reflection), "Generated reflection failed.");
		GraphRegistry registry;
		Accept(registry.RegisterBuiltins());
		Accept(RegisterReflectedGraphNodes(registry, reflection));
		CheckMath();
		CheckNativeInvocation(reflection);
		CheckMathGraph(registry, reflection);
		CheckLoops(registry, reflection);
		CheckStateAndSequence(registry);
		CheckGateFlipFlopAndStop(registry);
		CheckFailuresAndVectors(registry, reflection);
		std::cout << "Graph math, execution and trace checks passed.\n";
		return 0;
	}
	catch (const std::exception& error)
	{
		std::cerr << error.what() << '\n';
		return 1;
	}
}
