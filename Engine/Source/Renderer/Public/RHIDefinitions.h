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
	Opaque,
	AlphaBlend,
	Additive,
	NonPremultiplied,
	Masked,
};

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

END