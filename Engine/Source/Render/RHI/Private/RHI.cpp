#pragma once

#include "RHI.h"
#include "RHITexture.h"
#include "RHIBuffer.h"
#include "RHIPipeline.h"

void RHI::Free()
{
	for (auto& buffer : m_VertexBuffers) { Safe_Release(buffer); }
	for (auto& buffer : m_VertexStorageBuffers) { Safe_Release(buffer); }
	for (auto& buffer : m_FragmentStorageBuffers) { Safe_Release(buffer); }
	for (auto& buffer : m_ComputeStorageBuffers) { Safe_Release(buffer); }

	Safe_Release(m_IndexBuffer);
	Safe_Release(m_CurrentShader);
	m_CurrentPipeline = nullptr;
	Safe_Release(m_BackBuffer);
}

EResult RHI::BindVertexBuffers(uint32 firstSlot, RHIBuffer** vertexBuffers, uint32 count)
{
	if (!vertexBuffers || count == 0) return EResult::Fail;
	for (uint32 i = 0; i < count; ++i)
	{
		if (firstSlot + i >= MAX_BUFFER_SLOTS) break;
		Safe_Release(m_VertexBuffers[firstSlot + i]);
		m_VertexBuffers[firstSlot + i] = vertexBuffers[i];
		Safe_AddRef(m_VertexBuffers[firstSlot + i]);
	}
	m_NumVertexBuffersBound = std::max(m_NumVertexBuffersBound, firstSlot + count);
	return EResult::Success;
}

EResult RHI::BindIndexBuffer(RHIBuffer* indexBuffer)
{
	if (!indexBuffer) return EResult::Fail;
	Safe_Release(m_IndexBuffer);
	m_IndexBuffer = indexBuffer;
	Safe_AddRef(m_IndexBuffer);
	return EResult::Success;
}

EResult RHI::BindVertexStorageBuffers(uint32 firstSlot, RHIBuffer** storageBuffers, uint32 count)
{
	if (!storageBuffers || count == 0) return EResult::Fail;
	for (uint32 i = 0; i < count; ++i)
	{
		if (firstSlot + i >= MAX_BUFFER_SLOTS) break;
		Safe_Release(m_VertexStorageBuffers[firstSlot + i]);
		m_VertexStorageBuffers[firstSlot + i] = storageBuffers[i];
		Safe_AddRef(m_VertexStorageBuffers[firstSlot + i]);
	}
	m_NumVertexStorageBuffersBound = std::max(m_NumVertexStorageBuffersBound, firstSlot + count);
	return EResult::Success;
}

EResult RHI::BindFragmentStorageBuffers(uint32 firstSlot, RHIBuffer** storageBuffers, uint32 count)
{
	if (!storageBuffers || count == 0) return EResult::Fail;
	for (uint32 i = 0; i < count; ++i)
	{	
		if (firstSlot + i >= MAX_BUFFER_SLOTS) break;
		Safe_Release(m_FragmentStorageBuffers[firstSlot + i]);
		m_FragmentStorageBuffers[firstSlot + i] = storageBuffers[i];
		Safe_AddRef(m_FragmentStorageBuffers[firstSlot + i]);
	}
	m_NumFragmentStorageBuffersBound = std::max(m_NumFragmentStorageBuffersBound, firstSlot + count);
	return EResult::Success;
}

EResult RHI::BindComputeStorageBuffers(uint32 firstSlot, RHIBuffer** storageBuffers, uint32 count)
{
	if (!storageBuffers || count == 0) return EResult::Fail;
	for (uint32 i = 0; i < count; ++i)
	{
		if (firstSlot + i >= MAX_BUFFER_SLOTS) break;
		Safe_Release(m_ComputeStorageBuffers[firstSlot + i]);
		m_ComputeStorageBuffers[firstSlot + i] = storageBuffers[i];
		Safe_AddRef(m_ComputeStorageBuffers[firstSlot + i]);
	}
	m_NumComputeStorageBuffersBound = std::max(m_NumComputeStorageBuffersBound, firstSlot + count);
	return EResult::Success;
}