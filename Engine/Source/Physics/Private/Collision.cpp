#pragma once

#include "TerrainComponent.h"
#include "Collision.h"
#include "Mesh.h"

#pragma region Helper
static bool TryPrepareCollisionRay(const Ray& ray, f32 maxDistance, glm::dvec3& origin, glm::dvec3& direction)
{
	if (!IsFinite(ray.origin) || !IsFinite(ray.direction) ||
		!std::isfinite(maxDistance) || maxDistance < 0.f)
		return false;

	const glm::dvec3 candidateDirection(ray.direction);
	const double length = glm::length(candidateDirection);
	if (!std::isfinite(length) || length <= 0.0)
		return false;

	origin = glm::dvec3(ray.origin);
	direction = candidateDirection / length;
	return true;
}

static bool TryCommitCollisionRayHit(const glm::dvec3& point, const glm::dvec3& normal, double distance, HitResult& result)
{
	const double limit = std::numeric_limits<f32>::max();
	if (!std::isfinite(distance) || distance < 0.0 || distance > limit)
		return false;

	for (int axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(point[axis]) || std::abs(point[axis]) > limit ||
			!std::isfinite(normal[axis]) || std::abs(normal[axis]) > limit)
			return false;
	}

	result.point = vec3(point);
	result.normal = vec3(normal);
	result.distance = static_cast<f32>(distance);
	result.hasHit = true;
	return true;
}

// direction은 단위 벡터이며, 입력 도형과 maxDistance는 호출부에서 검증합니다.
static bool TryFindSphereBoxSweepDistance(const glm::dvec3& origin, const glm::dvec3& direction, double maxDistance, double radius, const BoundingBox& box, double& hitDistance)
{
	const glm::dvec3 minimum(box.min);
	const glm::dvec3 maximum(box.max);
	const double radiusSquared = radius * radius;
	const glm::dvec3 initialOffset = origin - glm::clamp(origin, minimum, maximum);

	if (glm::dot(initialOffset, initialOffset) <= radiusSquared)
	{
		hitDistance = 0.0;
		return true;
	}

	if (maxDistance == 0.0)
		return false;

	// 시작·끝과 각 축의 두 경계면 통과 시점: 최대 8개입니다.
	std::array<double, 8> boundaries{};
	int count = 0;
	boundaries[count++] = 0.0;
	boundaries[count++] = maxDistance;

	for (int axis = 0; axis < 3; ++axis)
	{
		if (direction[axis] == 0.0)
			continue;

		const double first = (minimum[axis] - origin[axis]) / direction[axis];
		const double second = (maximum[axis] - origin[axis]) / direction[axis];

		if (first > 0.0 && first < maxDistance)
			boundaries[count++] = first;

		if (second > 0.0 && second < maxDistance)
			boundaries[count++] = second;
	}

	std::sort(boundaries.begin(), boundaries.begin() + count);

	for (int index = 0; index + 1 < count; ++index)
	{
		const double start = boundaries[index];
		const double end = boundaries[index + 1];

		if (end <= start)
			continue;

		const glm::dvec3 startCenter = origin + direction * start;
		const glm::dvec3 middleCenter = origin + direction * (start + (end - start) * 0.5);
		glm::dvec3 offset(0.0);
		glm::dvec3 velocity(0.0);

		for (int axis = 0; axis < 3; ++axis)
		{
			if (middleCenter[axis] < minimum[axis])
			{
				offset[axis] = startCenter[axis] - minimum[axis];
				velocity[axis] = direction[axis];
			}
			else if (middleCenter[axis] > maximum[axis])
			{
				offset[axis] = startCenter[axis] - maximum[axis];
				velocity[axis] = direction[axis];
			}
		}

		// 구간 시작을 기준으로 a*s*s + 2*b*s + c = 0을 풉니다.
		const double c = glm::dot(offset, offset) - radiusSquared;
		if (c <= 0.0)
		{
			hitDistance = start;
			return true;
		}

		const double a = glm::dot(velocity, velocity);
		const double b = glm::dot(offset, velocity);

		if (a == 0.0 || b >= 0.0)
			continue;

		const glm::dvec3 perpendicular = offset - velocity * (b / a);
		const double radialSquared = radiusSquared - glm::dot(perpendicular, perpendicular);

		if (radialSquared < 0.0)
			continue;

		// 작은 진입 거리를 계산할 때의 뺄셈 오차를 줄인 근 공식입니다.
		const double root = std::sqrt(a * radialSquared);
		const double localDistance = c / (-b + root);

		if (localDistance < 0.0 || localDistance > end - start)
			continue;

		hitDistance = start + localDistance;
		return true;
	}

	return false;
}

// direction은 단위 벡터입니다. 실패하면 기존 최근접 결과를 유지합니다.
static bool TryUpdateClosestTriangleHit(const glm::dvec3& origin, const glm::dvec3& direction, const glm::dvec3& v0, const glm::dvec3& v1, const glm::dvec3& v2, double& closestDistance, HitResult& closestHit)
{
	const glm::dvec3 edge1 = v1 - v0;
	const glm::dvec3 edge2 = v2 - v0;
	const glm::dvec3 faceCross = glm::cross(edge1, edge2);
	const double normalLength = glm::length(faceCross);

	if (!std::isfinite(normalLength) || normalLength <= 0.0)
		return false;

	const glm::dvec3 pvec = glm::cross(direction, edge2);
	const double determinant = glm::dot(edge1, pvec);
	if (!std::isfinite(determinant) || determinant == 0.0)
		return false;

	const double inverseDeterminant = 1.0 / determinant;
	const glm::dvec3 offset = origin - v0;
	const double u = glm::dot(offset, pvec) * inverseDeterminant;
	if (!std::isfinite(u) || u < 0.0 || u > 1.0)
		return false;

	const glm::dvec3 qvec = glm::cross(offset, edge1);
	const double v = glm::dot(direction, qvec) * inverseDeterminant;
	if (!std::isfinite(v) || v < 0.0 || u + v > 1.0)
		return false;

	const double distance = glm::dot(edge2, qvec) * inverseDeterminant;
	if (!std::isfinite(distance) || distance < 0.0 || distance > closestDistance)
		return false;

	const glm::dvec3 point = origin + direction * distance;
	const glm::dvec3 normal = faceCross / normalLength;
	if (!TryCommitCollisionRayHit(point, normal, distance, closestHit))
		return false;

	closestHit.barycentric = vec3(1.0 - u - v, u, v);
	closestDistance = distance;
	return true;
}

static bool IsValidCollisionRect(const Rect& rect)
{
	return std::isfinite(rect.left) && std::isfinite(rect.top) &&
		std::isfinite(rect.width) && std::isfinite(rect.height) &&
		std::isfinite(rect.Right()) && std::isfinite(rect.Bottom()) &&
		rect.width > 0.f && rect.height > 0.f;
}

static bool TryBuildCollisionRectCorners(const Rect& rect, const mat3& matrix, std::array<glm::dvec2, 4>& corners)
{
	if (!IsValidCollisionRect(rect))
		return false;

	for (int column = 0; column < 3; ++column)
	{
		for (int row = 0; row < 3; ++row)
		{
			if (!std::isfinite(matrix[column][row]))
				return false;
		}
	}

	if (matrix[0][2] != 0.f || matrix[1][2] != 0.f || matrix[2][2] != 1.f)
		return false;

	const std::array<glm::dvec2, 4> localCorners{
		glm::dvec2(rect.left, rect.top),
		glm::dvec2(rect.Right(), rect.top),
		glm::dvec2(rect.Right(), rect.Bottom()),
		glm::dvec2(rect.left, rect.Bottom())
	};

	const glm::dmat3 transform(matrix);
	for (size_t index = 0; index < corners.size(); ++index)
	{
		corners[index] = glm::dvec2(transform * glm::dvec3(localCorners[index], 1.0));

		if (!std::isfinite(corners[index].x) || !std::isfinite(corners[index].y))
			return false;
	}

	const glm::dvec2 horizontal = corners[1] - corners[0];
	const glm::dvec2 vertical = corners[3] - corners[0];
	const double area = horizontal.x * vertical.y - horizontal.y * vertical.x;
	return std::isfinite(area) && area != 0.0;
}

