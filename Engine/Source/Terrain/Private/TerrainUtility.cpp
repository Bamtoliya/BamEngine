#include "TerrainUtility.h"
#include "Vertex.h"
#include "TerrainComponent.h"
#include <stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <cwctype>
#include <vector>

namespace
{
	bool TryGetHeightmapSize(uint32 resolution, uint32& axisCount, std::size_t& sampleCount)
	{
		if (resolution == 0) return false;
		const uint64 axis = static_cast<uint64>(resolution) + 1;
		const uint64 maxSamples = (std::numeric_limits<uint32>::max)() / sizeof(f32);
		if (axis > maxSamples / axis) return false;
		axisCount = static_cast<uint32>(axis);
		sampleCount = static_cast<std::size_t>(axis * axis);
		return true;
	}
	bool TryGetHeightDataLayout(const Engine::TerrainComponent& terrain, uint32& axisCount, std::size_t& sampleCount)
	{
		return TryGetHeightmapSize(terrain.Resolution, axisCount, sampleCount) &&
			terrain.HeightData.size() == sampleCount;
	}

	// ── 하이트맵 입출력 (16비트, UTF-8 경로) ──
	// 파일은 넓은 문자 경로로 직접 읽고 쓴다. stb의 파일 함수는 char* 경로(ACP)를 써서 한글 경로에서 실패한다.
	bool IsRawHeightmap(const std::filesystem::path& path)
	{
		auto ext = path.extension().wstring();
		for (auto& ch : ext) ch = static_cast<wchar_t>(std::towlower(ch));
		return ext == L".raw" || ext == L".r16";
	}

