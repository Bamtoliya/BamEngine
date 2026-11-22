#include "DeferredSceneRenderer.h"
#include "CloudResources.h"
#include "ComponentEnable.h"

#include "IBLManager.h"
#include "CompositeVFXSystem.h"
#include "FogComponent.h"
#include "LightComponent.h"
#include "LightManager.h"
#include "PipelineManager.h"
#include "RaytracingSceneManager.h"
#include "RenderTarget.h"
#include "RHI.h"
#include "Shader.h"
#include "PlatformPath.h"
#include "RHIBuffer.h"
#include "RHIPipeline.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "Renderer.h"
#include "ResourceManager.h"
#include "SamplerManager.h"
#include "Scene.h"
#include "SceneManager.h"
#include "SkyAtmosphereComponent.h"
#include "TimeManager.h"
#include "VolumetricCloudComponent.h"
#include <glm/gtc/constants.hpp>
#include <span>

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
		uint32 UseTiledCulling = 1u; // 0: 직접 광원 순회(프리뷰), 1: 타일 컬링 광원 순회(메인 씬)
		f32 ShadowAmbientFloor = 0.15f;
		vec3 SkyZenithColor{ 0.18f, 0.42f, 0.85f };
		f32 SkyIntensity = 1.0f;
		// 마스크(t14)의 채널별 광원 버퍼 인덱스. 마스크가 없으면 모두 INVALID_SHADOW_LIGHT
		std::array<uint32, MAX_RAYTRACED_SHADOW_LIGHTS> RaytracedShadowLights{ INVALID_SHADOW_LIGHT, INVALID_SHADOW_LIGHT, INVALID_SHADOW_LIGHT, INVALID_SHADOW_LIGHT };
		// 섀도맵을 가진 광원의 버퍼 인덱스. 다른 광원은 섀도맵을 읽지 않는다.
		uint32 CascadeShadowLight = INVALID_SHADOW_LIGHT;
		uint32 SpotShadowLight = INVALID_SHADOW_LIGHT;
		uint32 PointShadowLight = INVALID_SHADOW_LIGHT;
		uint32 ShadowLightPad = 0u;
	};
	static_assert(sizeof(DeferredLightingPassData) == 544);
	static_assert(offsetof(DeferredLightingPassData, Shadow) == 0);
	static_assert(offsetof(DeferredLightingPassData, Cloud) == 336);
	static_assert(offsetof(DeferredLightingPassData, SunDirection) == 464);
	static_assert(offsetof(DeferredLightingPassData, PlanetRadius) == 476);
	static_assert(offsetof(DeferredLightingPassData, HasIBLMaps) == 480);
	static_assert(offsetof(DeferredLightingPassData, IBLIntensity) == 484);
	static_assert(offsetof(DeferredLightingPassData, UseTiledCulling) == 488);
	static_assert(offsetof(DeferredLightingPassData, ShadowAmbientFloor) == 492);
	static_assert(offsetof(DeferredLightingPassData, SkyZenithColor) == 496);
	static_assert(offsetof(DeferredLightingPassData, SkyIntensity) == 508);
	static_assert(offsetof(DeferredLightingPassData, RaytracedShadowLights) == 512);
	static_assert(offsetof(DeferredLightingPassData, CascadeShadowLight) == 528);

	// raytracedShadowMaskRTCS.hlsl의 RaytracedShadowParams (b1, space3)
	struct RaytracedShadowParams
	{
		f32 NormalBias = 0.02f;
		f32 DistanceBiasScale = 0.0005f;
		f32 RayMaxDistance = 10000.0f;
		uint32 Pad = 0u;
		std::array<uint32, MAX_RAYTRACED_SHADOW_LIGHTS> Lights{};
	};
	static_assert(sizeof(RaytracedShadowParams) == 32);

	constexpr const wchar_t* RaytracedShadowShaderKey = L"Resources/Shader/RaytracedShadowMaskRTCS";
	constexpr const wchar_t* RaytracedShadowShaderFile = L"raytracedShadowMaskRTCS.cso";
	constexpr Engine::uint32 RaytracedShadowTLASSlot = MAX_MATERIAL_TEXTURE_SLOTS + 13; // t13, space3
	constexpr Engine::uint32 RaytracedShadowMaskSlot = MAX_MATERIAL_TEXTURE_SLOTS + 14; // t14, space3 (lightingPS)
}

