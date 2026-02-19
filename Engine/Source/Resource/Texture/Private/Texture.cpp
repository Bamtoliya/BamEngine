#pragma once

#include "Texture.h"
#include "Renderer.h"

#pragma region Constructor&Destructor
EResult Texture::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	const wchar* path = reinterpret_cast<const wchar*>(arg);
	m_Path = path;
	RHI* rhi = Renderer::Get().GetRHI();
	// Create RHI Texture
	{
		TEXTUREDESC textureDesc = {};
		textureDesc.FilePath = m_Path;
		m_RHITexture = rhi->CreateTextureFromFile(textureDesc.FilePath.c_str());
		if (!m_RHITexture)
			return EResult::Fail;
	}
	return EResult::Success;
}

Texture* Texture::Create(void* arg)
{
	Texture* instance = new Texture();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void Texture::Free()
{
	Safe_Release(m_RHITexture);
}
#pragma endregion


#pragma region Load

EResult Texture::LoadFromFile(const wstring& path)
{
	// Release existing texture
	Safe_Release(m_RHITexture);
	RHI* rhi = Renderer::Get().GetRHI();
	if(rhi)
	{
		m_RHITexture = rhi->CreateTextureFromFile(path.c_str());
		if (!m_RHITexture)
			return EResult::Fail;
	}
	m_Path = path;
	return EResult::Success;
}
#pragma endregion

#pragma region Bind
EResult Texture::Bind(uint32 slot)
{
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi || !m_RHITexture) return EResult::Fail;
	return rhi->BindTexture(m_RHITexture, slot);
}
#pragma endregion

