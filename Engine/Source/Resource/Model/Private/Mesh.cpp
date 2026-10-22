#pragma once
#include "Mesh.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include <archive/ArchiveInterface.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <utility>
#include <limits>

#pragma region Constructor&Destructor
EResult Mesh::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;
	CAST_DESC

	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;

	for (uint32 i = 0; i < static_cast<uint32>(EMeshStream::Max); ++i)
	{
		const auto& stream = desc->Streams[i];
		if (stream.Count == 0) continue;

		if (!stream.Data || stream.Stride == 0)
			return EResult::InvalidArgument;
		if (stream.Count > std::numeric_limits<uint32>::max() / stream.Stride)
			return EResult::InvalidArgument;
		if (i == static_cast<uint32>(EMeshStream::Position) && stream.Stride < sizeof(vec3))
			return EResult::InvalidArgument;
	}

	if (desc->IndexCount > 0)
	{
		if (!desc->IndexData ||
			(desc->IndexStride != sizeof(uint16) && desc->IndexStride != sizeof(uint32)))
			return EResult::InvalidArgument;
		if (desc->IndexCount > std::numeric_limits<uint32>::max() / desc->IndexStride)
			return EResult::InvalidArgument;
	}

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
	if (instance->Initialize(arg) != EResult::Success)
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

void Mesh::Free()
{
	++m_GeometryVersion;
	m_RaycastBVH = {};
	m_RaycastBVHVersion = 0;
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
	m_CachedInputLayouts.clear();
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
const vector<InputLayoutDesc>& Mesh::GetInputLayoutDescs() const
{
	return m_CachedInputLayouts;
}
const MeshRaycastBVH& Mesh::GetRaycastBVH() const
{
	if (m_RaycastBVHVersion == m_GeometryVersion) return m_RaycastBVH;

	MeshRaycastBVH result;
	const auto publish = [&]() -> const MeshRaycastBVH&
		{
			m_RaycastBVH = std::move(result);
			m_RaycastBVHVersion = m_GeometryVersion;
			return m_RaycastBVH;
		};

	if (m_Topology != ETopology::TriangleList) return publish();

	const uint32 vertexCount = GetStreamCount(EMeshStream::Position);
	const uint32 vertexStride = GetStreamStride(EMeshStream::Position);
	const auto& positions = GetRawStreamData(EMeshStream::Position);
	if (vertexCount < 3 || vertexStride < sizeof(vec3) ||
		positions.size() < static_cast<uint64>(vertexCount) * vertexStride)
		return publish();

	const bool indexed = m_IndexCount > 0 || !m_IndexRaw.empty();
	if (indexed && (m_IndexCount < 3 ||
		(m_IndexStride != sizeof(uint16) && m_IndexStride != sizeof(uint32)) ||
		m_IndexRaw.size() < static_cast<uint64>(m_IndexCount) * m_IndexStride))
		return publish();

	const auto readPosition = [&](uint32 index) -> vec3
		{
			vec3 position{};
			std::memcpy(&position, positions.data() + static_cast<sizet>(index) * vertexStride, sizeof(vec3));
			return position;
		};
	const auto readIndex = [&](uint32 index) -> uint32
		{
			if (!indexed) return index;
			if (m_IndexStride == sizeof(uint32))
			{
				uint32 value = 0;
				std::memcpy(&value, m_IndexRaw.data() + static_cast<sizet>(index) * sizeof(uint32), sizeof(uint32));
				return value;
			}
			uint16 value = 0;
			std::memcpy(&value, m_IndexRaw.data() + static_cast<sizet>(index) * sizeof(uint16), sizeof(uint16));
			return value;
		};
	const auto finite = [](const vec3& value)
		{
			return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
		};

	struct Primitive
	{
		MeshRaycastTriangle Triangle;
		vec3 Min;
		vec3 Max;
		vec3 Center;
	};
	const uint32 triangleCount = (indexed ? m_IndexCount : vertexCount) / 3;
	vector<Primitive> primitives;
	primitives.reserve(triangleCount);

	for (uint32 triangle = 0; triangle < triangleCount; ++triangle)
	{
		const uint32 first = triangle * 3;
		const uint32 i0 = readIndex(first), i1 = readIndex(first + 1), i2 = readIndex(first + 2);
		if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) continue;

		const vec3 a = readPosition(i0), b = readPosition(i1), c = readPosition(i2);
		if (!finite(a) || !finite(b) || !finite(c)) continue;

		Primitive primitive;
		primitive.Triangle = { { i0, i1, i2 }, triangle };
		primitive.Min = glm::min(a, glm::min(b, c));
		primitive.Max = glm::max(a, glm::max(b, c));
		primitive.Center = vec3((glm::dvec3(a) + glm::dvec3(b) + glm::dvec3(c)) / 3.0);
		primitives.push_back(primitive);
	}
	if (primitives.empty()) return publish();

	result.Nodes.reserve(primitives.size() / 2 + 1);
	const auto build = [&](auto&& self, uint32 first, uint32 count) -> uint32
		{
			MeshRaycastNode node;
			node.BoundsMin = vec3(FLT_MAX);
			node.BoundsMax = vec3(-FLT_MAX);
			node.First = first;
			node.Count = count;
			for (uint32 i = first; i < first + count; ++i)
			{
				node.BoundsMin = glm::min(node.BoundsMin, primitives[i].Min);
				node.BoundsMax = glm::max(node.BoundsMax, primitives[i].Max);
			}

			const uint32 nodeIndex = static_cast<uint32>(result.Nodes.size());
			result.Nodes.push_back(node);
			if (count <= 8) return nodeIndex;

			const glm::dvec3 extent = glm::dvec3(node.BoundsMax) - glm::dvec3(node.BoundsMin);
			int axis = extent.y > extent.x ? 1 : 0;
			if (extent.z > extent[axis]) axis = 2;

			const uint32 leftCount = count / 2;
			const auto begin = primitives.begin() + first;
			std::nth_element(begin, begin + leftCount, begin + count,
				[axis](const Primitive& a, const Primitive& b)
				{
					if (a.Center[axis] != b.Center[axis]) return a.Center[axis] < b.Center[axis];
					return a.Triangle.SourceIndex < b.Triangle.SourceIndex;
				});

			const uint32 left = self(self, first, leftCount);
			const uint32 right = self(self, first + leftCount, count - leftCount);
			result.Nodes[nodeIndex].Left = left;
			result.Nodes[nodeIndex].Right = right;
			result.Nodes[nodeIndex].Count = 0;
			return nodeIndex;
		};

	build(build, 0, static_cast<uint32>(primitives.size()));
	result.Triangles.reserve(primitives.size());
	for (const auto& primitive : primitives)
		result.Triangles.push_back(primitive.Triangle);

	return publish();
}
#pragma endregion

