#pragma once

#include "Scene.h"
#include "Layer.h"

#pragma region Constructor&Destructor
EResult Scene::Initialize(void* arg)
{
	return EResult::Success;
}

Scene* Scene::Create(void* arg)
{
	Scene* instance = new Scene();
	
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void Scene::Free()
{
	for (auto& layer : m_Layers)
	{
		Safe_Release(layer);
	}
	m_Layers.clear();
}
#pragma endregion


#pragma region Loop
void Scene::FixedUpdate(f32 dt)
{
	for (auto& layer : m_Layers)
	{
		layer->FixedUpdate(dt);
	}
}

void Scene::Update(f32 dt)
{
	for (auto& layer : m_Layers)
	{
		layer->Update(dt);
	}
}

void Scene::LateUpdate(f32 dt)
{
	for (auto& layer : m_Layers)
	{
		layer->LateUpdate(dt);
	}
}
#pragma endregion