static bool TryGetTransformedRectContact(const Rect& a, const mat3& transformA, const Rect& b, const mat3& transformB,
	glm::dvec2& normal, double& depth, glm::dvec2& point)
{
	std::array<glm::dvec2, 4> cornersA{}, cornersB{};
	if (!TryBuildCollisionRectCorners(a, transformA, cornersA) ||
		!TryBuildCollisionRectCorners(b, transformB, cornersB))
		return false;

	const std::array<glm::dvec2, 4> edges{
		cornersA[1] - cornersA[0], cornersA[3] - cornersA[0],
		cornersB[1] - cornersB[0], cornersB[3] - cornersB[0]
	};

	const glm::dvec2 origin = cornersA[0];
	normal = glm::dvec2(0.0);
	depth = std::numeric_limits<double>::max();

	for (const glm::dvec2& edge : edges)
	{
		const glm::dvec2 axis = glm::normalize(glm::dvec2(-edge.y, edge.x));
		double minimumA = std::numeric_limits<double>::max();
		double maximumA = std::numeric_limits<double>::lowest();
		double minimumB = std::numeric_limits<double>::max();
		double maximumB = std::numeric_limits<double>::lowest();

		for (size_t index = 0; index < cornersA.size(); ++index)
		{
			const double projectionA = glm::dot(cornersA[index] - origin, axis);
			const double projectionB = glm::dot(cornersB[index] - origin, axis);
			minimumA = glm::min(minimumA, projectionA);
			maximumA = glm::max(maximumA, projectionA);
			minimumB = glm::min(minimumB, projectionB);
			maximumB = glm::max(maximumB, projectionB);
		}

		const double moveNegative = maximumA - minimumB;
		const double movePositive = maximumB - minimumA;
		if (moveNegative < 0.0 || movePositive < 0.0)
			return false;

		const double candidateDepth = glm::min(moveNegative, movePositive);
		if (candidateDepth < depth)
		{
			depth = candidateDepth;
			normal = moveNegative <= movePositive ? -axis : axis;
		}
	}

	// 첫 도형에서 상대 도형을 향하는 면 또는 꼭짓점의 대표점을 구합니다.
	double minimumProjection = std::numeric_limits<double>::max();
	glm::dvec2 relativePoint(0.0);
	int count = 0;

	for (const glm::dvec2& corner : cornersA)
	{
		const glm::dvec2 relative = corner - origin;
		const double projection = glm::dot(relative, normal);

		if (projection < minimumProjection)
		{
			minimumProjection = projection;
			relativePoint = relative;
			count = 1;
		}
		else if (projection == minimumProjection)
		{
			relativePoint += relative;
			++count;
		}
	}

	point = origin + relativePoint / static_cast<double>(count);
	return true;
}

static glm::dvec3 ClosestPointOnCapsuleSegment(const Capsule& capsule, const glm::dvec3& point)
{
	const glm::dvec3 start(capsule.pointA);
	const glm::dvec3 segment = glm::dvec3(capsule.pointB) - start;
	const double lengthSquared = glm::dot(segment, segment);

	const double t = lengthSquared > 0.0
		? glm::clamp(glm::dot(point - start, segment) / lengthSquared, 0.0, 1.0)
		: 0.0;

	return start + segment * t;
}

static bool TryGetCapsuleRectContact(const Capsule& capsule, const Rect& rect, glm::dvec2& normal, double& depth)
{
	if (!capsule.IsValid() || !IsValidCollisionRect(rect) ||
		capsule.pointA.z != 0.f || capsule.pointB.z != 0.f)
		return false;

	const glm::dvec2 minimum(rect.left, rect.top);
	const glm::dvec2 maximum(rect.Right(), rect.Bottom());
	const glm::dvec2 center = (minimum + maximum) * 0.5;
	const glm::dvec2 halfSize = (maximum - minimum) * 0.5;
	const glm::dvec2 start(capsule.pointA);
	const glm::dvec2 end(capsule.pointB);
	const glm::dvec2 segment = end - start;

	std::array<glm::dvec2, 7> axes{
		glm::dvec2(1.0, 0.0),
		glm::dvec2(0.0, 1.0),
		glm::dvec2(-segment.y, segment.x)
	};

	const std::array<glm::dvec2, 4> corners{
		minimum, glm::dvec2(maximum.x, minimum.y),
		maximum, glm::dvec2(minimum.x, maximum.y)
	};

	for (size_t index = 0; index < corners.size(); ++index)
	{
		const glm::dvec3 closest = ClosestPointOnCapsuleSegment(capsule, glm::dvec3(corners[index], 0.0));
		axes[index + 3] = glm::dvec2(closest) - corners[index];
	}

	normal = glm::dvec2(0.0);
	depth = std::numeric_limits<double>::max();

	// 박스 중심을 기준으로 투영하여 큰 월드 좌표끼리의 연산을 줄입니다.
	const glm::dvec2 relativeStart = start - center;
	const glm::dvec2 relativeEnd = end - center;

	for (const glm::dvec2& candidate : axes)
	{
		const double length = glm::length(candidate);
		if (length == 0.0)
			continue;

		const glm::dvec2 axis = candidate / length;
		const double first = glm::dot(relativeStart, axis);
		const double second = glm::dot(relativeEnd, axis);
		const double boxRadius = glm::dot(halfSize, glm::abs(axis));

		const double moveNegative = (glm::max(first, second) + boxRadius) + capsule.radius;
		const double movePositive = (boxRadius - glm::min(first, second)) + capsule.radius;

		if (moveNegative < 0.0 || movePositive < 0.0)
			return false;

		const double candidateDepth = glm::min(moveNegative, movePositive);
		if (candidateDepth < depth)
		{
			depth = candidateDepth;
			normal = moveNegative <= movePositive ? -axis : axis;
		}
	}

	return true;
}

static bool TryGetTransformedRectCircleContact(const Rect& rect, const mat3& transform, const Circle& circle,
	glm::dvec2& normal, double& depth, glm::dvec2& point)
{
	if (!circle.IsValid())
		return false;

	std::array<glm::dvec2, 4> corners{};
	if (!TryBuildCollisionRectCorners(rect, transform, corners))
		return false;

	const glm::dvec2 horizontal = corners[1] - corners[0];
	const glm::dvec2 vertical = corners[3] - corners[0];
	const bool counterClockwise = horizontal.x * vertical.y - horizontal.y * vertical.x > 0.0;

	const glm::dvec2 center(circle.center);
	double closestSquared = std::numeric_limits<double>::max();
	glm::dvec2 closestOffset(0.0);
	glm::dvec2 boundaryNormal(0.0);
	bool inside = true;

	for (size_t index = 0; index < corners.size(); ++index)
	{
		const glm::dvec2 edge = corners[(index + 1) % corners.size()] - corners[index];
		const double lengthSquared = glm::dot(edge, edge);
		if (lengthSquared == 0.0)
			continue;

		const glm::dvec2 relativeStart = corners[index] - center;
		const double side = edge.y * relativeStart.x - edge.x * relativeStart.y;
		if (counterClockwise ? side < 0.0 : side > 0.0)
			inside = false;

		const double parameter = glm::clamp(-glm::dot(relativeStart, edge) / lengthSquared, 0.0, 1.0);
		const glm::dvec2 offset = relativeStart + edge * parameter;
		const double squared = glm::dot(offset, offset);

		if (squared < closestSquared)
		{
			closestSquared = squared;
			closestOffset = offset;

			// 원 중심이 경계 위에 있어 거리 방향을 사용할 수 없는 경우의 분리 방향입니다.
			const glm::dvec2 inward = counterClockwise
				? glm::dvec2(-edge.y, edge.x)
				: glm::dvec2(edge.y, -edge.x);

			boundaryNormal = inward / std::sqrt(lengthSquared);
		}
	}

	const double radius = circle.radius;
	if (!inside && closestSquared > radius * radius)
		return false;

	const double distance = std::sqrt(closestSquared);
	if (distance > 0.0)
		normal = (inside ? -closestOffset : closestOffset) / distance;
	else
		normal = boundaryNormal;

	depth = inside ? radius + distance : glm::max(radius - distance, 0.0);
	point = center + closestOffset;
	return true;
}

static bool TryGetCapsulePairContact2D(const Capsule& a, const Capsule& b, glm::dvec2& normal, double& depth)
{
	if (!a.IsValid() || !b.IsValid() ||
		a.pointA.z != 0.f || a.pointB.z != 0.f ||
		b.pointA.z != 0.f || b.pointB.z != 0.f)
		return false;

	const glm::dvec2 origin(a.pointA);
	const glm::dvec2 directionA = glm::dvec2(a.pointB) - origin;
	const glm::dvec2 directionB = glm::dvec2(b.pointB) - glm::dvec2(b.pointA);
	const glm::dvec2 relativeBStart = glm::dvec2(b.pointA) - origin;
	const glm::dvec2 relativeBEnd = glm::dvec2(b.pointB) - origin;
	const double radiusSum = static_cast<double>(a.radius) + b.radius;

	std::array<glm::dvec2, 8> axes{
		glm::dvec2(1.0, 0.0),
		glm::dvec2(0.0, 1.0),
		glm::dvec2(-directionA.y, directionA.x),
		glm::dvec2(-directionB.y, directionB.x)
	};

	const std::array<glm::dvec3, 2> endpointsA{
		glm::dvec3(a.pointA), glm::dvec3(a.pointB)
	};
	const std::array<glm::dvec3, 2> endpointsB{
		glm::dvec3(b.pointA), glm::dvec3(b.pointB)
	};

	for (size_t index = 0; index < endpointsA.size(); ++index)
	{
		axes[index + 4] = glm::dvec2(endpointsA[index] - ClosestPointOnCapsuleSegment(b, endpointsA[index]));
		axes[index + 6] = glm::dvec2(ClosestPointOnCapsuleSegment(a, endpointsB[index]) - endpointsB[index]);
	}

	normal = glm::dvec2(0.0);
	depth = std::numeric_limits<double>::max();

	for (const glm::dvec2& candidate : axes)
	{
		const double length = glm::length(candidate);
		if (length == 0.0)
			continue;

		const glm::dvec2 axis = candidate / length;
		const double aEnd = glm::dot(directionA, axis);
		const double bStart = glm::dot(relativeBStart, axis);
		const double bEnd = glm::dot(relativeBEnd, axis);

		const double moveNegative = (glm::max(0.0, aEnd) - glm::min(bStart, bEnd)) + radiusSum;
		const double movePositive = (glm::max(bStart, bEnd) - glm::min(0.0, aEnd)) + radiusSum;

		if (moveNegative < 0.0 || movePositive < 0.0)
			return false;

		const double candidateDepth = glm::min(moveNegative, movePositive);
		if (candidateDepth < depth)
		{
			depth = candidateDepth;
			normal = moveNegative <= movePositive ? -axis : axis;
		}
	}

	return true;
}

