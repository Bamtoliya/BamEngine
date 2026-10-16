#pragma once

#include "Engine_Includes.h"
#include "Structs.h"
#include <cmath>

namespace Editor::PickingMath
{
	inline bool IsFinite(const glm::vec3& value)
	{
		return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
	}

	inline bool TryGetViewportUV(const glm::vec2& mouse, const glm::vec2& origin, const glm::vec2& size, glm::vec2& uv)
	{
		if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0.0f || size.y <= 0.0f) return false;
		uv = (mouse - origin) / size;
		return std::isfinite(uv.x) && std::isfinite(uv.y) &&
			uv.x >= 0.0f && uv.y >= 0.0f && uv.x < 1.0f && uv.y < 1.0f;
	}

	// BamEngine uses left-handed projection with NDC depth [0, 1].
	inline bool TryBuildRay(const glm::mat4& inverseViewProjection, const glm::vec2& uv, Engine::Ray& ray,
		float* maxDistance = nullptr)
	{
		if (!std::isfinite(uv.x) || !std::isfinite(uv.y) ||
			uv.x < 0.0f || uv.y < 0.0f || uv.x >= 1.0f || uv.y >= 1.0f) return false;
		const float x = uv.x * 2.0f - 1.0f;
		const float y = 1.0f - uv.y * 2.0f;
		const glm::vec4 nearPoint = inverseViewProjection * glm::vec4(x, y, 0.0f, 1.0f);
		const glm::vec4 farPoint = inverseViewProjection * glm::vec4(x, y, 1.0f, 1.0f);
		if (!std::isfinite(nearPoint.w) || !std::isfinite(farPoint.w) ||
			nearPoint.w == 0.0f || farPoint.w == 0.0f) return false;
		const glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
		const glm::vec3 delta = glm::vec3(farPoint) / farPoint.w - origin;
		const float length = glm::length(delta);
		if (!IsFinite(origin) || !IsFinite(delta) || !std::isfinite(length) || length <= 0.0f) return false;
		ray = Engine::Ray(origin, delta / length);
		if (maxDistance) *maxDistance = length;
		return true;
	}

	// Broad phase only: tests the mesh's local AABB, including rotation, scale and parent transforms.
	// The world ray is normalized once; the local direction stays unnormalized to preserve world distance.
	inline bool RaycastBounds(const Engine::Ray& ray, const Engine::AABB& bounds, const glm::mat4& world,
		float maxDistance, float& distance)
	{
		if (!IsFinite(ray.origin) || !IsFinite(ray.direction) || !IsFinite(bounds.min) || !IsFinite(bounds.max) ||
			!std::isfinite(maxDistance) || maxDistance < 0.0f || glm::any(glm::greaterThan(bounds.min, bounds.max)))
			return false;
		const float length = glm::length(ray.direction);
		const float determinant = glm::determinant(world);
		if (!std::isfinite(length) || length <= 0.0f || !std::isfinite(determinant) || determinant == 0.0f)
			return false;
		const glm::mat4 inverseWorld = glm::inverse(world);
		const glm::vec3 origin = glm::vec3(inverseWorld * glm::vec4(ray.origin, 1.0f));
		const glm::vec3 direction = glm::vec3(inverseWorld * glm::vec4(ray.direction / length, 0.0f));
		if (!IsFinite(origin) || !IsFinite(direction)) return false;
		float enter = 0.0f;
		float exit = maxDistance;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (direction[axis] == 0.0f)
			{
				if (origin[axis] < bounds.min[axis] || origin[axis] > bounds.max[axis]) return false;
				continue;
			}
			const float first = (bounds.min[axis] - origin[axis]) / direction[axis];
			const float second = (bounds.max[axis] - origin[axis]) / direction[axis];
			enter = glm::max(enter, glm::min(first, second));
			exit = glm::min(exit, glm::max(first, second));
			if (enter > exit) return false;
		}
		distance = enter;
		return true;
	}
}
