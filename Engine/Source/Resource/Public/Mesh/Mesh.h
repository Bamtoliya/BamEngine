#pragma once

#include "RHIBuffer.h"
#include "Vertex.h"

BEGIN(Engine)
typedef struct tagMeshCreateInfo
{
	void* VertexData = { nullptr };
	uint32 VertexCount = { 0 };
	uint32 VertexStride = { 0 };

	void* IndexData = { nullptr };
	uint32 IndexCount = { 0 };
	uint32 IndexStride = { 0 };
} MESHDESC;
class ENGINE_API Mesh final : public Base
{
#pragma region Constructor&Destructor
private:
	Mesh() {}
	virtual ~Mesh() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Mesh* Create(void* arg = nullptr);
	Mesh* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Getter
public:
	RHIBuffer* GetVertexBuffer() const { return m_VertexBuffer; }
	RHIBuffer* GetIndexBuffer() const { return m_IndexBuffer; }
public:
	uint32 GetVertexCount() const { return m_VertexCount; }
	uint32 GetIndexCount() const { return m_IndexCount; }
#pragma endregion


#pragma region Variable
private:
	RHIBuffer* m_VertexBuffer = { nullptr };
	RHIBuffer* m_IndexBuffer = { nullptr };

	uint32 m_VertexCount = { 0 };
	uint32 m_IndexCount = { 0 };
#pragma endregion
};
END