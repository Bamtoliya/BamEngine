#pragma once

#include "Collision.h"


#pragma region Raycasts
bool Collision::Raycast(const Ray& ray, const Rect& rect, HitResult& outHitResult)
{
	if (abs(ray.direction.z) < 1e-6f)
		return false; // Ray is parallel to the rectangle plane

	f32 t = -ray.origin.z / ray.direction.z;

	vec3 hitPoint = ray.origin + t * ray.direction;

	f32 minX = std::min(rect.left, rect.Right());
	f32 maxX = std::max(rect.left, rect.Right());
	f32 minY = std::min(rect.top, rect.Bottom());
	f32 maxY = std::max(rect.top, rect.Bottom());

	if (hitPoint.x >= minX && hitPoint.x <= maxX && hitPoint.y >= minY && hitPoint.y <= maxY)
	{
		outHitResult.point = hitPoint;
		outHitResult.normal = (ray.origin.z > 0.0f) ? vec3(0.f, 0.f, 1.f) : vec3(0.f, 0.f, -1.f);
		outHitResult.distance = std::abs(t);
		outHitResult.hasHit = true;
		return true;
	}

	return false;
}

bool Collision::Raycast(const Ray& ray, const AABB& aabb, HitResult& outHitResult)
{
	vec3 invDir = 1.0f / ray.direction;

	vec3 t1 = (aabb.min - ray.origin) * invDir;
	vec3 t2 = (aabb.max - ray.origin) * invDir;

	vec3 tMin = glm::min(t1, t2);
	vec3 tMax = glm::max(t1, t2);

	f32 tEnter = glm::max(glm::max(tMin.x, tMin.y), tMin.z);
	f32 tExit = glm::min(glm::min(tMax.x, tMax.y), tMax.z);

	if (tEnter > tExit || tExit < 0)
		return false; // No intersection

	outHitResult.hasHit = true;
	outHitResult.distance = (tEnter < 0) ? tExit : tEnter;
	outHitResult.point = ray.origin + outHitResult.distance * ray.direction;

	vec3 center = (aabb.min + aabb.max) * 0.5f;
	vec3 extent = (aabb.max - aabb.min) * 0.5f;

	vec3 localHitPoint = outHitResult.point - center;

	vec3 normal = vec3(0.0f);
	f32 minDist = FLT_MAX;

	for (int i = 0; i < 3; ++i)
	{
		f32 distToFace = abs(extent[i] - abs(localHitPoint[i]));
		if (distToFace < minDist)
		{
			minDist = distToFace;
			normal = vec3(0.0f);
			normal[i] = (localHitPoint[i] > 0) ? 1.0f : -1.0f;
		}
	}
	outHitResult.normal = normal;

	return true;
}

bool Collision::Raycast(const Ray& ray, const AABB& aabb, const mat4& worldMatrix, HitResult& outHitResult)
{
    mat4 invWorld = glm::inverse(worldMatrix);
    vec3 localOrigin = vec3(invWorld * vec4(ray.origin, 1.0f));
    vec3 localDirection = vec3(invWorld * vec4(ray.direction, 0.0f));

    f32 localDirectionLength = glm::length(localDirection);
    localDirection = glm::normalize(localDirection);

    Ray localRay(localOrigin, localDirection);
    if (Raycast(localRay, aabb, outHitResult))
    {
        outHitResult.point = vec3(worldMatrix * vec4(outHitResult.point, 1.0f));
        outHitResult.normal = vec3(glm::transpose(glm::inverse(worldMatrix)) * vec4(outHitResult.normal, 0.0f));
        outHitResult.normal = glm::normalize(outHitResult.normal);
        outHitResult.distance /= localDirectionLength;
        return true;
    }

    return false;
}

