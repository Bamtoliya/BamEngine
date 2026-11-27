#pragma once

#include "Types.h"

struct InstanceData
{
	Engine::mat4 worldMatrix; // 64 bytes
	Engine::vec4 pickingData; // 16 bytes (x: pickingId, y: 1이면 데칼을 받지 않음)
	Engine::vec4 customData;  // 16 bytes (StaticMeshRendererComponent::customData, 의미는 머티리얼 셰이더가 정한다)
};
// instanceData.hlsli의 InstanceData와 크기·순서가 같아야 한다.
static_assert(sizeof(InstanceData) == 96, "InstanceData must be 96 bytes");

struct SpriteInstanceData
{
	Engine::mat4 worldMatrix; // 64 bytes
	Engine::vec4 color;       // 16 bytes
	Engine::vec4 uvTransform; // 16 bytes
	Engine::vec4 pickingData; // 16 bytes (x: pickingId)
};
static_assert(sizeof(SpriteInstanceData) == 112, "SpriteInstanceData must be 112 bytes");