#pragma once

#include "Collider.h"


#pragma region Constructor&Destructor
EResult Collider::Initialize(void* arg)
{
	return CollisionManager::Get().AddCollider(this);
}
void Collider::Free()
{
	CollisionManager::Get().RemoveCollider(this);
	__super::Free();
}
#pragma endregion