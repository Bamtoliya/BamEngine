#include "ViewportDeferredPipeline.h"
#include "RenderTargetManager.h"
#include "RenderPassManager.h"
#include "ResourceManager.h"
#include "PipelineManager.h"
#include "RHIPipeline.h"
#include "Renderer.h"
#include "LightManager.h"
#include "Light.h"
#include "SamplerManager.h"
#include "TimeManager.h"
#include "SceneManager.h"
#include "Scene.h"
#include "Components/Render/FogComponent.h"
#include "Components/Render/SkyAtmosphereComponent.h"
#include "Components/Render/VolumetricCloudComponent.h"

namespace
{
	struct DeferredLightingPassData
	{
		LightShadowData Shadow{};
		VolumetricCloudData Cloud{};
		vec3 SunDirection{ 0.0f, 1.0f, 0.0f };
		f32 PlanetRadius = 6360000.0f;
	};
}

ViewportDeferredPipeline::~ViewportDeferredPipeline()
{
	Free();
}

void ViewportDeferredPipeline::Initialize(const std::wstring& viewportPrefix, uint32 width, uint32 height, bool acceptSceneMeshes)
{
	m_Prefix = viewportPrefix.empty() ? L"VP_" : (viewportPrefix + L"_");
	m_Width = width;
	m_Height = height;
	m_AcceptSceneMeshes = acceptSceneMeshes;
	CreateRenderTargets(width, height);
	RegisterRenderPasses();
	CreateLightingPipeline();
}

void ViewportDeferredPipeline::Free()
{
	auto& rtm = Engine::RenderTargetManager::Get();
	for (const auto& name : m_OwnedRTNames)
	{
		rtm.RemoveRenderTarget(name);
	}
	m_OwnedRTNames.clear();

	m_ShadowPassID = Engine::INVALID_PASS_ID;
	m_GeometryPassID = Engine::INVALID_PASS_ID;
	m_LightingPassID = Engine::INVALID_PASS_ID;
	m_LightingPipeline = nullptr;
}

void ViewportDeferredPipeline::CreateRenderTargets(uint32 width, uint32 height)
{
	auto& rtm = Engine::RenderTargetManager::Get();

	Engine::RenderTargetDesc gbufferDescs[] = {
		// 0~4: GBuffer (Diffuse, Normal, PBR, Emission, Position)
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R8G8B8A8_UNORM, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_Diffuse" },
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R16G16B16A16_FLOAT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_Normal" },
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R8G8B8A8_UNORM, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_PBR" },
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R8G8B8A8_UNORM, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_Emission" },
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R32G32B32A32_FLOAT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"GBuffer_Position" },
		// 5: ShadowDepth (4096 Atlas)
		{ { 4096, 4096, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::D32_FLOAT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::DepthStencilTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"ShadowDepth" },
		// 6: Depth
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::D24_UNORM_S8_UINT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::DepthStencilTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) }, m_Prefix + L"Depth" },
		// 7: LightingColor
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R8G8B8A8_UNORM, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 1.f) }, m_Prefix + L"LightingColor" }
	};

	m_OwnedRTNames.clear();
	for (auto& desc : gbufferDescs)
	{
		rtm.CreateRenderTarget(&desc);
		m_OwnedRTNames.push_back(desc.name);
	}

	m_ShadowDepthName = m_OwnedRTNames[5];
	m_DepthName = m_OwnedRTNames[6];
	m_LightingColorName = m_OwnedRTNames[7];
}

void ViewportDeferredPipeline::RegisterRenderPasses()
{
	auto& rpm = Engine::RenderPassManager::Get();
	std::vector<std::wstring> gBufferNames = { m_OwnedRTNames.begin(), m_OwnedRTNames.begin() + 5 };
	// 1. ShadowPass
	m_ShadowPassID = rpm.RegisterRenderPass(
		m_Prefix + L"ShadowPass", {}, m_ShadowDepthName,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Shadow, Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked);
	// 2. GeometryPass (프리뷰 파이프라인의 경우 메인 씬 메쉬 유입을 차단하기 위해 acceptedBlendModes를 None으로 설정)
	const Engine::EBlendMode acceptedModes = m_AcceptSceneMeshes ? (Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked) : Engine::EBlendMode::None;
	m_GeometryPassID = rpm.RegisterRenderPass(
		m_Prefix + L"GeometryPass", gBufferNames, m_DepthName,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Geometry, acceptedModes);
	// 3. LightingPass (풀스크린 조명 합성)
	m_LightingPassID = rpm.RegisterRenderPass(
		m_Prefix + L"LightingPass", { m_LightingColorName }, L"",
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 100, Engine::ERenderSortType::None,
		Engine::ERenderPassType::Lighting, Engine::EBlendMode::None);
}