	bool ReadFileBytes(const std::filesystem::path& path, std::vector<unsigned char>& bytes)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file) return false;
		const auto size = static_cast<std::streamoff>(file.tellg());
		if (size <= 0 || size > (std::numeric_limits<int>::max)()) return false;
		bytes.resize(static_cast<std::size_t>(size));
		file.seekg(0);
		return static_cast<bool>(file.read(reinterpret_cast<char*>(bytes.data()), size));
	}

	// 높이 이미지: 값은 0~1, 가로·세로가 다를 수 있다.
	struct HeightImage
	{
		int Width = 0;
		int Height = 0;
		std::vector<float> Values;
	};

	bool DecodeHeightImage(const std::filesystem::path& path, HeightImage& image)
	{
		std::vector<unsigned char> bytes;
		if (!ReadFileBytes(path, bytes)) return false;

		if (IsRawHeightmap(path))
		{
			// RAW: 리틀 엔디언 uint16 정사각형(머리말 없음). 한 변은 파일 크기에서 구한다.
			if (bytes.size() % 2 != 0) return false;
			const std::size_t count = bytes.size() / 2;
			const auto side = static_cast<std::size_t>(std::llround(std::sqrt(static_cast<double>(count))));
			if (side < 2 || side * side != count) return false;
			image.Width = image.Height = static_cast<int>(side);
			image.Values.resize(count);
			for (std::size_t i = 0; i < count; ++i)
				image.Values[i] = static_cast<float>(bytes[i * 2] | (bytes[i * 2 + 1] << 8)) / 65535.0f;
			return true;
		}

		// PNG 등: 8비트 이미지도 16비트로 넓혀 읽으므로 같은 경로로 처리한다.
		int width = 0;
		int height = 0;
		int channels = 0;
		stbi_us* pixels = stbi_load_16_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width, &height, &channels, 1);
		if (!pixels || width <= 0 || height <= 0)
		{
			if (pixels) stbi_image_free(pixels);
			return false;
		}
		image.Width = width;
		image.Height = height;
		image.Values.resize(static_cast<std::size_t>(width) * height);
		for (std::size_t i = 0; i < image.Values.size(); ++i)
			image.Values[i] = static_cast<float>(pixels[i]) / 65535.0f;
		stbi_image_free(pixels);
		return true;
	}

	void AppendBigEndian32(std::vector<unsigned char>& out, uint32 value)
	{
		for (int shift = 24; shift >= 0; shift -= 8) out.push_back(static_cast<unsigned char>(value >> shift));
	}

	void AppendPngChunk(std::vector<unsigned char>& out, const char* type, const unsigned char* data, std::size_t size)
	{
		AppendBigEndian32(out, static_cast<uint32>(size));
		const std::size_t typeStart = out.size();
		out.insert(out.end(), type, type + 4);
		if (size > 0) out.insert(out.end(), data, data + size);
		const unsigned int crc = stbiw__crc32(out.data() + typeStart, static_cast<int>(size + 4));
		AppendBigEndian32(out, crc);
	}

	// 16비트 그레이스케일 PNG(색 형식 0, 비트 깊이 16). stb_image_write는 8비트만 쓰므로 압축과 CRC만 빌려 직접 조립한다.
	bool EncodeGray16Png(const std::vector<uint16>& samples, int side, std::vector<unsigned char>& png)
	{
		const std::size_t rowBytes = static_cast<std::size_t>(side) * 2 + 1;
		std::vector<unsigned char> filtered(rowBytes * side);
		for (int y = 0; y < side; ++y)
		{
			unsigned char* row = filtered.data() + rowBytes * y;
			row[0] = 0; // 필터 없음
			for (int x = 0; x < side; ++x)
			{
				const uint16 value = samples[static_cast<std::size_t>(y) * side + x];
				row[1 + x * 2] = static_cast<unsigned char>(value >> 8);
				row[2 + x * 2] = static_cast<unsigned char>(value & 0xFF);
			}
		}

		int zlibSize = 0;
		unsigned char* zlib = stbi_zlib_compress(filtered.data(), static_cast<int>(filtered.size()), &zlibSize, 8);
		if (!zlib || zlibSize <= 0) return false;

		static constexpr unsigned char signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
		png.assign(std::begin(signature), std::end(signature));
		std::vector<unsigned char> header;
		AppendBigEndian32(header, static_cast<uint32>(side));
		AppendBigEndian32(header, static_cast<uint32>(side));
		header.insert(header.end(), { 16, 0, 0, 0, 0 }); // 비트 깊이 16, 그레이스케일, 압축·필터·인터레이스 기본
		AppendPngChunk(png, "IHDR", header.data(), header.size());
		AppendPngChunk(png, "IDAT", zlib, static_cast<std::size_t>(zlibSize));
		AppendPngChunk(png, "IEND", nullptr, 0);
		STBIW_FREE(zlib);
		return true;
	}
	// ── 1. 레이아웃 구조체 먼저 선언 ──
	struct TerrainGridMeshLayout
	{
		uint32 Columns = 0;
		uint32 Rows = 0;
		uint32 VertexCount = 0;
		uint32 IndexCount = 0;
	};
	bool TryGetTerrainGridMeshLayout(uint32 segmentsX, uint32 segmentsZ, TerrainGridMeshLayout& layout)
	{
		if (segmentsX == 0 || segmentsZ == 0)
			return false;
		constexpr uint64 maxBytes = (std::numeric_limits<uint32>::max)();
		constexpr uint64 maxVertices = maxBytes / std::max(sizeof(Engine::VertexPosition), sizeof(Engine::VertexMaterial));
		constexpr uint64 maxQuads = maxBytes / (6 * sizeof(uint32));
		const uint64 columns = static_cast<uint64>(segmentsX) + 1;
		const uint64 rows = static_cast<uint64>(segmentsZ) + 1;
		const uint64 quads = static_cast<uint64>(segmentsX) * segmentsZ;
		if (columns > maxVertices / rows || quads > maxQuads)
			return false;
		layout.Columns = static_cast<uint32>(columns);
		layout.Rows = static_cast<uint32>(rows);
		layout.VertexCount = static_cast<uint32>(columns * rows);
		layout.IndexCount = static_cast<uint32>(quads * 6);
		return true;
	}
	// ── 2. TerrainGridMeshLayout 이후에 DDC 캐시 헬퍼 선언 ──
	constexpr uint32_t TERRAIN_GRID_MAGIC = 0x43475442; // "BTGC" (Bam Terrain Grid Cache)
	constexpr uint32_t TERRAIN_GRID_VERSION = 2;
	std::filesystem::path GetTerrainGridCachePath(float width, float length,
		uint32 segmentsX, uint32 segmentsZ, bool reverseWinding)
	{
		std::error_code ec;
		uint64_t hash = 14695981039346656037ULL;
		auto hashCombine = [&hash](const void* data, size_t size)
			{
				const auto* bytes = static_cast<const uint8_t*>(data);
				for (size_t i = 0; i < size; ++i)
				{
					hash ^= bytes[i];
					hash *= 1099511628211ULL;
				}
			};

		hashCombine(&width, sizeof(width));
		hashCombine(&length, sizeof(length));
		hashCombine(&segmentsX, sizeof(segmentsX));
		hashCombine(&segmentsZ, sizeof(segmentsZ));
		const uint8_t rw = reverseWinding ? 1 : 0;
		hashCombine(&rw, sizeof(rw));
		const char versionTag[] = "terrain_grid_v2";
		hashCombine(versionTag, sizeof(versionTag) - 1);

		char buf[32];
		std::snprintf(buf, sizeof(buf), "%016llx.bin", static_cast<unsigned long long>(hash));

		const auto cacheDir = Engine::PlatformPath::GetProjectRoot() / ".bam" / "cache" / "terrain";
		std::filesystem::create_directories(cacheDir, ec);
		return cacheDir / buf;
	}
	bool TryLoadTerrainGridFromCache(const std::filesystem::path& cachePath,
		std::vector<Engine::VertexPosition>& positions,
		std::vector<Engine::VertexMaterial>& materials,
		std::vector<uint32>& indices,
		const TerrainGridMeshLayout& layout)
	{
		std::error_code ec;
		if (cachePath.empty() || !std::filesystem::exists(cachePath, ec)) return false;
		std::ifstream file(cachePath, std::ios::binary);
		if (!file.is_open()) return false;
		uint32_t magic = 0, version = 0;
		file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
		file.read(reinterpret_cast<char*>(&version), sizeof(version));
		if (magic != TERRAIN_GRID_MAGIC || version != TERRAIN_GRID_VERSION) return false;
		TerrainGridMeshLayout cachedLayout;
		file.read(reinterpret_cast<char*>(&cachedLayout), sizeof(cachedLayout));
		if (!file || cachedLayout.VertexCount != layout.VertexCount || cachedLayout.IndexCount != layout.IndexCount)
			return false;
		positions.resize(layout.VertexCount);
		materials.resize(layout.VertexCount);
		indices.resize(layout.IndexCount);
		file.read(reinterpret_cast<char*>(positions.data()), layout.VertexCount * sizeof(Engine::VertexPosition));
		file.read(reinterpret_cast<char*>(materials.data()), layout.VertexCount * sizeof(Engine::VertexMaterial));
		file.read(reinterpret_cast<char*>(indices.data()), layout.IndexCount * sizeof(uint32));
		return file.good();
	}
	void SaveTerrainGridToCache(const std::filesystem::path& cachePath,
		const std::vector<Engine::VertexPosition>& positions,
		const std::vector<Engine::VertexMaterial>& materials,
		const std::vector<uint32>& indices,
		const TerrainGridMeshLayout& layout)
	{
		if (cachePath.empty() || positions.empty() || indices.empty()) return;
		std::error_code ec;
		const auto tmpPath = cachePath.string() + ".tmp";
		std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
		if (!file.is_open()) return;
		file.write(reinterpret_cast<const char*>(&TERRAIN_GRID_MAGIC), sizeof(TERRAIN_GRID_MAGIC));
		file.write(reinterpret_cast<const char*>(&TERRAIN_GRID_VERSION), sizeof(TERRAIN_GRID_VERSION));
		file.write(reinterpret_cast<const char*>(&layout), sizeof(layout));
		file.write(reinterpret_cast<const char*>(positions.data()), layout.VertexCount * sizeof(Engine::VertexPosition));
		file.write(reinterpret_cast<const char*>(materials.data()), layout.VertexCount * sizeof(Engine::VertexMaterial));
		file.write(reinterpret_cast<const char*>(indices.data()), layout.IndexCount * sizeof(uint32));
		file.close();
		std::filesystem::rename(tmpPath, cachePath, ec);
	}
} // namespace

