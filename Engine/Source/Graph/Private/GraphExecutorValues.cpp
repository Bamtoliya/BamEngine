#include "GraphExecutor.h"
#include "GraphMathLibrary.h"
#include "GraphValue.h"
#include <limits>
#include <stdexcept>
#include <type_traits>

namespace
{
using namespace Engine;

std::any NativeValue(const GraphValue& value, std::type_index type)
{
	if (type == typeid(std::int32_t))
	{
		const auto* integer = std::get_if<std::int64_t>(&value);
		if (!integer || *integer < std::numeric_limits<std::int32_t>::min() ||
			*integer > std::numeric_limits<std::int32_t>::max())
			throw std::runtime_error("Value is outside the native int32 range.");
		return static_cast<std::int32_t>(*integer);
	}
	return std::visit([&](const auto& item) -> std::any
	{
		using T = std::decay_t<decltype(item)>;
		if constexpr (!std::is_same_v<T, std::monostate>)
			if (type == typeid(T)) return item;
		throw std::runtime_error("Native argument type mismatch.");
	}, value);
}

GraphValue ReturnedValue(const std::any& value)
{
	if (!value.has_value()) return {};
	if (const auto* item = std::any_cast<bool>(&value)) return *item;
	if (const auto* item = std::any_cast<float>(&value)) return *item;
	if (const auto* item = std::any_cast<std::int64_t>(&value)) return *item;
	if (const auto* item = std::any_cast<std::int32_t>(&value)) return static_cast<std::int64_t>(*item);
	if (const auto* item = std::any_cast<std::array<float, 2>>(&value)) return *item;
	if (const auto* item = std::any_cast<std::array<float, 3>>(&value)) return *item;
	if (const auto* item = std::any_cast<std::array<float, 4>>(&value)) return *item;
	throw std::runtime_error("Unsupported native return value.");
}
}

namespace Engine
{

GraphValue GraphExecutor::Store(GraphNodeId node, const std::string& pin, GraphValue value)
{
	const auto* instance = m_Graph.FindNode(node);
	const auto* schema = instance ? m_Registry.FindPin(instance->Type, pin) : nullptr;
	if (!schema || !GraphValueMatchesType(value, schema->Type) || !IsFiniteGraphValue(value))
		throw std::runtime_error("Node produced a non-finite or incorrectly typed value.");
	m_Outputs[node].insert_or_assign(pin, value);
	Trace(EGraphTraceKind::Value, node, pin, 0, value);
	return value;
}

GraphValue GraphExecutor::ReadInput(GraphNodeId node, const std::string& pin)
{
	try
	{
		for (const auto& link : m_Graph.GetLinks())
			if (link.To == GraphPinReference{ node, pin })
			{
				Trace(EGraphTraceKind::Link, node, pin, link.Id);
				auto value = Evaluate(link.From.Node, link.From.Pin);
				Trace(EGraphTraceKind::Value, node, pin, 0, value);
				return value;
			}
		const auto* instance = m_Graph.FindNode(node);
		const auto* schema = instance ? m_Registry.FindPin(instance->Type, pin) : nullptr;
		if (!schema) throw std::runtime_error("Input pin is missing.");
		const auto found = instance->InputValues.find(pin);
		auto value = found != instance->InputValues.end() ? found->second : schema->DefaultValue;
		if (!GraphValueMatchesType(value, schema->Type) || !IsFiniteGraphValue(value))
			throw std::runtime_error("Input requires a finite value or connection.");
		Trace(EGraphTraceKind::Value, node, pin, 0, value);
		return value;
	}
	catch (const std::exception& error)
	{
		if (m_State != EGraphExecutionState::Failed) Fail(node, pin, error.what());
		throw;
	}
}

GraphValue GraphExecutor::Invoke(GraphNodeId node)
{
	const auto& instance = *m_Graph.FindNode(node);
	const auto& definition = *m_Registry.Find(instance.Type);
	const auto& function = m_Functions.at(instance.Type);
	const auto& types = function.GetParameterCppTypes();
	std::vector<std::any> args;
	for (const auto& pin : definition.Pins)
	{
		if (pin.Direction != EGraphPinDirection::Input || pin.Type == EGraphPinType::Execution) continue;
		if (args.size() >= types.size()) throw std::runtime_error("Function schema has too many argument pins.");
		try { args.push_back(NativeValue(ReadInput(node, pin.Key), types[args.size()])); }
		catch (const std::exception& error)
		{
			if (m_State != EGraphExecutionState::Failed) Fail(node, pin.Key, error.what());
			throw;
		}
	}
	const auto result = function.InvokeDynamicWithoutObject(args);
	if (!result.Succeeded()) throw std::runtime_error(result.Message);
	return ReturnedValue(result.Value);
}

GraphValue GraphExecutor::Evaluate(GraphNodeId node, const std::string& pin)
{
	if (!Consume(node)) throw std::runtime_error("Execution budget exhausted.");
	if (m_Evaluating.size() >= 128 || !m_Evaluating.insert(node).second)
	{
		Fail(node, pin, "Data recursion depth exceeded or a cycle was encountered.", EGraphError::ExecutionLimit);
		throw std::runtime_error("Data recursion limit.");
	}
	try
	{
		const auto& instance = *m_Graph.FindNode(node);
		const auto& type = instance.Type;
		GraphValue value;
		if (type == "Core.Bool" || type == "Core.Int" || type == "Core.Float" || type == "Core.Float2" ||
			type == "Core.Float3" || type == "Core.Float4") value = ReadInput(node, "value");
		else if (type == "Math.AddFloat" || type == "Math.MultiplyFloat" ||
			type == "Math.GreaterFloat" || type == "Math.LessFloat")
		{
			const float a = std::get<float>(ReadInput(node, "a"));
			const float b = std::get<float>(ReadInput(node, "b"));
			if (type == "Math.AddFloat") value = GraphMath::AddFloat(a, b);
			else if (type == "Math.MultiplyFloat") value = GraphMath::MultiplyFloat(a, b);
			else if (type == "Math.GreaterFloat") value = a > b;
			else value = a < b;
		}
		else if (type == "Logic.And" || type == "Logic.Or")
		{
			const bool a = std::get<bool>(ReadInput(node, "a"));
			const bool b = std::get<bool>(ReadInput(node, "b"));
			value = type == "Logic.And" ? a && b : a || b;
		}
		else if (type == "Logic.Not") value = !std::get<bool>(ReadInput(node, "value"));
		else if (type == "Logic.SelectFloat")
			value = ReadInput(node, std::get<bool>(ReadInput(node, "condition")) ? "true" : "false");
		else if (m_Functions.contains(type) && (!m_Registry.FindPin(type, "exec") ||
			m_Registry.FindPin(type, "exec")->Type != EGraphPinType::Execution)) value = Invoke(node);
		else if (type == "Flow.FlipFlop" && pin == "isA")
			value = !m_NodeStates[node].NextA;
		else
		{
			const auto outputs = m_Outputs.find(node);
			if (outputs == m_Outputs.end() || !outputs->second.contains(pin))
				throw std::runtime_error("Output is not available in this execution context: " + type + "." + pin);
			value = outputs->second.at(pin);
		}
		m_Evaluating.erase(node);
		return Store(node, pin, std::move(value));
	}
	catch (const std::exception& error)
	{
		m_Evaluating.erase(node);
		if (m_State != EGraphExecutionState::Failed) Fail(node, pin, error.what());
		throw;
	}
}

}
