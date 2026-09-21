#pragma once 

#include "RHIBuffer.h"

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

public:
	virtual void* GetNativeHandle() const override { return m_Buffer.Get(); }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		m_Buffer = static_cast<ID3D12Resource*>(nativeHandle);
	}

public:
	virtual void SetData(const void* data, uint32 size) override;
	virtual void* GetMappedPointer() const override { return m_MappedData; }
private:
	ComPtr<ID3D12Resource> m_Buffer;
	void* m_MappedData = nullptr;
};
END