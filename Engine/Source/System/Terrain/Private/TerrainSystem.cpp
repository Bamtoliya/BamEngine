#include "TerrainSystem.h"
#include "TerrainComponent.h"
#include "CoreComponents.h"
#include "TerrainUtility.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "PickingManager.h"
#include "Texture.h"

namespace
{
	bool TryInferTerrainResolution(std::size_t sampleCount, uint32& resolution)
	{
		const uint64 maxSamples = (std::numeric_limits<uint32>::max)() / sizeof(f32);
		if (sampleCount < 4 || sampleCount > maxSamples)
			return false;

		const uint64 axis = static_cast<uint64>(std::sqrt(static_cast<double>(sampleCount)));
		if (axis * axis != sampleCount)
			return false;

		resolution = static_cast<uint32>(axis - 1);
		return true;
	}

	bool TryResampleTerrainSplatData(const std::vector<uint8>& source, uint32 targetResolution,
		std::size_t targetCount, std::vector<uint8>& result)
	{
		uint32 sourceResolution = 0;
		if (source.size() % 4 != 0 ||
			!TryInferTerrainResolution(source.size() / 4, sourceResolution))
			return false;

		const std::size_t sourceAxis = static_cast<std::size_t>(sourceResolution) + 1;
		const std::size_t targetAxis = static_cast<std::size_t>(targetResolution) + 1;
		std::vector<uint8> candidate(targetCount * 4);

		for (std::size_t z = 0; z < targetAxis; ++z)
		{
			const uint64 sourceZ = (static_cast<uint64>(z) * sourceResolution +
				targetResolution / 2) / targetResolution;

			for (std::size_t x = 0; x < targetAxis; ++x)
			{
				const uint64 sourceX = (static_cast<uint64>(x) * sourceResolution +
					targetResolution / 2) / targetResolution;
				const std::size_t sourceIndex = (sourceZ * sourceAxis + sourceX) * 4;
				const std::size_t targetIndex = (z * targetAxis + x) * 4;
				std::copy_n(source.data() + sourceIndex, 4, candidate.data() + targetIndex);
			}
		}

		result.swap(candidate);
		return true;
	}
}

EResult TerrainSystem::Initialize(void* arg)
{
	(void)arg;
	return EResult::Success;
}

void TerrainSystem::Free()
{
}

