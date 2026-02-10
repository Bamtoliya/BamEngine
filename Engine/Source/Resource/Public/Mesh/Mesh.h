#pragma once

#include "RHIBuffer.h"
#include "Vertex.h"
#include "Resource.h"

BEGIN(Engine)

struct tagMeshCreateInfo
{
	void* VertexData = { nullptr };
	uint32 VertexCount = { 0 };
	uint32 VertexStride = { 0 };

	void* IndexData = { nullptr };
	uint32 IndexCount = { 0 };
	uint32 IndexStride = { 0 };
};

CLASS()
class ENGINE_API Mesh : public Resource
{
	REFLECT_CLASS(Mesh)
#pragma region Constructor&Destructor
protected:
	using DESC = tagMeshCreateInfo;
	Mesh() {}
	virtual ~Mesh() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Mesh* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
public:
	EResult Bind(uint32 slot) override;
#pragma endregion


#pragma region Getter
public:
	RHIBuffer* GetVertexBuffer() const { return m_VertexBuffer; }
	RHIBuffer* GetIndexBuffer() const { return m_IndexBuffer; }
public:
	uint32 GetVertexCount() const { return m_VertexCount; }
	uint32 GetIndexCount() const { return m_IndexCount; }
#pragma endregion

#pragma region Setter
public:
	EResult SetVertexBuffer(const void* data, uint32 vertexCount);
	EResult SetIndexBuffer(const void* data	, uint32 indexCount);
#pragma endregion

#pragma region Variable
protected:
	RHIBuffer* m_VertexBuffer = { nullptr };
	RHIBuffer* m_IndexBuffer = { nullptr };


	PROPERTY(CATEGORY("PROP_INFORMATION"), READONLY)
	uint32 m_VertexCount = { 0 };
	PROPERTY(CATEGORY("PROP_INFORMATION"), READONLY)
	uint32 m_IndexCount = { 0 };
#pragma endregion
};
END