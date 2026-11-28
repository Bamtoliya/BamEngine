#include "AutoExposureEffect.h"
#include "AutoExposure.h"
#include "PipelineManager.h"
#include "PlatformPath.h"
#include "PostProcessPipeline.h"
#include "RHI.h"
#include "RHIBuffer.h"
#include "RenderTargetManager.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SamplerManager.h"
#include "Shader.h"

namespace
{
	constexpr uint32 ModeClear = 0;
	constexpr uint32 ModeReset = 1;
	constexpr uint32 ModeAdapt = 2;

	// autoExposure.hlsli의 AutoExposureParams (b0, space3, RHI CBV slot 2)
	struct AutoExposureParams
	{
		uint32 SourceWidth = 0;
		uint32 SourceHeight = 0;
		f32 MinLogLuminance = AutoExposure::MinLogLuminance;
		f32 LogLuminanceRange = AutoExposure::MaxLogLuminance - AutoExposure::MinLogLuminance;

		f32 LowPercent = AutoExposure::LowPercent;
		f32 HighPercent = AutoExposure::HighPercent;
		f32 MiddleGray = AutoExposure::MiddleGray;
		f32 MinEV = 0.0f;

		f32 MaxEV = 0.0f;
		f32 CompensationEV = 0.0f;
		f32 DeltaTime = 0.0f;
		f32 SpeedToLight = 0.0f;

		f32 SpeedToDark = 0.0f;
		uint32 Mode = ModeClear;
		f32 _pad[2] = {};
	};
	static_assert(sizeof(AutoExposureParams) == 64, "AutoExposureParams must match autoExposure.hlsli");

	constexpr uint32 HistogramGroupSize = 16; // autoExposureHistogramCS의 numthreads

	// 시작 매니페스트에 없을 수 있어 처음 쓸 때 DXIL을 직접 등록한다(Client는 프로필 셰이더 목록으로 미리 등록한다).
	Shader* LoadShader(const wchar_t* key, const wchar_t* file, EShaderType type)
	{
		auto& resources = ResourceManager::Get();
		auto handle = resources.GetResourceHandle<Shader>(key);
		if (!handle.Get())
		{
			ShaderDesc desc{};
			desc.Key = key;
			desc.dxilPath = (PlatformPath::GetEngineResourceRoot() / "Shader" / "bin" / "dxil" / file).wstring();
			desc.Path = desc.dxilPath;
			desc.shaderType = type;
			handle = resources.LoadResource<Shader>(&desc);
		}
		Shader* shader = handle.Get();
		if (!shader || !shader->GetRHIShader())
		{
			ENGINE_LOG_ERROR("Auto exposure shader is not available: {}", Engine::Unicode::WideToUtf8(file));
			return nullptr;
		}
		return shader;
	}

	RHIPipeline* CreateComputePipeline(const wchar_t* key, const wchar_t* file)
	{
		Shader* shader = LoadShader(key, file, EShaderType::Compute);
		if (!shader)
			return nullptr;
		RHIPipelineDesc desc{};
		desc.pipelineType = EPipelineType::Compute;
		desc.computeShader = shader->GetRHIShader();
		return PipelineManager::Get().GetOrCreatePipeline(desc);
	}
}

AutoExposureEffect::AutoExposureEffect()
	: IPostProcessEffect(L"Auto Exposure", "POST_PROCESS_AUTO_EXPOSURE")
{
	AddFloat("MinEV", -4.0f, -10.0f, 10.0f, "AUTO_EXPOSURE_MIN_EV");
	AddFloat("MaxEV", 4.0f, -10.0f, 10.0f, "AUTO_EXPOSURE_MAX_EV");
	AddFloat("Compensation", 0.0f, -5.0f, 5.0f, "AUTO_EXPOSURE_COMPENSATION");
	AddFloat("SpeedToLight", 3.0f, 0.1f, 10.0f, "AUTO_EXPOSURE_SPEED_TO_LIGHT");
	AddFloat("SpeedToDark", 1.0f, 0.1f, 10.0f, "AUTO_EXPOSURE_SPEED_TO_DARK");
}

AutoExposureEffect::~AutoExposureEffect()
{
	Free();
}

void AutoExposureEffect::Initialize(const wstring& prefix)
{
	m_Prefix = prefix;
	m_ExposureRTName = prefix + L"_AutoExposure";
	m_ResetPending = true;
}

void AutoExposureEffect::Free()
{
	Safe_Release(m_HistogramBuffer);
	if (!m_ExposureRTName.empty() && RenderTargetManager::IsCreated())
		RenderTargetManager::Get().RemoveRenderTarget(m_ExposureRTName);
	m_HistogramPipeline = nullptr;
	m_AdaptPipeline = nullptr;
	m_ApplyPipeline = nullptr;
	m_ResourcesUnavailable = false;
	m_ResetPending = true;
}

