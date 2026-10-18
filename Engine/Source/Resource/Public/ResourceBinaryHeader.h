#pragma once

#include "Engine_API.h"
#include "Reflection/ReflectionMacro.h"

BEGIN(Engine)

inline constexpr uint64 ENGINE_ASSET_MAGIC = 0x54455353414D4142;

ENUM()
enum class EResourceType : uint32
{
	Unknown = 0,
	Texture,
	Sprite,
	Material,
	MaterialInstance,
	Shader,
	Mesh,
	Model,
	Animation,
	AudioClip,
	Skeleton,
	Prefab,
	Script,
	Scene,
	Save,
};

STRUCT()
struct ENGINE_API ResourceBinaryHeader
{
	REFLECT_BODY()

	PROPERTY()
	uint64 MagicNumber = ENGINE_ASSET_MAGIC;

	PROPERTY()
	EResourceType ResourceType = EResourceType::Unknown;

	PROPERTY()
	uint32 Version = 1;
};

END