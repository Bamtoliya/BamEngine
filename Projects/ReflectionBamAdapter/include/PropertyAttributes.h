#pragma once
#include "Engine_Includes.h"

BEGIN(Engine)

struct MetaRange
{
	f32 Min;
	f32 Max;
	f32 Speed;
	MetaRange(f32 InMin = -FLT_MAX, f32 InMax = FLT_MAX, f32 InSpeed = 1.f) : Min(InMin), Max(InMax), Speed(InSpeed) {}
};

struct MetaColor
{
	vec4 RGBA;
};

struct MetaEditCondition
{
	string_view ConditionVariableName;
	uint64 Mask;
	bool bExactMatch;

	constexpr MetaEditCondition(string_view InConditionVariableName, uint64 InMask = 0, bool InExactMatch = false)
		: ConditionVariableName(InConditionVariableName), Mask(InMask), bExactMatch(InExactMatch) {}

	template<typename T>
	requires(std::is_integral_v<T> || std::is_enum_v<T>)
	constexpr MetaEditCondition(string_view InConditionVariableName, T InMask, bool InExactMatch = false)
		: ConditionVariableName(InConditionVariableName), Mask(static_cast<uint64>(InMask)), bExactMatch(InExactMatch) {
	}
};

struct MetaOnChanged
{
	static constexpr size_t MaxArgumentCount = 4;
	string_view FunctionName;
	std::array<string_view, MaxArgumentCount> ArgumentPropertyNames{};
	uint32 ArgumentCount = 0;

	constexpr MetaOnChanged(string_view InFunctionName) : FunctionName(InFunctionName) {}

	template<typename... TArgs>
	requires ((std::is_convertible_v<TArgs, string_view> && ...) && (sizeof...(TArgs) <= MaxArgumentCount))
	constexpr MetaOnChanged(string_view inFunctionName, TArgs... inArgs)
		: FunctionName(inFunctionName)
		, ArgumentPropertyNames{ string_view(inArgs)... }
		, ArgumentCount(static_cast<uint32>(sizeof...(TArgs)))
	{
	}
};

END