Mesh* TerrainUtility::CreateGridMesh(float width, float length, uint32 resolution)
{
	return CreateGridMesh(width, length, resolution, resolution, false);
}

Mesh* TerrainUtility::CreateGridMesh(float width, float length, uint32 segmentsX, uint32 segmentsZ, bool reverseWinding)
{
	TerrainGridMeshLayout layout;
	if (!std::isfinite(width) || width <= 0.0f || !std::isfinite(length) || length <= 0.0f ||
		!TryGetTerrainGridMeshLayout(segmentsX, segmentsZ, layout))
		return nullptr;

	const float startX = -width * 0.5f;
	const float startZ = -length * 0.5f;

	std::vector<VertexPosition> positions;
	std::vector<VertexMaterial> materials;
	std::vector<uint32> indices;

	// ── 0. 지형 그리드 메시 디스크 캐시 확인 ──
	const auto cachePath = GetTerrainGridCachePath(width, length, segmentsX, segmentsZ, reverseWinding);
	const bool cacheHit = TryLoadTerrainGridFromCache(cachePath, positions, materials, indices, layout);

	if (!cacheHit)
	{
		const float dx = width / static_cast<float>(segmentsX);
		const float dz = length / static_cast<float>(segmentsZ);

		if (!std::isfinite(dx) || dx <= 0.0f || !std::isfinite(dz) || dz <= 0.0f ||
			!std::isfinite(startX + static_cast<float>(segmentsX) * dx) ||
			!std::isfinite(startZ + static_cast<float>(segmentsZ) * dz))
			return nullptr;

		positions.resize(layout.VertexCount);
		materials.resize(layout.VertexCount);
		indices.resize(layout.IndexCount);

		uint32 vertexIndex = 0;
		for (uint32 z = 0; z < layout.Rows; ++z)
		{
			const float currentZ = startZ + static_cast<float>(z) * dz;
			const float v = static_cast<float>(z) / static_cast<float>(segmentsZ);

			for (uint32 x = 0; x < layout.Columns; ++x)
			{
				const float currentX = startX + static_cast<float>(x) * dx;
				const float u = static_cast<float>(x) / static_cast<float>(segmentsX);
				positions[vertexIndex].position = vec3(currentX, 0.0f, currentZ);

				auto& material = materials[vertexIndex];
				material.texCoord = vec2(u, v);
				material.texCoord1 = material.texCoord;
				material.normal = vec3(0.0f, 1.0f, 0.0f);
				material.tangent = vec3(1.0f, 0.0f, 0.0f);
				material.binormal = vec3(0.0f, 0.0f, 1.0f);
				material.color = vec4(1.0f);
				++vertexIndex;
			}
		}

		uint32 index = 0;
		const auto appendTriangle = [&](uint32 a, uint32 b, uint32 c)
			{
				if (reverseWinding)
					std::swap(b, c);

				indices[index++] = a;
				indices[index++] = b;
				indices[index++] = c;
			};

		for (uint32 z = 0; z < segmentsZ; ++z)
		{
			for (uint32 x = 0; x < segmentsX; ++x)
			{
				const uint32 i0 = z * layout.Columns + x;
				const uint32 i1 = i0 + 1;
				const uint32 i2 = i0 + layout.Columns;
				const uint32 i3 = i2 + 1;

				appendTriangle(i0, i2, i1);
				appendTriangle(i1, i2, i3);
			}
		}

		// ── 디스크 캐시에 원자적 저장 ──
		SaveTerrainGridToCache(cachePath, positions, materials, indices, layout);
	}

	MeshCreateDesc desc{};
	desc.Streams[static_cast<uint32>(EMeshStream::Position)] =
	{ positions.data(), layout.VertexCount, sizeof(VertexPosition) };
	desc.Streams[static_cast<uint32>(EMeshStream::Material)] =
	{ materials.data(), layout.VertexCount, sizeof(VertexMaterial) };
	desc.IndexData = indices.data();
	desc.IndexCount = layout.IndexCount;
	desc.IndexStride = sizeof(uint32);
	desc.BoundingBoxMin = vec3(startX, 0.0f, startZ);
	desc.BoundingBoxMax = vec3(-startX, 0.0f, -startZ);
	desc.Flags = EMeshFlag::KeepRawData | EMeshFlag::HasPosition | EMeshFlag::HasNormal |
		EMeshFlag::HasTexCoord | EMeshFlag::HasTangent;

	return Mesh::Create(&desc);
}

