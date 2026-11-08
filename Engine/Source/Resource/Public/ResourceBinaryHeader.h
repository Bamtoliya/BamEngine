#pragma once

#include "Engine_API.h"
#include "ReflectionMacro.h"

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
	SpriteAnimation,
	ActionTimeline,
	ParticleEffect,
	CompositeEffect,
};

inline constexpr const char* GetResourceBinaryExtension(EResourceType type) noexcept
{
	switch (type)
	{
	case EResourceType::Material:         return ".bammat";
	case EResourceType::MaterialInstance: return ".bammatinst";
	case EResourceType::Mesh:             return ".bammesh";
	case EResourceType::Sprite:           return ".bamsprite";
	case EResourceType::Skeleton:         return ".bamskel";
	case EResourceType::Animation:        return ".bamanim";
	case EResourceType::Model:            return ".bammodel";
	case EResourceType::Texture:          return ".bamtex";
	case EResourceType::Shader:           return ".bamshader";
	case EResourceType::ParticleEffect:   return ".bameffect";
	case EResourceType::CompositeEffect:  return ".bamvfx";
	default:                              return ".beve";
	}
}

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