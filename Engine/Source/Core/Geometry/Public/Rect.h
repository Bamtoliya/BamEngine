#pragma once
#include "Engine_API.h"
#include "ReflectionMacro.h"

namespace Engine
{
	STRUCT(DISPLAY_NAME("STRUCT_RECT"), CATEGORY("CATEGORY_RECT"))
	struct ENGINE_API Rect
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_LEFT"), CATEGORY("CATEGORY_RECT"))
		f32 left;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TOP"), CATEGORY("CATEGORY_RECT"))
		f32 top;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_WIDTH"), CATEGORY("CATEGORY_RECT"))
		f32 width;
		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_HEIGHT"), CATEGORY("CATEGORY_RECT"))
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
}