Engine::DeferredSceneRenderer::~DeferredSceneRenderer()
{
	Free();
}

void Engine::DeferredSceneRenderer::Free()
{
	for (auto*& buffer : m_LocalLightBuffer)
		Safe_Release(buffer);
	m_LightingPipeline = nullptr;
	if (!m_RaytracedShadowMaskName.empty() && Engine::RenderTargetManager::IsCreated())
		Engine::RenderTargetManager::Get().RemoveRenderTarget(m_RaytracedShadowMaskName);
	m_RaytracedShadowMaskName.clear();
}

bool Engine::DeferredSceneRenderer::Initialize()
{
	m_LightingPipeline = nullptr;

	auto& manager = Engine::ResourceManager::Get();
	const auto vs = manager.GetResourceHandle<Engine::Shader>(L"FullscreenQuadVS");
	const auto ps = manager.GetResourceHandle<Engine::Shader>(L"LightingPS");

	if (!vs.IsValid() || !ps.IsValid() || !vs.Get() || !ps.Get())
		return false;

	if (!vs->GetRHIShader() || !ps->GetRHIShader())
		return false;

	RHIPipelineDesc pd{};
	pd.pipelineType = Engine::EPipelineType::Graphics;
	pd.vertexShader = vs->GetRHIShader();
	pd.pixelShader = ps->GetRHIShader();
	pd.colorAttachmentCount = 1;
	pd.colorAttachmentFormats[0] = Engine::ETextureFormat::R16G16B16A16_FLOAT;
	pd.depthStencilAttachmentFormat = Engine::ETextureFormat::UNKNOWN;
	pd.depthStencilState.depthTestEnable = false;
	pd.depthStencilState.depthWriteEnable = false;
	pd.topology = Engine::ETopology::TriangleList;
	pd.cullMode = Engine::ECullMode::None;
	pd.blendState = Engine::BlendState{};

	m_LightingPipeline = Engine::PipelineManager::Get().GetOrCreatePipeline(pd);
	return m_LightingPipeline && m_LightingPipeline->GetNativeHandle();
}

