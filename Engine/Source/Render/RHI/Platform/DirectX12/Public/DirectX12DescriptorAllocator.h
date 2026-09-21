#pragma once

#include "DirectX12Types.h"
#include <queue>
#include <mutex>

BEGIN(Engine)
class ENGINE_API DirectX12DescriptorAllocator final
{
public:
	DirectX12DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 maxCount);
    ~DirectX12DescriptorAllocator();

    bool Allocate(D3D12_CPU_DESCRIPTOR_HANDLE& outCPUHandle, uint32& outIndex);
    void Free(uint32 index);

    ID3D12DescriptorHeap* GetHeap() const { return m_Heap.Get(); }
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(uint32 index) const;

private:
	ComPtr<ID3D12DescriptorHeap> m_Heap = { nullptr };
    D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
    uint32 m_DescriptorSize = 0;
    uint32 m_MaxCount = 0;

    uint32 m_CurrentIndex = 0;
    std::queue<uint32> m_FreeIndices;

    std::mutex m_AllocationMutex;
};
END