#pragma region Setter
EResult Mesh::SetStreamBuffer(EMeshStream stream, void* data, uint32 count, uint32 stride)
{
	const uint32 idx = static_cast<uint32>(stream);
	if (idx >= static_cast<uint32>(EMeshStream::Max) || !data || count == 0 || stride == 0)
		return EResult::InvalidArgument;
	if (count > std::numeric_limits<uint32>::max() / stride)
		return EResult::InvalidArgument;

	const bool positionStream = stream == EMeshStream::Position;
	if (positionStream && stride < sizeof(vec3))
		return EResult::InvalidArgument;

	const uint32 totalSize = stride * count;
	vector<uint8> raw(totalSize);
	std::memcpy(raw.data(), data, totalSize);

	vec3 minBound = m_BoundingBoxMin;
	vec3 maxBound = m_BoundingBoxMax;
	if (positionStream)
	{
		minBound = vec3(FLT_MAX);
		maxBound = vec3(-FLT_MAX);
		for (uint32 i = 0; i < count; ++i)
		{
			vec3 position{};
			std::memcpy(&position, raw.data() + static_cast<sizet>(i) * stride, sizeof(vec3));
			if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
				return EResult::InvalidArgument;

			minBound = glm::min(minBound, position);
			maxBound = glm::max(maxBound, position);
		}
	}

	RHIBuffer* buffer = nullptr;
	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (rhi)
	{
		buffer = rhi->CreateBuffer(raw.data(), totalSize, stride, ERHIBufferType::Vertex);
		if (!buffer) return EResult::Fail;
	}

	Safe_Release(m_VertexBuffers[idx]);
	m_VertexBuffers[idx] = buffer;
	m_RawData[idx] = std::move(raw);
	m_StreamCounts[idx] = count;
	m_StreamStrides[idx] = stride;

	if (positionStream)
	{
		m_BoundingBoxMin = minBound;
		m_BoundingBoxMax = maxBound;
		++m_GeometryVersion;
	}

	RebuildCachedBuffers();
	return EResult::Success;
}

EResult Mesh::SetIndexBuffer(void* data, uint32 indexCount, uint32 stride)
{
	if (!data || indexCount == 0 || (stride != sizeof(uint16) && stride != sizeof(uint32)))
		return EResult::InvalidArgument;
	if (indexCount > std::numeric_limits<uint32>::max() / stride)
		return EResult::InvalidArgument;

	const uint32 totalSize = stride * indexCount;
	vector<uint8> raw(totalSize);
	std::memcpy(raw.data(), data, totalSize);

	RHIBuffer* buffer = nullptr;
	RHI* rhi = Renderer::IsCreated() ? Renderer::Get().GetRHI() : nullptr;
	if (rhi)
	{
		buffer = rhi->CreateBuffer(raw.data(), totalSize, stride, ERHIBufferType::Index);
		if (!buffer) return EResult::Fail;
	}

	Safe_Release(m_IndexBuffer);
	m_IndexBuffer = buffer;
	m_IndexRaw = std::move(raw);
	m_IndexCount = indexCount;
	m_IndexStride = stride;
	++m_GeometryVersion;

	return EResult::Success;
}
#pragma endregion