static bool TryGetCapsuleTransformedRectContact(const Capsule& capsule, const Rect& rect, const mat3& transform,
	glm::dvec2& normal, double& depth, glm::dvec2& point)
{
	if (!capsule.IsValid() || capsule.pointA.z != 0.f || capsule.pointB.z != 0.f)
		return false;

	std::array<glm::dvec2, 4> corners{};
	if (!TryBuildCollisionRectCorners(rect, transform, corners))
		return false;

	const glm::dvec2 start(capsule.pointA);
	const glm::dvec2 end(capsule.pointB);
	const glm::dvec2 segment = end - start;
	const glm::dvec2 horizontal = corners[1] - corners[0];
	const glm::dvec2 vertical = corners[3] - corners[0];

	std::array<glm::dvec2, 7> axes{
		glm::dvec2(vertical.y, -vertical.x),
		glm::dvec2(-horizontal.y, horizontal.x),
		glm::dvec2(-segment.y, segment.x)
	};

	for (size_t index = 0; index < corners.size(); ++index)
	{
		const glm::dvec3 closest = ClosestPointOnCapsuleSegment(capsule, glm::dvec3(corners[index], 0.0));
		axes[index + 3] = glm::dvec2(closest) - corners[index];
	}

	normal = glm::dvec2(0.0);
	depth = std::numeric_limits<double>::max();

	for (const glm::dvec2& candidate : axes)
	{
		const double length = glm::length(candidate);
		if (length == 0.0)
			continue;

		const glm::dvec2 axis = candidate / length;
		const double capsuleEnd = glm::dot(segment, axis);
		double minimumBox = std::numeric_limits<double>::max();
		double maximumBox = std::numeric_limits<double>::lowest();

		for (const glm::dvec2& corner : corners)
		{
			const double projection = glm::dot(corner - start, axis);
			minimumBox = glm::min(minimumBox, projection);
			maximumBox = glm::max(maximumBox, projection);
		}

		const double moveNegative = (glm::max(0.0, capsuleEnd) - minimumBox) + capsule.radius;
		const double movePositive = (maximumBox - glm::min(0.0, capsuleEnd)) + capsule.radius;
		if (moveNegative < 0.0 || movePositive < 0.0)
			return false;

		const double candidateDepth = glm::min(moveNegative, movePositive);
		if (candidateDepth < depth)
		{
			depth = candidateDepth;
			normal = moveNegative <= movePositive ? -axis : axis;
		}
	}

	const double alignment = glm::dot(segment, normal);
	glm::dvec2 corePoint;

	if (alignment > 0.0)
		corePoint = start;
	else if (alignment < 0.0)
		corePoint = end;
	else
	{
		const glm::dvec2 boxCenter = (corners[0] + corners[2]) * 0.5;
		corePoint = glm::dvec2(ClosestPointOnCapsuleSegment(capsule, glm::dvec3(boxCenter, 0.0)));
	}

	point = corePoint - normal * static_cast<double>(capsule.radius);
	return true;
}



#pragma endregion

#pragma region Raycasts
bool Collision::Raycast(const Ray& ray, const Rect& rect, HitResult& outHitResult, f32 maxDistance)
{
	outHitResult.Reset();

	glm::dvec3 origin, direction;
	if (!TryPrepareCollisionRay(ray, maxDistance, origin, direction))
		return false;

	if (!std::isfinite(rect.left) || !std::isfinite(rect.top) ||
		!std::isfinite(rect.width) || !std::isfinite(rect.height) ||
		!std::isfinite(rect.Right()) || !std::isfinite(rect.Bottom()) ||
		rect.width < 0.f || rect.height < 0.f)
		return false;

	if (direction.z == 0.0)
		return false;

	const double distance = -origin.z / direction.z;
	if (!std::isfinite(distance) || distance < 0.0 || distance > maxDistance)
		return false;

	glm::dvec3 point = origin + direction * distance;
	if (point.x < rect.left || point.x > rect.Right() ||
		point.y < rect.top || point.y > rect.Bottom())
		return false;

	point.z = 0.0;
	const glm::dvec3 normal(0.0, 0.0, direction.z > 0.0 ? -1.0 : 1.0);
	return TryCommitCollisionRayHit(point, normal, distance, outHitResult);
}

bool Collision::Raycast(const Ray& ray, const BoundingBox& aabb, HitResult& outHitResult, f32 maxDistance)
{
	return Raycast(ray, aabb, mat4(1.0f), outHitResult, maxDistance);
}
bool Collision::Raycast(const Ray& ray, const BoundingBox& aabb, const mat4& worldMatrix, HitResult& outHitResult, f32 maxDistance)
{
	outHitResult.Reset();
	if (!IsFinite(ray.origin) || !IsFinite(ray.direction) || !IsFinite(aabb.min) || !IsFinite(aabb.max) ||
		!std::isfinite(maxDistance) || maxDistance < 0.0f || glm::any(glm::greaterThan(aabb.min, aabb.max)))
		return false;
	const f32 length = glm::length(ray.direction);
	const f32 determinant = glm::determinant(worldMatrix);
	if (!std::isfinite(length) || length <= 0.0f || !std::isfinite(determinant) || determinant == 0.0f)
		return false;
	const vec3 worldDir = ray.direction / length;
	const mat4 invWorld = glm::inverse(worldMatrix);
	const vec3 origin = vec3(invWorld * vec4(ray.origin, 1.0f));
	const vec3 direction = vec3(invWorld * vec4(worldDir, 0.0f));
	if (!IsFinite(origin) || !IsFinite(direction))
		return false;
	f32 enter = 0.0f;
	f32 exit = maxDistance;
	int hitAxis = -1;
	f32 hitSign = 0.0f;
	for (int axis = 0; axis < 3; ++axis)
	{
		if (direction[axis] == 0.0f)
		{
			if (origin[axis] < aabb.min[axis] || origin[axis] > aabb.max[axis]) return false;
			continue;
		}
		const f32 inv = 1.0f / direction[axis];
		f32 first = (aabb.min[axis] - origin[axis]) * inv;
		f32 second = (aabb.max[axis] - origin[axis]) * inv;
		f32 nearSign = -1.0f;
		if (first > second)
		{
			std::swap(first, second);
			nearSign = 1.0f;
		}
		if (first > enter)
		{
			enter = first;
			hitAxis = axis;
			hitSign = nearSign;
		}
		exit = glm::min(exit, second);
		if (enter > exit) return false;
	}
	outHitResult.hasHit = true;
	outHitResult.distance = enter;
	outHitResult.point = ray.origin + worldDir * enter;
	if (hitAxis >= 0)
	{
		vec3 localNormal(0.0f);
		localNormal[hitAxis] = hitSign;
		const vec3 worldNormal = vec3(glm::transpose(invWorld) * vec4(localNormal, 0.0f));
		const f32 normalLength = glm::length(worldNormal);
		outHitResult.normal = (std::isfinite(normalLength) && normalLength > 0.0f) ? (worldNormal / normalLength) : localNormal;
	}
	else
	{
		outHitResult.normal = -worldDir;
	}
	return true;
}

bool Collision::Raycast(const Ray& ray, const vec3& v0, const vec3& v1, const vec3& v2, HitResult& outHitResult, f32 maxDistance)
{
	outHitResult.Reset();

	glm::dvec3 origin, direction;
	if (!TryPrepareCollisionRay(ray, maxDistance, origin, direction) ||
		!IsFinite(v0) || !IsFinite(v1) || !IsFinite(v2))
		return false;

	double closestDistance = maxDistance;
	return TryUpdateClosestTriangleHit(origin, direction, glm::dvec3(v0),
		glm::dvec3(v1), glm::dvec3(v2), closestDistance, outHitResult);
}

