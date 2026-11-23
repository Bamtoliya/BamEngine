#include "DepthOfFieldEffect.h"
#include "PostProcessPipeline.h"
#include "PostProcessOrder.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "Renderer.h"
#include "RenderTargetManager.h"
#include "RenderPassManager.h"
#include "SamplerManager.h"
#include "RHI.h"
#include "Shader.h"
#include "PlatformPath.h"

namespace
{
	// depthOfField.hlsli의 DepthOfFieldParams (b0, space3)
	struct DepthOfFieldShaderParams
	{
		vec3 CameraPosition{ 0.0f };
		f32 FocusDistance = 10.0f;
		vec3 CameraForward{ 0.0f, 0.0f, 1.0f };
		f32 HalfRange = 2.0f;
		f32 InvTransition = 0.1f;
		f32 LensScale = 0.0f;
		f32 MaxRadius = 0.0f;
		uint32 Mode = 0u;
		vec2 SourceSize{ 1.0f };
		vec2 InvSourceSize{ 1.0f };
		vec2 HalfSize{ 1.0f };
		vec2 InvHalfSize{ 1.0f };
		uint32 NearPass = 0u;
		uint32 DilateTiles = 0u;
		vec2 Padding{ 0.0f };
	};
	static_assert(sizeof(DepthOfFieldShaderParams) == 96);
	static_assert(offsetof(DepthOfFieldShaderParams, InvTransition) == 32);
	static_assert(offsetof(DepthOfFieldShaderParams, SourceSize) == 48);
	static_assert(offsetof(DepthOfFieldShaderParams, NearPass) == 80);

	// 절반 해상도 색(rgb)과 CoC(a). 흐림 결과도 같은 형식이다.
	constexpr ETextureFormat HalfFormat = ETextureFormat::R16G16B16A16_FLOAT;
	constexpr ETextureFormat TileFormat = ETextureFormat::R16_FLOAT;

	// Client는 프로필 셰이더 목록으로, Editor는 시작 매니페스트로 셰이더를 등록한다.
	// 매니페스트에 없으면 처음 사용할 때 DXIL을 직접 등록한다(DeferredSceneRenderer::GetOwnedShader와 같은 방식).
	Shader* FindOrLoadShader(const wchar_t* key, const wchar_t* file)
	{
		auto& resources = ResourceManager::Get();
		auto handle = resources.GetResourceHandle<Shader>(key);
		if (!handle.Get())
		{
			ShaderDesc desc{};
			desc.Key = key;
			desc.dxilPath = (PlatformPath::GetEngineResourceRoot() / "Shader" / "bin" / "dxil" / file).wstring();
			desc.Path = desc.dxilPath;
			desc.shaderType = EShaderType::Fragment;
			handle = resources.LoadResource<Shader>(&desc);
		}
		Shader* shader = handle.Get();
		if (!shader || !shader->GetRHIShader())
		{
			ENGINE_LOG_ERROR("Shader is not available: {}", WStrToStr(file));
			return nullptr;
		}
		return shader;
	}

	uint32 HalfExtent(uint32 extent) { return std::max(1u, (extent + 1u) / 2u); }
	uint32 TileExtent(uint32 extent) { return std::max(1u, (extent + DepthOfFieldTileSize - 1u) / DepthOfFieldTileSize); }
}

