#pragma once

#include "Engine_API.h"
#include "ReflectionMacro.h"

STRUCT()
struct ENGINE_API Bone
{
	REFLECT_STRUCT()

	PROPERTY()
	wstring Name;

	PROPERTY()
	mat4 OffsetMatrix = glm::identity<mat4>();

	PROPERTY()
	mat4 LocalTransform = glm::identity<mat4>();

	PROPERTY()
	int32 ParentIndex = { -1 };

	bool operator==(const Bone& other) const = default;
};