Mesh* TerrainUtility::CreateChunkGridMesh(float width, float length, uint32 resolution, uint32 minVertexX, uint32 maxVertexX, uint32 minVertexZ, uint32 maxVertexZ, uint32 lodStep)
{
	if (!std::isfinite(width) || width <= 0.0f || !std::isfinite(length) || length <= 0.0f ||
		resolution == 0 || resolution > TerrainComponent::MaxResolution ||
		minVertexX >= maxVertexX || maxVertexX > resolution ||
		minVertexZ >= maxVertexZ || maxVertexZ > resolution || lodStep == 0)
		return nullptr;

	std::vector<uint32> xs;
	std::vector<uint32> zs;
	for (uint32 x = minVertexX; x < maxVertexX; x += lodStep) xs.push_back(x);
	xs.push_back(maxVertexX);
	for (uint32 z = minVertexZ; z < maxVertexZ; z += lodStep) zs.push_back(z);
	zs.push_back(maxVertexZ);

	const uint32 segmentsX = static_cast<uint32>(xs.size() - 1);
	const uint32 segmentsZ = static_cast<uint32>(zs.size() - 1);
	TerrainGridMeshLayout layout;
	if (!TryGetTerrainGridMeshLayout(segmentsX, segmentsZ, layout))
		return nullptr;

	const float startX = -width * 0.5f;
	const float startZ = -length * 0.5f;
	const float invResolution = 1.0f / static_cast<float>(resolution);

	std::vector<VertexPosition> positions(layout.VertexCount);
	std::vector<VertexMaterial> materials(layout.VertexCount);
	std::vector<uint32> indices(layout.IndexCount);

	uint32 vertexIndex = 0;
	for (uint32 iz = 0; iz < layout.Rows; ++iz)
	{
		const float v = static_cast<float>(zs[iz]) * invResolution;
		const float currentZ = startZ + v * length;

		for (uint32 ix = 0; ix < layout.Columns; ++ix)
		{
			const float u = static_cast<float>(xs[ix]) * invResolution;
			const float currentX = startX + u * width;

			positions[vertexIndex].position = vec3(currentX, 0.0f, currentZ);

			auto& material = materials[vertexIndex];
			material.texCoord = vec2(u, v);
			material.texCoord1 = material.texCoord;
			material.normal = vec3(0.0f, 1.0f, 0.0f);
			material.tangent = vec3(1.0f, 0.0f, 0.0f);
			material.binormal = vec3(0.0f, 0.0f, 1.0f);
			material.color = vec4(1.0f);
			++vertexIndex;
		}
	}

	uint32 index = 0;
	for (uint32 z = 0; z < segmentsZ; ++z)
	{
		for (uint32 x = 0; x < segmentsX; ++x)
		{
			const uint32 i0 = z * layout.Columns + x;
			const uint32 i1 = i0 + 1;
			const uint32 i2 = i0 + layout.Columns;
			const uint32 i3 = i2 + 1;

			indices[index++] = i0;
			indices[index++] = i2;
			indices[index++] = i1;
			indices[index++] = i1;
			indices[index++] = i2;
			indices[index++] = i3;
		}
	}

	const float minLocalX = startX + (static_cast<float>(minVertexX) * invResolution) * width;
	const float maxLocalX = startX + (static_cast<float>(maxVertexX) * invResolution) * width;
	const float minLocalZ = startZ + (static_cast<float>(minVertexZ) * invResolution) * length;
	const float maxLocalZ = startZ + (static_cast<float>(maxVertexZ) * invResolution) * length;

	MeshCreateDesc desc{};
	desc.Streams[static_cast<uint32>(EMeshStream::Position)] =
	{ positions.data(), layout.VertexCount, sizeof(VertexPosition) };
	desc.Streams[static_cast<uint32>(EMeshStream::Material)] =
	{ materials.data(), layout.VertexCount, sizeof(VertexMaterial) };
	desc.IndexData = indices.data();
	desc.IndexCount = layout.IndexCount;
	desc.IndexStride = sizeof(uint32);
	desc.BoundingBoxMin = vec3(minLocalX, 0.0f, minLocalZ);
	desc.BoundingBoxMax = vec3(maxLocalX, 0.0f, maxLocalZ);
	desc.Flags = EMeshFlag::KeepRawData | EMeshFlag::HasPosition | EMeshFlag::HasNormal |
		EMeshFlag::HasTexCoord | EMeshFlag::HasTangent;

	return Mesh::Create(&desc);
}

bool TerrainUtility::ComputeChunkRange(uint32 resolution, uint32 chunkCountPerAxis, uint32 chunkX, uint32 chunkZ, uint32& outMinVertexX, uint32& outMaxVertexX, uint32& outMinVertexZ, uint32& outMaxVertexZ, vec2& outUVOffset, vec2& outUVScale)
{
	if (resolution == 0 || resolution > TerrainComponent::MaxResolution || chunkCountPerAxis == 0)
		return false;

	const uint32 effectiveChunks = std::clamp(chunkCountPerAxis, 1u, std::min(resolution, TerrainComponent::MaxChunkCountPerAxis));
	if (chunkX >= effectiveChunks || chunkZ >= effectiveChunks)
		return false;

	outMinVertexX = static_cast<uint32>((static_cast<uint64>(chunkX) * resolution) / effectiveChunks);
	outMaxVertexX = static_cast<uint32>((static_cast<uint64>(chunkX + 1u) * resolution) / effectiveChunks);
	outMinVertexZ = static_cast<uint32>((static_cast<uint64>(chunkZ) * resolution) / effectiveChunks);
	outMaxVertexZ = static_cast<uint32>((static_cast<uint64>(chunkZ + 1u) * resolution) / effectiveChunks);
	if (outMinVertexX >= outMaxVertexX || outMinVertexZ >= outMaxVertexZ)
		return false;

	const f32 invResolution = 1.0f / static_cast<f32>(resolution);
	outUVOffset = vec2(static_cast<f32>(outMinVertexX) * invResolution, static_cast<f32>(outMinVertexZ) * invResolution);
	outUVScale = vec2(static_cast<f32>(outMaxVertexX - outMinVertexX) * invResolution, static_cast<f32>(outMaxVertexZ - outMinVertexZ) * invResolution);
	return true;
}

