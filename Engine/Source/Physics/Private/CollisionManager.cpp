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
}
#pragma endregion