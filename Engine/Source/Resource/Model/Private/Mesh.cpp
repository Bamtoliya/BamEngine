#pragma once
#include "Mesh.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SerializationHelper.h"

#pragma region Constructor&Destructor
EResult Mesh::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	CAST_DESC

		RHI* rhi = Renderer::Get().GetRHI();

	// 1. 모든 버텍스 스트림을 루프 하나로 처리
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		const auto& stream = desc->Streams[i];
		m_StreamCounts[i] = stream.Count;
		m_StreamStrides[i] = stream.Stride;

		if (stream.Data && stream.Count > 0 && stream.Stride > 0)
		{
			uint32 totalSize = stream.Stride * stream.Count;
			m_VertexBuffers[i] = rhi->CreateBuffer(stream.Data, totalSize, stream.Stride, ERHIBufferType::Vertex);
			if (!m_VertexBuffers[i]) return EResult::Fail;
		}
	}

	// 2. Index Buffer 생성
	m_IndexCount = desc->IndexCount;
	m_IndexStride = desc->IndexStride;
	if (desc->IndexData && m_IndexCount > 0)
	{
		uint32 totalSize = desc->IndexStride * m_IndexCount;
		m_IndexBuffer = rhi->CreateBuffer(desc->IndexData, totalSize, desc->IndexStride, ERHIBufferType::Index);
		if (!m_IndexBuffer) return EResult::Fail;
	}

	// 3. Bounding Box & Flags
	m_BoundingBoxMin = desc->BoundingBoxMin;
	m_BoundingBoxMax = desc->BoundingBoxMax;
	m_Flags = desc->Flags;

	// 4. 원본 데이터 보존 (KeepRawData 플래그)
	if (HasFlag(m_Flags, EMeshFlag::KeepRawData))
	{
		for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
		{
			const auto& stream = desc->Streams[i];
			if (stream.Data && stream.Count > 0)
			{
				uint32 bytes = stream.Stride * stream.Count;
				m_RawData[i].resize(bytes);
				memcpy(m_RawData[i].data(), stream.Data, bytes);
			}
		}
		if (desc->IndexData && m_IndexCount > 0)
		{
			uint32 bytes = m_IndexStride * m_IndexCount;
			m_IndexRaw.resize(bytes);
			memcpy(m_IndexRaw.data(), desc->IndexData, bytes);
		}
	}

	// 5. Bind용 캐싱 배열 구축
	RebuildCachedBuffers();

	return EResult::Success;
}

Mesh* Mesh::Create(void* arg)
{
	Mesh* instance = new Mesh();
	if (IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

void Mesh::Free()
{
	for (auto& buffer : m_VertexBuffers)
	{
		Safe_Release(buffer);
	}
	Safe_Release(m_IndexBuffer);

	for (auto& raw : m_RawData)
	{
		raw.clear();
	}
	m_IndexRaw.clear();
}
#pragma endregion

#pragma region Bind
EResult Mesh::Bind(uint32 slot)
{
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return EResult::Fail;

	// 캐싱된 배열을 그대로 던진다 → if문 0개, 루프 0개
	if (m_ActiveBufferCount > 0)
		rhi->BindVertexBuffers(0, m_VertexBuffers, m_ActiveBufferCount);

	if (m_IndexBuffer)
		rhi->BindIndexBuffer(m_IndexBuffer);

	return EResult::Success;
}
#pragma endregion

#pragma region Getter
static const tagInputLayoutDesc* s_StreamLayoutTable[] = {
	&VertexPosition::Layout,   // EMeshStream::Position = 0
	&VertexMaterial::Layout,   // EMeshStream::Material = 1
	&VertexSkinData::Layout,   // EMeshStream::SkinData = 2
	nullptr                    // 여분 슬롯 (아직 미정의)
};
const vector<tagInputLayoutDesc> Mesh::GetInputLayoutDescs() const
{
	vector<tagInputLayoutDesc> result;
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		// 해당 슬롯에 실제로 버퍼가 존재하고, 매핑 테이블에 레이아웃이 정의되어 있을 때만 추가
		if (m_VertexBuffers[i] && s_StreamLayoutTable[i])
		{
			result.push_back(*s_StreamLayoutTable[i]);
		}
	}
	return result;
}
#pragma endregion


#pragma region Setter
EResult Mesh::SetStreamBuffer(EMeshStream stream, void* data, uint32 count, uint32 stride)
{
	uint32 idx = (uint32)stream;
	if (idx >= (uint32)EMeshStream::Max || !data || count == 0) return EResult::InvalidArgument;

	RHI* rhi = Renderer::Get().GetRHI();
	uint32 totalSize = stride * count;

	// 기존 버퍼가 있으면 해제 후 재생성
	Safe_Release(m_VertexBuffers[idx]);
	m_VertexBuffers[idx] = rhi->CreateBuffer(data, totalSize, stride, ERHIBufferType::Vertex);
	m_StreamCounts[idx] = count;
	m_StreamStrides[idx] = stride;

	RebuildCachedBuffers();
	return m_VertexBuffers[idx] ? EResult::Success : EResult::Fail;
}

EResult Mesh::SetIndexBuffer(void* data, uint32 indexCount, uint32 stride)
{
	if (!data || indexCount == 0) return EResult::InvalidArgument;

	RHI* rhi = Renderer::Get().GetRHI();
	uint32 totalSize = stride * indexCount;

	Safe_Release(m_IndexBuffer);
	m_IndexBuffer = rhi->CreateBuffer(data, totalSize, stride, ERHIBufferType::Index);
	m_IndexCount = indexCount;
	m_IndexStride = stride;

	return m_IndexBuffer ? EResult::Success : EResult::Fail;
}
#pragma endregion

#pragma region Internal
void Mesh::RebuildCachedBuffers()
{
	// m_VertexBuffers 배열에서 nullptr이 아닌 마지막 인덱스를 찾아서
	// SDL_BindGPUVertexBuffers에 넘길 개수를 확정합니다.
	// 중간에 빈 슬롯이 있어도 SDL_GPU는 순서대로 슬롯 바인딩하므로,
	// "가장 높은 활성 슬롯 + 1"이 넘길 개수입니다.
	m_ActiveBufferCount = 0;
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		if (m_VertexBuffers[i])
			m_ActiveBufferCount = i + 1;
	}
}
#pragma endregion

