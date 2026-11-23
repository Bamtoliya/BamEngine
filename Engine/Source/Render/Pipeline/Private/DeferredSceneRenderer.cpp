#include "DeferredSceneRenderer.h"
#include "CloudResources.h"
#include "ComponentEnable.h"

#include "IBLManager.h"
#include "CompositeVFXSystem.h"
#include "DecalCollector.h"
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
#include "TemporalAA.h"
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
		uint32 UseRaytracedReflections = 0u;
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

	// 이보다 매끈한 면은 거울 방향만 추적하고 누적하지 않는다.
	constexpr f32 ReflectionMirrorRoughness = 0.1f;
	constexpr f32 ReflectionRoughnessFadeEnd = 0.6f;

	// reflection.hlsli의 ReflectionDenoiseParams (b1, space3)
	struct ReflectionDenoiseParams
	{
		mat4 PrevViewProjection{ 1.0f };
		vec3 PrevCameraPosition{ 0.0f };
		uint32 HistoryState = 0u; // 0 이력 없음, 1 카메라가 움직임, 2 카메라가 멈춤
		uint32 InputTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 HitDistanceTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 HistoryTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 GeometryTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		f32 MirrorRoughness = ReflectionMirrorRoughness;
		f32 RoughnessFadeEnd = ReflectionRoughnessFadeEnd;
		f32 MaxHistory = 64.0f;
		uint32 FrameIndex = 0u;
	};
	static_assert(sizeof(ReflectionDenoiseParams) == 112);

	constexpr const wchar_t* ReflectionTemporalShaderKey = L"Resources/Shader/RaytracedReflectionTemporalRTCS";
	constexpr const wchar_t* ReflectionTemporalShaderFile = L"raytracedReflectionTemporalRTCS.cso";
	constexpr const wchar_t* ReflectionFilterShaderKey = L"Resources/Shader/RaytracedReflectionFilterRTCS";
	constexpr const wchar_t* ReflectionFilterShaderFile = L"raytracedReflectionFilterRTCS.cso";

	// raytracedReflectionRTCS.hlsl의 RaytracedReflectionParams (b1, space3)
	struct RaytracedReflectionParams
	{
		f32 NormalBias = 0.02f;
		f32 DistanceBiasScale = 0.0005f;
		f32 RayMaxDistance = 10000.0f;
		// 아주 거친 면은 표본 하나로 안정되지 않고 환경광과 차이도 작아 추적하지 않는다. 측정 전 시작값이다.
		f32 RoughnessFadeStart = 0.45f;
		f32 RoughnessFadeEnd = ReflectionRoughnessFadeEnd;
		uint32 InstanceTable = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 SunLight = INVALID_SHADOW_LIGHT;
		f32 PixelSpread = 0.001f;
		vec3 SkyZenith{ 0.0f };
		f32 IBLIntensity = 1.0f;
		uint32 HasIBLMaps = 0u;
		uint32 MaxHitLights = 64u;
		uint32 FrameIndex = 0u;
		f32 MirrorRoughness = ReflectionMirrorRoughness;
	};
	static_assert(sizeof(RaytracedReflectionParams) == 64);
	static_assert(offsetof(RaytracedReflectionParams, SkyZenith) == 32);

	constexpr const wchar_t* RaytracedReflectionShaderKey = L"Resources/Shader/RaytracedReflectionRTCS";
	constexpr const wchar_t* RaytracedReflectionShaderFile = L"raytracedReflectionRTCS.cso";
	constexpr Engine::uint32 RaytracedReflectionSlot = MAX_MATERIAL_TEXTURE_SLOTS + 15; // t15, space3 (lightingPS)

	// temporalAAPS.hlsl의 TemporalAAParams (b1, space3)
	struct TemporalAAParams
	{
		mat4 ViewProjection{ 1.0f };
		mat4 PreviousViewProjection{ 1.0f };
		mat4 InvViewProjection{ 1.0f };
		vec3 CameraPosition{ 0.0f };
		uint32 HasHistory = 0u;
		vec2 JitterPixels{ 0.0f };
		// 이번 프레임을 섞는 비율. 작을수록 매끈하지만 잔상이 길다. 측정 전 시작값이다.
		f32 StillBlend = 0.06f;
		f32 MovingBlend = 0.15f;
	};
	static_assert(sizeof(TemporalAAParams) == 224);
	static_assert(offsetof(TemporalAAParams, CameraPosition) == 192);
	static_assert(offsetof(TemporalAAParams, JitterPixels) == 208);

	// temporalAAPS.hlsl의 NO_MOTION
	constexpr f32 NoMotion = 100.0f;

	constexpr const wchar_t* TemporalAAShaderKey = L"Resources/Shader/TemporalAAPS";
	constexpr const wchar_t* TemporalAAShaderFile = L"temporalAAPS.cso";
	constexpr const wchar_t* MotionVectorVSKey = L"Resources/Shader/MotionVectorVS";
	constexpr const wchar_t* MotionVectorVSFile = L"motionVectorVS.cso";
	constexpr const wchar_t* MotionVectorSkinningVSKey = L"Resources/Shader/MotionVectorSkinningVS";
	constexpr const wchar_t* MotionVectorSkinningVSFile = L"motionVectorSkinningVS.cso";
	constexpr const wchar_t* MotionVectorPSKey = L"Resources/Shader/MotionVectorPS";
	constexpr const wchar_t* MotionVectorPSFile = L"motionVectorPS.cso";

	// decal.hlsli의 DecalParams (b1, space3)
	struct DecalParams
	{
		mat4 BoxToWorld{ 1.0f };
		mat4 WorldToBox{ 1.0f };
		vec4 BaseColor{ 1.0f };
		vec3 AxisX{ 1.0f, 0.0f, 0.0f };
		f32 Opacity = 1.0f;
		vec3 AxisY{ 0.0f, 1.0f, 0.0f };
		f32 NormalOpacity = 1.0f;
		vec3 AxisZ{ 0.0f, 0.0f, 1.0f };
		f32 PBROpacity = 0.0f;
		vec2 AngleFadeRange{ 0.0f, 1.0f };
		f32 DepthFade = 0.1f;
		f32 Roughness = 0.5f;
		f32 Metallic = 0.0f;
		uint32 TextureFlags = 0u;
		vec2 Padding{ 0.0f };
	};
	static_assert(sizeof(DecalParams) == 224);
	static_assert(offsetof(DecalParams, BaseColor) == 128);
	static_assert(offsetof(DecalParams, AxisZ) == 176);
	static_assert(offsetof(DecalParams, AngleFadeRange) == 192);
	static_assert(offsetof(DecalParams, TextureFlags) == 212);

	// decal.hlsli의 DECAL_HAS_*_TEXTURE
	constexpr Engine::uint32 DecalHasBaseColorTexture = 1u << 0u;
	constexpr Engine::uint32 DecalHasNormalTexture = 1u << 1u;
	constexpr Engine::uint32 DecalHasPBRTexture = 1u << 2u;

	constexpr const wchar_t* DecalVSKey = L"Resources/Shader/DecalVS";
	constexpr const wchar_t* DecalVSFile = L"decalVS.cso";
	constexpr const wchar_t* DecalPSKey = L"Resources/Shader/DecalPS";
	constexpr const wchar_t* DecalPSFile = L"decalPS.cso";

	vec3 NormalizedAxis(const vec3& axis)
	{
		const f32 lengthSquared = glm::dot(axis, axis);
		return lengthSquared > 1e-12f ? axis / std::sqrt(lengthSquared) : vec3(0.0f);
	}
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
	for (RenderPassID* pass : { &m_TemporalPassID, &m_MotionPassID, &m_DecalPassID })
	{
		if (*pass != INVALID_PASS_ID && Engine::RenderPassManager::IsCreated())
			Engine::RenderPassManager::Get().RemoveRenderPass(*pass);
		*pass = INVALID_PASS_ID;
	}
	const auto removeTarget = [](std::wstring& name)
		{
			if (!name.empty() && Engine::RenderTargetManager::IsCreated())
				Engine::RenderTargetManager::Get().RemoveRenderTarget(name);
			name.clear();
		};
	removeTarget(m_RaytracedShadowMaskName);
	removeTarget(m_DecalNormalCopyName);
	m_Decals.clear();
	for (auto& name : m_ReflectionTargetNames)
		removeTarget(name);
	m_ReflectionHistory.Reset();
	for (auto& name : m_TemporalTargetNames)
		removeTarget(name);
	m_TemporalHistory.Reset();
	m_MotionHistory.Reset();
	m_TemporalAAActive = false;
	m_TemporalJitterNDC = vec2(0.0f);
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

	// 0-1. 레이트레이싱 반사 사용 여부. 독립 프리뷰는 제외한다.
	const auto* rhi = renderer.GetRHI();
	const bool useRaytracedReflection = sceneToQuery && lightingPass && lightingPass->IsEnabled() && passes.AcceptSceneMeshes &&
		ResolveRaytracedShadow(false, m_RaytracedReflectionOverrides) && rhi && rhi->GetCapabilities().SupportsBindlessResources &&
		Engine::RaytracingSceneManager::IsCreated() && Engine::RaytracingSceneManager::Get().IsSupported() && GetRaytracedReflectionPipeline();
	m_RaytracedReflectionActive = useRaytracedReflection;
	// 껐다 켜면 오래된 결과를 이어 쓰지 않는다.
	if (!useRaytracedReflection)
		m_ReflectionHistory.Reset();

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

	// 1-1. TAA를 쓰면 장면을 그리는 패스와 조명만 지터 카메라를 쓴다. 그림자·반사·기즈모는 원래 카메라를 쓴다.
	const CameraBuffer sceneCamera = PrepareTemporalAA(cameraBuffer, sceneToQuery, passes);

	// 2. GeometryPass 카메라 등록 (씬 메쉬들이 GBuffer에 렌더링됨)
	auto* geomPass = rpm.GetRenderPassByID(passes.Geometry);
	if (geomPass && geomPass->IsEnabled())
		renderer.RegisterViewportCamera(sceneCamera, geomPass, passes.AcceptSceneMeshes, passes.Geometry);

	// 2-1. 데칼: G-Buffer 뒤·조명 앞. G-Buffer 위치를 화면 좌표로 다시 찾으므로 G-Buffer를 그린 카메라를 쓴다.
	SubmitDecalPass(sceneCamera, sceneToQuery, passes);

	// 3. ForwardPass & ForwardTransparentPass 카메라 등록 (스카이 데이터 상수버퍼 slot 3 주입)
	auto* forwardPass = rpm.GetRenderPassByID(passes.ForwardOpaque);
	if (forwardPass && forwardPass->IsEnabled())
	{
		forwardPass->SetPassConstantStruct(3, skyData);
		renderer.RegisterViewportCamera(sceneCamera, forwardPass, passes.AcceptSceneMeshes, passes.Geometry);
	}

	auto* forwardTransPass = rpm.GetRenderPassByID(passes.ForwardTransparent);
	if (forwardTransPass && forwardTransPass->IsEnabled())
	{
		forwardTransPass->SetPassTexture(MAX_MATERIAL_TEXTURE_SLOTS + 1, passes.LightingInputs[1]);
		forwardTransPass->SetPassTexture(MAX_MATERIAL_TEXTURE_SLOTS + 4, passes.LightingInputs[4]);
		renderer.RegisterViewportCamera(sceneCamera, forwardTransPass, passes.AcceptSceneMeshes, passes.Geometry);
	}

	// 4. LightingPass 풀스크린 조명 커스텀 커맨드 제출
	if (lightingPass && lightingPass->IsEnabled())
	{
		// 레이트레이싱 그림자나 반사를 쓰는 씬만 TLAS를 요청한다(빌드는 Renderer::Render 시작 시).
		if (useRaytracedShadow || useRaytracedReflection)
			RaytracingSceneManager::Get().RequestScene(sceneToQuery, passes.Geometry, useRaytracedReflection);
		SubmitLightingPass(cameraBuffer, sceneCamera, sceneToQuery, skyData, passes, useRaytracedShadow, useRaytracedReflection);
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

void Engine::DeferredSceneRenderer::SubmitLightingPass(const CameraBuffer& cameraBuffer, const CameraBuffer& sceneCamera, Scene* targetScene,
	const SkyAtmosphereData& skyData, const DeferredScenePassBindings& passes, bool useRaytracedShadow, bool useRaytracedReflection)
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
		[this, cameraBuffer, sceneCamera, targetScene, skyData, defaultBRDFLUT, inputs = passes.LightingInputs,
			pointDepth = passes.PointShadowInput, acceptSceneMeshes = passes.AcceptSceneMeshes, viewID = passes.Geometry, useRaytracedShadow, useRaytracedReflection,
			raytracedOverrides = m_RaytracedShadowOverrides](f32, Engine::RenderPass*) -> Engine::EResult
		{
			auto* rhi = Engine::Renderer::Get().GetRHI();
			auto& rtMgr = Engine::RenderTargetManager::Get();
			auto* sampler = Engine::SamplerManager::Get().GetDefaultSampler();
			if (!rhi || !sampler || !m_LightingPipeline) return Engine::EResult::Fail;
			CameraBuffer camBuf = cameraBuffer;
			camBuf.time = static_cast<f32>(Engine::TimeManager::Get().GetTotalTime());
			CameraBuffer sceneCamBuf = sceneCamera;
			sceneCamBuf.time = camBuf.time;
			const auto& sourceNames = inputs;
			auto& lightManager = Engine::LightManager::Get();
			bool useTiledCulling = false;
			// 1. 타일 기반 광원 컬링 Compute Shader 디스패치 (메인 씬인 경우에만 그래픽스 상태 확립 전에 선행 실행)
			if (acceptSceneMeshes)
			{
				if (auto* posRT = rtMgr.GetRenderTarget(sourceNames[4]))
				{
					if (auto* posTex = posRT->GetTexture())
						useTiledCulling = Engine::IsSuccess(lightManager.DispatchLightCulling(sceneCamBuf, posTex));
				}
			}
			// 2. 조명 그래픽스 파이프라인 바인딩 (모든 그래픽스 리소스 바인딩보다 먼저 실행하여 Root Signature 상태 확립)
			if (Engine::IsFailure(rhi->BindPipeline(m_LightingPipeline)))
				return Engine::EResult::Fail;
			// 3. 카메라 버퍼 바인딩 (slot 0 -> b0, space0). 반사 누적은 지터가 없는 카메라로 이전 화면을 찾는다.
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
			// 8-1. 레이트레이싱 반사 (G-Buffer·광원·환경광 바인딩을 그대로 사용하는 Compute)
			if (useRaytracedReflection)
			{
				// 이전 프레임의 출력이 SRV로 남아 있으면 같은 텍스처에 쓸 수 없다.
				(void)rhi->BindTexture(nullptr, RaytracedReflectionSlot);
				RaytracedReflectionInputs reflectionInputs;
				reflectionInputs.SunLight = shadowLights.CascadeLight;
				reflectionInputs.SkyZenith = glm::max(lightingData.SkyZenithColor, vec3(0.05f)) * glm::max(lightingData.SkyIntensity, 0.2f);
				reflectionInputs.IBLIntensity = lightingData.IBLIntensity;
				reflectionInputs.HasIBLMaps = lightingData.HasIBLMaps != 0u;
				rhi->BeginPassMetrics(sourceNames[4] + L"_RaytracedReflection");
				Engine::RHITexture* reflection = DispatchRaytracedReflection(*rhi, targetScene, viewID, sourceNames[4], camBuf, reflectionInputs);
				rhi->EndPassMetrics();
				if (Engine::IsFailure(rhi->BindPipeline(m_LightingPipeline)))
					return Engine::EResult::Fail;
				if (reflection && Engine::IsSuccess(rhi->BindTextureSampler(reflection, sampler, RaytracedReflectionSlot)))
				{
					lightingData.UseRaytracedReflections = 1u;
				}
				else if (!m_RaytracedReflectionFailureLogged)
				{
					ENGINE_LOG_ERROR("Raytraced reflections were not produced; environment reflections are used instead.");
					m_RaytracedReflectionFailureLogged = true;
				}
			}
			// 9. 조명 패스 상수 버퍼 바인딩 (slot 3 -> b1, space3)
			if (Engine::IsFailure(rhi->BindConstantBuffer(&lightingData, sizeof(lightingData), 3)))
				return Engine::EResult::Fail;
			// 10. 조명은 접촉 그림자에서 G-Buffer 위치를 화면 좌표로 다시 찾으므로 G-Buffer를 그린 카메라로 바꾼다.
			if (Engine::IsFailure(rhi->BindConstantBuffer(&sceneCamBuf, sizeof(CameraBuffer), 0)))
				return Engine::EResult::Fail;
			// 11. 풀스크린 쿼드 조명 합성 실행
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
	return GetRaytracingPipeline(RaytracedShadowShaderKey, RaytracedShadowShaderFile, m_RaytracedShadowShaderMissing);
}

Engine::RHIPipeline* Engine::DeferredSceneRenderer::GetRaytracedReflectionPipeline()
{
	return GetRaytracingPipeline(RaytracedReflectionShaderKey, RaytracedReflectionShaderFile, m_RaytracedReflectionShaderMissing);
}

Engine::RHIPipeline* Engine::DeferredSceneRenderer::GetRaytracingPipeline(const wchar_t* shaderKey, const wchar_t* shaderFile, bool& shaderMissing)
{
	Engine::Shader* shader = GetOwnedShader(shaderKey, shaderFile, EShaderType::Compute, shaderMissing);
	if (!shader)
		return nullptr;

	// 셰이더 핫 리로드로 RHI 셰이더가 바뀌어도 맞는 파이프라인을 쓰도록 매번 캐시에서 조회한다.
	RHIPipelineDesc desc{};
	desc.pipelineType = Engine::EPipelineType::Compute;
	desc.computeShader = shader->GetRHIShader();
	return Engine::PipelineManager::Get().GetOrCreatePipeline(desc);
}

Engine::Shader* Engine::DeferredSceneRenderer::GetOwnedShader(const wchar_t* shaderKey, const wchar_t* shaderFile, EShaderType type, bool& shaderMissing)
{
	if (shaderMissing)
		return nullptr;

	// DirectX12 전용 셰이더라 시작 매니페스트·프로필 공용 로드에 기대지 않고 처음 사용할 때 DXIL을 직접 등록한다.
	auto& resources = Engine::ResourceManager::Get();
	auto handle = resources.GetResourceHandle<Engine::Shader>(shaderKey);
	if (!handle.Get())
	{
		ShaderDesc desc{};
		desc.Key = shaderKey;
		desc.dxilPath = (PlatformPath::GetEngineResourceRoot() / "Shader" / "bin" / "dxil" / shaderFile).wstring();
		desc.Path = desc.dxilPath;
		desc.shaderType = type;
		handle = resources.LoadResource<Engine::Shader>(&desc);
	}

	Engine::Shader* shader = handle.Get();
	if (!shader || !shader->GetRHIShader())
	{
		ENGINE_LOG_ERROR("Shader is not available: {}", WStrToStr(shaderFile));
		shaderMissing = true;
		return nullptr;
	}
	return shader;
}

Engine::RenderTarget* Engine::DeferredSceneRenderer::EnsureOwnedTarget(const std::wstring& name, ETextureFormat format,
	Engine::uint32 width, Engine::uint32 height, std::wstring& ownedName, ETextureUsage usage, const vec4& clearColor)
{
	auto& targets = Engine::RenderTargetManager::Get();
	if (!ownedName.empty() && ownedName != name)
		targets.RemoveRenderTarget(ownedName);
	ownedName = name;

	Engine::RenderTarget* target = targets.GetRenderTarget(name);
	if (!target)
	{
		// 렌더 타깃 목록·진단 뷰어에서 볼 수 있도록 RenderTargetManager에 등록한다.
		Engine::RenderTargetDesc desc{};
		desc.name = name;
		desc.textureDesc.width = width;
		desc.textureDesc.height = height;
		desc.textureDesc.format = format;
		desc.textureDesc.dimension = Engine::ETextureDimension::Texture2D;
		desc.textureDesc.usage = usage;
		desc.textureDesc.clearColor = clearColor;
		target = targets.CreateRenderTarget(&desc);
	}
	else if (target->GetWidth() != width || target->GetHeight() != height)
	{
		if (Engine::IsFailure(target->Resize(width, height)))
			return nullptr;
	}
	return target && target->GetTexture() ? target : nullptr;
}

void Engine::DeferredSceneRenderer::SubmitDecalPass(const CameraBuffer& sceneCamera, Engine::Scene* scene, const DeferredScenePassBindings& passes)
{
	auto& rpm = Engine::RenderPassManager::Get();
	auto& renderer = Engine::Renderer::Get();
	auto* decalPass = m_DecalPassID != INVALID_PASS_ID ? rpm.GetRenderPassByID(m_DecalPassID) : nullptr;
	const auto disable = [&]()
		{
			if (decalPass)
				decalPass->SetEnabled(false);
		};

	const auto* geometry = rpm.GetRenderPassByID(passes.Geometry);
	if (!geometry || !geometry->IsEnabled() || renderer.GetRHIType() != ERHIType::DirectX12 || m_DecalShaderMissing)
		return disable();

	// 메인 뷰포트는 활성 씬 전체, 독립 프리뷰는 자기 씬의 데칼만 모은다(그림자 광원과 같은 규칙).
	m_Decals.clear();
	std::vector<Engine::Scene*> scenes;
	if (passes.AcceptSceneMeshes && Engine::SceneManager::IsCreated())
		scenes = Engine::SceneManager::Get().GetActiveScenes();
	else if (scene)
		scenes.push_back(scene);
	for (Engine::Scene* source : scenes)
	{
		if (!source) continue;
		CollectDecalComponents(*source, m_Decals);
		CompositeVFXSystem::CollectDecals(*source, m_Decals);
	}
	if (m_Decals.empty())
		return disable();
	if (m_Decals.size() > MaxDecalsPerView && !m_DecalLimitLogged)
	{
		ENGINE_LOG_WARN("More than {} decals are in view; decals with the highest sort orders are skipped.", MaxDecalsPerView);
		m_DecalLimitLogged = true;
	}
	SortDecals(m_Decals);

	auto& targets = Engine::RenderTargetManager::Get();
	const std::wstring& diffuseName = passes.LightingInputs[0];
	const std::wstring& normalName = passes.LightingInputs[1];
	const std::wstring& pbrName = passes.LightingInputs[2];
	const std::wstring& positionName = passes.LightingInputs[4];
	const auto* diffuse = targets.GetRenderTarget(diffuseName);
	const auto* normal = targets.GetRenderTarget(normalName);
	const auto* pbr = targets.GetRenderTarget(pbrName);
	if (!diffuse || !normal || !pbr || !targets.GetRenderTarget(positionName))
		return disable();
	if (!EnsureOwnedTarget(normalName + L"_DecalNormalCopy", normal->GetFormat(), normal->GetWidth(), normal->GetHeight(), m_DecalNormalCopyName,
		ETextureUsage::RenderTarget | ETextureUsage::Sampler))
		return disable();

	Engine::Shader* vs = GetOwnedShader(DecalVSKey, DecalVSFile, EShaderType::Vertex, m_DecalShaderMissing);
	Engine::Shader* ps = GetOwnedShader(DecalPSKey, DecalPSFile, EShaderType::Fragment, m_DecalShaderMissing);
	if (!vs || !ps)
		return disable();

	// 같은 우선순위에서는 나중에 등록한 패스가 뒤에 실행된다: G-Buffer → 데칼 → 조명(더 큰 우선순위)
	if (!decalPass)
	{
		RenderPassDesc desc{};
		desc.name = geometry->GetName() + L"_DecalPass";
		desc.priority = geometry->GetPriority();
		desc.loadOperation = ERenderPassLoadOperation::RPLO_Load;
		desc.passType = ERenderPassType::PostProcess;
		desc.acceptedBlendModes = EBlendMode::None;
		m_DecalPassID = rpm.RegisterRenderPass(std::move(desc));
		decalPass = m_DecalPassID != INVALID_PASS_ID ? rpm.GetRenderPassByID(m_DecalPassID) : nullptr;
		if (!decalPass)
			return;
	}
	decalPass->SetColorAttachments({ diffuseName, normalName, pbrName });
	decalPass->SetPrePassCopy(normalName, m_DecalNormalCopyName);
	decalPass->SetEnabled(true);

	// 각 타깃의 RGB만 섞는다. a에는 하늘 판정(디퓨즈), 셰이딩 플래그(법선), AO(PBR)가 있다.
	RHIPipelineDesc pipelineDesc{};
	pipelineDesc.pipelineType = EPipelineType::Graphics;
	pipelineDesc.vertexShader = vs->GetRHIShader();
	pipelineDesc.pixelShader = ps->GetRHIShader();
	pipelineDesc.colorAttachmentCount = 3;
	pipelineDesc.colorAttachmentFormats[0] = diffuse->GetFormat();
	pipelineDesc.colorAttachmentFormats[1] = normal->GetFormat();
	pipelineDesc.colorAttachmentFormats[2] = pbr->GetFormat();
	pipelineDesc.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
	pipelineDesc.depthStencilState.depthTestEnable = false;
	pipelineDesc.depthStencilState.depthWriteEnable = false;
	pipelineDesc.topology = ETopology::TriangleList;
	pipelineDesc.frontFace = EFrontFace::Clockwise;
	pipelineDesc.blendState.enable = true;
	pipelineDesc.blendState.srcColor = EBlendFactor::SrcAlpha;
	pipelineDesc.blendState.dstColor = EBlendFactor::InvSrcAlpha;
	pipelineDesc.blendState.colorBlendOp = EBlendOp::Add;
	pipelineDesc.blendState.srcAlpha = EBlendFactor::Zero;
	pipelineDesc.blendState.dstAlpha = EBlendFactor::One;
	pipelineDesc.blendState.alphaBlendOp = EBlendOp::Add;
	pipelineDesc.blendState.enableColorWriteMask = true;
	pipelineDesc.blendState.colorWriteMask = EColorChannel::RGB;
	// 바깥면을 버리고 안쪽 면만 그려 카메라가 상자 안에 있어도 한 번씩만 섞는다. 뒤집힌 상자는 반대쪽을 버린다.
	pipelineDesc.cullMode = ECullMode::Front;
	RHIPipeline* pipeline = Engine::PipelineManager::Get().GetOrCreatePipeline(pipelineDesc);
	pipelineDesc.cullMode = ECullMode::Back;
	RHIPipeline* mirroredPipeline = Engine::PipelineManager::Get().GetOrCreatePipeline(pipelineDesc);
	if (!pipeline || !mirroredPipeline)
		return disable();

	renderer.SubmitCustomCommand([this, pipeline, mirroredPipeline, sceneCamera, decals = m_Decals, positionName,
		normalCopyName = m_DecalNormalCopyName](f32, RenderPass*) -> EResult
		{
			auto* rhi = Renderer::Get().GetRHI();
			auto& targetManager = RenderTargetManager::Get();
			const auto* position = targetManager.GetRenderTarget(positionName);
			const auto* normalCopy = targetManager.GetRenderTarget(normalCopyName);
			SamplerDesc samplerDesc{};
			samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = ESamplerAddressMode::Clamp;
			auto* sampler = SamplerManager::Get().GetOrCreateSampler(samplerDesc);
			auto white = ResourceManager::Get().GetResourceHandle<Texture>(L"Engine://Texture/white1x1.png");
			if (!white.Get())
				white = ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png");
			RHITexture* fallback = white.Get() ? white->GetRHITexture() : nullptr;

			CameraBuffer camera = sceneCamera;
			camera.time = static_cast<f32>(TimeManager::Get().GetTotalTime());
			RHIPipeline* bound = nullptr;
			// 파이프라인을 바꾼 뒤에도 공통 입력이 묶여 있도록 다시 묶는다.
			const auto bindPipeline = [&](RHIPipeline* next) -> bool
				{
					if (next == bound) return true;
					bound = next;
					return IsSuccess(rhi->BindPipeline(next)) && IsSuccess(rhi->BindConstantBuffer(&camera, sizeof(CameraBuffer), 0)) &&
						IsSuccess(rhi->BindTextureSampler(position->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 0)) &&
						IsSuccess(rhi->BindTextureSampler(normalCopy->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + 1));
				};

			bool succeeded = rhi && sampler && fallback && position && position->GetTexture() && normalCopy && normalCopy->GetTexture();
			for (const DecalDrawData& decal : decals)
			{
				if (!succeeded) break;
				DecalParams params{};
				if (!TryGetDecalWorldToBox(decal.BoxToWorld, params.WorldToBox))
					continue;
				params.BoxToWorld = decal.BoxToWorld;
				params.BaseColor = decal.BaseColor;
				params.AxisX = NormalizedAxis(vec3(decal.BoxToWorld[0]));
				params.AxisY = NormalizedAxis(vec3(decal.BoxToWorld[1]));
				params.AxisZ = NormalizedAxis(vec3(decal.BoxToWorld[2]));
				params.Opacity = decal.Opacity;
				params.NormalOpacity = decal.NormalOpacity;
				params.PBROpacity = decal.PBROpacity;
				params.AngleFadeRange = GetDecalAngleFadeRange(decal.MaxAngle);
				params.DepthFade = decal.DepthFade;
				params.Roughness = decal.Roughness;
				params.Metallic = decal.Metallic;

				const Texture* textures[3] = { decal.BaseColorTexture, decal.NormalTexture, decal.PBRTexture };
				const uint32 textureFlags[3] = { DecalHasBaseColorTexture, DecalHasNormalTexture, DecalHasPBRTexture };
				succeeded = bindPipeline(IsDecalBoxMirrored(decal.BoxToWorld) ? mirroredPipeline : pipeline);
				for (uint32 slot = 0; succeeded && slot < 3; ++slot)
				{
					RHITexture* texture = textures[slot] ? textures[slot]->GetRHITexture() : nullptr;
					if (texture)
						params.TextureFlags |= textureFlags[slot];
					succeeded = IsSuccess(rhi->BindTextureSampler(texture ? texture : fallback, sampler, slot));
				}
				succeeded = succeeded && IsSuccess(rhi->BindConstantBuffer(&params, sizeof(params), 3)) && IsSuccess(rhi->Draw(36));
			}
			if (!succeeded && !m_DecalFailureLogged)
			{
				ENGINE_LOG_ERROR("Decals were not drawn this frame.");
				m_DecalFailureLogged = true;
			}
			return EResult::Success;
		}, m_DecalPassID);
}

bool Engine::DeferredSceneRenderer::EnsureTemporalAAPasses(RenderPassID transparentPass, const std::wstring& prefix)
{
	auto& rpm = Engine::RenderPassManager::Get();
	const auto* transparent = rpm.GetRenderPassByID(transparentPass);
	if (!transparent)
		return false;

	// 같은 우선순위에서는 나중에 등록한 패스가 뒤에 실행된다: 반투명 → 모션 벡터 → TAA → 후처리
	const auto ensure = [&](RenderPassID& id, const wchar_t* suffix, ERenderPassLoadOperation load)
		{
			if (id == INVALID_PASS_ID)
			{
				RenderPassDesc desc{};
				desc.name = prefix + suffix;
				desc.priority = transparent->GetPriority();
				desc.loadOperation = load;
				desc.passType = ERenderPassType::PostProcess;
				desc.acceptedBlendModes = EBlendMode::None;
				id = rpm.RegisterRenderPass(std::move(desc));
			}
			return id != INVALID_PASS_ID;
		};
	return ensure(m_MotionPassID, L"_MotionVectorPass", ERenderPassLoadOperation::RPLO_Clear) &&
		ensure(m_TemporalPassID, L"_TemporalAAPass", ERenderPassLoadOperation::RPLO_Load);
}

CameraBuffer Engine::DeferredSceneRenderer::PrepareTemporalAA(const CameraBuffer& camera, Engine::Scene* scene, const DeferredScenePassBindings& passes)
{
	// 쓰지 않는 동안의 이력은 다시 켤 때 이어 쓰지 않는다.
	const auto disable = [&]() -> const CameraBuffer&
		{
			m_TemporalAAActive = false;
			m_TemporalJitterNDC = vec2(0.0f);
			m_TemporalHistory.Reset();
			m_MotionHistory.Reset();
			return camera;
		};

	auto& rpm = Engine::RenderPassManager::Get();
	auto& renderer = Engine::Renderer::Get();
	auto* transparent = rpm.GetRenderPassByID(passes.ForwardTransparent);
	const auto* geometry = rpm.GetRenderPassByID(passes.Geometry);
	const auto* lighting = rpm.GetRenderPassByID(passes.Lighting);
	if (!scene || !passes.AcceptSceneMeshes || !ResolveRaytracedShadow(false, m_TemporalAAOverrides) || !SupportsTemporalAA(camera) ||
		renderer.GetRHIType() != ERHIType::DirectX12 || !geometry || !geometry->IsEnabled() || !lighting || !lighting->IsEnabled() ||
		!transparent || transparent->GetRenderTargetCount() == 0u || !transparent->HasPrePassCopy())
		return disable();

	// 반투명 패스가 쓰고 난 장면 복사 타깃을 이번 프레임의 입력으로 다시 쓴다.
	const std::wstring colorName = transparent->GetRenderTargetName(0);
	const std::wstring copyName = transparent->GetPrePassCopyDest();
	auto& targets = Engine::RenderTargetManager::Get();
	const auto* color = targets.GetRenderTarget(colorName);
	if (!color || !targets.GetRenderTarget(copyName) || !targets.GetRenderTarget(passes.LightingInputs[0]) || !targets.GetRenderTarget(passes.LightingInputs[4]))
		return disable();

	Engine::Shader* resolvePS = GetOwnedShader(TemporalAAShaderKey, TemporalAAShaderFile, EShaderType::Fragment, m_TemporalShaderMissing);
	Engine::Shader* motionVS = GetOwnedShader(MotionVectorVSKey, MotionVectorVSFile, EShaderType::Vertex, m_TemporalShaderMissing);
	Engine::Shader* motionSkinningVS = GetOwnedShader(MotionVectorSkinningVSKey, MotionVectorSkinningVSFile, EShaderType::Vertex, m_TemporalShaderMissing);
	Engine::Shader* motionPS = GetOwnedShader(MotionVectorPSKey, MotionVectorPSFile, EShaderType::Fragment, m_TemporalShaderMissing);
	const auto fullscreenVS = Engine::ResourceManager::Get().GetResourceHandle<Engine::Shader>(L"FullscreenQuadVS");
	if (!resolvePS || !motionVS || !motionSkinningVS || !motionPS || !fullscreenVS.Get() || !fullscreenVS->GetRHIShader())
		return disable();

	const uint32 width = color->GetWidth();
	const uint32 height = color->GetHeight();
	struct TargetInfo { const wchar_t* Suffix; ETextureFormat Format; vec4 ClearColor; };
	const TargetInfo targetInfos[TemporalTargetCount] = {
		{ L"_MotionVectors", ETextureFormat::R16G16_FLOAT, vec4(NoMotion, NoMotion, 0.0f, 0.0f) },
		{ L"_TemporalColor0", ETextureFormat::R16G16B16A16_FLOAT, vec4(0.0f) },
		{ L"_TemporalColor1", ETextureFormat::R16G16B16A16_FLOAT, vec4(0.0f) },
	};
	for (int index = 0; index < TemporalTargetCount; ++index)
	{
		if (!EnsureOwnedTarget(colorName + targetInfos[index].Suffix, targetInfos[index].Format, width, height, m_TemporalTargetNames[index],
			ETextureUsage::RenderTarget | ETextureUsage::Sampler, targetInfos[index].ClearColor))
			return disable();
	}
	if (!EnsureTemporalAAPasses(passes.ForwardTransparent, colorName))
		return disable();
	auto* motionPass = rpm.GetRenderPassByID(m_MotionPassID);
	auto* temporalPass = rpm.GetRenderPassByID(m_TemporalPassID);
	if (!motionPass || !temporalPass)
		return disable();

	RHIPipelineDesc pipelineDesc{};
	pipelineDesc.pipelineType = EPipelineType::Graphics;
	pipelineDesc.vertexShader = fullscreenVS->GetRHIShader();
	pipelineDesc.pixelShader = resolvePS->GetRHIShader();
	pipelineDesc.colorAttachmentCount = 2;
	pipelineDesc.colorAttachmentFormats[0] = color->GetFormat();
	pipelineDesc.colorAttachmentFormats[1] = ETextureFormat::R16G16B16A16_FLOAT;
	pipelineDesc.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
	pipelineDesc.depthStencilState.depthTestEnable = false;
	pipelineDesc.depthStencilState.depthWriteEnable = false;
	pipelineDesc.topology = ETopology::TriangleList;
	pipelineDesc.cullMode = ECullMode::None;
	RHIPipeline* pipeline = Engine::PipelineManager::Get().GetOrCreatePipeline(pipelineDesc);
	if (!pipeline)
		return disable();

	const auto frame = m_TemporalHistory.Begin(camera, scene, width, height);
	const vec2 jitter = TemporalJitterPixels(frame.FrameIndex);
	const CameraBuffer jittered = MakeJitteredCamera(camera, jitter, width, height);
	const std::wstring historyName = m_TemporalTargetNames[TemporalColor0 + frame.ReadIndex];
	const std::wstring writeName = m_TemporalTargetNames[TemporalColor0 + frame.WriteIndex];
	motionPass->SetColorAttachments({ m_TemporalTargetNames[TemporalMotion] });
	temporalPass->SetColorAttachments({ colorName, writeName });
	temporalPass->SetPrePassCopy(colorName, copyName);

	// 1. 움직인 물체의 모션 벡터. 실패해도 카메라 이동만 반영하고 계속한다.
	MotionVectorInfo motionInfo;
	motionInfo.geometryPass = passes.Geometry;
	motionInfo.jitteredViewProjection = jittered.viewProjMatrix;
	motionInfo.viewProjection = camera.viewProjMatrix;
	motionInfo.previousViewProjection = frame.PrevViewProjection;
	motionInfo.staticVertexShader = motionVS;
	motionInfo.skinnedVertexShader = motionSkinningVS;
	motionInfo.pixelShader = motionPS;
	motionInfo.draw = frame.Valid;
	renderer.SubmitCustomCommand([this, motionInfo, positionName = passes.LightingInputs[4]](f32, RenderPass* pass) mutable -> EResult
		{
			const auto* position = RenderTargetManager::Get().GetRenderTarget(positionName);
			motionInfo.position = position ? position->GetTexture() : nullptr;
			if (IsFailure(Renderer::Get().RenderMotionVectors(motionInfo, m_MotionHistory, pass)) && !m_TemporalFailureLogged)
			{
				ENGINE_LOG_ERROR("Motion vectors were not drawn; moving objects are treated as static by temporal anti-aliasing.");
				m_TemporalFailureLogged = true;
			}
			return EResult::Success;
		}, m_MotionPassID);

	// 2. 이전 결과와 섞어 장면 색과 다음 프레임의 이력에 함께 쓴다.
	TemporalAAParams params{};
	params.ViewProjection = camera.viewProjMatrix;
	params.PreviousViewProjection = frame.PrevViewProjection;
	params.InvViewProjection = camera.invViewProjMatrix;
	params.CameraPosition = camera.cameraPosition;
	params.HasHistory = frame.Valid ? 1u : 0u;
	params.JitterPixels = jitter;
	const std::array<std::wstring, 5> inputs{ passes.LightingInputs[0], copyName, historyName, m_TemporalTargetNames[TemporalMotion], passes.LightingInputs[4] };
	renderer.SubmitCustomCommand([this, pipeline, params, inputs](f32, RenderPass*) -> EResult
		{
			auto* rhi = Renderer::Get().GetRHI();
			SamplerDesc samplerDesc{};
			samplerDesc.AddressU = samplerDesc.AddressV = samplerDesc.AddressW = ESamplerAddressMode::Clamp;
			auto* sampler = SamplerManager::Get().GetOrCreateSampler(samplerDesc);
			bool succeeded = rhi && sampler && IsSuccess(rhi->BindPipeline(pipeline)) && IsSuccess(rhi->BindConstantBuffer(&params, sizeof(params), 3));
			for (uint32 index = 0; succeeded && index < inputs.size(); ++index)
			{
				const auto* input = RenderTargetManager::Get().GetRenderTarget(inputs[index]);
				succeeded = input && input->GetTexture() && IsSuccess(rhi->BindTextureSampler(input->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + index));
			}
			succeeded = succeeded && IsSuccess(rhi->Draw(3));
			if (succeeded)
			{
				m_TemporalHistory.Commit();
			}
			else if (!m_TemporalFailureLogged)
			{
				ENGINE_LOG_ERROR("Temporal anti-aliasing was not applied.");
				m_TemporalFailureLogged = true;
			}
			return EResult::Success;
		}, m_TemporalPassID);

	m_TemporalAAActive = true;
	m_TemporalJitterNDC = TemporalJitterNDC(jitter, width, height);
	return jittered;
}

Engine::RHITexture* Engine::DeferredSceneRenderer::DispatchRaytracedReflection(Engine::RHI& rhi, Engine::Scene* scene, RenderPassID viewID,
	const std::wstring& positionTargetName, const CameraBuffer& camera, const RaytracedReflectionInputs& inputs)
{
	if (!scene || !Engine::RaytracingSceneManager::IsCreated())
		return nullptr;

	auto& raytracing = Engine::RaytracingSceneManager::Get();
	Engine::RHIAccelerationStructure* topLevel = raytracing.GetTopLevelAS(scene, viewID);
	Engine::RenderTarget* position = Engine::RenderTargetManager::Get().GetRenderTarget(positionTargetName);
	if (!topLevel || !position || !position->GetTexture())
		return nullptr;

	RaytracedReflectionParams params{};
	params.InstanceTable = raytracing.GetInstanceTableIndex(scene, viewID);
	if (params.InstanceTable == Engine::RHI::INVALID_BINDLESS_INDEX)
		return nullptr;

	const Engine::uint32 width = position->GetWidth();
	const Engine::uint32 height = position->GetHeight();
	struct TargetInfo { const wchar_t* Suffix; Engine::ETextureFormat Format; };
	static constexpr TargetInfo targetInfos[ReflectionTargetCount] = {
		{ L"_ReflectionTrace", Engine::ETextureFormat::R16G16B16A16_FLOAT },        // 비율을 곱한 색, 합성 비율
		{ L"_ReflectionDistance", Engine::ETextureFormat::R16_FLOAT },
		{ L"_ReflectionAccumulated0", Engine::ETextureFormat::R16G16B16A16_FLOAT },
		{ L"_ReflectionAccumulated1", Engine::ETextureFormat::R16G16B16A16_FLOAT },
		{ L"_ReflectionGeometry0", Engine::ETextureFormat::R16G16B16A16_FLOAT },     // 카메라 거리, 법선, 누적 횟수
		{ L"_ReflectionGeometry1", Engine::ETextureFormat::R16G16B16A16_FLOAT },
		{ L"_RaytracedReflection", Engine::ETextureFormat::R16G16B16A16_FLOAT },
	};
	std::array<Engine::RHITexture*, ReflectionTargetCount> textures{};
	for (int index = 0; index < ReflectionTargetCount; ++index)
	{
		Engine::RenderTarget* target = EnsureOwnedTarget(positionTargetName + targetInfos[index].Suffix, targetInfos[index].Format,
			width, height, m_ReflectionTargetNames[index]);
		if (!target)
			return nullptr;
		textures[index] = target->GetTexture();
	}

	Engine::RHIPipeline* tracePipeline = GetRaytracedReflectionPipeline();
	Engine::RHIPipeline* temporalPipeline = GetRaytracingPipeline(ReflectionTemporalShaderKey, ReflectionTemporalShaderFile, m_ReflectionTemporalShaderMissing);
	Engine::RHIPipeline* filterPipeline = GetRaytracingPipeline(ReflectionFilterShaderKey, ReflectionFilterShaderFile, m_ReflectionFilterShaderMissing);
	if (!tracePipeline || !temporalPipeline || !filterPipeline)
		return nullptr;

	const auto frame = m_ReflectionHistory.Begin(camera, scene, width, height);
	const auto dispatch = [&](const wchar_t* name, Engine::RHIPipeline* pipeline, const auto& constants, std::initializer_list<Engine::RHITexture*> outputs, bool traces)
		{
			rhi.BeginPassMetrics(positionTargetName + name);
			bool succeeded = Engine::IsSuccess(rhi.BindPipeline(pipeline)) && Engine::IsSuccess(rhi.BindConstantBuffer(&constants, sizeof(constants), 3));
			if (succeeded && traces)
				succeeded = Engine::IsSuccess(rhi.BindAccelerationStructure(topLevel, RaytracedShadowTLASSlot));
			Engine::uint32 slot = 0;
			for (auto* output : outputs)
				succeeded = succeeded && Engine::IsSuccess(rhi.BindComputeStorageTexture(output, slot++));
			succeeded = succeeded && Engine::IsSuccess(rhi.Dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u));
			rhi.EndPassMetrics();
			return succeeded;
		};

	// 1. 추적
	params.SunLight = inputs.SunLight;
	params.SkyZenith = inputs.SkyZenith;
	params.IBLIntensity = inputs.IBLIntensity;
	params.HasIBLMaps = inputs.HasIBLMaps ? 1u : 0u;
	params.PixelSpread = 2.0f / (glm::max(std::abs(camera.projMatrix[1][1]), 0.1f) * static_cast<Engine::f32>(glm::max(height, 1u)));
	params.FrameIndex = frame.FrameIndex;
	if (!dispatch(L"_ReflectionTracePass", tracePipeline, params, { textures[ReflectionTrace], textures[ReflectionDistance] }, true))
		return nullptr;

	// 2. 시간 누적. 앞 패스가 쓴 텍스처의 번호를 얻을 때 읽기 상태로 바뀐다.
	Engine::RHITexture* accumulated = textures[ReflectionAccumulated0 + frame.WriteIndex];
	Engine::RHITexture* geometry = textures[ReflectionGeometry0 + frame.WriteIndex];
	ReflectionDenoiseParams denoise{};
	denoise.PrevViewProjection = frame.PrevViewProjection;
	denoise.PrevCameraPosition = frame.PrevCameraPosition;
	denoise.HistoryState = !frame.Valid ? 0u : (frame.CameraMoved ? 1u : 2u);
	denoise.FrameIndex = frame.FrameIndex;
	denoise.InputTexture = rhi.GetBindlessIndex(textures[ReflectionTrace]);
	denoise.HitDistanceTexture = rhi.GetBindlessIndex(textures[ReflectionDistance]);
	denoise.HistoryTexture = rhi.GetBindlessIndex(textures[ReflectionAccumulated0 + frame.ReadIndex]);
	denoise.GeometryTexture = rhi.GetBindlessIndex(textures[ReflectionGeometry0 + frame.ReadIndex]);
	for (const Engine::uint32 index : { denoise.InputTexture, denoise.HitDistanceTexture, denoise.HistoryTexture, denoise.GeometryTexture })
	{
		if (index == Engine::RHI::INVALID_BINDLESS_INDEX)
			return nullptr;
	}
	if (!dispatch(L"_ReflectionTemporalPass", temporalPipeline, denoise, { accumulated, geometry }, false))
		return nullptr;

	// 3. 공간 필터. 결과를 누적에 되먹이지 않아 시간이 지나도 더 번지지 않는다.
	denoise.InputTexture = rhi.GetBindlessIndex(accumulated);
	denoise.GeometryTexture = rhi.GetBindlessIndex(geometry);
	if (denoise.InputTexture == Engine::RHI::INVALID_BINDLESS_INDEX || denoise.GeometryTexture == Engine::RHI::INVALID_BINDLESS_INDEX ||
		!dispatch(L"_ReflectionFilterPass", filterPipeline, denoise, { textures[ReflectionFiltered] }, false))
		return nullptr;

	m_ReflectionHistory.Commit();
	return textures[ReflectionFiltered];
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
	Engine::RenderTarget* mask = EnsureOwnedTarget(positionTargetName + L"_RaytracedShadowMask",
		Engine::ETextureFormat::R8G8B8A8_UNORM, width, height, m_RaytracedShadowMaskName); // 광원 최대 4개의 가시성을 채널마다 담는다.
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
