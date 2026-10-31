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
		uint32 HasIBLMaps = 0u;
		f32 IBLIntensity = 1.0f;
		vec2 _iblPad{ 0.0f };
		vec4 _reserved{ 0.0f };
	};
	static_assert(sizeof(DeferredLightingPassData) == 512);
	static_assert(offsetof(DeferredLightingPassData, Shadow) == 0);
	static_assert(offsetof(DeferredLightingPassData, Cloud) == 336);
	static_assert(offsetof(DeferredLightingPassData, SunDirection) == 464);
	static_assert(offsetof(DeferredLightingPassData, PlanetRadius) == 476);
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
		rtm.RemoveRenderTarget(name);
	m_OwnedRTNames.clear();

	m_ShadowPassID = Engine::INVALID_PASS_ID;
	m_PointShadowPassID = Engine::INVALID_PASS_ID;
	m_GeometryPassID = Engine::INVALID_PASS_ID;
	m_LightingPassID = Engine::INVALID_PASS_ID;
	m_ForwardPassID = Engine::INVALID_PASS_ID;
	m_ForwardTransparentPassID = Engine::INVALID_PASS_ID;
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
		// 7: LightingColor (HDR 선형 부동소수점 버퍼)
		{ { width, height, 1, 1, 1, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::R16G16B16A16_FLOAT, Engine::ETextureDimension::Texture2D, Engine::ETextureUsage::RenderTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 1.f) }, m_Prefix + L"LightingColor" }
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

	// 8: PointShadowDepth (Point Light 큐브맵 섀도우)
	m_PointShadowDepthName = m_Prefix + L"PointShadowDepth";
	Engine::RenderTargetDesc pointShadowDesc = {
		{ 1024, 1024, 1, 1, 6, Engine::ETextureSampleCount::TextureSampleCount1, Engine::ETextureFormat::D32_FLOAT, Engine::ETextureDimension::TextureCube, Engine::ETextureUsage::DepthStencilTarget | Engine::ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f) },
		m_PointShadowDepthName
	};
	rtm.CreateRenderTarget(&pointShadowDesc);
	m_OwnedRTNames.push_back(m_PointShadowDepthName);
}

void ViewportDeferredPipeline::RegisterRenderPasses()
{
	auto& rpm = Engine::RenderPassManager::Get();
	std::vector<std::wstring> gBufferNames = { m_OwnedRTNames.begin(), m_OwnedRTNames.begin() + 5 };

	// 1. ShadowPass (Directional Light 그림자)
	m_ShadowPassID = rpm.RegisterRenderPass(
		m_Prefix + L"ShadowPass", {}, m_ShadowDepthName,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Shadow, Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked);

	// 2. PointShadowPass (Point Light 큐브맵 그림자)
	m_PointShadowPassID = rpm.RegisterRenderPass(
		m_Prefix + L"PointShadowPass", {}, m_PointShadowDepthName,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Shadow, Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked);

	// 3. GeometryPass (프리뷰 파이프라인의 경우 메인 씬 메쉬 유입을 차단하기 위해 acceptedBlendModes를 None으로 설정)
	const Engine::EBlendMode acceptedModes = m_AcceptSceneMeshes ? (Engine::EBlendMode::Opaque | Engine::EBlendMode::Masked) : Engine::EBlendMode::None;
	m_GeometryPassID = rpm.RegisterRenderPass(
		m_Prefix + L"GeometryPass", gBufferNames, m_DepthName,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Geometry, acceptedModes);

	// 4. LightingPass (풀스크린 조명 합성)
	m_LightingPassID = rpm.RegisterRenderPass(
		m_Prefix + L"LightingPass", { m_LightingColorName }, L"",
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Clear, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 100, Engine::ERenderSortType::None,
		Engine::ERenderPassType::Lighting, Engine::EBlendMode::None);

	// 5. ForwardPass (포워드 불투명 메쉬 렌더링)
	m_ForwardPassID = rpm.RegisterRenderPass(
		m_Prefix + L"ForwardPass", { m_LightingColorName }, m_DepthName,
		Engine::ERenderPassLoadOperation::RPLO_Load, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Load, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 110, Engine::ERenderSortType::FrontToBack,
		Engine::ERenderPassType::Forward, Engine::EBlendMode::Forward);

	// 6. ForwardTransparentPass (반투명 메쉬 렌더링)
	m_ForwardTransparentPassID = rpm.RegisterRenderPass(
		m_Prefix + L"ForwardTransparentPass", { m_LightingColorName }, m_DepthName,
		Engine::ERenderPassLoadOperation::RPLO_Load, Engine::ERenderPassStoreOperation::RPSO_Store,
		Engine::ERenderPassLoadOperation::RPLO_Load, Engine::ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 120, Engine::ERenderSortType::BackToFront,
		Engine::ERenderPassType::ForwardTransparent,
		Engine::EBlendMode::AlphaBlend | Engine::EBlendMode::Additive | Engine::EBlendMode::NonPremultiplied | Engine::EBlendMode::Decal);
}

