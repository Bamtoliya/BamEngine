#include "DirectX12Pipeline.h"
#include "DirectX12RHI.h"
#include "DirectX12Shader.h"
#include "Logger.h"
#include <directx/d3d12sdklayers.h>

#if defined(_MSC_VER)
#include <excpt.h>

// MSVC C2712 방지를 위해 C++ 언와인딩 객체가 없는 독립 헬퍼로 SEH 격리
static HRESULT SafeCreateGraphicsPipelineState(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, ID3D12PipelineState** outPipeline)
{
	__try
	{
		return device->CreateGraphicsPipelineState(desc, IID_PPV_ARGS(outPipeline));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return E_FAIL;
	}
}

static HRESULT SafeLoadGraphicsPipeline(ID3D12PipelineLibrary* library, LPCWSTR name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, ID3D12PipelineState** outPipeline)
{
	__try
	{
		return library->LoadGraphicsPipeline(name, desc, IID_PPV_ARGS(outPipeline));
	}
	__except (EXCEPTION_EXECUTE_HANDLER)
	{
		return E_FAIL;
	}
}
#else
static HRESULT SafeCreateGraphicsPipelineState(ID3D12Device* device, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, ID3D12PipelineState** outPipeline)
{
	return device->CreateGraphicsPipelineState(desc, IID_PPV_ARGS(outPipeline));
}

static HRESULT SafeLoadGraphicsPipeline(ID3D12PipelineLibrary* library, LPCWSTR name, const D3D12_GRAPHICS_PIPELINE_STATE_DESC* desc, ID3D12PipelineState** outPipeline)
{
	return library->LoadGraphicsPipeline(name, desc, IID_PPV_ARGS(outPipeline));
}
#endif

namespace
{
	std::wstring GeneratePipelineKey(const RHIPipelineDesc& desc)
	{
		uint64_t hash = 14695981039346656037ull;
		auto hashCombine = [&hash](const void* data, size_t size)
			{
				const auto* bytes = static_cast<const uint8_t*>(data);
				for (size_t i = 0; i < size; ++i)
				{
					hash ^= bytes[i];
					hash *= 1099511628211ull;
				}
			};

		// 루트 시그니처가 바뀌면 같은 셰이더라도 다른 PSO이므로 키를 분리한다.
		const uint32 rootSignatureVersion = DirectX12RHI::GLOBAL_ROOT_SIGNATURE_LAYOUT_VERSION;
		hashCombine(&rootSignatureVersion, sizeof(rootSignatureVersion));

		if (desc.pipelineType == EPipelineType::Compute)
		{
			if (desc.computeShader)
			{
				const auto& code = static_cast<DirectX12Shader*>(desc.computeShader)->GetBytecode();
				hashCombine(code.pShaderBytecode, code.BytecodeLength);
			}
		}
		else
		{
			if (desc.vertexShader)
			{
				const auto& code = static_cast<DirectX12Shader*>(desc.vertexShader)->GetBytecode();
				hashCombine(code.pShaderBytecode, code.BytecodeLength);
			}
			if (desc.pixelShader)
			{
				const auto& code = static_cast<DirectX12Shader*>(desc.pixelShader)->GetBytecode();
				hashCombine(code.pShaderBytecode, code.BytecodeLength);
			}
			hashCombine(&desc.fillMode, sizeof(desc.fillMode));
			hashCombine(&desc.cullMode, sizeof(desc.cullMode));
			hashCombine(&desc.frontFace, sizeof(desc.frontFace));
			hashCombine(&desc.depthStencilState, sizeof(desc.depthStencilState));
			hashCombine(&desc.blendState, sizeof(desc.blendState));
			hashCombine(&desc.topology, sizeof(desc.topology));
			hashCombine(&desc.colorAttachmentCount, sizeof(desc.colorAttachmentCount));
			for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
				hashCombine(&desc.colorAttachmentFormats[i], sizeof(desc.colorAttachmentFormats[i]));
			hashCombine(&desc.depthStencilAttachmentFormat, sizeof(desc.depthStencilAttachmentFormat));
		}

		wchar_t buf[32];
		std::swprintf(buf, sizeof(buf) / sizeof(wchar_t), L"pso_%016llx", static_cast<unsigned long long>(hash));
		return buf;
	}
}

#pragma region Constructor & Destructor
EResult DirectX12Pipeline::Initialize(const DESC& desc)
{
	Safe_AddRef(desc.vertexShader);
	Safe_AddRef(desc.pixelShader);
	Safe_AddRef(desc.computeShader);
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
	if (m_RHI)
	{
		auto* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
		dxRHI->DeferRelease(m_Pipeline.Get());
		dxRHI->InvalidatePipelineBinding(this);
	}
	m_Pipeline.Reset();
	Safe_Release(m_Desc.vertexShader);
	Safe_Release(m_Desc.pixelShader);
	Safe_Release(m_Desc.computeShader); // 추가
	__super::Free();
}

