#pragma once

#include "RHIBuffer.h"
#include "Resource.h"

BEGIN(Engine)

ENUM()
enum class EMeshFlag : uint8
{
	None = 0,
	HasPosition = 1 << 0,
	HasNormal = 1 << 1,
	HasTexCoord = 1 << 2,
	HasTangent = 1 << 3,
	HasBitangent = 1 << 4,
	HasColor = 1 << 5,
	Dynamic = 1 << 6,
	KeepRawData = 1 << 7,
};

ENABLE_BITMASK_OPERATORS(EMeshFlag);

struct tagMeshCreateDesc : public tagResourceCreateDesc
{
	void* VertexData = { nullptr };
	uint32 VertexCount = { 0 };
	uint32 VertexStride = { 0 };

	void* SkinData = { nullptr };
	uint32 SkinDataCount = { 0 };
	uint32 SkinDataStride = { 0 };

	void* IndexData = { nullptr };
	uint32 IndexCount = { 0 };
	uint32 IndexStride = { 0 };

	vec3 BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	vec3 BoundingBoxMax = { 0.0f, 0.0f, 0.0f };

	EMeshFlag Flags = { EMeshFlag::None };
};


STRUCT()
struct tagMeshBinaryHeader
{
	REFLECT_STRUCT()

	PROPERTY()
	uint32 VertexCount = { 0 };
	PROPERTY()
	uint32 VertexStride = { 0 };

	PROPERTY()
	uint32 SkinDataCount = { 0 };
	PROPERTY()
	uint32 SkinDataStride = { 0 };
	PROPERTY()
	uint32 IndexCount = { 0 };
	PROPERTY()
	uint32 IndexStride = { 0 };
	
	PROPERTY()
	vec3 BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	PROPERTY()
	vec3 BoundingBoxMax = { 0.0f, 0.0f, 0.0f };
	PROPERTY()
	EMeshFlag Flags = { EMeshFlag::None };
};

CLASS()
class ENGINE_API Mesh : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Mesh)
#pragma region Constructor&Destructor
protected:
	using DESC = tagMeshCreateDesc;
	Mesh() : Resource(EResourceType::Mesh) {}
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
	bool IsDynamic() const { return HasFlag(m_Flags, EMeshFlag::Dynamic); }
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


#pragma region Save&Load
public:
	virtual void Serialize(Archive& ar) override;
	virtual void Deserialize(Archive& ar) override;
#pragma endregion



#pragma region Variable
protected:
	RHIBuffer* m_VertexBuffer = { nullptr };
	RHIBuffer* m_SkinDataBuffer = { nullptr };
	RHIBuffer* m_IndexBuffer = { nullptr };

	uint32 m_VertexStride = 0;
	uint32 m_SkinDataStride = 0;
	uint32 m_IndexStride = 0;

	vector<uint8> m_VertexRaw;
	vector<uint8> m_SkinRaw;
	vector<uint8> m_IndexRaw;


	//Input Layout
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	ETopology m_Topology = { ETopology::TriangleList };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	uint32 m_VertexCount = { 0 };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	uint32 m_IndexCount = { 0 };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	uint32 m_SkinDataCount = { 0 };

	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	vec3 m_BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	vec3 m_BoundingBoxMax = { 0.0f, 0.0f, 0.0f };

	EMeshFlag m_Flags = { EMeshFlag::None };
#pragma endregion
};
END