#pragma once

#include "GraphTypes.h"
#include <algorithm>
#include <cmath>
#include <type_traits>

namespace Engine
{

inline bool GraphValueMatchesType(const GraphValue& value, EGraphPinType type)
{
	switch (type)
	{
	case EGraphPinType::Bool: return std::holds_alternative<bool>(value);
	case EGraphPinType::Int: return std::holds_alternative<std::int64_t>(value);
	case EGraphPinType::Float: return std::holds_alternative<float>(value);
	case EGraphPinType::Float2: return std::holds_alternative<std::array<float, 2>>(value);
	case EGraphPinType::Float3: return std::holds_alternative<std::array<float, 3>>(value);
	case EGraphPinType::Float4: return std::holds_alternative<std::array<float, 4>>(value);
	default: return false;
	}
}

inline bool IsFiniteGraphValue(const GraphValue& value)
{
	return std::visit([](const auto& item)
	{
		using T = std::decay_t<decltype(item)>;
		if constexpr (std::is_same_v<T, float>)
			return std::isfinite(item);
		else if constexpr (std::is_same_v<T, std::array<float, 2>> ||
			std::is_same_v<T, std::array<float, 3>> || std::is_same_v<T, std::array<float, 4>>)
			return std::all_of(item.begin(), item.end(), [](float element) { return std::isfinite(element); });
		else
			return true;
	}, value);
}

}