bool Collision::Raycast(const Ray& ray, const Mesh& mesh, const mat4& worldMatrix, HitResult& outHitResult, f32 maxDistance)
{
	outHitResult.Reset();
	if (mesh.GetTopology() != ETopology::TriangleList ||
		!IsFinite(ray.origin) || !IsFinite(ray.direction) ||
		!std::isfinite(maxDistance) || maxDistance < 0.0f)
		return false;

	const uint32 vertexCount = mesh.GetStreamCount(EMeshStream::Position);
	const uint32 vertexStride = mesh.GetStreamStride(EMeshStream::Position);
	const auto& rawPositions = mesh.GetRawStreamData(EMeshStream::Position);
	if (vertexCount < 3 || vertexStride < sizeof(vec3) ||
		rawPositions.size() < static_cast<uint64>(vertexCount) * vertexStride)
		return false;

	const f32 worldDirLength = glm::length(ray.direction);
	const f32 determinant = glm::determinant(worldMatrix);
	if (!std::isfinite(worldDirLength) || worldDirLength <= 0.0f ||
		!std::isfinite(determinant) || determinant == 0.0f)
		return false;

	const vec3 worldDir = ray.direction / worldDirLength;
	const mat4 invWorld = glm::inverse(worldMatrix);
	const vec3 localOrigin = vec3(invWorld * vec4(ray.origin, 1.0f));
	const vec3 localDirVector = vec3(invWorld * vec4(worldDir, 0.0f));
	const f32 localDirLength = glm::length(localDirVector);
	if (!IsFinite(localOrigin) || !IsFinite(localDirVector) ||
		!std::isfinite(localDirLength) || localDirLength <= 0.0f)
		return false;

	const auto& bvh = mesh.GetRaycastBVH();
	if (bvh.Nodes.empty()) return false;

	const Ray localRay(localOrigin, localDirVector / localDirLength);
	f32 closestLocalDistance = maxDistance * localDirLength;
	if (!std::isfinite(closestLocalDistance)) closestLocalDistance = FLT_MAX;

	const auto readPosition = [&](uint32 index) -> vec3
		{
			vec3 position{};
			std::memcpy(&position, rawPositions.data() + static_cast<sizet>(index) * vertexStride, sizeof(vec3));
			return position;
		};

	// 노드 검사만 보수적으로 넓힙니다. 실제 삼각형 검사는 정확한 거리 제한을 사용합니다.
	const double tolerance = 8.0 * std::numeric_limits<f32>::epsilon();
	const auto distanceLimit = [&]() -> double
		{
			const double distance = closestLocalDistance;
			return distance + tolerance * std::max(1.0, distance);
		};
	const auto intersectNode = [&](uint32 index, double& entry) -> bool
		{
			const auto& node = bvh.Nodes[index];
			double enter = 0.0;
			double exit = distanceLimit();
			for (int axis = 0; axis < 3; ++axis)
			{
				const double origin = localRay.origin[axis];
				const double direction = localRay.direction[axis];
				const double lower = node.BoundsMin[axis];
				const double upper = node.BoundsMax[axis];
				const double padding = tolerance * std::max({ 1.0, std::abs(lower), std::abs(upper) });
				if (direction == 0.0)
				{
					if (origin < lower - padding || origin > upper + padding) return false;
					continue;
				}

				double first = (lower - padding - origin) / direction;
				double second = (upper + padding - origin) / direction;
				if (first > second) std::swap(first, second);
				enter = std::max(enter, first);
				exit = std::min(exit, second);
				if (enter > exit) return false;
			}
			entry = enter;
			return true;
		};

	bool hasHit = false;
	uint32 bestSourceIndex = 0;
	HitResult bestLocalHit{};
	const auto visit = [&](auto&& self, uint32 index, double entry) -> void
		{
			if (entry > distanceLimit()) return;
			const auto& node = bvh.Nodes[index];
			if (node.Count > 0)
			{
				for (uint32 i = node.First; i < node.First + node.Count; ++i)
				{
					const auto& triangle = bvh.Triangles[i];
					HitResult hit{};
					if (!Raycast(localRay, readPosition(triangle.Indices[0]), readPosition(triangle.Indices[1]),
						readPosition(triangle.Indices[2]), hit, closestLocalDistance))
						continue;

					if (!hasHit || hit.distance < closestLocalDistance ||
						(hit.distance == closestLocalDistance && triangle.SourceIndex > bestSourceIndex))
					{
						hasHit = true;
						closestLocalDistance = hit.distance;
						bestSourceIndex = triangle.SourceIndex;
						bestLocalHit = hit;
					}
				}
				return;
			}

			uint32 left = node.Left, right = node.Right;
			double leftEntry = 0.0, rightEntry = 0.0;
			const bool hasLeft = intersectNode(left, leftEntry);
			const bool hasRight = intersectNode(right, rightEntry);
			if (hasLeft && hasRight && rightEntry < leftEntry)
			{
				std::swap(left, right);
				std::swap(leftEntry, rightEntry);
			}
			if (hasLeft) self(self, left, leftEntry);
			if (hasRight) self(self, right, rightEntry);
		};

	double rootEntry = 0.0;
	if (!intersectNode(0, rootEntry)) return false;
	visit(visit, 0, rootEntry);
	if (!hasHit) return false;

	const f32 worldDistance = bestLocalHit.distance / localDirLength;
	if (!std::isfinite(worldDistance) || worldDistance < 0.0f || worldDistance > maxDistance)
		return false;

	outHitResult.hasHit = true;
	outHitResult.distance = worldDistance;
	outHitResult.point = ray.origin + worldDir * worldDistance;
	outHitResult.barycentric = bestLocalHit.barycentric;
	const f32 sign = determinant < 0.0f ? -1.0f : 1.0f;
	const vec3 worldNormal = vec3(glm::transpose(invWorld) * vec4(bestLocalHit.normal * sign, 0.0f));
	const f32 normalLength = glm::length(worldNormal);
	outHitResult.normal = std::isfinite(normalLength) && normalLength > 0.0f ?
		worldNormal / normalLength : bestLocalHit.normal;
	return true;
}

bool Collision::Raycast(const Ray& ray, const BoundingSphere& sphere, HitResult& outHitResult, f32 maxDistance)
{
	outHitResult.Reset();

	glm::dvec3 origin, direction;
	if (!TryPrepareCollisionRay(ray, maxDistance, origin, direction) || !sphere.IsValid())
		return false;

	const glm::dvec3 center(sphere.center);
	const glm::dvec3 toCenter = center - origin;
	const double radius = sphere.radius;
	const double radiusSquared = radius * radius;

	// 내부·표면 시작은 Box 조회와 동일하게 거리 0으로 처리합니다.
	if (glm::dot(toCenter, toCenter) <= radiusSquared)
		return TryCommitCollisionRayHit(origin, -direction, 0.0, outHitResult);

	const double projection = glm::dot(toCenter, direction);
	if (projection < 0.0)
		return false;

	const glm::dvec3 perpendicular = toCenter - direction * projection;
	const double perpendicularSquared = glm::dot(perpendicular, perpendicular);
	if (perpendicularSquared > radiusSquared)
		return false;

	const double halfChord = std::sqrt(glm::max(radiusSquared - perpendicularSquared, 0.0));
	const double distance = projection - halfChord;
	if (!std::isfinite(distance) || distance < 0.0 || distance > maxDistance)
		return false;

	const glm::dvec3 point = origin + direction * distance;
	const glm::dvec3 surfaceNormal = point - center;
	const double normalLength = glm::length(surfaceNormal);
	const glm::dvec3 normal = normalLength > 0.0 ? surfaceNormal / normalLength : -direction;

	return TryCommitCollisionRayHit(point, normal, distance, outHitResult);
}

