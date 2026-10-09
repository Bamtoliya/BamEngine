#pragma once

#include "Engine_API.h"
#include "Types.h"
#include "Reflection/ReflectionMacro.h"

STRUCT()
struct ENGINE_API Bone
{
	REFLECT_STRUCT()

	PROPERTY()
	Engine::wstring Name;

	PROPERTY()
	Engine::mat4 OffsetMatrix = glm::identity<Engine::mat4>();

	PROPERTY()
	Engine::mat4 LocalTransform = glm::identity<Engine::mat4>();

	PROPERTY()
	Engine::int32 ParentIndex = { -1 };

	bool operator==(const Bone& other) const = default;
};