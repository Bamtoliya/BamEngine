#pragma once

#include "RHIBuffer.h"
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

	vec3 BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	vec3 BoundingBoxMax = { 0.0f, 0.0f, 0.0f };
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
public:
	ETopology GetTopology() const { return m_Topology; }
public:
	virtual const tagInputLayoutDesc GetInputLayoutDesc() const { return Vertex::Layout; }
#pragma endregion

#pragma region Setter
public:
	EResult SetVertexBuffer(const void* data, uint32 vertexCount);
	EResult SetIndexBuffer(const void* data	, uint32 indexCount);
public:
	void SetTopology(ETopology topology) { m_Topology = topology; }
#pragma endregion


#pragma region Bounds
public:
	vec3 GetMin() const { return m_BoundingBoxMin; }
	vec3 GetMax() const { return m_BoundingBoxMax; }
	vec3 GetCenter() const { return (m_BoundingBoxMin + m_BoundingBoxMax) * 0.5f; }
	vec3 GetExtents() const { return (m_BoundingBoxMax - m_BoundingBoxMin) * 0.5f; }
#pragma endregion


#pragma region Variable
protected:
	RHIBuffer* m_VertexBuffer = { nullptr };
	RHIBuffer* m_IndexBuffer = { nullptr };


	//Input Layout
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	ETopology m_Topology = { ETopology::TriangleList };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	uint32 m_VertexCount = { 0 };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	uint32 m_IndexCount = { 0 };

	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	vec3 m_BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	vec3 m_BoundingBoxMax = { 0.0f, 0.0f, 0.0f };
#pragma endregion
};
END