bool Collision::Raycast(const Ray& ray, const BoundingSphere& sphere, HitResult& outHitResult)
{
	vec3 l = sphere.center - ray.origin;
	f32 tca = glm::dot(l, ray.direction);
	if (tca < 0) return false; // Sphere is behind the ray

	f32 d2 = glm::dot(l, l) - tca * tca;
	f32 radius2 = sphere.radius * sphere.radius;
	if (d2 > radius2) return false; // Ray misses the sphere

	f32 thc = sqrt(radius2 - d2);
	f32 t0 = tca - thc;
	f32 t1 = tca + thc;

	if (t0 > t1) std::swap(t0, t1);

	if (t0 < 0.f)
	{
		t0 = t1; // If t0 is negative, let's use t1 instead
		if (t0 < 0) return false; // Both t0 and t1 are negative
	}

	outHitResult.hasHit = true;
	outHitResult.distance = t0;
	outHitResult.point = ray.origin + t0 * ray.direction;
	outHitResult.normal = glm::normalize(outHitResult.point - sphere.center);

	return true;
}

#pragma endregion


#pragma region Contains
bool Collision::Contains(const Rect& rect, const vec2& point)
{
	return (point.x >= rect.left && point.x <= rect.Right() && point.y >= rect.top && point.y <= rect.Bottom());
}

bool Collision::Contains(const AABB& aabb, const vec3& point)
{
	return (point.x >= aabb.min.x && point.x <= aabb.max.x &&
		point.y >= aabb.min.y && point.y <= aabb.max.y &&
		point.z >= aabb.min.z && point.z <= aabb.max.z);
}

bool Collision::Contains(const AABB& aabb, const mat4& worldMatrix, const vec3& point)
{
	mat4 invWorld = glm::inverse(worldMatrix);
	vec3 localPoint = vec3(invWorld * vec4(point, 1.0f));
	return Contains(aabb, localPoint);
}

bool Collision::Contains(const BoundingSphere& sphere, const vec3& point)
{
	return glm::distance2(sphere.center, point) <= sphere.radius * sphere.radius;
}
#pragma endregion

#pragma region Intersects
bool Collision::Intersects(const Rect& a, const Rect& b)
{
	return (a.left <= b.Right() && a.Right() >= b.left) &&
		(a.top <= b.Bottom() && a.Bottom() >= b.top);
}

bool Collision::Intersects(const Rect& a, const Rect& b, HitResult& outHitResult)
{
	if (!Intersects(a, b))
	{
		outHitResult.Reset();
		return false;
	}

	f32 xOverlap = glm::min(a.Right(), b.Right()) - glm::max(a.left, b.left);
	f32 yOverlap = glm::min(a.Bottom(), b.Bottom()) - glm::max(a.top, b.top);

	outHitResult.hasHit = true;
	vec2 aCenter = a.Center();
	vec2 bCenter = b.Center();
	if (xOverlap < yOverlap)
	{
		outHitResult.distance = xOverlap;
		outHitResult.normal = (aCenter.x < bCenter.x) ? vec3(-1, 0, 0) : vec3(1, 0, 0);
		outHitResult.point = vec3((aCenter.x + bCenter.x) * 0.5f, aCenter.y, 0);
	}
	else
	{
		outHitResult.distance = yOverlap;
		outHitResult.normal = (aCenter.y < bCenter.y) ? vec3(0, -1, 0) : vec3(0, 1, 0);
		outHitResult.point = vec3(aCenter.x, (aCenter.y + bCenter.y) * 0.5f, 0);
	}

	return true;
}

bool Collision::Intersects(const AABB& a, const AABB& b)
{
	return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
		(a.min.y <= b.max.y && a.max.y >= b.min.y) &&
		(a.min.z <= b.max.z && a.max.z >= b.min.z);
}

