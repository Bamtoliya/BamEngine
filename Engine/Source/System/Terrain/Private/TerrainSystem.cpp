#include "TerrainSystem.h"
#include "TerrainComponent.h"
#include "CoreComponents.h"
#include "TerrainUtility.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "PickingManager.h"
#include "RHI.h"
#include "RHITexture.h"

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

void TerrainSystem::EnsureTerrainResources(Scene* scene, entt::entity entity, TerrainComponent& terrain)
{
	if (!scene) return;

	// 1. GridMesh 동적 생성 및 핸들 할당
	if (!terrain.GridMesh.IsValid())
	{
		Mesh* mesh = TerrainUtility::CreateGridMesh(terrain.Width, terrain.Length, terrain.Resolution);
		if (mesh)
		{
			const uint64 entityId = static_cast<uint64>(entt::to_integral(entity));
			std::wstring key = L"Runtime_TerrainGridMesh_" + std::to_wstring(entityId);
			terrain.GridMesh = ResourceManager::Get().AddResource<Mesh>(key, mesh);
		}
	}

	// 2. CPU 높이 데이터 배열 확보 ((Resolution + 1) * (Resolution + 1))
	const uint32 vertexAxisCount = terrain.Resolution + 1;
	const uint32 totalVertexCount = vertexAxisCount * vertexAxisCount;
	if (terrain.HeightData.size() != totalVertexCount)
	{
		terrain.HeightData.assign(totalVertexCount, 0.0f);
		terrain.IsDirty = true;
	}

	// 3. GPU 높이맵 텍스처 (R32_FLOAT) 생성
	RHI* rhi = Renderer::Get().GetRHI();
	if (rhi && !terrain.HeightmapTexture)
	{
		RHITextureDesc desc{};
		desc.width = vertexAxisCount;
		desc.height = vertexAxisCount;
		desc.format = ETextureFormat::R32_FLOAT;
		desc.dimension = ETextureDimension::Texture2D;
		desc.usage = ETextureUsage::Sampler;
		desc.data = terrain.HeightData.data();
		desc.dataSize = static_cast<uint32>(terrain.HeightData.size() * sizeof(f32));

		RHITexture* rawTexture = rhi->CreateTexture(desc);
		if (rawTexture)
		{
			terrain.HeightmapTexture = std::shared_ptr<RHITexture>(rawTexture, [](RHITexture* p) { Safe_Release(p); });
			terrain.IsDirty = false;
		}
	}
}

void TerrainSystem::OnUpdate(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
	(void)registry;
	(void)dt;

	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;
		auto view = scene->GetRegistry().view<TerrainComponent>();
		for (auto [entity, terrain] : view.each())
		{
			EnsureTerrainResources(scene, entity, terrain);

			if (terrain.IsDirty && terrain.HeightmapTexture)
			{
				// TODO: 에디터 브러시로 편집된 높이 데이터를 GPU로 전송하는 로직 연결 예정
				terrain.IsDirty = false;
			}
		}
	}
}

void TerrainSystem::OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
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
			if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;

			Mesh* mesh = terrain.GridMesh.Get();
			MaterialInterface* material = terrain.TerrainMaterial.Get();
			if (!mesh || !material) continue;

			StaticDrawCommand command;
			command.mesh = mesh;
			command.material = material;
			command.worldMatrix = worldTransform.worldMatrix;
			command.pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

			for (RenderPass* pass : targetPasses)
			{
				if (!pass->IsAcceptsBlendMode(material->GetBlendMode())) continue;
				renderer.SubmitStatic(command, pass->GetID());
			}
		}
	}
}