#include "GraphReflection.h"
#include "GraphValue.h"
#include <reflection/Registry.h>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <typeindex>
#include <unordered_set>
#include <utility>

namespace
{
using namespace Engine;
using Direction = EGraphPinDirection;
using Type = EGraphPinType;
using Metadata = reflection::Metadata;

GraphDiagnostic Invalid(const std::string& source, const std::string& message)
{
	return { EGraphError::InvalidDefinition, 0, {}, 0, source + ": " + message };
}

bool HasGraphMetadata(const Metadata& metadata)
{
	for (const auto& entry : metadata.GetEntries())
		if (entry.Key.starts_with("Graph.")) return true;
	return false;
}

bool IsTrue(const Metadata& metadata, const std::string& key)
{
	const auto* value = metadata.Find(key);
	const auto* enabled = value ? std::get_if<bool>(value) : nullptr;
	return enabled && *enabled;
}

bool ReadText(const Metadata& metadata, const std::string& key, std::string& text, bool allowEmpty = false)
{
	const auto* value = metadata.Find(key);
	if (!value) return true;
	const auto* string = std::get_if<std::string>(value);
	if (!string || (!allowEmpty && string->empty())) return false;
	text = *string;
	return true;
}

std::optional<Type> ReflectedPinType(std::string name)
{
	std::erase_if(name, [](unsigned char ch) { return std::isspace(ch) != 0; });
	if (name == "bool") return Type::Bool;
	if (name == "float") return Type::Float;
	if (name == "int" || name == "signedint" || name == "int32_t" || name == "std::int32_t" ||
		name == "longlong" || name == "longlongint" || name == "signedlonglong" ||
		name == "signedlonglongint" || name == "int64_t" || name == "std::int64_t") return Type::Int;
	if constexpr (sizeof(long) == sizeof(std::int64_t))
		if (name == "long" || name == "longint" || name == "signedlong" || name == "signedlongint") return Type::Int;
	return std::nullopt;
}

std::optional<Type> ReflectedPinType(std::type_index type)
{
	if (type == typeid(bool)) return Type::Bool;
	if (type == typeid(float)) return Type::Float;
	if (type == typeid(std::int32_t) || type == typeid(std::int64_t)) return Type::Int;
	if (type == typeid(std::array<float, 2>)) return Type::Float2;
	if (type == typeid(std::array<float, 3>)) return Type::Float3;
	if (type == typeid(std::array<float, 4>)) return Type::Float4;
	return std::nullopt;
}

bool ReadDefault(const reflection::MetadataValue& value, Type type, GraphValue& result)
{
	if (type == Type::Bool)
	{
		if (const auto* item = std::get_if<bool>(&value)) { result = *item; return true; }
	}
	if (type == Type::Int)
	{
		if (const auto* item = std::get_if<std::int64_t>(&value)) { result = *item; return true; }
		if (const auto* item = std::get_if<std::uint64_t>(&value))
		{
			if (*item <= static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()))
			{
				result = static_cast<std::int64_t>(*item);
				return true;
			}
		}
	}
	if (type == Type::Float)
	{
		if (const auto* item = std::get_if<double>(&value))
		{
			if (std::isfinite(*item) && std::abs(*item) <= std::numeric_limits<float>::max())
			{
				result = static_cast<float>(*item);
				return true;
			}
		}
	}
	return false;
}

bool ReadPresentation(const Metadata& metadata, GraphNodeDefinition& node)
{
	return ReadText(metadata, "DisplayName", node.DisplayName, true) &&
		ReadText(metadata, "Category", node.Category, true) && ReadText(metadata, "Tooltip", node.Tooltip, true);
}

GraphPin TargetPin(const std::string& owner)
{
	return { "target", Direction::Input, Type::Object, {}, owner, "Target" };
}

