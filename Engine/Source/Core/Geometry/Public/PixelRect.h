#pragma once
#include "Types.h"

namespace Engine
{
	struct PixelRect
	{
		uint32 minX = 0;
		uint32 minY = 0;
		uint32 maxX = 0;
		uint32 maxY = 0;

		// 최소 좌표는 포함하고 최대 좌표는 포함하지 않습니다.
		bool IsEmpty() const { return minX >= maxX || minY >= maxY; }
		uint32 Width() const { return maxX > minX ? maxX - minX : 0; }
		uint32 Height() const { return maxY > minY ? maxY - minY : 0; }
	};
}