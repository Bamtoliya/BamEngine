#pragma once

#include "CameraBuffer.h"
#include "RenderPassTypes.h"
#include "RenderTargetTypes.h"
#include "RaytracedShadowSettings.h"
#include <array>

BEGIN(Engine)

class RHIBuffer;
class RHIPipeline;
class Scene;
struct SkyAtmosphereData;

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

private:
	SkyAtmosphereData BuildSkyAtmosphereData(Scene* scene);
	void SubmitLightingPass(const CameraBuffer& camera, Scene* scene, const SkyAtmosphereData& sky,
		const DeferredScenePassBindings& passes, bool useRaytracedShadow);

	// 레이트레이싱 그림자 마스크를 G-Buffer 크기로 계산한다. 성공하면 마스크 텍스처를 반환한다.
	// 조명 파이프라인 대신 Compute 파이프라인을 바인딩하므로 호출 후 조명 파이프라인을 다시 바인딩해야 한다.
	class RHITexture* DispatchRaytracedShadowMask(class RHI& rhi, Scene* scene, RenderPassID viewID, const std::wstring& positionTargetName);
	class RenderTarget* EnsureRaytracedShadowMask(const std::wstring& positionTargetName, uint32 width, uint32 height);
	RHIPipeline* GetRaytracedShadowPipeline();

	RHIPipeline* m_LightingPipeline = nullptr;
	RHIBuffer* m_LocalLightBuffer[MAX_SWAPCHAIN_BUFFERS] = {};
	// 이 뷰포트의 조명 버퍼에서 섀도맵·마스크를 적용받는 첫 그림자 방향광의 설정과 덮어쓰기를 합친 결과
	bool ShouldUseRaytracedShadow(Scene* scene, bool usesGlobalLights) const;

	RaytracedShadowOverrides m_RaytracedShadowOverrides;
	bool m_RaytracedShadowActive = false;
	std::wstring m_RaytracedShadowMaskName;
	bool m_RaytracedShadowShaderMissing = false;
	bool m_RaytracedShadowFailureLogged = false;
};

END
