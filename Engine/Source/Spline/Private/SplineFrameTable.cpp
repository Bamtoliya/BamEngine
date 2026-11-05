#include "SplineFrameTable.h"
#include "SplineCurve.h"
#include <glm/gtx/quaternion.hpp>

namespace
{
	bool TryNormalizeSplineFrameAxis(const glm::dvec3& value, glm::dvec3& result)
	{
		const double length = glm::length(value);
		if (!std::isfinite(length) || length <= 0.0)
			return false;

		result = value / length;
		return true;
	}

	glm::dvec3 ChooseSplineFrameUp(const glm::dvec3& forward)
	{
		const glm::dvec3 absolute = glm::abs(forward);
		if (absolute.x <= absolute.y && absolute.x <= absolute.z)
			return glm::dvec3(1.0, 0.0, 0.0);
		if (absolute.y <= absolute.z)
			return glm::dvec3(0.0, 1.0, 0.0);

		return glm::dvec3(0.0, 0.0, 1.0);
	}

	bool TryTransportSplineFrameUp(const glm::dvec3& fromForward, const glm::dvec3& fromUp,
		const glm::dvec3& toForward, glm::dvec3& outUp)
	{
		if (glm::dot(fromForward, toForward) <= -1.0 + 1.0e-8)
			return false;

		const glm::dvec3 up = glm::rotation(fromForward, toForward) * fromUp;
		return TryNormalizeSplineFrameAxis(up - toForward * glm::dot(up, toForward), outUp);
	}

	double GetSplineFrameRollRadians(std::span<const Engine::f32> rolls, bool closed, double parameter)
	{
		if (rolls.empty())
			return 0.0;

		const size_t segmentCount = closed ? rolls.size() : rolls.size() - 1;
		const double scaled = std::clamp(parameter, 0.0, 1.0) * static_cast<double>(segmentCount);
		const size_t segment = std::min(static_cast<size_t>(scaled), segmentCount - 1);
		const double localParameter = scaled - static_cast<double>(segment);
		const size_t next = closed ? (segment + 1) % rolls.size() : segment + 1;

		const double roll = std::lerp(static_cast<double>(rolls[segment]), static_cast<double>(rolls[next]),
			localParameter);

		return glm::radians(std::remainder(roll, 360.0));
	}

	bool TryStoreSplineFramePosition(const glm::dvec3& position, Engine::vec3& result)
	{
		const double limit = static_cast<double>((std::numeric_limits<float>::max)());
		for (int axis = 0; axis < 3; ++axis)
		{
			if (!std::isfinite(position[axis]) || std::abs(position[axis]) > limit)
				return false;
		}

		result = Engine::vec3(position);
		return true;
	}
}

namespace Engine
{
	bool SplineFrameTable::Build(std::span<const vec3> points, bool closed, const mat4& localToWorld,
		const vec3& initialUp, std::span<const f32> pointRollDegrees)
	{
		if (!pointRollDegrees.empty() && pointRollDegrees.size() != points.size())
			return false;

		for (const f32 roll : pointRollDegrees)
		{
			if (!std::isfinite(roll))
				return false;
		}

		glm::dvec3 seedUp;
		if (!TryNormalizeSplineFrameAxis(glm::dvec3(initialUp), seedUp))
			return false;

		SplineFrameTable candidate;
		if (!candidate.m_Distances.Build(points, closed, localToWorld, SubdivisionsPerSegment))
			return false;

		candidate.m_Closed = closed;

		if (candidate.GetLength() == 0.0)
		{
			*this = std::move(candidate);
			return true;
		}

		candidate.m_Points.assign(points.begin(), points.end());
		candidate.m_RollDegrees.assign(pointRollDegrees.begin(), pointRollDegrees.end());
		candidate.m_LocalToWorld = glm::dmat4(localToWorld);
		candidate.m_Closed = closed;

		const size_t segmentCount = closed ? points.size() : points.size() - 1;
		const size_t intervalCount = segmentCount * SubdivisionsPerSegment;
		const glm::dmat3 linear(candidate.m_LocalToWorld);
		candidate.m_Axes.reserve(intervalCount + 1);

		for (size_t i = 0; i <= intervalCount; ++i)
		{
			const f64 parameter = static_cast<f64>(i) / static_cast<f64>(intervalCount);
			SplineSample sample;
			AxisSample axes;
			if (!SplineCurve::Evaluate(points, closed, parameter, sample) ||
				!TryNormalizeSplineFrameAxis(linear * glm::dvec3(sample.Tangent), axes.Forward))
				return false;

			if (i == 0)
			{
				const glm::dvec3 up = std::abs(glm::dot(seedUp, axes.Forward)) > 0.999 ?
					ChooseSplineFrameUp(axes.Forward) : seedUp;

				if (!TryNormalizeSplineFrameAxis(up - axes.Forward * glm::dot(up, axes.Forward), axes.Up))
					return false;
			}
			else
			{
				const auto& previous = candidate.m_Axes.back();
				if (!TryTransportSplineFrameUp(previous.Forward, previous.Up, axes.Forward, axes.Up))
					return false;
			}

			candidate.m_Axes.push_back(axes);
		}

		if (closed)
		{
			const auto& first = candidate.m_Axes.front();
			const auto& last = candidate.m_Axes.back();
			const f64 sine = glm::dot(first.Forward, glm::cross(last.Up, first.Up));
			const f64 cosine = glm::dot(last.Up, first.Up);
			candidate.m_SeamCorrectionRadians = std::atan2(sine, cosine);
		}

		*this = std::move(candidate);
		return true;
	}

	bool SplineFrameTable::TryEvaluate(f64 distance, mat4& outMatrix) const
	{
		if (m_Axes.empty() || !std::isfinite(distance))
			return false;

		const f64 length = GetLength();
		const f64 clampedDistance = std::clamp(distance, 0.0, length);
		f64 parameter;
		SplineSample sample;
		if (!m_Distances.TryGetParameter(clampedDistance, parameter) ||
			!SplineCurve::Evaluate(m_Points, m_Closed, parameter, sample))
			return false;

		const size_t lastIndex = m_Axes.size() - 1;
		const size_t index = std::min(static_cast<size_t>(parameter * static_cast<f64>(lastIndex)), lastIndex);
		const auto& anchor = m_Axes[index];

		glm::dvec3 forward;
		glm::dvec3 up;
		glm::dvec3 right;
		if (!TryNormalizeSplineFrameAxis(glm::dmat3(m_LocalToWorld) * glm::dvec3(sample.Tangent), forward) ||
			!TryTransportSplineFrameUp(anchor.Forward, anchor.Up, forward, up) ||
			!TryNormalizeSplineFrameAxis(glm::cross(up, forward), right))
			return false;

		up = glm::cross(forward, right);

		vec3 position;
		const glm::dvec3 worldPosition(m_LocalToWorld * glm::dvec4(sample.Position, 1.0));
		if (!TryStoreSplineFramePosition(worldPosition, position))
			return false;

		const f64 seamCorrection = m_SeamCorrectionRadians * (clampedDistance / length);
		const f64 radians = GetSplineFrameRollRadians(m_RollDegrees, m_Closed, parameter) + seamCorrection;
		const f64 cosine = std::cos(radians);
		const f64 sine = std::sin(radians);

		mat4 matrix(1.0f);
		matrix[0] = vec4(vec3(right * cosine + up * sine), 0.0f);
		matrix[1] = vec4(vec3(up * cosine - right * sine), 0.0f);
		matrix[2] = vec4(vec3(forward), 0.0f);
		matrix[3] = vec4(position, 1.0f);
		outMatrix = matrix;
		return true;
	}
}