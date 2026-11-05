#include "SplineCurve.h"
#include <glm/gtx/spline.hpp>

namespace
{
	bool IsFiniteSplinePoint(const Engine::vec3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	bool IsRepresentableSplineValue(const glm::dvec3& value)
	{
		const double limit = static_cast<double>((std::numeric_limits<float>::max)());

		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z) &&
			std::abs(value.x) <= limit && std::abs(value.y) <= limit && std::abs(value.z) <= limit;
	}
}

namespace Engine
{
	bool SplineCurve::Evaluate(std::span<const vec3> points, bool closed, f64 parameter, SplineSample& outSample)
	{
		if (!std::isfinite(parameter) || points.empty() || (closed && points.size() < 3))
			return false;

		for (const auto& point : points)
		{
			if (!IsFiniteSplinePoint(point))
				return false;
		}

		if (points.size() == 1)
		{
			outSample = SplineSample{ points.front(), vec3(0.0f) };
			return true;
		}

		const size_t segmentCount = closed ? points.size() : points.size() - 1;
		const f64 scaled = std::clamp(parameter, 0.0, 1.0) * static_cast<f64>(segmentCount);
		const size_t segment = std::min(static_cast<size_t>(scaled), segmentCount - 1);
		const f64 localParameter = scaled - static_cast<f64>(segment);

		glm::dvec3 p0;
		glm::dvec3 p1;
		glm::dvec3 p2;
		glm::dvec3 p3;

		if (closed)
		{
			const size_t next = (segment + 1) % points.size();
			const size_t afterNext = (next + 1) % points.size();
			const size_t previous = segment == 0 ? points.size() - 1 : segment - 1;

			p0 = glm::dvec3(points[previous]);
			p1 = glm::dvec3(points[segment]);
			p2 = glm::dvec3(points[next]);
			p3 = glm::dvec3(points[afterNext]);
		}
		else
		{
			p1 = glm::dvec3(points[segment]);
			p2 = glm::dvec3(points[segment + 1]);

			// 열린 경로의 양 끝에는 직선으로 연장한 가상 제어점을 사용합니다.
			p0 = segment > 0 ? glm::dvec3(points[segment - 1]) : 2.0 * p1 - p2;
			p3 = segment + 2 < points.size() ? glm::dvec3(points[segment + 2]) : 2.0 * p2 - p1;
		}

		glm::dvec3 position = glm::catmullRom(p0, p1, p2, p3, localParameter);

		// 제어점 위치를 정확히 반환하고, 끝점에서 계산 오차가 누적되지 않게 합니다.
		if (localParameter == 0.0)
			position = p1;
		else if (localParameter == 1.0)
			position = p2;

		// Catmull–Rom 다항식 P(u) = a*u^3 + b*u^2 + c*u + p1의 계수입니다.
		const glm::dvec3 a = 0.5 * (-p0 + 3.0 * p1 - 3.0 * p2 + p3);
		const glm::dvec3 b = 0.5 * (2.0 * p0 - 5.0 * p1 + 4.0 * p2 - p3);
		const glm::dvec3 c = 0.5 * (-p0 + p2);

		const glm::dvec3 localTangent = (3.0 * a * localParameter + 2.0 * b) * localParameter + c;
		const glm::dvec3 tangent = localTangent * static_cast<f64>(segmentCount);

		if (!IsRepresentableSplineValue(position) || !IsRepresentableSplineValue(tangent))
			return false;

		outSample = SplineSample{ vec3(position), vec3(tangent) };
		return true;
	}
}