#pragma once

#include "AmbientOcclusion.h"
#include "CameraBuffer.h"
#include "DecalProjection.h"
#include "RenderPassTypes.h"
#include "RenderTargetTypes.h"
#include "RHITextureTypes.h"
#include "RaytracedShadowSettings.h"
#include "MotionHistory.h"
#include "ProbeGrid.h"
#include "ShaderReflection.h"
#include "ShadowLightAssignment.h"
#include "TemporalHistory.h"
#include <array>

BEGIN(Engine)

class RHIBuffer;
class RHIPipeline;
class Scene;
struct SkyAtmosphereData;

// 반사가 닿은 면의 밝기를 정하는 값
struct RaytracedReflectionInputs
{
	uint32 SunLight = INVALID_SHADOW_LIGHT;
	vec3 SkyZenith{ 0.0f };
	f32 IBLIntensity = 1.0f;
	bool HasIBLMaps = false;
};

// 이번 프레임에 쓰는 프로브 GI 볼륨(ProbeGIVolumeComponent에서 범위를 자른 값)
struct ProbeGIVolumeData
{
	ProbeGrid::GridDesc Grid{};
	uint32 RaysPerProbe = 128u;
	int32 ProbesPerFrame = 512;
	f32 Hysteresis = 0.97f;
	f32 Intensity = 1.0f;
	f32 BlendDistance = 1.0f;
};

// 이번 프레임에 갱신한 프로브 GI 텍스처. 조명 패스가 읽는다.
struct ProbeGIAtlases
{
	class RHITexture* Irradiance = nullptr;
	class RHITexture* Distance = nullptr;
	class RHITexture* ProbeData = nullptr; // rgb 벽 속에서 옮긴 거리, a 사용 여부
};

struct DeferredScenePassBindings
{
	RenderPassID DirectionalShadow;
	RenderPassID PointShadow;
	RenderPassID Geometry;
	RenderPassID Lighting;
	RenderPassID ForwardOpaque;
	RenderPassID ForwardTransparent;
	const std::array<std::wstring, 6>& LightingInputs;
	const std::wstring& PointShadowInput;
	bool AcceptSceneMeshes = true;
};

class ENGINE_API DeferredSceneRenderer
{
public:
	DeferredSceneRenderer() = default;
	~DeferredSceneRenderer();
	DeferredSceneRenderer(const DeferredSceneRenderer&) = delete;
	DeferredSceneRenderer& operator=(const DeferredSceneRenderer&) = delete;

	bool Initialize();
	void Free();
	void Submit(const CameraBuffer& camera, Scene* scene, const DeferredScenePassBindings& passes);

