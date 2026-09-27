#pragma once
#include "EntityFactory.h"
#include "Entity.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "PhysicsComponent.h"
#include "RenderComponents.h"

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
