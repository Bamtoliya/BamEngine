#pragma once

#include "Base.h"

struct Bone
{
	wstring Name;
	mat4 OffsetMatrix;
	int32 ParentIndex;

	bool operator==(const Bone& other) const = default;
};