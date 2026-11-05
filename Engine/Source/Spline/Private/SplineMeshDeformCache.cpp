#include "SplineMeshDeformCache.h"
#include "SplineComponent.h"
#include "SplineMeshComponent.h"
#include "SplineMeshDeformer.h"
#include "CoreComponents.h"
#include "Components/Render/StaticMeshRendererComponent.h"
#include "Mesh.h"
#include "Renderer.h"

namespace
{
	using namespace Engine;

	constexpr size_t MaxSplineDeformedIndexCount = SplineMeshDeformer::MaxVertexCount * 6;

	struct SplineDeformInput
	{
		const SplineComponent* Spline = nullptr;
		const SplineMeshComponent* Settings = nullptr;
		const Mesh* Source = nullptr;
		Handle SourceHandle;
		mat4 ShapeMatrix{ 1.0f };
		mat4 TranslationMatrix{ 1.0f };
	};

	struct CachedSplineDeformation
	{
		Handle SourceHandle;
		uint64 GeometryVersion = 0;
		std::vector<SplinePoint> Points;
		bool Closed = false;
		mat4 ShapeMatrix{ 1.0f };
		vec3 InitialUp{ 0.0f };
		f64 StartDistance = 0.0;
		f64 UVRepeatDistance = 0.0;
		std::shared_ptr<Mesh> Geometry;
		ESplineMeshBuildStatus FailureStatus = ESplineMeshBuildStatus::DeformationFailed;

		bool Matches(const SplineDeformInput& input) const
		{
			return SourceHandle == input.SourceHandle && GeometryVersion == input.Source->GetGeometryVersion() &&
				Points == input.Spline->Points && Closed == input.Spline->Closed && ShapeMatrix == input.ShapeMatrix &&
				InitialUp == input.Settings->InitialUp && StartDistance == input.Settings->StartDistance &&
				UVRepeatDistance == input.Settings->UVRepeatDistance;
		}
	};

	struct SplineDeformSceneCache
	{
		std::unordered_map<entt::entity, CachedSplineDeformation> Entries;
	};

	bool TryReadSplineDeformInput(const entt::registry& world, entt::entity entity, const mat4& worldMatrix,
		SplineDeformInput& input)
	{
		if (!world.valid(entity) || worldMatrix[3].w != 1.0f)
			return false;

		const auto* settings = world.try_get<SplineMeshComponent>(entity);
		const auto* spline = world.try_get<SplineComponent>(entity);
		const auto* renderer = world.try_get<StaticMeshRendererComponent>(entity);

		if (!settings || !settings->DeformToPath || !spline || !spline->IsValid() || !renderer)
			return false;

		if (!std::isfinite(settings->StartDistance) || settings->StartDistance < 0.0 ||
			!std::isfinite(settings->InitialUp.x) || !std::isfinite(settings->InitialUp.y) ||
			!std::isfinite(settings->InitialUp.z))
			return false;

		if (!std::isfinite(settings->UVRepeatDistance) || settings->UVRepeatDistance < 0.0)
			return false;

		const Mesh* source = renderer->meshHandle.Get();
		if (!source)
			return false;

		input.Spline = spline;
		input.Settings = settings;
		input.Source = source;
		input.SourceHandle = renderer->meshHandle.GetRawHandle();
		input.ShapeMatrix = worldMatrix;
		input.ShapeMatrix[3] = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		input.TranslationMatrix[3] = worldMatrix[3];
		return true;
	}

	bool TryCopySplineDeformIndices(const Mesh& source, std::vector<uint32>& indices)
	{
		const uint32 count = source.GetIndexCount();
		const uint32 stride = source.GetIndexStride();
		const auto& raw = source.GetRawIndexData();
		const uint32 vertexCount = source.GetVertexCount();

		if (count == 0)
			return raw.empty() && vertexCount % 3 == 0;

		if (count % 3 != 0 || count > MaxSplineDeformedIndexCount ||
			(stride != sizeof(uint16) && stride != sizeof(uint32)) ||
			raw.size() != static_cast<size_t>(count) * stride)
			return false;

		indices.resize(count);
		for (size_t i = 0; i < count; ++i)
		{
			const auto* bytes = raw.data() + i * stride;
			uint32 index = 0;
			if (stride == sizeof(uint16))
			{
				uint16 shortIndex = 0;
				std::memcpy(&shortIndex, bytes, sizeof(shortIndex));
				index = shortIndex;
			}
			else
			{
				std::memcpy(&index, bytes, sizeof(index));
			}

			if (index >= vertexCount)
				return false;

			indices[i] = index;
		}

		return true;
	}

