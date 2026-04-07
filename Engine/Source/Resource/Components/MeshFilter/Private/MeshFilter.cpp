#pragma once

#include "MeshFilter.h"
#include "ComponentRegistry.h"

REGISTER_COMPONENT(MeshFilter)

#pragma region Constructor&Destructor
EResult MeshFilter::Initialize(void* arg)
{
	return EResult::Success;
}
MeshFilter* MeshFilter::Create(void* arg)
{
	MeshFilter* instance = new MeshFilter();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
Component* MeshFilter::Clone(GameObject* owner, void* arg)
{
	MeshFilter* instance = new MeshFilter();
	instance->SetOwner(owner);
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
void MeshFilter::Free()
{
	Component::Free();
}
void MeshFilter::SetMeshHandle(ResourceHandle<Mesh> handle)
{
	m_MeshHandle = handle;
}
#pragma endregion
