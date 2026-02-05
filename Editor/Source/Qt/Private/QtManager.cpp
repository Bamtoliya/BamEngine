#pragma once

#include "QtManager.h"

IMPLEMENT_SINGLETON(QtManager)

#pragma region Constructor&Destructor
EResult QtManager::Initialize(void* arg)
{
	if (arg)
	{

	}
	return EResult::Success;
}
void QtManager::Free()
{
}
#pragma endregion