	std::shared_ptr<Mesh> CreateSplineDeformedMesh(const SplineDeformInput& input, ESplineMeshBuildStatus& outFailure)
	{
		static_assert(std::is_trivially_copyable_v<VertexPosition>);
		static_assert(std::is_trivially_copyable_v<VertexMaterial>);

		const Mesh& source = *input.Source;
		const uint32 count = source.GetVertexCount();
		if (count > SplineMeshDeformer::MaxVertexCount || source.GetIndexCount() > MaxSplineDeformedIndexCount)
		{
			outFailure = ESplineMeshBuildStatus::SourceLimitExceeded;
			return {};
		}
		const auto& rawPositions = source.GetRawStreamData(EMeshStream::Position);
		const auto& rawMaterials = source.GetRawStreamData(EMeshStream::Material);

		if (source.IsDynamic() || source.GetTopology() != ETopology::TriangleList ||
			source.GetStreamCount(EMeshStream::SkinData) != 0 ||
			count < 3 || count > SplineMeshDeformer::MaxVertexCount ||
			source.GetStreamCount(EMeshStream::Material) != count ||
			source.GetStreamStride(EMeshStream::Position) != sizeof(VertexPosition) ||
			source.GetStreamStride(EMeshStream::Material) != sizeof(VertexMaterial) ||
			rawPositions.size() != static_cast<size_t>(count) * sizeof(VertexPosition) ||
			rawMaterials.size() != static_cast<size_t>(count) * sizeof(VertexMaterial))
			return {};

		std::vector<uint32> indices;
		if (!TryCopySplineDeformIndices(source, indices))
			return {};

		std::array<vec3, SplineComponent::MaxControlPoints> points{};
		std::array<f32, SplineComponent::MaxControlPoints> rolls{};
		const size_t pointCount = input.Spline->Points.size();

		for (size_t i = 0; i < pointCount; ++i)
		{
			points[i] = input.Spline->Points[i].Position;
			rolls[i] = input.Spline->Points[i].RollDegrees;
		}

		SplineFrameTable frames;
		if (!frames.Build(std::span<const vec3>(points.data(), pointCount), input.Spline->Closed,
			input.ShapeMatrix, input.Settings->InitialUp, std::span<const f32>(rolls.data(), pointCount)))
			return {};

		if (frames.GetLength() == 0.0)
		{
			outFailure = ESplineMeshBuildStatus::InvalidPath;
			return {};
		}

		if (input.Settings->StartDistance >= frames.GetLength())
		{
			outFailure = ESplineMeshBuildStatus::StartDistanceAtOrPastEnd;
			return {};
		}

		std::vector<VertexPosition> positions(count);
		std::vector<VertexMaterial> materials(count);
		std::memcpy(positions.data(), rawPositions.data(), rawPositions.size());
		std::memcpy(materials.data(), rawMaterials.data(), rawMaterials.size());

		SplineDeformedMeshData deformed;
		using DeformFailure = SplineMeshDeformer::Failure;
		DeformFailure failure{};

		if (!SplineMeshDeformer::Build(frames, positions, materials, input.Settings->StartDistance,
			frames.GetLength(), deformed, input.Settings->UVRepeatDistance, failure))
		{
			if (failure == DeformFailure::ZeroSourceZLength)
				outFailure = ESplineMeshBuildStatus::ZeroSourceZLength;
			else if (failure == DeformFailure::FoldedSection)
				outFailure = ESplineMeshBuildStatus::FoldedSection;

			return {};
		}

		MeshCreateDesc desc{};
		desc.Streams[static_cast<uint32>(EMeshStream::Position)] =
		{ deformed.Positions.data(), count, static_cast<uint32>(sizeof(VertexPosition)) };
		desc.Streams[static_cast<uint32>(EMeshStream::Material)] =
		{ deformed.Materials.data(), count, static_cast<uint32>(sizeof(VertexMaterial)) };

		if (!indices.empty())
		{
			desc.IndexData = indices.data();
			desc.IndexCount = static_cast<uint32>(indices.size());
			desc.IndexStride = sizeof(uint32);
		}

		desc.BoundingBoxMin = deformed.Bounds.min;
		desc.BoundingBoxMax = deformed.Bounds.max;
		desc.Flags = EMeshFlag::KeepRawData | EMeshFlag::HasPosition | EMeshFlag::HasNormal |
			EMeshFlag::HasTexCoord | EMeshFlag::HasTangent | EMeshFlag::HasBinormal | EMeshFlag::HasColor;

		Mesh* mesh = Mesh::Create(&desc);
		if (!mesh)
			return {};

		return std::shared_ptr<Mesh>(mesh, [](Mesh* value)
			{
				value->Free();
				delete value;
			});
	}

