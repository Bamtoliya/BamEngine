#pragma once

#include "Editor_Includes.h"
#include "RenderPassTypes.h"
#include "CameraBuffer.h"
#include "DeferredProfilePasses.h"
#include "DeferredSceneRenderer.h"

BEGIN(Engine)
class Scene;
class RHIPipeline;
class RenderPass;
END

BEGIN(Editor)

class ViewportDeferredPipeline
{
public:
	ViewportDeferredPipeline() = default;
	~ViewportDeferredPipeline();

	// 1. 초기화: 뷰포트 고유 접두사(예: L"SceneView_", L"GameView_")와 해상도로 RT 및 Pass 생성
	bool Initialize(const std::wstring& viewportPrefix, uint32 width, uint32 height, bool acceptSceneMeshes = true);
	[[nodiscard]] Engine::RenderPipelineProfileResult Initialize(const Engine::RenderPipelineProfile& profile,
		const std::wstring& viewportPrefix, uint32 width, uint32 height);
	void Free();

	// 2. 해상도 리사이즈: 뷰포트 패널 크기가 바뀔 때 RT 리사이즈 (ShadowDepth 제외)
	EResult Resize(uint32 width, uint32 height);

	// 3. 매 프레임 렌더링 제출 (뷰포트 Update에서 호출)
	void Submit(const CameraBuffer& cameraBuffer, Engine::Scene* targetScene = nullptr);

	// 4. 게터 (최종 조명 합성 완료 텍스처 및 주요 패스 정보)
	[[nodiscard]] const std::wstring& GetFinalColorRTName() const { return m_LightingColorName; }
	[[nodiscard]] const std::wstring& GetDepthRTName() const { return m_DepthName; }
	[[nodiscard]] const std::wstring& GetPositionRTName() const { return m_LightingInputs[4]; }
	[[nodiscard]] const std::wstring& GetShadowDepthRTName() const { return m_ShadowDepthName; }
	[[nodiscard]] const std::wstring& GetPointShadowDepthRTName() const { return m_PointShadowDepthName; }
	[[nodiscard]] const std::vector<std::wstring>& GetOwnedRTNames() const { return m_OwnedRTNames; }

	[[nodiscard]] Engine::RenderPassID GetShadowPassID() const { return m_ShadowPassID; }
	[[nodiscard]] Engine::RenderPassID GetPointShadowPassID() const { return m_PointShadowPassID; }
	[[nodiscard]] Engine::RenderPassID GetGeometryPassID() const { return m_GeometryPassID; }
	[[nodiscard]] Engine::RenderPassID GetLightingPassID() const { return m_LightingPassID; }
	[[nodiscard]] Engine::RenderPassID GetForwardPassID() const { return m_ForwardPassID; }
	[[nodiscard]] Engine::RenderPassID GetForwardTransparentPassID() const { return m_ForwardTransparentPassID; }

	[[nodiscard]] uint32 GetWidth() const { return m_Width; }
	[[nodiscard]] uint32 GetHeight() const { return m_Height; }
	[[nodiscard]] bool UsesScenePostProcess() const { return m_ProfilePasses.UsesScenePostProcess(); }

private:
	void CreateRenderTargets(uint32 width, uint32 height);
	void RegisterRenderPasses();

private:
	std::wstring m_Prefix;
	uint32 m_Width = 0;
	uint32 m_Height = 0;
	bool m_AcceptSceneMeshes = true;

	// 타깃 목록은 기존 구성에서는 고정 슬롯 순서, 프로필 구성에서는 생성 순서를 따른다.
	std::vector<std::wstring> m_OwnedRTNames;
	Engine::DeferredProfilePasses m_ProfilePasses;
	std::array<std::wstring, 6> m_LightingInputs;
	std::wstring m_ShadowDepthName;
	std::wstring m_PointShadowDepthName;
	std::wstring m_DepthName;
	std::wstring m_LightingColorName;

	// 렌더패스 ID
	Engine::RenderPassID m_ShadowPassID = Engine::INVALID_PASS_ID;
	Engine::RenderPassID m_PointShadowPassID = Engine::INVALID_PASS_ID;
	Engine::RenderPassID m_GeometryPassID = Engine::INVALID_PASS_ID;
	Engine::RenderPassID m_LightingPassID = Engine::INVALID_PASS_ID;
	Engine::RenderPassID m_ForwardPassID = Engine::INVALID_PASS_ID;
	Engine::RenderPassID m_ForwardTransparentPassID = Engine::INVALID_PASS_ID;

	Engine::DeferredSceneRenderer m_SceneRenderer;
};

END
