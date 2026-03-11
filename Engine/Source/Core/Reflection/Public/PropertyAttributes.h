#pragma once
#include "Base.h"

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
};

END