void ViewportDeferredPipeline::CreateLightingPipeline()
{
	auto& rm = Engine::ResourceManager::Get();
	RHIPipelineDesc pd = {}; // Engine:: 제거
	pd.pipelineType = Engine::EPipelineType::Graphics;
	pd.vertexShader = rm.GetResourceHandle<Engine::Shader>(L"FullscreenQuadVS")->GetRHIShader();
	pd.pixelShader = rm.GetResourceHandle<Engine::Shader>(L"LightingPS")->GetRHIShader();
	pd.colorAttachmentCount = 1;
	pd.colorAttachmentFormats[0] = Engine::ETextureFormat::R8G8B8A8_UNORM;
	pd.depthStencilAttachmentFormat = Engine::ETextureFormat::UNKNOWN;
	pd.depthStencilState.depthTestEnable = false;
	pd.depthStencilState.depthWriteEnable = false;
	pd.topology = Engine::ETopology::TriangleList;
	pd.cullMode = Engine::ECullMode::None;
	pd.blendState = Engine::BlendState{};
	m_LightingPipeline = Engine::PipelineManager::Get().GetOrCreatePipeline(pd);
}

void ViewportDeferredPipeline::Resize(uint32 width, uint32 height)
{
	if (m_Width == width && m_Height == height) return;
	if (width == 0 || height == 0) return;

	m_Width = width;
	m_Height = height;

	auto& rtm = Engine::RenderTargetManager::Get();
	for (const auto& name : m_OwnedRTNames)
	{
		if (name == m_ShadowDepthName) continue; // Shadow는 4096 고정
		rtm.ResizeRenderTarget(name, width, height);
	}
}

void ViewportDeferredPipeline::Submit(const CameraBuffer& cameraBuffer, Engine::Scene* targetScene)
{
	(void)targetScene;
	auto& rpm = Engine::RenderPassManager::Get();
	auto& renderer = Engine::Renderer::Get();

	// 1. ShadowPass 카메라 등록
	renderer.RegisterViewportCamera(rpm.GetRenderPassByID(m_ShadowPassID));

	// 2. GeometryPass 카메라 등록 (씬 메쉬들이 GBuffer에 렌더링됨)
	renderer.RegisterViewportCamera(cameraBuffer, rpm.GetRenderPassByID(m_GeometryPassID));

	// 3. LightingPass 풀스크린 커스텀 커맨드 제출
	SubmitLightingPass(cameraBuffer, targetScene);
}

void ViewportDeferredPipeline::SubmitLightingPass(const CameraBuffer& cameraBuffer, Engine::Scene* targetScene)
{
	(void)targetScene;
	Engine::Renderer::Get().SubmitCustomCommand(
		[this, cameraBuffer](f32 dt, Engine::RenderPass* pass) -> Engine::EResult
		{
			(void)dt;
			(void)pass;
			auto* rhi = Engine::Renderer::Get().GetRHI();
			auto& rtMgr = Engine::RenderTargetManager::Get();
			auto* sampler = Engine::SamplerManager::Get().GetDefaultSampler();
			if (!rhi || !sampler || !m_LightingPipeline) return Engine::EResult::Fail;

			// 1. 카메라 버퍼 바인딩 (slot 0 -> b0, space0)
			CameraBuffer camBuf = cameraBuffer;
			camBuf.time = static_cast<f32>(Engine::TimeManager::Get().GetTotalTime());
			if (Engine::IsFailure(rhi->BindConstantBuffer(&camBuf, sizeof(CameraBuffer), 0)))
				return Engine::EResult::Fail;

			// 2. FogData 상수 버퍼 바인딩 (slot 2 -> b0, space3, 비활성화 상태로 전달)
			Engine::FogData fogData{};
			fogData.FogEnabled = 0u;
			if (Engine::IsFailure(rhi->BindConstantBuffer(&fogData, sizeof(fogData), 2)))
				return Engine::EResult::Fail;

			// 3. GBuffer 5개 + ShadowDepth 텍스처 바인딩 (t0~t5, space3)
			const std::wstring sourceNames[] = {
				m_Prefix + L"GBuffer_Diffuse", m_Prefix + L"GBuffer_Normal", m_Prefix + L"GBuffer_PBR",
				m_Prefix + L"GBuffer_Emission", m_Prefix + L"GBuffer_Position", m_ShadowDepthName
			};

			for (uint32 i = 0; i < 6; ++i)
			{
				auto* source = rtMgr.GetRenderTarget(sourceNames[i]);
				if (!source || !source->GetTexture()) return Engine::EResult::Fail;
				if (Engine::IsFailure(rhi->BindTextureSampler(source->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + i)))
					return Engine::EResult::Fail;
			}

			// 4. 파이프라인 바인딩
			if (Engine::IsFailure(rhi->BindPipeline(m_LightingPipeline)))
				return Engine::EResult::Fail;

			// 5. 광원 버퍼 바인딩 (t6, space3)
			auto& lightManager = Engine::LightManager::Get();
			if (Engine::IsFailure(lightManager.Bind(0)))
				return Engine::EResult::Fail;

			DeferredLightingPassData lightingData{};
			if (lightManager.HasShadowCastingLights())
				lightingData.Shadow = lightManager.GetShadowData(0u);

			// 6. 조명 패스 상수 버퍼 바인딩 (slot 3 -> b1, space3)
			if (Engine::IsFailure(rhi->BindConstantBuffer(&lightingData, sizeof(lightingData), 3)))
				return Engine::EResult::Fail;

			// 7. 풀스크린 쿼드 조명 합성 실행
			if (Engine::IsFailure(rhi->Draw(3)))
				return Engine::EResult::Fail;

			return Engine::EResult::Success;
		},
		m_LightingPassID
	);
}