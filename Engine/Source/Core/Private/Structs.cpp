#pragma once

#include "Structs.h"

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
	sizeof(Vertex)
};

const tagInputLayoutDesc Vertex2D::Layout =
{
	{
		{ 0, EVertexElementFormat::Float3, offsetof(Vertex2D, position) },
		{ 1, EVertexElementFormat::Float4, offsetof(Vertex2D, color) },
		{ 2, EVertexElementFormat::Float2, offsetof(Vertex2D, texCoord) }
	},
	sizeof(Vertex2D)
};

const tagInputLayoutDesc VertexSkinData::Layout =
{
	{
		{ 0, EVertexElementFormat::UInt4, offsetof(VertexSkinData, boneIDs) },
		{ 1, EVertexElementFormat::Float4, offsetof(VertexSkinData, weights) }
	},
	sizeof(VertexSkinData)
};

#ifdef _DEBUG
const tagInputLayoutDesc DebugVertex::Layout =
{
	{
		{ 0, EVertexElementFormat::Float3, offsetof(DebugVertex, position) },
		{ 1, EVertexElementFormat::Float4, offsetof(DebugVertex, color) }
	},
	sizeof(DebugVertex)
};
#endif