bool TerrainUtility::ComputeChunkLocalBounds(const TerrainComponent& terrain, uint32 minVertexX, uint32 maxVertexX, uint32 minVertexZ, uint32 maxVertexZ, BoundingBox& outLocalBounds)
{
	uint32 axisCount = 0;
	std::size_t sampleCount = 0;
	if (!std::isfinite(terrain.Width) || terrain.Width <= 0.0f ||
		!std::isfinite(terrain.Length) || terrain.Length <= 0.0f ||
		!std::isfinite(terrain.MaxHeight) || terrain.MaxHeight < 0.0f ||
		!TryGetGridLayout(terrain.Resolution, axisCount, sampleCount) ||
		minVertexX >= maxVertexX || maxVertexX > terrain.Resolution ||
		minVertexZ >= maxVertexZ || maxVertexZ > terrain.Resolution)
		return false;

	const f32 invResolution = 1.0f / static_cast<f32>(terrain.Resolution);
	const f32 minX = -terrain.Width * 0.5f + static_cast<f32>(minVertexX) * invResolution * terrain.Width;
	const f32 maxX = -terrain.Width * 0.5f + static_cast<f32>(maxVertexX) * invResolution * terrain.Width;
	const f32 minZ = -terrain.Length * 0.5f + static_cast<f32>(minVertexZ) * invResolution * terrain.Length;
	const f32 maxZ = -terrain.Length * 0.5f + static_cast<f32>(maxVertexZ) * invResolution * terrain.Length;

	f32 minY = 0.0f;
	f32 maxY = terrain.MaxHeight;
	if (terrain.HeightData.size() == sampleCount)
	{
		f32 minH = 1.0f;
		f32 maxH = 0.0f;
		for (uint32 z = minVertexZ; z <= maxVertexZ; ++z)
		{
			const std::size_t rowOffset = static_cast<std::size_t>(z) * axisCount;
			for (uint32 x = minVertexX; x <= maxVertexX; ++x)
			{
				const f32 raw = terrain.HeightData[rowOffset + x];
				const f32 h = std::isfinite(raw) ? std::clamp(raw, 0.0f, 1.0f) : 0.0f;
				minH = (std::min)(minH, h);
				maxH = (std::max)(maxH, h);
			}
		}
		minY = minH * terrain.MaxHeight;
		maxY = maxH * terrain.MaxHeight;
	}

	outLocalBounds = BoundingBox(vec3(minX, minY, minZ), vec3(maxX, maxY, maxZ));
	return outLocalBounds.IsValid();
}

bool TerrainUtility::ComputeChunkWorldBounds(const BoundingBox& localBounds, const mat4& worldMatrix, BoundingBox& outWorldBounds)
{
	return localBounds.TryTransformToAABB(worldMatrix, outWorldBounds);
}

f32 TerrainUtility::ComputeDistanceToBoundingBox(const vec3& point, const BoundingBox& bounds)
{
	const vec3 clamped = glm::clamp(point, bounds.min, bounds.max);
	return glm::length(point - clamped);
}

uint32 TerrainUtility::SelectChunkLOD(f32 distance, uint32 previousLOD, f32 lod0Distance, f32 lod1Distance, f32 lod2Distance, f32 hysteresis)
{
	const f32 d0 = std::isfinite(lod0Distance) ? (std::max)(0.0f, lod0Distance) : 80.0f;
	const f32 d1 = std::isfinite(lod1Distance) ? (std::max)(d0, lod1Distance) : (std::max)(d0, 180.0f);
	(void)lod2Distance;
	const f32 dist = std::isfinite(distance) ? (std::max)(0.0f, distance) : 0.0f;
	const f32 h = (previousLOD < TerrainComponent::MaxLODLevels && std::isfinite(hysteresis)) ? (std::max)(0.0f, hysteresis) : 0.0f;

	if (previousLOD >= TerrainComponent::MaxLODLevels || h <= 0.0f)
	{
		if (dist <= d0) return 0u;
		if (dist <= d1) return 1u;
		return 2u;
	}

	if (previousLOD == 0u)
	{
		if (dist > d1 + h) return 2u;
		if (dist > d0 + h) return 1u;
		return 0u;
	}
	if (previousLOD == 1u)
	{
		if (dist < (std::max)(0.0f, d0 - h)) return 0u;
		if (dist > d1 + h) return 2u;
		return 1u;
	}
	if (dist < (std::max)(0.0f, d0 - h)) return 0u;
	if (dist < (std::max)(0.0f, d1 - h)) return 1u;
	return 2u;
}

bool TerrainUtility::ShouldChunkBeLoaded(f32 distance, bool currentlyLoaded, f32 loadDistance, f32 hysteresis)
{
	if (!std::isfinite(loadDistance) || loadDistance <= 0.0f)
		return true;

	const f32 dist = std::isfinite(distance) ? (std::max)(0.0f, distance) : 0.0f;
	const f32 h = std::isfinite(hysteresis) ? (std::max)(0.0f, hysteresis) : 0.0f;
	return currentlyLoaded ? (dist <= loadDistance + h) : (dist <= loadDistance);
}

bool TerrainUtility::IsChunkVisibleInFrustum(const Frustum& frustum, const BoundingBox& worldBounds)
{
	return FrustumCuller::TestBoundingBox(frustum, worldBounds);
}

