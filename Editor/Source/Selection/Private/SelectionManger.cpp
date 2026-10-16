#pragma once

#include "SelectionManager.h"
#include "CollisionManager.h"

#include "SceneManager.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "TransformSystem.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Sprite.h"
#include "PickingMath.h"

#include <algorithm>
#include <cmath>

IMPLEMENT_SINGLETON(SelectionManager)

#pragma region Constructor&Destructor
EResult SelectionManager::Initialize(void* arg)
{
	return EResult::Success;
}

void SelectionManager::Free()
{
	m_SelectedObjects.clear();
	m_SelectedEntities.clear();
}
#pragma endregion

#pragma region Object Selection
vector<class GameObject*>& SelectionManager::GetSelectionContext()
{
	return m_SelectedObjects;
}

GameObject* SelectionManager::GetPrimarySelection()
{
	return m_SelectedObjects.empty() ? nullptr : m_SelectedObjects.back();
}

void SelectionManager::ToggleSelection(GameObject* gameObject)
{
	if (m_LockPrimarySeletion) return;
	auto it = std::find(m_SelectedObjects.begin(), m_SelectedObjects.end(), gameObject);
	if (it != m_SelectedObjects.end())
	{
		m_SelectedObjects.erase(it);
	}
	else
	{
		m_SelectedObjects.push_back(gameObject);
	}
}

void SelectionManager::ClearSelection(bool force)
{
	if (m_LockPrimarySeletion && !force) return;
	m_SelectedEntities.clear();
	m_SelectedObjects.clear();
}

void SelectionManager::SetSelectedObject(GameObject* gameObject)
{
	if (m_LockPrimarySeletion) return;
	ClearSelection();
	if (gameObject)
	{
		m_SelectedObjects.push_back(gameObject);
	}
}

bool SelectionManager::IsSelected(GameObject* gameObject) const
{
	return find(m_SelectedObjects.begin(), m_SelectedObjects.end(), gameObject) != m_SelectedObjects.end();
}

void SelectionManager::AddToSelection(GameObject* gameObject)
{
	if (!gameObject) return;
	if (!IsSelected(gameObject))
	{
		m_SelectedObjects.push_back(gameObject);
	}
}

GameObject* SelectionManager::PickObjectByRay(const Ray& ray)
{
	HitResult hitResult;
	if (CollisionManager::Get().Raycast(ray, hitResult))
	{
		GameObject* hitObject = static_cast<GameObject*>(hitResult.userData);
		if (hitObject)
		{
			return hitObject;
		}
	}
	return nullptr;
}
#pragma endregion

#pragma region Entity Selection
vector<class Entity*>& SelectionManager::GetSelectedEntities()
{
	return m_SelectedEntities;
}

Entity* SelectionManager::GetPrimarySelectedEntity()
{
	return m_SelectedEntities.empty() ? nullptr : m_SelectedEntities.back();;
}

void SelectionManager::ToggleEntitySelection(Entity* entity)
{
	if (m_LockPrimarySeletion || !entity) return;
	m_SelectedObjects.clear();
	auto it = std::find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity);
	if (it != m_SelectedEntities.end())
	{
		m_SelectedEntities.erase(it);
	}
	else
	{
		m_SelectedEntities.push_back(entity);
	}
}

void SelectionManager::SetSelectedEntity(Entity* entity)
{
	if (m_LockPrimarySeletion) return;
	ClearSelection();
	if (entity)
	{
		m_SelectedEntities.push_back(entity);
	}
}

bool SelectionManager::IsEntitySelected(Entity* entity) const
{
	return find(m_SelectedEntities.begin(), m_SelectedEntities.end(), entity) != m_SelectedEntities.end();
}

void SelectionManager::AddToEntitySelection(Entity* entity)
{
	if (!entity) return;
	if (!IsEntitySelected(entity))
	{
		m_SelectedEntities.push_back(entity);
	}
}

Entity* SelectionManager::PickEntityByRay(const struct Ray& ray, float maxDistance)
{
	if (!std::isfinite(maxDistance) || maxDistance < 0.0f) return nullptr;
	Entity* closest = nullptr;
	float closestDistance = maxDistance;
	for (Scene* scene : SceneManager::Get().GetActiveScenes())
	{
		if (!scene) continue;
		auto& world = scene->GetRegistry();
		TransformSystem::UpdateHierarchy(world);
		for (auto [entity, transform, flag] : world.view<WorldTransformComponent, FlagComponent>().each())
		{
			if (!HasFlag(flag.flags, EEntityFlag::Active) || !HasFlag(flag.flags, EEntityFlag::Visible) ||
				HasFlag(flag.flags, EEntityFlag::Dead)) continue;
			Entity* candidate = scene->FindEntity(entity);
			if (!candidate) continue;
			const auto testMesh = [&](Mesh* mesh, const mat4& matrix)
			{
				if (!mesh) return;
				float distance = 0.0f;
				if (PickingMath::RaycastBounds(ray, AABB(mesh->GetMin(), mesh->GetMax()), matrix,
					closestDistance, distance) && (!closest || distance < closestDistance))
				{
					closest = candidate;
					closestDistance = distance;
				}
			};
			if (const auto* renderer = world.try_get<StaticMeshRendererComponent>(entity))
			{
				if (renderer->materialHandle.Get()) testMesh(renderer->meshHandle.Get(), transform.worldMatrix);
			}
			if (const auto* renderer = world.try_get<SpriteRendererComponent>(entity))
			{
				mat4 quadMatrix;
				if (renderer->drawMode == ESpriteDrawMode::Simple && renderer->spriteHandle.Get() &&
					renderer->materialHandle.Get() && renderer->spriteHandle.Get()->TryGetQuadMatrix(quadMatrix))
					testMesh(renderer->meshHandle.Get(), transform.worldMatrix * quadMatrix);
			}
		}
	}
	return closest;
}

#pragma endregion


#pragma region Asset Selection
void SelectionManager::SetSelectedAsset(const filesystem::path& assetPath)
{
	m_LastSelectedAssetPath.clear();
	m_LastSelectedAssetPath = assetPath;
}

filesystem::path SelectionManager::GetSelectedAssetPath() const
{
	return m_LastSelectedAssetPath;
}
void SelectionManager::SetSelectedResource(const filesystem::path& assetPath)
{
	m_LastSelectedResourcePath.clear();
	m_LastSelectedResourcePath = assetPath;
}
filesystem::path SelectionManager::GetSelectedAssetResource() const
{
	return m_LastSelectedResourcePath;
}
#pragma endregion
