#pragma once

#include "SDLGPUTexture.h"


#pragma region Constructor&Destructor

EResult SDLGPUTexture::Initialize(const RHITextureDesc& desc)
{
	if (desc.format == ETextureFormat::UNKNOWN || !m_IsOwned) return EResult::Success;

	m_Width = desc.width;
	m_Height = desc.height;
	m_Depth = desc.depth;
	m_MipLevels = desc.mipLevels;
	m_ArraySize = desc.arraySize;
	
	m_SampleCount = desc.sampleCount;
	m_Format = desc.format;
	m_Dimension = desc.dimension;
	m_Usage = desc.usage;
	m_sRGB = desc.sRGB;

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


SDLGPUTexture* SDLGPUTexture::Create(SDLGPURHI* rhi, const RHITextureDesc& desc, bool isOwned)
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
