#pragma once

#include "Vertex.h"

const tagInputLayoutDesc VertexPosition::Layout =
{
	{
		{  0, EVertexElementFormat::Float3, offsetof(VertexPosition, position) },
	},
	sizeof(VertexPosition),
	EVertexInputRate::PerVertex,
	0
};

const tagInputLayoutDesc VertexMaterial::Layout =
{
	{
		{  0, EVertexElementFormat::Float3, offsetof(VertexMaterial, normal) },
		{  1, EVertexElementFormat::Float2, offsetof(VertexMaterial, texCoord) },
		{  2, EVertexElementFormat::Float3, offsetof(VertexMaterial, tangent) },
		{  3, EVertexElementFormat::Float3, offsetof(VertexMaterial, bitangent) },
		{  4, EVertexElementFormat::Float4, offsetof(VertexMaterial, color) }
	},
	sizeof(VertexMaterial),
	EVertexInputRate::PerVertex,
	0
};

const tagInputLayoutDesc VertexSkinData::Layout =
{
	{
		{ 0, EVertexElementFormat::UInt4, offsetof(VertexSkinData, boneIDs) },
		{ 1, EVertexElementFormat::Float4, offsetof(VertexSkinData, weights) }
	},
	sizeof(VertexSkinData),
	EVertexInputRate::PerVertex,
	0
};

const tagInputLayoutDesc Vertex::Layout =
{
	{
		{  0, EVertexElementFormat::Float3, offsetof(Vertex, position) },
		{  1, EVertexElementFormat::Float3, offsetof(Vertex, normal) },
		{  2, EVertexElementFormat::Float2, offsetof(Vertex, texCoord) },
		{  3, EVertexElementFormat::Float3, offsetof(Vertex, tangent) },
		{  4, EVertexElementFormat::Float3, offsetof(Vertex, bitangent) },
		{  5, EVertexElementFormat::Float4, offsetof(Vertex, color) }
	},
	sizeof(Vertex),
	EVertexInputRate::PerVertex,
	0
};

const tagInputLayoutDesc Vertex2D::Layout =
{
	{
		{ 0, EVertexElementFormat::Float3, offsetof(Vertex2D, position) },
		{ 1, EVertexElementFormat::Float4, offsetof(Vertex2D, color) },
		{ 2, EVertexElementFormat::Float2, offsetof(Vertex2D, texCoord) }
	},
	sizeof(Vertex2D),
	EVertexInputRate::PerVertex,
	0
};

#ifdef _DEBUG
const tagInputLayoutDesc DebugVertex::Layout =
{
	{
		{ 0, EVertexElementFormat::Float3, offsetof(DebugVertex, position) },
		{ 1, EVertexElementFormat::Float4, offsetof(DebugVertex, color) }
	},
	sizeof(DebugVertex),
	EVertexInputRate::PerVertex,
	0
};
#endif