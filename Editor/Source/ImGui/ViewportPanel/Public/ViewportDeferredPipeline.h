#pragma once

#include "Editor_Includes.h"
#include "RenderPassTypes.h"
#include "CameraBuffer.h"
#include <string>
#include <vector>

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
	void Initialize(const std::wstring& viewportPrefix, uint32 width, uint32 height, bool acceptSceneMeshes = true);
	void Free();

	// 2. 해상도 리사이즈: 뷰포트 패널 크기가 바뀔 때 RT 리사이즈
	void Resize(uint32 width, uint32 height);

	// 3. 매 프레임 렌더링 제출 (뷰포트 Update에서 호출)
	//    targetScene: 렌더링할 타겟 씬 (nullptr이면 기본 activeScenes 렌더링)
	void Submit(const CameraBuffer& cameraBuffer, Engine::Scene* targetScene = nullptr);

	// 4. 게터 (최종 조명 합성 완료 텍스처 및 주요 패스 정보)
	[[nodiscard]] const std::wstring& GetFinalColorRTName() const { return m_LightingColorName; }
	[[nodiscard]] const std::wstring& GetDepthRTName() const { return m_DepthName; }
	[[nodiscard]] Engine::RenderPassID GetGeometryPassID() const { return m_GeometryPassID; }
	[[nodiscard]] Engine::RenderPassID GetLightingPassID() const { return m_LightingPassID; }
	[[nodiscard]] Engine::RenderPassID GetShadowPassID() const { return m_ShadowPassID; }
	[[nodiscard]] uint32 GetWidth() const { return m_Width; }
	[[nodiscard]] uint32 GetHeight() const { return m_Height; }

private:
	void CreateRenderTargets(uint32 width, uint32 height);
	void RegisterRenderPasses();
	void CreateLightingPipeline();
	void SubmitLightingPass(const CameraBuffer& cameraBuffer, Engine::Scene* targetScene);

private:
	std::wstring m_Prefix;
	uint32 m_Width = 0;
	uint32 m_Height = 0;
	bool m_AcceptSceneMeshes = true;

	// 렌더타겟 이름 목록
	std::vector<std::wstring> m_OwnedRTNames;
	std::wstring m_ShadowDepthName;
	std::wstring m_DepthName;
	std::wstring m_LightingColorName;

	// 렌더패스 ID
	Engine::RenderPassID m_ShadowPassID = Engine::INVALID_PASS_ID;
	Engine::RenderPassID m_GeometryPassID = Engine::INVALID_PASS_ID;
	Engine::RenderPassID m_LightingPassID = Engine::INVALID_PASS_ID;

	// 풀스크린 쿼드 조명 합성 파이프라인
	Engine::RHIPipeline* m_LightingPipeline = nullptr;
};

END