	SplineDeformedMeshInstance MakeSplineDeformedInstance(const CachedSplineDeformation& entry,
		const mat4& translation)
	{
		if (!entry.Geometry)
			return { .BuildStatus = entry.FailureStatus };

		const BoundingBox bounds(entry.Geometry->GetMin(), entry.Geometry->GetMax());
		BoundingBox worldBounds;
		if (!bounds.TryTransformToAABB(translation, worldBounds))
			return {};

		return { entry.Geometry, translation, worldBounds, ESplineMeshBuildStatus::Ready };
	}
}

namespace Engine
{
	SplineDeformedMeshInstance SplineMeshDeformCache::GetOrBuild(entt::registry& world, entt::entity entity,
		const mat4& worldMatrix)
	{
		SplineDeformInput input;
		if (!TryReadSplineDeformInput(world, entity, worldMatrix, input))
			return {};

		auto& entries = world.ctx().emplace<SplineDeformSceneCache>().Entries;
		const auto found = entries.find(entity);
		if (found != entries.end() && found->second.Matches(input))
			return MakeSplineDeformedInstance(found->second, input.TranslationMatrix);

		if (!Renderer::IsCreated() || !Renderer::Get().GetRHI())
			return {};

		CachedSplineDeformation candidate;
		candidate.SourceHandle = input.SourceHandle;
		candidate.GeometryVersion = input.Source->GetGeometryVersion();
		candidate.Points = input.Spline->Points;
		candidate.Closed = input.Spline->Closed;
		candidate.ShapeMatrix = input.ShapeMatrix;
		candidate.InitialUp = input.Settings->InitialUp;
		candidate.StartDistance = input.Settings->StartDistance;
		candidate.UVRepeatDistance = input.Settings->UVRepeatDistance;
		candidate.Geometry = CreateSplineDeformedMesh(input, candidate.FailureStatus);

		const auto inserted = entries.insert_or_assign(entity, std::move(candidate));
		return MakeSplineDeformedInstance(inserted.first->second, input.TranslationMatrix);
	}

	SplineDeformedMeshInstance SplineMeshDeformCache::Find(const entt::registry& world, entt::entity entity,
		const mat4& worldMatrix)
	{
		const auto* cache = world.ctx().find<SplineDeformSceneCache>();
		if (!cache)
			return {};

		const auto found = cache->Entries.find(entity);
		if (found == cache->Entries.end())
			return {};

		SplineDeformInput input;
		if (!TryReadSplineDeformInput(world, entity, worldMatrix, input) || !found->second.Matches(input))
			return {};

		return MakeSplineDeformedInstance(found->second, input.TranslationMatrix);
	}

	void SplineMeshDeformCache::Prune(entt::registry& world)
	{
		auto* cache = world.ctx().find<SplineDeformSceneCache>();
		if (!cache)
			return;

		std::erase_if(cache->Entries, [&world](const auto& item)
			{
				const entt::entity entity = item.first;
				if (!world.valid(entity) ||
					!world.all_of<SplineComponent, StaticMeshRendererComponent, WorldTransformComponent, FlagComponent>(entity))
					return true;

				const auto* settings = world.try_get<SplineMeshComponent>(entity);
				const auto& renderer = world.get<StaticMeshRendererComponent>(entity);
				return !settings || !settings->DeformToPath || !renderer.meshHandle.IsValid();
			});
	}
}