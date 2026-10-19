#include "DirectX12Pipeline.h"
#include "DirectX12RHI.h"
#include "DirectX12Shader.h"
#include "Logger.h"
#include <directx/d3d12sdklayers.h>
#include <cstddef>

#pragma region Constructor & Destructor
EResult DirectX12Pipeline::Initialize(const DESC& desc)
{
	Safe_AddRef(desc.vertexShader);
	Safe_AddRef(desc.pixelShader);
	return CreatePipelineState();
}

DirectX12Pipeline* DirectX12Pipeline::Create(RHI* rhi, const DESC& desc)
{
	DirectX12Pipeline* instance = new DirectX12Pipeline(rhi, desc);
	if (IsFailure(instance->Initialize(desc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void DirectX12Pipeline::Free()
{
	m_Pipeline = nullptr;
	Safe_Release(m_Desc.vertexShader);
	Safe_Release(m_Desc.pixelShader);
	__super::Free();
}

EResult DirectX12Pipeline::Rebuild()
{
	m_Pipeline = nullptr;
	return CreatePipelineState();
}
#pragma endregion

#pragma region Pipeline State Creation
EResult DirectX12Pipeline::CreatePipelineState()
{
	DirectX12RHI* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12Device* dxDevice = static_cast<ID3D12Device*>(dxRHI->GetNativeRHI());

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = static_cast<ID3D12RootSignature*>(dxRHI->GetGlobalRootSignature());

	if (m_Desc.vertexShader)
	{
		DirectX12Shader* vs = static_cast<DirectX12Shader*>(m_Desc.vertexShader);
		psoDesc.VS = { vs->GetBytecode().pShaderBytecode, vs->GetBytecode().BytecodeLength };
	}
	if (m_Desc.pixelShader)
	{
		DirectX12Shader* ps = static_cast<DirectX12Shader*>(m_Desc.pixelShader);
		psoDesc.PS = { ps->GetBytecode().pShaderBytecode, ps->GetBytecode().BytecodeLength };
	}

	psoDesc.RasterizerState.FillMode = ToD3D12FillMode(m_Desc.fillMode);
	psoDesc.RasterizerState.CullMode = ToD3D12CullMode(m_Desc.cullMode);
	psoDesc.RasterizerState.FrontCounterClockwise = ToD3D12FrontFace(m_Desc.frontFace);
	psoDesc.RasterizerState.DepthClipEnable = TRUE;
	psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	psoDesc.DepthStencilState.DepthEnable = m_Desc.depthStencilState.depthTestEnable ? TRUE : FALSE;
	psoDesc.DepthStencilState.DepthWriteMask = m_Desc.depthStencilState.depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = ToD3D12ComparisonFunc(m_Desc.depthStencilState.depthCompareOp);
	psoDesc.DepthStencilState.StencilEnable = m_Desc.depthStencilState.stencilTestEnable ? TRUE : FALSE;
	psoDesc.DepthStencilState.StencilReadMask = m_Desc.depthStencilState.stencilReadMask;
	psoDesc.DepthStencilState.StencilWriteMask = m_Desc.depthStencilState.stencilWriteMask;

	D3D12_DEPTH_STENCILOP_DESC stencilOpDesc = {};
	stencilOpDesc.StencilFailOp = ToD3D12StencilOp(m_Desc.depthStencilState.stencilFailOp);
	stencilOpDesc.StencilDepthFailOp = ToD3D12StencilOp(m_Desc.depthStencilState.stencilDepthFailOp);
	stencilOpDesc.StencilPassOp = ToD3D12StencilOp(m_Desc.depthStencilState.stencilPassOp);
	stencilOpDesc.StencilFunc = ToD3D12ComparisonFunc(m_Desc.depthStencilState.stencilCompareOp);
	psoDesc.DepthStencilState.FrontFace = stencilOpDesc;
	psoDesc.DepthStencilState.BackFace = stencilOpDesc;

	psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	psoDesc.BlendState.IndependentBlendEnable = FALSE;
	for (uint32 i = 0; i < m_Desc.colorAttachmentCount; ++i)
	{
		psoDesc.BlendState.RenderTarget[i].BlendEnable = m_Desc.blendState.enable ? TRUE : FALSE;
		psoDesc.BlendState.RenderTarget[i].SrcBlend = ToD3D12Blend(m_Desc.blendState.srcColor);
		psoDesc.BlendState.RenderTarget[i].DestBlend = ToD3D12Blend(m_Desc.blendState.dstColor);
		psoDesc.BlendState.RenderTarget[i].BlendOp = ToD3D12BlendOp(m_Desc.blendState.colorBlendOp);
		psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = ToD3D12Blend(m_Desc.blendState.srcAlpha);
		psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = ToD3D12Blend(m_Desc.blendState.dstAlpha);
		psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = ToD3D12BlendOp(m_Desc.blendState.alphaBlendOp);
		psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = m_Desc.blendState.enableColorWriteMask ? (UINT8)m_Desc.blendState.colorWriteMask : D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
	for (uint32 slot = 0; slot < m_Desc.inputLayouts.size(); ++slot)
	{
		const InputLayoutDesc& layoutDesc = m_Desc.inputLayouts[slot];
		for (uint32 i = 0; i < layoutDesc.elements.size(); ++i)
		{
			D3D12_INPUT_ELEMENT_DESC inputElementDesc = {};
			inputElementDesc.SemanticName = layoutDesc.elements[i].semanticName.c_str();
			inputElementDesc.SemanticIndex = layoutDesc.elements[i].semanticIndex;
			inputElementDesc.Format = ToDXGIFormat(layoutDesc.elements[i].format);
			inputElementDesc.InputSlot = slot;
			inputElementDesc.AlignedByteOffset = layoutDesc.elements[i].offset;
			inputElementDesc.InputSlotClass = ToD3D12InputClassification(layoutDesc.inputRate);
			inputElementDesc.InstanceDataStepRate = layoutDesc.instanceDataStepRate;
			inputElementDescs.push_back(inputElementDesc);
		}
	}
	psoDesc.InputLayout = { inputElementDescs.data(), (UINT)inputElementDescs.size() };
	psoDesc.PrimitiveTopologyType = ToD3D12PrimitiveTopologyType(m_Desc.topology);
	psoDesc.NumRenderTargets = m_Desc.colorAttachmentCount;
	for (uint32 i = 0; i < m_Desc.colorAttachmentCount; ++i)
	{
		psoDesc.RTVFormats[i] = ToDXGIFormat(m_Desc.colorAttachmentFormats[i]);
	}
	psoDesc.DSVFormat = m_Desc.depthStencilAttachmentFormat != Engine::ETextureFormat::UNKNOWN ? ToDXGIFormat(m_Desc.depthStencilAttachmentFormat) : DXGI_FORMAT_UNKNOWN;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.SampleMask = UINT_MAX;

	ComPtr<ID3D12InfoQueue> infoQueue;
	dxDevice->QueryInterface(IID_PPV_ARGS(&infoQueue));
	const UINT64 firstMessage = infoQueue ? infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter() : 0;
	const HRESULT hr = dxDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_Pipeline));
	if (FAILED(hr))
	{
		ENGINE_LOG_ERROR("CreateGraphicsPipelineState failed: HRESULT=0x{:08X}, VS bytes={}, PS bytes={}, RTs={}",
			static_cast<uint32>(hr), psoDesc.VS.BytecodeLength, psoDesc.PS.BytecodeLength, psoDesc.NumRenderTargets);
		if (infoQueue)
		{
			const UINT64 count = infoQueue->GetNumStoredMessagesAllowedByRetrievalFilter();
			for (UINT64 index = firstMessage; index < count; ++index)
			{
				SIZE_T bytes = 0;
				if (FAILED(infoQueue->GetMessage(index, nullptr, &bytes)) || bytes < sizeof(D3D12_MESSAGE)) continue;
				std::vector<std::max_align_t> storage((bytes + sizeof(std::max_align_t) - 1) / sizeof(std::max_align_t));
				auto* message = reinterpret_cast<D3D12_MESSAGE*>(storage.data());
				if (SUCCEEDED(infoQueue->GetMessage(index, message, &bytes)))
					ENGINE_LOG_ERROR("D3D12 PSO validation (id={}): {}", static_cast<uint32>(message->ID),
						message->pDescription ? message->pDescription : "");
			}
		}
		return EResult::Fail;
	}

	return EResult::Success;
}
#pragma endregion