EResult DirectX12Pipeline::Rebuild()
{
	return CreatePipelineState();
}
void DirectX12Pipeline::SetNativeHandle(void* nativeHandle)
{
	auto* pipelineState = static_cast<ID3D12PipelineState*>(nativeHandle);
	if (m_Pipeline.Get() == pipelineState) return;

	auto* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
	dxRHI->DeferRelease(m_Pipeline.Get());
	m_Pipeline = pipelineState;
	dxRHI->InvalidatePipelineBinding(this);
}
#pragma endregion

#pragma region Pipeline State Creation
EResult DirectX12Pipeline::CreatePipelineState()
{
	DirectX12RHI* dxRHI = static_cast<DirectX12RHI*>(m_RHI);
	ID3D12Device* dxDevice = static_cast<ID3D12Device*>(dxRHI->GetNativeRHI());

	ID3D12PipelineLibrary* pLibrary = dxRHI->GetPipelineLibrary();
	const std::wstring psoName = GeneratePipelineKey(m_Desc);

	// ── Compute Pipeline 생성 ──
	if (m_Desc.pipelineType == EPipelineType::Compute)
	{
		if (!m_Desc.computeShader)
		{
			ENGINE_LOG_ERROR("Compute pipeline requires a valid computeShader.");
			return EResult::InvalidArgument;
		}
		DirectX12Shader* cs = static_cast<DirectX12Shader*>(m_Desc.computeShader);
		D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
		computePsoDesc.pRootSignature = static_cast<ID3D12RootSignature*>(dxRHI->GetGlobalRootSignature());
		computePsoDesc.CS = { cs->GetBytecode().pShaderBytecode, cs->GetBytecode().BytecodeLength };
		ComPtr<ID3D12PipelineState> newPipeline;
		HRESULT hr = E_FAIL;
		// 1. 라이브러리에 저장된 적이 있는 이름일 때만 안전하게 Load 시도 (크래시 원천 방지)
		if (pLibrary && dxRHI->ContainsPipeline(psoName))
			hr = pLibrary->LoadComputePipeline(psoName.c_str(), &computePsoDesc, IID_PPV_ARGS(&newPipeline));
		// 2. 캐시 미적중 시 드라이버 컴파일 및 라이브러리 저장
		if (FAILED(hr))
		{
			hr = dxDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&newPipeline));
			if (FAILED(hr))
			{
				ENGINE_LOG_ERROR("CreateComputePipelineState failed: HRESULT=0x{:08X}", static_cast<uint32>(hr));
				return EResult::Fail;
			}
			if (pLibrary && !dxRHI->ContainsPipeline(psoName))
			{
				const HRESULT storeHr = pLibrary->StorePipeline(psoName.c_str(), newPipeline.Get());
				if (SUCCEEDED(storeHr)) dxRHI->RegisterPipelineName(psoName);
				else ENGINE_LOG_WARN("StorePipeline failed for Compute PSO: HRESULT=0x{:08X}", static_cast<uint32>(storeHr));
			}
		}
		dxRHI->DeferRelease(m_Pipeline.Get());
		m_Pipeline.Swap(newPipeline);
		dxRHI->InvalidatePipelineBinding(this);
		return EResult::Success;
	}

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
	ComPtr<ID3D12PipelineState> newPipeline;
	HRESULT hr = E_FAIL;
	// 1. 라이브러리에 저장된 적이 있는 이름일 때만 안전하게 Load 시도 (드라이버 제거 방지)
	if (pLibrary && dxRHI->ContainsPipeline(psoName))
		hr = SafeLoadGraphicsPipeline(pLibrary, psoName.c_str(), &psoDesc, newPipeline.GetAddressOf());
	// 2. 캐시 미적중 시 드라이버 컴파일 및 라이브러리 등록 (SEH 격리로 D3D12 SDKLayers 크래시 원천 차단)
	if (FAILED(hr))
	{
		hr = SafeCreateGraphicsPipelineState(dxDevice, &psoDesc, newPipeline.GetAddressOf());
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
		if (pLibrary && !dxRHI->ContainsPipeline(psoName))
		{
			const HRESULT storeHr = pLibrary->StorePipeline(psoName.c_str(), newPipeline.Get());
			if (SUCCEEDED(storeHr)) dxRHI->RegisterPipelineName(psoName);
			else ENGINE_LOG_WARN("StorePipeline failed for Graphics PSO: HRESULT=0x{:08X}", static_cast<uint32>(storeHr));
		}
	}
	dxRHI->DeferRelease(m_Pipeline.Get());
	m_Pipeline.Swap(newPipeline);
	dxRHI->InvalidatePipelineBinding(this);
	return EResult::Success;
}
#pragma endregion