	void SetRaytracedShadowOverrides(const RaytracedShadowOverrides& overrides) { m_RaytracedShadowOverrides = overrides; }
	// 마지막 Submit에서 조명·설정·장치 지원을 모두 반영해 레이트레이싱 그림자를 요청했는지
	bool IsRaytracedShadowActive() const { return m_RaytracedShadowActive; }
	// 반사는 조명 설정이 없으므로 기본이 꺼짐이고 프로젝트 → 그래픽 → 뷰포트 순서로 덮어쓴다.
	void SetRaytracedReflectionOverrides(const RaytracedShadowOverrides& overrides) { m_RaytracedReflectionOverrides = overrides; }
	bool IsRaytracedReflectionActive() const { return m_RaytracedReflectionActive; }
	// 시간 누적 안티앨리어싱도 기본이 꺼짐이고 같은 순서로 덮어쓴다.
	void SetTemporalAAOverrides(const RaytracedShadowOverrides& overrides) { m_TemporalAAOverrides = overrides; }
	bool IsTemporalAAActive() const { return m_TemporalAAActive; }
	// 프로브 GI도 기본이 꺼짐이고 같은 순서로 덮어쓴다. 켜져 있어도 활성 ProbeGIVolumeComponent가 없거나 장치가 반사를 지원하지 않으면 쓰지 않는다.
	// 프로브를 광선으로 갱신해 아틀라스(렌더 타깃 목록의 <위치 타깃>_ProbeGIIrradiance·_ProbeGIDistance)에 쓰고,
	// 조명 패스가 볼륨 안의 확산 환경광을 프로브 조도로 바꾼다.
	void SetProbeGIOverrides(const RaytracedShadowOverrides& overrides) { m_ProbeGIOverrides = overrides; }
	bool IsProbeGIActive() const { return m_ProbeGIActive; }
	// 켜면 프로브 GI를 쓰는 동안 간접 확산광만 출력한다(뷰포트 확인용, 저장하지 않음).
	void SetProbeGIDebugView(bool enabled) { m_ProbeGIDebugView = enabled; }
	bool IsProbeGIDebugView() const { return m_ProbeGIDebugView; }
	// 이번 프레임 깊이 타깃의 지터(NDC). 후처리 뒤에 깊이를 비교하는 오버레이가 같은 화면 위치의 깊이를 계산할 때 쓴다. TAA를 쓰지 않으면 0이다.
	vec2 GetTemporalJitterNDC() const { return m_TemporalJitterNDC; }
	// 앰비언트 오클루전도 기본이 꺼짐이고 같은 순서로 덮어쓴다. 레이트레이싱을 쓸 수 없으면 화면 공간으로 되돌린다.
	void SetAmbientOcclusionOverrides(const AmbientOcclusionOverrides& overrides) { m_AmbientOcclusionOverrides = overrides; }
	// 마지막으로 실행한 방식(Off, ScreenSpace, Raytraced)
	EAmbientOcclusionMode GetActiveAmbientOcclusion() const { return m_AmbientOcclusionActive; }

private:
	SkyAtmosphereData BuildSkyAtmosphereData(Scene* scene);
	// sceneCamera는 G-Buffer를 그린 카메라다. G-Buffer를 화면 좌표로 다시 찾는 계산은 이 카메라를 써야 맞는다.
	void SubmitLightingPass(const CameraBuffer& camera, const CameraBuffer& sceneCamera, Scene* scene, const SkyAtmosphereData& sky,
		const DeferredScenePassBindings& passes, bool useRaytracedShadow, bool useRaytracedReflection, EAmbientOcclusionMode ambientOcclusion,
		const vec2& previousJitterNDC, bool useProbeGI);

	// 레이트레이싱 그림자 마스크를 G-Buffer 크기로 계산한다. 성공하면 마스크 텍스처를 반환한다.
	// denoise가 참이면(크기가 있는 광원이 있음) 누적·필터까지 거친 결과를 반환한다. camera는 지터가 없는 카메라다.
	// 조명 파이프라인 대신 Compute 파이프라인을 바인딩하므로 호출 후 조명 파이프라인을 다시 바인딩해야 한다.
	class RHITexture* DispatchRaytracedShadowMask(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName,
		const CameraBuffer& camera, const RaytracedShadowAssignment& lights, bool denoise);
	// 반사를 G-Buffer 크기로 추적하고 누적·필터한 결과를 반환한다. 조건과 주의는 그림자 마스크와 같으며, 광원·환경광 텍스처를 먼저 바인딩해야 한다.
	class RHITexture* DispatchRaytracedReflection(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName,
		const CameraBuffer& camera, const struct RaytracedReflectionInputs& inputs);
	// 앰비언트 오클루전을 G-Buffer 크기로 계산하고 누적한 결과를 반환한다. TLAS나 셰이더가 없으면 레이트레이싱 대신 화면 공간으로 계산한다.
	// 조건과 주의는 그림자 마스크와 같으며, G-Buffer와 광원 버퍼를 먼저 바인딩해야 한다. camera는 지터가 없는 카메라다.
	class RHITexture* DispatchAmbientOcclusion(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName,
		const CameraBuffer& camera, const CameraBuffer& sceneCamera, EAmbientOcclusionMode mode, const vec2& previousJitterNDC);
	// 이번 프레임 차례의 프로브를 추적하고, 벽 속 프로브를 옮기거나 끄고, 조도·거리 아틀라스에 섞는다. 모두 기록하면 true와 텍스처를 돌려준다.
	// 조건과 주의는 반사와 같으며, 광원·환경광 텍스처를 먼저 바인딩해야 한다. env는 볼륨 밖 닿은 면과 하늘의 환경광이다.
	bool DispatchProbeGIUpdate(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName,
		const ProbeGIVolumeData& volume, const RaytracedReflectionInputs& env, ProbeGIAtlases& atlases);
	// 이 렌더러가 소유하는 타깃. ownedName에 이름을 기록하고 크기가 바뀌면 다시 만든다. 기본은 Compute 쓰기용이다.
	class RenderTarget* EnsureOwnedTarget(const std::wstring& name, ETextureFormat format, uint32 width, uint32 height, std::wstring& ownedName,
		ETextureUsage usage = ETextureUsage::Sampler | ETextureUsage::ComputeReadWrite, const vec4& clearColor = vec4(0.0f, 0.0f, 0.0f, 1.0f));
	// TAA를 쓸 수 있으면 타깃과 패스를 준비해 명령을 제출하고, 장면을 그릴 지터 카메라를 반환한다. 쓸 수 없으면 카메라를 그대로 반환한다.
	CameraBuffer PrepareTemporalAA(const CameraBuffer& camera, Scene* scene, const DeferredScenePassBindings& passes);
	// 이 뷰의 데칼을 모아 G-Buffer 뒤·조명 앞의 데칼 패스에 제출한다. 데칼이 없으면 패스를 꺼서 법선 복사도 하지 않는다.
	// sceneCamera는 G-Buffer를 그린 카메라다(TAA 지터 포함).
	void SubmitDecalPass(const CameraBuffer& sceneCamera, Scene* scene, const DeferredScenePassBindings& passes);
	bool EnsureTemporalAAPasses(RenderPassID geometryPass, RenderPassID transparentPass, const std::wstring& prefix);
	class Shader* GetOwnedShader(const wchar_t* shaderKey, const wchar_t* shaderFile, EShaderType type, bool& shaderMissing);
	RHIPipeline* GetRaytracingPipeline(const wchar_t* shaderKey, const wchar_t* shaderFile, bool& shaderMissing);
	RHIPipeline* GetRaytracedShadowPipeline();
	RHIPipeline* GetRaytracedReflectionPipeline();

