#pragma once

#include "RHI.h"
#include "RHITexture.h"
#include "RHIBuffer.h"
#include "RHIPipeline.h"

void RHI::Free()
{
	for (auto& buffer : m_VertexBuffers) { Safe_Release(buffer); }
	for (auto& texture : m_CurrentTextures)
	{ Safe_Release(texture); }
	for (auto& buffer : m_StorageBuffers)
	{ 
		Safe_Release(buffer); 
	}

	for (auto& cache : m_ConstantBuffers)
	{
		if (!cache.isDynamic && cache.buffer)
		{
			Safe_Release(cache.buffer);
			cache.buffer = nullptr;
		}
	}

	for (auto& buffer : m_DynamicConstantBuffers)
	{ 
		Safe_Release(buffer);
	}
	for (auto& buffer : m_DynamicInstanceBuffers)
	{
		Safe_Release(buffer);
	}

	for (auto& renderTarget : m_CurrentRenderTargets) { Safe_Release(renderTarget); }

	Safe_Release(m_IndexBuffer);
	Safe_Release(m_CurrentShader);
	m_CurrentPipeline = nullptr;
	Safe_Release(m_BackBuffer);
}

#pragma region Bind Resources
#pragma region Texture
EResult RHI::BindTexture(RHITexture* texture, uint32 slot)
{
	if (slot >= MAX_TEXTURE_SLOTS) return EResult::Fail;
	if (!texture)
	{
		Safe_Release(m_CurrentTextures[slot]);
		m_CurrentTextures[slot] = nullptr;
		return EResult::Success;
	}

	if (m_CurrentTextures[slot] == texture) return EResult::Success;
	if (m_CurrentTextures[slot]) Safe_Release(m_CurrentTextures[slot]);
	m_CurrentTextures[slot] = texture;
	Safe_AddRef(m_CurrentTextures[slot]);
	return EResult::Success;
}
#pragma endregion

#pragma region Buffer
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

EResult RHI::BindConstantBuffer(RHIBuffer* buffer, uint32 slot)
{
	if (slot >= MAX_CONSTANT_BUFFER_SLOTS) return EResult::Fail;

	ConstantBufferBinding& state = m_ConstantBuffers[slot];

	if (!state.isDynamic && state.buffer != buffer)
	{
		Safe_Release(state.buffer);
	}

	state.buffer = buffer;
	state.isDynamic = false;
	state.offset = 0;

	if (state.buffer)
		Safe_AddRef(state.buffer);

	return EResult::Success;
}

EResult RHI::BindConstantBuffer(const void* data, uint32 size, uint32 slot)
{
	if (slot >= MAX_CONSTANT_BUFFER_SLOTS || !data || size == 0) return EResult::Fail;

	uint32 alignedSize = (size + (m_ConstantBufferAlignment - 1)) & ~(m_ConstantBufferAlignment - 1);

	if (m_DynamicBufferCursor + alignedSize > DYNAMIC_CONSTANT_BUFFER_SIZE) return EResult::Fail;

	RHIBuffer* ringBuffer = m_DynamicConstantBuffers[m_CurrentBackBufferIndex];
	if (!ringBuffer) return EResult::Fail;

	void* mappedPtr = ringBuffer->GetMappedPointer();
	if (!mappedPtr) return EResult::Fail;

	memcpy(static_cast<uint8*>(mappedPtr) + m_DynamicBufferCursor, data, size);

	ConstantBufferBinding& state = m_ConstantBuffers[slot];

	if (!state.isDynamic && state.buffer)
	{
		Safe_Release(state.buffer);
	}

	state.buffer = ringBuffer;
	state.offset = m_DynamicBufferCursor;
	state.isDynamic = true;

	m_DynamicBufferCursor += alignedSize;

	return EResult::Success;
}

EResult RHI::BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size)
{
	return EResult();
}

EResult RHI::BindStorageBuffer(RHIBuffer* buffer, uint32 slot)
{
	if (slot >= MAX_STORAGE_BUFFERS) return EResult::Fail;

	if (m_StorageBuffers[slot] == buffer)
		return EResult::Success;

	Safe_Release(m_StorageBuffers[slot]);
	m_StorageBuffers[slot] = buffer;
	Safe_AddRef(m_StorageBuffers[slot]);

	return EResult::Success;
}

