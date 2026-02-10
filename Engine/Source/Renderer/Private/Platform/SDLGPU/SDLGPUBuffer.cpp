#pragma once

#include "SDLGPUBuffer.h"
#include "SDLGPURHI.h"

#pragma region Constructor&Destructor
EResult SDLGPUBuffer::Initialize(const DESC& desc)
{
	memcpy(m_Data.data(), desc.InitialData, desc.Size);
	SDLGPURHI* rhi = static_cast<SDLGPURHI*>(m_RHI);
	SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(rhi->GetNativeRHI());

	SDL_GPUBufferCreateInfo createInfo = {};
	switch (desc.BufferType)
	{
	case ERHIBufferType::Vertex:
		createInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		break;
	case ERHIBufferType::Index:
		createInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
		break;
	default:
		break;
	}
	createInfo.size = desc.Size;

	m_GPUBuffer = SDL_CreateGPUBuffer(device, &createInfo);
	if (!m_GPUBuffer) return EResult::Fail;

	if (desc.InitialData)
	{
		rhi->UploadBufferData(m_GPUBuffer, desc.InitialData, desc.Size);
	}

	return EResult::Success;
}
SDLGPUBuffer* SDLGPUBuffer::Create(RHI* rhi,const DESC& desc)
{
	SDLGPUBuffer* instance = new SDLGPUBuffer(rhi, desc);
	if (IsFailure(instance->Initialize(desc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
void SDLGPUBuffer::Free()
{
	m_Data.clear();	
	if (m_GPUBuffer)
	{
		SDL_ReleaseGPUBuffer(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), m_GPUBuffer);
		m_GPUBuffer = nullptr;
	}
	__super::Free();
}
#pragma endregion


void SDLGPUBuffer::SetData(const void* data, uint32 size)
{
	if (!data || size == 0) return;
	if (size > m_Data.size())
	{
		m_Size = size;
		m_Data.resize(size);
	}
	memcpy(m_Data.data(), data, size);
	SDLGPURHI* rhi = static_cast<SDLGPURHI*>(m_RHI);
	if (rhi && m_GPUBuffer)
	{
		rhi->UploadBufferData(m_GPUBuffer, m_Data.data(), m_Size);
	}
}