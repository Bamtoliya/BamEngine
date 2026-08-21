#pragma once
#include "EntityFactory.h"
#include "Entity.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "PhysicsComponent.h"

void EntityFactory::CreateEmptyEntity(Scene* scene)
{
	Entity& entity = scene->CreateEntity();
	scene->AddComponent<TransformComponent>(entity);
}
