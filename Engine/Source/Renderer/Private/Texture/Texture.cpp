#pragma once

#include "Texture.h"
#include "Renderer.h"

#pragma region Constructor&Destructor
EResult Texture::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	wstring* path = reinterpret_cast<wstring*>(arg);
	m_Path = *path;
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
		delete instance;
		return nullptr;
	}
	return instance;
}

void Texture::Free()
{
	Base::Free();
	Safe_Release(m_RHITexture);
}
#pragma endregion
