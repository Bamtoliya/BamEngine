#include "DirectX12Pipeline.h"
#include "DirectX12RHI.h"
#include "DirectX12Shader.h"

#pragma region Constructor & Destructor
EResult DirectX12Pipeline::Initialize(const DESC& desc)
{
	DirectX12RHI* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12Device* dxDevice = static_cast<ID3D12Device*>(dxRHI->GetNativeRHI());

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};

	psoDesc.pRootSignature = static_cast<ID3D12RootSignature*>(dxRHI->GetGlobalRootSignature());

	Safe_AddRef(desc.vertexShader);
	Safe_AddRef(desc.pixelShader);

	if (desc.vertexShader)
	{
		DirectX12Shader* vs = static_cast<DirectX12Shader*>(desc.vertexShader);
		psoDesc.VS = { vs->GetBytecode().pShaderBytecode, vs->GetBytecode().BytecodeLength };
	}
	if (desc.pixelShader)
	{
		DirectX12Shader* ps = static_cast<DirectX12Shader*>(desc.pixelShader);
		psoDesc.PS = { ps->GetBytecode().pShaderBytecode, ps->GetBytecode().BytecodeLength };
	}

	psoDesc.RasterizerState.FillMode = ToD3D12FillMode(desc.fillMode);
	psoDesc.RasterizerState.CullMode = ToD3D12CullMode(desc.cullMode);
	psoDesc.RasterizerState.FrontCounterClockwise = ToD3D12FrontFace(desc.frontFace);
	psoDesc.RasterizerState.DepthClipEnable = TRUE;
	psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	psoDesc.DepthStencilState.DepthEnable = desc.depthStencilState.depthTestEnable ? TRUE : FALSE;
	psoDesc.DepthStencilState.DepthWriteMask = desc.depthStencilState.depthWriteEnable ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	psoDesc.DepthStencilState.DepthFunc = ToD3D12ComparisonFunc(desc.depthStencilState.depthCompareOp);
	psoDesc.DepthStencilState.StencilEnable = desc.depthStencilState.stencilTestEnable ? TRUE : FALSE;

	psoDesc.DepthStencilState.StencilReadMask = desc.depthStencilState.stencilReadMask;
	psoDesc.DepthStencilState.StencilWriteMask = desc.depthStencilState.stencilWriteMask;

	D3D12_DEPTH_STENCILOP_DESC stencilOpDesc = {};
	stencilOpDesc.StencilFailOp = ToD3D12StencilOp(desc.depthStencilState.stencilFailOp);
	stencilOpDesc.StencilDepthFailOp = ToD3D12StencilOp(desc.depthStencilState.stencilDepthFailOp);
	stencilOpDesc.StencilPassOp = ToD3D12StencilOp(desc.depthStencilState.stencilPassOp);
	stencilOpDesc.StencilFunc = ToD3D12ComparisonFunc(desc.depthStencilState.stencilCompareOp);
	
	psoDesc.DepthStencilState.FrontFace = stencilOpDesc;
	psoDesc.DepthStencilState.BackFace = stencilOpDesc;

	psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	psoDesc.BlendState.IndependentBlendEnable = FALSE;

	for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
	{
		psoDesc.BlendState.RenderTarget[i].BlendEnable = desc.blendState.enable ? TRUE : FALSE;
		psoDesc.BlendState.RenderTarget[i].SrcBlend = ToD3D12Blend(desc.blendState.srcColor);
		psoDesc.BlendState.RenderTarget[i].DestBlend = ToD3D12Blend(desc.blendState.dstColor);
		psoDesc.BlendState.RenderTarget[i].BlendOp = ToD3D12BlendOp(desc.blendState.colorBlendOp);
		psoDesc.BlendState.RenderTarget[i].SrcBlendAlpha = ToD3D12Blend(desc.blendState.srcAlpha);
		psoDesc.BlendState.RenderTarget[i].DestBlendAlpha = ToD3D12Blend(desc.blendState.dstAlpha);
		psoDesc.BlendState.RenderTarget[i].BlendOpAlpha = ToD3D12BlendOp(desc.blendState.alphaBlendOp);
		psoDesc.BlendState.RenderTarget[i].RenderTargetWriteMask = desc.blendState.enableColorWriteMask ? (UINT8)desc.blendState.colorWriteMask : D3D12_COLOR_WRITE_ENABLE_ALL;
	};

	vector<D3D12_INPUT_ELEMENT_DESC> inputElementDescs;
	for (uint32 slot = 0; slot < desc.inputLayouts.size(); ++slot)
	{
		const InputLayoutDesc& layoutDesc = desc.inputLayouts[slot];
		for (uint32 i = 0; i < layoutDesc.elements.size(); ++i)
		{
			D3D12_INPUT_ELEMENT_DESC inputElementDesc = {};
			inputElementDesc.SemanticName = layoutDesc.elements[i].semanticName.c_str();
			inputElementDesc.SemanticIndex = layoutDesc.elements[i].semanticIndex;
			inputElementDesc.Format = ToDXGIFormat(layoutDesc.elements[i].format);
			inputElementDesc.InputSlot = i;
			inputElementDesc.AlignedByteOffset = layoutDesc.elements[i].offset;
			inputElementDesc.InputSlotClass = ToD3D12InputClassification(layoutDesc.inputRate);
			inputElementDesc.InstanceDataStepRate = layoutDesc.instanceDataStepRate;
			inputElementDescs.push_back(inputElementDesc);
		}
	}

	psoDesc.InputLayout = { inputElementDescs.data(), (UINT)inputElementDescs.size() };

	psoDesc.PrimitiveTopologyType = ToD3D12PrimitiveTopologyType(desc.topology);
	psoDesc.NumRenderTargets = desc.colorAttachmentCount;
	for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
	{
		psoDesc.RTVFormats[i] = ToDXGIFormat(desc.colorAttachmentFormats[i]);
	}
	psoDesc.DSVFormat = desc.depthStencilAttachmentFormat != Engine::ETextureFormat::UNKNOWN ? ToDXGIFormat(desc.depthStencilAttachmentFormat) : DXGI_FORMAT_UNKNOWN;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleDesc.Quality = 0;
	psoDesc.SampleMask = UINT_MAX;

	if (FAILED(dxDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_Pipeline))))
	{
		return EResult::Fail;
	}

	return EResult::Success;
}

DirectX12Pipeline* DirectX12Pipeline::Create(RHI* rhi, const DESC& desc)
{
	DirectX12Pipeline* instance = new DirectX12Pipeline(rhi, desc);
	if (IsFailure(instance->Initialize(desc)))
	{
		delete instance;
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
#pragma endregion