void AutoExposureEffect::SetEnabled(bool enabled)
{
	// 꺼져 있던 동안의 노출은 현재 장면과 무관하므로 다시 켜면 처음부터 잰다.
	if (enabled && !IsEnabled())
		m_ResetPending = true;
	IPostProcessEffect::SetEnabled(enabled);
}

void AutoExposureEffect::AddFloat(const string& name, f32 defaultVal, f32 minVal, f32 maxVal, const string& localKey)
{
	m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float, &defaultVal, sizeof(f32), 0, minVal, maxVal, localKey);
}

void AutoExposureEffect::SetFloat(const string& name, f32 value)
{
	auto it = m_Parameters.find(name);
	if (it == m_Parameters.end())
		return;
	memcpy(it->second.data.data(), &value, sizeof(f32));
	if (name == "MinEV") m_MinEV = value;
	else if (name == "MaxEV") m_MaxEV = value;
	else if (name == "Compensation") m_CompensationEV = value;
	else if (name == "SpeedToLight") m_SpeedToLight = value;
	else if (name == "SpeedToDark") m_SpeedToDark = value;
}

void AutoExposureEffect::ResetParameter(const string& name)
{
	auto it = m_Parameters.find(name);
	if (it == m_Parameters.end())
		return;
	SetFloat(name, *reinterpret_cast<const f32*>(it->second.defaultValue.data()));
}

void AutoExposureEffect::ResetAllParameters()
{
	for (const auto& [name, param] : m_Parameters)
		SetFloat(name, *reinterpret_cast<const f32*>(param.defaultValue.data()));
}

bool AutoExposureEffect::EnsureResources()
{
	if (m_ResourcesUnavailable)
		return false;
	if (m_HistogramPipeline && m_AdaptPipeline && m_ApplyPipeline && m_HistogramBuffer &&
		RenderTargetManager::Get().GetRenderTarget(m_ExposureRTName))
		return true;

	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi || Renderer::Get().GetRHIType() != ERHIType::DirectX12)
	{
		ENGINE_LOG_WARN("Auto exposure requires DirectX12 compute storage bindings; the effect is skipped.");
		m_ResourcesUnavailable = true;
		return false;
	}

	if (!m_HistogramPipeline)
		m_HistogramPipeline = CreateComputePipeline(L"PostProcess_AutoExposureHistogramCS", L"autoExposureHistogramCS.cso");
	if (!m_AdaptPipeline)
		m_AdaptPipeline = CreateComputePipeline(L"PostProcess_AutoExposureAdaptCS", L"autoExposureAdaptCS.cso");
	if (!m_ApplyPipeline)
	{
		Shader* vs = ResourceManager::Get().GetResourceHandle<Shader>(L"FullscreenQuadVS").Get();
		Shader* ps = LoadShader(L"PostProcess_AutoExposurePS", L"autoExposurePS.cso", EShaderType::Fragment);
		if (vs && vs->GetRHIShader() && ps)
		{
			RHIPipelineDesc desc{};
			desc.pipelineType = EPipelineType::Graphics;
			desc.vertexShader = vs->GetRHIShader();
			desc.pixelShader = ps->GetRHIShader();
			desc.colorAttachmentCount = 1;
			desc.colorAttachmentFormats[0] = PostProcessPipeline::HdrFormat;
			desc.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
			desc.depthStencilState.depthTestEnable = false;
			desc.depthStencilState.depthWriteEnable = false;
			desc.topology = ETopology::TriangleList;
			desc.cullMode = ECullMode::None;
			desc.blendState = BlendState(EBlendMode::None);
			m_ApplyPipeline = PipelineManager::Get().GetOrCreatePipeline(desc);
		}
	}
	if (!m_HistogramPipeline || !m_AdaptPipeline || !m_ApplyPipeline)
	{
		m_ResourcesUnavailable = true;
		return false;
	}

	if (!m_HistogramBuffer)
	{
		// 내용은 정해지지 않은 채 만들어지므로 첫 프레임에 비운다.
		m_HistogramBuffer = rhi->CreateBuffer(nullptr, AutoExposure::HistogramBinCount * sizeof(uint32), sizeof(uint32), ERHIBufferType::Storage);
		m_ResetPending = true;
	}

	auto& targets = RenderTargetManager::Get();
	if (!targets.GetRenderTarget(m_ExposureRTName))
	{
		RenderTargetDesc desc{};
		desc.name = m_ExposureRTName;
		desc.fixedSize = true;
		desc.textureDesc.width = 1;
		desc.textureDesc.height = 1;
		desc.textureDesc.format = ETextureFormat::R32_FLOAT;
		desc.textureDesc.dimension = ETextureDimension::Texture2D;
		desc.textureDesc.usage = ETextureUsage::Sampler | ETextureUsage::ComputeReadWrite;
		desc.textureDesc.clearColor = vec4(0.0f);
		targets.CreateRenderTarget(&desc);
		m_ResetPending = true;
	}

	if (!m_HistogramBuffer || !targets.GetRenderTarget(m_ExposureRTName))
	{
		ENGINE_LOG_ERROR("Failed to create auto exposure resources.");
		m_ResourcesUnavailable = true;
		return false;
	}
	return true;
}

