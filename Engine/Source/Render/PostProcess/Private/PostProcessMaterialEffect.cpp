#include "PostProcessMaterialEffect.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "Renderer.h"
#include "RenderTargetManager.h"
#include "SamplerManager.h"
#include "RHI.h"

void PostProcessMaterialEffect::Initialize(const wstring& prefix)
{
	if (!m_Material) return;

	ResourceManager& rm = ResourceManager::Get();

	RHIPipelineDesc pd = {};
	pd.pipelineType = EPipelineType::Graphics;

	// 머티리얼에 지정된 PostProcess용 VS가 있으면 사용, 없으면 기본 FullscreenQuadVS 사용
	Shader* vs = m_Material->GetVertexShader(ERenderPassType::PostProcess);
	if (!vs) vs = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS").Get();

	Shader* ps = m_Material->GetPixelShader(ERenderPassType::PostProcess);
	if (!vs || !ps || !vs->GetRHIShader() || !ps->GetRHIShader())
		return;

	pd.vertexShader = vs->GetRHIShader();
	pd.pixelShader = ps->GetRHIShader();
	pd.colorAttachmentCount = 1;
	pd.colorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
	pd.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
	pd.depthStencilState.depthTestEnable = false;
	pd.depthStencilState.depthWriteEnable = false;
	pd.topology = ETopology::TriangleList;
	pd.cullMode = m_Material->GetCullMode();
	pd.blendState = m_Material->GetBlendState();

	m_Pipeline = PipelineManager::Get().GetOrCreatePipeline(pd);

	UpdateParameterBuffer();
}

void PostProcessMaterialEffect::Free()
{
	m_Pipeline = nullptr;
}

void PostProcessMaterialEffect::UpdateParameterBuffer()
{
	if (!m_Material) return;

	const auto& params = m_Material->GetParameters();
	if (params.empty())
	{
		m_ParamBuffer.clear();
		return;
	}

	size_t maxEnd = 0;
	for (const auto& [name, param] : params)
	{
		const size_t end = param.offset + param.data.size();
		if (end > maxEnd) maxEnd = end;
	}

	if (maxEnd == 0) return;

	const size_t alignedSize = (maxEnd + 15) & ~15;
	if (m_ParamBuffer.size() != alignedSize)
		m_ParamBuffer.assign(alignedSize, 0);

	for (const auto& [name, param] : params)
	{
		if (param.offset + param.data.size() <= m_ParamBuffer.size())
		{
			memcpy(m_ParamBuffer.data() + param.offset, param.data.data(), param.data.size());
		}
	}
}

EResult PostProcessMaterialEffect::SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT)
{
	if (!m_Pipeline || !m_Material) return EResult::Fail;

	UpdateParameterBuffer();

	Renderer::Get().SubmitCustomCommand([this, capturedRead = sourceRT](f32 dt, RenderPass* pass) -> EResult
		{
			auto* src = RenderTargetManager::Get().GetRenderTarget(capturedRead);
			if (!src || !src->GetTexture()) return EResult::Fail;

			auto* rhi = Renderer::Get().GetRHI();
			SamplerDesc clampDesc{};
			clampDesc.AddressU = ESamplerAddressMode::Clamp;
			clampDesc.AddressV = ESamplerAddressMode::Clamp;
			clampDesc.AddressW = ESamplerAddressMode::Clamp;
			auto* sampler = SamplerManager::Get().GetOrCreateSampler(clampDesc);

			// 1. 이전 패스 출력 화면 텍스처 바인딩 (space3, t0 / slot 16)
			if (IsFailure(rhi->BindTextureSampler(src->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS)))
				return EResult::Fail;

			// 2. 머티리얼이 보유한 추가 텍스처가 있을 때만 바인딩 (space2, t0~t15)
			if (!m_Material->GetTextureBindings().empty())
			{
				if (IsFailure(m_Material->Bind(2)))
					return EResult::Fail;
			}

			// 3. 머티리얼 파라미터 상수 버퍼 바인딩 (space3, b0 / slot 2)
			if (!m_ParamBuffer.empty())
			{
				if (IsFailure(rhi->BindConstantBuffer(m_ParamBuffer.data(), static_cast<uint32>(m_ParamBuffer.size()), 2)))
					return EResult::Fail;
			}

			// 4. 파이프라인 바인딩 및 풀스크린 Quad 드로우
			if (IsFailure(rhi->BindPipeline(m_Pipeline)))
				return EResult::Fail;

			return rhi->Draw(3);
		}, passID);

	return EResult::Success;
}