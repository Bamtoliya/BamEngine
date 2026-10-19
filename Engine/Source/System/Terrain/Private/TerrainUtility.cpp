#include "TerrainUtility.h"
#include "Vertex.h"
#include <vector>
#include "TerrainComponent.h"
#include <algorithm>
#include <cmath>

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
	if (terrain.HeightData.empty() || terrain.Resolution == 0 || terrain.MaxHeight <= 0.0001f || params.Radius <= 0.0001f)
		return false;
	// 1. 월드 좌표를 터레인 로컬 좌표계로 역변환
	const glm::mat4 invWorld = glm::inverse(worldMatrix);
	const glm::vec3 localPos = glm::vec3(invWorld * glm::vec4(hitWorldPos, 1.0f));
	const float halfWidth = terrain.Width * 0.5f;
	const float halfLength = terrain.Length * 0.5f;
	if (localPos.x < -halfWidth || localPos.x > halfWidth || localPos.z < -halfLength || localPos.z > halfLength)
		return false;
	const uint32 vertexAxisCount = terrain.Resolution + 1;
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