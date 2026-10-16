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
	scene->AddComponent<NameComponent>(entity, name);


	scene->AddComponent<FlagComponent>(entity, EEntityFlag::Default);

	auto& transform = scene->AddComponent<TransformComponent>(entity);
	transform.position = vec3(0.0f, 0.0f, 0.0f);
	transform.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
	transform.scale = vec3(1.0f, 1.0f, 1.0f);

	auto& worldTransform = scene->AddComponent<WorldTransformComponent>(entity, glm::identity<mat4>());

	scene->AddComponent<HierarchyComponent>(entity);

	return entity;
}

Entity& EntityFactory::CreatePrimitiveEntity(Scene* scene, const wstring& name, const wstring& meshKey, const wstring& materialKey)
{
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "UI_HISTORY_CREATE");
	Entity& entity = scene->CreateEntity();
	scene->AddComponent<NameComponent>(entity, name);


	scene->AddComponent<FlagComponent>(entity, EEntityFlag::Default);

	auto& transform = scene->AddComponent<TransformComponent>(entity);
	transform.position = vec3(0.0f, 0.0f, 0.0f);
	transform.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
	transform.scale = vec3(1.0f, 1.0f, 1.0f);

	auto& worldTransform = scene->AddComponent<WorldTransformComponent>(entity, glm::identity<mat4>());

	scene->AddComponent<HierarchyComponent>(entity);

	ResourceManager& resourceManager = ResourceManager::Get();
	ResourceHandle<Mesh> meshHandle = resourceManager.GetResourceHandle<Mesh>(meshKey);
	ResourceHandle<Material> materialHandle = resourceManager.GetResourceHandle<Material>(materialKey);
	if(!meshHandle || !materialHandle)
	{
		ENGINE_LOG_INFO("Failed to create primitive entity: Mesh or Material not found. MeshKey: {}, MaterialKey: {}", WStrToStr(meshKey), WStrToStr(materialKey));
		return entity;
	}
	scene->AddComponent<StaticMeshRendererComponent>(entity, meshHandle, materialHandle);

	return entity;
}

Entity& EntityFactory::CreateSpriteEntity(Scene* scene, const wstring& name, const wstring& materialKey)
{
	SceneEditScope edit(Application::Get().GetEditHistory(), *scene, "UI_HISTORY_CREATE");
	Entity& entity = scene->CreateEntity();
	scene->AddComponent<NameComponent>(entity, name);


	scene->AddComponent<FlagComponent>(entity, EEntityFlag::Default);

	auto& transform = scene->AddComponent<TransformComponent>(entity);
	transform.position = vec3(0.0f, 0.0f, 0.0f);
	transform.rotation = quat(1.0f, 0.0f, 0.0f, 0.0f);
	transform.scale = vec3(1.0f, 1.0f, 1.0f);

	auto& worldTransform = scene->AddComponent<WorldTransformComponent>(entity, glm::identity<mat4>());

	scene->AddComponent<HierarchyComponent>(entity);

	ResourceManager& resourceManager = ResourceManager::Get();

	ResourceHandle<Sprite> spriteHandle = resourceManager.GetResourceHandle<Sprite>(RESOURCE_PATH L"Texture/uv1.png");
	ResourceHandle<Mesh> meshHandle = resourceManager.GetResourceHandle<Mesh>(L"QuadMesh");
	ResourceHandle<Material> materialHandle = resourceManager.GetResourceHandle<Material>(materialKey);

	scene->AddComponent<SpriteRendererComponent>(entity, spriteHandle, meshHandle, materialHandle);
	return entity;
}