void ViewportDeferredPipeline::CreateLightingPipeline()
{
	auto& rm = Engine::ResourceManager::Get();
	RHIPipelineDesc pd = {};
	pd.pipelineType = Engine::EPipelineType::Graphics;
	pd.vertexShader = rm.GetResourceHandle<Engine::Shader>(L"FullscreenQuadVS")->GetRHIShader();
	pd.pixelShader = rm.GetResourceHandle<Engine::Shader>(L"LightingPS")->GetRHIShader();
	pd.colorAttachmentCount = 1;
	pd.colorAttachmentFormats[0] = Engine::ETextureFormat::R16G16B16A16_FLOAT;
	pd.depthStencilAttachmentFormat = Engine::ETextureFormat::UNKNOWN;
	pd.depthStencilState.depthTestEnable = false;
	pd.depthStencilState.depthWriteEnable = false;
	pd.topology = Engine::ETopology::TriangleList;
	pd.cullMode = Engine::ECullMode::None;
	pd.blendState = Engine::BlendState{};
	m_LightingPipeline = Engine::PipelineManager::Get().GetOrCreatePipeline(pd);
}

EResult ViewportDeferredPipeline::Resize(uint32 width, uint32 height)
{
	if (width == 0 || height == 0 || m_OwnedRTNames.empty())
		return EResult::InvalidArgument;

	auto& rtMgr = Engine::RenderTargetManager::Get();
	bool succeeded = true;

	for (const auto& name : m_OwnedRTNames)
	{
		if (name == m_ShadowDepthName || name == m_PointShadowDepthName)
			continue;

		auto* target = rtMgr.GetRenderTarget(name);
		if (!target || target->Resize(width, height) != EResult::Success)
			succeeded = false;
	}

	if (!succeeded) return EResult::Fail;

	m_Width = width;
	m_Height = height;
	return EResult::Success;
}

void ViewportDeferredPipeline::Submit(const CameraBuffer& cameraBuffer, Engine::Scene* targetScene)
{
	auto& rpm = Engine::RenderPassManager::Get();
	auto& renderer = Engine::Renderer::Get();

	Engine::Scene* sceneToQuery = targetScene ? targetScene : (Engine::SceneManager::IsCreated() ? Engine::SceneManager::Get().GetCurrentScene() : nullptr);
	const Engine::SkyAtmosphereData skyData = BuildSkyAtmosphereData(sceneToQuery);

	// 1. ShadowPass & PointShadowPass 카메라 등록
	auto* shadowPass = rpm.GetRenderPassByID(m_ShadowPassID);
	if (shadowPass && shadowPass->IsEnabled())
		renderer.RegisterViewportCamera(shadowPass);

	auto* pointShadowPass = rpm.GetRenderPassByID(m_PointShadowPassID);
	if (pointShadowPass && pointShadowPass->IsEnabled())
		renderer.RegisterViewportCamera(pointShadowPass);

	// 2. GeometryPass 카메라 등록 (씬 메쉬들이 GBuffer에 렌더링됨)
	auto* geomPass = rpm.GetRenderPassByID(m_GeometryPassID);
	if (geomPass && geomPass->IsEnabled())
		renderer.RegisterViewportCamera(cameraBuffer, geomPass);

	// 3. ForwardPass & ForwardTransparentPass 카메라 등록 (스카이 데이터 상수버퍼 slot 3 주입)
	auto* forwardPass = rpm.GetRenderPassByID(m_ForwardPassID);
	if (forwardPass && forwardPass->IsEnabled())
	{
		forwardPass->SetPassConstantStruct(3, skyData);
		renderer.RegisterViewportCamera(cameraBuffer, forwardPass);
	}

	auto* forwardTransPass = rpm.GetRenderPassByID(m_ForwardTransparentPassID);
	if (forwardTransPass && forwardTransPass->IsEnabled())
		renderer.RegisterViewportCamera(cameraBuffer, forwardTransPass);

	// 4. LightingPass 풀스크린 조명 커스텀 커맨드 제출
	auto* lightingPass = rpm.GetRenderPassByID(m_LightingPassID);
	if (lightingPass && lightingPass->IsEnabled())
		SubmitLightingPass(cameraBuffer, targetScene, skyData);
}

