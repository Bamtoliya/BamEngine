#pragma once

#include "Mesh.h"

BEGIN(Engine)


class ENGINE_API TriangleMesh final : public Mesh
{
#pragma region Constructor&Destructor
private:
	TriangleMesh() {}
	virtual ~TriangleMesh() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static TriangleMesh* Create(void* arg = nullptr);
#pragma endregion
};

class ENGINE_API QuadMesh final : public Mesh
{
#pragma region Constructor&Destructor
	private:
	QuadMesh() {}
	virtual ~QuadMesh() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static QuadMesh* Create(void* arg = nullptr);
#pragma endregion
};

class ENGINE_API CubeMesh final : public Mesh
{
#pragma region Constructor&Destructor
private:
	CubeMesh() {}
	virtual ~CubeMesh() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static CubeMesh* Create(void* arg = nullptr);
#pragma endregion
};

class ENGINE_API SphereMesh final : public Mesh
{
#pragma region Constructor&Destructor
private:
	SphereMesh() {}
	virtual ~SphereMesh() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static SphereMesh* Create(void* arg = nullptr);
#pragma endregion
};

END