DepthOfFieldEffect::DepthOfFieldEffect()
	: IPostProcessEffect(L"Depth Of Field", "POST_PROCESS_DEPTH_OF_FIELD")
{
	const DepthOfFieldSettings defaults;
	const int32 mode = static_cast<int32>(defaults.Mode);
	m_Parameters["Mode"] = MaterialParameter(EMaterialParameterType::Int, &mode, sizeof(int32), 0, 0.0f, 1.0f, "DEPTH_OF_FIELD_MODE");
	AddFloat("FocusDistance", defaults.FocusDistance, 0.1f, 200.0f, "DEPTH_OF_FIELD_FOCUS_DISTANCE");
	AddFloat("FocusRange", defaults.FocusRange, 0.0f, 50.0f, "DEPTH_OF_FIELD_FOCUS_RANGE");
	AddFloat("TransitionRange", defaults.TransitionRange, 0.1f, 100.0f, "DEPTH_OF_FIELD_TRANSITION_RANGE");
	AddFloat("FStop", defaults.FStop, 0.7f, 22.0f, "DEPTH_OF_FIELD_F_STOP");
	AddFloat("SensorHeight", defaults.SensorHeight, 4.0f, 60.0f, "DEPTH_OF_FIELD_SENSOR_HEIGHT");
	AddFloat("MaxBlurRadius", defaults.MaxBlurRadius, 0.0f, DepthOfFieldMaxBlurRadiusLimit, "DEPTH_OF_FIELD_MAX_BLUR_RADIUS");
	SyncSettings();
}

DepthOfFieldEffect::~DepthOfFieldEffect()
{
	Free();
}

void DepthOfFieldEffect::Initialize(const wstring& prefix)
{
	m_Prefix = prefix;
	ResourceManager& rm = ResourceManager::Get();

	Shader* vs = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS").Get();
	if (!vs || !vs->GetRHIShader()) return;

	const auto createPipeline = [&](const wchar_t* psKey, const wchar_t* psFile, ETextureFormat format) -> RHIPipeline*
		{
			Shader* ps = FindOrLoadShader(psKey, psFile);
			if (!ps) return nullptr;

			RHIPipelineDesc pd = {};
			pd.pipelineType = EPipelineType::Graphics;
			pd.vertexShader = vs->GetRHIShader();
			pd.pixelShader = ps->GetRHIShader();
			pd.colorAttachmentCount = 1;
			pd.colorAttachmentFormats[0] = format;
			pd.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
			pd.depthStencilState.depthTestEnable = false;
			pd.depthStencilState.depthWriteEnable = false;
			pd.topology = ETopology::TriangleList;
			pd.cullMode = ECullMode::None;
			pd.blendState = BlendState(EBlendMode::None);
			return PipelineManager::Get().GetOrCreatePipeline(pd);
		};

	m_PrefilterPipeline = createPipeline(L"PostProcess_DofPrefilterPS", L"dofPrefilterPS.cso", HalfFormat);
	m_TilePipeline = createPipeline(L"PostProcess_DofNearTilePS", L"dofNearTilePS.cso", TileFormat);
	m_DilatePipeline = createPipeline(L"PostProcess_DofNearDilatePS", L"dofNearDilatePS.cso", TileFormat);
	m_BlurPipeline = createPipeline(L"PostProcess_DofBlurPS", L"dofBlurPS.cso", HalfFormat);
	m_CompositePipeline = createPipeline(L"PostProcess_DofCompositePS", L"dofCompositePS.cso", PostProcessPipeline::HdrFormat);

	m_HalfRTName = m_Prefix + L"_DoF_Half";
	m_TileRTName = m_Prefix + L"_DoF_NearTile";
	m_DilatedTileRTName = m_Prefix + L"_DoF_NearTileDilated";
	m_FarRTName = m_Prefix + L"_DoF_Far";
	m_NearRTName = m_Prefix + L"_DoF_Near";
	EnsureTargets(256, 256);

	// 실행 순서대로 등록한다. 같은 우선순위에서는 먼저 등록한 패스가 먼저 실행되고,
	// 우선순위는 PostProcessPipeline이 체인 위치에 맞춰 바꾼다.
	const std::pair<const wchar_t*, const wstring*> passes[PrePassCount] = {
		{ L"_DoF_Prefilter", &m_HalfRTName },
		{ L"_DoF_NearTile", &m_TileRTName },
		{ L"_DoF_NearDilate", &m_DilatedTileRTName },
		{ L"_DoF_FarBlur", &m_FarRTName },
		{ L"_DoF_NearBlur", &m_NearRTName }
	};
	auto& rpMgr = RenderPassManager::Get();
	for (uint32 i = 0; i < PrePassCount; ++i)
	{
		m_PrePassIDs[i] = rpMgr.RegisterRenderPass(
			m_Prefix + passes[i].first, { *passes[i].second }, L"",
			ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			vec4(0.f, 0.f, 0.f, 0.f), PostProcessPrePassPriority(0), ERenderSortType::None, ERenderPassType::PostProcess, EBlendMode::None);
	}

	SetEnabled(false);
}

