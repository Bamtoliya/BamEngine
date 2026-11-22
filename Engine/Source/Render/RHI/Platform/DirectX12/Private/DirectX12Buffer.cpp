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
	D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;

	const bool isStaticGeometryBuffer =
		(m_BufferType == ERHIBufferType::Vertex || m_BufferType == ERHIBufferType::Index) &&
		desc.initialData != nullptr;

	if (m_BufferType == ERHIBufferType::Readback)
	{
		heapProps.Type = D3D12_HEAP_TYPE_READBACK;
		initialState = D3D12_RESOURCE_STATE_COPY_DEST;
	}
	else if (m_BufferType == ERHIBufferType::Storage)
	{
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		initialState = D3D12_RESOURCE_STATE_COMMON;
		resourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	}
	else if (isStaticGeometryBuffer)
	{
		heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
		initialState = D3D12_RESOURCE_STATE_COMMON;
	}
	else
	{
		heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
	}

	m_CurrentState = initialState;

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
	resourceDesc.Flags = resourceFlags;
	// 정적 정점·인덱스 버퍼는 큰 힙에 배치한다. 버퍼마다 GPU 메모리를 새로 잡으면 로딩이 크게 느려진다.
	const bool placed = isStaticGeometryBuffer && dxRHI->CreatePlacedGeometryBuffer(resourceDesc, initialState, m_Buffer, m_HeapAllocation);
	if (!placed && FAILED(device->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&m_Buffer))))
	{
		return EResult::Fail;
	}

	if (heapProps.Type == D3D12_HEAP_TYPE_UPLOAD)
	{
		D3D12_RANGE readRange = { 0, 0 };

		if (FAILED(m_Buffer->Map(0, &readRange, &m_MappedData)))
		{
			return EResult::Fail;
		}

		// 초기 데이터(initialData)가 있다면 즉시 복사
		if (desc.initialData != nullptr && m_MappedData != nullptr)
		{
			memcpy(m_MappedData, desc.initialData, desc.size);
		}
	}
	else if (heapProps.Type == D3D12_HEAP_TYPE_DEFAULT && desc.initialData != nullptr)
	{
		if (IsFailure(dxRHI->UploadBufferData(this, desc.initialData, desc.size, 0)))
		{
			return EResult::Fail;
		}
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
		m_Buffer->Unmap(0, nullptr);

	m_MappedData = nullptr;

	if (m_RHI)
		static_cast<DirectX12RHI*>(m_RHI)->DeferRelease(m_Buffer.Get(), m_HeapAllocation);

	m_HeapAllocation = {};
	m_Buffer.Reset();
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
	if (!data || size == 0 || !m_Buffer)
		return;

	if (size > m_Size)
		size = m_Size;

	if (m_MappedData)
	{
		memcpy(m_MappedData, data, size);
	}
	else if (m_RHI)
	{
		static_cast<DirectX12RHI*>(m_RHI)->UploadBufferData(this, data, size, 0);
	}
}

EResult DirectX12Buffer::UploadRangeInternal(const void* data, uint32 size, uint32 offset)
{
	if (!m_Buffer)
		return EResult::Fail;

	if (m_MappedData)
	{
		auto* destination = static_cast<uint8*>(m_MappedData) + offset;
		std::memcpy(destination, data, size);
		return EResult::Success;
	}

	if (!m_RHI)
		return EResult::Fail;

	return static_cast<DirectX12RHI*>(m_RHI)->UploadBufferData(this, data, size, offset);
}

EResult DirectX12Buffer::QueryUploadComplete(bool& complete)
{
	complete = false;

	if (!m_Buffer)
		return EResult::Fail;

	complete = true;
	return EResult::Success;
}