EResult RHI::BindInstanceBuffer(RHIBuffer* buffer, uint32 offset)
{
	return EResult::NotImplemented;
}
EResult RHI::BindInstanceBuffer(const void* data, uint32 instanceCount, uint32 stride)
{
	if (!data || instanceCount == 0 || stride == 0) return EResult::InvalidArgument;
	const uint32 bytesNeeded = instanceCount * stride;
	const uint32 alignedSize = (bytesNeeded + (m_ConstantBufferAlignment - 1)) & ~(m_ConstantBufferAlignment - 1);
	if (m_DynamicInstanceBufferCursor + alignedSize > DYNAMIC_INSTANCE_BUFFER_SIZE)
	{
		ENGINE_LOG_ERROR("Dynamic instance buffer overflow. Needed: {}, Remaining: {}",
			alignedSize, DYNAMIC_INSTANCE_BUFFER_SIZE - m_DynamicInstanceBufferCursor);
		return EResult::OutOfMemory;
	}
	RHIBuffer* ringBuffer = m_DynamicInstanceBuffers[m_CurrentBackBufferIndex];
	if (!ringBuffer) return EResult::Fail;
	void* mappedPtr = ringBuffer->GetMappedPointer();
	if (!mappedPtr) return EResult::Fail;
	memcpy(static_cast<uint8*>(mappedPtr) + m_DynamicInstanceBufferCursor, data, bytesNeeded);
	const EResult result = BindInstanceBuffer(ringBuffer, m_DynamicInstanceBufferCursor);
	if (IsSuccess(result))
	{
		m_DynamicInstanceBufferCursor += alignedSize;
	}
	return result;
}
#pragma endregion

#pragma endregion

#pragma region Frame Metrics
void RHI::BeginMetricsFrame()
{
	m_CurrentFrameMetrics = {};
	m_CurrentFrameMetrics.PassMetrics.reserve(16);
	m_FrameStartTime = std::chrono::high_resolution_clock::now();
}
void RHI::EndMetricsFrame(bool renderSucceeded)
{
	const auto now = std::chrono::high_resolution_clock::now();
	m_CurrentFrameMetrics.TotalCPUTimeMs = std::chrono::duration<f32, std::milli>(now - m_FrameStartTime).count();
	m_CurrentFrameMetrics.MemoryStats = QueryMemoryStats();
	m_CurrentFrameMetrics.RenderSucceeded = renderSucceeded;
	m_CurrentFrameMetrics.Available = true;
	m_LastFrameMetrics = m_CurrentFrameMetrics;
}
void RHI::BeginPassMetrics(const std::wstring& passName)
{
	m_CurrentPassName = passName;
	m_CurrentPassStartTime = std::chrono::high_resolution_clock::now();
}
void RHI::EndPassMetrics()
{
	const auto now = std::chrono::high_resolution_clock::now();
	const f32 passCpuMs = std::chrono::duration<f32, std::milli>(now - m_CurrentPassStartTime).count();
	RHIPassMetric passMetric{};
	passMetric.PassName = m_CurrentPassName;
	passMetric.CPUTimeMs = passCpuMs;
	passMetric.GPUTimeMs = 0.0f; // 기본 백엔드는 0, D3D12에서 타임스탬프 결과 주입
	m_CurrentFrameMetrics.PassMetrics.push_back(passMetric);
	m_CurrentPassName.clear();
}
void RHI::RecordDraw(uint32 vertexCount)
{
	++m_CurrentFrameMetrics.DrawCalls;
	m_CurrentFrameMetrics.SubmittedVertices += vertexCount;
}

void RHI::RecordIndexedDraw(uint32 indexCount)
{
	++m_CurrentFrameMetrics.IndexedDrawCalls;
	m_CurrentFrameMetrics.SubmittedIndices += indexCount;
}
#pragma endregion





EResult RHI::InitializeConstantBuffers()
{
	for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
	{
		m_DynamicConstantBuffers[i] = CreateBuffer(nullptr, DYNAMIC_CONSTANT_BUFFER_SIZE, 0, ERHIBufferType::Upload);
		if (!m_DynamicConstantBuffers[i])
			return EResult::Fail;
	}
	return EResult::Success;
}

EResult RHI::InitializeInstanceBuffers()
{
	for (uint32 i = 0; i < m_SwapChainBufferCount; ++i)
	{
		m_DynamicInstanceBuffers[i] = CreateBuffer(nullptr, DYNAMIC_INSTANCE_BUFFER_SIZE, 80, ERHIBufferType::Upload);
		if (!m_DynamicInstanceBuffers[i])
			return EResult::Fail;
	}
	return EResult::Success;
}