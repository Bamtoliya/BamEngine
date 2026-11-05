#include "SplineMeshDeformer.h"

namespace
{
	bool TryStoreSplineDeformedPosition(const glm::dvec3& value, Engine::vec3& result)
	{
		const double limit = static_cast<double>((std::numeric_limits<float>::max)());
		for (int axis = 0; axis < 3; ++axis)
		{
			if (!std::isfinite(value[axis]) || std::abs(value[axis]) > limit)
				return false;
		}

		result = Engine::vec3(value);
		return true;
	}

	bool TryStoreSplineDeformedDirection(const glm::dvec3& value, Engine::vec3& result)
	{
		const double length = glm::length(value);
		if (!std::isfinite(length))
			return false;

		// 원본에서 생략된 방향 스트림의 0 벡터는 그대로 둡니다.
		result = length == 0.0 ? Engine::vec3(0.0f) : Engine::vec3(value / length);
		return true;
	}

	bool TryDeformSplineMaterial(const glm::dmat3& derivative, const Engine::VertexMaterial& source,
		Engine::VertexMaterial& result)
	{
		const glm::dmat3 normalAxes(
			glm::cross(derivative[1], derivative[2]),
			glm::cross(derivative[2], derivative[0]),
			glm::cross(derivative[0], derivative[1]));

		result = source;
		return TryStoreSplineDeformedDirection(normalAxes * glm::dvec3(source.normal), result.normal) &&
			TryStoreSplineDeformedDirection(derivative * glm::dvec3(source.tangent), result.tangent) &&
			TryStoreSplineDeformedDirection(derivative * glm::dvec3(source.binormal), result.binormal);
	}
}

namespace Engine
{
	bool SplineMeshDeformer::Build(const SplineFrameTable& frames, std::span<const VertexPosition> positions, std::span<const VertexMaterial> materials, f64 startDistance, f64 endDistance, SplineDeformedMeshData& outMesh, f64 uvRepeatDistance, Failure& outFailure)
	{
		outFailure = Failure::Unclassified;
		const f64 pathLength = frames.GetLength();
		if (positions.empty() || positions.size() > MaxVertexCount || positions.size() != materials.size() ||
			!std::isfinite(startDistance) || !std::isfinite(endDistance) ||
			startDistance < 0.0 || endDistance <= startDistance || endDistance > pathLength ||
			!std::isfinite(uvRepeatDistance) || uvRepeatDistance < 0.0)
			return false;

		f64 minZ = std::numeric_limits<f64>::infinity();
		f64 maxZ = -minZ;
		for (const auto& vertex : positions)
		{
			const auto& position = vertex.position;
			if (!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z))
				return false;

			const f64 z = static_cast<f64>(position.z);
			minZ = std::min(minZ, z);
			maxZ = std::max(maxZ, z);
		}

		const f64 sourceLength = maxZ - minZ;
		if (sourceLength <= 0.0)
		{
			outFailure = Failure::ZeroSourceZLength;
			return false;
		}

		const f64 spanLength = endDistance - startDistance;
		const f64 lengthScale = spanLength / sourceLength;
		const f64 sampleStep = pathLength * 1.0e-4;
		const bool closed = frames.IsClosed();
		const f32 limit = (std::numeric_limits<f32>::max)();

		f64 uvRepeatCount = 0.0;
		if (uvRepeatDistance > 0.0)
		{
			uvRepeatCount = spanLength / uvRepeatDistance;
			if (!std::isfinite(uvRepeatCount) || uvRepeatCount > static_cast<f64>(limit))
				return false;

			if (closed && startDistance == 0.0 && endDistance == pathLength)
				uvRepeatCount = std::max(1.0, std::round(uvRepeatCount));
		}

		SplineDeformedMeshData candidate;
		candidate.Positions.resize(positions.size());
		candidate.Materials.resize(materials.size());
		candidate.Bounds = BoundingBox{ vec3(limit), vec3(-limit) };

		for (size_t i = 0; i < positions.size(); ++i)
		{
			const glm::dvec3 local(positions[i].position);
			const f64 alpha = (local.z - minZ) / sourceLength;
			const f64 mappedDistance = std::lerp(startDistance, endDistance, alpha);
			const f64 distance = closed && mappedDistance == pathLength ? 0.0 : mappedDistance;
			const f64 lower = distance - sampleStep;
			const f64 upper = distance + sampleStep;

			const f64 beforeDistance = closed && lower < 0.0 ? lower + pathLength : std::max(0.0, lower);
			const f64 afterDistance = closed && upper > pathLength ? upper - pathLength : std::min(pathLength, upper);
			const f64 sampleSpan = closed ? upper - lower : afterDistance - beforeDistance;

			mat4 frame;
			mat4 before;
			mat4 after;
			if (sampleSpan <= 0.0 || !frames.TryEvaluate(distance, frame) ||
				!frames.TryEvaluate(beforeDistance, before) || !frames.TryEvaluate(afterDistance, after))
				return false;

			const glm::dvec3 right(frame[0]);
			const glm::dvec3 up(frame[1]);
			const glm::dvec3 forward(frame[2]);
			const glm::dvec3 center(frame[3]);
			const glm::dvec3 position = center + right * local.x + up * local.y;

			const glm::dvec3 rightChange = glm::dvec3(after[0]) - glm::dvec3(before[0]);
			const glm::dvec3 upChange = glm::dvec3(after[1]) - glm::dvec3(before[1]);
			const glm::dvec3 offsetChange = (rightChange * local.x + upChange * local.y) / sampleSpan;
			const glm::dvec3 along = forward + offsetChange;

			// 곡선 안쪽에서 단면이 접혀 길이 방향이 역전되는 경우를 거부합니다.
			const f64 signedScale = glm::dot(glm::cross(right, up), along);
			if (!std::isfinite(signedScale))
				return false;

			if (signedScale <= 1.0e-6)
			{
				outFailure = Failure::FoldedSection;
				return false;
			}

			const glm::dmat3 derivative(right, up, along * lengthScale);
			auto& outputPosition = candidate.Positions[i].position;
			if (!TryStoreSplineDeformedPosition(position, outputPosition) ||
				!TryDeformSplineMaterial(derivative, materials[i], candidate.Materials[i]))
				return false;

			if (uvRepeatDistance > 0.0)
				candidate.Materials[i].texCoord.y = static_cast<f32>(alpha * uvRepeatCount);

			candidate.Bounds.min = glm::min(candidate.Bounds.min, outputPosition);
			candidate.Bounds.max = glm::max(candidate.Bounds.max, outputPosition);
		}

		outMesh = std::move(candidate);
		outFailure = Failure::None;
		return true;
	}
	bool SplineMeshDeformer::Build(const SplineFrameTable& frames, std::span<const VertexPosition> positions,
		std::span<const VertexMaterial> materials, f64 startDistance, f64 endDistance,
		SplineDeformedMeshData& outMesh, f64 uvRepeatDistance)
	{
		Failure failure{};
		return Build(frames, positions, materials, startDistance, endDistance, outMesh, uvRepeatDistance, failure);
	}
}