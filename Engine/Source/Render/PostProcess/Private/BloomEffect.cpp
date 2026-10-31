#include "BloomEffect.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "Renderer.h"
#include "RenderTargetManager.h"
#include "RenderPassManager.h"
#include "SamplerManager.h"
#include "RHI.h"

BloomEffect::BloomEffect()
	: IPostProcessEffect(L"Bloom", "POST_PROCESS_BLOOM")
{
	AddFloat("Threshold", 1.0f, 0.0f, 5.0f, "BLOOM_THRESHOLD");
	AddFloat("SoftKnee", 0.5f, 0.0f, 1.0f, "BLOOM_SOFT_KNEE");
	AddFloat("Intensity", 1.0f, 0.0f, 5.0f, "BLOOM_INTENSITY");
	AddFloat("Scatter", 1.0f, 0.1f, 2.0f, "BLOOM_SCATTER");
}

BloomEffect::~BloomEffect()
{
	Free();
}

void BloomEffect::Initialize(const wstring& prefix)
{
	m_Prefix = prefix;
	ResourceManager& rm = ResourceManager::Get();

	Shader* vs = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS").Get();
	if (!vs || !vs->GetRHIShader()) return;

	auto CreatePostPipeline = [&](const wstring& psKey, EBlendMode blendMode) -> RHIPipeline*
		{
			Shader* ps = rm.GetResourceHandle<Shader>(psKey).Get();
			if (!ps || !ps->GetRHIShader()) return nullptr;

			RHIPipelineDesc pd = {};
			pd.pipelineType = EPipelineType::Graphics;
			pd.vertexShader = vs->GetRHIShader();
			pd.pixelShader = ps->GetRHIShader();
			pd.colorAttachmentCount = 1;
			pd.colorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
			pd.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
			pd.depthStencilState.depthTestEnable = false;
			pd.depthStencilState.depthWriteEnable = false;
			pd.topology = ETopology::TriangleList;
			pd.cullMode = ECullMode::None;
			pd.blendState = BlendState(blendMode);

			return PipelineManager::Get().GetOrCreatePipeline(pd);
		};

	m_PrefilterPipeline = CreatePostPipeline(L"PostProcess_BloomPrefilterPS", EBlendMode::None);
	m_DownsamplePipeline = CreatePostPipeline(L"PostProcess_BloomDownsamplePS", EBlendMode::None);
	m_UpsamplePipeline = CreatePostPipeline(L"PostProcess_BloomUpsamplePS", EBlendMode::Additive);
	m_CompositePipeline = CreatePostPipeline(L"PostProcess_BloomCompositePS", EBlendMode::None);

	// 1. 렌더타깃 이름 설정 및 선행 생성 (최초 256x256으로 객체 생성 보장)
	for (uint32 i = 0; i < BLOOM_MIP_LEVELS; ++i)
		m_DownRTNames[i] = m_Prefix + L"_Bloom_Down_" + std::to_wstring(i);
	CreateMipRenderTargets(256, 256);

	// 2. 7개 서브 렌더패스 등록 (우선순위 121 ~ 127)
	auto& rpMgr = RenderPassManager::Get();

	// 121: Prefilter Pass
	m_PrefilterPassID = rpMgr.RegisterRenderPass(
		m_Prefix + L"_Bloom_Prefilter", { m_DownRTNames[0] }, L"",
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.f, 0.f, 0.f, 1.f), 121, ERenderSortType::None, ERenderPassType::PostProcess, EBlendMode::None);

	// 122 ~ 124: Downsample Passes
	for (uint32 i = 0; i < BLOOM_MIP_LEVELS - 1; ++i)
	{
		m_DownPassIDs[i] = rpMgr.RegisterRenderPass(
			m_Prefix + L"_Bloom_Down_" + std::to_wstring(i), { m_DownRTNames[i + 1] }, L"",
			ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			vec4(0.f, 0.f, 0.f, 1.f), 122 + i, ERenderSortType::None, ERenderPassType::PostProcess, EBlendMode::None);
	}

	// 125 ~ 127: Upsample Passes (RPLO_Load로 기존 타깃 밉에 가산 누적)
	for (uint32 i = 0; i < BLOOM_MIP_LEVELS - 1; ++i)
	{
		const uint32 targetMip = (BLOOM_MIP_LEVELS - 2) - i; // 2 -> 1 -> 0
		m_UpPassIDs[i] = rpMgr.RegisterRenderPass(
			m_Prefix + L"_Bloom_Up_" + std::to_wstring(i), { m_DownRTNames[targetMip] }, L"",
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			vec4(0.f, 0.f, 0.f, 1.f), 125 + i, ERenderSortType::None, ERenderPassType::PostProcess, EBlendMode::Additive);
	}

	// 3. 초기 상태에서는 서브 패스들을 모두 비활성화
	SetEnabled(false);
}

