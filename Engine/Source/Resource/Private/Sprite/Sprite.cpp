#include "Sprite.h"
#include "Sprite.h"
#include "Sprite.h"
#pragma once

#include "Sprite.h"
#include "ResourceManager.h"

#pragma region Constructor&Destructor
EResult Sprite::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg)))
		return EResult::Fail;

	if (arg)
	{
		CAST_DESC
		if (desc->Texture)
		{
			m_Texture = desc->Texture;
			Safe_AddRef(m_Texture);
		}
		else if (!desc->TexturePath.empty())
		{
			if (IsFailure(ResourceManager::Get().LoadTexture(m_Tag, desc->TexturePath)))
			{
				return EResult::Fail;
			}
			Texture* texture = ResourceManager::Get().GetTexture(m_Tag);
			if (texture)
			{
				m_Texture = texture;
				Safe_AddRef(m_Texture);
			}
		}
		m_Region = desc->Region;
		m_Pivot = desc->Pivot;
	}
	return EResult::Success;
}

Sprite* Sprite::Create(void* arg)
{
	Sprite* instance = new Sprite();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void Sprite::Free()
{
	Safe_Release(m_Texture);
}
#pragma endregion


EResult Sprite::SetRegion(const Rect& region)
{
	m_Region = region;
	return EResult::Success;
}
EResult Sprite::SetPivot(const vec2& pivot)
{
	m_Pivot = pivot;
	return EResult::Success;
}
EResult Sprite::SetTexture(Texture* texture)
{
	if (m_Texture)
		Safe_Release(m_Texture);
		if (texture)
		{
			m_Texture = texture;
			Safe_AddRef(m_Texture);
		}
	return EResult::Success;
}