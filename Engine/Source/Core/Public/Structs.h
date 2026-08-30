#pragma once

#include "Types.h"
#include "RenderTypes.h"
#include "Reflection/ReflectionMacro.h"
#include "Vertex.h"

namespace Engine
{
#pragma region Ray
	struct ENGINE_API Ray
	{
		vec3 origin;
		vec3 direction;

		Ray() = default;
		Ray(const vec3& origin, const vec3& direction)
			: origin(origin), direction(direction) {
		}
	};

	struct ENGINE_API HitResult
	{
		vec3 point;
		vec3 normal;
		union {
			f32 distance;
			f32 denetrationDepth;
		};

		union
		{
			vec3 extents;
			vec3 barycentric;
		};
		
		void* userData;
		bool hasHit;

		void Reset()
		{
			point = vec3(0.f);
			normal = vec3(0.f);
			distance = 0.f;
			extents = vec3(0.f);
			userData = nullptr;
			hasHit = false;
		}
	};
#pragma endregion

#pragma region Bounds

	STRUCT()
	struct ENGINE_API AABB
	{
		REFLECT_STRUCT();

		PROPERTY()
		vec3 min = {0.f, 0.f, 0.f};
		PROPERTY()
		vec3 max = {0.f, 0.f, 0.f};
		AABB() {}
		AABB(const vec3& min, const vec3& max)
			: min(min), max(max) {
		}
		vec3 Center() const { return (min + max) * 0.5f; }
		vec3 Extent() const { return (max - min) * 0.5f; }
	};

	struct BoundingBox
	{
		vec3 center;
		vec3 extent;
		BoundingBox() = default;
		BoundingBox(const vec3& center, const vec3& extent)
			: center(center), extent(extent) {
		}
	};

	struct BoundingSphere
	{
		vec3 center;
		f32 radius;
		BoundingSphere() = default;
		BoundingSphere(const vec3& center, f32 radius)
			: center(center), radius(radius) {
		}
	};

	struct Capsule
	{
		vec3 pointA;
		vec3 pointB;
		f32 radius;
		Capsule() = default;
		Capsule(const vec3& pointA, const vec3& pointB, f32 radius)
			: pointA(pointA), pointB(pointB), radius(radius) {
		}
		vec3 Center() const { return (pointA + pointB) * 0.5f; }
		f32 Height() const { return glm::distance(pointA, pointB); }
	};
#pragma endregion

#pragma region Rect
	STRUCT()
	struct ENGINE_API Rect
	{
		REFLECT_STRUCT();

		PROPERTY(EDITABLE)
		f32 left;
		PROPERTY(EDITABLE)
		f32 top;
		PROPERTY(EDITABLE)
		f32 width;
		PROPERTY(EDITABLE)
		f32	height;

		Rect() : left(0), top(0), width(0), height(0) {}
		Rect(f32 l, f32 r, f32 w, f32 h) : left(l), top(r), width(w), height(h) {}
		Rect(const vec4& vec) : left(vec.x), top(vec.y), width(vec.z), height(vec.w) {}

		f32 Right() const { return left + width; }
		f32 Bottom() const { return top + height; }

		vec2 Center() const { return vec2(left + width * 0.5f, top + height * 0.5f); }

		bool Contains(const vec2& point) const
		{
			return point.x >= left && point.x <= Right() && point.y >= top && point.y <= Bottom();
		}

		bool Intersects(const Rect& other) const
		{
			return !(other.left > Right() || other.Right() < left || other.top > Bottom() || other.Bottom() < top);
		}

		operator vec4() const { return vec4(left, top, width, height); }
		bool operator==(const Rect& other) const
		{
			return left == other.left && top == other.top && width == other.width && height == other.height;
		}

	};
#pragma endregion
}