void BloomEffect::SetEnabled(bool enabled)
{
	IPostProcessEffect::SetEnabled(enabled);

	auto& rpMgr = RenderPassManager::Get();
	if (auto* p = rpMgr.GetRenderPassByID(m_PrefilterPassID))
		p->SetEnabled(enabled);

	for (uint32 i = 0; i < BLOOM_MIP_LEVELS - 1; ++i)
	{
		if (auto* p = rpMgr.GetRenderPassByID(m_DownPassIDs[i]))
			p->SetEnabled(enabled);
		if (auto* p = rpMgr.GetRenderPassByID(m_UpPassIDs[i]))
			p->SetEnabled(enabled);
	}
}

void BloomEffect::AddFloat(const string& name, f32 defaultVal, f32 minVal, f32 maxVal, const string& localKey)
{
	m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float, &defaultVal, sizeof(f32), 0, minVal, maxVal, localKey);
}
void BloomEffect::SetFloat(const string& name, f32 value)
{
	auto it = m_Parameters.find(name);
	if (it != m_Parameters.end())
	{
		memcpy(it->second.data.data(), &value, sizeof(f32));
		if (name == "Threshold") m_Threshold = value;
		else if (name == "SoftKnee") m_SoftKnee = value;
		else if (name == "Intensity") m_Intensity = value;
		else if (name == "Scatter") m_Scatter = value;
	}
}
void BloomEffect::ResetParameter(const string& name)
{
	auto it = m_Parameters.find(name);
	if (it != m_Parameters.end())
	{
		it->second.data = it->second.defaultValue;
		f32 defVal = *reinterpret_cast<const f32*>(it->second.defaultValue.data());
		SetFloat(name, defVal);
	}
}
void BloomEffect::ResetAllParameters()
{
	for (auto& [name, param] : m_Parameters)
	{
		param.data = param.defaultValue;
		f32 defVal = *reinterpret_cast<const f32*>(param.defaultValue.data());
		SetFloat(name, defVal);
	}
}

void BloomEffect::Free()
{
	m_PrefilterPipeline = nullptr;
	m_DownsamplePipeline = nullptr;
	m_UpsamplePipeline = nullptr;
	m_CompositePipeline = nullptr;
}

void BloomEffect::CreateMipRenderTargets(uint32 baseWidth, uint32 baseHeight)
{
	auto& rtMgr = RenderTargetManager::Get();

	uint32 w = baseWidth;
	uint32 h = baseHeight;

	for (uint32 i = 0; i < BLOOM_MIP_LEVELS; ++i)
	{
		w = std::max(1u, w / 2);
		h = std::max(1u, h / 2);

		m_MipWidths[i] = w;
		m_MipHeights[i] = h;

		RenderTargetDesc desc = {
			{ w, h, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R8G8B8A8_UNORM,
				ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 1.f) },
			m_DownRTNames[i]
		};

		if (!rtMgr.GetRenderTarget(m_DownRTNames[i]))
			rtMgr.CreateRenderTarget(&desc);
		else
			rtMgr.GetRenderTarget(m_DownRTNames[i])->Resize(w, h);
	}
}

