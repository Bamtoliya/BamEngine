#pragma once

#include "SDLGPUSampler.h"


#pragma region Constructor&Destructor
EResult SDLGPUSampler::Initialize(const DESC& desc)
{
	SDL_GPUSamplerCreateInfo samplerInfo = {};
	// Min Filter
	switch (desc.MinFilter)
	{
	case ESamplerFilter::Point:
		samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
		break;
	case ESamplerFilter::Linear:
		samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
		break;
	}

	// Mag Filter
	switch (desc.MagFilter)
	{
	case ESamplerFilter::Point:
		samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
		break;
	case ESamplerFilter::Linear:
		samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
		break;
	}

	// Mip Filter
	switch (desc.MipFilter)
	{
	case ESamplerFilter::Point:
		samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
		break;
	case ESamplerFilter::Linear:
		samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_LINEAR;
		break;
	}

	// Address Mode
	auto ConvertAddressMode = [](ESamplerAddressMode addressMode) -> SDL_GPUSamplerAddressMode
	{
		switch (addressMode)
		{
		case ESamplerAddressMode::Wrap:
			return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		case ESamplerAddressMode::Mirror:
			return SDL_GPU_SAMPLERADDRESSMODE_MIRRORED_REPEAT;
		case ESamplerAddressMode::Clamp:
			return SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
		default:
			return SDL_GPU_SAMPLERADDRESSMODE_REPEAT;
		}
	};
	samplerInfo.address_mode_u = ConvertAddressMode(desc.AddressU);
	samplerInfo.address_mode_v = ConvertAddressMode(desc.AddressV);
	samplerInfo.address_mode_w = ConvertAddressMode(desc.AddressW);
	samplerInfo.max_anisotropy = desc.MaxAnisotropy;

	m_Sampler = SDL_CreateGPUSampler(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), &samplerInfo);
	if (!m_Sampler)
	{
		return EResult::Fail;
	}

	return EResult::Success;
}

SDLGPUSampler* SDLGPUSampler::Create(RHI* rhi, const DESC& desc)
{
	SDLGPUSampler* instance = new SDLGPUSampler(rhi, desc);
	if (IsFailure(instance->Initialize(desc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void SDLGPUSampler::Free()
{
	if (m_Sampler)
	{
		SDL_ReleaseGPUSampler(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), m_Sampler);
		m_Sampler = nullptr;
	}
	__super::Free();
}
#pragma endregion