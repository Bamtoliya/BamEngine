#pragma once 

#include "RHIBuffer.h"
#include "GpuHeapPageAllocator.h"

BEGIN(Engine)
class DirectX12Buffer final : public RHIBuffer
{
	friend class SDLGPURHI;
private:
	DirectX12Buffer(RHI* rhi, const DESC& desc) :
		RHIBuffer(rhi, desc)
	{
	}
	virtual ~DirectX12Buffer() = default;
	EResult Initialize(const DESC& desc);
public:
	static DirectX12Buffer* Create(RHI* rhi, const DESC& desc);
	virtual void Free() override;

public:
	D3D12_VERTEX_BUFFER_VIEW GetVertexBufferView() const;
	D3D12_INDEX_BUFFER_VIEW GetIndexBufferView() const;
	D3D12_RESOURCE_STATES GetCurrentState() const { return m_CurrentState; }
	void SetCurrentState(D3D12_RESOURCE_STATES state) { m_CurrentState = state; }

public:
	virtual void* GetNativeHandle() const override { return m_Buffer.Get(); }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		m_Buffer = static_cast<ID3D12Resource*>(nativeHandle);
	}

public:
	uint32 GetBindlessDescriptorIndex() const { return m_BindlessDescriptorIndex; }
	void SetBindlessDescriptorIndex(uint32 index) { m_BindlessDescriptorIndex = index; }

public:
	virtual void SetData(const void* data, uint32 size) override;
	virtual void* GetMappedPointer() const override { return m_MappedData; }
public:
	EResult QueryUploadComplete(bool& complete) override;
protected:
	EResult UploadRangeInternal(const void* data, uint32 size, uint32 offset) override;
private:
	ComPtr<ID3D12Resource> m_Buffer;
	GpuHeapAllocation m_HeapAllocation; // 정적 지오메트리 힙에 배치했을 때만 유효
	uint32 m_BindlessDescriptorIndex = UINT32_MAX;
	void* m_MappedData = nullptr;
	D3D12_RESOURCE_STATES m_CurrentState = D3D12_RESOURCE_STATE_COMMON;
};
END