#pragma once

#include "SplineFrameTable.h"
#include "Vertex.h"
#include "BoundingBox.h"

namespace Engine
{
	struct SplineDeformedMeshData
	{
		std::vector<VertexPosition> Positions;
		std::vector<VertexMaterial> Materials;
		BoundingBox Bounds;
	};

	class ENGINE_API SplineMeshDeformer final
	{
	public:
		enum class Failure : uint8
		{
			None,
			Unclassified,
			ZeroSourceZLength,
			FoldedSection
		};

		static bool Build(const SplineFrameTable& frames, std::span<const VertexPosition> positions,
			std::span<const VertexMaterial> materials, f64 startDistance, f64 endDistance,
			SplineDeformedMeshData& outMesh, f64 uvRepeatDistance, Failure& outFailure);

		static constexpr size_t MaxVertexCount = 16384;

		// 최소/최대 Z를 [startDistance, endDistance]에 대응시킵니다.
		// 결과는 frames와 같은 좌표계이며, 원본 정점 순서를 따릅니다.
		// 실패하면 outMesh를 보존합니다.
		static bool Build(const SplineFrameTable& frames, std::span<const VertexPosition> positions,
			std::span<const VertexMaterial> materials, f64 startDistance, f64 endDistance,
			SplineDeformedMeshData& outMesh, f64 uvRepeatDistance = 0.0);
	};
}