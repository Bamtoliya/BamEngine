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
    m_ClearColor = desc.clearColor;
    m_ClearDepth = desc.clearDepth;
    m_ClearStencil = desc.clearStencil;

    if (!m_IsOwned)
    {
        m_Texture = static_cast<ID3D12Resource*>(desc.nativeHandle);
        m_RTVHandle = desc.rtvHandle;
        m_CurrentState = desc.initialState;
        return EResult::Success;
    }

    m_Texture = desc.nativeHandle;
    m_RTVHandle = desc.rtvHandle;
    m_CurrentState = desc.initialState;

    DirectX12RHI* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
    ID3D12Device* dxDevice = static_cast<ID3D12Device*>(m_RHI->GetNativeRHI());

    DXGI_FORMAT dxgiFormat = ToDXGIFormat(m_Format);
    bool        isDepth = IsDepthFormat(dxgiFormat);
    bool        needsSRV = HasFlag(m_Usage, ETextureUsage::Sampler);

    D3D12_RESOURCE_DESC resourceDesc = {};
    resourceDesc.Width = m_Width;
    resourceDesc.Height = m_Height;
    resourceDesc.Dimension = ToD3D12ResourceDimension(m_Dimension);
    resourceDesc.Alignment = 0;
    resourceDesc.DepthOrArraySize = static_cast<UINT16>(m_ArraySize);
    resourceDesc.MipLevels = static_cast<UINT16>(m_MipLevels);
    // [핵심 수정] DSV+SRV 동시 사용 시 Typeless 포맷으로 리소스 생성
    resourceDesc.Format = (isDepth && needsSRV)
        ? ToTypelessFormat(dxgiFormat)
        : dxgiFormat;
    resourceDesc.SampleDesc.Count = static_cast<UINT>(m_SampleCount);
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = ToD3D12ResourceFlags(desc.usage);

    // DSV 전용(SRV 불필요)이면 DENY_SHADER_RESOURCE 추가 (성능 최적화)
    if (isDepth && !needsSRV)
        resourceDesc.Flags |= D3D12_RESOURCE_FLAG_DENY_SHADER_RESOURCE;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    D3D12_CLEAR_VALUE  clearValue = {};
    D3D12_CLEAR_VALUE* pClearValue = nullptr;

    if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET)
    {
        clearValue.Format = resourceDesc.Format;
        clearValue.Color[0] = desc.clearColor.x;
        clearValue.Color[1] = desc.clearColor.y;
        clearValue.Color[2] = desc.clearColor.z;
        clearValue.Color[3] = desc.clearColor.w;
        pClearValue = &clearValue;
    }
    else if (resourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)
    {
        // ClearValue 포맷은 반드시 실제 depth 포맷 사용 (typeless 불가)
        clearValue.Format = dxgiFormat;
        clearValue.DepthStencil.Depth = desc.clearDepth;
        clearValue.DepthStencil.Stencil = desc.clearStencil;
        pClearValue = &clearValue;
    }

    m_CurrentState = D3D12_RESOURCE_STATE_COMMON;
    HRESULT hr = dxDevice->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        m_CurrentState,
        pClearValue,
        IID_PPV_ARGS(&m_Texture)
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

        // DSV는 실제 depth 포맷 사용
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
        dsvDesc.Format = dxgiFormat;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dxDevice->CreateDepthStencilView(m_Texture.Get(), &dsvDesc, m_DSVHandle);
    }

    if (HasFlag(m_Usage, ETextureUsage::Sampler))
    {
        uint32 index = 0;
        dxRHI->GetSRVAllocator()->Allocate(m_SRVHandle, index);

        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        // [핵심 수정] Depth 텍스처는 SRV 전용 포맷으로 변환
        srvDesc.Format = isDepth ? ToDepthSRVFormat(dxgiFormat) : resourceDesc.Format;
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
            srvDesc.TextureCubeArray.NumCubes = m_ArraySize / 6;
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