EResult BloomEffect::SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT)
{
	if (!m_CompositePipeline) return EResult::Fail;

	auto* src = RenderTargetManager::Get().GetRenderTarget(sourceRT);
	if (!src) return EResult::Fail;

	CreateMipRenderTargets(src->GetWidth(), src->GetHeight());

	// 1. Pass 121: Prefilter (sourceRT -> DownRT[0])
	Renderer::Get().SubmitCustomCommand(
		[this, capturedSource = sourceRT](f32 dt, RenderPass* pass) -> EResult
		{
			auto* srcRT = RenderTargetManager::Get().GetRenderTarget(capturedSource);
			if (!srcRT || !srcRT->GetTexture()) return EResult::Fail;
			auto* rhi = Renderer::Get().GetRHI();
			auto* sampler = SamplerManager::Get().GetDefaultSampler();

			if (IsFailure(rhi->BindTextureSampler(srcRT->GetTexture(), sampler, 16))) return EResult::Fail;

			struct PrefilterBuffer { f32 threshold; f32 softKnee; f32 _pad[2]; } cb = { m_Threshold, m_SoftKnee, {} };
			if (IsFailure(rhi->BindConstantBuffer(&cb, sizeof(cb), 2))) return EResult::Fail;
			if (IsFailure(rhi->BindPipeline(m_PrefilterPipeline))) return EResult::Fail;
			return rhi->Draw(3);
		}, m_PrefilterPassID);

	// 2. Pass 122 ~ 124: Downsample Chain (DownRT[i] -> DownRT[i+1])
	for (uint32 i = 0; i < BLOOM_MIP_LEVELS - 1; ++i)
	{
		Renderer::Get().SubmitCustomCommand(
			[this, readIdx = i](f32 dt, RenderPass* pass) -> EResult
			{
				auto* readRT = RenderTargetManager::Get().GetRenderTarget(m_DownRTNames[readIdx]);
				if (!readRT || !readRT->GetTexture()) return EResult::Fail;
				auto* rhi = Renderer::Get().GetRHI();
				auto* sampler = SamplerManager::Get().GetDefaultSampler();

				if (IsFailure(rhi->BindTextureSampler(readRT->GetTexture(), sampler, 16))) return EResult::Fail;
				if (IsFailure(rhi->BindPipeline(m_DownsamplePipeline))) return EResult::Fail;
				return rhi->Draw(3);
			}, m_DownPassIDs[i]);
	}

	// 3. Pass 125 ~ 127: Upsample Chain (DownRT[read] -> DownRT[target] with Additive Blend)
	for (uint32 i = 0; i < BLOOM_MIP_LEVELS - 1; ++i)
	{
		const uint32 readMip = BLOOM_MIP_LEVELS - 1 - i; // 3 -> 2 -> 1
		Renderer::Get().SubmitCustomCommand(
			[this, readIdx = readMip](f32 dt, RenderPass* pass) -> EResult
			{
				auto* readRT = RenderTargetManager::Get().GetRenderTarget(m_DownRTNames[readIdx]);
				if (!readRT || !readRT->GetTexture()) return EResult::Fail;
				auto* rhi = Renderer::Get().GetRHI();
				auto* sampler = SamplerManager::Get().GetDefaultSampler();

				if (IsFailure(rhi->BindTextureSampler(readRT->GetTexture(), sampler, 16))) return EResult::Fail;

				struct UpsampleBuffer { f32 scatter; f32 _pad[3]; } cb = { m_Scatter, {} };
				if (IsFailure(rhi->BindConstantBuffer(&cb, sizeof(cb), 2))) return EResult::Fail;
				if (IsFailure(rhi->BindPipeline(m_UpsamplePipeline))) return EResult::Fail;
				return rhi->Draw(3);
			}, m_UpPassIDs[i]);
	}

	// 4. Pass 130 (메인 슬롯): Final Composite (sourceRT + DownRT[0] -> writeRT)
	Renderer::Get().SubmitCustomCommand(
		[this, capturedSource = sourceRT](f32 dt, RenderPass* pass) -> EResult
		{
			auto& rtMgr = RenderTargetManager::Get();
			auto* srcRT = rtMgr.GetRenderTarget(capturedSource);
			auto* bloomRT = rtMgr.GetRenderTarget(m_DownRTNames[0]);
			if (!srcRT || !bloomRT || !srcRT->GetTexture() || !bloomRT->GetTexture()) return EResult::Fail;

			auto* rhi = Renderer::Get().GetRHI();
			auto* sampler = SamplerManager::Get().GetDefaultSampler();

			if (IsFailure(rhi->BindTextureSampler(srcRT->GetTexture(), sampler, 16))) return EResult::Fail;
			if (IsFailure(rhi->BindTextureSampler(bloomRT->GetTexture(), sampler, 17))) return EResult::Fail;

			struct BloomCompositeBuffer { f32 intensity; f32 _pad[3]; } cb = { m_Intensity, {} };
			if (IsFailure(rhi->BindConstantBuffer(&cb, sizeof(cb), 2))) return EResult::Fail;
			if (IsFailure(rhi->BindPipeline(m_CompositePipeline))) return EResult::Fail;
			return rhi->Draw(3);
		}, passID);

	return EResult::Success;
}