#pragma once
#include "Engine_Includes.h"
#include "ReflectionMacro.h"

namespace Engine {
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
}