#pragma region Save&Load

static const char* s_StreamNames[] = { "PositionData", "MaterialData", "SkinData" };

void Mesh::Serialize(Archive& ar)
{
	Resource::Serialize(ar);

	// 1. 헤더 구성
	tagMeshBinaryHeader header = {};
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		header.StreamCounts[i] = m_StreamCounts[i];
		header.StreamStrides[i] = m_StreamStrides[i];
	}
	header.IndexCount = m_IndexCount;
	header.IndexStride = m_IndexStride;
	header.BoundingBoxMin = m_BoundingBoxMin;
	header.BoundingBoxMax = m_BoundingBoxMax;
	header.Flags = m_Flags;

	// 2. 헤더를 통째로 바이너리 직렬화
	ar.ProcessRaw("MeshHeader", &header, sizeof(tagMeshBinaryHeader));

	// 3. 스트림별 원본 데이터 직렬화
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		if (m_RawData[i].size() > 0)
			ar.ProcessRaw(s_StreamNames[i], m_RawData[i].data(), m_RawData[i].size());
	}

	// 4. 인덱스 데이터 직렬화
	if (m_IndexRaw.size() > 0)
		ar.ProcessRaw("IndexData", m_IndexRaw.data(), m_IndexRaw.size());
}

void Mesh::Deserialize(Archive& ar)
{
	Resource::Deserialize(ar);

	// 1. 헤더를 통째로 바이너리 역직렬화
	tagMeshBinaryHeader header = {};
	ar.ProcessRaw("MeshHeader", &header, sizeof(tagMeshBinaryHeader));

	// 2. 헤더에서 멤버로 복사
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		m_StreamCounts[i] = header.StreamCounts[i];
		m_StreamStrides[i] = header.StreamStrides[i];
	}
	m_IndexCount = header.IndexCount;
	m_IndexStride = header.IndexStride;
	m_BoundingBoxMin = header.BoundingBoxMin;
	m_BoundingBoxMax = header.BoundingBoxMax;
	m_Flags = header.Flags;

	// 3. 스트림별 원본 데이터 역직렬화
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		uint32 bytes = m_StreamStrides[i] * m_StreamCounts[i];
		if (bytes > 0)
		{
			m_RawData[i].resize(bytes);
			ar.ProcessRaw(s_StreamNames[i], m_RawData[i].data(), m_RawData[i].size());
		}
	}

	// 4. 인덱스 데이터 역직렬화
	uint32 indexBytes = m_IndexStride * m_IndexCount;
	if (indexBytes > 0)
	{
		m_IndexRaw.resize(indexBytes);
		ar.ProcessRaw("IndexData", m_IndexRaw.data(), m_IndexRaw.size());
	}

	// 5. GPU 버퍼 재생성
	RHI* rhi = Renderer::Get().GetRHI();
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		if (m_RawData[i].size() > 0 && m_StreamCounts[i] > 0)
		{
			m_VertexBuffers[i] = rhi->CreateBuffer(
				m_RawData[i].data(),
				(uint32)m_RawData[i].size(),
				m_StreamStrides[i],
				ERHIBufferType::Vertex
			);
		}
	}
	if (m_IndexRaw.size() > 0 && m_IndexCount > 0)
	{
		m_IndexBuffer = rhi->CreateBuffer(m_IndexRaw.data(), indexBytes, m_IndexStride, ERHIBufferType::Index);
	}

	// 6. Bind용 캐싱 배열 구축
	RebuildCachedBuffers();
}

#pragma endregion
