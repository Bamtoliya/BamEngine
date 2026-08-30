#pragma once
#include "Base.h"
#include "Reflection/ReflectionMacro.h"

BEGIN(Engine)

ENUM()
enum class EBlendMode
{
	None = 0,
	Opaque = 1 << 0,
	AlphaBlend = 1 << 1,
	Additive = 1 << 2,
	NonPremultiplied = 1 << 3,
	Masked = 1 << 4,
	Forward = 1 << 5,
	Decal = 1 << 6,
	ALL = (1 << 7) - 1
};

ENABLE_BITMASK_OPERATORS(EBlendMode)

ENUM()
enum class EBlendFactor
{
	None,
	Zero,
	One,
	SrcColor,
	InvSrcColor,
	SrcAlpha,
	InvSrcAlpha,
	DstColor,
	InvDstColor,
	DstAlpha,
	InvDstAlpha,
	SrcAlphaSaturate,
	BlendColor,
	InvBlendColor,
};

ENUM()
enum class EBlendOp
{
	None,
	Add,
	Subtract,
	RevSubtract,
	Min,
	Max
};

ENUM()
enum class EColorChannel : uint8
{
	NONE = 0,
	R = 1 << 0,
	G = 1 << 1,
	B = 1 << 2,
	A = 1 << 3,
	RGB = R | G | B,
	RGBA = R | G | B | A,
};

ENABLE_BITMASK_OPERATORS(EColorChannel)

STRUCT()
struct BlendState
{
	REFLECT_STRUCT()

	PROPERTY(EDITABLE)
	EBlendFactor srcColor = EBlendFactor::One;
	PROPERTY(EDITABLE)
	EBlendFactor dstColor = EBlendFactor::Zero;
	PROPERTY(EDITABLE)
	EBlendOp colorBlendOp = EBlendOp::Add;
	PROPERTY(EDITABLE)
	EBlendFactor srcAlpha = EBlendFactor::One;
	PROPERTY(EDITABLE)
	EBlendFactor dstAlpha = EBlendFactor::Zero;
	PROPERTY(EDITABLE)
	EBlendOp alphaBlendOp = EBlendOp::Add;
	PROPERTY(EDITABLE)
	EColorChannel colorWriteMask = EColorChannel::RGBA;

	PROPERTY(EDITABLE)
	bool enable = false;
	PROPERTY(EDITABLE)
	bool enableColorWriteMask = true;

	bool operator==(const BlendState&) const = default;

	BlendState(EBlendMode mode = EBlendMode::None)
	{
		switch (mode)
		{
		default:
		case EBlendMode::None:
		case EBlendMode::Opaque:
		case EBlendMode::Masked:
		case EBlendMode::Forward:
			break;
		case EBlendMode::AlphaBlend:
			srcColor = EBlendFactor::SrcAlpha;
			dstColor = EBlendFactor::InvSrcAlpha;
			colorBlendOp = EBlendOp::Add;
			srcAlpha = EBlendFactor::SrcAlpha;
			dstAlpha = EBlendFactor::InvSrcAlpha;
			alphaBlendOp = EBlendOp::Add;
			enable = true;
			break;
		case EBlendMode::Additive:
			srcColor = EBlendFactor::SrcAlpha;
			dstColor = EBlendFactor::One;
			colorBlendOp = EBlendOp::Add;
			srcAlpha = EBlendFactor::One;
			dstAlpha = EBlendFactor::One;
			alphaBlendOp = EBlendOp::Add;
			colorWriteMask = EColorChannel::RGBA;
			enable = true;
			break;
		case EBlendMode::NonPremultiplied:
			srcColor = EBlendFactor::SrcAlpha;
			dstColor = EBlendFactor::InvSrcAlpha;
			colorBlendOp = EBlendOp::Add;
			srcAlpha = EBlendFactor::One;
			dstAlpha = EBlendFactor::InvSrcAlpha;
			alphaBlendOp = EBlendOp::Add;
			colorWriteMask = EColorChannel::RGBA;
			enable = true;
			break;
		case EBlendMode::Decal:
			srcColor = EBlendFactor::SrcAlpha;
			dstColor = EBlendFactor::InvSrcAlpha;
			colorBlendOp = EBlendOp::Add;
			srcAlpha = EBlendFactor::SrcAlpha;
			dstAlpha = EBlendFactor::InvSrcAlpha;
			alphaBlendOp = EBlendOp::Add;
			colorWriteMask = EColorChannel::RGBA;  // 알파 채널 안 건드림
			enable = true;
			break;
		}
	}
};
END