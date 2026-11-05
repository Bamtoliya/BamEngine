#include "SplineDistanceTable.h"
#include "SplineCurve.h"

namespace
{
	bool IsSplineDistanceTransformValid(const Engine::mat4& matrix)
	{
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
			{
				if (!std::isfinite(matrix[column][row]))
					return false;
			}
		}

		// 경로 길이 계산에는 아핀 변환만 허용합니다.
		return matrix[0][3] == 0.0f && matrix[1][3] == 0.0f &&
			matrix[2][3] == 0.0f && matrix[3][3] == 1.0f;
	}
}

namespace Engine
{
	bool SplineDistanceTable::Build(std::span<const vec3> points, bool closed, const mat4& localToWorld,
		size_t subdivisionsPerSegment)
	{
		if (points.empty() || (closed && points.size() < 3) ||
			subdivisionsPerSegment < 4 || subdivisionsPerSegment > 64 ||
			!IsSplineDistanceTransformValid(localToWorld))
			return false;

		const size_t segmentCount = closed ? points.size() : points.size() - 1;
		if (segmentCount > MaxSubdivisionCount / subdivisionsPerSegment)
			return false;

		const size_t subdivisionCount = std::max(size_t{ 1 }, segmentCount * subdivisionsPerSegment);
		const glm::dmat3 linearTransform{ glm::dmat4(localToWorld) };

		SplineSample previous;
		if (!SplineCurve::Evaluate(points, closed, 0.0, previous))
			return false;

		std::vector<Entry> candidate;
		candidate.reserve(subdivisionCount + 1);
		candidate.push_back(Entry{ 0.0, 0.0 });

		f64 totalDistance = 0.0;
		for (size_t i = 1; i <= subdivisionCount; ++i)
		{
			const f64 parameter = static_cast<f64>(i) / static_cast<f64>(subdivisionCount);

			SplineSample current;
			if (!SplineCurve::Evaluate(points, closed, parameter, current))
				return false;

			const glm::dvec3 localDelta = glm::dvec3(current.Position) - glm::dvec3(previous.Position);
			const glm::dvec3 worldDelta = linearTransform * localDelta;
			const f64 stepDistance = glm::length(worldDelta);

			if (!std::isfinite(stepDistance))
				return false;

			totalDistance += stepDistance;
			if (!std::isfinite(totalDistance))
				return false;

			candidate.push_back(Entry{ parameter, totalDistance });
			previous = current;
		}

		m_Entries = std::move(candidate);
		return true;
	}

	bool SplineDistanceTable::TryGetParameter(f64 distance, f64& outParameter) const
	{
		if (m_Entries.empty() || !std::isfinite(distance))
			return false;

		const f64 totalDistance = GetLength();
		if (totalDistance == 0.0 || distance <= 0.0)
		{
			outParameter = 0.0;
			return true;
		}

		if (distance >= totalDistance)
		{
			outParameter = 1.0;
			return true;
		}

		const auto upper = std::upper_bound(m_Entries.begin(), m_Entries.end(), distance,
			[](f64 value, const Entry& entry) { return value < entry.Distance; });

		const auto lower = std::prev(upper);
		const f64 intervalDistance = upper->Distance - lower->Distance;
		const f64 alpha = (distance - lower->Distance) / intervalDistance;

		outParameter = lower->Parameter + (upper->Parameter - lower->Parameter) * alpha;
		return true;
	}
}