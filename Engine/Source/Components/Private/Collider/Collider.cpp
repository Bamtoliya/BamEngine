#pragma once

#include "Collider.h"


#pragma region Constructor&Destructor
EResult Collider::Initialize(void* arg)
{
	return EResult::Success;
}

Collider* Collider::Create(void* arg)
{
	Collider* instance = new Collider();
	EResult result = instance->Initialize(arg);
	if (result != EResult::Success)
	{
		delete instance;
		return nullptr;
	}
	return nullptr;
}

void Collider::Free()
{
}
#pragma endregion