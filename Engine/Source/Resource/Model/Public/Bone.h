#pragma once

#include "Base.h"

struct Bone
{
	wstring Name;
	mat4 OffsetMatrix = glm::identity<mat4>();
	int32 ParentIndex = { -1 };

	bool operator==(const Bone& other) const = default;
};