EResult AutoExposureEffect::SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT)
{
	if (!EnsureResources())
		return EResult::Fail;

	auto* source = RenderTargetManager::Get().GetRenderTarget(sourceRT);
	if (!source || !source->GetTexture())
		return EResult::Fail;

	AutoExposureParams params{};
	params.SourceWidth = source->GetWidth();
	params.SourceHeight = source->GetHeight();
	params.MinEV = m_MinEV;
	params.MaxEV = m_MaxEV;
	params.CompensationEV = m_CompensationEV;
	params.DeltaTime = dt;
	params.SpeedToLight = m_SpeedToLight;
	params.SpeedToDark = m_SpeedToDark;
	params.Mode = m_ResetPending ? ModeReset : ModeAdapt;
	m_ResetPending = false;

	SamplerDesc clampDesc{};
	clampDesc.AddressU = ESamplerAddressMode::Clamp;
	clampDesc.AddressV = ESamplerAddressMode::Clamp;
	clampDesc.AddressW = ESamplerAddressMode::Clamp;
	RHISampler* clampSampler = SamplerManager::Get().GetOrCreateSampler(clampDesc);

	// 측정·적응(Compute)과 노출 적용(그래픽)을 이 효과의 패스 안에서 차례로 기록한다.
	Renderer::Get().SubmitCustomCommand(
		[this, capturedSource = sourceRT, params, clampSampler](f32, RenderPass*) -> EResult
		{
			auto& targets = RenderTargetManager::Get();
			auto* src = targets.GetRenderTarget(capturedSource);
			auto* exposure = targets.GetRenderTarget(m_ExposureRTName);
			if (!src || !src->GetTexture() || !exposure || !exposure->GetTexture() || !m_HistogramBuffer)
				return EResult::Fail;
			RHI* rhi = Renderer::Get().GetRHI();

			const auto adapt = [&](uint32 mode) -> bool
				{
					AutoExposureParams adaptParams = params;
					adaptParams.Mode = mode;
					return IsSuccess(rhi->BindPipeline(m_AdaptPipeline)) &&
						IsSuccess(rhi->BindConstantBuffer(&adaptParams, sizeof(adaptParams), 2)) &&
						IsSuccess(rhi->BindComputeStorageBuffer(m_HistogramBuffer, 0)) &&
						IsSuccess(rhi->BindComputeStorageTexture(exposure->GetTexture(), 0)) &&
						IsSuccess(rhi->Dispatch(1, 1, 1)) &&
						IsSuccess(rhi->ResourceBarrierUAV(m_HistogramBuffer));
				};

			rhi->BeginPassMetrics(m_Prefix + L"_AutoExposureMeasure");
			// 1. 새로 만들었거나 초기화를 요청받았으면 히스토그램을 비운다.
			bool succeeded = params.Mode != ModeReset || adapt(ModeClear);
			// 2. 밝기 히스토그램
			succeeded = succeeded &&
				IsSuccess(rhi->BindPipeline(m_HistogramPipeline)) &&
				IsSuccess(rhi->BindConstantBuffer(&params, sizeof(params), 2)) &&
				IsSuccess(rhi->BindTexture(src->GetTexture(), MAX_MATERIAL_TEXTURE_SLOTS)) &&
				IsSuccess(rhi->BindComputeStorageBuffer(m_HistogramBuffer, 0)) &&
				IsSuccess(rhi->Dispatch((params.SourceWidth + HistogramGroupSize - 1) / HistogramGroupSize,
					(params.SourceHeight + HistogramGroupSize - 1) / HistogramGroupSize, 1)) &&
				IsSuccess(rhi->ResourceBarrierUAV(m_HistogramBuffer));
			// 3. 평균 밝기로 목표 노출을 구하고 이전 노출에서 옮긴다. 히스토그램은 여기서 비운다.
			succeeded = succeeded && adapt(params.Mode);
			rhi->EndPassMetrics();
			if (!succeeded)
				return EResult::Fail;

			// 4. 장면 색에 노출을 곱한다. 노출 텍스처는 바인딩할 때 읽기 상태로 바뀐다.
			if (IsFailure(rhi->BindTextureSampler(src->GetTexture(), clampSampler, MAX_MATERIAL_TEXTURE_SLOTS)) ||
				IsFailure(rhi->BindTextureSampler(exposure->GetTexture(), clampSampler, MAX_MATERIAL_TEXTURE_SLOTS + 1)) ||
				IsFailure(rhi->BindPipeline(m_ApplyPipeline)))
				return EResult::Fail;
			return rhi->Draw(3);
		}, passID);

	return EResult::Success;
}
