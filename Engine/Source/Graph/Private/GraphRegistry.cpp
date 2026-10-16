#include "GraphRegistry.h"
#include "GraphValue.h"
#include <unordered_set>
#include <utility>

namespace Engine
{

GraphDiagnostic GraphRegistry::Register(GraphNodeDefinition definition)
{
	auto invalid = [](std::string pin, std::string message)
	{
		return GraphDiagnostic{ EGraphError::InvalidDefinition, 0, std::move(pin), 0, std::move(message) };
	};
	if (definition.Type.empty() || definition.Domains.empty())
		return invalid({}, "Node type and at least one domain are required.");
	if (Find(definition.Type))
		return { EGraphError::DuplicateType, 0, {}, 0, "Node type is already registered: " + definition.Type };

	std::unordered_set<int> domains;
	for (auto domain : definition.Domains)
	{
		if ((domain != EGraphDomain::Logic && domain != EGraphDomain::Shader) ||
			!domains.insert(static_cast<int>(domain)).second)
			return invalid({}, "Invalid or duplicate graph domain.");
	}
	const bool shader = domains.contains(static_cast<int>(EGraphDomain::Shader));
	std::unordered_set<std::string> keys;
	for (const auto& pin : definition.Pins)
	{
		if (pin.Key.empty() || !keys.insert(pin.Key).second)
			return invalid(pin.Key, "Pin keys must be non-empty and unique within a node.");
		if (pin.Direction != EGraphPinDirection::Input && pin.Direction != EGraphPinDirection::Output)
			return invalid(pin.Key, "Invalid pin direction.");
		switch (pin.Type)
		{
		case EGraphPinType::Execution:
		case EGraphPinType::Bool:
		case EGraphPinType::Int:
		case EGraphPinType::Float:
		case EGraphPinType::Float2:
		case EGraphPinType::Float3:
		case EGraphPinType::Float4: break;
		case EGraphPinType::Object:
			if (pin.ObjectType.empty()) return invalid(pin.Key, "Object pins require a reflected type name.");
			break;
		default: return invalid(pin.Key, "Invalid pin type.");
		}
		if (pin.Type != EGraphPinType::Object && !pin.ObjectType.empty())
			return invalid(pin.Key, "Only object pins can specify an object type.");
		if (shader && (pin.Type == EGraphPinType::Execution || pin.Type == EGraphPinType::Object))
			return invalid(pin.Key, "Shader nodes cannot declare execution or object pins.");
		if (!std::holds_alternative<std::monostate>(pin.DefaultValue))
		{
			if (pin.Direction != EGraphPinDirection::Input ||
				!GraphValueMatchesType(pin.DefaultValue, pin.Type) || !IsFiniteGraphValue(pin.DefaultValue))
				return invalid(pin.Key, "Only data inputs can have a finite, exactly typed default value.");
		}
	}

	const auto type = definition.Type;
	m_Definitions.emplace(type, std::move(definition));
	return {};
}

const GraphNodeDefinition* GraphRegistry::Find(const std::string& type) const
{
	const auto found = m_Definitions.find(type);
	return found == m_Definitions.end() ? nullptr : &found->second;
}

const GraphPin* GraphRegistry::FindPin(const std::string& type, const std::string& key) const
{
	const auto* definition = Find(type);
	if (!definition) return nullptr;
	const auto found = std::find_if(definition->Pins.begin(), definition->Pins.end(),
		[&](const GraphPin& pin) { return pin.Key == key; });
	return found == definition->Pins.end() ? nullptr : &*found;
}

std::vector<std::string> GraphRegistry::GetTypes(EGraphDomain domain) const
{
	std::vector<std::string> types;
	for (const auto& [type, definition] : m_Definitions)
	{
		if (std::find(definition.Domains.begin(), definition.Domains.end(), domain) != definition.Domains.end())
			types.push_back(type);
	}
	std::sort(types.begin(), types.end());
	return types;
}

GraphDiagnostic GraphRegistry::RegisterBuiltins()
{
	using Direction = EGraphPinDirection;
	using Type = EGraphPinType;
	const std::vector<EGraphDomain> common{ EGraphDomain::Logic, EGraphDomain::Shader };
	const std::vector<EGraphDomain> logic{ EGraphDomain::Logic };
	std::vector<GraphNodeDefinition> definitions{
		{ "Core.Bool", common, {
			{ "value", Direction::Input, Type::Bool, false }, { "result", Direction::Output, Type::Bool, {} } } },
		{ "Core.Int", common, {
			{ "value", Direction::Input, Type::Int, std::int64_t{ 0 } },
			{ "result", Direction::Output, Type::Int, {} } } },
		{ "Core.Float", common, {
			{ "value", Direction::Input, Type::Float, 0.0f }, { "result", Direction::Output, Type::Float, {} } } },
		{ "Core.Float2", common, {
			{ "value", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "result", Direction::Output, Type::Float2, {} } } },
		{ "Core.Float3", common, {
			{ "value", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "result", Direction::Output, Type::Float3, {} } } },
		{ "Core.Float4", common, {
			{ "value", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "result", Direction::Output, Type::Float4, {} } } },
		{ "Math.AddFloat", common, {
			{ "a", Direction::Input, Type::Float, 0.0f }, { "b", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.MultiplyFloat", common, {
			{ "a", Direction::Input, Type::Float, 1.0f }, { "b", Direction::Input, Type::Float, 1.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.GreaterFloat", common, {
			{ "a", Direction::Input, Type::Float, 0.0f }, { "b", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Bool, {} } } },
		{ "Math.LessFloat", common, {
			{ "a", Direction::Input, Type::Float, 0.0f }, { "b", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Bool, {} } } },
		{ "Logic.And", common, {
			{ "a", Direction::Input, Type::Bool, false }, { "b", Direction::Input, Type::Bool, false },
			{ "result", Direction::Output, Type::Bool, {} } } },
		{ "Logic.Or", common, {
			{ "a", Direction::Input, Type::Bool, false }, { "b", Direction::Input, Type::Bool, false },
			{ "result", Direction::Output, Type::Bool, {} } } },
		{ "Logic.Not", common, {
			{ "value", Direction::Input, Type::Bool, false }, { "result", Direction::Output, Type::Bool, {} } } },
		{ "Logic.SelectFloat", common, {
			{ "condition", Direction::Input, Type::Bool, false }, { "true", Direction::Input, Type::Float, 0.0f },
			{ "false", Direction::Input, Type::Float, 0.0f }, { "result", Direction::Output, Type::Float, {} } } },
		{ "Event.Start", logic, { { "exec", Direction::Output, Type::Execution, {} } } },
		{ "Event.Update", logic, {
			{ "exec", Direction::Output, Type::Execution, {} }, { "delta", Direction::Output, Type::Float, {} } } },
		{ "Flow.Branch", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "condition", Direction::Input, Type::Bool, false },
			{ "true", Direction::Output, Type::Execution, {} }, { "false", Direction::Output, Type::Execution, {} } } },
		{ "Flow.Sequence", logic, {
			{ "exec", Direction::Input, Type::Execution, {} },
			{ "then0", Direction::Output, Type::Execution, {} }, { "then1", Direction::Output, Type::Execution, {} } } },
		{ "Flow.ForLoop", logic, {
			{ "exec", Direction::Input, Type::Execution, {} },
			{ "first", Direction::Input, Type::Int, std::int64_t{ 0 } },
			{ "last", Direction::Input, Type::Int, std::int64_t{ 1 } },
			{ "body", Direction::Output, Type::Execution, {} }, { "index", Direction::Output, Type::Int, {} },
			{ "completed", Direction::Output, Type::Execution, {} } } },
		{ "Flow.WhileLoop", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "condition", Direction::Input, Type::Bool, false },
			{ "body", Direction::Output, Type::Execution, {} }, { "completed", Direction::Output, Type::Execution, {} } } },
		{ "Flow.DoOnce", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "reset", Direction::Input, Type::Execution, {} },
			{ "startClosed", Direction::Input, Type::Bool, false }, { "then", Direction::Output, Type::Execution, {} } } },
		{ "Flow.Gate", logic, {
			{ "enter", Direction::Input, Type::Execution, {} }, { "open", Direction::Input, Type::Execution, {} },
			{ "close", Direction::Input, Type::Execution, {} }, { "toggle", Direction::Input, Type::Execution, {} },
			{ "startOpen", Direction::Input, Type::Bool, true }, { "exit", Direction::Output, Type::Execution, {} } } },
		{ "Flow.FlipFlop", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "reset", Direction::Input, Type::Execution, {} },
			{ "a", Direction::Output, Type::Execution, {} }, { "b", Direction::Output, Type::Execution, {} },
			{ "isA", Direction::Output, Type::Bool, {} } } },
		{ "Logic.FloatResult", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "value", Direction::Input, Type::Float, {} } } },
		{ "Logic.BoolResult", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "value", Direction::Input, Type::Bool, {} } } },
		{ "Logic.IntResult", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "value", Direction::Input, Type::Int, {} } } },
		{ "Logic.Float2Result", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "value", Direction::Input, Type::Float2, {} } } },
		{ "Logic.Float3Result", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "value", Direction::Input, Type::Float3, {} } } },
		{ "Logic.Float4Result", logic, {
			{ "exec", Direction::Input, Type::Execution, {} }, { "value", Direction::Input, Type::Float4, {} } } },
		{ "Shader.ColorOutput", { EGraphDomain::Shader }, {
			{ "color", Direction::Input, Type::Float4, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f } } } }
	};
	for (auto& definition : definitions)
	{
		const auto separator = definition.Type.find('.');
		definition.Category = definition.Type.substr(0, separator);
		definition.DisplayName = definition.Type.substr(separator + 1);
	}
	for (auto& definition : definitions)
	{
		if (definition.Type == "Flow.Branch") definition.Tooltip = "Run the true or false execution path.";
		if (definition.Type == "Flow.Sequence") definition.Tooltip = "Finish then0 before running then1.";
		if (definition.Type == "Flow.ForLoop") definition.Tooltip = "Run body for each index in [first, last).";
		if (definition.Type == "Flow.WhileLoop") definition.Tooltip = "Re-evaluate condition before each body iteration.";
		if (definition.Type == "Flow.DoOnce")
			definition.Tooltip = "Pass once until reset; state belongs to the graph instance.";
		if (definition.Type == "Flow.Gate") definition.Tooltip = "Pass enter to exit only while the gate is open.";
		if (definition.Type == "Flow.FlipFlop") definition.Tooltip = "Alternate a and b; reset makes a the next output.";
	}

	// A collision leaves all existing registrations intact, without a partial builtin set.
	GraphRegistry candidate = *this;
	for (const auto& definition : definitions)
	{
		auto result = candidate.Register(definition);
		if (!result.Succeeded()) return result;
	}
	m_Definitions.swap(candidate.m_Definitions);
	return {};
}

}
