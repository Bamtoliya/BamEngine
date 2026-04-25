#pragma once

#include "Skeleton.h"


#pragma region Constructor&Destructor
EResult Skeleton::Initialize(void* arg)
{
	TODO("Skeleton Initialize Implement");
	if(IsFailure(Resource::Initialize(arg)))
		return EResult::Fail;
	CAST_DESC
	m_Bones = desc->Bones;

	for (uint32 i = 0; i < m_Bones.size(); ++i)
	{
		m_BoneMap[m_Bones[i].Name] = i;
	}

	return EResult::Success;
}

Skeleton* Skeleton::Create(void* arg)
{
	Skeleton* instance = new Skeleton();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

void Skeleton::Free()
{
	TODO("Skeleton Free Implement");
	m_Bones.clear();
	m_BoneMap.clear();
}

#pragma endregion