GraphDiagnostic RegisterFunction(GraphRegistry& graph, const reflection::FunctionInfo& function,
	const std::string& owner, const std::string& member)
{
	const auto& metadata = function.Metadata;
	if (!HasGraphMetadata(metadata)) return {};
	const auto source = owner.empty() ? member : owner + "::" + member;
	const auto location = source + " " + function.Signature;
	const bool pure = IsTrue(metadata, "Graph.Pure");
	const bool callable = IsTrue(metadata, "Graph.Callable");
	if (pure == callable || (metadata.Find("Graph.Pure") && !pure) ||
		(metadata.Find("Graph.Callable") && !callable))
		return Invalid(location, "Specify exactly one of GRAPH_PURE(id) and GRAPH_CALLABLE(id).");
	if (!function.CanInvoke())
		return Invalid(location, "Reflection function binding is unavailable: " +
			std::string(reflection::ToString(function.GetBindingStatus())));

	std::unordered_set<std::string> parameters;
	for (const auto& parameter : function.Parameters)
		if (parameter.Name.empty() || !parameters.insert(parameter.Name).second)
			return Invalid(location, "Graph parameters require unique, non-empty names.");
	for (const auto& entry : metadata.GetEntries())
	{
		if (!entry.Key.starts_with("Graph.")) continue;
		if (entry.Key == "Graph.Pure" || entry.Key == "Graph.Callable" || entry.Key == "Graph.Id") continue;
		bool recognized = false;
		for (const std::string prefix : { "Graph.PinKey.", "Graph.PinName.", "Graph.PinDefault." })
		{
			if (!entry.Key.starts_with(prefix)) continue;
			recognized = parameters.contains(entry.Key.substr(prefix.size()));
			break;
		}
		if (!recognized) return Invalid(location, "Unknown graph metadata or parameter: " + entry.Key);
	}

	GraphNodeDefinition node;
	if (!ReadText(metadata, "Graph.Id", node.Type) || node.Type.empty())
		return Invalid(location, "A non-empty stable GRAPH_ID is required.");
	node.Domains = { EGraphDomain::Logic };
	node.DisplayName = function.Name;
	node.Category = "Reflection/Functions";
	if (!ReadPresentation(metadata, node)) return Invalid(location, "Invalid display metadata.");
	node.Reflection = { EGraphReflectionKind::Function, owner, member, function.Signature };
	if (callable) node.Pins.push_back({ "exec", Direction::Input, Type::Execution, {} });
	if (!owner.empty() && !function.IsStaticMember) node.Pins.push_back(TargetPin(owner));
	std::size_t parameterIndex = 0;
	for (const auto& parameter : function.Parameters)
	{
		if (parameter.TypeName.find_first_of("*&") != std::string::npos)
			return Invalid(location, "Pointer/reference parameters are not supported by graphs.");
		const auto& nativeTypes = function.GetParameterCppTypes();
		const auto type = nativeTypes.size() == function.Parameters.size() ?
			ReflectedPinType(nativeTypes[parameterIndex]) : ReflectedPinType(parameter.TypeName);
		++parameterIndex;
		if (!type) return Invalid(location, "Unsupported by-value parameter: " + parameter.TypeName);
		GraphPin pin{ parameter.Name, Direction::Input, *type, {} };
		pin.DisplayName = parameter.Name;
		if (!ReadText(metadata, "Graph.PinKey." + parameter.Name, pin.Key) ||
			!ReadText(metadata, "Graph.PinName." + parameter.Name, pin.DisplayName))
			return Invalid(location, "Invalid pin key or display name: " + parameter.Name);
		if (const auto* value = metadata.Find("Graph.PinDefault." + parameter.Name))
		{
			if (!ReadDefault(*value, *type, pin.DefaultValue))
				return Invalid(location, "Pin default must be finite and match the parameter type: " + parameter.Name);
			if (*type == Type::Int)
			{
				// Graph Int is int64; a reflected int32 input still has its native range.
				std::string name = parameter.TypeName;
				std::erase_if(name, [](unsigned char ch) { return std::isspace(ch) != 0; });
				if (name == "int" || name == "signedint" || name == "int32_t" || name == "std::int32_t")
				{
					const auto integer = std::get<std::int64_t>(pin.DefaultValue);
					if (integer < std::numeric_limits<std::int32_t>::min() ||
						integer > std::numeric_limits<std::int32_t>::max())
						return Invalid(location, "Pin default exceeds int32 range: " + parameter.Name);
				}
			}
		}
		node.Pins.push_back(std::move(pin));
	}
	const auto& resultName = function.CanonicalReturnType.empty() ? function.ReturnType : function.CanonicalReturnType;
	if (resultName != "void")
	{
		if (resultName.find_first_of("*&") != std::string::npos)
			return Invalid(location, "Pointer/reference results are not supported by graphs.");
		const auto type = function.CanInvokeDynamicWithoutObject() ?
			ReflectedPinType(function.GetReturnCppType()) : ReflectedPinType(resultName);
		if (!type) return Invalid(location, "Unsupported return type: " + resultName);
		node.Pins.push_back({ "result", Direction::Output, *type, {} });
	}
	else if (pure)
		return Invalid(location, "A pure node must return a value.");
	if (callable) node.Pins.push_back({ "then", Direction::Output, Type::Execution, {} });
	auto result = graph.Register(std::move(node));
	if (!result.Succeeded()) result.Message = location + ": " + result.Message;
	return result;
}