void DepthOfFieldEffect::Free()
{
	if (RenderPassManager::IsCreated())
	{
		auto& passes = RenderPassManager::Get();
		for (const auto id : m_PrePassIDs)
			passes.RemoveRenderPass(id);
	}
	m_PrePassIDs.fill(INVALID_PASS_ID);

	for (wstring* name : { &m_HalfRTName, &m_TileRTName, &m_DilatedTileRTName, &m_FarRTName, &m_NearRTName })
	{
		if (!name->empty() && RenderTargetManager::IsCreated())
			RenderTargetManager::Get().RemoveRenderTarget(*name);
		name->clear();
	}

	m_PrefilterPipeline = nullptr;
	m_TilePipeline = nullptr;
	m_DilatePipeline = nullptr;
	m_BlurPipeline = nullptr;
	m_CompositePipeline = nullptr;
	m_HasSceneInputs = false;
}

void DepthOfFieldEffect::SetEnabled(bool enabled)
{
	IPostProcessEffect::SetEnabled(enabled);
	SetPrePassesEnabled(enabled);
}

void DepthOfFieldEffect::SetPrePassesEnabled(bool enabled)
{
	if (!RenderPassManager::IsCreated())
		return;
	auto& rpMgr = RenderPassManager::Get();
	for (const auto id : m_PrePassIDs)
	{
		if (auto* pass = rpMgr.GetRenderPassByID(id))
			pass->SetEnabled(enabled);
	}
}

void DepthOfFieldEffect::GetPrePasses(vector<RenderPassID>& outPasses) const
{
	outPasses.insert(outPasses.end(), m_PrePassIDs.begin(), m_PrePassIDs.end());
}

void DepthOfFieldEffect::SetSceneInputs(const PostProcessSceneInputs* inputs)
{
	m_HasSceneInputs = inputs && inputs->IsValid();
	if (m_HasSceneInputs)
		m_SceneInputs = *inputs;
}

void DepthOfFieldEffect::AddFloat(const string& name, f32 defaultValue, f32 minValue, f32 maxValue, const string& localKey)
{
	m_Parameters[name] = MaterialParameter(EMaterialParameterType::Float, &defaultValue, sizeof(f32), 0, minValue, maxValue, localKey);
}

void DepthOfFieldEffect::SyncSettings()
{
	const auto readFloat = [this](const char* name, f32& out)
		{
			const auto it = m_Parameters.find(name);
			if (it != m_Parameters.end() && it->second.data.size() >= sizeof(f32))
				memcpy(&out, it->second.data.data(), sizeof(f32));
		};

	if (const auto it = m_Parameters.find("Mode"); it != m_Parameters.end() && it->second.data.size() >= sizeof(int32))
	{
		int32 mode = 0;
		memcpy(&mode, it->second.data.data(), sizeof(int32));
		m_Settings.Mode = mode == static_cast<int32>(EDepthOfFieldMode::PhysicalLens) ? EDepthOfFieldMode::PhysicalLens : EDepthOfFieldMode::FocusRange;
	}
	readFloat("FocusDistance", m_Settings.FocusDistance);
	readFloat("FocusRange", m_Settings.FocusRange);
	readFloat("TransitionRange", m_Settings.TransitionRange);
	readFloat("FStop", m_Settings.FStop);
	readFloat("SensorHeight", m_Settings.SensorHeight);
	readFloat("MaxBlurRadius", m_Settings.MaxBlurRadius);
}