bool Collision::Raycast(const Ray& ray, const TerrainComponent& terrain, const mat4& worldMatrix, HitResult& outHitResult, f32 maxDistance)
{
	outHitResult.Reset();

	glm::dvec3 worldOrigin, worldDirection;
	if (!TryPrepareCollisionRay(ray, maxDistance, worldOrigin, worldDirection))
		return false;

	if (terrain.Resolution == 0 ||
		!std::isfinite(terrain.Width) || terrain.Width <= 0.f ||
		!std::isfinite(terrain.Length) || terrain.Length <= 0.f ||
		!std::isfinite(terrain.MaxHeight) || terrain.MaxHeight < 0.f)
		return false;

	const uint64 axisCount = static_cast<uint64>(terrain.Resolution) + 1;
	const uint64 sampleLimit = (std::numeric_limits<std::size_t>::max)() / sizeof(f32);
	if (axisCount > sampleLimit / axisCount)
		return false;

	const std::size_t sampleCount = static_cast<std::size_t>(axisCount * axisCount);
	if (terrain.HeightData.size() != sampleCount)
		return false;

	// 현재 높이맵은 0~1의 정규화된 높이를 저장합니다.
	for (const f32 height : terrain.HeightData)
	{
		if (!std::isfinite(height) || height < 0.f || height > 1.f)
			return false;
	}

	for (int column = 0; column < 4; ++column)
	{
		for (int row = 0; row < 4; ++row)
		{
			if (!std::isfinite(worldMatrix[column][row]))
				return false;
		}
	}

	if (worldMatrix[0][3] != 0.f || worldMatrix[1][3] != 0.f ||
		worldMatrix[2][3] != 0.f || worldMatrix[3][3] != 1.f)
		return false;

	const glm::dmat4 world(worldMatrix);
	const double determinant = glm::determinant(glm::dmat3(world));
	if (!std::isfinite(determinant) || determinant == 0.0)
		return false;

	const glm::dmat4 inverse = glm::inverse(world);
	for (int column = 0; column < 4; ++column)
	{
		for (int row = 0; row < 4; ++row)
		{
			if (!std::isfinite(inverse[column][row]))
				return false;
		}
	}

	const glm::dvec3 localOrigin(inverse * glm::dvec4(worldOrigin, 1.0));
	const glm::dvec3 localDirection(inverse * glm::dvec4(worldDirection, 0.0));

	for (int axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(localOrigin[axis]) || !std::isfinite(localDirection[axis]))
			return false;
	}

	if (localDirection.x == 0.0 && localDirection.y == 0.0 && localDirection.z == 0.0)
		return false;

	const f32 stepX = terrain.Width / static_cast<f32>(terrain.Resolution);
	const f32 stepZ = terrain.Length / static_cast<f32>(terrain.Resolution);
	const f32 startX = -terrain.Width * 0.5f;
	const f32 startZ = -terrain.Length * 0.5f;
	const f32 endX = startX + static_cast<f32>(terrain.Resolution) * stepX;
	const f32 endZ = startZ + static_cast<f32>(terrain.Resolution) * stepZ;

	if (!std::isfinite(stepX) || stepX <= 0.f ||
		!std::isfinite(stepZ) || stepZ <= 0.f ||
		!std::isfinite(endX) || !std::isfinite(endZ))
		return false;

	const glm::dvec3 boxMin(startX, 0.0, startZ);
	const glm::dvec3 boxMax(endX, terrain.MaxHeight, endZ);

	double tNear = 0.0;
	double tFar = maxDistance;

	for (int axis = 0; axis < 3; ++axis)
	{
		if (localDirection[axis] == 0.0)
		{
			if (localOrigin[axis] < boxMin[axis] || localOrigin[axis] > boxMax[axis])
				return false;

			continue;
		}

		double first = (boxMin[axis] - localOrigin[axis]) / localDirection[axis];
		double second = (boxMax[axis] - localOrigin[axis]) / localDirection[axis];
		if (first > second)
			std::swap(first, second);

		tNear = std::max(tNear, first);
		tFar = std::min(tFar, second);
		if (tNear > tFar)
			return false;
	}

	const std::size_t stride = static_cast<std::size_t>(axisCount);
	const std::size_t resolution = terrain.Resolution;

	const auto readWorldPosition = [&](std::size_t x, std::size_t z) -> glm::dvec3
		{
			const vec3 localPosition(
				startX + static_cast<f32>(x) * stepX,
				terrain.HeightData[z * stride + x] * terrain.MaxHeight,
				startZ + static_cast<f32>(z) * stepZ);

			return glm::dvec3(world * glm::dvec4(glm::dvec3(localPosition), 1.0));
		};

	double closestDistance = maxDistance;
	HitResult closestHit{};
	closestHit.Reset();

	std::vector<double> xCoordinates(stride);
	std::vector<double> zCoordinates(stride);

	for (std::size_t index = 0; index < stride; ++index)
	{
		xCoordinates[index] = startX + static_cast<f32>(index) * stepX;
		zCoordinates[index] = startZ + static_cast<f32>(index) * stepZ;
	}

	// 후보 범위만 보수적으로 넓힙니다. 실제 삼각형 검사는 기존 거리 제한을 사용합니다.
	const double tolerance = 64.0 * std::numeric_limits<double>::epsilon();

	for (std::size_t x = 0; x < resolution; ++x)
	{
		double enter = tNear;
		double exit = std::min(tFar, closestDistance);

		const double lowerX = xCoordinates[x];
		const double upperX = xCoordinates[x + 1];
		const double paddingX = tolerance *
			std::max({ 1.0, std::abs(localOrigin.x), std::abs(lowerX), std::abs(upperX) });

		if (localDirection.x == 0.0)
		{
			if (localOrigin.x < lowerX - paddingX || localOrigin.x > upperX + paddingX)
				continue;
		}
		else
		{
			double first = (lowerX - paddingX - localOrigin.x) / localDirection.x;
			double second = (upperX + paddingX - localOrigin.x) / localDirection.x;
			if (first > second)
				std::swap(first, second);

			enter = std::max(enter, first);
			exit = std::min(exit, second);
		}

		if (enter > exit)
			continue;

		const double firstZ = localOrigin.z + localDirection.z * enter;
		const double lastZ = localOrigin.z + localDirection.z * exit;
		const double paddingZ = tolerance * std::max({
			1.0, std::abs(localOrigin.z), std::abs(firstZ), std::abs(lastZ),
			std::abs(localDirection.z * enter), std::abs(localDirection.z * exit) });

		const double lowerZ = glm::clamp(
			std::min(firstZ, lastZ) - paddingZ, zCoordinates.front(), zCoordinates.back());
		const double upperZ = glm::clamp(
			std::max(firstZ, lastZ) + paddingZ, zCoordinates.front(), zCoordinates.back());

		// 경계에 걸리면 양쪽 셀을 포함합니다.
		const auto firstBoundary = std::lower_bound(
			zCoordinates.begin() + 1, zCoordinates.end(), lowerZ);
		const auto afterLastStart = std::upper_bound(
			zCoordinates.begin(), zCoordinates.end() - 1, upperZ);

		const std::size_t firstCell = static_cast<std::size_t>(firstBoundary - zCoordinates.begin()) - 1;
		const std::size_t endCell = static_cast<std::size_t>(afterLastStart - zCoordinates.begin());

		for (std::size_t z = firstCell; z < endCell; ++z)
		{
			const glm::dvec3 v0 = readWorldPosition(x, z);
			const glm::dvec3 v1 = readWorldPosition(x + 1, z);
			const glm::dvec3 v2 = readWorldPosition(x, z + 1);
			const glm::dvec3 v3 = readWorldPosition(x + 1, z + 1);

			TryUpdateClosestTriangleHit(worldOrigin, worldDirection,
				v0, v2, v1, closestDistance, closestHit);

			TryUpdateClosestTriangleHit(worldOrigin, worldDirection,
				v1, v2, v3, closestDistance, closestHit);
		}
	}

	if (!closestHit.hasHit)
		return false;

	outHitResult = closestHit;
	return true;
}
#pragma endregion

#pragma region Contains
bool Collision::Contains(const Rect& rect, const vec2& point)
{
	return rect.Contains(point);
}

bool Collision::Contains(const BoundingBox& box, const vec3& point)
{
	return box.Contains(point);
}

bool Collision::Contains(const BoundingBox& box, const mat4& worldMatrix, const vec3& point)
{
	mat4 invWorld = glm::inverse(worldMatrix);
	vec3 localPoint = vec3(invWorld * vec4(point, 1.0f));
	return Contains(box, localPoint);
}

bool Collision::Contains(const BoundingSphere& sphere, const vec3& point)
{
	return sphere.Contains(point);
}
#pragma endregion

#pragma region Intersects
bool Collision::Intersects(const Rect& a, const Rect& b)
{
	return a.Intersects(b);
}

bool Collision::Intersects(const Rect& a, const Rect& b, HitResult& outHitResult)
{
	outHitResult.Reset();

	if (!IsValidCollisionRect(a) || !IsValidCollisionRect(b) || !Intersects(a, b))
		return false;

	const f32 moveLeft = a.Right() - b.left;
	const f32 moveRight = b.Right() - a.left;
	const f32 moveDown = a.Bottom() - b.top;
	const f32 moveUp = b.Bottom() - a.top;

	const f32 depthX = glm::min(moveLeft, moveRight);
	const f32 depthY = glm::min(moveDown, moveUp);
	if (!std::isfinite(depthX) || !std::isfinite(depthY))
		return false;

	const vec2 overlapMin(glm::max(a.left, b.left), glm::max(a.top, b.top));
	const vec2 overlapMax(glm::min(a.Right(), b.Right()), glm::min(a.Bottom(), b.Bottom()));
	const vec2 halfSize = (overlapMax - overlapMin) * 0.5f;

	outHitResult.point = vec3(overlapMin + halfSize, 0.f);
	outHitResult.extents = vec3(halfSize, 0.f);

	if (depthX < depthY)
	{
		outHitResult.denetrationDepth = depthX;
		outHitResult.normal = moveLeft < moveRight ? vec3(-1.f, 0.f, 0.f) : vec3(1.f, 0.f, 0.f);
	}
	else
	{
		outHitResult.denetrationDepth = depthY;
		outHitResult.normal = moveDown < moveUp ? vec3(0.f, -1.f, 0.f) : vec3(0.f, 1.f, 0.f);
	}

	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const Rect& a, const mat3& transformA, const Rect& b, const mat3& transformB)
{
	glm::dvec2 normal, point;
	double depth;
	return TryGetTransformedRectContact(a, transformA, b, transformB, normal, depth, point);
}

