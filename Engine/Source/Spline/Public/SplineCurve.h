#pragma once

#include "Engine_Includes.h"

namespace Engine
{
	struct SplineSample
	{
		vec3 Position{ 0.0f };
		vec3 Tangent{ 0.0f };
	};

	class ENGINE_API SplineCurve final
	{
	public:
		// parameter: 곡선 전체의 0~1. 유한한 범위 초과 값은 끝점으로 제한합니다.
		// Tangent: 정규화하지 않은 dPosition / dParameter.
		// 실패 시 outSample을 변경하지 않습니다.
		static bool Evaluate(std::span<const vec3> points, bool closed, f64 parameter, SplineSample& outSample);
	};
}