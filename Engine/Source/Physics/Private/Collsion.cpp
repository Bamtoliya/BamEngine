#pragma once

#include "Collision.h"


#pragma region Raycasts
bool Collision::Raycast(const Ray& ray, const Rect& rect, HitResult& outHitResult)
{
	if (abs(ray.Direction.z) < 1e-6f)
		return false; // Ray is parallel to the rectangle plane

	f32 t = -ray.Origin.z / ray.Direction.z;

	vec3 hitPoint = ray.Origin + t * ray.Direction;

	f32 minX = std::min(rect.Left, rect.Right());
	f32 maxX = std::max(rect.Left, rect.Right());
	f32 minY = std::min(rect.Top, rect.Bottom());
	f32 maxY = std::max(rect.Top, rect.Bottom());

	if (hitPoint.x >= minX && hitPoint.x <= maxX && hitPoint.y >= minY && hitPoint.y <= maxY)
	{
		outHitResult.Point = hitPoint;
		outHitResult.Normal = (ray.Origin.z > 0.0f) ? vec3(0.f, 0.f, 1.f) : vec3(0.f, 0.f, -1.f);
		outHitResult.Distance = std::abs(t);
		outHitResult.HasHit = true;
		return true;
	}

	return false;
}

bool Collision::Raycast(const Ray& ray, const AABB& aabb, HitResult& outHitResult)
{
	vec3 invDir = 1.0f / ray.Direction;

	vec3 t1 = (aabb.Min - ray.Origin) * invDir;
	vec3 t2 = (aabb.Max - ray.Origin) * invDir;

	vec3 tMin = glm::min(t1, t2);
	vec3 tMax = glm::max(t1, t2);

	f32 tEnter = glm::max(glm::max(tMin.x, tMin.y), tMin.z);
	f32 tExit = glm::min(glm::min(tMax.x, tMax.y), tMax.z);

	if (tEnter > tExit || tExit < 0)
		return false; // No intersection

	outHitResult.HasHit = true;
	outHitResult.Distance = (tEnter < 0) ? tExit : tEnter;
	outHitResult.Point = ray.Origin + outHitResult.Distance * ray.Direction;

	vec3 center = (aabb.Min + aabb.Max) * 0.5f;
	vec3 extent = (aabb.Max - aabb.Min) * 0.5f;

	vec3 localHitPoint = outHitResult.Point - center;

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
	outHitResult.Normal = normal;

	return false;
}

bool Collision::Raycast(const Ray& ray, const AABB& aabb, const mat4& worldMatrix, HitResult& outHitResult)
{
	mat4 invWorld = glm::inverse(worldMatrix);
	vec3 localOrigin = vec3(invWorld * vec4(ray.Origin, 1.0f));
	vec3 localDirection = vec3(invWorld * vec4(ray.Direction, 0.0f));

	f32 localDirectionLength = glm::length(localDirection);
	localDirection = glm::normalize(localDirection);

	Ray localRay(localOrigin, localDirection);
	if (Raycast(localRay, aabb, outHitResult))
	{
		outHitResult.Point = vec3(worldMatrix * vec4(outHitResult.Point, 1.0f));
		outHitResult.Normal = vec3(glm::transpose(glm::inverse(worldMatrix)) * vec4(outHitResult.Normal, 0.0f));
		outHitResult.Normal = glm::normalize(outHitResult.Normal);
		outHitResult.Distance *= localDirectionLength;
		return true;
	}

	return false;
}

bool Collision::Raycast(const Ray& ray, const BoundingSphere& sphere, HitResult& outHitResult)
{
	vec3 l = sphere.Center - ray.Origin;
	f32 tca = glm::dot(l, ray.Direction);
	if (tca < 0) return false; // Sphere is behind the ray

	f32 d2 = glm::dot(l, l) - tca * tca;
	f32 radius2 = sphere.Radius * sphere.Radius;
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

	outHitResult.HasHit = true;
	outHitResult.Distance = t0;
	outHitResult.Point = ray.Origin + t0 * ray.Direction;
	outHitResult.Normal = glm::normalize(outHitResult.Point - sphere.Center);

	return true;
}

#pragma endregion


#pragma region Contains
bool Collision::Contains(const Rect& rect, const vec2& point)
{
	return (point.x >= rect.Left && point.x <= rect.Right() && point.y >= rect.Top && point.y <= rect.Bottom());
}

bool Collision::Contains(const AABB& aabb, const vec3& point)
{
	return (point.x >= aabb.Min.x && point.x <= aabb.Max.x &&
		point.y >= aabb.Min.y && point.y <= aabb.Max.y &&
		point.z >= aabb.Min.z && point.z <= aabb.Max.z);
}

bool Collision::Contains(const AABB& aabb, const mat4& worldMatrix, const vec3& point)
{
	mat4 invWorld = glm::inverse(worldMatrix);
	vec3 localPoint = vec3(invWorld * vec4(point, 1.0f));
	return Contains(aabb, localPoint);
}

