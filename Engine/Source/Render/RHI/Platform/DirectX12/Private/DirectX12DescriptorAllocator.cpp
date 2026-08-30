#include "DirectX12DescriptorAllocator.h"

DirectX12DescriptorAllocator::DirectX12DescriptorAllocator(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32 maxCount)
	: m_Type(type), m_MaxCount(maxCount)
{
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = type;
    heapDesc.NumDescriptors = maxCount;
	if (type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV || type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
	{
		
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	}
	else
	{
		heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}
    
    heapDesc.NodeMask = 0;

    device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_Heap));
    m_DescriptorSize = device->GetDescriptorHandleIncrementSize(type);
}

DirectX12DescriptorAllocator::~DirectX12DescriptorAllocator()
{
	m_Heap.Reset();
    m_Heap = nullptr;
}

bool DirectX12DescriptorAllocator::Allocate(D3D12_CPU_DESCRIPTOR_HANDLE& outCPUHandle, uint32& outIndex)
{
	std::lock_guard<std::mutex> lock(m_AllocationMutex);

	if (!m_FreeIndices.empty())
	{
		outIndex = m_FreeIndices.front();
		m_FreeIndices.pop();
	}
	else
	{
		if (m_CurrentIndex >= m_MaxCount)
			return false;
		outIndex = m_CurrentIndex++;
	}

	outCPUHandle = m_Heap->GetCPUDescriptorHandleForHeapStart();
	outCPUHandle.ptr += static_cast<SIZE_T>(outIndex) * m_DescriptorSize;

	return true;
}

void DirectX12DescriptorAllocator::Free(uint32 index)
{
	std::lock_guard<std::mutex> lock(m_AllocationMutex);
	m_FreeIndices.push(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE DirectX12DescriptorAllocator::GetGPUHandle(uint32 index) const
{
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = m_Heap->GetGPUDescriptorHandleForHeapStart();
	gpuHandle.ptr += static_cast<SIZE_T>(index) * m_DescriptorSize;
    return gpuHandle;
}
