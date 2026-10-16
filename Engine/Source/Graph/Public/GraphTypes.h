#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <variant>

namespace Engine
{

using GraphNodeId = std::uint64_t;
using GraphLinkId = std::uint64_t;

enum class EGraphDomain { Logic, Shader };
enum class EGraphPinDirection { Input, Output };
enum class EGraphPinType { Execution, Bool, Int, Float, Float2, Float3, Float4, Object };

// monostate means "no default". Execution and Object pins never carry literal values.
using GraphValue = std::variant<std::monostate, bool, std::int64_t, float,
	std::array<float, 2>, std::array<float, 3>, std::array<float, 4>>;

enum class EGraphError
{
	None,
	InvalidDefinition,
	DuplicateType,
	UnsupportedDomain,
	NodeNotFound,
	LinkNotFound,
	PinNotFound,
	InvalidDirection,
	TypeMismatch,
	InvalidDefault,
	DuplicateLink,
	InputAlreadyConnected,
	ExecutionOutputAlreadyConnected,
	DataCycle,
	MissingRequiredInput,
	IdExhausted,
	UnsupportedExecution,
	InvalidExecutionState,
	ExecutionFailed,
	ExecutionLimit
};

// Successful AddNode/Connect return their new ID in Node/Link respectively.
struct GraphDiagnostic
{
	EGraphError Code = EGraphError::None;
	GraphNodeId Node = 0;
	std::string Pin;
	GraphLinkId Link = 0;
	std::string Message;

	bool Succeeded() const { return Code == EGraphError::None; }
};

}