bool Collision::Contains(const BoundingSphere& sphere, const vec3& point)
{
	return glm::distance2(sphere.Center, point) <= sphere.Radius * sphere.Radius;
}
#pragma endregion

#pragma region Intersects
bool Collision::Intersects(const Rect& a, const Rect& b)
{
	return (a.Left <= b.Right() && a.Right() >= b.Left) &&
		(a.Top <= b.Bottom() && a.Bottom() >= b.Top);
}

bool Collision::Intersects(const Rect& a, const Rect& b, HitResult& outHitResult)
{
	if (!Intersects(a, b))
	{
		outHitResult.Reset();
		return false;
	}

	f32 xOverlap = glm::min(a.Right(), b.Right()) - glm::max(a.Left, b.Left);
	f32 yOverlap = glm::min(a.Bottom(), b.Bottom()) - glm::max(a.Top, b.Top);

	outHitResult.HasHit = true;
	vec2 aCenter = a.Center();
	vec2 bCenter = b.Center();
	if (xOverlap < yOverlap)
	{
		outHitResult.Distance = xOverlap;
		outHitResult.Normal = (aCenter.x < bCenter.x) ? vec3(-1, 0, 0) : vec3(1, 0, 0);
		outHitResult.Point = vec3((aCenter.x + bCenter.x) * 0.5f, aCenter.y, 0);
	}
	else
	{
		outHitResult.Distance = yOverlap;
		outHitResult.Normal = (aCenter.y < bCenter.y) ? vec3(0, -1, 0) : vec3(0, 1, 0);
		outHitResult.Point = vec3(aCenter.x, (aCenter.y + bCenter.y) * 0.5f, 0);
	}

	return true;
}

bool Collision::Intersects(const AABB& a, const AABB& b)
{
	return (a.Min.x <= b.Max.x && a.Max.x >= b.Min.x) &&
		(a.Min.y <= b.Max.y && a.Max.y >= b.Min.y) &&
		(a.Min.z <= b.Max.z && a.Max.z >= b.Min.z);
}

bool Collision::Intersects(const AABB& a, const AABB& b, HitResult& outHitResult)
{
	if (!Intersects(a, b))
	{
		outHitResult.Reset();
		return false;
	}

	outHitResult.HasHit = true;

	f32 xOverlap = glm::min(a.Max.x, b.Max.x) - glm::max(a.Min.x, b.Min.x);
	f32 yOverlap = glm::min(a.Max.y, b.Max.y) - glm::max(a.Min.y, b.Min.y);
	f32 zOverlap = glm::min(a.Max.z, b.Max.z) - glm::max(a.Min.z, b.Min.z);

	outHitResult.Extents = vec3(xOverlap * 0.5f, yOverlap * 0.5f, zOverlap * 0.5f);
	outHitResult.Point.x = glm::max(a.Min.x, b.Min.x) + outHitResult.Extents.x;
	outHitResult.Point.y = glm::max(a.Min.y, b.Min.y) + outHitResult.Extents.y;
	outHitResult.Point.z = glm::max(a.Min.z, b.Min.z) + outHitResult.Extents.z;

	vec3 aCenter = a.Center();
	vec3 bCenter = b.Center();

	if (xOverlap < yOverlap && xOverlap < zOverlap)
	{
		outHitResult.PenetrationDepth = xOverlap;
		outHitResult.Normal = (aCenter.x < bCenter.x) ? vec3(-1, 0, 0) : vec3(1, 0, 0);
	}
	else if (yOverlap < zOverlap)
	{
		outHitResult.PenetrationDepth = yOverlap;
		outHitResult.Normal = (aCenter.y < bCenter.y) ? vec3(0, -1, 0) : vec3(0, 1, 0);
	}
	else
	{
		outHitResult.PenetrationDepth = zOverlap;
		outHitResult.Normal = (aCenter.z < bCenter.z) ? vec3(0, 0, -1) : vec3(0, 0, 1);
	}
	return true;
}

bool Collision::Intersects(const BoundingSphere& a, const BoundingSphere& b)
{
	f32 radiusSum = a.Radius + b.Radius;
	return glm::distance2(a.Center, b.Center) <= radiusSum * radiusSum;
}

bool Collision::Intersects(const BoundingSphere& a, const BoundingSphere& b, HitResult& outHitResult)
{
	vec3 centerDelta = b.Center - a.Center;
	f32 distance2 = glm::dot(centerDelta, centerDelta);
	f32 radiusSum = a.Radius + b.Radius;

	if (distance2 > radiusSum * radiusSum)
	{
		outHitResult.Reset();
		return false; // No intersection
	}

	f32 distance = sqrt(distance2);
	outHitResult.HasHit = true;
	outHitResult.PenetrationDepth = radiusSum - distance;

	if (distance > 1e-6f)
	{
		outHitResult.Normal = centerDelta / distance;
		outHitResult.Point = a.Center + outHitResult.Normal * a.Radius;
	}
	else
	{
		outHitResult.Normal = vec3(0, 1, 0); // Arbitrary normal if centers are the same
		outHitResult.Point = a.Center; // Arbitrary point on the surface
	}
	return true;
}

