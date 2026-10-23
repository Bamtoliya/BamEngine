#pragma once

#include "SDLGPUBuffer.h"
#include "SDLGPURHI.h"

#pragma region Constructor&Destructor
EResult SDLGPUBuffer::Initialize(const DESC& desc)
{
	SDLGPURHI* rhi = static_cast<SDLGPURHI*>(m_RHI);
	SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(rhi->GetNativeRHI());

	SDL_GPUBufferCreateInfo createInfo = {};
	switch (desc.bufferType)
	{
	case ERHIBufferType::Vertex:
		createInfo.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
		break;
	case ERHIBufferType::Index:
		createInfo.usage = SDL_GPU_BUFFERUSAGE_INDEX;
		break;
	case ERHIBufferType::Structured:
		createInfo.usage = SDL_GPU_BUFFERUSAGE_GRAPHICS_STORAGE_READ;
		break;
	default:
		break;
	}
	createInfo.size = desc.size;

	m_Size = desc.size;

	m_GPUBuffer = SDL_CreateGPUBuffer(device, &createInfo);
	if (!m_GPUBuffer) return EResult::Fail;

	if (desc.initialData)
	{
		const EResult result = rhi->UploadBufferData(m_GPUBuffer, desc.initialData, desc.size);
		if (result != EResult::Success)
			return result;
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
	if (m_RHI)
	{
		auto* device = static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());

		if (m_UploadFence)
			SDL_ReleaseGPUFence(device, m_UploadFence);

		if (m_GPUBuffer)
			SDL_ReleaseGPUBuffer(device, m_GPUBuffer);
	}

	m_UploadFence = nullptr;
	m_GPUBuffer = nullptr;
	__super::Free();
}
#pragma endregion


void SDLGPUBuffer::SetData(const void* data, uint32 size)
{
	if (!data || size == 0 || !m_RHI || !m_GPUBuffer)
		return;

	if (size > m_Size)
		size = m_Size;

	auto* rhi = static_cast<SDLGPURHI*>(m_RHI);
	rhi->UploadBufferData(m_GPUBuffer, const_cast<void*>(data), size);
}

EResult SDLGPUBuffer::UploadRangeInternal(const void* data, uint32 size, uint32 offset)
{
	bool complete = false;
	const EResult result = QueryUploadComplete(complete);

	if (result != EResult::Success)
		return result;

	if (!complete)
		return EResult::AlreadyInitialized;

	auto* rhi = static_cast<SDLGPURHI*>(m_RHI);
	return rhi->UploadBufferRange(m_GPUBuffer, data, size, offset, m_UploadFence);
}

EResult SDLGPUBuffer::QueryUploadComplete(bool& complete)
{
	complete = false;

	if (!m_RHI || !m_GPUBuffer)
		return EResult::Fail;

	auto* device = static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());
	if (!device)
		return EResult::Fail;

	if (m_UploadFence)
	{
		if (!SDL_QueryGPUFence(device, m_UploadFence))
			return EResult::Success;

		SDL_ReleaseGPUFence(device, m_UploadFence);
		m_UploadFence = nullptr;
	}

	complete = true;
	return EResult::Success;
}