#pragma once

#include "CollisionManager.h"

IMPLEMENT_SINGLETON(CollisionManager)

#pragma region Constructor&Destructor
EResult CollisionManager::Initialize(void* arg)
{
	return EResult::Success;
}

void CollisionManager::Free()
{
	RELEASE_VECTOR(m_Colliders);
}
EResult CollisionManager::AddCollider(Collider* collider)
{
	if (!collider) return EResult::InvalidArgument;
	m_Colliders.push_back(collider);
	Safe_AddRef(collider);
	return EResult::Success;
}
EResult CollisionManager::RemoveCollider(Collider* collider)
{
	auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
	if (it != m_Colliders.end())
	{
		Safe_Release(*it);
		m_Colliders.erase(it);
		return EResult::Success;
	}
	return EResult::InvalidArgument;
}
bool CollisionManager::Raycast(const Ray& ray, HitResult& outResult)
{
	HitResult tempResult;
	bool hasHit = false;
	f32 minDistance = FLT_MAX;

	for (Collider* collider : m_Colliders)
	{
		if (collider->Raycast(ray, tempResult))
		{
			hasHit = true;
			if (tempResult.Distance < minDistance)
			{
				minDistance = tempResult.Distance;
				outResult = tempResult;
				outResult.UserData = collider->GetOwner();
			}
		}
	}

	return hasHit;
}
#pragma endregion