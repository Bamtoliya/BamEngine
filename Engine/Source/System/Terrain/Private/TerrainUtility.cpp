#include "TerrainUtility.h"
#include "Vertex.h"
#include <vector>
#include "TerrainComponent.h"
#include <algorithm>
#include <cmath>
#include <stb_image.h>
#include <cstddef>
#include <limits>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <filesystem>

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
}

Mesh* TerrainUtility::CreateGridMesh(float width, float length, uint32 resolution)
{
	if (resolution == 0 || width <= 0.0f || length <= 0.0f) return nullptr;

	const uint32 vertCountX = resolution + 1;
	const uint32 vertCountZ = resolution + 1;
	const uint32 totalVertices = vertCountX * vertCountZ;
	const uint32 totalQuads = resolution * resolution;
	const uint32 totalIndices = totalQuads * 6;

	std::vector<VertexPosition> positions(totalVertices);
	std::vector<VertexMaterial> materials(totalVertices);
	std::vector<uint32> indices(totalIndices);

	const float dx = width / static_cast<float>(resolution);
	const float dz = length / static_cast<float>(resolution);
	const float startX = -width * 0.5f;
	const float startZ = -length * 0.5f;

	// 1. 정점 데이터 생성 (XZ 평면, Y=0, UV는 0.0 ~ 1.0)
	uint32 vIdx = 0;
	for (uint32 z = 0; z < vertCountZ; ++z)
	{
		const float currentZ = startZ + static_cast<float>(z) * dz;
		const float v = static_cast<float>(z) / static_cast<float>(resolution);

		for (uint32 x = 0; x < vertCountX; ++x)
		{
			const float currentX = startX + static_cast<float>(x) * dx;
			const float u = static_cast<float>(x) / static_cast<float>(resolution);

			positions[vIdx].position = vec3(currentX, 0.0f, currentZ);
			materials[vIdx].texCoord = vec2(u, v);
			materials[vIdx].normal = vec3(0.0f, 1.0f, 0.0f);
			materials[vIdx].tangent = vec3(1.0f, 0.0f, 0.0f);
			materials[vIdx].binormal = vec3(0.0f, 0.0f, 1.0f);
			materials[vIdx].color = vec4(1.0f);
			++vIdx;
		}
	}

	// 2. 인덱스 데이터 생성 (시계 방향 와인딩)
	uint32 iIdx = 0;
	for (uint32 z = 0; z < resolution; ++z)
	{
		for (uint32 x = 0; x < resolution; ++x)
		{
			const uint32 i0 = z * vertCountX + x;
			const uint32 i1 = i0 + 1;
			const uint32 i2 = (z + 1) * vertCountX + x;
			const uint32 i3 = i2 + 1;

			// 첫 번째 삼각형 (i0 -> i2 -> i1)
			indices[iIdx++] = i0;
			indices[iIdx++] = i2;
			indices[iIdx++] = i1;

			// 두 번째 삼각형 (i1 -> i2 -> i3)
			indices[iIdx++] = i1;
			indices[iIdx++] = i2;
			indices[iIdx++] = i3;
		}
	}

	// 3. BamEngine MeshCreateDesc 구성
	MeshCreateDesc desc{};
	desc.Streams[static_cast<uint32>(EMeshStream::Position)] = { positions.data(), totalVertices, sizeof(VertexPosition) };
	desc.Streams[static_cast<uint32>(EMeshStream::Material)] = { materials.data(), totalVertices, sizeof(VertexMaterial) };
	desc.IndexData = indices.data();
	desc.IndexCount = totalIndices;
	desc.IndexStride = sizeof(uint32);
	desc.BoundingBoxMin = vec3(startX, 0.0f, startZ);
	desc.BoundingBoxMax = vec3(-startX, 0.0f, -startZ);
	desc.Flags = EMeshFlag::KeepRawData | EMeshFlag::HasPosition | EMeshFlag::HasNormal | EMeshFlag::HasTexCoord | EMeshFlag::HasTangent;

	return Mesh::Create(&desc);
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
	// 2. 영향받는 사각 영역(AABB) 정점 인덱스 산출 (전체 순회 방지 최적화)
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
	if (modified) terrain.IsDirty = true;
	return modified;
}

bool TerrainUtility::LoadHeightmapFromFile(TerrainComponent& terrain, const std::wstring& filePath)
{
	if (terrain.Resolution == 0) return false;

	const std::string u8Path = std::filesystem::path(filePath).string();
	int imgW = 0;
	int imgH = 0;
	int channels = 0;
	// 1채널(그레이스케일)로 강제 변환하여 로드
	unsigned char* pixels = stbi_load(u8Path.c_str(), &imgW, &imgH, &channels, 1);
	if (!pixels || imgW <= 0 || imgH <= 0)
	{
		if (pixels) stbi_image_free(pixels);
		return false;
	}

	const uint32 vertexAxisCount = terrain.Resolution + 1;
	terrain.HeightData.resize(vertexAxisCount * vertexAxisCount);

	// 이미지 해상도와 터레인 정점 그리드가 달라도 바이리니어(Bilinear) 샘플링으로 매끄럽게 리샘플링
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

			const float p00 = static_cast<float>(pixels[y0 * imgW + x0]) / 255.0f;
			const float p10 = static_cast<float>(pixels[y0 * imgW + x1]) / 255.0f;
			const float p01 = static_cast<float>(pixels[y1 * imgW + x0]) / 255.0f;
			const float p11 = static_cast<float>(pixels[y1 * imgW + x1]) / 255.0f;

			const float val = glm::mix(glm::mix(p00, p10, tx), glm::mix(p01, p11, tx), ty);
			terrain.HeightData[z * vertexAxisCount + x] = val;
		}
	}

	stbi_image_free(pixels);
	terrain.IsDirty = true;
	return true;
}

