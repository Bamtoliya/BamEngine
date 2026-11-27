#include "GraphRegistry.h"
#include "GraphValue.h"

namespace Engine
{
	namespace
	{
		GraphDiagnostic ValidateAndRegister(std::unordered_map<std::string, GraphNodeDefinition>& defs,
			GraphNodeDefinition definition, bool allowUpdate)
		{
			auto invalid = [](std::string pin, std::string message)
				{
					return GraphDiagnostic{ EGraphError::InvalidDefinition, 0, std::move(pin), 0, std::move(message) };
				};
			if (definition.Type.empty() || definition.Domains.empty())
				return invalid({}, "Node type and at least one domain are required.");

			if (!allowUpdate && defs.find(definition.Type) != defs.end())
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
				case EGraphPinType::Float4:
				case EGraphPinType::String: break;
				case EGraphPinType::Object:
					if (pin.ObjectType.empty()) return invalid(pin.Key, "Object pins require a reflected type name.");
					break;
				default: return invalid(pin.Key, "Invalid pin type.");
				}
				if (pin.Type != EGraphPinType::Object && !pin.ObjectType.empty())
					return invalid(pin.Key, "Only object pins can specify an object type.");
				// 연결할 수 없는 문자열 입력은 HLSL로 내보내지 않는 노드 설정값(텍스처 이름 등)이므로 허용한다.
				const bool literalString = pin.Type == EGraphPinType::String && pin.Direction == EGraphPinDirection::Input && pin.LiteralOnly;
				if (shader && (pin.Type == EGraphPinType::Execution || pin.Type == EGraphPinType::Object || (pin.Type == EGraphPinType::String && !literalString)))
					return invalid(pin.Key, "Shader nodes cannot declare execution, object, or connectable string pins.");
				if (!std::holds_alternative<std::monostate>(pin.DefaultValue))
				{
					if (pin.Direction != EGraphPinDirection::Input ||
						!GraphValueMatchesType(pin.DefaultValue, pin.Type) || !IsFiniteGraphValue(pin.DefaultValue))
						return invalid(pin.Key, "Only data inputs can have a finite, exactly typed default value.");
				}
			}

