#pragma once
#include "DirectX12Texture.h"
#include "DirectX12Types.h"

#pragma region Constructor & Destructor
EResult DirectX12Texture::Initialize(const DESC& desc)
{
	if (desc.format == ETextureFormat::UNKNOWN) return EResult::InvalidArgument;
	m_Width = desc.width;
	m_Height = desc.height;
	m_Depth = desc.depth;
	m_MipLevels = desc.mipLevels;
	m_ArraySize = desc.arraySize;

	m_SampleCount = desc.sampleCount;
	m_Format = desc.format;
	m_Dimension = desc.dimension;
	m_Usage = desc.usage;
	m_sRGB = desc.sRGB;

	if (!m_IsOwned)
	{
		m_Texture = static_cast<ID3D12Resource*>(desc.nativeHandle);
		m_RTVHandle = desc.rtvHandle;
		m_CurrentState = desc.initialState;
		Safe_Release(m_RHI);
		return EResult::Success;
	}

	m_Texture = desc.nativeHandle;
	m_RTVHandle = desc.rtvHandle;
	m_CurrentState = desc.initialState;

	DirectX12RHI* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12Device* dxDevice = static_cast<ID3D12Device*>(m_RHI->GetNativeRHI());

	D3D12_RESOURCE_DESC resourceDesc = {};
	resourceDesc.Width = m_Width;
	resourceDesc.Height = m_Height;
	resourceDesc.Dimension = ToD3D12ResourceDimension(m_Dimension);
	resourceDesc.Alignment = 0;
	resourceDesc.DepthOrArraySize = static_cast<UINT16>(m_ArraySize);
	resourceDesc.MipLevels = static_cast<UINT16>(m_MipLevels);
	resourceDesc.Format = ToDXGIFormat(m_Format);
	resourceDesc.SampleDesc.Count = static_cast<UINT>(m_SampleCount);
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	resourceDesc.Flags = ToD3D12ResourceFlags(desc.usage);

	D3D12_HEAP_PROPERTIES heapProps = {};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	heapProps.CreationNodeMask = 1;
	heapProps.VisibleNodeMask = 1;

	D3D12_CLEAR_VALUE clearValue = {};
	D3D12_CLEAR_VALUE* pClearValue = nullptr;

	if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
	{
		clearValue.Format = resourceDesc.Format;
		clearValue.Color[0] = 0.0f; // 기본 Clear Color
		clearValue.Color[1] = 0.0f;
		clearValue.Color[2] = 0.0f;
		clearValue.Color[3] = 1.0f;
		pClearValue = &clearValue;
	}
	else if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
	{
		clearValue.Format = resourceDesc.Format;
		clearValue.DepthStencil.Depth = 1.0f;
		clearValue.DepthStencil.Stencil = 0;
		pClearValue = &clearValue;
	}
	// 5) 텍스처 초기 상태 설정
	// 당장 화면에 쓰이지 않으므로 COMMON 상태로 생성합니다.
	m_CurrentState = D3D12_RESOURCE_STATE_COMMON;
	// 6) GPU 메모리 할당 및 텍스처 생성!
	HRESULT hr = dxDevice->CreateCommittedResource(
		&heapProps,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		m_CurrentState,
		pClearValue,
		IID_PPV_ARGS(&m_Texture) // 우리의 ComPtr에 쏙 들어감
	);
	if (FAILED(hr))
		return EResult::Fail;

	if (HasFlag(m_Usage, ETextureUsage::RenderTarget))
	{
		uint32 index = 0;
		dxRHI->GetRTVAllocator()->Allocate(m_RTVHandle, index);
		dxDevice->CreateRenderTargetView(m_Texture.Get(), nullptr, m_RTVHandle);
	}

	if (HasFlag(m_Usage, ETextureUsage::DepthStencilTarget))
	{
		uint32 index = 0;
		dxRHI->GetDSVAllocator()->Allocate(m_DSVHandle, index);
		dxDevice->CreateDepthStencilView(m_Texture.Get(), nullptr, m_DSVHandle);
	}

	if (HasFlag(m_Usage, ETextureUsage::Sampler))
	{
		uint32 index = 0;
		dxRHI->GetSRVAllocator()->Allocate(m_SRVHandle, index);

		D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

		srvDesc.Format = resourceDesc.Format;
		srvDesc.ViewDimension = ToD3D12SRVDimension(m_Dimension);
		srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		switch (m_Dimension)
		{
		case ETextureDimension::Texture1D:
			srvDesc.Texture1D.MipLevels = m_MipLevels;
			break;
		case ETextureDimension::Texture2D:
			srvDesc.Texture2D.MipLevels = m_MipLevels;
			break;
		case ETextureDimension::Texture2DArray:
			srvDesc.Texture2DArray.MipLevels = m_MipLevels;
			srvDesc.Texture2DArray.ArraySize = m_ArraySize;
			break;
		case ETextureDimension::TextureCube:
			srvDesc.TextureCube.MipLevels = m_MipLevels;
			break;
		case ETextureDimension::Texture3D:
			srvDesc.Texture3D.MipLevels = m_MipLevels;
			break;
		case ETextureDimension::TextureCubeArray:
			srvDesc.TextureCubeArray.MipLevels = m_MipLevels;
			srvDesc.TextureCubeArray.NumCubes = m_ArraySize / 6; // Cube Array의 경우, ArraySize는 큐브의 개수 * 6
			break;
		default:
			break;
		}

		dxDevice->CreateShaderResourceView(m_Texture.Get(), &srvDesc, m_SRVHandle);
		m_SRVGPUHandle = dxRHI->GetSRVAllocator()->GetGPUHandle(index);
	}


	return EResult::Success;
}

DirectX12Texture* DirectX12Texture::Create(DirectX12RHI* rhi, const DESC& desc, bool isOwned)
{
	DirectX12Texture* instance = new DirectX12Texture(rhi);
	instance->m_IsOwned = isOwned;
	if (IsFailure(instance->Initialize(desc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void DirectX12Texture::Free()
{
	if (m_Texture && m_IsOwned)
	{
		m_Texture.Reset();
	}
	m_Texture = nullptr;
	RHIResource::Free();
}
#pragma endregion

EResult DirectX12Texture::Bind(uint32 slot)
{
    return EResult::NotImplemented;
}