Engine::SkyAtmosphereData ViewportDeferredPipeline::BuildSkyAtmosphereData(Engine::Scene* scene)
{
	Engine::SkyAtmosphereData skyData{};
	if (!scene)
	{
		if (Engine::SceneManager::IsCreated())
			scene = Engine::SceneManager::Get().GetCurrentScene();
	}
	if (!scene) return skyData;

	auto& registry = scene->GetRegistry();

	// 1. SkyAtmosphereComponent 검색 및 태양광/대기 산란 계산
	auto skyView = registry.view<Engine::SkyAtmosphereComponent>();
	for (auto [e, sky] : skyView.each())
	{
		if (!sky.IsEnabled) continue;

		f32 hourAngle = (sky.TimeOfDay - 12.0f) * (glm::pi<float>() / 12.0f);
		f32 latRad = glm::radians(sky.Latitude);
		vec3 toSun = glm::normalize(vec3(std::sin(hourAngle), std::cos(hourAngle) * std::cos(latRad), -std::cos(hourAngle) * std::sin(latRad)));
		skyData.SunDirection = toSun;
		skyData.ZenithColor = sky.ZenithColor;
		skyData.HorizonColor = sky.HorizonColor;
		skyData.GroundColor = sky.GroundColor;
		skyData.HorizonBlend = sky.HorizonBlend;
		skyData.SkyIntensity = sky.SkyIntensity;
		skyData.SunSize = sky.SunSize;
		skyData.SunColor = sky.CustomSunColor;
		skyData.SunIntensity = sky.CustomSunIntensity;
		skyData.Cloud.EnableClouds = sky.EnableClouds ? 1u : 0u;
		skyData.Cloud.CloudCoverage = sky.CloudCoverage;
		skyData.Cloud.CloudDensity = sky.CloudDensity;
		skyData.Cloud.LayerBottomAltitude = sky.CloudAltitude;
		skyData.Cloud.WindSpeed = sky.CloudSpeed * 100.0f;
		if (sky.AutoSyncDirectionalLight)
		{
			auto lightView = registry.view<Engine::LightComponent>();
			for (auto [le, light] : lightView.each())
			{
				if (light.IsEnabled && light.LightType == Engine::ELightType::Directional)
				{
					skyData.SunColor = light.Color;
					skyData.SunIntensity = light.Intensity;
					break;
				}
			}
		}
		break;
	}

	// 2. 씬에 전용 VolumetricCloudComponent가 있으면 상세 설정으로 덮어쓰기
	auto cloudView = registry.view<Engine::VolumetricCloudComponent>();
	for (auto [ce, cloud] : cloudView.each())
	{
		if (!cloud.IsEnabled) continue;
		skyData.Cloud.EnableClouds = 1u;
		skyData.Cloud.LayerBottomAltitude = cloud.LayerBottomAltitude;
		skyData.Cloud.LayerHeight = cloud.LayerHeight;
		skyData.Cloud.TracingMaxDistance = cloud.TracingMaxDistance;
		skyData.Cloud.CloudCoverage = cloud.CloudCoverage;
		skyData.Cloud.CloudDensity = cloud.CloudDensity;
		skyData.Cloud.WeatherScale = 1.0f / std::max(cloud.WeatherMapSize, 100.0f);
		skyData.Cloud.ShapeScale = 1.0f / std::max(cloud.ShapeNoiseSize, 50.0f);
		skyData.Cloud.DetailScale = 1.0f / std::max(cloud.DetailNoiseSize, 10.0f);
		skyData.Cloud.WindDirection = cloud.WindDirection;
		skyData.Cloud.WindSpeed = cloud.WindSpeed;
		skyData.Cloud.MorphSpeed = cloud.MorphSpeed;
		skyData.Cloud.CloudAlbedo = cloud.CloudAlbedo;
		skyData.Cloud.ExtinctionFactor = cloud.ExtinctionFactor;
		skyData.Cloud.PowderExtinction = cloud.PowderExtinction;
		break;
	}

	return skyData;
}