void DepthOfFieldEffect::SetFloat(const string& name, f32 value)
{
	auto it = m_Parameters.find(name);
	if (it == m_Parameters.end() || it->second.type != EMaterialParameterType::Float)
		return;
	memcpy(it->second.data.data(), &value, sizeof(f32));
	SyncSettings();
}

void DepthOfFieldEffect::SetInt(const string& name, int32 value)
{
	auto it = m_Parameters.find(name);
	if (it == m_Parameters.end() || it->second.type != EMaterialParameterType::Int)
		return;
	// 범위 밖의 값(손으로 고친 프리셋)은 가까운 모드로 고정한다.
	value = std::clamp(value, static_cast<int32>(it->second.minValue), static_cast<int32>(it->second.maxValue));
	memcpy(it->second.data.data(), &value, sizeof(int32));
	SyncSettings();
}

void DepthOfFieldEffect::ResetParameter(const string& name)
{
	auto it = m_Parameters.find(name);
	if (it == m_Parameters.end())
		return;
	it->second.data = it->second.defaultValue;
	SyncSettings();
}

void DepthOfFieldEffect::ResetAllParameters()
{
	for (auto& [name, param] : m_Parameters)
		param.data = param.defaultValue;
	SyncSettings();
}

bool DepthOfFieldEffect::EnsureTargets(uint32 width, uint32 height)
{
	auto& rtMgr = RenderTargetManager::Get();
	const auto ensure = [&](const wstring& name, uint32 w, uint32 h, ETextureFormat format) -> bool
		{
			if (auto* target = rtMgr.GetRenderTarget(name))
			{
				if (target->GetWidth() == w && target->GetHeight() == h)
					return true;
				return target->Resize(w, h) == EResult::Success;
			}
			RenderTargetDesc desc = {
				{ w, h, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, format,
				  ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 0.f) },
				name
			};
			return rtMgr.CreateRenderTarget(&desc) != nullptr;
		};

	const uint32 halfW = HalfExtent(width), halfH = HalfExtent(height);
	const uint32 tileW = TileExtent(width), tileH = TileExtent(height);
	bool succeeded = ensure(m_HalfRTName, halfW, halfH, HalfFormat);
	succeeded = ensure(m_TileRTName, tileW, tileH, TileFormat) && succeeded;
	succeeded = ensure(m_DilatedTileRTName, tileW, tileH, TileFormat) && succeeded;
	succeeded = ensure(m_FarRTName, halfW, halfH, HalfFormat) && succeeded;
	succeeded = ensure(m_NearRTName, halfW, halfH, HalfFormat) && succeeded;
	return succeeded;
}