bool Collision::Intersects(const AABB& a, const AABB& b, HitResult& outHitResult)
{
	if (!Intersects(a, b))
	{
		outHitResult.Reset();
		return false;
	}

	outHitResult.hasHit = true;

	f32 xOverlap = glm::min(a.max.x, b.max.x) - glm::max(a.min.x, b.min.x);
	f32 yOverlap = glm::min(a.max.y, b.max.y) - glm::max(a.min.y, b.min.y);
	f32 zOverlap = glm::min(a.max.z, b.max.z) - glm::max(a.min.z, b.min.z);

	outHitResult.extents = vec3(xOverlap * 0.5f, yOverlap * 0.5f, zOverlap * 0.5f);
	outHitResult.point.x = glm::max(a.min.x, b.min.x) + outHitResult.extents.x;
	outHitResult.point.y = glm::max(a.min.y, b.min.y) + outHitResult.extents.y;
	outHitResult.point.z = glm::max(a.min.z, b.min.z) + outHitResult.extents.z;

	vec3 aCenter = a.Center();
	vec3 bCenter = b.Center();

	if (xOverlap < yOverlap && xOverlap < zOverlap)
	{
		outHitResult.denetrationDepth = xOverlap;
		outHitResult.normal = (aCenter.x < bCenter.x) ? vec3(-1, 0, 0) : vec3(1, 0, 0);
	}
	else if (yOverlap < zOverlap)
	{
		outHitResult.denetrationDepth = yOverlap;
		outHitResult.normal = (aCenter.y < bCenter.y) ? vec3(0, -1, 0) : vec3(0, 1, 0);
	}
	else
	{
		outHitResult.denetrationDepth = zOverlap;
		outHitResult.normal = (aCenter.z < bCenter.z) ? vec3(0, 0, -1) : vec3(0, 0, 1);
	}
	return true;
}

bool Collision::Intersects(const BoundingSphere& a, const BoundingSphere& b)
{
	f32 radiusSum = a.radius + b.radius;
	return glm::distance2(a.center, b.center) <= radiusSum * radiusSum;
}

bool Collision::Intersects(const BoundingSphere& a, const BoundingSphere& b, HitResult& outHitResult)
{
	vec3 centerDelta = b.center - a.center;
	f32 distance2 = glm::dot(centerDelta, centerDelta);
	f32 radiusSum = a.radius + b.radius;

	if (distance2 > radiusSum * radiusSum)
	{
		outHitResult.Reset();
		return false; // No intersection
	}

	f32 distance = sqrt(distance2);
	outHitResult.hasHit = true;
	outHitResult.denetrationDepth = radiusSum - distance;

	if (distance > 1e-6f)
	{
		outHitResult.normal = centerDelta / distance;
		outHitResult.point = a.center + outHitResult.normal * a.radius;
	}
	else
	{
		outHitResult.normal = vec3(0, 1, 0); // Arbitrary normal if centers are the same
		outHitResult.point = a.center; // Arbitrary point on the surface
	}
	return true;
}

bool Collision::Intersects(const AABB& aabb, const BoundingSphere& sphere)
{
	vec3 closestPoint = glm::clamp(sphere.center, aabb.min, aabb.max);
	return glm::distance2(closestPoint, sphere.center) <= sphere.radius * sphere.radius;
}

bool Collision::Intersects(const AABB& aabb, const BoundingSphere& sphere, HitResult& outHitResult)
{
	vec3 closestPoint = glm::clamp(sphere.center, aabb.min, aabb.max);
	f32 distance2 = glm::distance2(closestPoint, sphere.center);
	if (distance2 > sphere.radius * sphere.radius)
	{
		outHitResult.Reset();
		return false; // No intersection
	}
	f32 distance = sqrt(distance2);
	outHitResult.hasHit = true;
	outHitResult.denetrationDepth = sphere.radius - distance;
	if (distance > 1e-6f)
	{
		outHitResult.normal = (closestPoint - sphere.center) / distance;
		outHitResult.point = closestPoint;
	}
	else
	{
		outHitResult.normal = vec3(0, 1, 0); // Arbitrary normal if center is inside the box
		outHitResult.point = closestPoint; // Closest point on the box surface
	}
	return true;
}

bool Collision::Intersects(const AABB& a, const AABB& b, const mat4& worldMatrixb)
{
	mat4 invWorldB = glm::inverse(worldMatrixb);
	vec3 localMinB = vec3(invWorldB * vec4(b.min, 1.0f));
	vec3 localMaxB = vec3(invWorldB * vec4(b.max, 1.0f));
	AABB localB(localMinB, localMaxB);
	return Intersects(a, localB);
}