GraphDiagnostic RegisterProperty(GraphRegistry& graph, const reflection::TypeInfo& owner,
	const reflection::PropertyInfo& property)
{
	const auto& metadata = property.Metadata;
	if (!HasGraphMetadata(metadata)) return {};
	const auto location = owner.QualifiedName + "::" + property.Name;
	const bool readOnly = IsTrue(metadata, "Graph.ReadOnly");
	const bool readWrite = IsTrue(metadata, "Graph.ReadWrite");
	if (readOnly == readWrite || (metadata.Find("Graph.ReadOnly") && !readOnly) ||
		(metadata.Find("Graph.ReadWrite") && !readWrite))
		return Invalid(location, "Specify exactly one of GRAPH_READONLY and GRAPH_READWRITE.");
	for (const auto& entry : metadata.GetEntries())
		if (entry.Key.starts_with("Graph.") && entry.Key != "Graph.Id" &&
			entry.Key != "Graph.ReadOnly" && entry.Key != "Graph.ReadWrite")
			return Invalid(location, "Unknown property graph metadata: " + entry.Key);
	if (!property.CanRead() || (readWrite && !property.CanWrite()))
		return Invalid(location, "Property access binding does not support the requested graph policy.");
	const auto type = ReflectedPinType(property.GetValueCppType());
	if (!type) return Invalid(location, "Unsupported property type: " + property.TypeName);

	GraphNodeDefinition node;
	std::string id;
	if (!ReadText(metadata, "Graph.Id", id) || id.empty())
		return Invalid(location, "Property exposure requires a stable GRAPH_ID.");
	node.Domains = { EGraphDomain::Logic };
	node.DisplayName = property.Name;
	node.Category = "Reflection/Properties";
	if (!ReadPresentation(metadata, node)) return Invalid(location, "Invalid display metadata.");
	const auto displayName = node.DisplayName;
	node.Type = id + ".Get";
	node.DisplayName = "Get " + displayName;
	node.Reflection = { EGraphReflectionKind::PropertyGet, owner.QualifiedName, property.Name, {} };
	node.Pins = { TargetPin(owner.QualifiedName), { "result", Direction::Output, *type, {} } };
	auto result = graph.Register(node);
	if (!result.Succeeded()) { result.Message = location + ": " + result.Message; return result; }
	if (!readWrite) return {};
	node.Type = id + ".Set";
	node.DisplayName = "Set " + displayName;
	node.Reflection.Kind = EGraphReflectionKind::PropertySet;
	node.Pins = { { "exec", Direction::Input, Type::Execution, {} }, TargetPin(owner.QualifiedName),
		{ "value", Direction::Input, *type, {} }, { "then", Direction::Output, Type::Execution, {} } };
	result = graph.Register(std::move(node));
	if (!result.Succeeded()) result.Message = location + ": " + result.Message;
	return result;
}
}

namespace Engine
{
GraphDiagnostic RegisterReflectedGraphNodes(GraphRegistry& graph, const reflection::Registry& reflection)
{
	GraphRegistry candidate = graph;
	for (const auto& entry : reflection.GetFreeFunctions())
	{
		auto result = RegisterFunction(candidate, *entry.Function, {}, std::string(entry.QualifiedName));
		if (!result.Succeeded()) return result;
	}
	for (const auto* type : reflection.GetTypes())
	{
		if (HasGraphMetadata(type->Metadata))
			return Invalid(type->QualifiedName, "Graph annotations belong on FUNCTION or PROPERTY declarations.");
		for (const auto& function : type->Functions)
		{
			auto result = RegisterFunction(candidate, function, type->QualifiedName, function.Name);
			if (!result.Succeeded()) return result;
		}
		for (const auto& property : type->Properties)
		{
			auto result = RegisterProperty(candidate, *type, property);
			if (!result.Succeeded()) return result;
		}
	}
	graph = std::move(candidate);
	return {};
}
}
