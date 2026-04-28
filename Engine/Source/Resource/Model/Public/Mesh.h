#pragma once

#include "RHIBuffer.h"
#include "Resource.h"
#include "Vertex.h"

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

enum class EMeshStream : uint32
{
	Position = 0,
	Material = 1,
	SkinData = 2,
	Max = MAX_BUFFER_SLOTS // RHI.h에 정의된 4 사용
};

struct tagMeshStreamDesc
{
	void* Data = nullptr;
	uint32 Count = 0;
	uint32 Stride = 0;
};


ENABLE_BITMASK_OPERATORS(EMeshFlag);

struct tagMeshCreateDesc : public tagResourceCreateDesc
{
	tagMeshStreamDesc Streams[(uint32)EMeshStream::Max] = {};

	void* IndexData = { nullptr };
	uint32 IndexCount = { 0 };
	uint32 IndexStride = { 0 };

	vec3 BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	vec3 BoundingBoxMax = { 0.0f, 0.0f, 0.0f };

	EMeshFlag Flags = { EMeshFlag::None };
};


struct tagMeshBinaryHeader
{
	uint32 StreamCounts[(uint32)EMeshStream::Max] = { 0 };
	uint32 StreamStrides[(uint32)EMeshStream::Max] = { 0 };

	uint32 IndexCount = { 0 };
	uint32 IndexStride = { 0 };

	vec3 BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	vec3 BoundingBoxMax = { 0.0f, 0.0f, 0.0f };

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
	RHIBuffer* GetStreamBuffer(EMeshStream stream) const { return m_VertexBuffers[(uint32)stream]; }
	uint32 GetStreamCount(EMeshStream stream) const { return m_StreamCounts[(uint32)stream]; }
	uint32 GetStreamStride(EMeshStream stream) const { return m_StreamStrides[(uint32)stream]; }
	RHIBuffer* GetPositionBuffer() const { return GetStreamBuffer(EMeshStream::Position); }
	RHIBuffer* GetMaterialBuffer() const { return GetStreamBuffer(EMeshStream::Material); }
	RHIBuffer* GetSkinDataBuffer() const { return GetStreamBuffer(EMeshStream::SkinData); }
	RHIBuffer* GetIndexBuffer() const { return m_IndexBuffer; }
	uint32 GetVertexCount() const { return GetStreamCount(EMeshStream::Position); }
	uint32 GetIndexCount() const { return m_IndexCount; }
	uint32 GetActiveBufferCount() const { return m_ActiveBufferCount; }
public:
	ETopology GetTopology() const { return m_Topology; }
public:
	virtual const vector<tagInputLayoutDesc> GetInputLayoutDescs() const;
	bool IsDynamic() const { return HasFlag(m_Flags, EMeshFlag::Dynamic); }
#pragma endregion

#pragma region Setter
public:
	EResult SetStreamBuffer(EMeshStream stream, void* data, uint32 count, uint32 stride);
	EResult SetIndexBuffer(void* data, uint32 indexCount, uint32 stride);
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

private:
	// 캐싱된 버퍼 배열을 다시 구축하는 내부 헬퍼
	void RebuildCachedBuffers();

#pragma region Variable
protected:
	EMeshFlag m_Flags = { EMeshFlag::None };

	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	ETopology m_Topology = { ETopology::TriangleList };

	uint32 m_StreamCounts[(uint32)EMeshStream::Max] = { 0 };
	uint32 m_StreamStrides[(uint32)EMeshStream::Max] = { 0 };
	RHIBuffer* m_VertexBuffers[(uint32)EMeshStream::Max] = { nullptr };
	vector<uint8> m_RawData[(uint32)EMeshStream::Max];

	RHIBuffer* m_IndexBuffer = { nullptr };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	uint32 m_IndexCount = { 0 };
	uint32 m_IndexStride = { 0 };
	vector<uint8> m_IndexRaw;

	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	vec3 m_BoundingBoxMin = { 0.0f, 0.0f, 0.0f };
	PROPERTY(CATEGORY(L"PROP_INFORMATION"), READONLY)
	vec3 m_BoundingBoxMax = { 0.0f, 0.0f, 0.0f };

	uint32 m_ActiveBufferCount = { 0 };
#pragma endregion
};
END