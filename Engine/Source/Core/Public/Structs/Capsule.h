#pragma once
#include "Engine_API.h"
#include "Reflection/ReflectionMacro.h"
#include <glm/glm.hpp>

using namespace Engine;

STRUCT(DISPLAY_NAME("STRUCT_CAPSULE"), CATEGORY("CATEGORY_BOUNDS"))
struct Capsule
{
	REFLECT_BODY()
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_POINT_A"))
	vec3 pointA{ 0.f };
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_POINT_B"))
	vec3 pointB{ 0.f };
	PROPERTY(EDITABLE, DISPLAY_NAME("PROP_RADIUS"))
	f32 radius = { 0.f };
	Capsule() = default;
	Capsule(const vec3& pointA, const vec3& pointB, f32 radius)
		: pointA(pointA), pointB(pointB), radius(radius) {
	}
	vec3 Center() const { return (pointA + pointB) * 0.5f; }
	f32 Height() const { return glm::distance(pointA, pointB); }
	bool IsValid() const
	{
		return IsFinite(pointA) && IsFinite(pointB) &&
			std::isfinite(radius) && radius >= 0.f;
	}

	bool Contains(const vec3& point) const
	{
		if (!IsValid() || !IsFinite(point))
			return false;

		const glm::dvec3 start(pointA);
		const glm::dvec3 segment = glm::dvec3(pointB) - start;
		const glm::dvec3 position(point);
		const double lengthSquared = glm::dot(segment, segment);

		const double t = lengthSquared > 0.0
			? glm::clamp(glm::dot(position - start, segment) / lengthSquared, 0.0, 1.0)
			: 0.0;

		const glm::dvec3 delta = start + segment * t - position;
		return glm::dot(delta, delta) <= static_cast<double>(radius) * radius;
	}

};