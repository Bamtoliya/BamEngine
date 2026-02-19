#pragma once

#include "Collider.h"

BEGIN(Engine)

class ENGINE_API Collision
{
public:
	static bool Intersects(const Ray& ray, const AABB& aabb, HitResult& outHitResult);
	static bool Intersects(const Ray& ray, const AABB& aabb, const mat4& worldMatrix, HitResult& outHitResult);
	static bool Intersects(const Ray& ray, const BoundingSphere& sphere, HitResult& outHitResult);

	
	static bool Contains(const AABB& aabb, const vec3& point);
	static bool Contains(const AABB& aabb, const mat4& worldMatrix, const vec3& point);
	static bool Contains(const BoundingSphere& sphere, const vec3& point);

	static bool Intersects(const AABB& a, const AABB& b);
	static bool Intersects(const AABB&a, const AABB& b, HitResult& outHitResult);
	
	static bool Intersects(const BoundingSphere& a, const BoundingSphere& b);
	static bool Intersects(const BoundingSphere& a, const BoundingSphere& b, HitResult& outHitResult);

	static bool Intersects(const AABB& aabb, const BoundingSphere& sphere);
	static bool Intersects(const AABB& aabb, const BoundingSphere& sphere, HitResult& outHitResult);

	static bool Intersects(const AABB& a, const AABB& b, const mat4& worldMatrixb);
	static bool Intersects(const AABB& a, const AABB& b, const mat4& worldMatrixb, HitResult& outHitResult);

	static bool Intersects(const AABB& a, const mat4& worldMatrixa, const AABB& b, const mat4& worldMatrixb);
	static bool Intersects(const AABB& a, const mat4& worldMatrixa, const AABB& b, const mat4& worldMatrixb, HitResult& outHitResult);

public:
	static bool Sweep(const AABB& box, const vec3& direction, f32 distance, HitResult& outHitResult);
	static bool Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, HitResult& outHitResult);
};
END