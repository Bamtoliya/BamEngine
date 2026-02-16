#pragma once

#include "Types.h"
#include "ReflectionMacro.h"

namespace Engine
{
#pragma region Ray
	struct Ray
	{
		vec3 Origin;
		vec3 Direction;

		Ray() = default;
		Ray(const vec3& origin, const vec3& direction)
			: Origin(origin), Direction(direction) {
		}
	};

	struct HitResult
	{
		vec3 Point;
		vec3 Normal;
		f32 Distance;
		void* UserData;

		void Reset()
		{
			Point = vec3(0.f);
			Normal = vec3(0.f);
			Distance = 0.f;
			UserData = nullptr;
		}
	};
#pragma endregion

#pragma region Bounds
	struct AABB
	{
		vec3 Min;
		vec3 Max;
		AABB() = default;
		AABB(const vec3& min, const vec3& max)
			: Min(min), Max(max) {
		}
		bool Intersect(const Ray& ray, float* t = nullptr) const;
	};

	struct BoundingBox
	{
		vec3 Center;
		vec3 Extent;
		BoundingBox() = default;
		BoundingBox(const vec3& center, const vec3& extent)
			: Center(center), Extent(extent) {
		}
		bool Intersect(const Ray& ray, float* t = nullptr) const;
	};

	struct BoundingSphere
	{
		vec3 Center;
		float Radius;
		BoundingSphere() = default;
		BoundingSphere(const vec3& center, float radius)
			: Center(center), Radius(radius) {
		}
		bool Intersect(const Ray& ray, float* t = nullptr) const;
	};
#pragma endregion


#pragma region Rect
	STRUCT()
	struct Rect
	{
		REFLECT_STRUCT(Rect);

		PROPERTY()
		f32 Left;
		PROPERTY()
		f32 Top;
		PROPERTY()
		f32 Width;
		PROPERTY()
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


#pragma region Vertex
	STRUCT()
		struct ENGINE_API Vertex
	{
		REFLECT_STRUCT(Vertex);

		PROPERTY()
		glm::vec3 position = {};
		PROPERTY()
		glm::vec3 normal = {};
		PROPERTY()
		glm::vec2 texCoord = {};
		PROPERTY()
		glm::vec3 tangent = {};
		PROPERTY()
		glm::vec3 bitangent = {};
		PROPERTY()
		glm::vec4 color = { 1.f, 1.f, 1.f, 1.f };

		Vertex() = default;
		Vertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv)
			: position(pos), normal(norm), texCoord(uv)
		{
		}

		Vertex(const glm::vec3& pos, const glm::vec2& uv)
			: position(pos), texCoord(uv)
		{
		}

		Vertex(
			const glm::vec3& pos,
			const glm::vec3& nor,
			const glm::vec2& uv,
			const glm::vec3& tan)
			: position(pos), normal(nor), texCoord(uv), tangent(tan)
		{

		}

		Vertex(
			const glm::vec3& pos,
			const glm::vec3& nor,
			const glm::vec2& uv,
			const glm::vec3& tan,
			const glm::vec3& bitan,
			const glm::vec4& col)
			: position(pos), normal(nor), texCoord(uv), tangent(tan), bitangent(bitan), color(col)
		{
		}
	};

	struct Vertex2D
	{
		glm::vec3 position = {};
		glm::vec4 color = {};
		glm::vec2 texCoord = {};
	};
#pragma endregion
}