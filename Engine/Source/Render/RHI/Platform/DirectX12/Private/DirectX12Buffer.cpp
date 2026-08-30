#include "DirectX12Buffer.h"
#include "DirectX12RHI.h"

EResult DirectX12Buffer::Initialize(const DESC& desc)
{
	DirectX12RHI* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12Device* device = static_cast<ID3D12Device*>(dxRHI->GetNativeRHI());

	if (m_BufferType == ERHIBufferType::Constant)
	{
		m_Size = (desc.size + 255) & ~255; // Align to 256 bytes for constant buffers
	}

	D3D12_HEAP_PROPERTIES heapProps = {};
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_GENERIC_READ;

	if (m_BufferType == ERHIBufferType::Readback)
	{
		heapProps.Type = D3D12_HEAP_TYPE_READBACK;
		initialState = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	else
	{
		TODO("StaticMesh May Use D3D12_HEAP_TYPE_DEFAULT");
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	D3D12_RESOURCE_DESC resourceDesc = {};

	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = m_Size;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	if (FAILED(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&m_Buffer))))
	{
		return EResult::Fail;
	}

	if(heapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
	{
		D3D12_RANGE readRange = { 0, 0 }; // CPU에서 이 메모리를 읽지는 않을 것이라고 힌트 제공
		m_Buffer->Map(0, &readRange, &m_MappedData);
	}
	// 초기 데이터(initialData)가 있다면 즉시 복사
	if (desc.initialData != nullptr && m_MappedData != nullptr)
	{
		memcpy(m_MappedData, desc.initialData, desc.size);
	}
	return EResult::Success;
}

DirectX12Buffer* DirectX12Buffer::Create(RHI* rhi, const DESC& desc)
{
	DirectX12Buffer* instance = new DirectX12Buffer(rhi, desc);
	if (IsFailure(instance->Initialize(desc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void DirectX12Buffer::Free()
{
	if (m_Buffer && m_MappedData)
	{
		m_Buffer->Unmap(0, nullptr);
		m_MappedData = nullptr;
	};
	m_Buffer = nullptr;
	__super::Free();
}

D3D12_VERTEX_BUFFER_VIEW DirectX12Buffer::GetVertexBufferView() const
{
	D3D12_VERTEX_BUFFER_VIEW vbv = {};
	if (m_Buffer)
	{
		vbv.BufferLocation = m_Buffer->GetGPUVirtualAddress();
		vbv.SizeInBytes = m_Size;
		vbv.StrideInBytes = m_Stride;
	}
	return vbv;
}

D3D12_INDEX_BUFFER_VIEW DirectX12Buffer::GetIndexBufferView() const
{
	D3D12_INDEX_BUFFER_VIEW ibv = {};
	if (m_Buffer)
	{
		ibv.BufferLocation = m_Buffer->GetGPUVirtualAddress();
		ibv.SizeInBytes = m_Size;
		// DX12는 인덱스 버퍼 포맷이 16비트인지 32비트인지 stride를 보고 알아서 판단하도록 유도합니다.
		ibv.Format = (m_Stride == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	}
	return ibv;
}

void DirectX12Buffer::SetData(const void* data, uint32 size)
{
	if (m_MappedData && data)
	{
		memcpy(m_MappedData, data, size);
	}
}
