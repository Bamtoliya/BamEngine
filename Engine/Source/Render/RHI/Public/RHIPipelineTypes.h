#pragma once

#include "Base.h"
#include "Reflection/ReflectionMacro.h"

BEGIN(Engine)

ENUM()
enum class EPipelineType
{
	Graphics,
	Compute,
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

ENUM()
enum class EStencilOp
{
	Keep,           // 현재 값 그대로 유지 (제일 많이 쓰임)
	Zero,           // 0으로 덮어쓰기
	Replace,        // 레퍼런스 값으로 덮어쓰기
	IncrementClamp, // 1 증가 (255에서 멈춤)
	DecrementClamp, // 1 감소 (0에서 멈춤)
	Invert,         // 비트 반전 (~연산)
	IncrementWrap,  // 1 증가 (255 넘으면 0으로 돌아감)
	DecrementWrap   // 1 감소 (0보다 작아지면 255로 돌아감)
};
END