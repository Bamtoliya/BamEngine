#pragma once
#include "EntityFactory.h"
#include "Entity.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "PhysicsComponent.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "ResourceManager.h"

Entity& EntityFactory::CreateEmptyEntity(Scene* scene, const wstring& name)
{
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
