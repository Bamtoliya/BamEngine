#pragma once

#include "Engine_Includes.h"

namespace Engine
{
	struct SplinePlacementSettings
	{
		f64 Spacing = 1.0;
		f64 StartDistance = 0.0;
		vec3 InitialUp{ 0.0f, 1.0f, 0.0f };
	};

	class ENGINE_API SplinePlacement final
	{
	public:
		static constexpr size_t MaxInstanceCount = 1024;

		// 결과는 월드 행렬입니다. 로컬 +Z가 경로 전방을 향합니다.
		// 실패 시 outMatrices를 보존합니다.
		// 길이 0 또는 시작 거리가 배치 범위 밖이면 빈 결과로 성공합니다.
		// pointRollDegrees는 비어 있거나 points와 같은 개수여야 합니다.
		// 각도는 구간 매개변수에 따라 숫자 그대로 선형 보간합니다.
		static bool BuildMatrices(std::span<const vec3> points, bool closed, const mat4& localToWorld,
			const SplinePlacementSettings& settings, std::vector<mat4>& outMatrices,
			std::span<const f32> pointRollDegrees = {});
	};
}