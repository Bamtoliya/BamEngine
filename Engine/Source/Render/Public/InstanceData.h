#pragma once

#include "Types.h"

struct InstanceData
{
	mat4 worldMatrix; // 64 bytes
	vec4 pickingData; // 16 bytes (x: pickingId)
};
static_assert(sizeof(InstanceData) == 80, "InstanceData must be 80 bytes");