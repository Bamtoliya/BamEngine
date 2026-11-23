#pragma once

#include "CameraBuffer.h"
#include "RenderPassTypes.h"
#include "RenderTargetTypes.h"
#include "RHITextureTypes.h"
#include "RaytracedShadowSettings.h"
#include "MotionHistory.h"
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
	// 이번 프레임 깊이 타깃의 지터(NDC). 후처리 뒤에 깊이를 비교하는 오버레이가 같은 화면 위치의 깊이를 계산할 때 쓴다. TAA를 쓰지 않으면 0이다.
	vec2 GetTemporalJitterNDC() const { return m_TemporalJitterNDC; }

private:
	SkyAtmosphereData BuildSkyAtmosphereData(Scene* scene);
	// sceneCamera는 G-Buffer를 그린 카메라다. G-Buffer를 화면 좌표로 다시 찾는 계산은 이 카메라를 써야 맞는다.
	void SubmitLightingPass(const CameraBuffer& camera, const CameraBuffer& sceneCamera, Scene* scene, const SkyAtmosphereData& sky,
		const DeferredScenePassBindings& passes, bool useRaytracedShadow, bool useRaytracedReflection);

	// 레이트레이싱 그림자 마스크를 G-Buffer 크기로 계산한다. 성공하면 마스크 텍스처를 반환한다.
	// 조명 파이프라인 대신 Compute 파이프라인을 바인딩하므로 호출 후 조명 파이프라인을 다시 바인딩해야 한다.
	class RHITexture* DispatchRaytracedShadowMask(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName,
		const RaytracedShadowAssignment& lights);
	// 반사를 G-Buffer 크기로 추적하고 누적·필터한 결과를 반환한다. 조건과 주의는 그림자 마스크와 같으며, 광원·환경광 텍스처를 먼저 바인딩해야 한다.
	class RHITexture* DispatchRaytracedReflection(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName,
		const CameraBuffer& camera, const struct RaytracedReflectionInputs& inputs);
	// 이 렌더러가 소유하는 타깃. ownedName에 이름을 기록하고 크기가 바뀌면 다시 만든다. 기본은 Compute 쓰기용이다.
	class RenderTarget* EnsureOwnedTarget(const std::wstring& name, ETextureFormat format, uint32 width, uint32 height, std::wstring& ownedName,
		ETextureUsage usage = ETextureUsage::Sampler | ETextureUsage::ComputeReadWrite, const vec4& clearColor = vec4(0.0f, 0.0f, 0.0f, 1.0f));
	// TAA를 쓸 수 있으면 타깃과 패스를 준비해 명령을 제출하고, 장면을 그릴 지터 카메라를 반환한다. 쓸 수 없으면 카메라를 그대로 반환한다.
	CameraBuffer PrepareTemporalAA(const CameraBuffer& camera, Scene* scene, const DeferredScenePassBindings& passes);
	bool EnsureTemporalAAPasses(RenderPassID transparentPass, const std::wstring& prefix);
	class Shader* GetOwnedShader(const wchar_t* shaderKey, const wchar_t* shaderFile, EShaderType type, bool& shaderMissing);
	RHIPipeline* GetRaytracingPipeline(const wchar_t* shaderKey, const wchar_t* shaderFile, bool& shaderMissing);
	RHIPipeline* GetRaytracedShadowPipeline();
	RHIPipeline* GetRaytracedReflectionPipeline();

	RHIPipeline* m_LightingPipeline = nullptr;
	RHIBuffer* m_LocalLightBuffer[MAX_SWAPCHAIN_BUFFERS] = {};
	// 이 뷰포트의 조명 버퍼에 들어갈 그림자 광원
	ShadowLightSet CollectViewShadowLights(Scene* scene, bool usesGlobalLights) const;

	RaytracedShadowOverrides m_RaytracedShadowOverrides;
	bool m_RaytracedShadowActive = false;
	std::wstring m_RaytracedShadowMaskName;
	bool m_RaytracedShadowShaderMissing = false;
	bool m_RaytracedShadowFailureLogged = false;

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
	bool m_TemporalFailureLogged = false;
};

END