bool Collision::Intersects(const Rect& a, const mat3& transformA, const Rect& b, const mat3& transformB, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec2 normal, point;
	double depth;
	if (!TryGetTransformedRectContact(a, transformA, b, transformB, normal, depth, point))
		return false;

	const double limit = std::numeric_limits<f32>::max();
	if (!std::isfinite(depth) || depth > limit)
		return false;

	for (int axis = 0; axis < 2; ++axis)
	{
		if (!std::isfinite(point[axis]) || std::abs(point[axis]) > limit)
			return false;
	}

	outHitResult.point = vec3(vec2(point), 0.f);
	outHitResult.normal = vec3(vec2(normal), 0.f);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const Circle& a, const Circle& b)
{
	return Intersects(BoundingSphere(vec3(a.center, 0.f), a.radius),
		BoundingSphere(vec3(b.center, 0.f), b.radius));
}

bool Collision::Intersects(const Circle& a, const Circle& b, HitResult& outHitResult)
{
	return Intersects(BoundingSphere(vec3(a.center, 0.f), a.radius),
		BoundingSphere(vec3(b.center, 0.f), b.radius), outHitResult);
}

bool Collision::Intersects(const Rect& rect, const Circle& circle)
{
	if (!IsValidCollisionRect(rect) || !circle.IsValid())
		return false;

	const glm::dvec2 center(circle.center);
	const glm::dvec2 minimum(rect.left, rect.top);
	const glm::dvec2 maximum(rect.Right(), rect.Bottom());
	const glm::dvec2 closest = glm::clamp(center, minimum, maximum);
	const glm::dvec2 delta = closest - center;

	return glm::dot(delta, delta) <= static_cast<double>(circle.radius) * circle.radius;
}

bool Collision::Intersects(const Rect& rect, const Circle& circle, HitResult& outHitResult)
{
	outHitResult.Reset();
	if (!Intersects(rect, circle))
		return false;

	const glm::dvec2 center(circle.center);
	const glm::dvec2 minimum(rect.left, rect.top);
	const glm::dvec2 maximum(rect.Right(), rect.Bottom());
	glm::dvec2 point = glm::clamp(center, minimum, maximum);
	const glm::dvec2 delta = point - center;
	const double distance = glm::length(delta);

	glm::dvec2 normal(0.0);
	double depth = 0.0;

	if (distance > 0.0)
	{
		normal = delta / distance;
		depth = glm::max(static_cast<double>(circle.radius) - distance, 0.0);
	}
	else
	{
		double faceDistance = std::numeric_limits<double>::max();

		for (int axis = 0; axis < 2; ++axis)
		{
			const double toMin = center[axis] - minimum[axis];
			const double toMax = maximum[axis] - center[axis];
			const double candidateDistance = glm::min(toMin, toMax);

			if (candidateDistance < faceDistance)
			{
				faceDistance = candidateDistance;
				normal = glm::dvec2(0.0);
				normal[axis] = toMin <= toMax ? 1.0 : -1.0;
				point = center;
				point[axis] = toMin <= toMax ? minimum[axis] : maximum[axis];
			}
		}

		depth = faceDistance + static_cast<double>(circle.radius);
	}

	const vec3 resultPoint(vec2(point), 0.f);
	if (!std::isfinite(depth) || depth > std::numeric_limits<f32>::max() || !IsFinite(resultPoint))
		return false;

	outHitResult.point = resultPoint;
	outHitResult.normal = vec3(vec2(normal), 0.f);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const Circle& circle, const Rect& rect)
{
	return Intersects(rect, circle);
}

bool Collision::Intersects(const Circle& circle, const Rect& rect, HitResult& outHitResult)
{
	if (!Intersects(rect, circle, outHitResult))
		return false;

	outHitResult.normal = -outHitResult.normal;
	return true;
}

bool Collision::Intersects(const Rect& rect, const mat3& transform, const Circle& circle)
{
	glm::dvec2 normal, point;
	double depth;
	return TryGetTransformedRectCircleContact(rect, transform, circle, normal, depth, point);
}

bool Collision::Intersects(const Rect& rect, const mat3& transform, const Circle& circle, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec2 normal, point;
	double depth;
	if (!TryGetTransformedRectCircleContact(rect, transform, circle, normal, depth, point))
		return false;

	const double limit = std::numeric_limits<f32>::max();
	if (!std::isfinite(depth) || depth > limit)
		return false;

	for (int axis = 0; axis < 2; ++axis)
	{
		if (!std::isfinite(point[axis]) || std::abs(point[axis]) > limit)
			return false;
	}

	outHitResult.point = vec3(vec2(point), 0.f);
	outHitResult.normal = vec3(vec2(normal), 0.f);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const Circle& circle, const Rect& rect, const mat3& transform)
{
	return Intersects(rect, transform, circle);
}

bool Collision::Intersects(const Circle& circle, const Rect& rect, const mat3& transform, HitResult& outHitResult)
{
	if (!Intersects(rect, transform, circle, outHitResult))
		return false;

	outHitResult.normal = -outHitResult.normal;
	return true;
}

bool Collision::Intersects(const Capsule& capsule, const Rect& rect, const mat3& transform)
{
	glm::dvec2 normal, point;
	double depth;
	return TryGetCapsuleTransformedRectContact(capsule, rect, transform, normal, depth, point);
}

bool Collision::Intersects(const Capsule& capsule, const Rect& rect, const mat3& transform, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec2 normal, point;
	double depth;
	if (!TryGetCapsuleTransformedRectContact(capsule, rect, transform, normal, depth, point))
		return false;

	const double limit = std::numeric_limits<f32>::max();
	if (!std::isfinite(depth) || depth > limit)
		return false;

	for (int axis = 0; axis < 2; ++axis)
	{
		if (!std::isfinite(point[axis]) || std::abs(point[axis]) > limit)
			return false;
	}

	outHitResult.point = vec3(vec2(point), 0.f);
	outHitResult.normal = vec3(vec2(normal), 0.f);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const Rect& rect, const mat3& transform, const Capsule& capsule)
{
	return Intersects(capsule, rect, transform);
}

bool Collision::Intersects(const Rect& rect, const mat3& transform, const Capsule& capsule, HitResult& outHitResult)
{
	if (!Intersects(capsule, rect, transform, outHitResult))
		return false;

	outHitResult.normal = -outHitResult.normal;
	return true;
}

bool Collision::Intersects(const Capsule& capsule, const Circle& circle)
{
	if (!circle.IsValid() || capsule.pointA.z != 0.f || capsule.pointB.z != 0.f)
		return false;

	return Intersects(capsule, BoundingSphere(vec3(circle.center, 0.f), circle.radius));
}

bool Collision::Intersects(const Capsule& capsule, const Circle& circle, HitResult& outHitResult)
{
	outHitResult.Reset();
	if (!circle.IsValid() || capsule.pointA.z != 0.f || capsule.pointB.z != 0.f)
		return false;

	return Intersects(capsule, BoundingSphere(vec3(circle.center, 0.f), circle.radius), outHitResult);
}

bool Collision::Intersects(const Circle& circle, const Capsule& capsule)
{
	return Intersects(capsule, circle);
}

bool Collision::Intersects(const Circle& circle, const Capsule& capsule, HitResult& outHitResult)
{
	if (!Intersects(capsule, circle, outHitResult))
		return false;

	outHitResult.normal = -outHitResult.normal;
	return true;
}

bool Collision::Intersects(const Capsule& capsule, const Rect& rect)
{
	glm::dvec2 normal;
	double depth;
	return TryGetCapsuleRectContact(capsule, rect, normal, depth);
}

bool Collision::Intersects(const Capsule& capsule, const Rect& rect, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec2 normal;
	double depth;
	if (!TryGetCapsuleRectContact(capsule, rect, normal, depth))
		return false;

	const glm::dvec2 start(capsule.pointA);
	const glm::dvec2 end(capsule.pointB);
	const double alignment = glm::dot(end - start, normal);
	glm::dvec2 corePoint;

	if (alignment > 0.0)
		corePoint = start;
	else if (alignment < 0.0)
		corePoint = end;
	else
	{
		const glm::dvec3 center(
			(static_cast<double>(rect.left) + rect.Right()) * 0.5,
			(static_cast<double>(rect.top) + rect.Bottom()) * 0.5, 0.0);

		corePoint = glm::dvec2(ClosestPointOnCapsuleSegment(capsule, center));
	}

	// 캡슐 표면에서 박스를 향하는 대표 접촉점입니다.
	const glm::dvec2 point = corePoint - normal * static_cast<double>(capsule.radius);
	const double limit = std::numeric_limits<f32>::max();

	if (!std::isfinite(depth) || depth > limit)
		return false;

	for (int axis = 0; axis < 2; ++axis)
	{
		if (!std::isfinite(point[axis]) || std::abs(point[axis]) > limit)
			return false;
	}

	outHitResult.point = vec3(vec2(point), 0.f);
	outHitResult.normal = vec3(vec2(normal), 0.f);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const Rect& rect, const Capsule& capsule)
{
	return Intersects(capsule, rect);
}

