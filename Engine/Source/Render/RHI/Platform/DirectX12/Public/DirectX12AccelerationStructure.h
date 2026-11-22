#pragma once

#include "RHIAccelerationStructure.h"
#include "RenderTargetTypes.h"

BEGIN(Engine)
class DirectX12RHI;

// D3D12 BLAS/TLAS. 결과 버퍼는 RAYTRACING_ACCELERATION_STRUCTURE 상태로 생성되어 그 상태를 유지한다.
// BLAS는 한 번 빌드한 뒤 재사용하고, TLAS는 결과·스크래치·인스턴스 버퍼를 재빌드마다 재사용한다.
class DirectX12AccelerationStructure final : public RHIAccelerationStructure
{
private:
	DirectX12AccelerationStructure(RHI* rhi, ERHIAccelerationStructureType type) :
		RHIAccelerationStructure(rhi, type) {}
	virtual ~DirectX12AccelerationStructure() = default;
public:
	// 현재 프레임 커맨드 리스트에 BLAS 빌드를 기록한다. BLAS 간 UAV 배리어는 BuildTopLevel이 한 번에 기록한다.
	static DirectX12AccelerationStructure* CreateBottomLevel(DirectX12RHI* rhi, const RHIRaytracingGeometryDesc& desc);
	static EResult BuildTopLevel(DirectX12RHI* rhi, RHIAccelerationStructure*& topLevel,
		const RHIRaytracingInstanceDesc* instances, uint32 instanceCount);
	virtual void Free() override;
public:
	virtual void* GetNativeHandle() const override { return m_Result.Get(); }
	D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress() const { return m_Result ? m_Result->GetGPUVirtualAddress() : 0; }
private:
	EResult EnsureInstanceCapacity(DirectX12RHI* rhi, uint32 frameIndex, uint32 instanceCount);
private:
	ComPtr<ID3D12Resource> m_Result;

	// TLAS 전용: 재빌드마다 재사용하고 부족할 때만 키운다.
	ComPtr<ID3D12Resource> m_Scratch;
	uint64 m_ScratchSizeInBytes = 0;
	// CPU가 쓰는 인스턴스 목록은 GPU가 이전 프레임 목록을 읽는 동안 덮어쓰지 않도록 프레임 슬롯별로 둔다.
	ComPtr<ID3D12Resource> m_InstanceBuffers[MAX_SWAPCHAIN_BUFFERS];
	void* m_InstanceMapped[MAX_SWAPCHAIN_BUFFERS] = {};
	uint32 m_InstanceCapacity[MAX_SWAPCHAIN_BUFFERS] = {};
};
END
