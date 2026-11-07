#pragma once

#include "Editor_Includes.h"
#include "ViewportDeferredPipeline.h"
#include "PostProcess.h"
#include "Grid.h"
#include "CameraBuffer.h"
#include <entt/entt.hpp>
#include <memory>
#include <string>
#include "ChannelFilter.h"
#include "InputMap.h"
#include "ImGuiSplitter.h"

BEGIN(Engine)
class Scene;
class Entity;
struct BoundingBox;
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
	bool SetModelFocusTarget(const Engine::Entity& model, bool refocus = true);
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
	[[nodiscard]] PostProcessChain& GetPostProcessChain() { return m_PostProcessChain; }
	[[nodiscard]] const PostProcessChain& GetPostProcessChain() const { return m_PostProcessChain; }

	static void BeginShortcutFrame() { s_ShortcutTarget = nullptr; }
	static PreviewViewportWidget* GetShortcutTarget() { return s_ShortcutTarget; }
	bool CanFocusTarget() const;
	void RefocusTarget();
	void ToggleGrid() { m_ShowGrid = !m_ShowGrid; }
	[[nodiscard]] bool IsGridVisible() const { return m_ShowGrid; }

private:
	bool TryGetModelBounds(entt::entity root, Engine::BoundingBox& bounds) const;
	bool FrameModelBounds(const Engine::BoundingBox& bounds);
	void ApplyFocus(const Engine::vec3& targetPos, Engine::f32 distance);
	void CheckResize(uint32 targetWidth, uint32 targetHeight);
	void DrawOverlayToolbar();
	void DrawRenderTargetPopup();
	void DrawPreviewLightingPopup();

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
	inline static PreviewViewportWidget* s_ShortcutTarget = nullptr;
	Engine::vec3 m_FocusPosition = Engine::vec3(0.0f);
	Engine::f32 m_FocusDistance = 3.0f;
	entt::entity m_FocusModel = entt::null;
	bool m_ModelFocusPending = false;
	std::optional<Engine::EMouseButton> m_CameraDragButton;

	// 빛
	std::optional<std::array<float, 3>> m_PreviewLightDraft;
	std::array<entt::entity, 3> m_StudioLights{ entt::null, entt::null, entt::null };

	// 채널 필터링
	std::wstring m_SelectedRTName;
	ChannelFilter m_ChannelFilter;
	EViewportChannelView m_ChannelView = EViewportChannelView::RGBA;
};

END
