#include "DirectX12Sampler.h"
#include "DirectX12RHI.h"

EResult DirectX12Sampler::Initialize(const DESC& desc)
{
    DirectX12RHI* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
    ID3D12Device* device = static_cast<ID3D12Device*>(dxRHI->GetNativeRHI());

    // 1. 할당기에서 빈자리(Handle) 번호표 뽑기
    if (!dxRHI->GetSamplerAllocator()->Allocate(m_CPUHandle, m_HeapIndex))
    {
        return EResult::Fail; // 할당기 꽉 참
    }
    // 2. 샘플러 설정 채우기
    D3D12_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = ToD3D12Filter(m_Desc.MinFilter, m_Desc.MagFilter, m_Desc.MipFilter);
    samplerDesc.AddressU = ToD3D12AddressMode(m_Desc.AddressU);
    samplerDesc.AddressV = ToD3D12AddressMode(m_Desc.AddressV);
    samplerDesc.AddressW = ToD3D12AddressMode(m_Desc.AddressW);
    samplerDesc.MipLODBias = 0.0f;
    samplerDesc.MaxAnisotropy = m_Desc.MaxAnisotropy;
    samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 기본적으로 비교 안함 (ShadowMap 등에선 쓰임)

    samplerDesc.BorderColor[0] = m_Desc.BorderColor.x;
    samplerDesc.BorderColor[1] = m_Desc.BorderColor.y;
    samplerDesc.BorderColor[2] = m_Desc.BorderColor.z;
    samplerDesc.BorderColor[3] = m_Desc.BorderColor.w;

    samplerDesc.MinLOD = 0.0f;
    samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
    // 3. 디바이스에 샘플러 굽기 (지정된 Handle 위치에 기록)
    device->CreateSampler(&samplerDesc, m_CPUHandle);
    m_GPUHandle = dxRHI->GetSamplerAllocator()->GetGPUHandle(m_HeapIndex);
    return EResult::Success;
}

DirectX12Sampler* DirectX12Sampler::Create(RHI* rhi, const DESC& desc)
{
    DirectX12Sampler* instance = new DirectX12Sampler(rhi, desc);
    if (IsFailure(instance->Initialize(desc)))
    {
        instance->Release();
        return nullptr;
    }
    return instance;
}

void DirectX12Sampler::Free()
{
    if (m_RHI)
    {
		DirectX12RHI* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
		if (dxRHI->GetSamplerAllocator())
		{
			dxRHI->GetSamplerAllocator()->Free(m_HeapIndex);
		}
    }
    __super::Free();
}