bool TerrainUtility::ApplyBrush(TerrainComponent& terrain, const glm::vec3& hitWorldPos, const glm::mat4& worldMatrix, const TerrainBrushParams& params, float dt)
{
	uint32 vertexAxisCount = 0;
	std::size_t sampleCount = 0;
	if (!TryGetHeightDataLayout(terrain, vertexAxisCount, sampleCount) ||
		terrain.MaxHeight <= 0.0001f || params.Radius <= 0.0001f)
		return false;
	// 1. 월드 좌표를 터레인 로컬 좌표계로 역변환
	const glm::mat4 invWorld = glm::inverse(worldMatrix);
	const glm::vec3 localPos = glm::vec3(invWorld * glm::vec4(hitWorldPos, 1.0f));
	const float halfWidth = terrain.Width * 0.5f;
	const float halfLength = terrain.Length * 0.5f;
	if (localPos.x < -halfWidth || localPos.x > halfWidth || localPos.z < -halfLength || localPos.z > halfLength)
		return false;
	const float radius = params.Radius;
	// 2. 영향받는 사각 영역(BoundingBox) 정점 인덱스 산출 (전체 순회 방지 최적화)
	const float uMin = glm::clamp((localPos.x - radius + halfWidth) / terrain.Width, 0.0f, 1.0f);
	const float uMax = glm::clamp((localPos.x + radius + halfWidth) / terrain.Width, 0.0f, 1.0f);
	const float vMin = glm::clamp((localPos.z - radius + halfLength) / terrain.Length, 0.0f, 1.0f);
	const float vMax = glm::clamp((localPos.z + radius + halfLength) / terrain.Length, 0.0f, 1.0f);
	const uint32 minX = static_cast<uint32>(uMin * static_cast<float>(terrain.Resolution));
	const uint32 maxX = glm::min(static_cast<uint32>(std::ceil(uMax * static_cast<float>(terrain.Resolution))), terrain.Resolution);
	const uint32 minZ = static_cast<uint32>(vMin * static_cast<float>(terrain.Resolution));
	const uint32 maxZ = glm::min(static_cast<uint32>(std::ceil(vMax * static_cast<float>(terrain.Resolution))), terrain.Resolution);
	bool modified = false;
	const float invMaxHeight = 1.0f / terrain.MaxHeight;
	const float heightDelta = (params.Strength * dt) * invMaxHeight;
	for (uint32 z = minZ; z <= maxZ; ++z)
	{
		const float vz = (static_cast<float>(z) / static_cast<float>(terrain.Resolution) - 0.5f) * terrain.Length;
		const float dz = vz - localPos.z;
		for (uint32 x = minX; x <= maxX; ++x)
		{
			const float vx = (static_cast<float>(x) / static_cast<float>(terrain.Resolution) - 0.5f) * terrain.Width;
			const float dx = vx - localPos.x;
			const float dist = std::sqrt(dx * dx + dz * dz);
			if (dist > radius) continue;
			// Smoothstep 기반 부드러운 감쇄 가중치 (Falloff)
			const float t = dist / radius;
			const float weight = 1.0f - (t * t * (3.0f - 2.0f * t));
			const uint32 index = z * vertexAxisCount + x;
			float currentHeight = terrain.HeightData[index];
			switch (params.Mode)
			{
			case ETerrainBrushMode::Raise:
				currentHeight += heightDelta * weight;
				break;
			case ETerrainBrushMode::Lower:
				currentHeight -= heightDelta * weight;
				break;
			case ETerrainBrushMode::Flatten:
			{
				const float targetNorm = glm::clamp(params.TargetHeight * invMaxHeight, 0.0f, 1.0f);
				currentHeight = glm::mix(currentHeight, targetNorm, glm::clamp(params.Strength * dt * weight, 0.0f, 1.0f));
				break;
			}
			case ETerrainBrushMode::Smooth:
			{
				float sum = 0.0f;
				int count = 0;
				if (x > 0) { sum += terrain.HeightData[index - 1]; ++count; }
				if (x < terrain.Resolution) { sum += terrain.HeightData[index + 1]; ++count; }
				if (z > 0) { sum += terrain.HeightData[index - vertexAxisCount]; ++count; }
				if (z < terrain.Resolution) { sum += terrain.HeightData[index + vertexAxisCount]; ++count; }
				const float avg = (count > 0) ? (sum / static_cast<float>(count)) : currentHeight;
				currentHeight = glm::mix(currentHeight, avg, glm::clamp(params.Strength * dt * weight, 0.0f, 1.0f));
				break;
			}
			}
			currentHeight = glm::clamp(currentHeight, 0.0f, 1.0f);
			if (std::abs(currentHeight - terrain.HeightData[index]) > 0.00001f)
			{
				terrain.HeightData[index] = currentHeight;
				modified = true;
			}
		}
	}
	if (modified)
	{
		terrain.MarkHeightDirty(PixelRect{ minX, minZ, maxX + 1, maxZ + 1 });
		terrain.IsDataDirty = true; // 씬 저장 때 데이터 파일을 다시 쓴다
	}

	return modified;
}

bool TerrainUtility::LoadHeightmapFromFile(TerrainComponent& terrain, const std::wstring& filePath)
{
	uint32 vertexAxisCount = 0;
	std::size_t sampleCount = 0;
	if (!TryGetHeightmapSize(terrain.Resolution, vertexAxisCount, sampleCount)) return false;

	HeightImage image;
	if (!DecodeHeightImage(std::filesystem::path(filePath), image)) return false;

	// 이미지 해상도와 정점 그리드가 달라도 바이리니어 샘플링으로 맞춘다. 실패하면 기존 높이를 바꾸지 않는다.
	std::vector<f32> heights(sampleCount);
	const int imgW = image.Width;
	const int imgH = image.Height;
	const auto at = [&](int px, int py) { return image.Values[static_cast<std::size_t>(py) * imgW + px]; };
	for (uint32 z = 0; z < vertexAxisCount; ++z)
	{
		const float v = static_cast<float>(z) / static_cast<float>(vertexAxisCount - 1);
		const float srcY = v * static_cast<float>(imgH - 1);
		const int y0 = glm::clamp(static_cast<int>(srcY), 0, imgH - 1);
		const int y1 = glm::clamp(y0 + 1, 0, imgH - 1);
		const float ty = srcY - static_cast<float>(y0);

		for (uint32 x = 0; x < vertexAxisCount; ++x)
		{
			const float u = static_cast<float>(x) / static_cast<float>(vertexAxisCount - 1);
			const float srcX = u * static_cast<float>(imgW - 1);
			const int x0 = glm::clamp(static_cast<int>(srcX), 0, imgW - 1);
			const int x1 = glm::clamp(x0 + 1, 0, imgW - 1);
			const float tx = srcX - static_cast<float>(x0);
			heights[static_cast<std::size_t>(z) * vertexAxisCount + x] =
				glm::mix(glm::mix(at(x0, y0), at(x1, y0), tx), glm::mix(at(x0, y1), at(x1, y1), tx), ty);
		}
	}

	terrain.HeightData.swap(heights);
	terrain.MarkHeightDirty();
	terrain.IsDataDirty = true;
	return true;
}

