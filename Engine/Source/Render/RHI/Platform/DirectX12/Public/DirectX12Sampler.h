#pragma once

#include "RHISampler.h"

BEGIN(Engine)
class ENGINE_API DirectX12Sampler final : public RHISampler
{
#pragma region Constructor&Destructor
private:
	DirectX12Sampler(RHI* rhi, const DESC& desc) :
		RHISampler(rhi, desc)
	{
	}
	virtual ~DirectX12Sampler() = default;
	EResult Initialize(const DESC& desc);
public:
	static DirectX12Sampler* Create(RHI* rhi, const DESC& desc);
	virtual void Free() override;
#pragma endregion

public:
	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const { return m_CPUHandle; }
	void SetCPUHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) { m_CPUHandle = handle; }
	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle() const { return m_GPUHandle; }
	void SetGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { m_GPUHandle = handle; }
	uint32 GetHeapIndex() const { return m_HeapIndex; }
public:
	virtual void* GetNativeHandle() const override { return nullptr; }
	virtual void SetNativeHandle(void* nativeHandle) override {}
private:
	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE m_GPUHandle = {};
	uint32 m_HeapIndex = 0;
};
END