	RHIPipeline* m_LightingPipeline = nullptr;
	RHIBuffer* m_LocalLightBuffer[MAX_SWAPCHAIN_BUFFERS] = {};
	// 이 뷰포트의 조명 버퍼에 들어갈 그림자 광원
	ShadowLightSet CollectViewShadowLights(Scene* scene, bool usesGlobalLights) const;
	// 계층에서 활성인 엔티티의 활성 ProbeGIVolumeComponent 중 카메라를 담은 것(여럿이면 먼저 찾은 것),
	// 없으면 카메라에서 가장 가까운 것으로 격자와 갱신 값을 만든다. 볼륨이 없으면 false다.
	static bool FindProbeGIVolume(Scene* scene, const vec3& cameraPosition, ProbeGIVolumeData& volume);

	RaytracedShadowOverrides m_RaytracedShadowOverrides;
	bool m_RaytracedShadowActive = false;
	std::wstring m_RaytracedShadowMaskName;
	bool m_RaytracedShadowShaderMissing = false;
	bool m_RaytracedShadowFailureLogged = false;
	// 크기가 있는 광원의 추적 결과, 반그림자 반지름, 누적 결과·반지름·표면(각각 두 장을 번갈아 쓴다). 최종 결과는 m_RaytracedShadowMaskName이다.
	enum EShadowTarget { ShadowTrace, ShadowPenumbra, ShadowAccumulated0, ShadowAccumulated1, ShadowPenumbraHistory0, ShadowPenumbraHistory1,
		ShadowGeometry0, ShadowGeometry1, ShadowTargetCount };
	std::array<std::wstring, ShadowTargetCount> m_ShadowTargetNames;
	TemporalHistory m_ShadowHistory;
	// 채널과 광원의 연결이 바뀌면 이전 결과가 다른 광원의 것이므로 버린다.
	RaytracedShadowAssignment m_ShadowHistoryLights;
	bool m_ShadowTemporalShaderMissing = false;
	bool m_ShadowFilterShaderMissing = false;

