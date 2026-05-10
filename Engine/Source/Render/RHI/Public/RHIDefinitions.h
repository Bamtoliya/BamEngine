#pragma once

#include "Base.h"
#include "ReflectionMacro.h"

BEGIN(Engine)

ENUM()
enum class EPipelineType
{
	Graphics,
	Compute,
};

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
enum class EFillMode
{
	Wireframe,
	Solid,
};

ENUM()
enum class ECullMode
{
	None,
	Front,
	Back,
};

ENUM()
enum class EFrontFace
{
	Clockwise,
	CounterClockwise,
};

ENUM()
enum class EDepthMode
{
	None,
	ReadOnly,
	ReadWrite,
};

ENUM()
enum class ETopology
{
	TriangleList,
	TriangleStrip,
	LineList,
	LineStrip,
	PointList,
};

ENUM()
enum class ECompareOp
{
	Never,
	Less,
	Equal,
	LessOrEqual,
	Greater,
	NotEqual,
	GreaterOrEqual,
	Always
};

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
struct tagBlendState
{
	REFLECT_STRUCT()

	PROPERTY(EDITABLE)
	EBlendFactor SrcColor = EBlendFactor::One;
	PROPERTY(EDITABLE)
	EBlendFactor DstColor = EBlendFactor::Zero;
	PROPERTY(EDITABLE)
	EBlendOp ColorBlendOp = EBlendOp::Add;
	PROPERTY(EDITABLE)
	EBlendFactor SrcAlpha = EBlendFactor::One;
	PROPERTY(EDITABLE)
	EBlendFactor DstAlpha = EBlendFactor::Zero;
	PROPERTY(EDITABLE)
	EBlendOp AlphaBlendOp = EBlendOp::Add;
	PROPERTY(EDITABLE)
	EColorChannel ColorWriteMask = EColorChannel::RGBA;

	PROPERTY(EDITABLE)
	bool Enable = false;
	PROPERTY(EDITABLE)
	bool EnableColorWriteMask = true;

	bool operator==(const tagBlendState&) const = default;

	tagBlendState(EBlendMode mode = EBlendMode::None)
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
			SrcColor = EBlendFactor::SrcAlpha;
			DstColor = EBlendFactor::InvSrcAlpha;
			ColorBlendOp = EBlendOp::Add;
			SrcAlpha = EBlendFactor::SrcAlpha;
			DstAlpha = EBlendFactor::InvSrcAlpha;
			AlphaBlendOp = EBlendOp::Add;
			Enable = true;
			break;
		case EBlendMode::Additive:
			SrcColor = EBlendFactor::SrcAlpha;
			DstColor = EBlendFactor::One;
			ColorBlendOp = EBlendOp::Add;
			SrcAlpha = EBlendFactor::One;
			DstAlpha = EBlendFactor::One;
			AlphaBlendOp = EBlendOp::Add;
			ColorWriteMask = EColorChannel::RGBA;
			Enable = true;
			break;
		case EBlendMode::NonPremultiplied:
			SrcColor = EBlendFactor::SrcAlpha;
			DstColor = EBlendFactor::InvSrcAlpha;
			ColorBlendOp = EBlendOp::Add;
			SrcAlpha = EBlendFactor::One;
			DstAlpha = EBlendFactor::InvSrcAlpha;
			AlphaBlendOp = EBlendOp::Add;
			ColorWriteMask = EColorChannel::RGBA;
			Enable = true;
			break;
		case EBlendMode::Decal:
			SrcColor = EBlendFactor::SrcAlpha;
			DstColor = EBlendFactor::InvSrcAlpha;
			ColorBlendOp = EBlendOp::Add;
			SrcAlpha = EBlendFactor::SrcAlpha;
			DstAlpha = EBlendFactor::InvSrcAlpha;
			AlphaBlendOp = EBlendOp::Add;
			ColorWriteMask = EColorChannel::RGBA;  // 알파 채널 안 건드림
			Enable = true;
			break;
		}
	}
};
END