#include "DeferredSceneRenderer.h"

#include "CompositeVFXSystem.h"
#include "FogComponent.h"
#include "LightComponent.h"
#include "LightManager.h"
#include "PipelineManager.h"
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
		f32 _pad = 0.0f;
		vec4 _reserved{ 0.0f };
	};
	static_assert(sizeof(DeferredLightingPassData) == 512);
	static_assert(offsetof(DeferredLightingPassData, Shadow) == 0);
	static_assert(offsetof(DeferredLightingPassData, Cloud) == 336);
	static_assert(offsetof(DeferredLightingPassData, SunDirection) == 464);
	static_assert(offsetof(DeferredLightingPassData, PlanetRadius) == 476);
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

	// 1. ShadowPass & PointShadowPass 카메라 등록
	auto* shadowPass = rpm.GetRenderPassByID(passes.DirectionalShadow);
	if (shadowPass && shadowPass->IsEnabled())
		renderer.RegisterViewportCamera(shadowPass);

	auto* pointShadowPass = rpm.GetRenderPassByID(passes.PointShadow);
	if (pointShadowPass && pointShadowPass->IsEnabled())
		renderer.RegisterViewportCamera(pointShadowPass);

	// 2. GeometryPass 카메라 등록 (씬 메쉬들이 GBuffer에 렌더링됨)
	auto* geomPass = rpm.GetRenderPassByID(passes.Geometry);
	if (geomPass && geomPass->IsEnabled())
		renderer.RegisterViewportCamera(cameraBuffer, geomPass, passes.AcceptSceneMeshes);

	// 3. ForwardPass & ForwardTransparentPass 카메라 등록 (스카이 데이터 상수버퍼 slot 3 주입)
	auto* forwardPass = rpm.GetRenderPassByID(passes.ForwardOpaque);
	if (forwardPass && forwardPass->IsEnabled())
	{
		forwardPass->SetPassConstantStruct(3, skyData);
		renderer.RegisterViewportCamera(cameraBuffer, forwardPass);
	}

	auto* forwardTransPass = rpm.GetRenderPassByID(passes.ForwardTransparent);
	if (forwardTransPass && forwardTransPass->IsEnabled())
		renderer.RegisterViewportCamera(cameraBuffer, forwardTransPass, passes.AcceptSceneMeshes);

	// 4. LightingPass 풀스크린 조명 커스텀 커맨드 제출
	auto* lightingPass = rpm.GetRenderPassByID(passes.Lighting);
	if (lightingPass && lightingPass->IsEnabled())
		SubmitLightingPass(cameraBuffer, sceneToQuery, skyData, passes);
}

Engine::SkyAtmosphereData Engine::DeferredSceneRenderer::BuildSkyAtmosphereData(Engine::Scene* scene)
{
	Engine::SkyAtmosphereData skyData{};
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

void Engine::DeferredSceneRenderer::SubmitLightingPass(const CameraBuffer& cameraBuffer, Scene* targetScene,
	const SkyAtmosphereData& skyData, const DeferredScenePassBindings& passes)
{
	Engine::Renderer::Get().SubmitCustomCommand(
		[this, cameraBuffer, targetScene, skyData, inputs = passes.LightingInputs,
			pointDepth = passes.PointShadowInput, acceptSceneMeshes = passes.AcceptSceneMeshes](f32, Engine::RenderPass*) -> Engine::EResult
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
			if (!acceptSceneMeshes)
			{
				// [격리] 독립 프리뷰(Thumbnail, MatPreview)는 자체 버퍼를 바인딩하여 씬 뷰포트의 LightManager 오염 방지
				const uint32 frameIndex = rhi->GetFrameResourceIndex();
				constexpr uint32 headerSize = sizeof(LightBufferHeader);
				constexpr uint32 totalSize = headerSize + sizeof(GPULight) * MAX_LIGHTS;
				if (!m_LocalLightBuffer[frameIndex])
				{
					std::array<uint8, totalSize> zeros{};
					m_LocalLightBuffer[frameIndex] = rhi->CreateBuffer(zeros.data(), totalSize, sizeof(GPULight), ERHIBufferType::Structured);
				}
				if (m_LocalLightBuffer[frameIndex])
				{
					std::array<uint8, totalSize> data{};
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
							const auto optGpuLight = light.GetGPULightDesc();
							if (optGpuLight.has_value())
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
			DeferredLightingPassData lightingData{};
			if (lightManager.HasShadowCastingLights())
				lightingData.Shadow = lightManager.GetShadowData(0u);
			// 메인 씬만 타일 기반 컬링을 켜고, 독립 프리뷰는 타일 컬링 없이 전체 스튜디오 광원 순회
			lightingData.UseTiledCulling = useTiledCulling ? 1u : 0u;
			// skyData로부터 태양 방향 및 구름 데이터 주입
			lightingData.Cloud = skyData.Cloud;
			lightingData.SunDirection = skyData.SunDirection;
			// 8. IBL 텍스처 바인딩 (t10, t11, t12)
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