EResult DepthOfFieldEffect::SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT)
{
	// 실패할 때는 명령을 하나도 제출하지 않는다. 파이프라인이 이 효과를 건너뛴다.
	const bool ready = m_HasSceneInputs && m_PrefilterPipeline && m_TilePipeline && m_DilatePipeline && m_BlurPipeline && m_CompositePipeline;
	auto* src = ready ? RenderTargetManager::Get().GetRenderTarget(sourceRT) : nullptr;
	if (!src || !EnsureTargets(src->GetWidth(), src->GetHeight()))
	{
		SetPrePassesEnabled(false);
		return EResult::Fail;
	}
	SetPrePassesEnabled(true);

	const uint32 width = src->GetWidth();
	const uint32 height = src->GetHeight();
	const DepthOfFieldSettings& settings = m_SceneInputs.CameraDepthOfField ? *m_SceneInputs.CameraDepthOfField : m_Settings;
	const CameraBuffer& camera = m_SceneInputs.Camera;
	const DepthOfFieldCoCParams coc = MakeDepthOfFieldCoCParams(settings, camera.projMatrix, height);

	DepthOfFieldShaderParams params;
	params.CameraPosition = camera.cameraPosition;
	params.FocusDistance = coc.FocusDistance;
	params.CameraForward = GetCameraForward(camera);
	params.HalfRange = coc.HalfRange;
	params.InvTransition = coc.InvTransition;
	params.LensScale = coc.LensScale;
	params.MaxRadius = coc.MaxRadius;
	params.Mode = static_cast<uint32>(coc.Mode);
	params.SourceSize = vec2(static_cast<f32>(width), static_cast<f32>(height));
	params.InvSourceSize = 1.0f / params.SourceSize;
	params.HalfSize = vec2(static_cast<f32>(HalfExtent(width)), static_cast<f32>(HalfExtent(height)));
	params.InvHalfSize = 1.0f / params.HalfSize;
	params.DilateTiles = static_cast<uint32>(std::ceil(coc.MaxRadius / static_cast<f32>(DepthOfFieldTileSize)));

	SamplerDesc clampDesc{};
	clampDesc.AddressU = ESamplerAddressMode::Clamp;
	clampDesc.AddressV = ESamplerAddressMode::Clamp;
	clampDesc.AddressW = ESamplerAddressMode::Clamp;
	RHISampler* sampler = SamplerManager::Get().GetOrCreateSampler(clampDesc);

	// 이름 순서대로 t0, t1, ... (space3)에 묶고 그린다.
	const auto submit = [sampler](RHIPipeline* pipeline, vector<wstring> inputs, const DepthOfFieldShaderParams& cb, RenderPassID pass)
		{
			Renderer::Get().SubmitCustomCommand([pipeline, inputs = std::move(inputs), cb, sampler](f32, RenderPass*) -> EResult
				{
					auto& rtMgr = RenderTargetManager::Get();
					auto* rhi = Renderer::Get().GetRHI();
					for (uint32 i = 0; i < inputs.size(); ++i)
					{
						auto* target = rtMgr.GetRenderTarget(inputs[i]);
						if (!target || !target->GetTexture()) return EResult::Fail;
						if (IsFailure(rhi->BindTextureSampler(target->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + i))) return EResult::Fail;
					}
					if (IsFailure(rhi->BindConstantBuffer(&cb, sizeof(cb), 2))) return EResult::Fail;
					if (IsFailure(rhi->BindPipeline(pipeline))) return EResult::Fail;
					return rhi->Draw(3);
				}, pass);
		};

	const wstring& position = m_SceneInputs.PositionRT;
	const wstring& diffuse = m_SceneInputs.DiffuseRT;

	// 1. 절반 해상도 색과 CoC
	submit(m_PrefilterPipeline, { sourceRT, position, diffuse }, params, m_PrePassIDs[Prefilter]);
	// 2~3. 앞쪽 CoC의 타일 최댓값을 이웃 타일로 넓힌다. 흐린 앞 물체가 선명한 뒤 배경 위로 번질 범위다.
	submit(m_TilePipeline, { m_HalfRTName }, params, m_PrePassIDs[NearTile]);
	submit(m_DilatePipeline, { m_TileRTName }, params, m_PrePassIDs[NearDilate]);
	// 4~5. 뒤쪽과 앞쪽을 따로 흐린다.
	DepthOfFieldShaderParams nearParams = params;
	nearParams.NearPass = 1u;
	submit(m_BlurPipeline, { m_HalfRTName, m_DilatedTileRTName }, params, m_PrePassIDs[FarBlur]);
	submit(m_BlurPipeline, { m_HalfRTName, m_DilatedTileRTName }, nearParams, m_PrePassIDs[NearBlur]);
	// 6. 메인 슬롯: 전체 해상도 CoC로 원래 화면, 뒤쪽 흐림, 앞쪽 흐림을 섞는다.
	submit(m_CompositePipeline, { sourceRT, position, diffuse, m_FarRTName, m_NearRTName }, params, passID);

	return EResult::Success;
}