void ViewportDeferredPipeline::SubmitLightingPass(const CameraBuffer& cameraBuffer, Engine::Scene* targetScene, const Engine::SkyAtmosphereData& skyData)
{
	Engine::Renderer::Get().SubmitCustomCommand(
		[this, cameraBuffer, targetScene, skyData](f32 dt, Engine::RenderPass* pass) -> Engine::EResult
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

			Engine::Scene* sceneToQuery = targetScene ? targetScene : Engine::SceneManager::Get().GetCurrentScene();

			// 2. FogData 상수 버퍼 바인딩 (slot 2 -> b0, space3)
			Engine::FogData fogData{};
			fogData.FogEnabled = 0u;

			if (sceneToQuery)
			{
				auto fogView = sceneToQuery->GetRegistry().view<Engine::FogComponent>();
				for (auto [e, fog] : fogView.each())
				{
					if (!fog.IsEnabled) continue;

					fogData.FogEnabled = 1u;
					fogData.FogType = static_cast<uint32>(fog.FogType);
					fogData.FogColor = fog.FogColor;
					fogData.FogDensity = fog.FogDensity;
					fogData.FogHeight = fog.FogHeight;
					fogData.FogHeightFalloff = fog.FogHeightFalloff;
					fogData.FogStartDistance = fog.FogStartDistance;
					fogData.FogEndDistance = fog.FogEndDistance;
					fogData.FogMaxOpacity = fog.FogMaxOpacity;

					if (fog.AutoSyncAtmosphere)
					{
						auto skyView = sceneToQuery->GetRegistry().view<Engine::SkyAtmosphereComponent>();
						for (auto [se, sky] : skyView.each())
						{
							if (sky.IsEnabled)
							{
								fogData.FogColor = glm::mix(fog.FogColor, sky.HorizonColor, 0.65f);
								break;
							}
						}
					}
					break;
				}
			}

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

			// 3-1. Point Light TextureCube 바인딩 (t7, space3)
			if (auto* pointShadowRT = rtMgr.GetRenderTarget(m_PointShadowDepthName))
			{
				if (auto* pointShadowTex = pointShadowRT->GetTexture())
				{
					if (Engine::IsFailure(rhi->BindTextureSampler(pointShadowTex, sampler, MAX_MATERIAL_TEXTURE_SLOTS + 7)))
						return Engine::EResult::Fail;
				}
			}

			auto& lightManager = Engine::LightManager::Get();

			// 3-2. 타일 기반 광원 컬링 Compute Shader 디스패치
			if (auto* posRT = rtMgr.GetRenderTarget(sourceNames[4]))
			{
				if (auto* posTex = posRT->GetTexture())
					lightManager.DispatchLightCulling(camBuf, posTex);
			}

			// 4. 조명 그래픽스 파이프라인 바인딩 (컴퓨트 디스패치 직후 바인딩)
			if (Engine::IsFailure(rhi->BindPipeline(m_LightingPipeline)))
				return Engine::EResult::Fail;

			// 4-1. 타일 결과 버퍼 바인딩 (슬롯 1 -> t8, 슬롯 2 -> t9)
			if (auto* gridBuf = lightManager.GetTileLightGridBuffer())
				rhi->BindStorageBuffer(gridBuf, 1);
			if (auto* idxBuf = lightManager.GetTileLightIndexBuffer())
				rhi->BindStorageBuffer(idxBuf, 2);

			// 5. 광원 데이터 버퍼 바인딩 (t6, space3)
			if (Engine::IsFailure(lightManager.Bind(0)))
				return Engine::EResult::Fail;

			DeferredLightingPassData lightingData{};
			if (lightManager.HasShadowCastingLights())
				lightingData.Shadow = lightManager.GetShadowData(0u);

			// skyData로부터 정확한 태양 방향 및 구름 데이터 주입
			lightingData.Cloud = skyData.Cloud;
			lightingData.SunDirection = skyData.SunDirection;

			// ── IBL 텍스처 바인딩 (t10, t11, t12) ──
			if (sceneToQuery)
			{
				auto skyView = sceneToQuery->GetRegistry().view<Engine::SkyAtmosphereComponent>();
				for (auto [e, sky] : skyView.each())
				{
					if (sky.IsEnabled && sky.IrradianceMap.IsValid() && sky.PrefilteredMap.IsValid() && sky.BRDFLutMap.IsValid())
					{
						auto* irrTex = sky.IrradianceMap.Get();
						auto* prefTex = sky.PrefilteredMap.Get();
						auto* lutTex = sky.BRDFLutMap.Get();
						if (irrTex && prefTex && lutTex)
						{
							rhi->BindTextureSampler(irrTex->GetRHITexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 10);
							rhi->BindTextureSampler(prefTex->GetRHITexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 11);
							rhi->BindTextureSampler(lutTex->GetRHITexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 12);
							lightingData.HasIBLMaps = 1u;
							lightingData.IBLIntensity = sky.IBLIntensity;
							break;
						}
					}
				}
			}

			auto& rm = Engine::ResourceManager::Get();
			auto shapeHandle = rm.GetResourceHandle<Engine::Texture>(L"Resources/Texture/CloudBaseShape3D.bamtex");
			auto weatherHandle = rm.GetResourceHandle<Engine::Texture>(L"Resources/Texture/CloudWeatherMap2D.bamtex");
			auto* shapeTexture = shapeHandle.Get();
			auto* weatherTexture = weatherHandle.Get();
			if (shapeTexture && weatherTexture)
			{
				auto* shapeRHI = shapeTexture->GetRHITexture();
				auto* weatherRHI = weatherTexture->GetRHITexture();
				if (shapeRHI && weatherRHI)
				{
					rhi->BindTextureSampler(shapeRHI, sampler, 0);
					rhi->BindTextureSampler(weatherRHI, sampler, 1);
				}
			}

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