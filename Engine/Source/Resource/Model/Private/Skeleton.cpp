#pragma once

#include "Skeleton.h"


#pragma region Constructor&Destructor
EResult Skeleton::Initialize(void* arg)
{
	return EResult::Success;
}

Skeleton* Skeleton::Create(void* arg)
{
	return nullptr;
}

void Skeleton::Free()
{
}

#pragma endregion