#pragma once
#include "EntityFactory.h"
#include "Application.h"
#include "Entity.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "PhysicsComponent.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "ResourceManager.h"
#include "UILayout.h"
#include <stdexcept>
Entity& EntityFactory::CreateCanvasEntity(Scene* scene, const wstring& name)
{
	if (!scene) throw std::invalid_argument("Canvas creation requires a scene.");
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "UI_HISTORY_CREATE");
	Entity& entity = scene->CreateEntity();
	auto& world = scene->GetRegistry();
	const auto handle = entity.GetEntityHandle();
	world.emplace<NameComponent>(handle, name);
	world.emplace<FlagComponent>(handle);
	world.emplace<HierarchyComponent>(handle);
	world.emplace<CanvasComponent>(handle);
	world.emplace<UIRenderOrderComponent>(handle);
	return entity;
}

Entity& EntityFactory::CreateUIImageEntity(Scene* scene, Entity& parent, const wstring& name)
{
	if (!scene || parent.GetScene() != scene ||
		UILayout::FindCanvas(scene->GetRegistry(), parent.GetEntityHandle()) == entt::null)
		throw std::invalid_argument("UI Image requires a parent in a canvas.");
	auto& world = scene->GetRegistry();
	if (!world.all_of<HierarchyComponent>(parent.GetEntityHandle()))
		throw std::invalid_argument("UI parent requires a hierarchy component.");
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "UI_HISTORY_CREATE");
	Entity& entity = scene->CreateEntity();
	const auto handle = entity.GetEntityHandle();
	world.emplace<NameComponent>(handle, name);
	world.emplace<FlagComponent>(handle);
	world.emplace<HierarchyComponent>(handle).parent = parent.GetEntityHandle();
	auto& rect = world.emplace<RectTransformComponent>(handle);
	rect.sizeDelta = vec2(200.0f, 120.0f);
	world.emplace<UIRenderOrderComponent>(handle);
	auto& image = world.emplace<UIImageComponent>(handle);
	image.materialHandle = ResourceManager::Get().GetResourceHandle<Material>(L"Resources/Material/UIMaterial");
	world.get<HierarchyComponent>(parent.GetEntityHandle()).children.push_back(handle);
	return entity;
}

Entity& EntityFactory::CreateUIButtonEntity(Scene* scene, Entity& parent, const wstring& name)
{
	if (!scene) throw std::invalid_argument("UI Button creation requires a scene.");
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "UI_HISTORY_CREATE");
	Entity& entity = CreateUIImageEntity(scene, parent, name);
	scene->GetRegistry().emplace<UIButtonComponent>(entity.GetEntityHandle());
	return entity;
}

Entity& EntityFactory::CreateEmptyEntity(Scene* scene, const wstring& name)
{
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "UI_HISTORY_CREATE");
	Entity& entity = scene->CreateEntity();
	scene->AddComponent<HierarchyComponent>(entity);
	scene->AddComponent<NameComponent>(entity, name);
	scene->AddComponent<FlagComponent>(entity, EEntityFlag::Default);
	auto& transform = scene->AddComponent<TransformComponent>(entity);
	transform.position = vec3(0.0f, 0.0f, 0.0f);
	transform.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
	transform.scale = vec3(1.0f, 1.0f, 1.0f);
	scene->AddComponent<WorldTransformComponent>(entity);
	return entity;
}

Entity& EntityFactory::CreatePrimitiveEntity(Scene* scene, const wstring& name, const wstring& meshKey, const wstring& materialKey)
{
	Entity& entity = CreateEmptyEntity(scene, name);
	ResourceManager& resourceManager = ResourceManager::Get();
	ResourceHandle<Mesh> meshHandle = resourceManager.GetResourceHandle<Mesh>(meshKey);
	ResourceHandle<Material> materialHandle = resourceManager.GetResourceHandle<Material>(materialKey);
	if(!meshHandle || !materialHandle)
	{
		EDITOR_LOG_INFO("Failed to create primitive entity: Mesh or Material not found. MeshKey: {}, MaterialKey: {}", WStrToStr(meshKey), WStrToStr(materialKey));
		return entity;
	}
	scene->AddComponent<StaticMeshRendererComponent>(entity, meshHandle, materialHandle);

	return entity;
}

Entity& EntityFactory::CreateSpriteEntity(Scene* scene, const wstring& name, const wstring& materialKey)
{
	Entity& entity = CreateEmptyEntity(scene, name);

	ResourceManager& resourceManager = ResourceManager::Get();

	ResourceHandle<Sprite> spriteHandle = resourceManager.GetResourceHandle<Sprite>(RESOURCE_PATH L"Texture/uv1.bamsprite.json");
	ResourceHandle<Mesh> meshHandle = resourceManager.GetResourceHandle<Mesh>(L"QuadMesh");
	ResourceHandle<Material> materialHandle = resourceManager.GetResourceHandle<Material>(materialKey);

	if (!spriteHandle || !meshHandle || !materialHandle)
	{
		EDITOR_LOG_ERROR("Failed to create sprite entity: Missing resources. Sprite: {}, Mesh: {}, Material: {}",
			spriteHandle ? "OK" : "Missing", meshHandle ? "OK" : "Missing", materialHandle ? "OK" : "Missing");
	}

	scene->AddComponent<SpriteRendererComponent>(entity, spriteHandle, meshHandle, materialHandle);
	return entity;
}

Entity& EntityFactory::CreateLightEntity(Scene* scene, const wstring& name, const wstring& lightType)
{
	Entity& entity = CreateEmptyEntity(scene, name);
	scene->AddComponent<LightComponent>(entity);
	if (lightType == L"Directional")
	{
		auto& light = scene->GetComponent<LightComponent>(entity);
		light.LightType = ELightType::Directional;
		light.Color = vec3(1.0f, 0.95f, 0.85f); // 따뜻한 햇빛
		light.Intensity = 1.2f;
		light.Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse | ELightFlags::AffectSpecular | ELightFlags::CastShadows;
		light.ShadowRange = 50.0f;
		light.ShadowBias = 0.002f;
	}
	else if (lightType == L"Point")
	{
		scene->GetComponent<LightComponent>(entity).LightType = ELightType::Point;
	}
	else if (lightType == L"Spot")
	{
		scene->GetComponent<LightComponent>(entity).LightType = ELightType::Spot;
	}
	else if (lightType == L"Sky")
	{
		auto& light = scene->GetComponent<LightComponent>(entity);
		light.LightType = ELightType::Sky;
		light.Color = vec3(0.35f, 0.55f, 0.85f);       // 하늘 천정(Zenith) 색상
		light.GroundColor = vec3(0.1f, 0.08f, 0.05f); // 지표면 반사 앰비언트
		light.IndirectIntensity = 0.8f;
		light.Intensity = 1.0f;
		light.Flags = ELightFlags::UseInDeferredRendering | ELightFlags::AffectDiffuse;
	}
	else
	{
		EDITOR_LOG_WARN("Unknown light type: {}. Defaulting to Point light.", WStrToStr(lightType));
		scene->GetComponent<LightComponent>(entity).LightType = ELightType::Point;
	}
	return entity;
}

Entity& EntityFactory::CreateTerrainEntity(Scene* scene, const wstring& name)
{
	Entity& entity = CreateEmptyEntity(scene, name);
	auto& terrain = scene->AddComponent<TerrainComponent>(entity);
	terrain.TerrainMaterial = ResourceManager::Get().GetResourceHandle<Material>(RESOURCE_PATH L"Material/TerrainMaterial");
	return entity;
}