			const auto type = definition.Type;
			if (allowUpdate) defs.insert_or_assign(type, std::move(definition));
			else defs.emplace(type, std::move(definition));
			return {};
		}
	}

	GraphDiagnostic GraphRegistry::Register(GraphNodeDefinition definition)
	{
		return ValidateAndRegister(m_Definitions, std::move(definition), false);
	}

	GraphDiagnostic GraphRegistry::RegisterOrUpdate(GraphNodeDefinition definition)
	{
		return ValidateAndRegister(m_Definitions, std::move(definition), true);
	}

	void GraphRegistry::ClearReflectedNodes()
	{
		std::erase_if(m_Definitions, [](const auto& pair)
			{
				return pair.second.Reflection.Kind != EGraphReflectionKind::None;
			});
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
	const std::vector<EGraphDomain> shader{ EGraphDomain::Shader };
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
		{ "Core.Reroute", logic, {
			{ "In", Direction::Input, Type::Execution, {} }, { "Out", Direction::Output, Type::Execution, {} } } },
		{ "Core.Reroute.Bool", common, {
			{ "In", Direction::Input, Type::Bool, false }, { "Out", Direction::Output, Type::Bool, {} } } },
		{ "Core.Reroute.Int", common, {
			{ "In", Direction::Input, Type::Int, std::int64_t{ 0 } }, { "Out", Direction::Output, Type::Int, {} } } },
		{ "Core.Reroute.Float", common, {
			{ "In", Direction::Input, Type::Float, 0.0f }, { "Out", Direction::Output, Type::Float, {} } } },
		{ "Core.Reroute.Float2", common, {
			{ "In", Direction::Input, Type::Float2, std::array<float, 2>{} }, { "Out", Direction::Output, Type::Float2, {} } } },
		{ "Core.Reroute.Float3", common, {
			{ "In", Direction::Input, Type::Float3, std::array<float, 3>{} }, { "Out", Direction::Output, Type::Float3, {} } } },
		{ "Core.Reroute.Float4", common, {
			{ "In", Direction::Input, Type::Float4, std::array<float, 4>{} }, { "Out", Direction::Output, Type::Float4, {} } } },
		// 정리용 주석 박스: 실행·셰이더 생성 대상이 아니며 크기와 문구만 보관한다. 런타임 로드 검증을 위해 엔진 기본 노드로 둔다.
		{ "Core.Comment", common, {
			{ "Size", Direction::Input, Type::Float2, std::array<float, 2>{ 400.0f, 250.0f } } } },
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
		{ "Event.OnActionPressed", logic, {
			{ "mapId", Direction::Input, Type::String, std::string{ "Gameplay" }, {}, "PIN_INPUT_MAP_ID", true },
			{ "actionId", Direction::Input, Type::String, std::string{}, {}, "PIN_INPUT_ACTION_ID", true },
			{ "exec", Direction::Output, Type::Execution, {} } } },
		{ "Event.OnActionReleased", logic, {
			{ "mapId", Direction::Input, Type::String, std::string{ "Gameplay" }, {}, "PIN_INPUT_MAP_ID", true },
			{ "actionId", Direction::Input, Type::String, std::string{}, {}, "PIN_INPUT_ACTION_ID", true },
			{ "exec", Direction::Output, Type::Execution, {} } } },
		{ "Event.OnActionTimelineEvent", logic, {
			{ "eventTag", Direction::Input, Type::String, std::string{}, {}, "PIN_ACTION_TIMELINE_EVENT_TAG", true },
			{ "exec", Direction::Output, Type::Execution, {} } } },
		// reason: 0 끝까지 재생(Completed), 1 Stop, 2 재시작·교체·인계로 끊김(Interrupted). EActionTimelineEndReason 순서와 같다.
		{ "Event.OnActionTimelineEnded", logic, {
			{ "exec", Direction::Output, Type::Execution, {} },
			{ "reason", Direction::Output, Type::Int, {}, {}, "PIN_ACTION_TIMELINE_END_REASON" },
			{ "completed", Direction::Output, Type::Bool, {}, {}, "PIN_ACTION_TIMELINE_COMPLETED" } } },
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
			{ "color", Direction::Input, Type::Float4, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f } } } },
		{ "Core.String", logic, {
			{ "value", Direction::Input, Type::String, std::string{} }, { "result", Direction::Output, Type::String, {} } } },
		{ "Core.Reroute.String", logic, {
			{ "In", Direction::Input, Type::String, std::string{} }, { "Out", Direction::Output, Type::String, {} } } },
		{ "Math.MakeFloat3", common, {
			{ "x", Direction::Input, Type::Float, 0.0f },
			{ "y", Direction::Input, Type::Float, 0.0f },
			{ "z", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float3, {} } } },
		{ "Math.BreakFloat3", common, {
			{ "value", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "x", Direction::Output, Type::Float, {} },
			{ "y", Direction::Output, Type::Float, {} },
			{ "z", Direction::Output, Type::Float, {} } } },
		{ "Math.MakeFloat4", common, {
			{ "x", Direction::Input, Type::Float, 0.0f },
			{ "y", Direction::Input, Type::Float, 0.0f },
			{ "z", Direction::Input, Type::Float, 0.0f },
			{ "w", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float4, {} } } },
		{ "Math.BreakFloat4", common, {
			{ "value", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "x", Direction::Output, Type::Float, {} },
			{ "y", Direction::Output, Type::Float, {} },
			{ "z", Direction::Output, Type::Float, {} },
			{ "w", Direction::Output, Type::Float, {} } } },
		{ "Math.AddFloat2", common, {
			{ "a", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "b", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "result", Direction::Output, Type::Float2, {} } } },
		{ "Math.SubtractFloat2", common, {
			{ "a", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "b", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "result", Direction::Output, Type::Float2, {} } } },
		{ "Math.MultiplyFloat2", shader, {
			{ "a", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "b", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "result", Direction::Output, Type::Float2, {} } } },
		{ "Math.DotFloat2", common, {
			{ "a", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "b", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.NormalizeFloat2", common, {
			{ "value", Direction::Input, Type::Float2, std::array<float, 2>{ 0.0f, 1.0f } },
			{ "result", Direction::Output, Type::Float2, {} } } },
		{ "Math.LerpFloat2", common, {
			{ "a", Direction::Input, Type::Float2, std::array<float, 2>{} },
			{ "b", Direction::Input, Type::Float2, std::array<float, 2>{ 1.0f, 1.0f } },
			{ "t", Direction::Input, Type::Float, 0.5f },
			{ "result", Direction::Output, Type::Float2, {} } } },
		{ "Math.SubtractFloat", common, {
			{ "a", Direction::Input, Type::Float, 0.0f },
			{ "b", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.DivideFloat", common, {
			{ "a", Direction::Input, Type::Float, 0.0f },
			{ "b", Direction::Input, Type::Float, 1.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.LerpFloat", common, {
			{ "a", Direction::Input, Type::Float, 0.0f },
			{ "b", Direction::Input, Type::Float, 1.0f },
			{ "t", Direction::Input, Type::Float, 0.5f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.ClampFloat", common, {
			{ "value", Direction::Input, Type::Float, 0.0f },
			{ "minimum", Direction::Input, Type::Float, 0.0f },
			{ "maximum", Direction::Input, Type::Float, 1.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.SaturateFloat", common, {
			{ "value", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.PowFloat", common, {
			{ "base", Direction::Input, Type::Float, 1.0f },
			{ "exponent", Direction::Input, Type::Float, 1.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.AbsFloat", common, {
			{ "value", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.MinFloat", common, {
			{ "a", Direction::Input, Type::Float, 0.0f },
			{ "b", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.MaxFloat", common, {
			{ "a", Direction::Input, Type::Float, 0.0f },
			{ "b", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.SinFloat", common, {
			{ "value", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.CosFloat", common, {
			{ "value", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.StepFloat", common, {
			{ "edge", Direction::Input, Type::Float, 0.0f },
			{ "value", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.SmoothStepFloat", common, {
			{ "edge0", Direction::Input, Type::Float, 0.0f },
			{ "edge1", Direction::Input, Type::Float, 1.0f },
			{ "value", Direction::Input, Type::Float, 0.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.AddFloat3", common, {
			{ "a", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "b", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "result", Direction::Output, Type::Float3, {} } } },
		{ "Math.SubtractFloat3", common, {
			{ "a", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "b", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "result", Direction::Output, Type::Float3, {} } } },
		{ "Math.MultiplyFloat3", shader, {
			{ "a", Direction::Input, Type::Float3, std::array<float, 3>{ 1.0f, 1.0f, 1.0f } },
			{ "b", Direction::Input, Type::Float3, std::array<float, 3>{ 1.0f, 1.0f, 1.0f } },
			{ "result", Direction::Output, Type::Float3, {} } } },
		{ "Math.ScaleFloat3", common, {
			{ "value", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "scale", Direction::Input, Type::Float, 1.0f },
			{ "result", Direction::Output, Type::Float3, {} } } },
		{ "Math.DotFloat3", common, {
			{ "a", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "b", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.NormalizeFloat3", common, {
			{ "value", Direction::Input, Type::Float3, std::array<float, 3>{ 0.0f, 0.0f, 1.0f } },
			{ "result", Direction::Output, Type::Float3, {} } } },
		{ "Math.LerpFloat3", common, {
			{ "a", Direction::Input, Type::Float3, std::array<float, 3>{} },
			{ "b", Direction::Input, Type::Float3, std::array<float, 3>{ 1.0f, 1.0f, 1.0f } },
			{ "t", Direction::Input, Type::Float, 0.5f },
			{ "result", Direction::Output, Type::Float3, {} } } },
		{ "Math.AddFloat4", common, {
			{ "a", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "b", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "result", Direction::Output, Type::Float4, {} } } },
		{ "Math.SubtractFloat4", common, {
			{ "a", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "b", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "result", Direction::Output, Type::Float4, {} } } },
		{ "Math.MultiplyFloat4", shader, {
			{ "a", Direction::Input, Type::Float4, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f } },
			{ "b", Direction::Input, Type::Float4, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f } },
			{ "result", Direction::Output, Type::Float4, {} } } },
		{ "Math.ScaleFloat4", common, {
			{ "value", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "scale", Direction::Input, Type::Float, 1.0f },
			{ "result", Direction::Output, Type::Float4, {} } } },
		{ "Math.DotFloat4", common, {
			{ "a", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "b", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "result", Direction::Output, Type::Float, {} } } },
		{ "Math.NormalizeFloat4", common, {
			{ "value", Direction::Input, Type::Float4, std::array<float, 4>{ 0.0f, 0.0f, 0.0f, 1.0f } },
			{ "result", Direction::Output, Type::Float4, {} } } },
		{ "Math.LerpFloat4", common, {
			{ "a", Direction::Input, Type::Float4, std::array<float, 4>{} },
			{ "b", Direction::Input, Type::Float4, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f } },
			{ "t", Direction::Input, Type::Float, 0.5f },
			{ "result", Direction::Output, Type::Float4, {} } } },
		{ "Shader.TexCoord0", shader, {
			{ "uv", Direction::Output, Type::Float2, {}, {}, "Graph.PinName.UV0" } } },
		{ "Shader.TexCoord1", shader, {
			{ "uv", Direction::Output, Type::Float2, {}, {}, "Graph.PinName.UV1" } } },
		{ "Shader.Time", shader, {
			{ "time", Direction::Output, Type::Float, {} } } },
		// StaticMeshRendererComponent::customData (InstanceData.CustomData). 같은 머티리얼에서 인스턴스마다 다른 값을 준다.
		{ "Shader.InstanceCustomData", shader, {
			{ "data", Direction::Output, Type::Float4, {} } } },
		{ "Shader.WorldPosition", shader, {
			{ "position", Direction::Output, Type::Float3, {} } } },
		{ "Shader.WorldNormal", shader, {
			{ "normal", Direction::Output, Type::Float3, {} } } },
		{ "Shader.ViewDirection", shader, {
			{ "direction", Direction::Output, Type::Float3, {} } } },
		{ "Shader.Fresnel", shader, {
			{ "normal", Direction::Input, Type::Float3, std::array<float, 3>{ 0.0f, 0.0f, 1.0f } },
			{ "viewDir", Direction::Input, Type::Float3, std::array<float, 3>{ 0.0f, 0.0f, 1.0f } },
			{ "power", Direction::Input, Type::Float, 5.0f },
			{ "result", Direction::Output, Type::Float, {} } } },
		// 지형 도메인 노드(ShaderGraphCompiler::GenerateTerrainMaterialHlsl에서만 컴파일된다). layer는 0~7 상수로만 받는다.
		{ "Terrain.LayerWeight", shader, {
			{ "layer", Direction::Input, Type::Int, std::int64_t{ 0 }, {}, "Graph.PinName.TerrainLayer", true },
			{ "weight", Direction::Output, Type::Float, {} } } },
		{ "Terrain.LayerSample", shader, {
			{ "layer", Direction::Input, Type::Int, std::int64_t{ 0 }, {}, "Graph.PinName.TerrainLayer", true },
			{ "rgba", Direction::Output, Type::Float4, {}, {}, "Graph.PinName.RGBA" },
			{ "rgb", Direction::Output, Type::Float3, {}, {}, "Graph.PinName.RGB" } } },
		{ "Terrain.LayerBlend", shader, {
			{ "rgba", Direction::Output, Type::Float4, {}, {}, "Graph.PinName.RGBA" },
			{ "rgb", Direction::Output, Type::Float3, {}, {}, "Graph.PinName.RGB" } } },
		{ "Terrain.Slope", shader, {
			{ "slope", Direction::Output, Type::Float, {} } } },
		{ "Terrain.Height", shader, {
			{ "height", Direction::Output, Type::Float, {} } } },
		{ "Shader.TextureSample", shader, {
			{ "slot", Direction::Input, Type::Int, std::int64_t{ 0 }, {}, "Graph.PinName.TextureSlot" },
			{ "uv", Direction::Input, Type::Float2, std::array<float, 2>{}, {}, "Graph.PinName.TextureUV" },
			{ "name", Direction::Input, Type::String, std::string{}, {}, "Graph.PinName.TextureName" },
			{ "rgba", Direction::Output, Type::Float4, {}, {}, "Graph.PinName.RGBA" },
			{ "rgb", Direction::Output, Type::Float3, {}, {}, "Graph.PinName.RGB" },
			{ "r", Direction::Output, Type::Float, {}, {}, "Graph.PinName.R" },
			{ "g", Direction::Output, Type::Float, {}, {}, "Graph.PinName.G" },
			{ "b", Direction::Output, Type::Float, {}, {}, "Graph.PinName.B" },
			{ "a", Direction::Output, Type::Float, {}, {}, "Graph.PinName.A" } } },
		{ "Shader.ScreenUV", shader, {
			{ "uv", Direction::Output, Type::Float2, {}, {}, "Graph.PinName.ScreenUV" } } },
		{ "Shader.SceneColor", shader, {
			{ "uv", Direction::Input, Type::Float2, std::array<float, 2>{}, {}, "Graph.PinName.ScreenUV" },
			{ "rgba", Direction::Output, Type::Float4, {}, {}, "Graph.PinName.RGBA" },
			{ "rgb", Direction::Output, Type::Float3, {}, {}, "Graph.PinName.RGB" },
			{ "r", Direction::Output, Type::Float, {}, {}, "Graph.PinName.R" },
			{ "g", Direction::Output, Type::Float, {}, {}, "Graph.PinName.G" },
			{ "b", Direction::Output, Type::Float, {}, {}, "Graph.PinName.B" },
			{ "a", Direction::Output, Type::Float, {}, {}, "Graph.PinName.A" } } },
		{ "Shader.MaterialOutput", { EGraphDomain::Shader }, {
			// [VS Stage Input]
			{ "worldPositionOffset", Direction::Input, Type::Float3, std::array<float, 3>{ 0.0f, 0.0f, 0.0f } },
			{ "customUV0", Direction::Input, Type::Float2, std::array<float, 2>{ 0.0f, 0.0f }, {}, "Graph.PinName.CustomUV0" },
			{ "customUV1", Direction::Input, Type::Float2, std::array<float, 2>{ 0.0f, 0.0f }, {}, "Graph.PinName.CustomUV1" },
			// [PS Stage Inputs]
			{ "baseColor", Direction::Input, Type::Float4, std::array<float, 4>{ 1.0f, 1.0f, 1.0f, 1.0f } },
			{ "roughness", Direction::Input, Type::Float, 0.2f },
			{ "metallic", Direction::Input, Type::Float, 0.0f },
			{ "cavity", Direction::Input, Type::Float, 0.5f },
			{ "ao", Direction::Input, Type::Float, 1.0f },
			{ "emissive", Direction::Input, Type::Float3, std::array<float, 3>{ 0.0f, 0.0f, 0.0f } },
			{ "normal", Direction::Input, Type::Float3, std::array<float, 3>{ 0.0f, 0.0f, 1.0f } },
			{ "opacityMask", Direction::Input, Type::Float, 1.0f } } },
		{ "Event.OnTriggerEnter", logic, {
			{ "exec", Direction::Output, Type::Execution, {} }, { "other", Direction::Output, Type::Int, {} } } },
		{ "Event.OnTriggerStay", logic, {
			{ "exec", Direction::Output, Type::Execution, {} }, { "other", Direction::Output, Type::Int, {} } } },
		{ "Event.OnTriggerExit", logic, {
			{ "exec", Direction::Output, Type::Execution, {} }, { "other", Direction::Output, Type::Int, {} } } },
	};
	for (auto& definition : definitions)
	{
		const auto separator = definition.Type.find('.');
		definition.Category = definition.Type.substr(0, separator);
		definition.DisplayName = definition.Type.substr(separator + 1);
		if (definition.Type.rfind("Core.Reroute", 0) == 0)
		{
			definition.Category = "Organization";
			definition.DisplayName = "Reroute";
			definition.Tooltip = "Pass through execution or data wire cleanly.";
		}
		if (definition.Type == "Core.Comment")
		{
			definition.Category = "Organization";
			for (auto& pin : definition.Pins) pin.LiteralOnly = true; // 연결 불가, 값만 저장
		}
		if (definition.Type == "Math.AddFloat2") definition.DisplayName = "Graph.Node.AddFloat2";
		if (definition.Type == "Math.MultiplyFloat2") definition.DisplayName = "Graph.Node.MultiplyFloat2";
		if (definition.Type == "Shader.TexCoord0") definition.DisplayName = "Graph.Node.TexCoord0";
		if (definition.Type == "Shader.TexCoord1") definition.DisplayName = "Graph.Node.TexCoord1";
		if (definition.Type == "Shader.TextureSample")
		{
			definition.Category = "Texture";
			definition.DisplayName = "Graph.Node.TextureSample";
			for (auto& pin : definition.Pins)
			{
				if (pin.Key == "slot" || pin.Key == "name") pin.LiteralOnly = true;
			}
		}
		if (definition.Type == "Shader.ScreenUV")
		{
			definition.Category = "Coordinates";
			definition.DisplayName = "Graph.Node.ScreenUV";
		}
		if (definition.Type == "Shader.SceneColor")
		{
			definition.Category = "Texture";
			definition.DisplayName = "Graph.Node.SceneColor";
		}
	}
	for (auto& definition : definitions)
	{
		if (definition.Type == "Shader.ScreenUV") definition.Tooltip = "Normalized screen-space coordinates [0, 1].";
		if (definition.Type == "Shader.SceneColor") definition.Tooltip = "Sample the opaque scene background color buffer.";
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
