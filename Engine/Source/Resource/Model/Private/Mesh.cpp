#pragma once
#include "Mesh.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include <archive/ArchiveInterface.h>

#pragma region Constructor&Destructor
EResult Mesh::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	CAST_DESC

	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;

	// 1. 모든 버텍스 스트림을 루프 하나로 처리
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		const auto& stream = desc->Streams[i];
		m_StreamCounts[i] = stream.Count;
		m_StreamStrides[i] = stream.Stride;

		if (stream.Data && stream.Count > 0 && stream.Stride > 0 && rhi)
		{
			uint32 totalSize = stream.Stride * stream.Count;
			m_VertexBuffers[i] = rhi->CreateBuffer(stream.Data, totalSize, stream.Stride, ERHIBufferType::Vertex);
			if (!m_VertexBuffers[i]) return EResult::Fail;
		}
	}

	// 2. Index Buffer 생성
	m_IndexCount = desc->IndexCount;
	m_IndexStride = desc->IndexStride;
	if (desc->IndexData && m_IndexCount > 0 && rhi)
	{
		uint32 totalSize = desc->IndexStride * m_IndexCount;
		m_IndexBuffer = rhi->CreateBuffer(desc->IndexData, totalSize, desc->IndexStride, ERHIBufferType::Index);
		if (!m_IndexBuffer) return EResult::Fail;
	}

	// 3. Bounding Box & Flags
	m_BoundingBoxMin = desc->BoundingBoxMin;
	m_BoundingBoxMax = desc->BoundingBoxMax;
	const auto& posStream = desc->Streams[(uint32)EMeshStream::Position];
	if (m_BoundingBoxMin == vec3(0.0f) && m_BoundingBoxMax == vec3(0.0f) &&
		posStream.Data && posStream.Count > 0 && posStream.Stride >= sizeof(vec3))
	{
		vec3 minBound(FLT_MAX);
		vec3 maxBound(-FLT_MAX);
		const uint8* rawPos = static_cast<const uint8*>(posStream.Data);
		for (uint32 i = 0; i < posStream.Count; ++i)
		{
			vec3 pos{};
			memcpy(&pos, rawPos + static_cast<sizet>(i) * posStream.Stride, sizeof(vec3));
			minBound = glm::min(minBound, pos);
			maxBound = glm::max(maxBound, pos);
		}
		m_BoundingBoxMin = minBound;
		m_BoundingBoxMax = maxBound;
	}
	m_Flags = desc->Flags | EMeshFlag::KeepRawData;

	// 4. 원본 데이터 보존 (직렬화 및 재로드를 위해 보존)
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		const auto& stream = desc->Streams[i];
		if (stream.Data && stream.Count > 0 && stream.Stride > 0)
		{
			uint32 bytes = stream.Stride * stream.Count;
			m_RawData[i].resize(bytes);
			memcpy(m_RawData[i].data(), stream.Data, bytes);
		}
	}
	if (desc->IndexData && m_IndexCount > 0 && m_IndexStride > 0)
	{
		uint32 bytes = m_IndexStride * m_IndexCount;
		m_IndexRaw.resize(bytes);
		memcpy(m_IndexRaw.data(), desc->IndexData, bytes);
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
static const InputLayoutDesc* s_StreamLayoutTable[] = {
	&VertexPosition::Layout,   // EMeshStream::Position = 0
	&VertexMaterial::Layout,   // EMeshStream::Material = 1
	&VertexSkinData::Layout,   // EMeshStream::SkinData = 2
	nullptr                    // 여분 슬롯 (아직 미정의)
};
const vector<InputLayoutDesc> Mesh::GetInputLayoutDescs() const
{
	vector<InputLayoutDesc> result;
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

	uint32 totalSize = stride * count;
	m_RawData[idx].resize(totalSize);
	memcpy(m_RawData[idx].data(), data, totalSize);
	m_StreamCounts[idx] = count;
	m_StreamStrides[idx] = stride;

	Safe_Release(m_VertexBuffers[idx]);
	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (rhi)
	{
		m_VertexBuffers[idx] = rhi->CreateBuffer(data, totalSize, stride, ERHIBufferType::Vertex);
		if (!m_VertexBuffers[idx]) return EResult::Fail;
	}

	RebuildCachedBuffers();
	return EResult::Success;
}

EResult Mesh::SetIndexBuffer(void* data, uint32 indexCount, uint32 stride)
{
	if (!data || indexCount == 0) return EResult::InvalidArgument;

	uint32 totalSize = stride * indexCount;
	m_IndexRaw.resize(totalSize);
	memcpy(m_IndexRaw.data(), data, totalSize);
	m_IndexCount = indexCount;
	m_IndexStride = stride;

	Safe_Release(m_IndexBuffer);
	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (rhi)
	{
		m_IndexBuffer = rhi->CreateBuffer(data, totalSize, stride, ERHIBufferType::Index);
		if (!m_IndexBuffer) return EResult::Fail;
	}

	return EResult::Success;
}
#pragma endregion

#pragma region Save&Load
EResult Mesh::OnPostLoad()
{
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		const uint64 expectedBytes = static_cast<uint64>(m_StreamCounts[i]) * m_StreamStrides[i];
		if (m_RawData[i].size() != expectedBytes)
			return EResult::Fail;
	}

	const uint64 expectedIndexBytes = static_cast<uint64>(m_IndexCount) * m_IndexStride;
	if (m_IndexRaw.size() != expectedIndexBytes)
		return EResult::Fail;

	for (auto& buffer : m_VertexBuffers)
		Safe_Release(buffer);
	Safe_Release(m_IndexBuffer);

	m_Flags = m_Flags | EMeshFlag::KeepRawData;
	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		if (rhi && !m_RawData[i].empty() && m_StreamStrides[i] > 0)
		{
			m_VertexBuffers[i] = rhi->CreateBuffer(
				m_RawData[i].data(), static_cast<uint32>(m_RawData[i].size()), m_StreamStrides[i], ERHIBufferType::Vertex);
			if (!m_VertexBuffers[i])
				return EResult::Fail;
		}
	}

	if (rhi && !m_IndexRaw.empty() && m_IndexStride > 0)
	{
		m_IndexBuffer = rhi->CreateBuffer(
			m_IndexRaw.data(), static_cast<uint32>(m_IndexRaw.size()), m_IndexStride, ERHIBufferType::Index);
		if (!m_IndexBuffer)
			return EResult::Fail;
	}

	RebuildCachedBuffers();
	return EResult::Success;
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