#pragma once

#include "Vertex.h"

const InputLayoutDesc VertexPosition::Layout =
{
	{
		{ "POSITION",	0, 0, EVertexElementFormat::Float3, offsetof(VertexPosition, position)},
	},
	sizeof(VertexPosition),
	EVertexInputRate::PerVertex,
	0
};

const InputLayoutDesc VertexMaterial::Layout =
{
	{
		{ "NORMAL",		0, 0, EVertexElementFormat::Float3, offsetof(VertexMaterial, normal) },
		{ "TEXCOORD",	0, 1, EVertexElementFormat::Float2, offsetof(VertexMaterial, texCoord) },
		{ "TANGENT",	0, 2, EVertexElementFormat::Float3, offsetof(VertexMaterial, tangent) },
		{ "BITANGENT",	0, 3, EVertexElementFormat::Float3, offsetof(VertexMaterial, bitangent) },
		{ "COLOR",		0, 4, EVertexElementFormat::Float4, offsetof(VertexMaterial, color) }
	},
	sizeof(VertexMaterial),
	EVertexInputRate::PerVertex,
	0
};

const InputLayoutDesc VertexSkinData::Layout =
{
	{
		{ "BONEIDS",	0, 0, EVertexElementFormat::UInt4, offsetof(VertexSkinData, boneIDs) },
		{ "WEIGHTS",	0, 1, EVertexElementFormat::Float4, offsetof(VertexSkinData, weights) }
	},
	sizeof(VertexSkinData),
	EVertexInputRate::PerVertex,
	0
};

const InputLayoutDesc Vertex::Layout =
{
	{
		{ "POSITION",	0, 0, EVertexElementFormat::Float3, offsetof(Vertex, position) },
		{ "NORMAL",		0, 1, EVertexElementFormat::Float3, offsetof(Vertex, normal) },
		{ "TEXCOORD",	0, 2, EVertexElementFormat::Float2, offsetof(Vertex, texCoord) },
		{ "TANGENT",	0, 3, EVertexElementFormat::Float3, offsetof(Vertex, tangent) },
		{ "BITANGENT",	0, 4, EVertexElementFormat::Float3, offsetof(Vertex, bitangent) },
		{ "COLOR",		0, 5, EVertexElementFormat::Float4, offsetof(Vertex, color) }
	},
	sizeof(Vertex),
	EVertexInputRate::PerVertex,
	0
};

const InputLayoutDesc Vertex2D::Layout =
{
	{
		{ "POSITION",	0, 0, EVertexElementFormat::Float3, offsetof(Vertex2D, position) },
		{ "COLOR",		0, 1, EVertexElementFormat::Float4, offsetof(Vertex2D, color) },
		{ "TEXCOORD",	0, 2, EVertexElementFormat::Float2, offsetof(Vertex2D, texCoord) }
	},
	sizeof(Vertex2D),
	EVertexInputRate::PerVertex,
	0
};

#ifdef _DEBUG
const InputLayoutDesc DebugVertex::Layout =
{
	{
		{ "POSITION",	0, 0, EVertexElementFormat::Float3, offsetof(DebugVertex, position) },
		{ "COLOR",		0, 1, EVertexElementFormat::Float4, offsetof(DebugVertex, color) }
	},
	sizeof(DebugVertex),
	EVertexInputRate::PerVertex,
	0
};
#endif