#pragma once

#include "CameraBuffer.h"
#include "RenderPassTypes.h"
#include "RenderTargetTypes.h"
#include "RHITextureTypes.h"
#include "RaytracedShadowSettings.h"
#include "ShadowLightAssignment.h"
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

private:
	SkyAtmosphereData BuildSkyAtmosphereData(Scene* scene);
	void SubmitLightingPass(const CameraBuffer& camera, Scene* scene, const SkyAtmosphereData& sky,
		const DeferredScenePassBindings& passes, bool useRaytracedShadow, bool useRaytracedReflection);

	// 레이트레이싱 그림자 마스크를 G-Buffer 크기로 계산한다. 성공하면 마스크 텍스처를 반환한다.
	// 조명 파이프라인 대신 Compute 파이프라인을 바인딩하므로 호출 후 조명 파이프라인을 다시 바인딩해야 한다.
	class RHITexture* DispatchRaytracedShadowMask(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName,
		const RaytracedShadowAssignment& lights);
	// 반사를 G-Buffer 크기로 계산한다. 조건과 주의는 그림자 마스크와 같으며, 광원·환경광 텍스처를 먼저 바인딩해야 한다.
	class RHITexture* DispatchRaytracedReflection(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName,
		const CameraBuffer& camera, const struct RaytracedReflectionInputs& inputs);
	// 이 렌더러가 소유하는 Compute 쓰기 타깃. ownedName에 이름을 기록하고 크기가 바뀌면 다시 만든다.
	class RenderTarget* EnsureRaytracingTarget(const std::wstring& name, ETextureFormat format, uint32 width, uint32 height, std::wstring& ownedName);
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
	std::wstring m_RaytracedReflectionName;
	bool m_RaytracedReflectionShaderMissing = false;
	bool m_RaytracedReflectionFailureLogged = false;
};

END
