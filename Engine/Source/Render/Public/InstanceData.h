#pragma once

#include "Types.h"

struct InstanceData
{
	Engine::mat4 worldMatrix; // 64 bytes
	Engine::vec4 pickingData; // 16 bytes (x: pickingId, y: 1이면 데칼을 받지 않음)
};
static_assert(sizeof(InstanceData) == 80, "InstanceData must be 80 bytes");

struct SpriteInstanceData
{
	Engine::mat4 worldMatrix; // 64 bytes
	Engine::vec4 color;       // 16 bytes
	Engine::vec4 uvTransform; // 16 bytes
	Engine::vec4 pickingData; // 16 bytes (x: pickingId)
};
static_assert(sizeof(SpriteInstanceData) == 112, "SpriteInstanceData must be 112 bytes");