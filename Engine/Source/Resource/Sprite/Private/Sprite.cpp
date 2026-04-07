#pragma once
#include "Sprite.h"
#include "ResourceManager.h"

#pragma region Constructor&Destructor
EResult Sprite::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;

	CAST_DESC
	if (desc->Texture.IsValid())
	{
		m_Texture = desc->Texture;		
	}
	else if (!desc->TexturePath.empty())
	{
		m_Texture = ResourceManager::Get().LoadResource<Texture>(desc->TexturePath);
	}
	
	m_Key = m_Texture->GetKey() + L"_Sprite";
	m_Region = desc->Region;
	m_Pivot = desc->Pivot;
	return EResult::Success;
}

Sprite* Sprite::Create(void* arg)
{
	Sprite* instance = new Sprite();
	if (IsFailure(instance->Initialize(arg)))
	{
		delete instance;
		return nullptr;
	}
	return instance;
}

void Sprite::Free()
{

}
#pragma endregion


#pragma region Bind
EResult Sprite::Bind(uint32 slot)
{
	if (!m_Texture) return EResult::Fail;
	return m_Texture->Bind(slot);
}
#pragma endregion

#pragma region Setter
EResult Sprite::SetRegion(const Rect& region)
{
	m_Region = region;
	IncreaseVersion();
	return EResult::Success;
}
EResult Sprite::SetPivot(const vec2& pivot)
{
	m_Pivot = pivot;
	IncreaseVersion();
	return EResult::Success;
}
EResult Sprite::SetTexture(ResourceHandle<Texture> texture)
{
	if (!texture) return EResult::InvalidArgument;
	m_Texture = texture;
	IncreaseVersion();
	return EResult::Success;
}
#pragma endregion