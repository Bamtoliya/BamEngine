#pragma once

#include "Editor_Includes.h"
#include "ViewportDeferredPipeline.h"
#include "PostProcess.h"
#include "Grid.h"
#include "CameraBuffer.h"
#include <entt/entt.hpp>
#include <memory>
#include <string>

BEGIN(Engine)
class Scene;
class Entity;
END

BEGIN(Editor)

class EditorCameraController;

class PreviewViewportWidget
{
public:
	PreviewViewportWidget() = default;
	~PreviewViewportWidget();

	void Initialize(const std::wstring& widgetId, uint32 width = 512, uint32 height = 512, bool enableGrid = true);
	void Free();

	// ── 씬 & 엔티티 인터페이스 ──
	[[nodiscard]] Engine::Scene* GetScene() const { return m_PreviewScene; }
	[[nodiscard]] entt::registry& GetRegistry();
	Engine::Entity& CreateEntity(const std::wstring& name = L"PreviewEntity");
	void SetupDefaultStudioLighting();

	// ── 카메라 & 조작 인터페이스 ──
	[[nodiscard]] EditorCameraController* GetCameraController() const { return m_CameraController; }
	void FocusTarget(const Engine::vec3& targetPos, Engine::f32 distance = 3.5f);
	void SetCameraSpeed(Engine::f32 speed);

	// ── 프레임 틱 및 렌더링 ──
	void BeginFrame(Engine::f32 dt);
	void Submit(Engine::f32 dt);
	void Draw(const ImVec2& size = ImVec2(0.0f, 0.0f));

	// ── 상태 조회 ──
	[[nodiscard]] bool IsHovered() const { return m_IsHovered; }
	[[nodiscard]] bool IsFocused() const { return m_IsFocused; }
	[[nodiscard]] uint32 GetWidth() const { return m_Width; }
	[[nodiscard]] uint32 GetHeight() const { return m_Height; }

	// ── 텍스처 조회 ──
	[[nodiscard]] const std::wstring& GetDisplayRTName() const { return m_DisplayRTName; }
	[[nodiscard]] Engine::RHITexture* GetPreviewRHITexture() const;

private:
	void CheckResize(uint32 targetWidth, uint32 targetHeight);
	void DrawOverlayToolbar();

private:
	std::wstring m_Id;
	uint32 m_Width = 512;
	uint32 m_Height = 512;

	// 렌더링 파이프라인 & 후처리
	ViewportDeferredPipeline m_Pipeline;
	PostProcessChain m_PostProcessChain;
	std::wstring m_DisplayRTName;

	// 독립 씬 & 카메라
	Engine::Scene* m_PreviewScene = nullptr;
	EditorCameraController* m_CameraController = nullptr;

	// 바닥 그리드
	Grid m_Grid;
	bool m_ShowGrid = true;

	// UI 상호작용 상태
	bool m_IsHovered = false;
	bool m_IsFocused = false;
};

END