void Engine::DeferredSceneRenderer::Submit(const CameraBuffer& cameraBuffer, Scene* targetScene,
	const DeferredScenePassBindings& passes)
{
	auto& rpm = Engine::RenderPassManager::Get();
	auto& renderer = Engine::Renderer::Get();

	Engine::Scene* sceneToQuery = targetScene;
	if (!sceneToQuery && Engine::SceneManager::IsCreated())
		sceneToQuery = Engine::SceneManager::Get().GetCurrentScene();
	const Engine::SkyAtmosphereData skyData = BuildSkyAtmosphereData(sceneToQuery);
	if (skyData.Cloud.EnableClouds != 0u)
		CloudResources::Prepare();

	// 0. 레이트레이싱 그림자 사용 여부. 켜지면 방향광 캐스케이드에서는 TLAS가 맡는 캐스터를 그리지 않는다.
	// 셰이더를 쓸 수 없으면 캐스터를 빼기 전에 섀도맵으로 되돌린다.
	auto* lightingPass = rpm.GetRenderPassByID(passes.Lighting);
	ShadowLightSet shadowLights;
	RaytracedShadowAssignment raytracedLights;
	if (sceneToQuery && lightingPass && lightingPass->IsEnabled() &&
		Engine::RaytracingSceneManager::IsCreated() && Engine::RaytracingSceneManager::Get().IsSupported())
	{
		shadowLights = CollectViewShadowLights(sceneToQuery, passes.AcceptSceneMeshes);
		raytracedLights = AssignRaytracedShadows(shadowLights, m_RaytracedShadowOverrides);
		if (!raytracedLights.Empty() && !GetRaytracedShadowPipeline())
			raytracedLights = {};
	}
	const bool useRaytracedShadow = !raytracedLights.Empty();
	m_RaytracedShadowActive = useRaytracedShadow;

	// 1. ShadowPass & PointShadowPass 카메라 등록
	auto* shadowPass = rpm.GetRenderPassByID(passes.DirectionalShadow);
	if (shadowPass)
	{
		shadowPass->SetSkipsRaytracedShadowCasters(raytracedLights.Contains(shadowLights.CascadeLight));
		shadowPass->SetSkipsRaytracedSpotCasters(raytracedLights.Contains(shadowLights.SpotLight));
	}
	if (shadowPass && shadowPass->IsEnabled())
		renderer.RegisterShadowPassForView(shadowPass, cameraBuffer, passes.Geometry);

	auto* pointShadowPass = rpm.GetRenderPassByID(passes.PointShadow);
	if (pointShadowPass)
		pointShadowPass->SetSkipsRaytracedShadowCasters(raytracedLights.Contains(shadowLights.PointLight));
	if (pointShadowPass && pointShadowPass->IsEnabled())
		renderer.RegisterShadowPassForView(pointShadowPass, cameraBuffer, passes.Geometry);

	// 2. GeometryPass 카메라 등록 (씬 메쉬들이 GBuffer에 렌더링됨)
	auto* geomPass = rpm.GetRenderPassByID(passes.Geometry);
	if (geomPass && geomPass->IsEnabled())
		renderer.RegisterViewportCamera(cameraBuffer, geomPass, passes.AcceptSceneMeshes, passes.Geometry);

	// 3. ForwardPass & ForwardTransparentPass 카메라 등록 (스카이 데이터 상수버퍼 slot 3 주입)
	auto* forwardPass = rpm.GetRenderPassByID(passes.ForwardOpaque);
	if (forwardPass && forwardPass->IsEnabled())
	{
		forwardPass->SetPassConstantStruct(3, skyData);
		renderer.RegisterViewportCamera(cameraBuffer, forwardPass, passes.AcceptSceneMeshes, passes.Geometry);
	}

	auto* forwardTransPass = rpm.GetRenderPassByID(passes.ForwardTransparent);
	if (forwardTransPass && forwardTransPass->IsEnabled())
	{
		forwardTransPass->SetPassTexture(MAX_MATERIAL_TEXTURE_SLOTS + 1, passes.LightingInputs[1]);
		forwardTransPass->SetPassTexture(MAX_MATERIAL_TEXTURE_SLOTS + 4, passes.LightingInputs[4]);
		renderer.RegisterViewportCamera(cameraBuffer, forwardTransPass, passes.AcceptSceneMeshes, passes.Geometry);
	}

	// 4. LightingPass 풀스크린 조명 커스텀 커맨드 제출
	if (lightingPass && lightingPass->IsEnabled())
	{
		// 레이트레이싱 그림자를 쓰는 씬만 TLAS를 요청한다(빌드는 Renderer::Render 시작 시).
		if (useRaytracedShadow)
			RaytracingSceneManager::Get().RequestScene(sceneToQuery, passes.Geometry);
		SubmitLightingPass(cameraBuffer, sceneToQuery, skyData, passes, useRaytracedShadow);
	}
}

