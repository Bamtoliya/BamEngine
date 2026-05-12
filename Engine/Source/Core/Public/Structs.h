#pragma once

#include "Types.h"
#include "RenderTypes.h"
#include "ReflectionMacro.h"
#include "Vertex.h"

namespace Engine
{
#pragma region Ray
	struct ENGINE_API Ray
	{
		vec3 Origin;
		vec3 Direction;

		Ray() = default;
		Ray(const vec3& origin, const vec3& direction)
			: Origin(origin), Direction(direction) {
		}
	};

	struct ENGINE_API HitResult
	{
		vec3 Point;
		vec3 Normal;
		union {
			f32 Distance;
			f32 PenetrationDepth;
		};

		union
		{
			vec3 Extents;
			vec3 Barycentric;
		};
		
		void* UserData;
		bool HasHit;

		void Reset()
		{
			Point = vec3(0.f);
			Normal = vec3(0.f);
			Distance = 0.f;
			Extents = vec3(0.f);
			UserData = nullptr;
			HasHit = false;
		}
	};
#pragma endregion

#pragma region Bounds

	STRUCT()
	struct ENGINE_API AABB
	{
		REFLECT_STRUCT();

		PROPERTY()
		vec3 Min = {0.f, 0.f, 0.f};
		PROPERTY()
		vec3 Max = {0.f, 0.f, 0.f};
		AABB() {}
		AABB(const vec3& min, const vec3& max)
			: Min(min), Max(max) {
		}
		vec3 Center() const { return (Min + Max) * 0.5f; }
		vec3 Extent() const { return (Max - Min) * 0.5f; }
	};

	struct BoundingBox
	{
		vec3 Center;
		vec3 Extent;
		BoundingBox() = default;
		BoundingBox(const vec3& center, const vec3& extent)
			: Center(center), Extent(extent) {
		}
	};

	struct BoundingSphere
	{
		vec3 Center;
		float Radius;
		BoundingSphere() = default;
		BoundingSphere(const vec3& center, float radius)
			: Center(center), Radius(radius) {
		}
	};

	struct Capsule
	{
		vec3 PointA;
		vec3 PointB;
		f32 Radius;
		Capsule() = default;
		Capsule(const vec3& pointA, const vec3& pointB, f32 radius)
			: PointA(pointA), PointB(pointB), Radius(radius) {
		}
		vec3 Center() const { return (PointA + PointB) * 0.5f; }
		f32 Height() const { return glm::distance(PointA, PointB); }
	};
#pragma endregion

#pragma region Rect
	STRUCT()
	struct ENGINE_API Rect
	{
		REFLECT_STRUCT();

		PROPERTY(EDITABLE)
		f32 Left;
		PROPERTY(EDITABLE)
		f32 Top;
		PROPERTY(EDITABLE)
		f32 Width;
		PROPERTY(EDITABLE)
		f32	Height;

		Rect() : Left(0), Top(0), Width(0), Height(0) {}
		Rect(f32 l, f32 r, f32 w, f32 h) : Left(l), Top(r), Width(w), Height(h) {}
		Rect(const vec4& vec) : Left(vec.x), Top(vec.y), Width(vec.z), Height(vec.w) {}

		f32 Right() const { return Left + Width; }
		f32 Bottom() const { return Top + Height; }

		vec2 Center() const { return vec2(Left + Width * 0.5f, Top + Height * 0.5f); }

		bool Contains(const vec2& point) const
		{
			return point.x >= Left && point.x <= Right() && point.y >= Top && point.y <= Bottom();
		}

		bool Intersects(const Rect& other) const
		{
			return !(other.Left > Right() || other.Right() < Left || other.Top > Bottom() || other.Bottom() < Top);
		}

		operator vec4() const { return vec4(Left, Top, Width, Height); }
		bool operator==(const Rect& other) const
		{
			return Left == other.Left && Top == other.Top && Width == other.Width && Height == other.Height;
		}

	};
#pragma endregion
}