#pragma once

#include "Primitives.h"

#pragma region Constructor&Destructor
EResult TriangleMesh::Initialize(void* arg)
{
	vector<Vertex> vertices(3);
	vertices[0] = { vec3(0.0f,  0.5f, 0.f), vec3(0,0,-1), vec2(0.5f, 0.0f), vec3(1,0,0), vec3(0,1,0), vec4(1,0,0,1) };
	vertices[1] = { vec3(0.5f, -0.5f, 0.f), vec3(0,0,-1), vec2(1.0f, 1.0f), vec3(1,0,0), vec3(0,1,0), vec4(0,1,0,1) };
	vertices[2] = { vec3(-0.5f, -0.5f, 0.f), vec3(0,0,-1), vec2(0.0f, 1.0f), vec3(1,0,0), vec3(0,1,0), vec4(0,0,1,1) };

	vector<uint32> indices = { 0, 1, 2 };

	DESC desc = {};
	desc.VertexData = vertices.data();
	desc.VertexCount = static_cast<uint32>(vertices.size());
	desc.VertexStride = sizeof(Vertex);
	desc.IndexData = indices.data();
	desc.IndexCount = static_cast<uint32>(indices.size());
	desc.IndexStride = sizeof(uint32);

	return Mesh::Initialize(&desc);
}

TriangleMesh* TriangleMesh::Create(void* arg)
{
	TriangleMesh* mesh = new TriangleMesh{};
	if (mesh->Initialize(arg) != EResult::Success)
	{
		mesh->Release();
		return nullptr;
	}
	return mesh;
}
EResult QuadMesh::Initialize(void* arg)
{
	vector<Vertex> vertices(4);

	vertices[0] = { vec3(-0.5f,  0.5f, 0.f), vec3(0,0,-1), vec2(0,0), vec3(1,0,0), vec3(0,1,0), vec4(1,0,0,1) }; // TopLeft
	vertices[1] = { vec3(0.5f,  0.5f, 0.f), vec3(0,0,-1), vec2(1,0), vec3(1,0,0), vec3(0,1,0), vec4(0,1,0,1) }; // TopRight
	vertices[2] = { vec3(0.5f, -0.5f, 0.f), vec3(0,0,-1), vec2(1,1), vec3(1,0,0), vec3(0,1,0), vec4(0,0,1,1) }; // BotRight
	vertices[3] = { vec3(-0.5f, -0.5f, 0.f), vec3(0,0,-1), vec2(0,1), vec3(1,0,0), vec3(0,1,0), vec4(1,1,0,1) }; // BotLeft

	vector<uint32> indices = { 0, 1, 2, 0, 2, 3 };

	DESC desc = {};
	desc.VertexData = vertices.data();
	desc.VertexCount = static_cast<uint32>(vertices.size());
	desc.VertexStride = sizeof(Vertex);
	desc.IndexData = indices.data();
	desc.IndexCount = static_cast<uint32>(indices.size());
	desc.IndexStride = sizeof(uint32);

	return Mesh::Initialize(&desc);
}

QuadMesh* QuadMesh::Create(void* arg)
{
	QuadMesh* mesh = new QuadMesh{};
	if (mesh->Initialize(arg) != EResult::Success)
	{
		mesh->Release();
		return nullptr;
	}
	return mesh;
}
EResult CubeMesh::Initialize(void* arg)
{
	vector<Vertex> vertices(8);
	vector<uint32> indices;

	auto AddFace = [&](vec3 c, vec3 r, vec3 u, vec3 n, vec4 color)
		{
			uint32 baseIdx = static_cast<uint32>(vertices.size());

			// 네 모서리 계산
			vec3 tl = c - r + u;
			vec3 tr = c + r + u;
			vec3 br = c + r - u;
			vec3 bl = c - r - u;

			// 정점 추가
			vertices.push_back({ tl, n, vec2(0,0), r, u, color });
			vertices.push_back({ tr, n, vec2(1,0), r, u, color });
			vertices.push_back({ br, n, vec2(1,1), r, u, color });
			vertices.push_back({ bl, n, vec2(0,1), r, u, color });

			// 인덱스 추가 (삼각형 2개)
			indices.push_back(baseIdx + 0);
			indices.push_back(baseIdx + 1);
			indices.push_back(baseIdx + 2);
			indices.push_back(baseIdx + 0);
			indices.push_back(baseIdx + 2);
			indices.push_back(baseIdx + 3);
		};

	vec3 p(0.5f); // Half Extents

	// 6개 면 생성 (Front, Back, Right, Left, Top, Bottom)
	AddFace(vec3(0, 0, -p.z), vec3(p.x, 0, 0), vec3(0, p.y, 0), vec3(0, 0, -1), vec4(1, 0, 0, 1)); // Front
	AddFace(vec3(0, 0, p.z), vec3(-p.x, 0, 0), vec3(0, p.y, 0), vec3(0, 0, 1), vec4(0, 1, 0, 1));  // Back
	AddFace(vec3(p.x, 0, 0), vec3(0, 0, -p.z), vec3(0, p.y, 0), vec3(1, 0, 0), vec4(0, 0, 1, 1));  // Right
	AddFace(vec3(-p.x, 0, 0), vec3(0, 0, p.z), vec3(0, p.y, 0), vec3(-1, 0, 0), vec4(1, 1, 0, 1)); // Left
	AddFace(vec3(0, p.y, 0), vec3(p.x, 0, 0), vec3(0, 0, -p.z), vec3(0, 1, 0), vec4(0, 1, 1, 1));  // Top
	AddFace(vec3(0, -p.y, 0), vec3(p.x, 0, 0), vec3(0, 0, p.z), vec3(0, -1, 0), vec4(1, 0, 1, 1)); // Bottom

	DESC desc = {};
	desc.VertexData = vertices.data();
	desc.VertexCount = static_cast<uint32>(vertices.size());
	desc.VertexStride = sizeof(Vertex);
	desc.IndexData = indices.data();
	desc.IndexCount = static_cast<uint32>(indices.size());
	desc.IndexStride = sizeof(uint32);

	return Mesh::Initialize(&desc);
}

CubeMesh* CubeMesh::Create(void* arg)
{
	CubeMesh* mesh = new CubeMesh{};
	if (mesh->Initialize(arg) != EResult::Success)
	{
		mesh->Release();
		return nullptr;
	}
	return mesh;
}
EResult SphereMesh::Initialize(void* arg)
{
	return EResult::Success;
}

SphereMesh* SphereMesh::Create(void* arg)
{
	SphereMesh* mesh = new SphereMesh{};
	if (mesh->Initialize(arg) != EResult::Success)
	{
		mesh->Release();
		return nullptr;
	}
	return mesh;
}
#pragma endregion
