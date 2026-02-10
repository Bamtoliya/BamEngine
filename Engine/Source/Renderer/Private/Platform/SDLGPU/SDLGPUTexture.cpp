#pragma once

#include "SDLGPUTexture.h"


#pragma region Constructor&Destructor

EResult SDLGPUTexture::Initialize(const SDL_GPUTextureCreateInfo& createInfo)
{
	if (createInfo.format == SDL_GPU_TEXTUREFORMAT_INVALID && !m_IsOwned) return EResult::Success;
	m_Texture = SDL_CreateGPUTexture(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), &createInfo);
	return m_Texture ? EResult::Success : EResult::Fail;
}


SDLGPUTexture* SDLGPUTexture::Create(SDLGPURHI* rhi, const SDL_GPUTextureCreateInfo& createInfo, bool isOwned)
{
	SDLGPUTexture* Instance = new SDLGPUTexture(rhi);
	Instance->m_IsOwned = isOwned;
	if (IsFailure(Instance->Initialize(createInfo)))
	{
		Safe_Release(Instance);
		return nullptr;
	}
	return Instance;
}

void SDLGPUTexture::Free()
{
	if (m_Texture && m_IsOwned)
	{
		SDL_ReleaseGPUTexture(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), m_Texture);
	}
	m_Texture = nullptr;
	__super::Free();
}
#pragma endregion

#pragma region Bind
EResult SDLGPUTexture::Bind(uint32 slot)
{
	if (!m_RHI || !m_Texture) return EResult::Fail;
	return m_RHI->BindTexture(this, slot);
}