bool Collision::Intersects(const Rect& rect, const Capsule& capsule, HitResult& outHitResult)
{
	if (!Intersects(capsule, rect, outHitResult))
		return false;

	outHitResult.normal = -outHitResult.normal;
	return true;
}

bool Collision::Intersects2D(const Capsule& a, const Capsule& b)
{
	glm::dvec2 normal;
	double depth;
	return TryGetCapsulePairContact2D(a, b, normal, depth);
}

bool Collision::Intersects2D(const Capsule& a, const Capsule& b, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec2 normal;
	double depth;
	if (!TryGetCapsulePairContact2D(a, b, normal, depth))
		return false;

	const glm::dvec2 start(a.pointA);
	const glm::dvec2 end(a.pointB);
	const double alignment = glm::dot(end - start, normal);
	glm::dvec2 corePoint;

	if (alignment > 0.0)
		corePoint = start;
	else if (alignment < 0.0)
		corePoint = end;
	else
	{
		const glm::dvec3 centerB = (glm::dvec3(b.pointA) + glm::dvec3(b.pointB)) * 0.5;
		corePoint = glm::dvec2(ClosestPointOnCapsuleSegment(a, centerB));
	}

	const glm::dvec2 point = corePoint - normal * static_cast<double>(a.radius);
	const double limit = std::numeric_limits<f32>::max();

	if (!std::isfinite(depth) || depth > limit)
		return false;

	for (int axis = 0; axis < 2; ++axis)
	{
		if (!std::isfinite(point[axis]) || std::abs(point[axis]) > limit)
			return false;
	}

	outHitResult.point = vec3(vec2(point), 0.f);
	outHitResult.normal = vec3(vec2(normal), 0.f);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const BoundingBox& a, const BoundingBox& b)
{
	if (!a.IsValid() || !b.IsValid())
		return false;

	return a.min.x <= b.max.x && a.max.x >= b.min.x &&
		a.min.y <= b.max.y && a.max.y >= b.min.y &&
		a.min.z <= b.max.z && a.max.z >= b.min.z;
}

bool Collision::Intersects(const BoundingBox& a, const BoundingBox& b, HitResult& outHitResult)
{
	outHitResult.Reset();
	if (!Intersects(a, b))
		return false;

	double depth = std::numeric_limits<double>::max();
	vec3 normal(0.f);

	for (int axis = 0; axis < 3; ++axis)
	{
		const double moveNegative = static_cast<double>(a.max[axis]) - b.min[axis];
		const double movePositive = static_cast<double>(b.max[axis]) - a.min[axis];
		const double axisDepth = glm::min(moveNegative, movePositive);

		if (axisDepth < depth)
		{
			depth = axisDepth;
			normal = vec3(0.f);
			normal[axis] = moveNegative <= movePositive ? -1.f : 1.f;
		}
	}

	if (!std::isfinite(depth) || depth > std::numeric_limits<f32>::max())
		return false;

	const glm::dvec3 overlapMin = glm::max(glm::dvec3(a.min), glm::dvec3(b.min));
	const glm::dvec3 overlapMax = glm::min(glm::dvec3(a.max), glm::dvec3(b.max));
	const glm::dvec3 halfSize = (overlapMax - overlapMin) * 0.5;

	outHitResult.point = vec3(overlapMin + halfSize);
	outHitResult.extents = vec3(halfSize);
	outHitResult.normal = normal;
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const BoundingSphere& a, const BoundingSphere& b)
{
	if (!a.IsValid() || !b.IsValid())
		return false;

	const glm::dvec3 delta = glm::dvec3(a.center) - glm::dvec3(b.center);
	const double radiusSum = static_cast<double>(a.radius) + b.radius;
	return glm::dot(delta, delta) <= radiusSum * radiusSum;
}

bool Collision::Intersects(const BoundingSphere& a, const BoundingSphere& b, HitResult& outHitResult)
{
	outHitResult.Reset();
	if (!Intersects(a, b))
		return false;

	const glm::dvec3 delta = glm::dvec3(a.center) - glm::dvec3(b.center);
	const double distance = glm::length(delta);
	const double radiusSum = static_cast<double>(a.radius) + b.radius;
	const double depth = glm::max(radiusSum - distance, 0.0);

	if (!std::isfinite(depth) || depth > std::numeric_limits<f32>::max())
		return false;

	const glm::dvec3 normal = distance > 0.0 ? delta / distance : glm::dvec3(1.0, 0.0, 0.0);
	const glm::dvec3 point = glm::dvec3(a.center) - normal * static_cast<double>(a.radius);
	const vec3 resultPoint(point);
	if (!IsFinite(resultPoint))
		return false;

	outHitResult.point = resultPoint;
	outHitResult.normal = vec3(normal);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const BoundingBox& box, const BoundingSphere& sphere)
{
	if (!box.IsValid() || !sphere.IsValid())
		return false;

	const glm::dvec3 center(sphere.center);
	const glm::dvec3 closest = glm::clamp(center, glm::dvec3(box.min), glm::dvec3(box.max));
	const glm::dvec3 delta = closest - center;
	return glm::dot(delta, delta) <= static_cast<double>(sphere.radius) * sphere.radius;
}

bool Collision::Intersects(const BoundingBox& box, const BoundingSphere& sphere, HitResult& outHitResult)
{
	outHitResult.Reset();
	if (!Intersects(box, sphere))
		return false;

	const glm::dvec3 center(sphere.center);
	glm::dvec3 point = glm::clamp(center, glm::dvec3(box.min), glm::dvec3(box.max));
	const glm::dvec3 delta = point - center;
	const double distance = glm::length(delta);

	glm::dvec3 normal(0.0);
	double depth;

	if (distance > 0.0)
	{
		normal = delta / distance;
		depth = glm::max(static_cast<double>(sphere.radius) - distance, 0.0);
	}
	else
	{
		// 구 중심이 박스 내부 또는 표면에 있으면 가장 가까운 면을 선택합니다.
		double faceDistance = std::numeric_limits<double>::max();

		for (int axis = 0; axis < 3; ++axis)
		{
			const double toMin = center[axis] - box.min[axis];
			const double toMax = static_cast<double>(box.max[axis]) - center[axis];
			const double candidateDistance = glm::min(toMin, toMax);

			if (candidateDistance < faceDistance)
			{
				faceDistance = candidateDistance;
				normal = glm::dvec3(0.0);
				normal[axis] = toMin <= toMax ? 1.0 : -1.0;
				point = center;
				point[axis] = toMin <= toMax ? box.min[axis] : box.max[axis];
			}
		}

		depth = faceDistance + static_cast<double>(sphere.radius);
	}

	if (!std::isfinite(depth) || depth > std::numeric_limits<f32>::max())
		return false;

	outHitResult.point = vec3(point);
	outHitResult.normal = vec3(normal);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const BoundingBox& a, const BoundingBox& b, const mat4& worldMatrixb)
{
	BoundingBox worldB;
	if (!a.IsValid() || !b.TryTransformToAABB(worldMatrixb, worldB))
		return false;

	return Intersects(a, worldB);
}

bool Collision::Intersects(const BoundingBox& a, const BoundingBox& b, const mat4& worldMatrixb, HitResult& outHitResult)
{
	outHitResult.Reset();

	BoundingBox worldB;
	if (!a.IsValid() || !b.TryTransformToAABB(worldMatrixb, worldB))
		return false;

	return Intersects(a, worldB, outHitResult);
}

bool Collision::Intersects(const BoundingBox& a, const mat4& worldMatrixa, const BoundingBox& b, const mat4& worldMatrixb)
{
	BoundingBox worldA, worldB;
	if (!a.TryTransformToAABB(worldMatrixa, worldA) ||
		!b.TryTransformToAABB(worldMatrixb, worldB))
		return false;

	return Intersects(worldA, worldB);
}