	RaytracedShadowOverrides m_RaytracedReflectionOverrides;
	bool m_RaytracedReflectionActive = false;
	// 추적 결과, 닿은 거리, 누적 결과와 표면(각각 두 장을 번갈아 쓴다), 필터 결과
	enum EReflectionTarget { ReflectionTrace, ReflectionDistance, ReflectionAccumulated0, ReflectionAccumulated1, ReflectionGeometry0, ReflectionGeometry1, ReflectionFiltered, ReflectionTargetCount };
	std::array<std::wstring, ReflectionTargetCount> m_ReflectionTargetNames;
	TemporalHistory m_ReflectionHistory;
	bool m_RaytracedReflectionShaderMissing = false;
	bool m_ReflectionTemporalShaderMissing = false;
	bool m_ReflectionFilterShaderMissing = false;
	bool m_RaytracedReflectionFailureLogged = false;

	RaytracedShadowOverrides m_ProbeGIOverrides;
	bool m_ProbeGIActive = false;
	bool m_ProbeGIDebugView = false;
	// 마지막 Submit에서 고른 볼륨. 활성이 아니면 의미가 없다.
	ProbeGIVolumeData m_ProbeGIVolume{};
	ProbeGrid::ProbeUpdateSchedule m_ProbeGISchedule;
	// 광선 결과(열 = 광선, 행 = 갱신 순서), 조도 아틀라스, 거리 아틀라스, 프로브 상태(이동량·사용 여부)
	enum EProbeGITarget { ProbeGIRayData, ProbeGIIrradiance, ProbeGIDistance, ProbeGIProbeData, ProbeGITargetCount };
	std::array<std::wstring, ProbeGITargetCount> m_ProbeGITargetNames;
	bool m_ProbeGITraceShaderMissing = false;
	bool m_ProbeGIRelocateShaderMissing = false;
	bool m_ProbeGIIrradianceShaderMissing = false;
	bool m_ProbeGIDistanceShaderMissing = false;
	bool m_ProbeGIFailureLogged = false;

	RaytracedShadowOverrides m_TemporalAAOverrides;
	bool m_TemporalAAActive = false;
	vec2 m_TemporalJitterNDC{ 0.0f };
	// 움직인 물체의 이동량, 이전 결과(두 장을 번갈아 쓴다)
	enum ETemporalTarget { TemporalMotion, TemporalColor0, TemporalColor1, TemporalTargetCount };
	std::array<std::wstring, TemporalTargetCount> m_TemporalTargetNames;
	TemporalHistory m_TemporalHistory;
	MotionHistory m_MotionHistory;
	RenderPassID m_MotionPassID = INVALID_PASS_ID;
	RenderPassID m_TemporalPassID = INVALID_PASS_ID;
	bool m_TemporalShaderMissing = false;
	bool m_MotionTerrainShaderMissing = false;
	bool m_TemporalFailureLogged = false;

	AmbientOcclusionOverrides m_AmbientOcclusionOverrides;
	EAmbientOcclusionMode m_AmbientOcclusionActive = EAmbientOcclusionMode::Off;
	// 이력을 쌓은 방식. 바뀌면 노이즈 성질이 달라 이력을 버린다.
	EAmbientOcclusionMode m_AmbientOcclusionHistoryPath = EAmbientOcclusionMode::Off;
	// 이전 프레임 G-Buffer의 TAA 지터(NDC)
	vec2 m_AmbientOcclusionPrevJitterNDC{ 0.0f };
	// 이번 프레임 값, 누적 결과(두 장을 번갈아 쓴다)
	enum EAmbientOcclusionTarget { AmbientOcclusionRaw, AmbientOcclusionHistory0, AmbientOcclusionHistory1, AmbientOcclusionTargetCount };
	std::array<std::wstring, AmbientOcclusionTargetCount> m_AmbientOcclusionTargetNames;
	TemporalHistory m_AmbientOcclusionHistory;
	bool m_ScreenSpaceAOShaderMissing = false;
	bool m_RaytracedAOShaderMissing = false;
	bool m_AmbientOcclusionTemporalShaderMissing = false;
	bool m_AmbientOcclusionFailureLogged = false;
	RenderPassID m_DecalPassID = INVALID_PASS_ID;
	// 데칼이 법선을 바꾸기 전의 G-Buffer 법선. 셰이딩 플래그와 표면 방향을 여기서 읽는다.
	std::wstring m_DecalNormalCopyName;
	std::vector<DecalDrawData> m_Decals;
	bool m_DecalShaderMissing = false;
	bool m_DecalFailureLogged = false;
	bool m_DecalLimitLogged = false;
};

END