bool TerrainUtility::SaveHeightmapToFile(const TerrainComponent& terrain, const std::wstring& filePath)
{
	uint32 vertexAxisCount = 0;
	std::size_t sampleCount = 0;
	if (!TryGetHeightDataLayout(terrain, vertexAxisCount, sampleCount)) return false;

	std::vector<uint16> samples(sampleCount);
	for (std::size_t i = 0; i < sampleCount; ++i)
	{
		const f32 height = terrain.HeightData[i];
		if (!std::isfinite(height)) return false;
		samples[i] = static_cast<uint16>(std::lround(glm::clamp(height, 0.0f, 1.0f) * 65535.0f));
	}

	// 확장자가 .raw·.r16이면 RAW(리틀 엔디언 uint16), 그 외에는 16비트 PNG로 쓴다.
	const std::filesystem::path path(filePath);
	std::vector<unsigned char> bytes;
	if (IsRawHeightmap(path))
	{
		bytes.resize(sampleCount * 2);
		for (std::size_t i = 0; i < sampleCount; ++i)
		{
			bytes[i * 2] = static_cast<unsigned char>(samples[i] & 0xFF);
			bytes[i * 2 + 1] = static_cast<unsigned char>(samples[i] >> 8);
		}
	}
	else if (!EncodeGray16Png(samples, static_cast<int>(vertexAxisCount), bytes))
	{
		return false;
	}

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) return false;
	file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	return static_cast<bool>(file.flush());
}

namespace
{
	// 정점당 channels바이트 배열을 최근접 샘플로 다른 해상도에 맞춘다.
	bool ResampleNearest(const std::vector<uint8>& source, uint32 channels, uint32 targetResolution, std::vector<uint8>& result)
	{
		uint32 targetAxis = 0;
		std::size_t targetCount = 0;
		if (channels == 0 || source.size() % channels != 0 || !TryGetHeightmapSize(targetResolution, targetAxis, targetCount))
			return false;
		const std::size_t sourceCount = source.size() / channels;
		const auto sourceAxis = static_cast<std::size_t>(std::llround(std::sqrt(static_cast<double>(sourceCount))));
		if (sourceAxis < 2 || sourceAxis * sourceAxis != sourceCount) return false;
		const std::size_t sourceResolution = sourceAxis - 1;

		std::vector<uint8> candidate(targetCount * channels);
		for (std::size_t z = 0; z < targetAxis; ++z)
		{
			const std::size_t sourceZ = (z * sourceResolution + targetResolution / 2) / targetResolution;
			for (std::size_t x = 0; x < targetAxis; ++x)
			{
				const std::size_t sourceX = (x * sourceResolution + targetResolution / 2) / targetResolution;
				std::copy_n(source.data() + (sourceZ * sourceAxis + sourceX) * channels, channels, candidate.data() + (z * targetAxis + x) * channels);
			}
		}
		result.swap(candidate);
		return true;
	}

	// 하나의 배열을 정해진 크기로 맞춘다. 비었거나 리샘플링할 수 없으면 기본값으로 채운다.
	bool FitLayerPlane(std::vector<uint8>& data, uint32 channels, uint32 resolution, std::size_t sampleCount, uint8 firstChannelDefault)
	{
		if (data.size() == sampleCount * channels) return false;
		if (data.empty() || !ResampleNearest(data, channels, resolution, data))
		{
			data.assign(sampleCount * channels, 0);
			if (firstChannelDefault != 0)
				for (std::size_t i = 0; i < sampleCount; ++i) data[i * channels] = firstChannelDefault;
		}
		return true;
	}

	// 브러시 반경 안의 정점을 돈다. visit(index, falloffFactor 0~1)가 값을 바꿨으면 true를 돌려준다.
	template <typename Visit>
	bool ForEachBrushVertex(const Engine::TerrainComponent& terrain, const glm::vec3& hitWorldPos, const glm::mat4& worldMatrix, float radius, float falloff, Visit&& visit)
	{
		uint32 axisCount = 0;
		std::size_t sampleCount = 0;
		if (!(radius > 0.0f) || !TryGetHeightmapSize(terrain.Resolution, axisCount, sampleCount) ||
			!(terrain.Width > 0.0f) || !(terrain.Length > 0.0f))
			return false;

		const glm::vec3 local = glm::vec3(glm::inverse(worldMatrix) * glm::vec4(hitWorldPos, 1.0f));
		if (!std::isfinite(local.x) || !std::isfinite(local.z)) return false;
		const float halfW = terrain.Width * 0.5f;
		const float halfL = terrain.Length * 0.5f;
		const float stepX = terrain.Width / static_cast<float>(terrain.Resolution);
		const float stepZ = terrain.Length / static_cast<float>(terrain.Resolution);
		const int32 maxIndex = static_cast<int32>(terrain.Resolution);
		const int32 minX = std::clamp(static_cast<int32>(std::floor((local.x + halfW - radius) / stepX)), 0, maxIndex);
		const int32 maxX = std::clamp(static_cast<int32>(std::ceil((local.x + halfW + radius) / stepX)), 0, maxIndex);
		const int32 minZ = std::clamp(static_cast<int32>(std::floor((local.z + halfL - radius) / stepZ)), 0, maxIndex);
		const int32 maxZ = std::clamp(static_cast<int32>(std::ceil((local.z + halfL + radius) / stepZ)), 0, maxIndex);

		bool modified = false;
		for (int32 z = minZ; z <= maxZ; ++z)
		{
			for (int32 x = minX; x <= maxX; ++x)
			{
				const float dx = static_cast<float>(x) * stepX - halfW - local.x;
				const float dz = static_cast<float>(z) * stepZ - halfL - local.z;
				const float dist = std::sqrt(dx * dx + dz * dz);
				if (dist > radius) continue;
				const float factor = std::pow(std::clamp(1.0f - dist / radius, 0.0f, 1.0f), std::max(0.01f, falloff));
				modified |= visit(static_cast<std::size_t>(z) * axisCount + static_cast<std::size_t>(x), factor);
			}
		}
		return modified;
	}
}