TerrainSystem* TerrainSystem::Create(void* arg)
{
	auto* instance = new TerrainSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

bool TerrainSystem::EnsureTerrainResources(TerrainComponent& terrain)
{
	uint32 axisCount = 0;
	std::size_t sampleCount = 0;

	if (!std::isfinite(terrain.Width) || terrain.Width <= 0.f ||
		!std::isfinite(terrain.Length) || terrain.Length <= 0.f ||
		!std::isfinite(terrain.MaxHeight) || terrain.MaxHeight < 0.f ||
		!TerrainUtility::TryGetGridLayout(terrain.Resolution, axisCount, sampleCount))
		return false;

	const f32 stepX = terrain.Width / static_cast<f32>(terrain.Resolution);
	const f32 stepZ = terrain.Length / static_cast<f32>(terrain.Resolution);
	if (!std::isfinite(stepX) || stepX <= 0.f || !std::isfinite(stepZ) || stepZ <= 0.f)
		return false;

	const bool heightLayoutMatches = terrain.HeightData.size() == sampleCount;
	const bool splatLayoutMatches = terrain.SplatData.size() == sampleCount * 4;

	const bool meshMatches = terrain.GridMesh &&
		terrain.GridMesh->GetVertexCount() == sampleCount &&
		terrain.GridMesh->GetMin().x == -terrain.Width * 0.5f &&
		terrain.GridMesh->GetMax().x == terrain.Width * 0.5f &&
		terrain.GridMesh->GetMin().z == -terrain.Length * 0.5f &&
		terrain.GridMesh->GetMax().z == terrain.Length * 0.5f;

	const auto textureMatches = [&](const std::shared_ptr<RHITexture>& texture, ETextureFormat format)
		{
			return texture && texture->GetWidth() == axisCount &&
				texture->GetHeight() == axisCount && texture->GetFormat() == format;
		};

	const bool heightTextureMatches = textureMatches(terrain.HeightmapTexture, ETextureFormat::R32_FLOAT);
	const bool splatTextureMatches = textureMatches(terrain.SplatmapTexture, ETextureFormat::R8G8B8A8_UNORM);

	if (heightLayoutMatches && splatLayoutMatches && meshMatches &&
		heightTextureMatches && splatTextureMatches)
		return true;

	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi)
		return false;

	TerrainComponent candidate = terrain;

	for (const f32 height : candidate.HeightData)
	{
		if (!std::isfinite(height) || height < 0.f || height > 1.f)
			return false;
	}

	if (!heightLayoutMatches)
	{
		if (candidate.HeightData.empty())
		{
			candidate.HeightData.resize(sampleCount);
			const f32 center = static_cast<f32>(terrain.Resolution) * 0.5f;

			for (uint32 z = 0; z < axisCount; ++z)
			{
				for (uint32 x = 0; x < axisCount; ++x)
				{
					const f32 dx = (static_cast<f32>(x) - center) / center;
					const f32 dz = (static_cast<f32>(z) - center) / center;
					candidate.HeightData[static_cast<std::size_t>(z) * axisCount + x] =
						std::max(0.f, 1.f - dx * dx - dz * dz);
				}
			}
		}
		else
		{
			uint32 sourceResolution = 0;
			if (!TryInferTerrainResolution(candidate.HeightData.size(), sourceResolution) ||
				!TerrainUtility::ResampleHeightData(
					candidate.HeightData, sourceResolution, terrain.Resolution, candidate.HeightData))
				return false;
		}
	}

	if (!splatLayoutMatches)
	{
		if (candidate.SplatData.empty())
		{
			candidate.SplatData.assign(sampleCount * 4, 0);
			for (std::size_t index = 0; index < sampleCount; ++index)
				candidate.SplatData[index * 4] = 255;
		}
		else if (!TryResampleTerrainSplatData(
			candidate.SplatData, terrain.Resolution, sampleCount, candidate.SplatData))
		{
			return false;
		}
	}

	if (!meshMatches)
	{
		Mesh* mesh = TerrainUtility::CreateGridMesh(terrain.Width, terrain.Length, terrain.Resolution);
		if (!mesh)
			return false;

		candidate.GridMesh = std::shared_ptr<Mesh>(mesh, [](Mesh* value)
			{
				value->Free();
				delete value;
			});
	}

	const auto createTexture = [&](ETextureFormat format, void* data, uint32 dataSize)
		{
			RHITextureDesc desc{};
			desc.width = axisCount;
			desc.height = axisCount;
			desc.format = format;
			desc.dimension = ETextureDimension::Texture2D;
			desc.usage = ETextureUsage::Sampler;
			desc.data = data;
			desc.dataSize = dataSize;

			return std::shared_ptr<RHITexture>(rhi->CreateTexture(desc),
				[](RHITexture* value) { Safe_Release(value); });
		};

	if (!heightLayoutMatches || !heightTextureMatches)
	{
		candidate.HeightmapTexture = createTexture(ETextureFormat::R32_FLOAT,
			candidate.HeightData.data(), static_cast<uint32>(sampleCount * sizeof(f32)));

		if (!candidate.HeightmapTexture)
			return false;

		candidate.IsDirty = false;
		candidate.HeightDirtyRegion.reset();
	}

	if (!splatLayoutMatches || !splatTextureMatches)
	{
		candidate.SplatmapTexture = createTexture(ETextureFormat::R8G8B8A8_UNORM,
			candidate.SplatData.data(), static_cast<uint32>(sampleCount * 4));

		if (!candidate.SplatmapTexture)
			return false;

		candidate.IsSplatDirty = false;
	}

	terrain = std::move(candidate);
	return true;
}

void TerrainSystem::UpdateTerrainHeightmap(TerrainComponent& terrain)
{
	if (!terrain.IsDirty || !terrain.HeightmapTexture) return;

	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return;

	uint32 axisCount = 0;
	std::size_t sampleCount = 0;
	if (!TerrainUtility::TryGetGridLayout(terrain.Resolution, axisCount, sampleCount) ||
		terrain.HeightData.size() != sampleCount)
		return;

	const PixelRect fullRegion{ 0, 0, axisCount, axisCount };
	PixelRect region = terrain.HeightDirtyRegion.value_or(fullRegion);

	// 이전 해상도의 영역 등이 남아 있다면 전체 갱신으로 처리합니다.
	if (region.IsEmpty() || region.maxX > axisCount || region.maxY > axisCount)
		region = fullRegion;

	RHITextureDesc desc{};
	desc.width = region.Width();
	desc.height = region.Height();
	desc.format = ETextureFormat::R32_FLOAT;
	desc.dimension = ETextureDimension::Texture2D;
	desc.usage = ETextureUsage::Sampler;
	desc.dataSize = static_cast<uint32>(static_cast<uint64>(desc.width) * desc.height * sizeof(f32));

	std::vector<f32> uploadData;
	const bool isFullUpload = region.minX == 0 && region.minY == 0 &&
		region.maxX == axisCount && region.maxY == axisCount;

	if (isFullUpload)
	{
		desc.data = terrain.HeightData.data();
	}
	else
	{
		uploadData.resize(static_cast<std::size_t>(desc.width) * desc.height);

		for (uint32 row = 0; row < desc.height; ++row)
		{
			const std::size_t sourceIndex = static_cast<std::size_t>(region.minY + row) * axisCount + region.minX;
			const std::size_t targetIndex = static_cast<std::size_t>(row) * desc.width;
			std::copy_n(terrain.HeightData.data() + sourceIndex, desc.width, uploadData.data() + targetIndex);
		}

		desc.data = uploadData.data();
	}

	if (IsSuccess(rhi->UploadTextureData(terrain.HeightmapTexture.get(), desc, region.minX, region.minY)))
	{
		terrain.IsDirty = false;
		terrain.HeightDirtyRegion.reset();
	}
}

void TerrainSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)registry;
	(void)dt;

	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;
		auto view = scene->GetRegistry().view<TerrainComponent>();
		for (auto [entity, terrain] : view.each())
		{
			EnsureTerrainResources(terrain);
		}
	}
}

void TerrainSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)registry;
	(void)dt;

	auto& renderer = Renderer::Get();
	vector<RenderPass*> targetPasses;

	for (const auto& viewport : renderer.GetActiveViewportCameras())
	{
		RenderPass* pass = viewport.renderPass;
		if (!pass) continue;

		const ERenderPassType passType = pass->GetPassType();
		const bool isShadowPass = passType == ERenderPassType::Shadow;
		if (!isShadowPass && !viewport.hasCamera) continue;

		if (passType != ERenderPassType::Geometry && passType != ERenderPassType::Forward && !isShadowPass)
			continue;

		if (std::find(targetPasses.begin(), targetPasses.end(), pass) == targetPasses.end())
			targetPasses.push_back(pass);
	}

	if (targetPasses.empty()) return;

	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;

		auto view = scene->GetRegistry().view<TerrainComponent, WorldTransformComponent, FlagComponent>();
		for (auto [entity, terrain, worldTransform, flag] : view.each())
		{
			if (!EnsureTerrainResources(terrain))
				continue;

			UpdateTerrainHeightmap(terrain);
			UpdateTerrainSplatmap(terrain);

			if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;

			Mesh* mesh = terrain.GridMesh.get();
			MaterialInterface* material = terrain.TerrainMaterial.Get();

			if (!mesh || !material || !terrain.HeightmapTexture || !terrain.SplatmapTexture ||
				terrain.IsDirty || terrain.IsSplatDirty)
				continue;

			if (terrain.Resolution == 0 ||
				!std::isfinite(terrain.Width) || terrain.Width <= 0.f ||
				!std::isfinite(terrain.Length) || terrain.Length <= 0.f ||
				!std::isfinite(terrain.MaxHeight) || terrain.MaxHeight < 0.f)
				continue;

			const uint64 axisCount = static_cast<uint64>(terrain.Resolution) + 1;
			if (terrain.HeightmapTexture->GetWidth() != axisCount ||
				terrain.HeightmapTexture->GetHeight() != axisCount ||
				static_cast<uint64>(mesh->GetVertexCount()) != axisCount * axisCount)
				continue;

			if (mesh->GetMin().x != -terrain.Width * 0.5f || mesh->GetMax().x != terrain.Width * 0.5f ||
				mesh->GetMin().z != -terrain.Length * 0.5f || mesh->GetMax().z != terrain.Length * 0.5f)
				continue;

			const vec2 cellSpacing(terrain.Width / static_cast<f32>(terrain.Resolution),
				terrain.Length / static_cast<f32>(terrain.Resolution));

			if (!std::isfinite(cellSpacing.x) || !std::isfinite(cellSpacing.y) ||
				cellSpacing.x <= 0.f || cellSpacing.y <= 0.f)
				continue;

			StaticDrawCommand command;
			command.mesh = mesh;
			command.material = material;
			command.worldMatrix = worldTransform.worldMatrix;
			command.pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);
			command.heightfield.emplace();
			command.heightfield->gridMesh = terrain.GridMesh;
			command.heightfield->heightmap = terrain.HeightmapTexture;
			command.heightfield->splatmap = terrain.SplatmapTexture;
			command.heightfield->maxHeight = terrain.MaxHeight;
			command.heightfield->cellSpacing = cellSpacing;

			for (RenderPass* pass : targetPasses)
			{
				if (!pass->IsAcceptsBlendMode(material->GetBlendMode())) continue;
				renderer.SubmitStatic(command, pass->GetID());
			}
		}
	}
}

void TerrainSystem::UpdateTerrainSplatmap(TerrainComponent& terrain)
{
	if (!terrain.IsSplatDirty || !terrain.SplatmapTexture) return;
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return;
	const uint32 vertexAxisCount = terrain.Resolution + 1;
	RHITextureDesc desc{};
	desc.width = vertexAxisCount;
	desc.height = vertexAxisCount;
	desc.format = ETextureFormat::R8G8B8A8_UNORM;
	desc.dimension = ETextureDimension::Texture2D;
	desc.usage = ETextureUsage::Sampler;
	desc.data = terrain.SplatData.data();
	desc.dataSize = static_cast<uint32>(terrain.SplatData.size());
	if (IsSuccess(rhi->UploadTextureData(terrain.SplatmapTexture.get(), desc)))
		terrain.IsSplatDirty = false;
}