bool Collision::Intersects(const AABB& aabb, const BoundingSphere& sphere)
{
	vec3 closestPoint = glm::clamp(sphere.Center, aabb.Min, aabb.Max);
	return glm::distance2(closestPoint, sphere.Center) <= sphere.Radius * sphere.Radius;
}

bool Collision::Intersects(const AABB& aabb, const BoundingSphere& sphere, HitResult& outHitResult)
{
	vec3 closestPoint = glm::clamp(sphere.Center, aabb.Min, aabb.Max);
	f32 distance2 = glm::distance2(closestPoint, sphere.Center);
	if (distance2 > sphere.Radius * sphere.Radius)
	{
		outHitResult.Reset();
		return false; // No intersection
	}
	f32 distance = sqrt(distance2);
	outHitResult.HasHit = true;
	outHitResult.PenetrationDepth = sphere.Radius - distance;
	if (distance > 1e-6f)
	{
		outHitResult.Normal = (closestPoint - sphere.Center) / distance;
		outHitResult.Point = closestPoint;
	}
	else
	{
		outHitResult.Normal = vec3(0, 1, 0); // Arbitrary normal if center is inside the box
		outHitResult.Point = closestPoint; // Closest point on the box surface
	}
	return true;
}

bool Collision::Intersects(const AABB& a, const AABB& b, const mat4& worldMatrixb)
{
	mat4 invWorldB = glm::inverse(worldMatrixb);
	vec3 localMinB = vec3(invWorldB * vec4(b.Min, 1.0f));
	vec3 localMaxB = vec3(invWorldB * vec4(b.Max, 1.0f));
	AABB localB(localMinB, localMaxB);
	return Intersects(a, localB);
}

static AABB LocalToWorld(const AABB& local, const mat4& worldMatrix)
{
	vec3 worldMin = vec3(worldMatrix * vec4(local.Min, 1.0f));
	vec3 worldMax = vec3(worldMatrix * vec4(local.Max, 1.0f));
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
	vec3 localMinA = vec3(invWorldA * vec4(a.Min, 1.0f));
	vec3 localMaxA = vec3(invWorldA * vec4(a.Max, 1.0f));
	AABB localA(localMinA, localMaxA);
	mat4 invWorldB = glm::inverse(worldMatrixb);
	vec3 localMinB = vec3(invWorldB * vec4(b.Min, 1.0f));
	vec3 localMaxB = vec3(invWorldB * vec4(b.Max, 1.0f));
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

	AABB expandedObstacle(obstacle.Min - boxExtent, obstacle.Max + boxExtent);

	vec3 boxCenter = box.Center();
	Ray sweepRay(boxCenter, glm::normalize(direction));
	if (Raycast(sweepRay, expandedObstacle, outHitResult))
	{
		if (outHitResult.Distance <= distance)
		{
			outHitResult.Distance = outHitResult.Distance - boxExtent.x; // Adjust for box size
			return true;
		}
	}
	outHitResult.Reset();
	return false;
}

bool Collision::Sweep(const AABB& box, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult)
{
	vec3 boxExtent = box.Extent();
	BoundingSphere expandedSphere(obstacle.Center, obstacle.Radius + glm::length(boxExtent));
	vec3 boxCenter = box.Center();
	Ray sweepRay(boxCenter, glm::normalize(direction));
	if (Raycast(sweepRay, expandedSphere, outHitResult))
	{
		if (outHitResult.Distance <= distance)
		{
			outHitResult.Distance = outHitResult.Distance - glm::length(boxExtent); // Adjust for box size
			return true;
		}
	}
	outHitResult.Reset();
	return false;
}

bool Collision::Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, const AABB& obstacle, HitResult& outHitResult)
{
	BoundingSphere expandedSphere(sphere.Center, sphere.Radius);
	vec3 obstacleExtent = obstacle.Extent();
	expandedSphere.Radius += glm::length(obstacleExtent);
	Ray sweepRay(sphere.Center, glm::normalize(direction));
	if (Raycast(sweepRay, expandedSphere, outHitResult))
	{
		if (outHitResult.Distance <= distance)
		{
			outHitResult.Distance = outHitResult.Distance - sphere.Radius; // Adjust for sphere size
			return true;
		}
	}
	outHitResult.Reset();
	return false;
}

bool Collision::Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult)
{
	BoundingSphere expandedSphere(sphere.Center, sphere.Radius + obstacle.Radius);
	Ray sweepRay(sphere.Center, glm::normalize(direction));
	if (Raycast(sweepRay, expandedSphere, outHitResult))
	{
		if (outHitResult.Distance <= distance)
		{
			outHitResult.Distance = outHitResult.Distance - sphere.Radius; // Adjust for sphere size
			return true;
		}
	}
	outHitResult.Reset();
	return false;
}
#pragma endregion

