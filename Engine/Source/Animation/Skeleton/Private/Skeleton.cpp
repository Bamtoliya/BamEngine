#pragma once

#include "Skeleton.h"


#pragma region Constructor&Destructor
EResult Skeleton::Initialize(void* arg)
{
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

EResult Skeleton::OnPostLoad()
{
	m_BoneMap.clear();
	for (uint32 i = 0; i < m_Bones.size(); ++i)
	{
		m_BoneMap[m_Bones[i].Name] = i;
	}
	return EResult::Success;
}
#pragma endregion

uint64 Engine::Skeleton::ComputeSkinBindingSignature(std::span<const Bone> bones)
{
	if (bones.empty())
		return 0;

	uint64 hash = Engine::CompileTimeHash("BamEngine.SkinBinding.v1");
	const auto addByte = [&](uint8 byte)
		{
			hash ^= byte;
			hash *= 1099511628211ull;
		};
	const auto addNumber = [&](uint64 value)
		{
			for (uint32 shift = 0; shift < 64; shift += 8)
				addByte(static_cast<uint8>(value >> shift));
		};
	const auto addMatrix = [&](const mat4& matrix)
		{
			for (int column = 0; column < 4; ++column)
			{
				for (int row = 0; row < 4; ++row)
				{
					const float value = matrix[column][row];
					if (!std::isfinite(value))
						return false;
					addNumber(std::bit_cast<uint32>(value == 0.0f ? 0.0f : value));
				}
			}
			return true;
		};

	addNumber(bones.size());
	for (const auto& bone : bones)
	{
		const auto name = Engine::WStrToStr(bone.Name);
		addNumber(name.size());
		for (unsigned char byte : name)
			addByte(byte);

		addNumber(static_cast<uint32>(bone.ParentIndex));
		if (!addMatrix(bone.LocalTransform) || !addMatrix(bone.OffsetMatrix))
			return 0;
	}

	return hash == 0 ? 1 : hash;
}