bool TerrainUtility::EnsureLayerData(TerrainComponent& terrain)
{
	uint32 axisCount = 0;
	std::size_t sampleCount = 0;
	if (!TryGetHeightmapSize(terrain.Resolution, axisCount, sampleCount)) return false;
	const bool splatChanged = FitLayerPlane(terrain.SplatData, TerrainComponent::LayersPerPlane, terrain.Resolution, sampleCount, 255);
	const bool extraChanged = FitLayerPlane(terrain.SplatDataExtra, TerrainComponent::LayersPerPlane, terrain.Resolution, sampleCount, 0);
	const bool attributeChanged = FitLayerPlane(terrain.AttributeData, 1, terrain.Resolution, sampleCount, 0);
	if (splatChanged || extraChanged) terrain.IsSplatDirty = true;
	if (attributeChanged) terrain.IsAttributeDirty = true;
	return splatChanged || extraChanged || attributeChanged;
}

bool TerrainUtility::ApplyPaint(TerrainComponent& terrain, const vec3& hitWorldPos, const glm::mat4& worldMatrix, uint32 targetLayer, f32 radius, f32 intensity, f32 falloff)
{
	if (!(intensity > 0.0f) || targetLayer >= TerrainComponent::MaxLayers) return false;
	EnsureLayerData(terrain);

	const bool modified = ForEachBrushVertex(terrain, hitWorldPos, worldMatrix, radius, falloff, [&](std::size_t index, float factor)
		{
			uint8* planes[2] = { terrain.SplatData.data() + index * 4, terrain.SplatDataExtra.data() + index * 4 };
			float weights[TerrainComponent::MaxLayers];
			for (uint32 layer = 0; layer < TerrainComponent::MaxLayers; ++layer)
				weights[layer] = static_cast<float>(planes[layer / 4][layer % 4]);

			weights[targetLayer] = std::clamp(weights[targetLayer] + intensity * factor * 255.0f, 0.0f, 255.0f);
			float sum = 0.0f;
			for (const float weight : weights) sum += weight;
			if (!(sum > 0.0f)) return false;

			bool changed = false;
			for (uint32 layer = 0; layer < TerrainComponent::MaxLayers; ++layer)
			{
				const auto value = static_cast<uint8>(std::lround(std::clamp(weights[layer] / sum * 255.0f, 0.0f, 255.0f)));
				uint8& stored = planes[layer / 4][layer % 4];
				changed |= stored != value;
				stored = value;
			}
			return changed;
		});

	if (modified)
	{
		terrain.IsSplatDirty = true;
		terrain.IsDataDirty = true;
	}
	return modified;
}

bool TerrainUtility::ApplyAttributePaint(TerrainComponent& terrain, const vec3& hitWorldPos, const glm::mat4& worldMatrix, uint8 attribute, f32 radius)
{
	EnsureLayerData(terrain);
	const bool modified = ForEachBrushVertex(terrain, hitWorldPos, worldMatrix, radius, 1.0f, [&](std::size_t index, float)
		{
			if (terrain.AttributeData[index] == attribute) return false;
			terrain.AttributeData[index] = attribute;
			return true;
		});
	if (modified)
	{
		terrain.IsAttributeDirty = true;
		terrain.IsDataDirty = true;
	}
	return modified;
}

bool TerrainUtility::ResampleHeightData(const std::vector<f32>& source, uint32 sourceResolution,
	uint32 targetResolution, std::vector<f32>& result)
{
	uint32 sourceAxis = 0;
	uint32 targetAxis = 0;
	std::size_t sourceCount = 0;
	std::size_t targetCount = 0;

	if (!TryGetHeightmapSize(sourceResolution, sourceAxis, sourceCount) ||
		!TryGetHeightmapSize(targetResolution, targetAxis, targetCount) ||
		source.size() != sourceCount)
		return false;

	if (std::any_of(source.begin(), source.end(), [](f32 value) { return !std::isfinite(value); }))
		return false;

	std::vector<f32> candidate(targetCount);
	for (uint32 z = 0; z < targetAxis; ++z)
	{
		const f64 sourceZ = static_cast<f64>(z) * sourceResolution / targetResolution;
		const uint32 z0 = static_cast<uint32>(sourceZ);
		const uint32 z1 = std::min(z0 + 1, sourceResolution);
		const f64 tz = sourceZ - z0;
		const std::size_t row0 = static_cast<std::size_t>(z0) * sourceAxis;
		const std::size_t row1 = static_cast<std::size_t>(z1) * sourceAxis;

		for (uint32 x = 0; x < targetAxis; ++x)
		{
			const f64 sourceX = static_cast<f64>(x) * sourceResolution / targetResolution;
			const uint32 x0 = static_cast<uint32>(sourceX);
			const uint32 x1 = std::min(x0 + 1, sourceResolution);
			const f64 tx = sourceX - x0;

			const f64 top = source[row0 + x0] * (1.0 - tx) + source[row0 + x1] * tx;
			const f64 bottom = source[row1 + x0] * (1.0 - tx) + source[row1 + x1] * tx;
			const f32 height = static_cast<f32>(top * (1.0 - tz) + bottom * tz);
			if (!std::isfinite(height)) return false;

			candidate[static_cast<std::size_t>(z) * targetAxis + x] = height;
		}
	}

	result.swap(candidate);
	return true;
}

bool TerrainUtility::TryGetGridLayout(uint32 resolution, uint32& axisCount, std::size_t& sampleCount)
{
	// 지형 데이터 상한: 분할 1024(정점 1025). 파일 읽기·쓰기와 리소스 생성이 모두 이 검사를 거친다.
	if (resolution > TerrainComponent::MaxResolution) return false;
	TerrainGridMeshLayout layout;
	if (!TryGetTerrainGridMeshLayout(resolution, resolution, layout))
		return false;

	axisCount = layout.Columns;
	sampleCount = layout.VertexCount;
	return true;
}