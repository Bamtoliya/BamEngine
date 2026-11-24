#include "TerrainSystem.h"
#include "ComponentEnable.h"
#include "TerrainComponent.h"
#include "WorldComponents.h"
#include "TerrainUtility.h"
#include "TerrainDataFile.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "PickingManager.h"
#include "Texture.h"
#include "MotionHistory.h"

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

namespace
{
	// 레이어 에셋이 없거나 알베도가 없을 때의 색과 반복 길이. 0~2는 이전 고정 셰이더의 잔디·암석·흙 색, 반복 5m(이전 0.2배율)와 같다.
	constexpr vec4 DefaultLayerTints[HeightfieldDrawData::MaxLayers] = {
		{ 0.35f, 0.65f, 0.25f, 1.0f }, { 0.48f, 0.45f, 0.42f, 1.0f }, { 0.55f, 0.42f, 0.28f, 1.0f }, { 0.70f, 0.68f, 0.60f, 1.0f },
		{ 0.30f, 0.45f, 0.70f, 1.0f }, { 0.80f, 0.75f, 0.45f, 1.0f }, { 0.60f, 0.35f, 0.55f, 1.0f }, { 0.85f, 0.85f, 0.90f, 1.0f } };
	constexpr f32 DefaultLayerTiling = 5.0f;

	RHITexture* GetFallbackLayerTexture()
	{
		static ResourceHandle<Texture> white;
		static bool requested = false;
		if (!requested)
		{
			requested = true;
			white = ResourceManager::Get().GetResourceHandle<Texture>(L"Engine://Texture/white1x1.png");
			if (!white.Get()) white = ResourceHandle<Texture>(ResourceManager::Get().LoadFile(L"Engine://Texture/white1x1.png"));
		}
		return white.Get() ? white->GetRHITexture() : nullptr;
	}

	void FillTerrainLayerDrawData(const TerrainComponent& terrain, HeightfieldDrawData& draw)
	{
		// 흰 텍스처가 없으면 스플랫 텍스처라도 묶어 이전 드로우의 슬롯을 읽지 않게 한다.
		RHITexture* fallback = GetFallbackLayerTexture();
		if (!fallback) fallback = terrain.SplatmapTexture.get();
		for (uint32 layer = 0; layer < HeightfieldDrawData::MaxLayers; ++layer)
		{
			const auto* asset = layer < terrain.Layers.size() ? terrain.Layers[layer].Layer.Get() : nullptr;
			const Texture* albedo = asset ? asset->GetSettings().Albedo.Get() : nullptr;
			RHITexture* texture = albedo ? albedo->GetRHITexture() : nullptr;
			draw.layerTextures[layer] = texture ? texture : fallback;
			draw.layerTints[layer] = texture ? vec4(1.0f) : DefaultLayerTints[layer];
			const f32 tiling = asset ? asset->GetSettings().Tiling : DefaultLayerTiling;
			draw.layerTiling[layer] = std::isfinite(tiling) && tiling > 0.01f ? tiling : DefaultLayerTiling;
		}
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
	// 편집 모드에서는 OnUpdate(Gameplay 단계)가 돌지 않고 OnSubmit만 이 함수를 부르므로 데이터 동기화도 여기서 한다.
	TerrainDataFile::LoadIfChanged(terrain);
	TerrainUtility::EnsureLayerData(terrain);

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
	// 레이어 4~7 평면은 EnsureLayerData가 크기를 맞춘다. 크기가 맞을 때만 텍스처를 만든다.
	const bool extraLayoutMatches = terrain.SplatDataExtra.size() == sampleCount * 4;
	const bool extraTextureMatches = textureMatches(terrain.SplatmapExtraTexture, ETextureFormat::R8G8B8A8_UNORM);

	if (heightLayoutMatches && splatLayoutMatches && meshMatches &&
		heightTextureMatches && splatTextureMatches && (!extraLayoutMatches || extraTextureMatches))
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

	if (extraLayoutMatches && !extraTextureMatches)
	{
		candidate.SplatmapExtraTexture = createTexture(ETextureFormat::R8G8B8A8_UNORM,
			candidate.SplatDataExtra.data(), static_cast<uint32>(sampleCount * 4));
		if (!candidate.SplatmapExtraTexture)
			return false;
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
			if (!IsComponentEnabled<TerrainComponent>(scene->GetRegistry(), entity)) continue;
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
			if (!IsComponentEnabled<TerrainComponent>(scene->GetRegistry(), entity)) continue;
			if (!EnsureTerrainResources(terrain))
				continue;

			UpdateTerrainHeightmap(terrain);
			UpdateTerrainSplatmap(terrain);

			if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;

			Mesh* mesh = terrain.GridMesh.get();
			// 이전 씬의 구형 키(Resources/Material/TerrainMaterial)나 빈 핸들은 찾지 못하므로 엔진 기본 머티리얼로 그린다(컴포넌트 값은 바꾸지 않는다).
			MaterialInterface* material = terrain.TerrainMaterial.Get();
			if (!material)
			{
				static ResourceHandle<Material> defaultMaterial;
				static bool defaultMaterialRequested = false; // 실패해도 매 프레임 다시 읽고 로그를 남기지 않게 한 번만 시도한다
				if (!defaultMaterialRequested)
				{
					defaultMaterialRequested = true;
					defaultMaterial = ResourceHandle<Material>(ResourceManager::Get().LoadFile(TerrainComponent::DefaultMaterialUri));
				}
				material = defaultMaterial.Get();
			}

			// 레이어 4~7 텍스처가 없으면 셰이더가 다른 드로우의 t5를 읽으므로 그리지 않는다(EnsureLayerData 후 바로 만들어진다).
			if (!mesh || !material || !terrain.HeightmapTexture || !terrain.SplatmapTexture || !terrain.SplatmapExtraTexture ||
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
			// 월드 행렬만 움직이는 지형의 모션 벡터를 위한 번호. 씬마다 엔티티 번호가 겹치므로 씬 주소로 구분한다(MeshRenderSystem과 같다).
			command.objectId = { static_cast<uint32>(std::hash<const Scene*>{}(scene)) | 1u, static_cast<uint32>(entt::to_integral(entity)) };
			command.motionStamp = MakeHeightfieldMotionStamp(terrain.HeightmapTexture.get(), terrain.MaxHeight, cellSpacing.x, cellSpacing.y);
			command.pickingId = PickingManager::Get().RegisterPickingEntity<TerrainComponent>(scene, entity);
			command.heightfield.emplace();
			command.heightfield->gridMesh = terrain.GridMesh;
			command.heightfield->heightmap = terrain.HeightmapTexture;
			command.heightfield->splatmap = terrain.SplatmapTexture;
			command.heightfield->splatmapExtra = terrain.SplatmapExtraTexture;
			FillTerrainLayerDrawData(terrain, *command.heightfield);
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
	bool uploaded = IsSuccess(rhi->UploadTextureData(terrain.SplatmapTexture.get(), desc));
	// 레이어 4~7도 같은 표시로 함께 올린다(페인트는 두 평면을 한 번에 바꾼다).
	if (uploaded && terrain.SplatmapExtraTexture && terrain.SplatDataExtra.size() == terrain.SplatData.size())
	{
		desc.data = terrain.SplatDataExtra.data();
		desc.dataSize = static_cast<uint32>(terrain.SplatDataExtra.size());
		uploaded = IsSuccess(rhi->UploadTextureData(terrain.SplatmapExtraTexture.get(), desc));
	}
	if (uploaded)
		terrain.IsSplatDirty = false;
}