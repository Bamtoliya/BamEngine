#pragma once
#include "Sprite.h"
#include "ResourceManager.h"
#include "SerializationHelper.h"

#pragma region Constructor&Destructor
EResult Sprite::Initialize(void* arg)
{
	if (IsFailure(__super::Initialize(arg))) return EResult::Fail;

	CAST_DESC
	if (desc->texture)
	{
		m_Texture = desc->texture;
		//m_Key = desc->texture->GetKey() + L"_Sprite";
		//m_Path = desc->texture->GetPath() + L"_Sprite";
	}
	else if (!desc->texturePath.empty())
	{
		TextureCreateDesc texDesc;
		texDesc.Path = desc->texturePath;
		m_Texture = ResourceManager::Get().LoadResource<Texture>(&texDesc);
	}
	
	m_Region = desc->region;
	m_Pivot = desc->pivot;
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
vec4 Sprite::GetUVTransform()
{
	Texture* texture = m_Texture.Get();
	RHITexture* rhiTexture = texture ? texture->GetRHITexture() : nullptr;

	if (!rhiTexture)
		return vec4(0.f);

	const f32 width = static_cast<f32>(rhiTexture->GetWidth());
	const f32 height = static_cast<f32>(rhiTexture->GetHeight());

	if (width <= 0.f || height <= 0.f)
		return vec4(0.f);

	// 영역을 지정하지 않은 Sprite는 텍스처 전체를 사용합니다.
	if (m_Region.width == 0.f && m_Region.height == 0.f)
		return vec4(0.f, 0.f, 1.f, 1.f);

	return vec4(
		m_Region.left / width,
		m_Region.top / height,
		m_Region.width / width,
		m_Region.height / height);
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