bool Collision::Intersects(const BoundingBox& a, const mat4& worldMatrixa, const BoundingBox& b, const mat4& worldMatrixb, HitResult& outHitResult)
{
	outHitResult.Reset();

	BoundingBox worldA, worldB;
	if (!a.TryTransformToAABB(worldMatrixa, worldA) ||
		!b.TryTransformToAABB(worldMatrixb, worldB))
		return false;

	return Intersects(worldA, worldB, outHitResult);
}
bool Collision::Intersects(const Capsule& capsule, const BoundingSphere& sphere)
{
	if (!capsule.IsValid() || !sphere.IsValid())
		return false;

	const glm::dvec3 center(sphere.center);
	const glm::dvec3 delta = ClosestPointOnCapsuleSegment(capsule, center) - center;
	const double radiusSum = static_cast<double>(capsule.radius) + sphere.radius;

	return glm::dot(delta, delta) <= radiusSum * radiusSum;
}

bool Collision::Intersects(const Capsule& capsule, const BoundingSphere& sphere, HitResult& outHitResult)
{
	outHitResult.Reset();
	if (!capsule.IsValid() || !sphere.IsValid())
		return false;

	const glm::dvec3 center(sphere.center);
	const glm::dvec3 closest = ClosestPointOnCapsuleSegment(capsule, center);
	const glm::dvec3 delta = closest - center;
	const double radiusSum = static_cast<double>(capsule.radius) + sphere.radius;
	const double distanceSquared = glm::dot(delta, delta);

	if (distanceSquared > radiusSum * radiusSum)
		return false;

	const double distance = std::sqrt(distanceSquared);
	const double depth = glm::max(radiusSum - distance, 0.0);
	glm::dvec3 normal;

	if (distance > 0.0)
	{
		normal = delta / distance;
	}
	else
	{
		// 구 중심이 중심선 위에 있으면 중심선에 수직인 방향을 선택합니다.
		const glm::dvec3 segment = glm::dvec3(capsule.pointB) - glm::dvec3(capsule.pointA);
		const glm::dvec3 perpendicular(-segment.y, segment.x, 0.0);
		const double perpendicularLength = glm::length(perpendicular);

		normal = perpendicularLength > 0.0
			? perpendicular / perpendicularLength
			: glm::dvec3(1.0, 0.0, 0.0);
	}

	const glm::dvec3 point = closest - normal * static_cast<double>(capsule.radius);
	const double limit = std::numeric_limits<f32>::max();

	if (!std::isfinite(depth) || depth > limit)
		return false;

	for (int axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(point[axis]) || std::abs(point[axis]) > limit)
			return false;
	}

	outHitResult.point = vec3(point);
	outHitResult.normal = vec3(normal);
	outHitResult.denetrationDepth = static_cast<f32>(depth);
	outHitResult.hasHit = true;
	return true;
}

bool Collision::Intersects(const BoundingSphere& sphere, const Capsule& capsule)
{
	return Intersects(capsule, sphere);
}

bool Collision::Intersects(const BoundingSphere& sphere, const Capsule& capsule, HitResult& outHitResult)
{
	if (!Intersects(capsule, sphere, outHitResult))
		return false;

	outHitResult.normal = -outHitResult.normal;
	return true;
}
#pragma endregion

#pragma region Sweep

// 첫 도형이 direction 방향으로 최대 distance만큼 평행 이동할 때의 최초 접촉을 검사합니다.
// 결과 distance는 월드 이동 거리이며, 초기 겹침·접촉은 0입니다.
// normal은 이동 도형의 분리 방향입니다. 초기 겹침·접촉에서는 -direction의 단위 방향입니다.
bool Collision::Sweep(const BoundingBox& box, const vec3& direction, f32 distance, const BoundingBox& obstacle, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec3 origin, unitDirection;
	if (!box.IsValid() || !obstacle.IsValid() ||
		!TryPrepareCollisionRay(Ray(vec3(0.f), direction), distance, origin, unitDirection))
		return false;

	// 이동량이 이 범위 안에 있으면 두 박스가 겹칩니다.
	const glm::dvec3 lower = glm::dvec3(obstacle.min) - glm::dvec3(box.max);
	const glm::dvec3 upper = glm::dvec3(obstacle.max) - glm::dvec3(box.min);

	double enter = 0.0;
	double exit = distance;
	glm::dvec3 normal = -unitDirection;

	for (int axis = 0; axis < 3; ++axis)
	{
		if (unitDirection[axis] == 0.0)
		{
			if (lower[axis] > 0.0 || upper[axis] < 0.0)
				return false;

			continue;
		}

		double first = lower[axis] / unitDirection[axis];
		double second = upper[axis] / unitDirection[axis];
		const double nearSign = unitDirection[axis] > 0.0 ? -1.0 : 1.0;

		if (first > second)
			std::swap(first, second);

		if (first > enter)
		{
			enter = first;
			normal = glm::dvec3(0.0);
			normal[axis] = nearSign;
		}

		exit = glm::min(exit, second);
		if (enter > exit)
			return false;
	}

	const glm::dvec3 center = (glm::dvec3(box.min) + glm::dvec3(box.max)) * 0.5;
	const glm::dvec3 movedCenter = center + unitDirection * enter;
	const glm::dvec3 point = glm::clamp(movedCenter, glm::dvec3(obstacle.min), glm::dvec3(obstacle.max));

	return TryCommitCollisionRayHit(point, normal, enter, outHitResult);
}

bool Collision::Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec3 origin, unitDirection;
	if (!sphere.IsValid() || !obstacle.IsValid() ||
		!TryPrepareCollisionRay(Ray(sphere.center, direction), distance, origin, unitDirection))
		return false;

	const glm::dvec3 obstacleCenter(obstacle.center);
	const glm::dvec3 toCenter = obstacleCenter - origin;
	const double radiusSum = static_cast<double>(sphere.radius) + obstacle.radius;
	const double radiusSquared = radiusSum * radiusSum;

	if (glm::dot(toCenter, toCenter) <= radiusSquared)
		return TryCommitCollisionRayHit(origin, -unitDirection, 0.0, outHitResult);

	const double projection = glm::dot(toCenter, unitDirection);
	if (projection < 0.0)
		return false;

	const glm::dvec3 perpendicular = toCenter - unitDirection * projection;
	const double perpendicularSquared = glm::dot(perpendicular, perpendicular);
	if (perpendicularSquared > radiusSquared)
		return false;

	const double halfChord = std::sqrt(glm::max(radiusSquared - perpendicularSquared, 0.0));
	const double hitDistance = projection - halfChord;
	if (!std::isfinite(hitDistance) || hitDistance < 0.0 || hitDistance > distance)
		return false;

	const glm::dvec3 movedCenter = origin + unitDirection * hitDistance;
	const glm::dvec3 separation = movedCenter - obstacleCenter;
	const double separationLength = glm::length(separation);
	const glm::dvec3 normal = separationLength > 0.0 ? separation / separationLength : -unitDirection;
	const glm::dvec3 point = obstacleCenter + normal * static_cast<double>(obstacle.radius);

	return TryCommitCollisionRayHit(point, normal, hitDistance, outHitResult);
}

bool Collision::Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, const BoundingBox& obstacle, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec3 origin, unitDirection;
	if (!sphere.IsValid() || !obstacle.IsValid() ||
		!TryPrepareCollisionRay(Ray(sphere.center, direction), distance, origin, unitDirection))
		return false;

	double hitDistance = 0.0;
	if (!TryFindSphereBoxSweepDistance(origin, unitDirection, distance, sphere.radius, obstacle, hitDistance))
		return false;

	const glm::dvec3 movedCenter = origin + unitDirection * hitDistance;
	const glm::dvec3 point = glm::clamp(movedCenter, glm::dvec3(obstacle.min), glm::dvec3(obstacle.max));
	const glm::dvec3 separation = movedCenter - point;
	const double separationLength = glm::length(separation);

	glm::dvec3 normal = -unitDirection;
	if (hitDistance > 0.0 && separationLength > 0.0)
		normal = separation / separationLength;

	return TryCommitCollisionRayHit(point, normal, hitDistance, outHitResult);
}

bool Collision::Sweep(const BoundingBox& box, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult)
{
	outHitResult.Reset();

	glm::dvec3 origin, relativeDirection;
	if (!box.IsValid() || !obstacle.IsValid() ||
		!TryPrepareCollisionRay(Ray(obstacle.center, -direction), distance, origin, relativeDirection))
		return false;

	double hitDistance = 0.0;
	if (!TryFindSphereBoxSweepDistance(origin, relativeDirection, distance, obstacle.radius, box, hitDistance))
		return false;

	const glm::dvec3 relativeCenter = origin + relativeDirection * hitDistance;
	const glm::dvec3 relativePoint = glm::clamp(relativeCenter, glm::dvec3(box.min), glm::dvec3(box.max));
	const glm::dvec3 separation = relativeCenter - relativePoint;
	const double separationLength = glm::length(separation);

	glm::dvec3 relativeNormal = -relativeDirection;
	if (hitDistance > 0.0 && separationLength > 0.0)
		relativeNormal = separation / separationLength;

	const glm::dvec3 point = relativePoint - relativeDirection * hitDistance;
	return TryCommitCollisionRayHit(point, -relativeNormal, hitDistance, outHitResult);
}
#pragma endregion