#pragma once

#include "Macro.h"
#include "Structs.h"
#include <cfloat>

namespace Engine
{
	struct TerrainComponent;
	class ENGINE_API Collision
	{
	public:
		static bool Raycast(const Ray& ray, const Rect& rect, HitResult& outHitResult, f32 maxDistance = FLT_MAX);
		static bool Raycast(const Ray& ray, const BoundingBox& box, HitResult& outHitResult, f32 maxDistance = FLT_MAX);
		static bool Raycast(const Ray& ray, const BoundingBox& box, const mat4& worldMatrix, HitResult& outHitResult, f32 maxDistance = FLT_MAX);
		static bool Raycast(const Ray& ray, const vec3& v0, const vec3& v1, const vec3& v2, HitResult& outHitResult, f32 maxDistance = FLT_MAX);
		static bool Raycast(const Ray& ray, const class Mesh& mesh, const mat4& worldMatrix, HitResult& outHitResult, f32 maxDistance = FLT_MAX);
		static bool Raycast(const Ray& ray, const BoundingSphere& sphere, HitResult& outHitResult, f32 maxDistance = FLT_MAX);
		static bool Raycast(const Ray& ray, const Capsule& capsule, HitResult& outHitResult, f32 maxDistance = FLT_MAX);
		static bool Raycast(const Ray& ray, const TerrainComponent& terrain, const mat4& worldMatrix, HitResult& outHitResult, f32 maxDistance = FLT_MAX);
	public:
		static bool Contains(const Rect& rect, const vec2& point);
		static bool Contains(const BoundingBox& box, const vec3& point);
		static bool Contains(const BoundingBox& box, const mat4& worldMatrix, const vec3& point);
		static bool Contains(const BoundingSphere& sphere, const vec3& point);
	public://2D
		static bool Intersects(const Rect& a, const Rect& b);
		static bool Intersects(const Rect& a, const Rect& b, HitResult& outHitResult);
		static bool Intersects(const Rect& a, const mat3& transformA, const Rect& b, const mat3& transformB);
		static bool Intersects(const Rect& a, const mat3& transformA, const Rect& b, const mat3& transformB, HitResult& outHitResult);
		static bool Intersects(const Circle& a, const Circle& b);
		static bool Intersects(const Circle& a, const Circle& b, HitResult& outHitResult);
		static bool Intersects(const Rect& rect, const Circle& circle);
		static bool Intersects(const Rect& rect, const Circle& circle, HitResult& outHitResult);
		static bool Intersects(const Circle& circle, const Rect& rect);
		static bool Intersects(const Circle& circle, const Rect& rect, HitResult& outHitResult);
		static bool Intersects(const Rect& rect, const mat3& transform, const Circle& circle);
		static bool Intersects(const Rect& rect, const mat3& transform, const Circle& circle, HitResult& outHitResult);
		static bool Intersects(const Circle& circle, const Rect& rect, const mat3& transform);
		static bool Intersects(const Circle& circle, const Rect& rect, const mat3& transform, HitResult& outHitResult);
		static bool Intersects(const Capsule& capsule, const Rect& rect, const mat3& transform);
		static bool Intersects(const Capsule& capsule, const Rect& rect, const mat3& transform, HitResult& outHitResult);
		static bool Intersects(const Rect& rect, const mat3& transform, const Capsule& capsule);
		static bool Intersects(const Rect& rect, const mat3& transform, const Capsule& capsule, HitResult& outHitResult);
		static bool Intersects(const Capsule& capsule, const Circle& circle);
		static bool Intersects(const Capsule& capsule, const Circle& circle, HitResult& outHitResult);
		static bool Intersects(const Circle& circle, const Capsule& capsule);
		static bool Intersects(const Circle& circle, const Capsule& capsule, HitResult& outHitResult);
		static bool Intersects(const Capsule& capsule, const Rect& rect);
		static bool Intersects(const Capsule& capsule, const Rect& rect, HitResult& outHitResult);
		static bool Intersects(const Rect& rect, const Capsule& capsule);
		static bool Intersects(const Rect& rect, const Capsule& capsule, HitResult& outHitResult);
		static bool Intersects2D(const Capsule& a, const Capsule& b);
		static bool Intersects2D(const Capsule& a, const Capsule& b, HitResult& outHitResult);
		static bool Intersects(const Capsule& capsule, const BoundingSphere& sphere);
		static bool Intersects(const Capsule& capsule, const BoundingSphere& sphere, HitResult& outHitResult);
		static bool Intersects(const BoundingSphere& sphere, const Capsule& capsule);
		static bool Intersects(const BoundingSphere& sphere, const Capsule& capsule, HitResult& outHitResult);
	public://3D
		static bool Intersects(const BoundingBox& a, const BoundingBox& b);
		static bool Intersects(const BoundingBox& a, const BoundingBox& b, HitResult& outHitResult);
		static bool Intersects(const BoundingSphere& a, const BoundingSphere& b);
		static bool Intersects(const BoundingSphere& a, const BoundingSphere& b, HitResult& outHitResult);
		static bool Intersects(const BoundingBox& box, const BoundingSphere& sphere);
		static bool Intersects(const BoundingBox& box, const BoundingSphere& sphere, HitResult& outHitResult);
		static bool Intersects(const BoundingBox& a, const BoundingBox& b, const mat4& worldMatrixb);
		static bool Intersects(const BoundingBox& a, const BoundingBox& b, const mat4& worldMatrixb, HitResult& outHitResult);
		static bool Intersects(const BoundingBox& a, const mat4& worldMatrixa, const BoundingBox& b, const mat4& worldMatrixb);
		static bool Intersects(const BoundingBox& a, const mat4& worldMatrixa, const BoundingBox& b, const mat4& worldMatrixb, HitResult& outHitResult);
		static bool Intersects(const Capsule& capsule, const BoundingBox& box);
		static bool Intersects(const Capsule& capsule, const BoundingBox& box, HitResult& outHitResult);
		static bool Intersects(const BoundingBox& box, const Capsule& capsule) { return Intersects(capsule, box); }
		static bool Intersects(const BoundingBox& box, const Capsule& capsule, HitResult& outHitResult) { return Intersects(capsule, box, outHitResult); }
		static bool Intersects(const Capsule& capsule, const BoundingBox& box, const mat4& worldMatrixBox, HitResult& outHitResult);
		static bool Intersects3D(const Capsule& a, const Capsule& b);
		static bool Intersects3D(const Capsule& a, const Capsule& b, HitResult& outHitResult);
	public:
		static bool Sweep(const BoundingBox& box, const vec3& direction, f32 distance, const BoundingBox& obstacle, HitResult& outHitResult);
		static bool Sweep(const BoundingBox& box, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult);
		static bool Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, const BoundingBox& obstacle, HitResult& outHitResult);
		static bool Sweep(const BoundingSphere& sphere, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult);
		static bool Sweep(const Capsule& capsule, const vec3& direction, f32 distance, const BoundingBox& obstacle, HitResult& outHitResult);
		static bool Sweep(const Capsule& capsule, const vec3& direction, f32 distance, const BoundingBox& obstacle, const mat4& worldMatrixObstacle, HitResult& outHitResult);
		static bool Sweep(const Capsule& capsule, const vec3& direction, f32 distance, const BoundingSphere& obstacle, HitResult& outHitResult);
		static bool Sweep(const Capsule& capsule, const vec3& direction, f32 distance, const Capsule& obstacle, HitResult& outHitResult);
	};
}