#pragma region Save&Load
archive::ArchiveResult Mesh::ReadArchive(const archive::IArchive& ar)
{
	Mesh candidate(*this);

	// 복사된 GPU 포인터의 소유권은 원본에 남긴다.
	for (auto& buffer : candidate.m_VertexBuffers) buffer = nullptr;
	candidate.m_IndexBuffer = nullptr;

	struct CandidateCleanup
	{
		Mesh& Value;
		~CandidateCleanup() { Value.Free(); }
	} cleanup{ candidate };

	const auto result = candidate.Resource::ReadArchive(ar);
	if (!result) return result;

	using std::swap;
	swap(m_Version, candidate.m_Version);
	swap(m_ResourceType, candidate.m_ResourceType);
	swap(m_Key, candidate.m_Key);
	swap(m_Path, candidate.m_Path);
	swap(m_Flags, candidate.m_Flags);
	swap(m_Topology, candidate.m_Topology);
	swap(m_StreamCounts, candidate.m_StreamCounts);
	swap(m_StreamStrides, candidate.m_StreamStrides);
	swap(m_RawData, candidate.m_RawData);
	swap(m_VertexBuffers, candidate.m_VertexBuffers);
	swap(m_IndexCount, candidate.m_IndexCount);
	swap(m_IndexStride, candidate.m_IndexStride);
	swap(m_IndexRaw, candidate.m_IndexRaw);
	swap(m_IndexBuffer, candidate.m_IndexBuffer);
	swap(m_BoundingBoxMin, candidate.m_BoundingBoxMin);
	swap(m_BoundingBoxMax, candidate.m_BoundingBoxMax);
	swap(m_ActiveBufferCount, candidate.m_ActiveBufferCount);
	swap(m_CachedInputLayouts, candidate.m_CachedInputLayouts);

	++m_GeometryVersion;
	return {};
}
EResult Mesh::Reload(const std::filesystem::path& filePath)
{
	std::filesystem::path targetPath = filePath;
	if (targetPath.empty())
		targetPath = m_Path.empty() ? std::filesystem::path(m_Key) : std::filesystem::path(m_Path);

	std::error_code ec;
	if (!std::filesystem::exists(targetPath, ec))
		return EResult::FileNotFound;

	archive::ArchiveResult result;
	std::string ext = targetPath.extension().string();
	for (char& c : ext) c = static_cast<char>(tolower(c));

	if (ext == ".json")
	{
		archive::JsonArchive ar;
		result = ar.LoadFromFile(targetPath);
		if (!result)
		{
			ENGINE_LOG_ERROR("[Mesh Hot Reload] Failed to load JSON archive: {}", targetPath.string());
			return EResult::Fail;
		}
		result = ReadArchive(ar);
	}
	else
	{
		// .beve 또는 .bammesh 바이너리 아카이브 포맷
		archive::BeveArchive ar;
		result = ar.LoadFromFile(targetPath);
		if (!result)
		{
			ENGINE_LOG_ERROR("[Mesh Hot Reload] Failed to load binary archive: {}", targetPath.string());
			return EResult::Fail;
		}
		result = ReadArchive(ar);
	}

	if (!result)
	{
		ENGINE_LOG_ERROR("[Mesh Hot Reload] Failed to read archive data: {}", targetPath.string());
		return EResult::Fail;
	}

	IncreaseVersion();
	ENGINE_LOG_INFO("[Mesh Hot Reload] Successfully reloaded mesh: {}", targetPath.string());
	return EResult::Success;
}
EResult Mesh::OnPostLoad()
{
	for (uint32 i = 0; i < static_cast<uint32>(EMeshStream::Max); ++i)
	{
		const uint32 count = m_StreamCounts[i];
		const uint32 stride = m_StreamStrides[i];

		if (count > 0)
		{
			if (stride == 0)
				return EResult::Fail;
			if (i == static_cast<uint32>(EMeshStream::Position) && stride < sizeof(vec3))
				return EResult::Fail;
		}

		const uint64 expectedBytes = static_cast<uint64>(count) * stride;
		if (expectedBytes > std::numeric_limits<uint32>::max() || m_RawData[i].size() != expectedBytes)
			return EResult::Fail;
	}

	if (m_IndexCount > 0 && m_IndexStride != sizeof(uint16) && m_IndexStride != sizeof(uint32))
		return EResult::Fail;

	const uint64 expectedIndexBytes = static_cast<uint64>(m_IndexCount) * m_IndexStride;
	if (expectedIndexBytes > std::numeric_limits<uint32>::max() || m_IndexRaw.size() != expectedIndexBytes)
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
	m_ActiveBufferCount = 0;
	m_CachedInputLayouts.clear();
	for (uint32 i = 0; i < (uint32)EMeshStream::Max; ++i)
	{
		if (m_VertexBuffers[i])
		{
			m_ActiveBufferCount = i + 1;
			if (s_StreamLayoutTable[i])
				m_CachedInputLayouts.push_back(*s_StreamLayoutTable[i]);
		}
	}
}
#pragma endregion