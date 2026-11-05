#pragma once

#include "SplineDistanceTable.h"

namespace Engine
{
	class ENGINE_API SplineFrameTable final
	{
	public:
		// 실패하면 이전 표를 보존합니다.
		bool Build(std::span<const vec3> points, bool closed, const mat4& localToWorld,
			const vec3& initialUp, std::span<const f32> pointRollDegrees = {});

		f64 GetLength() const { return m_Distances.GetLength(); }
		bool IsClosed() const { return m_Closed; }

		// 월드 프레임의 +Z가 접선 방향입니다. 유한한 거리는 [0, 길이]로 제한합니다.
		// 길이가 0이거나 평가에 실패하면 outMatrix를 변경하지 않습니다.
		bool TryEvaluate(f64 distance, mat4& outMatrix) const;

	private:
		struct AxisSample
		{
			glm::dvec3 Forward{ 0.0 };
			glm::dvec3 Up{ 0.0 };
		};

		static constexpr size_t SubdivisionsPerSegment = 32;

		SplineDistanceTable m_Distances;
		std::vector<vec3> m_Points;
		std::vector<f32> m_RollDegrees;
		std::vector<AxisSample> m_Axes;
		glm::dmat4 m_LocalToWorld{ 1.0 };
		f64 m_SeamCorrectionRadians = 0.0;
		bool m_Closed = false;
	};
}