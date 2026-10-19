#include "TerrainUtility.h"
#include "Vertex.h"
#include <vector>

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