Engine::SkyAtmosphereData Engine::DeferredSceneRenderer::BuildSkyAtmosphereData(Engine::Scene* scene)
{
	Engine::SkyAtmosphereData skyData{};
	// 구름은 활성 VolumetricCloudComponent가 있을 때만 그린다.
	skyData.Cloud.EnableClouds = 0u;
	if (!scene) return skyData;

	auto& registry = scene->GetRegistry();

	// 1. SkyAtmosphereComponent 검색 및 태양광/대기 산란 계산
	auto skyView = registry.view<Engine::SkyAtmosphereComponent>();
	for (auto [e, sky] : skyView.each())
	{
		if (!IsComponentEnabled(registry, e, sky)) continue;

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
		if (sky.AutoSyncDirectionalLight)
		{
			auto lightView = registry.view<Engine::LightComponent>();
			for (auto [le, light] : lightView.each())
			{
				if (IsComponentEnabled(registry, le, light) && light.LightType == Engine::ELightType::Directional)
				{
					skyData.SunColor = light.Color;
					skyData.SunIntensity = light.Intensity;
					break;
				}
			}
		}
		break;
	}

	// 2. VolumetricCloudComponent의 구름 설정
	auto cloudView = registry.view<Engine::VolumetricCloudComponent>();
	for (auto [ce, cloud] : cloudView.each())
	{
		if (!IsComponentEnabled(registry, ce, cloud)) continue;
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

void Engine::DeferredSceneRenderer::SubmitLightingPass(const CameraBuffer& cameraBuffer, Scene* targetScene,
	const SkyAtmosphereData& skyData, const DeferredScenePassBindings& passes, bool useRaytracedShadow)
{
	// The first LUT lookup may upload pixels and must finish recording before the lighting pass begins.
	Texture* defaultBRDFLUT = nullptr;
	if (targetScene)
	{
		auto& world = targetScene->GetRegistry();
		auto skyView = world.view<SkyAtmosphereComponent>();
		for (auto [entity, sky] : skyView.each())
		{
			if (!IsComponentEnabled(world, entity, sky))
				continue;
			const auto* prefiltered = sky.PrefilteredMap.Get();
			const auto* irradiance = sky.IrradianceMap.Get();
			if (!prefiltered || !prefiltered->GetRHITexture() || (irradiance && !irradiance->GetRHITexture()))
				continue;

			const auto* lut = sky.BRDFLutMap.Get();
			if (!lut || !lut->GetRHITexture())
				defaultBRDFLUT = IBLManager::Get().GetDefaultBRDFLUT();
			break;
		}
	}

	Engine::Renderer::Get().SubmitCustomCommand(
		[this, cameraBuffer, targetScene, skyData, defaultBRDFLUT, inputs = passes.LightingInputs,
			pointDepth = passes.PointShadowInput, acceptSceneMeshes = passes.AcceptSceneMeshes, viewID = passes.Geometry, useRaytracedShadow,
			raytracedOverrides = m_RaytracedShadowOverrides](f32, Engine::RenderPass*) -> Engine::EResult
		{
			auto* rhi = Engine::Renderer::Get().GetRHI();
			auto& rtMgr = Engine::RenderTargetManager::Get();
			auto* sampler = Engine::SamplerManager::Get().GetDefaultSampler();
			if (!rhi || !sampler || !m_LightingPipeline) return Engine::EResult::Fail;
			CameraBuffer camBuf = cameraBuffer;
			camBuf.time = static_cast<f32>(Engine::TimeManager::Get().GetTotalTime());
			const auto& sourceNames = inputs;
			auto& lightManager = Engine::LightManager::Get();
			bool useTiledCulling = false;
			// 1. 타일 기반 광원 컬링 Compute Shader 디스패치 (메인 씬인 경우에만 그래픽스 상태 확립 전에 선행 실행)
			if (acceptSceneMeshes)
			{
				if (auto* posRT = rtMgr.GetRenderTarget(sourceNames[4]))
				{
					if (auto* posTex = posRT->GetTexture())
						useTiledCulling = Engine::IsSuccess(lightManager.DispatchLightCulling(camBuf, posTex));
				}
			}
			// 2. 조명 그래픽스 파이프라인 바인딩 (모든 그래픽스 리소스 바인딩보다 먼저 실행하여 Root Signature 상태 확립)
			if (Engine::IsFailure(rhi->BindPipeline(m_LightingPipeline)))
				return Engine::EResult::Fail;
			// 3. 카메라 버퍼 바인딩 (slot 0 -> b0, space0)
			if (Engine::IsFailure(rhi->BindConstantBuffer(&camBuf, sizeof(CameraBuffer), 0)))
				return Engine::EResult::Fail;
			Engine::Scene* sceneToQuery = targetScene;
			// 4. FogData 상수 버퍼 바인딩 (slot 2 -> b0, space3)
			Engine::FogData fogData{};
			fogData.FogEnabled = 0u;
			if (sceneToQuery)
			{
				auto fogView = sceneToQuery->GetRegistry().view<Engine::FogComponent>();
				for (auto [e, fog] : fogView.each())
				{
					if (!IsComponentEnabled(sceneToQuery->GetRegistry(), e, fog)) continue;
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
							if (IsComponentEnabled(sceneToQuery->GetRegistry(), se, sky))
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
			// 5. GBuffer 5개 + ShadowDepth 텍스처 바인딩 (t0~t5, space3 및 Root Parameter 8번 Sampler Table 설정)
			for (uint32 i = 0; i < 6; ++i)
			{
				auto* source = rtMgr.GetRenderTarget(sourceNames[i]);
				if (!source || !source->GetTexture()) return Engine::EResult::Fail;
				if (Engine::IsFailure(rhi->BindTextureSampler(source->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + i)))
					return Engine::EResult::Fail;
			}
			// 5-1. Point Light TextureCube 바인딩 (t7, space3)
			if (auto* pointShadowRT = rtMgr.GetRenderTarget(pointDepth))
			{
				if (auto* pointShadowTex = pointShadowRT->GetTexture())
				{
					if (Engine::IsFailure(rhi->BindTextureSampler(pointShadowTex, sampler, MAX_MATERIAL_TEXTURE_SLOTS + 7)))
						return Engine::EResult::Fail;
				}
			}
			// 6. 타일 결과 버퍼 바인딩 (슬롯 1 -> t8, 슬롯 2 -> t9)
			if (auto* gridBuf = lightManager.GetTileLightGridBuffer())
				rhi->BindStorageBuffer(gridBuf, 1);
			if (auto* idxBuf = lightManager.GetTileLightIndexBuffer())
				rhi->BindStorageBuffer(idxBuf, 2);
			// 7. 광원 데이터 버퍼 바인딩 (slot 0 -> t6, space3)
			ShadowLightSet localShadowLights;
			if (!acceptSceneMeshes)
			{
				// [격리] 독립 프리뷰(Thumbnail, MatPreview)는 자체 버퍼를 바인딩하여 씬 뷰포트의 LightManager 오염 방지
				const uint32 frameIndex = rhi->GetFrameResourceIndex();
				constexpr uint32 headerSize = sizeof(LightBufferHeader);
				const uint32 totalSize = headerSize + static_cast<uint32>(sizeof(GPULight)) * MAX_LIGHTS;
				if (!m_LocalLightBuffer[frameIndex])
				{
					std::vector<uint8> zeros(totalSize, 0);
					m_LocalLightBuffer[frameIndex] = rhi->CreateBuffer(zeros.data(), totalSize, sizeof(GPULight), ERHIBufferType::Structured);
				}
				if (m_LocalLightBuffer[frameIndex])
				{
					std::vector<uint8> data(totalSize, 0);
					auto* header = reinterpret_cast<LightBufferHeader*>(data.data());
					auto* gpuLights = reinterpret_cast<GPULight*>(data.data() + headerSize);
					uint32 count = 0;
					if (targetScene)
					{
						auto view = targetScene->GetRegistry().view<Engine::LightComponent>();
						for (const auto entity : view)
						{
							if (count >= MAX_LIGHTS) break;
							const auto& light = view.get<Engine::LightComponent>(entity);
							if (!IsComponentEnabled(targetScene->GetRegistry(), entity, light)) continue;
							const auto optGpuLight = light.GetGPULightDesc();
							if (!optGpuLight.has_value()) continue;
							LightManager::AddShadowLight(localShadowLights, light, *optGpuLight, count);
							gpuLights[count++] = optGpuLight.value();
						}
						auto remaining = std::span<GPULight>{ gpuLights + count, MAX_LIGHTS - count };
						count += Engine::CompositeVFXSystem::CollectFlashLights(*targetScene, remaining);
					}
					header->NumLights = count;
					m_LocalLightBuffer[frameIndex]->SetData(data.data(), totalSize);
					if (Engine::IsFailure(rhi->BindStorageBuffer(m_LocalLightBuffer[frameIndex], 0)))
						return Engine::EResult::Fail;
				}
			}
			else
			{
				// 메인 뷰포트(SceneView, GameView)는 안전하게 전역 LightManager 바인딩
				if (Engine::IsFailure(lightManager.Bind(0)))
					return Engine::EResult::Fail;
			}
			// 7-1. 레이트레이싱 그림자 마스크 (G-Buffer·카메라·광원 버퍼 바인딩을 그대로 사용하는 Compute)
			DeferredLightingPassData lightingData{};
			const ShadowLightSet& shadowLights = acceptSceneMeshes ? lightManager.GetShadowLights() : localShadowLights;
			lightingData.CascadeShadowLight = shadowLights.CascadeLight;
			lightingData.SpotShadowLight = shadowLights.SpotLight;
			lightingData.PointShadowLight = shadowLights.PointLight;
			// 채널은 이번 프레임 광원 버퍼의 인덱스로 다시 정한다.
			const RaytracedShadowAssignment raytracedLights = useRaytracedShadow ?
				AssignRaytracedShadows(shadowLights, raytracedOverrides) : RaytracedShadowAssignment{};
			if (!raytracedLights.Empty())
			{
				// 조명 패스 안에서 실행되므로 추적 비용을 별도 구간으로 잰다(조명 시간에도 포함된다).
				rhi->BeginPassMetrics(sourceNames[4] + L"_RaytracedShadowMask");
				Engine::RHITexture* shadowMask = DispatchRaytracedShadowMask(*rhi, targetScene, viewID, sourceNames[4], raytracedLights);
				rhi->EndPassMetrics();
				// Compute 파이프라인으로 바뀐 상태를 조명 파이프라인으로 되돌린다(같은 루트 시그니처라 그래픽 바인딩은 유지된다).
				if (Engine::IsFailure(rhi->BindPipeline(m_LightingPipeline)))
					return Engine::EResult::Fail;
				if (shadowMask && Engine::IsSuccess(rhi->BindTextureSampler(shadowMask, sampler, RaytracedShadowMaskSlot)))
				{
					lightingData.RaytracedShadowLights = raytracedLights.Lights;
				}
				else if (!m_RaytracedShadowFailureLogged)
				{
					// 섀도맵에서 TLAS 캐스터를 이미 제외했으므로 이번 프레임에는 해당 그림자가 빠진다.
					ENGINE_LOG_ERROR("Raytraced shadow mask was not produced; casters handled by raytracing are missing from shadows this frame.");
					m_RaytracedShadowFailureLogged = true;
				}
			}
			if (lightManager.HasShadowCastingLights())
				lightingData.Shadow = lightManager.GetShadowData(cameraBuffer);
			// 메인 씬만 타일 기반 컬링을 켜고, 독립 프리뷰는 타일 컬링 없이 전체 스튜디오 광원 순회
			lightingData.UseTiledCulling = useTiledCulling ? 1u : 0u;
			// skyData로부터 태양 방향 및 구름, 대기 조명 데이터 주입
			lightingData.Cloud = skyData.Cloud;
			lightingData.SunDirection = skyData.SunDirection;
			lightingData.SkyZenithColor = skyData.ZenithColor;
			lightingData.SkyIntensity = skyData.SkyIntensity;
			// 8. IBL 텍스처 바인딩 (t10, t11, t12)
			if (sceneToQuery)
			{
				auto skyView = sceneToQuery->GetRegistry().view<Engine::SkyAtmosphereComponent>();
				for (auto [e, sky] : skyView.each())
				{
					if (IsComponentEnabled(sceneToQuery->GetRegistry(), e, sky))
					{
						lightingData.IBLIntensity = sky.IBLIntensity;
						lightingData.ShadowAmbientFloor = sky.ShadowAmbientFloor;

						Texture* irrTex = sky.IrradianceMap.IsValid() ? sky.IrradianceMap.Get() : nullptr;
						Texture* prefTex = sky.PrefilteredMap.IsValid() ? sky.PrefilteredMap.Get() : nullptr;
						Texture* lutTex = sky.BRDFLutMap.Get();
						if (!lutTex || !lutTex->GetRHITexture())
							lutTex = defaultBRDFLUT;

						// Prefiltered 큐브맵만 있고 Irradiance가 없으면 Prefiltered로 대체
						if (prefTex && !irrTex)
							irrTex = prefTex;

						if (irrTex && prefTex && lutTex && irrTex->GetRHITexture() && prefTex->GetRHITexture() && lutTex->GetRHITexture())
						{
							rhi->BindTextureSampler(irrTex->GetRHITexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 10);
							rhi->BindTextureSampler(prefTex->GetRHITexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 11);
							rhi->BindTextureSampler(lutTex->GetRHITexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 12);
							lightingData.HasIBLMaps = 1u;
							break;
						}
					}
				}
			}
			auto& rm = Engine::ResourceManager::Get();
			auto shapeHandle = rm.GetResourceHandle<Engine::Texture>(CloudResources::BaseShapeKey);
			auto weatherHandle = rm.GetResourceHandle<Engine::Texture>(CloudResources::WeatherMapKey);
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
			// 9. 조명 패스 상수 버퍼 바인딩 (slot 3 -> b1, space3)
			if (Engine::IsFailure(rhi->BindConstantBuffer(&lightingData, sizeof(lightingData), 3)))
				return Engine::EResult::Fail;
			// 10. 풀스크린 쿼드 조명 합성 실행
			if (Engine::IsFailure(rhi->Draw(3)))
				return Engine::EResult::Fail;
			return Engine::EResult::Success;
		},
		passes.Lighting
	);
}

Engine::ShadowLightSet Engine::DeferredSceneRenderer::CollectViewShadowLights(Engine::Scene* scene, bool usesGlobalLights) const
{
	// 메인 뷰포트는 LightManager가 모은 활성 씬 전체의 광원을, 독립 프리뷰는 자기 씬의 광원만 사용한다.
	std::vector<Engine::Scene*> scenes;
	if (usesGlobalLights && Engine::SceneManager::IsCreated())
		scenes = Engine::SceneManager::Get().GetActiveScenes();
	else
		scenes.push_back(scene);
	return Engine::LightManager::CollectShadowLights(scenes);
}

Engine::RHIPipeline* Engine::DeferredSceneRenderer::GetRaytracedShadowPipeline()
{
	if (m_RaytracedShadowShaderMissing)
		return nullptr;

	// DXR 전용 셰이더라 시작 매니페스트·프로필 공용 로드에 기대지 않고 처음 사용할 때 DXIL을 직접 등록한다.
	auto& resources = Engine::ResourceManager::Get();
	auto handle = resources.GetResourceHandle<Engine::Shader>(RaytracedShadowShaderKey);
	if (!handle.Get())
	{
		ShaderDesc desc{};
		desc.Key = RaytracedShadowShaderKey;
		desc.dxilPath = (PlatformPath::GetEngineResourceRoot() / "Shader" / "bin" / "dxil" / RaytracedShadowShaderFile).wstring();
		desc.Path = desc.dxilPath;
		desc.shaderType = EShaderType::Compute;
		handle = resources.LoadResource<Engine::Shader>(&desc);
	}

	Engine::Shader* shader = handle.Get();
	if (!shader || !shader->GetRHIShader())
	{
		ENGINE_LOG_ERROR("Raytraced shadow shader is not available: {}", WStrToStr(RaytracedShadowShaderFile));
		m_RaytracedShadowShaderMissing = true;
		return nullptr;
	}

	// 셰이더 핫 리로드로 RHI 셰이더가 바뀌어도 맞는 파이프라인을 쓰도록 매번 캐시에서 조회한다.
	RHIPipelineDesc desc{};
	desc.pipelineType = Engine::EPipelineType::Compute;
	desc.computeShader = shader->GetRHIShader();
	return Engine::PipelineManager::Get().GetOrCreatePipeline(desc);
}

Engine::RenderTarget* Engine::DeferredSceneRenderer::EnsureRaytracedShadowMask(const std::wstring& positionTargetName,
	Engine::uint32 width, Engine::uint32 height)
{
	auto& targets = Engine::RenderTargetManager::Get();
	const std::wstring name = positionTargetName + L"_RaytracedShadowMask";
	if (!m_RaytracedShadowMaskName.empty() && m_RaytracedShadowMaskName != name)
		targets.RemoveRenderTarget(m_RaytracedShadowMaskName);
	m_RaytracedShadowMaskName = name;

	Engine::RenderTarget* mask = targets.GetRenderTarget(name);
	if (!mask)
	{
		// 광원 최대 4개의 가시성(0~1)을 채널마다 담는다. 렌더 타깃 목록·진단 뷰어에서 볼 수 있도록 RenderTargetManager에 등록한다.
		Engine::RenderTargetDesc desc{};
		desc.name = name;
		desc.textureDesc.width = width;
		desc.textureDesc.height = height;
		desc.textureDesc.format = Engine::ETextureFormat::R8G8B8A8_UNORM;
		desc.textureDesc.dimension = Engine::ETextureDimension::Texture2D;
		desc.textureDesc.usage = Engine::ETextureUsage::Sampler | Engine::ETextureUsage::ComputeReadWrite;
		mask = targets.CreateRenderTarget(&desc);
	}
	else if (mask->GetWidth() != width || mask->GetHeight() != height)
	{
		if (Engine::IsFailure(mask->Resize(width, height)))
			return nullptr;
	}
	return mask && mask->GetTexture() ? mask : nullptr;
}

Engine::RHITexture* Engine::DeferredSceneRenderer::DispatchRaytracedShadowMask(Engine::RHI& rhi, Engine::Scene* scene,
	RenderPassID viewID, const std::wstring& positionTargetName, const RaytracedShadowAssignment& lights)
{
	if (!scene || !Engine::RaytracingSceneManager::IsCreated())
		return nullptr;

	Engine::RHIAccelerationStructure* topLevel = Engine::RaytracingSceneManager::Get().GetTopLevelAS(scene, viewID);
	Engine::RenderTarget* position = Engine::RenderTargetManager::Get().GetRenderTarget(positionTargetName);
	if (!topLevel || !position || !position->GetTexture())
		return nullptr;

	const Engine::uint32 width = position->GetWidth();
	const Engine::uint32 height = position->GetHeight();
	Engine::RenderTarget* mask = EnsureRaytracedShadowMask(positionTargetName, width, height);
	Engine::RHIPipeline* pipeline = GetRaytracedShadowPipeline();
	if (!mask || !pipeline)
		return nullptr;

	RaytracedShadowParams params{};
	params.Lights = lights.Lights;
	if (Engine::IsFailure(rhi.BindPipeline(pipeline)) ||
		Engine::IsFailure(rhi.BindConstantBuffer(&params, sizeof(params), 3)) ||
		Engine::IsFailure(rhi.BindAccelerationStructure(topLevel, RaytracedShadowTLASSlot)) ||
		Engine::IsFailure(rhi.BindComputeStorageTexture(mask->GetTexture(), 0)) ||
		Engine::IsFailure(rhi.Dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u)))
	{
		return nullptr;
	}
	return mask->GetTexture();
}