static AABB LocalToWorld(const AABB& local, const mat4& worldMatrix)
{
	vec3 worldMin = vec3(worldMatrix * vec4(local.min, 1.0f));
	vec3 worldMax = vec3(worldMatrix * vec4(local.max, 1.0f));
	return AABB(glm::min(worldMin, worldMax), glm::max(worldMin, worldMax));
}

bool Collision::Intersects(const AABB& a, const AABB& b, const mat4& worldMatrixb, HitResult& outHitResult)
{
	AABB localB = LocalToWorld(b, worldMatrixb);
	return Intersects(a, localB, outHitResult);
}

bool Collision::Intersects(const AABB& a, const mat4& worldMatrixa, const AABB& b, const mat4& worldMatrixb)
{
	mat4 invWorldA = glm::inverse(worldMatrixa);
	vec3 localMinA = vec3(invWorldA * vec4(a.min, 1.0f));
	vec3 localMaxA = vec3(invWorldA * vec4(a.max, 1.0f));
	AABB localA(localMinA, localMaxA);
	mat4 invWorldB = glm::inverse(worldMatrixb);
	vec3 localMinB = vec3(invWorldB * vec4(b.min, 1.0f));
	vec3 localMaxB = vec3(invWorldB * vec4(b.max, 1.0f));
	AABB localB(localMinB, localMaxB);
	return Intersects(localA, localB);
}

bool Collision::Intersects(const AABB& a, const mat4& worldMatrixa, const AABB& b, const mat4& worldMatrixb, HitResult& outHitResult)
{
	AABB localA = LocalToWorld(a, worldMatrixa);
	AABB localB = LocalToWorld(b, worldMatrixb);
	return Intersects(localA, localB, outHitResult);
}
#pragma endregion

#pragma region Sweep
bool Collision::Sweep(const AABB& box, const vec3& direction, f32 distance, const AABB& obstacle, HitResult& outHitResult)
{
	vec3 boxExtent = box.Extent();

	AABB expandedObstacle(obstacle.min - boxExtent, obstacle.max + boxExtent);

	vec3 boxCenter = box.Center();
	Ray sweepRay(boxCenter, glm::normalize(direction));
	if (Raycast(sweepRay, expandedObstacle, outHitResult))
	{
		if (outHitResult.distance <= distance)
		{
			outHitResult.distance = outHitResult.distance - boxExtent.x; // Adjust for box size
			return true;
		}
	}
	outHitResult.Reset();
	return false;
}

bool Collision::Sweep(const AABB& box, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult)
{
	vec3 boxExtent = box.Extent();
	BoundingSphere expandedSphere(obstacle.center, obstacle.radius + glm::length(boxExtent));
	vec3 boxCenter = box.Center();
	Ray sweepRay(boxCenter, glm::normalize(direction));
	if (Raycast(sweepRay, expandedSphere, outHitResult))
	{
		if (outHitResult.distance <= distance)
		{
			outHitResult.distance = outHitResult.distance - glm::length(boxExtent); // Adjust for box size
			return true;
		}
	}
	outHitResult.Reset();
	return false;
}

bool Collision::Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, const AABB& obstacle, HitResult& outHitResult)
{
	BoundingSphere expandedSphere(sphere.center, sphere.radius);
	vec3 obstacleExtent = obstacle.Extent();
	expandedSphere.radius += glm::length(obstacleExtent);
	Ray sweepRay(sphere.center, glm::normalize(direction));
	if (Raycast(sweepRay, expandedSphere, outHitResult))
	{
		if (outHitResult.distance <= distance)
		{
			outHitResult.distance = outHitResult.distance - sphere.radius; // Adjust for sphere size
			return true;
		}
	}
	outHitResult.Reset();
	return false;
}

bool Collision::Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult)
{
	BoundingSphere expandedSphere(sphere.center, sphere.radius + obstacle.radius);
	Ray sweepRay(sphere.center, glm::normalize(direction));
	if (Raycast(sweepRay, expandedSphere, outHitResult))
	{
		if (outHitResult.distance <= distance)
		{
			outHitResult.distance = outHitResult.distance - sphere.radius; // Adjust for sphere size
			return true;
		}
	}
	outHitResult.Reset();
	return false;
}
#pragma endregion

