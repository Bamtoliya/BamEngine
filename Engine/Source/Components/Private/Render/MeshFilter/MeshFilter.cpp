#pragma once

#include "MeshFilter.h"

#pragma region Constructor&Destructor
EResult MeshFilter::Initialize(void* arg)
{
	return EResult::Success;
}
MeshFilter* MeshFilter::Create(void* arg)
{

	return nullptr;
}
Component* MeshFilter::Clone(GameObject* owner, void* arg)
{

	return nullptr;
}
void MeshFilter::Free()
{
	Component::Free();
}
#pragma endregion
