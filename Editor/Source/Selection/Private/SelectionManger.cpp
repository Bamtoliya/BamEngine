#pragma once

#include "SelectionManager.h"
#include "CollisionManager.h"

IMPLEMENT_SINGLETON(SelectionManager)

void SelectionManager::Free()
{
	m_SelectedObjects.clear();
}

GameObject* SelectionManager::PickObjectByRay(const Ray& ray)
{
	HitResult hitResult;
	if (CollisionManager::Get().Raycast(ray, hitResult))
	{
		GameObject* hitObject = static_cast<GameObject*>(hitResult.UserData);
		if (hitObject)
		{
			return hitObject;
		}
	}
	return nullptr;
}
