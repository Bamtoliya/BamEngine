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
#include "FlagComponent.h"
#include "ProbeGIVolumeComponent.h"
#include "WorldTransformComponent.h"
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
	// probeGISample.hlsli의 ProbeGILightingData (LightingPassData 끝, 64바이트)
	struct ProbeGILightingData
	{
		vec3 GridOrigin{ 0.0f };
		uint32 Enabled = 0u;
		vec3 GridSpacing{ 1.0f };
		f32 Intensity = 1.0f;
		std::array<uint32, 3> GridCounts{ 2u, 2u, 2u };
		f32 BlendDistance = 1.0f;
		f32 NormalBias = 0.0f;
		f32 ViewBias = 0.0f;
		uint32 DebugView = 0u;
		uint32 Pad = 0u;
	};
	static_assert(sizeof(ProbeGILightingData) == 64);
	static_assert(offsetof(ProbeGILightingData, GridCounts) == 32);

	// 표본 위치를 띄우는 거리. 가장 좁은 프로브 간격에 대한 비율이며 측정 전 시작값이다.
	constexpr f32 ProbeGINormalBiasRatio = 0.1f;
	constexpr f32 ProbeGIViewBiasRatio = 0.3f;
	// lightingPS의 gProbeIrradianceAtlas(t2, space1)·gProbeDistanceAtlas(t3, space1)
	constexpr Engine::uint32 ProbeGIIrradianceSlot = 2;
	constexpr Engine::uint32 ProbeGIDistanceSlot = 3;
	constexpr Engine::uint32 ProbeGIProbeDataSlot = 4; // gProbeData(t4, space1)

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
		uint32 UseAmbientOcclusion = 0u; // 1이면 t13의 앰비언트 오클루전을 환경광에 곱한다
		uint32 AmbientOcclusionPad[3] = {};
		ProbeGILightingData ProbeGI{};
	};
	static_assert(sizeof(DeferredLightingPassData) == 624);
	static_assert(offsetof(DeferredLightingPassData, ProbeGI) == 560);
	static_assert(offsetof(DeferredLightingPassData, UseAmbientOcclusion) == 544);
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
		uint32 FrameIndex = 0u;
		std::array<uint32, MAX_RAYTRACED_SHADOW_LIGHTS> Lights{};
		uint32 WritePenumbra = 0u;
		uint32 Pad[3]{};
	};
	static_assert(sizeof(RaytracedShadowParams) == 48);

	// raytracedShadow.hlsli의 RaytracedShadowDenoiseParams (b1, space3)
	struct RaytracedShadowDenoiseParams
	{
		mat4 PrevViewProjection{ 1.0f };
		vec3 PrevCameraPosition{ 0.0f };
		uint32 HistoryState = 0u; // 0 이력 없음, 1 카메라가 움직임, 2 카메라가 멈춤
		uint32 InputTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 PenumbraTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 HistoryTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 PenumbraHistoryTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 GeometryTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		// 측정 전 시작값. 카메라가 움직이면 이 값의 3/8까지만 이어 쓴다.
		f32 MaxHistory = 32.0f;
		f32 PixelSizeConstant = 0.0f;
		f32 PixelSizePerMeter = 0.001f;
		std::array<uint32, MAX_RAYTRACED_SHADOW_LIGHTS> Lights{};
	};
	static_assert(sizeof(RaytracedShadowDenoiseParams) == 128);
	static_assert(offsetof(RaytracedShadowDenoiseParams, Lights) == 112);

	constexpr const wchar_t* ShadowTemporalShaderKey = L"Resources/Shader/RaytracedShadowTemporalRTCS";
	constexpr const wchar_t* ShadowTemporalShaderFile = L"raytracedShadowTemporalRTCS.cso";
	constexpr const wchar_t* ShadowFilterShaderKey = L"Resources/Shader/RaytracedShadowFilterRTCS";
	constexpr const wchar_t* ShadowFilterShaderFile = L"raytracedShadowFilterRTCS.cso";

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
		uint32 MotionTexture = Engine::RHI::INVALID_BINDLESS_INDEX; // 이번 프레임의 모션 벡터(TAA가 켜져 있을 때만)
		uint32 Padding[3]{};
	};
	static_assert(sizeof(ReflectionDenoiseParams) == 128);

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

	// probeGI.hlsli의 ProbeGIUpdateParams (b1, space3)
	struct ProbeGIUpdateParams
	{
		vec3 GridOrigin{ 0.0f };
		uint32 RaysPerProbe = 128u;
		vec3 GridSpacing{ 1.0f };
		uint32 ProbeCount = 0u;
		std::array<uint32, 3> GridCounts{};
		uint32 UpdateCount = 0u;
		uint32 FirstProbe = 0u;
		uint32 FreshCount = 0u;
		uint32 InstanceTable = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 ClearAll = 0u;
		// 광선 묶음 회전의 행(xyz)
		vec4 RayRotation0{ 1.0f, 0.0f, 0.0f, 0.0f };
		vec4 RayRotation1{ 0.0f, 1.0f, 0.0f, 0.0f };
		vec4 RayRotation2{ 0.0f, 0.0f, 1.0f, 0.0f };
		vec3 SkyZenith{ 0.0f };
		f32 IBLIntensity = 1.0f;
		uint32 HasIBLMaps = 0u;
		// 닿은 면에서 확인할 광원 수. 그림자를 드리우는 광원마다 그림자 광선을 쏘므로 반사보다 적게 둔다. 측정 전 시작값이다.
		uint32 MaxHitLights = 16u;
		f32 NormalBias = 0.02f;
		f32 RayMaxDistance = 10000.0f;
		f32 Hysteresis = 0.97f;
		f32 MaxProbeDistance = 1.0f;
		uint32 RayDataTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		// 거리는 방향이 아주 가까운 광선만 섞어 날카롭게 둔다(DDGI 기본값)
		f32 DistanceExponent = 50.0f;
		uint32 ProbeDataTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		// 다중 반사에 쓰는 지난 프레임 아틀라스. 없으면 닿은 면은 기존 환경광을 쓴다.
		uint32 IrradianceTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		uint32 DistanceTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
		f32 BlendDistance = 1.0f;
		f32 SampleNormalBias = 0.0f;
		f32 SampleViewBias = 0.0f;
		// 이전 값의 이 배수보다 크게 바뀐 조도 텍셀은 이력 비율을 0.75 낮춘다. 측정 전 시작값이다.
		f32 ChangeThreshold = 0.5f;
		f32 Pad = 0.0f;
	};
	static_assert(sizeof(ProbeGIUpdateParams) == 192);
	static_assert(offsetof(ProbeGIUpdateParams, ProbeDataTexture) == 160);
	static_assert(offsetof(ProbeGIUpdateParams, GridCounts) == 32);
	static_assert(offsetof(ProbeGIUpdateParams, RayRotation0) == 64);
	static_assert(offsetof(ProbeGIUpdateParams, SkyZenith) == 112);
	static_assert(offsetof(ProbeGIUpdateParams, HasIBLMaps) == 128);
	static_assert(offsetof(ProbeGIUpdateParams, Hysteresis) == 144);

	constexpr const wchar_t* ProbeGITraceShaderKey = L"Resources/Shader/ProbeGITraceRTCS";
	constexpr const wchar_t* ProbeGITraceShaderFile = L"probeGITraceRTCS.cso";
	constexpr const wchar_t* ProbeGIIrradianceShaderKey = L"Resources/Shader/ProbeGIBlendIrradianceRTCS";
	constexpr const wchar_t* ProbeGIIrradianceShaderFile = L"probeGIBlendIrradianceRTCS.cso";
	constexpr const wchar_t* ProbeGIDistanceShaderKey = L"Resources/Shader/ProbeGIBlendDistanceRTCS";
	constexpr const wchar_t* ProbeGIDistanceShaderFile = L"probeGIBlendDistanceRTCS.cso";
	constexpr const wchar_t* ProbeGIRelocateShaderKey = L"Resources/Shader/ProbeGIRelocateRTCS";
	constexpr const wchar_t* ProbeGIRelocateShaderFile = L"probeGIRelocateRTCS.cso";

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
	constexpr const wchar_t* MotionVectorTerrainVSKey = L"Resources/Shader/MotionVectorTerrainVS";
	constexpr const wchar_t* MotionVectorTerrainVSFile = L"motionVectorTerrainVS.cso";
	constexpr const wchar_t* MotionVectorPSKey = L"Resources/Shader/MotionVectorPS";
	constexpr const wchar_t* MotionVectorPSFile = L"motionVectorPS.cso";

	// ambientOcclusion.hlsli의 AmbientOcclusionParams (b1, space3)
	struct AmbientOcclusionParams
	{
		mat4 PrevViewProjection{ 1.0f };
		vec3 CameraPosition{ 0.0f };
		f32 Radius = 0.5f;
		vec3 ScreenRight{ 1.0f, 0.0f, 0.0f };
		f32 PixelsPerMeter = 0.0f;
		vec3 ScreenUp{ 0.0f, 1.0f, 0.0f };
		f32 MaxRadiusPixels = 128.0f;
		vec3 PrevCameraPosition{ 0.0f };
		f32 Power = 1.0f;
		vec2 PrevJitterUV{ 0.0f };
		uint32 FrameIndex = 0u;
		uint32 HistoryValid = 0u;
		uint32 SliceCount = 2u;
		uint32 StepCount = 6u;
		uint32 RayCount = 2u;
		f32 MaxHistory = 16.0f;
		f32 NormalBias = 0.02f;
		f32 DistanceBiasScale = 0.0005f;
		vec2 Pad{ 0.0f };
	};
	static_assert(sizeof(AmbientOcclusionParams) == 176);
	static_assert(offsetof(AmbientOcclusionParams, CameraPosition) == 64);
	static_assert(offsetof(AmbientOcclusionParams, PrevJitterUV) == 128);
	static_assert(offsetof(AmbientOcclusionParams, SliceCount) == 144);

	// 화면 공간은 접촉부 음영, 레이트레이싱은 조금 더 넓은 차폐를 본다. 모두 측정 전 시작값이다.
	constexpr f32 ScreenSpaceAORadius = 0.5f;
	constexpr f32 ScreenSpaceAOPower = 1.5f;
	constexpr f32 ScreenSpaceAOMaxHistory = 16.0f;
	constexpr f32 ScreenSpaceAOMaxRadiusFraction = 0.1f; // 화면 높이 대비 최대 반경
	constexpr f32 RaytracedAORadius = 1.0f;
	constexpr uint32 RaytracedAORayCount = 2u;
	constexpr f32 RaytracedAOMaxHistory = 32.0f;

	constexpr const wchar_t* ScreenSpaceAOShaderKey = L"Resources/Shader/ScreenSpaceAmbientOcclusionCS";
	constexpr const wchar_t* ScreenSpaceAOShaderFile = L"screenSpaceAmbientOcclusionCS.cso";
	constexpr const wchar_t* RaytracedAOShaderKey = L"Resources/Shader/RaytracedAmbientOcclusionRTCS";
	constexpr const wchar_t* RaytracedAOShaderFile = L"raytracedAmbientOcclusionRTCS.cso";
	constexpr const wchar_t* AmbientOcclusionTemporalShaderKey = L"Resources/Shader/AmbientOcclusionTemporalCS";
	constexpr const wchar_t* AmbientOcclusionTemporalShaderFile = L"ambientOcclusionTemporalCS.cso";
	// lightingPS의 t13. Compute에서는 같은 번호에 TLAS를 묶으므로 그 전에 비운다.
	constexpr Engine::uint32 AmbientOcclusionSlot = MAX_MATERIAL_TEXTURE_SLOTS + 13;
	// 시간 누적 Compute의 입력(t14 이번 프레임 값, t15 이전 결과). 이후 그림자 마스크·반사가 같은 번호를 다시 묶는다.
	constexpr Engine::uint32 AmbientOcclusionRawSlot = MAX_MATERIAL_TEXTURE_SLOTS + 14;
	constexpr Engine::uint32 AmbientOcclusionHistorySlot = MAX_MATERIAL_TEXTURE_SLOTS + 15;
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
	for (auto& name : m_ShadowTargetNames)
		removeTarget(name);
	m_ShadowHistory.Reset();
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
	for (auto& name : m_AmbientOcclusionTargetNames)
		removeTarget(name);
	m_AmbientOcclusionHistory.Reset();
	m_AmbientOcclusionActive = EAmbientOcclusionMode::Off;
	m_AmbientOcclusionHistoryPath = EAmbientOcclusionMode::Off;
	m_AmbientOcclusionPrevJitterNDC = vec2(0.0f);
	for (auto& name : m_ProbeGITargetNames)
		removeTarget(name);
	m_ProbeGISchedule.Reset();
	m_ProbeGIActive = false;
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
	// 껐다 켜면 오래된 결과를 이어 쓰지 않는다.
	if (!useRaytracedShadow)
		m_ShadowHistory.Reset();

	// 0-1. 레이트레이싱 반사 사용 여부. 독립 프리뷰는 제외한다.
	const auto* rhi = renderer.GetRHI();
	const bool useRaytracedReflection = sceneToQuery && lightingPass && lightingPass->IsEnabled() && passes.AcceptSceneMeshes &&
		ResolveRaytracedShadow(false, m_RaytracedReflectionOverrides) && rhi && rhi->GetCapabilities().SupportsBindlessResources &&
		Engine::RaytracingSceneManager::IsCreated() && Engine::RaytracingSceneManager::Get().IsSupported() && GetRaytracedReflectionPipeline();
	m_RaytracedReflectionActive = useRaytracedReflection;
	// 껐다 켜면 오래된 결과를 이어 쓰지 않는다.
	if (!useRaytracedReflection)
		m_ReflectionHistory.Reset();

	// 0-2. 프로브 GI 사용 여부. 반사와 같은 광선 조회 조건에 활성 볼륨이 있어야 한다. 독립 프리뷰는 제외한다.
	// 갱신은 조명 명령 안에서 하며, 셰이더가 없거나 기록에 실패하면 그 프레임만 건너뛴다.
	const bool useProbeGI = sceneToQuery && lightingPass && lightingPass->IsEnabled() && passes.AcceptSceneMeshes &&
		ResolveRaytracedShadow(false, m_ProbeGIOverrides) && rhi && rhi->GetCapabilities().SupportsBindlessResources &&
		Engine::RaytracingSceneManager::IsCreated() && Engine::RaytracingSceneManager::Get().IsSupported() &&
		FindProbeGIVolume(sceneToQuery, cameraBuffer.cameraPosition, m_ProbeGIVolume);
	m_ProbeGIActive = useProbeGI;
	// 껐다 켜면 오래된 아틀라스를 이어 쓰지 않는다.
	if (!useProbeGI)
		m_ProbeGISchedule.Reset();

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

	// 3-1. 앰비언트 오클루전 방식. 쓰기 텍스처가 필요한 Compute라 DirectX12만 지원하고, 독립 프리뷰와 직교 카메라는 제외한다.
	const EAmbientOcclusionMode requestedOcclusion = ResolveAmbientOcclusion(m_AmbientOcclusionOverrides);
	EAmbientOcclusionMode ambientOcclusion = EAmbientOcclusionMode::Off;
	if (requestedOcclusion != EAmbientOcclusionMode::Off && sceneToQuery && lightingPass && lightingPass->IsEnabled() && passes.AcceptSceneMeshes &&
		SupportsScreenSpaceAmbientOcclusion(cameraBuffer.projMatrix) && renderer.GetRHIType() == ERHIType::DirectX12)
	{
		const bool screenSpaceAvailable = GetRaytracingPipeline(ScreenSpaceAOShaderKey, ScreenSpaceAOShaderFile, m_ScreenSpaceAOShaderMissing) &&
			GetRaytracingPipeline(AmbientOcclusionTemporalShaderKey, AmbientOcclusionTemporalShaderFile, m_AmbientOcclusionTemporalShaderMissing);
		const bool raytracingAvailable = requestedOcclusion == EAmbientOcclusionMode::Raytraced &&
			Engine::RaytracingSceneManager::IsCreated() && Engine::RaytracingSceneManager::Get().IsSupported() &&
			GetRaytracingPipeline(RaytracedAOShaderKey, RaytracedAOShaderFile, m_RaytracedAOShaderMissing);
		ambientOcclusion = SelectAmbientOcclusionPath(requestedOcclusion, screenSpaceAvailable, raytracingAvailable);
	}
	// 실제로 쓴 방식은 조명 명령이 실행될 때 기록한다(TLAS가 없으면 화면 공간으로 바뀔 수 있다).
	if (ambientOcclusion == EAmbientOcclusionMode::Off)
	{
		m_AmbientOcclusionActive = EAmbientOcclusionMode::Off;
		m_AmbientOcclusionHistory.Reset();
	}
	// 이전 결과는 지난 프레임의 지터로 그린 G-Buffer 위치에 있다.
	const vec2 previousJitterNDC = m_AmbientOcclusionPrevJitterNDC;
	m_AmbientOcclusionPrevJitterNDC = m_TemporalJitterNDC;

	// 4. LightingPass 풀스크린 조명 커스텀 커맨드 제출
	if (lightingPass && lightingPass->IsEnabled())
	{
		// 레이트레이싱 그림자·반사·AO·프로브 GI를 쓰는 씬만 TLAS를 요청한다(빌드는 Renderer::Render 시작 시).
		// 프로브 GI는 반사처럼 닿은 면의 재질을 읽으므로 인스턴스 표도 준비한다.
		if (useRaytracedShadow || useRaytracedReflection || ambientOcclusion == EAmbientOcclusionMode::Raytraced || useProbeGI)
			RaytracingSceneManager::Get().RequestScene(sceneToQuery, passes.Geometry, useRaytracedReflection || useProbeGI);
		SubmitLightingPass(cameraBuffer, sceneCamera, sceneToQuery, skyData, passes, useRaytracedShadow, useRaytracedReflection, ambientOcclusion,
			previousJitterNDC, useProbeGI);
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
	const SkyAtmosphereData& skyData, const DeferredScenePassBindings& passes, bool useRaytracedShadow, bool useRaytracedReflection,
	EAmbientOcclusionMode ambientOcclusion, const vec2& previousJitterNDC, bool useProbeGI)
{
	// 렌더 패스 안에서는 텍스처 업로드가 거부되고 실패한 리소스는 다시 시도하지 않는다.
	// 람다가 처음 읽을 수 있는 IBL 큐브맵·BRDF LUT·구름 텍스처는 패스 밖인 제출 단계에서 미리 올린다.
	if (targetScene)
	{
		auto& world = targetScene->GetRegistry();
		for (auto [entity, sky] : world.view<SkyAtmosphereComponent>().each())
		{
			if (!IsComponentEnabled(world, entity, sky))
				continue;
			for (auto* texture : { sky.IrradianceMap.Get(), sky.PrefilteredMap.Get(), sky.BRDFLutMap.Get() })
				if (texture) static_cast<void>(texture->EnsureResident());
		}
	}
	if (ResourceManager::IsCreated())
	{
		auto& resources = ResourceManager::Get();
		for (const auto& key : { CloudResources::BaseShapeKey, CloudResources::WeatherMapKey })
			if (auto* texture = resources.GetResourceHandle<Texture>(key).Get()) static_cast<void>(texture->EnsureResident());
	}

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
			raytracedOverrides = m_RaytracedShadowOverrides, ambientOcclusion, previousJitterNDC, useProbeGI,
			probeGIVolume = m_ProbeGIVolume, probeGIDebugView = m_ProbeGIDebugView](f32, Engine::RenderPass*) -> Engine::EResult
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
			// 7-0. 앰비언트 오클루전 (G-Buffer·광원 버퍼 바인딩을 그대로 사용하는 Compute).
			// 지난 프레임에 t13·t14·t15에 묶은 결과가 남아 있으면 TLAS나 쓰기 텍스처와 겹치므로 먼저 비운다.
			for (const uint32 slot : { AmbientOcclusionSlot, AmbientOcclusionRawSlot, AmbientOcclusionHistorySlot })
				(void)rhi->BindTexture(nullptr, slot);
			Engine::RHITexture* ambientOcclusionTexture = nullptr;
			if (ambientOcclusion != EAmbientOcclusionMode::Off)
			{
				ambientOcclusionTexture = DispatchAmbientOcclusion(*rhi, targetScene, viewID, sourceNames[4], camBuf, sceneCamBuf, ambientOcclusion, previousJitterNDC);
				if (Engine::IsFailure(rhi->BindPipeline(m_LightingPipeline)))
					return Engine::EResult::Fail;
				if (!ambientOcclusionTexture)
				{
					m_AmbientOcclusionActive = EAmbientOcclusionMode::Off;
					if (!m_AmbientOcclusionFailureLogged)
					{
						ENGINE_LOG_ERROR("Ambient occlusion was not produced; only material occlusion is applied this frame.");
						m_AmbientOcclusionFailureLogged = true;
					}
				}
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
				// 이전 프레임의 출력이 SRV로 남아 있으면 같은 텍스처에 쓸 수 없다.
				(void)rhi->BindTexture(nullptr, RaytracedShadowMaskSlot);
				// 크기가 있는 광원만 누적·필터한다. 번호로 읽는 리소스를 쓸 수 없으면 한 표본의 결과를 그대로 쓴다.
				const bool denoise = HasSoftRaytracedShadow(shadowLights, raytracedLights) && rhi->GetCapabilities().SupportsBindlessResources;
				rhi->BeginPassMetrics(sourceNames[4] + L"_RaytracedShadowMask");
				Engine::RHITexture* shadowMask = DispatchRaytracedShadowMask(*rhi, targetScene, viewID, sourceNames[4], camBuf, raytracedLights, denoise);
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
			// 8-2. 프로브 GI 갱신 (광원·환경광 바인딩을 그대로 사용하는 Compute). 이번 프레임에 갱신한 아틀라스만 조명에 쓴다.
			bool probeGIBound = false;
			if (useProbeGI)
			{
				RaytracedReflectionInputs probeEnvironment;
				probeEnvironment.SkyZenith = glm::max(lightingData.SkyZenithColor, vec3(0.05f)) * glm::max(lightingData.SkyIntensity, 0.2f);
				probeEnvironment.IBLIntensity = lightingData.IBLIntensity;
				probeEnvironment.HasIBLMaps = lightingData.HasIBLMaps != 0u;
				ProbeGIAtlases atlases;
				rhi->BeginPassMetrics(sourceNames[4] + L"_ProbeGIUpdate");
				const bool updated = DispatchProbeGIUpdate(*rhi, targetScene, viewID, sourceNames[4], probeGIVolume, probeEnvironment, atlases);
				rhi->EndPassMetrics();
				if (Engine::IsFailure(rhi->BindPipeline(m_LightingPipeline)))
					return Engine::EResult::Fail;
				// 재질 슬롯 2·3·4(t2·t3·t4, space1)에 기본 선형 샘플러로 묶는다. 셰이더는 gBufferSampler로 읽는다.
				probeGIBound = updated && Engine::IsSuccess(rhi->BindTextureSampler(atlases.Irradiance, sampler, ProbeGIIrradianceSlot)) &&
					Engine::IsSuccess(rhi->BindTextureSampler(atlases.Distance, sampler, ProbeGIDistanceSlot)) &&
					Engine::IsSuccess(rhi->BindTextureSampler(atlases.ProbeData, sampler, ProbeGIProbeDataSlot));
				if (probeGIBound)
				{
					const ProbeGrid::GridDesc& grid = probeGIVolume.Grid;
					ProbeGILightingData& gi = lightingData.ProbeGI;
					gi.Enabled = 1u;
					gi.GridOrigin = grid.Origin;
					gi.GridSpacing = grid.Spacing;
					gi.GridCounts = { grid.Counts.X, grid.Counts.Y, grid.Counts.Z };
					gi.Intensity = probeGIVolume.Intensity;
					gi.BlendDistance = probeGIVolume.BlendDistance;
					const f32 minSpacing = ProbeGrid::MinSpacing(grid.Spacing);
					gi.NormalBias = minSpacing * ProbeGINormalBiasRatio;
					gi.ViewBias = minSpacing * ProbeGIViewBiasRatio;
					gi.DebugView = probeGIDebugView ? 1u : 0u;
				}
				else if (!m_ProbeGIFailureLogged)
				{
					ENGINE_LOG_ERROR("Probe GI was not updated this frame; the previous ambient lighting is used instead.");
					m_ProbeGIFailureLogged = true;
				}
			}
			// 8-3. 앰비언트 오클루전 결과 (t13). 그림자·반사·프로브 Compute가 TLAS를 쓰고 난 뒤에 묶는다.
			if (ambientOcclusionTexture && Engine::IsSuccess(rhi->BindTextureSampler(ambientOcclusionTexture, sampler, AmbientOcclusionSlot)))
				lightingData.UseAmbientOcclusion = 1u;
			// 9. 조명 패스 상수 버퍼 바인딩 (slot 3 -> b1, space3)
			if (Engine::IsFailure(rhi->BindConstantBuffer(&lightingData, sizeof(lightingData), 3)))
				return Engine::EResult::Fail;
			// 10. 조명은 접촉 그림자에서 G-Buffer 위치를 화면 좌표로 다시 찾으므로 G-Buffer를 그린 카메라로 바꾼다.
			if (Engine::IsFailure(rhi->BindConstantBuffer(&sceneCamBuf, sizeof(CameraBuffer), 0)))
				return Engine::EResult::Fail;
			// 11. 풀스크린 쿼드 조명 합성 실행
			if (Engine::IsFailure(rhi->Draw(3)))
				return Engine::EResult::Fail;
			// t13에 남은 AO가 다른 뷰포트의 TLAS 바인딩과 겹치지 않게 비운다(이미 기록한 Draw에는 영향이 없다).
			if (ambientOcclusionTexture)
				(void)rhi->BindTexture(nullptr, AmbientOcclusionSlot);
			// 다음 프레임 갱신 Compute가 같은 아틀라스에 쓰므로 읽기 바인딩을 남기지 않는다.
			if (probeGIBound)
			{
				for (const uint32 slot : { ProbeGIIrradianceSlot, ProbeGIDistanceSlot, ProbeGIProbeDataSlot })
					(void)rhi->BindTexture(nullptr, slot);
			}
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

bool Engine::DeferredSceneRenderer::FindProbeGIVolume(Engine::Scene* scene, const vec3& cameraPosition, ProbeGIVolumeData& volume)
{
	if (!scene)
		return false;
	auto& registry = scene->GetRegistry();
	bool found = false;
	f32 bestDistance = 0.0f;
	for (auto [entity, component, transform] : registry.view<Engine::ProbeGIVolumeComponent, Engine::WorldTransformComponent>().each())
	{
		if (!IsComponentEnabled(registry, entity, component))
			continue;
		if (const auto* flags = registry.try_get<Engine::FlagComponent>(entity); flags && !flags->IsActiveInHierarchy())
			continue;
		const ProbeGrid::GridDesc grid = ProbeGrid::MakeGrid(vec3(transform.worldMatrix[3]), component.Size,
			component.ProbeCountX, component.ProbeCountY, component.ProbeCountZ);
		// 볼륨은 뷰마다 하나만 쓴다. 카메라를 담은 볼륨을, 없으면 가장 가까운 볼륨을 고른다.
		const f32 distance = ProbeGrid::DistanceToVolume(grid, cameraPosition);
		if (found && distance >= bestDistance)
			continue;
		found = true;
		bestDistance = distance;
		volume.Grid = grid;
		volume.RaysPerProbe = ProbeGrid::ClampRaysPerProbe(component.RaysPerProbe);
		volume.ProbesPerFrame = component.ProbesPerFrame;
		volume.Hysteresis = glm::clamp(component.Hysteresis, 0.0f, 0.99f);
		volume.Intensity = glm::max(component.Intensity, 0.0f);
		volume.BlendDistance = glm::max(component.BlendDistance, 0.0f);
	}
	return found;
}

bool Engine::DeferredSceneRenderer::DispatchProbeGIUpdate(Engine::RHI& rhi, Engine::Scene* scene, RenderPassID viewID,
	const std::wstring& positionTargetName, const ProbeGIVolumeData& volume, const RaytracedReflectionInputs& env,
	ProbeGIAtlases& atlases)
{
	atlases = {};
	if (!scene || !Engine::RaytracingSceneManager::IsCreated())
		return false;
	auto& raytracing = Engine::RaytracingSceneManager::Get();
	Engine::RHIAccelerationStructure* topLevel = raytracing.GetTopLevelAS(scene, viewID);
	const Engine::uint32 instanceTable = raytracing.GetInstanceTableIndex(scene, viewID);
	if (!topLevel || instanceTable == Engine::RHI::INVALID_BINDLESS_INDEX)
		return false;

	Engine::RHIPipeline* tracePipeline = GetRaytracingPipeline(ProbeGITraceShaderKey, ProbeGITraceShaderFile, m_ProbeGITraceShaderMissing);
	Engine::RHIPipeline* relocatePipeline = GetRaytracingPipeline(ProbeGIRelocateShaderKey, ProbeGIRelocateShaderFile, m_ProbeGIRelocateShaderMissing);
	Engine::RHIPipeline* irradiancePipeline = GetRaytracingPipeline(ProbeGIIrradianceShaderKey, ProbeGIIrradianceShaderFile, m_ProbeGIIrradianceShaderMissing);
	Engine::RHIPipeline* distancePipeline = GetRaytracingPipeline(ProbeGIDistanceShaderKey, ProbeGIDistanceShaderFile, m_ProbeGIDistanceShaderMissing);
	if (!tracePipeline || !relocatePipeline || !irradiancePipeline || !distancePipeline)
		return false;

	const ProbeGrid::GridCounts& counts = volume.Grid.Counts;
	const auto frame = m_ProbeGISchedule.Begin(volume.Grid, scene, volume.ProbesPerFrame);
	if (frame.UpdateCount == 0u)
		return false;

	// 아틀라스와 프로브 상태는 격자 크기, 광선 결과는 (광선 수 × 이번 프레임 갱신 수) 크기다.
	// 아틀라스나 상태를 새로 만들거나 크기를 바꾸면 내용이 정해지지 않으므로 일정이 비우기부터 다시 시작한다.
	const ProbeGrid::AtlasSize irradianceSize = ProbeGrid::GetAtlasSize(counts, ProbeGrid::IrradianceTexels);
	const ProbeGrid::AtlasSize distanceSize = ProbeGrid::GetAtlasSize(counts, ProbeGrid::DistanceTexels);
	struct TargetInfo { const wchar_t* Suffix; Engine::uint32 Width; Engine::uint32 Height; };
	const TargetInfo targetInfos[ProbeGITargetCount] = {
		{ L"_ProbeGIRayData", volume.RaysPerProbe, frame.UpdateCount },
		{ L"_ProbeGIIrradiance", irradianceSize.Width, irradianceSize.Height },
		{ L"_ProbeGIDistance", distanceSize.Width, distanceSize.Height },
		{ L"_ProbeGIProbeData", counts.X * counts.Y, counts.Z },
	};
	std::array<Engine::RHITexture*, ProbeGITargetCount> textures{};
	for (int index = 0; index < ProbeGITargetCount; ++index)
	{
		const std::wstring name = positionTargetName + targetInfos[index].Suffix;
		Engine::RenderTarget* existing = Engine::RenderTargetManager::Get().GetRenderTarget(name);
		if (index != ProbeGIRayData && (!existing || existing->GetWidth() != targetInfos[index].Width || existing->GetHeight() != targetInfos[index].Height))
			m_ProbeGISchedule.Reset();
		Engine::RenderTarget* target = EnsureOwnedTarget(name, Engine::ETextureFormat::R16G16B16A16_FLOAT,
			targetInfos[index].Width, targetInfos[index].Height, m_ProbeGITargetNames[index]);
		if (!target)
			return false;
		textures[index] = target->GetTexture();
	}
	// 위에서 일정을 초기화했을 수 있으므로 다시 받는다(같은 프레임에 두 번 불러도 Commit 전에는 같은 값이다).
	const auto scheduled = m_ProbeGISchedule.Begin(volume.Grid, scene, volume.ProbesPerFrame);

	ProbeGIUpdateParams params{};
	params.GridOrigin = volume.Grid.Origin;
	params.GridSpacing = volume.Grid.Spacing;
	params.GridCounts = { counts.X, counts.Y, counts.Z };
	params.ProbeCount = counts.Total();
	params.RaysPerProbe = volume.RaysPerProbe;
	params.UpdateCount = scheduled.UpdateCount;
	params.FirstProbe = scheduled.FirstProbe;
	params.FreshCount = scheduled.FreshCount;
	params.InstanceTable = instanceTable;
	const mat3 rotation = ProbeGrid::RandomRayRotation(scheduled.Index);
	params.RayRotation0 = vec4(rotation[0][0], rotation[1][0], rotation[2][0], 0.0f);
	params.RayRotation1 = vec4(rotation[0][1], rotation[1][1], rotation[2][1], 0.0f);
	params.RayRotation2 = vec4(rotation[0][2], rotation[1][2], rotation[2][2], 0.0f);
	params.SkyZenith = env.SkyZenith;
	params.IBLIntensity = env.IBLIntensity;
	params.HasIBLMaps = env.HasIBLMaps ? 1u : 0u;
	params.Hysteresis = volume.Hysteresis;
	// DDGI처럼 대각선 간격의 1.5배까지만 거리를 기록한다. 그보다 먼 면은 가시성 검사에서 차이가 없다.
	params.MaxProbeDistance = glm::length(volume.Grid.Spacing) * 1.5f;
	params.BlendDistance = volume.BlendDistance;
	const f32 minSpacing = ProbeGrid::MinSpacing(volume.Grid.Spacing);
	params.SampleNormalBias = minSpacing * ProbeGINormalBiasRatio;
	params.SampleViewBias = minSpacing * ProbeGIViewBiasRatio;

	const auto dispatch = [&](const wchar_t* name, Engine::RHIPipeline* pipeline, Engine::RHITexture* output, bool traces,
		Engine::uint32 groupsX, Engine::uint32 groupsY)
		{
			rhi.BeginPassMetrics(positionTargetName + name);
			bool succeeded = Engine::IsSuccess(rhi.BindPipeline(pipeline)) && Engine::IsSuccess(rhi.BindConstantBuffer(&params, sizeof(params), 3));
			if (succeeded && traces)
				succeeded = Engine::IsSuccess(rhi.BindAccelerationStructure(topLevel, RaytracedShadowTLASSlot));
			succeeded = succeeded && Engine::IsSuccess(rhi.BindComputeStorageTexture(output, 0)) &&
				Engine::IsSuccess(rhi.Dispatch(groupsX, groupsY, 1u));
			rhi.EndPassMetrics();
			return succeeded;
		};

	// 0. 새로 시작하면 모든 프로브 칸을 0으로, 프로브 상태를 격자 위치·켠 상태로 비운다(행·스레드 = 프로브 번호).
	if (scheduled.Clear)
	{
		params.ClearAll = 1u;
		if (!dispatch(L"_ProbeGIClear", irradiancePipeline, textures[ProbeGIIrradiance], false, params.ProbeCount, 1u) ||
			!dispatch(L"_ProbeGIClear", distancePipeline, textures[ProbeGIDistance], false, params.ProbeCount, 1u) ||
			!dispatch(L"_ProbeGIClear", relocatePipeline, textures[ProbeGIProbeData], false, (params.ProbeCount + 63u) / 64u, 1u))
			return false;
		params.ClearAll = 0u;
	}

	// 1. 추적. 광선 32개씩 한 그룹, 행마다 프로브 하나. 프로브 위치와 다중 반사에 지난 프레임의 상태·아틀라스를 번호로 읽는다.
	//    번호를 얻을 때 읽기 상태로 바뀌므로 위의 비우기 쓰기도 끝난 뒤 읽는다. 이번 프레임에 비웠으면 닿은 면은 기존 환경광을 쓴다.
	params.ProbeDataTexture = rhi.GetBindlessIndex(textures[ProbeGIProbeData]);
	params.IrradianceTexture = rhi.GetBindlessIndex(textures[ProbeGIIrradiance]);
	params.DistanceTexture = rhi.GetBindlessIndex(textures[ProbeGIDistance]);
	if (params.ProbeDataTexture == Engine::RHI::INVALID_BINDLESS_INDEX)
		return false;
	if (params.DistanceTexture == Engine::RHI::INVALID_BINDLESS_INDEX)
		params.IrradianceTexture = Engine::RHI::INVALID_BINDLESS_INDEX;
	if (!dispatch(L"_ProbeGITracePass", tracePipeline, textures[ProbeGIRayData], true, (params.RaysPerProbe + 31u) / 32u, params.UpdateCount))
		return false;

	// 2. 벽 속 프로브 옮기기·끄기, 3. 섞기. 광선 결과는 번호로 읽는다. 섞기는 프로브 칸 하나가 스레드 그룹 하나다.
	params.RayDataTexture = rhi.GetBindlessIndex(textures[ProbeGIRayData]);
	if (params.RayDataTexture == Engine::RHI::INVALID_BINDLESS_INDEX ||
		!dispatch(L"_ProbeGIRelocatePass", relocatePipeline, textures[ProbeGIProbeData], false, (params.UpdateCount + 63u) / 64u, 1u) ||
		!dispatch(L"_ProbeGIIrradiancePass", irradiancePipeline, textures[ProbeGIIrradiance], false, params.UpdateCount, 1u) ||
		!dispatch(L"_ProbeGIDistancePass", distancePipeline, textures[ProbeGIDistance], false, params.UpdateCount, 1u))
		return false;

	m_ProbeGISchedule.Commit();
	atlases.Irradiance = textures[ProbeGIIrradiance];
	atlases.Distance = textures[ProbeGIDistance];
	atlases.ProbeData = textures[ProbeGIProbeData];
	return true;
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

	// 자리만 등록된 텍스처는 처음 읽을 때 GPU에 올리는데, 렌더 패스 안에서는 올릴 수 없다(DirectX12RHI::UploadTextureData).
	// 그리기 명령을 기록하기 전인 지금 올린다. 읽지 못한 텍스처는 다시 시도하지 않으므로 빼고 색만 칠한다.
	for (DecalDrawData& decal : m_Decals)
	{
		for (Texture** texture : { &decal.BaseColorTexture, &decal.NormalTexture, &decal.PBRTexture })
		{
			if (*texture && IsFailure((*texture)->EnsureResident()))
				*texture = nullptr;
		}
	}
	auto white = Engine::ResourceManager::Get().GetResourceHandle<Texture>(L"Engine://Texture/white1x1.png");
	if (!white.Get())
		white = Engine::ResourceManager::Get().GetResourceHandle<Texture>(L"Resources/Texture/white1x1.png");
	if (white.Get())
		static_cast<void>(white->EnsureResident());

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

bool Engine::DeferredSceneRenderer::EnsureTemporalAAPasses(RenderPassID geometryPass, RenderPassID transparentPass, const std::wstring& prefix)
{
	auto& rpm = Engine::RenderPassManager::Get();
	const auto* geometry = rpm.GetRenderPassByID(geometryPass);
	const auto* transparent = rpm.GetRenderPassByID(transparentPass);
	if (!geometry || !transparent)
		return false;

	// 같은 우선순위에서는 나중에 등록한 패스가 뒤에 실행된다.
	// 모션 벡터는 G-Buffer 위치만 읽으므로 G-Buffer 직후(조명 앞)에 그린다. 조명 안의 레이트레이싱 반사 누적이 같은 프레임의
	// 이동량을 읽기 위해서다: G-Buffer → 모션 벡터 → 조명 → … → 반투명 → TAA → 후처리
	const auto ensure = [&](RenderPassID& id, const wchar_t* suffix, ERenderPassLoadOperation load, const RenderPass& priorityPass)
		{
			if (id == INVALID_PASS_ID)
			{
				RenderPassDesc desc{};
				desc.name = prefix + suffix;
				desc.priority = priorityPass.GetPriority();
				desc.loadOperation = load;
				desc.passType = ERenderPassType::PostProcess;
				desc.acceptedBlendModes = EBlendMode::None;
				id = rpm.RegisterRenderPass(std::move(desc));
			}
			return id != INVALID_PASS_ID;
		};
	return ensure(m_MotionPassID, L"_MotionVectorPass", ERenderPassLoadOperation::RPLO_Clear, *geometry) &&
		ensure(m_TemporalPassID, L"_TemporalAAPass", ERenderPassLoadOperation::RPLO_Load, *transparent);
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
	// 지형 셰이더가 없어도 TAA는 켜 두고 움직인 지형만 카메라 이동으로 처리한다.
	Engine::Shader* motionTerrainVS = GetOwnedShader(MotionVectorTerrainVSKey, MotionVectorTerrainVSFile, EShaderType::Vertex, m_MotionTerrainShaderMissing);
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
	if (!EnsureTemporalAAPasses(passes.Geometry, passes.ForwardTransparent, colorName))
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
	motionInfo.terrainVertexShader = motionTerrainVS;
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
	// TAA가 켜져 있으면 모션 벡터 패스가 조명 앞에서 이번 프레임의 이동량을 이미 그렸다. 얻지 못하면 카메라 이동만 반영한다.
	if (m_TemporalAAActive && !m_TemporalTargetNames[TemporalMotion].empty())
	{
		const Engine::RenderTarget* motion = Engine::RenderTargetManager::Get().GetRenderTarget(m_TemporalTargetNames[TemporalMotion]);
		if (motion && motion->GetTexture())
			denoise.MotionTexture = rhi.GetBindlessIndex(motion->GetTexture());
	}
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

Engine::RHITexture* Engine::DeferredSceneRenderer::DispatchAmbientOcclusion(Engine::RHI& rhi, Engine::Scene* scene, RenderPassID viewID,
	const std::wstring& positionTargetName, const CameraBuffer& camera, const CameraBuffer& sceneCamera, EAmbientOcclusionMode mode,
	const vec2& previousJitterNDC)
{
	Engine::RenderTarget* position = Engine::RenderTargetManager::Get().GetRenderTarget(positionTargetName);
	if (!scene || !position || !position->GetTexture())
		return nullptr;

	// TLAS 빌드에 실패했거나 셰이더가 없으면 이번 프레임은 화면 공간으로 계산한다.
	Engine::RHIAccelerationStructure* topLevel = nullptr;
	Engine::RHIPipeline* raytracedPipeline = nullptr;
	if (mode == EAmbientOcclusionMode::Raytraced)
	{
		topLevel = Engine::RaytracingSceneManager::IsCreated() ? Engine::RaytracingSceneManager::Get().GetTopLevelAS(scene, viewID) : nullptr;
		raytracedPipeline = topLevel ? GetRaytracingPipeline(RaytracedAOShaderKey, RaytracedAOShaderFile, m_RaytracedAOShaderMissing) : nullptr;
		if (!raytracedPipeline)
			mode = EAmbientOcclusionMode::ScreenSpace;
	}
	const bool raytraced = mode == EAmbientOcclusionMode::Raytraced;
	Engine::RHIPipeline* occlusionPipeline = raytraced ? raytracedPipeline :
		GetRaytracingPipeline(ScreenSpaceAOShaderKey, ScreenSpaceAOShaderFile, m_ScreenSpaceAOShaderMissing);
	Engine::RHIPipeline* temporalPipeline = GetRaytracingPipeline(AmbientOcclusionTemporalShaderKey, AmbientOcclusionTemporalShaderFile, m_AmbientOcclusionTemporalShaderMissing);
	if (!occlusionPipeline || !temporalPipeline)
		return nullptr;

	const Engine::uint32 width = position->GetWidth();
	const Engine::uint32 height = position->GetHeight();
	struct TargetInfo { const wchar_t* Suffix; Engine::ETextureFormat Format; };
	static constexpr TargetInfo targetInfos[AmbientOcclusionTargetCount] = {
		{ L"_AmbientOcclusionRaw", Engine::ETextureFormat::R32_FLOAT },               // 이번 프레임 가시성
		{ L"_AmbientOcclusion0", Engine::ETextureFormat::R16G16B16A16_FLOAT },        // 가시성, 카메라 거리, 누적 횟수
		{ L"_AmbientOcclusion1", Engine::ETextureFormat::R16G16B16A16_FLOAT },
	};
	std::array<Engine::RHITexture*, AmbientOcclusionTargetCount> textures{};
	for (int index = 0; index < AmbientOcclusionTargetCount; ++index)
	{
		Engine::RenderTarget* target = EnsureOwnedTarget(positionTargetName + targetInfos[index].Suffix, targetInfos[index].Format,
			width, height, m_AmbientOcclusionTargetNames[index]);
		if (!target)
			return nullptr;
		textures[index] = target->GetTexture();
	}

	// 방식이 바뀌면 노이즈 성질이 달라 이전 결과를 이어 쓰지 않는다.
	if (m_AmbientOcclusionHistoryPath != mode)
		m_AmbientOcclusionHistory.Reset();
	m_AmbientOcclusionHistoryPath = mode;
	m_AmbientOcclusionActive = mode;
	const auto frame = m_AmbientOcclusionHistory.Begin(camera, scene, width, height);

	// 화면 방향과 반경은 G-Buffer를 그린 카메라 기준이다(지터는 방향과 크기를 바꾸지 않는다).
	const ScreenAxes axes = GetScreenAxes(sceneCamera.viewMatrix, sceneCamera.projMatrix);
	AmbientOcclusionParams params{};
	params.PrevViewProjection = frame.PrevViewProjection;
	params.CameraPosition = sceneCamera.cameraPosition;
	params.Radius = raytraced ? RaytracedAORadius : ScreenSpaceAORadius;
	params.ScreenRight = axes.Right;
	params.PixelsPerMeter = PixelsPerMeterAtUnitDistance(sceneCamera.projMatrix, height);
	params.ScreenUp = axes.Up;
	params.MaxRadiusPixels = glm::max(ScreenSpaceAOMaxRadiusFraction * static_cast<f32>(height), 8.0f);
	params.PrevCameraPosition = frame.PrevCameraPosition;
	params.Power = raytraced ? 1.0f : ScreenSpaceAOPower;
	params.PrevJitterUV = NDCOffsetToUV(previousJitterNDC);
	params.FrameIndex = frame.FrameIndex;
	params.HistoryValid = frame.Valid ? 1u : 0u;
	params.RayCount = RaytracedAORayCount;
	params.MaxHistory = raytraced ? RaytracedAOMaxHistory : ScreenSpaceAOMaxHistory;

	// 1. 이번 프레임 값
	rhi.BeginPassMetrics(positionTargetName + (raytraced ? L"_RaytracedAOPass" : L"_ScreenSpaceAOPass"));
	bool succeeded = Engine::IsSuccess(rhi.BindPipeline(occlusionPipeline)) && Engine::IsSuccess(rhi.BindConstantBuffer(&params, sizeof(params), 3));
	if (succeeded && raytraced)
		succeeded = Engine::IsSuccess(rhi.BindAccelerationStructure(topLevel, RaytracedShadowTLASSlot));
	succeeded = succeeded && Engine::IsSuccess(rhi.BindComputeStorageTexture(textures[AmbientOcclusionRaw], 0)) &&
		Engine::IsSuccess(rhi.Dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u));
	rhi.EndPassMetrics();
	if (!succeeded)
		return nullptr;

	// 2. 공간 필터와 시간 누적. 입력은 t14·t15로 읽고 다음 Compute 전에 비운다.
	Engine::RHITexture* history = textures[AmbientOcclusionHistory0 + frame.ReadIndex];
	Engine::RHITexture* accumulated = textures[AmbientOcclusionHistory0 + frame.WriteIndex];
	rhi.BeginPassMetrics(positionTargetName + L"_AmbientOcclusionTemporalPass");
	succeeded = Engine::IsSuccess(rhi.BindPipeline(temporalPipeline)) && Engine::IsSuccess(rhi.BindConstantBuffer(&params, sizeof(params), 3)) &&
		Engine::IsSuccess(rhi.BindTexture(textures[AmbientOcclusionRaw], AmbientOcclusionRawSlot)) &&
		Engine::IsSuccess(rhi.BindTexture(history, AmbientOcclusionHistorySlot)) &&
		Engine::IsSuccess(rhi.BindComputeStorageTexture(accumulated, 0)) &&
		Engine::IsSuccess(rhi.Dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u));
	rhi.EndPassMetrics();
	(void)rhi.BindTexture(nullptr, AmbientOcclusionRawSlot);
	(void)rhi.BindTexture(nullptr, AmbientOcclusionHistorySlot);
	if (!succeeded)
		return nullptr;

	m_AmbientOcclusionHistory.Commit();
	return accumulated;
}

Engine::RHITexture* Engine::DeferredSceneRenderer::DispatchRaytracedShadowMask(Engine::RHI& rhi, Engine::Scene* scene,
	RenderPassID viewID, const std::wstring& positionTargetName, const CameraBuffer& camera, const RaytracedShadowAssignment& lights, bool denoise)
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

	const auto dispatch = [&](const wchar_t* name, Engine::RHIPipeline* computePipeline, const void* constants, Engine::uint32 constantSize,
		std::initializer_list<Engine::RHITexture*> outputs, bool traces)
		{
			rhi.BeginPassMetrics(positionTargetName + name);
			bool succeeded = Engine::IsSuccess(rhi.BindPipeline(computePipeline)) && Engine::IsSuccess(rhi.BindConstantBuffer(constants, constantSize, 3));
			if (succeeded && traces)
				succeeded = Engine::IsSuccess(rhi.BindAccelerationStructure(topLevel, RaytracedShadowTLASSlot));
			Engine::uint32 slot = 0;
			for (auto* output : outputs)
				succeeded = succeeded && Engine::IsSuccess(rhi.BindComputeStorageTexture(output, slot++));
			succeeded = succeeded && Engine::IsSuccess(rhi.Dispatch((width + 7u) / 8u, (height + 7u) / 8u, 1u));
			rhi.EndPassMetrics();
			return succeeded;
		};

	// 누적에 쓸 타깃과 셰이더. 하나라도 없으면 한 표본의 결과를 그대로 쓴다.
	struct TargetInfo { const wchar_t* Suffix; Engine::ETextureFormat Format; };
	static constexpr TargetInfo targetInfos[ShadowTargetCount] = {
		{ L"_RaytracedShadowTrace", Engine::ETextureFormat::R8G8B8A8_UNORM },
		{ L"_RaytracedShadowPenumbra", Engine::ETextureFormat::R16G16B16A16_FLOAT },          // 채널별 반그림자 반지름(m)
		{ L"_RaytracedShadowAccumulated0", Engine::ETextureFormat::R16G16B16A16_FLOAT },
		{ L"_RaytracedShadowAccumulated1", Engine::ETextureFormat::R16G16B16A16_FLOAT },
		{ L"_RaytracedShadowPenumbraHistory0", Engine::ETextureFormat::R16G16B16A16_FLOAT },
		{ L"_RaytracedShadowPenumbraHistory1", Engine::ETextureFormat::R16G16B16A16_FLOAT },
		{ L"_RaytracedShadowGeometry0", Engine::ETextureFormat::R16G16B16A16_FLOAT },          // 카메라 거리, 법선, 누적 횟수
		{ L"_RaytracedShadowGeometry1", Engine::ETextureFormat::R16G16B16A16_FLOAT },
	};
	std::array<Engine::RHITexture*, ShadowTargetCount> textures{};
	Engine::RHIPipeline* temporalPipeline = nullptr;
	Engine::RHIPipeline* filterPipeline = nullptr;
	if (denoise)
	{
		temporalPipeline = GetRaytracingPipeline(ShadowTemporalShaderKey, ShadowTemporalShaderFile, m_ShadowTemporalShaderMissing);
		filterPipeline = GetRaytracingPipeline(ShadowFilterShaderKey, ShadowFilterShaderFile, m_ShadowFilterShaderMissing);
		denoise = temporalPipeline && filterPipeline;
		for (int index = 0; denoise && index < ShadowTargetCount; ++index)
		{
			Engine::RenderTarget* target = EnsureOwnedTarget(positionTargetName + targetInfos[index].Suffix, targetInfos[index].Format,
				width, height, m_ShadowTargetNames[index]);
			textures[index] = target ? target->GetTexture() : nullptr;
			denoise = textures[index] != nullptr;
		}
	}

	RaytracedShadowParams params{};
	params.Lights = lights.Lights;
	if (!denoise)
	{
		m_ShadowHistory.Reset();
		return dispatch(L"_RaytracedShadowTracePass", pipeline, &params, sizeof(params), { mask->GetTexture() }, true) ? mask->GetTexture() : nullptr;
	}

	if (lights.Lights != m_ShadowHistoryLights.Lights)
	{
		m_ShadowHistory.Reset();
		m_ShadowHistoryLights = lights;
	}
	const auto frame = m_ShadowHistory.Begin(camera, scene, width, height);

	// 1. 추적. 광원 원판의 표본을 프레임마다 바꾼다.
	params.FrameIndex = frame.FrameIndex;
	params.WritePenumbra = 1u;
	if (!dispatch(L"_RaytracedShadowTracePass", pipeline, &params, sizeof(params), { textures[ShadowTrace], textures[ShadowPenumbra] }, true))
		return nullptr;

	// 2. 시간 누적. 앞 패스가 쓴 텍스처의 번호를 얻을 때 읽기 상태로 바뀐다.
	RaytracedShadowDenoiseParams denoiseParams{};
	denoiseParams.PrevViewProjection = frame.PrevViewProjection;
	denoiseParams.PrevCameraPosition = frame.PrevCameraPosition;
	denoiseParams.HistoryState = !frame.Valid ? 0u : (frame.CameraMoved ? 1u : 2u);
	denoiseParams.Lights = lights.Lights;
	// 픽셀 하나의 월드 크기. 원근 투영은 카메라 거리에 비례하고 직교 투영은 일정하다.
	const Engine::f32 pixelSpread = 2.0f / (glm::max(std::abs(camera.projMatrix[1][1]), 1e-4f) * static_cast<Engine::f32>(glm::max(height, 1u)));
	const bool orthographic = std::abs(camera.projMatrix[3][3] - 1.0f) < 1e-4f;
	denoiseParams.PixelSizeConstant = orthographic ? pixelSpread : 0.0f;
	denoiseParams.PixelSizePerMeter = orthographic ? 0.0f : pixelSpread;
	denoiseParams.InputTexture = rhi.GetBindlessIndex(textures[ShadowTrace]);
	denoiseParams.PenumbraTexture = rhi.GetBindlessIndex(textures[ShadowPenumbra]);
	denoiseParams.HistoryTexture = rhi.GetBindlessIndex(textures[ShadowAccumulated0 + frame.ReadIndex]);
	denoiseParams.PenumbraHistoryTexture = rhi.GetBindlessIndex(textures[ShadowPenumbraHistory0 + frame.ReadIndex]);
	denoiseParams.GeometryTexture = rhi.GetBindlessIndex(textures[ShadowGeometry0 + frame.ReadIndex]);
	for (const Engine::uint32 index : { denoiseParams.InputTexture, denoiseParams.PenumbraTexture, denoiseParams.HistoryTexture,
		denoiseParams.PenumbraHistoryTexture, denoiseParams.GeometryTexture })
	{
		if (index == Engine::RHI::INVALID_BINDLESS_INDEX)
			return nullptr;
	}
	Engine::RHITexture* accumulated = textures[ShadowAccumulated0 + frame.WriteIndex];
	Engine::RHITexture* penumbra = textures[ShadowPenumbraHistory0 + frame.WriteIndex];
	Engine::RHITexture* geometry = textures[ShadowGeometry0 + frame.WriteIndex];
	if (!dispatch(L"_RaytracedShadowTemporalPass", temporalPipeline, &denoiseParams, sizeof(denoiseParams), { accumulated, penumbra, geometry }, false))
		return nullptr;

	// 3. 공간 필터. 결과를 누적에 되먹이지 않아 시간이 지나도 더 번지지 않는다.
	denoiseParams.InputTexture = rhi.GetBindlessIndex(accumulated);
	denoiseParams.PenumbraTexture = rhi.GetBindlessIndex(penumbra);
	denoiseParams.GeometryTexture = rhi.GetBindlessIndex(geometry);
	if (denoiseParams.InputTexture == Engine::RHI::INVALID_BINDLESS_INDEX || denoiseParams.PenumbraTexture == Engine::RHI::INVALID_BINDLESS_INDEX ||
		denoiseParams.GeometryTexture == Engine::RHI::INVALID_BINDLESS_INDEX ||
		!dispatch(L"_RaytracedShadowFilterPass", filterPipeline, &denoiseParams, sizeof(denoiseParams), { mask->GetTexture() }, false))
		return nullptr;

	m_ShadowHistory.Commit();
	return mask->GetTexture();
}
