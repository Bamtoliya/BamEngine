#pragma once

#include "SDLGPUTexture.h"


#pragma region Constructor&Destructor

EResult SDLGPUTexture::Initialize(const tagRHITextureDesc& desc)
{
	if (desc.Format == ETextureFormat::UNKNOWN && !m_IsOwned) return EResult::Success;

	m_Width = desc.Width;
	m_Height = desc.Height;
	m_Depth = desc.Depth;
	m_MipLevels = desc.MipLevels;
	m_ArraySize = desc.ArraySize;
	
	m_SampleCount = desc.SampleCount;
	m_Format = desc.Format;
	m_Dimension = desc.Dimension;
	m_Usage = desc.Usage;

	SDL_GPUTextureCreateInfo createInfo{};
	createInfo.width = m_Width;
	createInfo.height = m_Height;
	createInfo.layer_count_or_depth = m_Dimension == ETextureDimension::Texture3D ? m_Depth : m_ArraySize;
	createInfo.num_levels = m_MipLevels;

	createInfo.sample_count = ToSDLGPUTextureSampleCount(m_SampleCount);
	createInfo.format = ToSDLGPUTextureFormat(m_Format);
	createInfo.type = ToSDLGPUTextureType(m_Dimension);
	createInfo.usage = ToSDLGPUTextureUsage(m_Usage);

	m_Texture = SDL_CreateGPUTexture(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), &createInfo);
	return m_Texture ? EResult::Success : EResult::Fail;
}


SDLGPUTexture* SDLGPUTexture::Create(SDLGPURHI* rhi, const tagRHITextureDesc& desc, bool isOwned)
{
	SDLGPUTexture* Instance = new SDLGPUTexture(rhi);
	Instance->m_IsOwned = isOwned;
	if (IsFailure(Instance->Initialize(desc)))
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
