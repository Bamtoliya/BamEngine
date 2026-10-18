#pragma once

#include "Engine_API.h"
#include "Reflection/ReflectionMacro.h"
#include "RHITextureTypes.h"

BEGIN(Engine)

STRUCT()
struct ENGINE_API TextureBinaryHeader
{
	REFLECT_BODY()

	PROPERTY()
	uint32 width = 0;

	PROPERTY()
	uint32 height = 0;

	PROPERTY()
	uint32 depth = 0;

	PROPERTY()
	uint32 mipLevels = 0;

	PROPERTY()
	uint32 arraySize = 0;

	PROPERTY()
	ETextureFormat format = ETextureFormat::UNKNOWN;

	PROPERTY()
	ETextureDimension dimension = ETextureDimension::Texture2D;

	PROPERTY()
	uint32 dataSize = 0;
};

END