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

#pragma region Getter
uint32 Skeleton::GetBoneIndex(const wstring& boneName) const
{
	auto iter = m_BoneMap.find(boneName);
	if (iter != m_BoneMap.end())
	{
		return iter->second;
	}
	return UINT32_MAX; // Not found
}
wstring Skeleton::GetBoneName(uint32 boneIndex) const
{
	if (boneIndex < m_Bones.size())
	{
		return m_Bones[boneIndex].Name;
	}
	return L"";
}
#pragma endregion