bool TerrainUtility::SaveHeightmapToFile(const TerrainComponent& terrain, const std::wstring& filePath)
{
	uint32 vertexAxisCount = 0;
	std::size_t sampleCount = 0;
	if (!TryGetHeightDataLayout(terrain, vertexAxisCount, sampleCount)) return false;

	std::vector<unsigned char> outPixels(sampleCount);
	for (std::size_t i = 0; i < sampleCount; ++i)
	{
		const f32 height = terrain.HeightData[i];
		if (!std::isfinite(height)) return false;
		outPixels[i] = static_cast<unsigned char>(glm::clamp(height, 0.0f, 1.0f) * 255.0f);
	}

	const std::string u8Path = std::filesystem::path(filePath).string();
	const int side = static_cast<int>(vertexAxisCount);
	return stbi_write_png(u8Path.c_str(), side, side, 1, outPixels.data(), side) != 0;
}

void TerrainUtility::ApplyPaint(TerrainComponent& terrain, const vec3& hitPoint, uint32 targetLayer, f32 radius, f32 intensity, f32 falloff)
{
	if (radius <= 0.0f || intensity <= 0.0f || targetLayer >= 4) return;

	const uint32 vertexAxisCount = terrain.Resolution + 1;
	const uint32 totalVertexCount = vertexAxisCount * vertexAxisCount;
	if (terrain.SplatData.size() != totalVertexCount * 4)
	{
		terrain.SplatData.assign(totalVertexCount * 4, 0);
		// 기본적으로 Layer 0(잔디)을 255로 초기화
		for (uint32 i = 0; i < totalVertexCount; ++i)
			terrain.SplatData[i * 4 + 0] = 255;
		terrain.IsSplatDirty = true;
	}

	const float halfW = terrain.Width * 0.5f;
	const float halfL = terrain.Length * 0.5f;
	const float localX = hitPoint.x + halfW;
	const float localZ = hitPoint.z + halfL;

	const float stepX = terrain.Width / static_cast<float>(terrain.Resolution);
	const float stepZ = terrain.Length / static_cast<float>(terrain.Resolution);

	const int32 minX = std::clamp(static_cast<int32>(std::floor((localX - radius) / stepX)), 0, static_cast<int32>(terrain.Resolution));
	const int32 maxX = std::clamp(static_cast<int32>(std::ceil((localX + radius) / stepX)), 0, static_cast<int32>(terrain.Resolution));
	const int32 minZ = std::clamp(static_cast<int32>(std::floor((localZ - radius) / stepZ)), 0, static_cast<int32>(terrain.Resolution));
	const int32 maxZ = std::clamp(static_cast<int32>(std::ceil((localZ + radius) / stepZ)), 0, static_cast<int32>(terrain.Resolution));

	for (int32 z = minZ; z <= maxZ; ++z)
	{
		for (int32 x = minX; x <= maxX; ++x)
		{
			const float vx = static_cast<float>(x) * stepX - halfW;
			const float vz = static_cast<float>(z) * stepZ - halfL;
			const float dist = std::sqrt((vx - hitPoint.x) * (vx - hitPoint.x) + (vz - hitPoint.z) * (vz - hitPoint.z));
			if (dist > radius) continue;

			const float t = dist / radius;
			const float factor = std::pow(std::clamp(1.0f - t, 0.0f, 1.0f), std::max(0.01f, falloff));
			const float delta = intensity * factor * 255.0f;

			const uint32 pixelIndex = (z * vertexAxisCount + x) * 4;
			float weights[4] = {
				static_cast<float>(terrain.SplatData[pixelIndex + 0]),
				static_cast<float>(terrain.SplatData[pixelIndex + 1]),
				static_cast<float>(terrain.SplatData[pixelIndex + 2]),
				static_cast<float>(terrain.SplatData[pixelIndex + 3])
			};

			weights[targetLayer] = std::clamp(weights[targetLayer] + delta, 0.0f, 255.0f);

			// 레이어들의 합이 255를 유지하도록 정규화
			float sum = weights[0] + weights[1] + weights[2] + weights[3];
			if (sum > 0.0f)
			{
				for (int i = 0; i < 4; ++i)
					terrain.SplatData[pixelIndex + i] = static_cast<uint8>(std::clamp((weights[i] / sum) * 255.0f, 0.0f, 255.0f));
			}
		}
	}

	terrain.IsSplatDirty = true;
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