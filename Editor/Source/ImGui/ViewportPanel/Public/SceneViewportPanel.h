#pragma once

#include "CameraViewportPanel.h"
#include "DebugRenderer.h"
#include "Grid.h"
#include "ViewportDeferredPipeline.h"
#include "SceneEditHistory.h"
#pragma push_macro("COLOR")
#undef COLOR

#include "ImGuizmo.h"

#pragma pop_macro("COLOR")

BEGIN(Engine)
struct PostProcessComponent;
END

BEGIN(Editor)

enum class EScenePickingMode : uint8
{
	GPUId,
	RayBounds,
};

struct SceneViewportPanelDesc : public CameraViewportPanelDesc
{

};

class SceneViewportPanel : public CameraViewportPanel
{
	using DESC = SceneViewportPanelDesc;
#pragma region Constructor&Destructor
public:
	SceneViewportPanel() { m_Name = L"Scene Viewport"; }
	virtual ~SceneViewportPanel() = default;
public:
	virtual void Initialize(void* arg = nullptr) override;
public:
	virtual void Free();
	virtual PostProcessChain* GetPostProcessChain() override { return &m_PostProcessChain; }
	virtual const PostProcessChain* GetPostProcessChain() const override { return &m_PostProcessChain; }
public:
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::Viewport; }
	static SceneViewportPanel* GetActiveSceneViewport() { return s_ActiveSceneViewport; }
	void SetGizmoOperation(ImGuizmo::OPERATION op) { m_GizmoOperation = op; }
	ImGuizmo::OPERATION GetGizmoOperation() const { return m_GizmoOperation; }
	void ToggleGizmoMode() { m_GizmoMode = (m_GizmoMode == ImGuizmo::LOCAL) ? ImGuizmo::WORLD : ImGuizmo::LOCAL; }
	void ToggleSnap() { m_GizmoUseSnap = !m_GizmoUseSnap; }
	void ToggleGrid() { m_Grid.Toggle(); }
	void FocusSelectedEntity();
private:
	inline static SceneViewportPanel* s_ActiveSceneViewport = nullptr;
#pragma endregion

#pragma region Workspace Settings
public:
	void RestoreViewportState(const ViewportWorkspace& state) override;
	ViewportWorkspace CaptureViewportState() const override;
#pragma endregion

#pragma region Prepare Rendering Pipeline
private:
	void PrepareRenderTargetsAndPasses(uint32 width, uint32 height);
	void PreparePipelines();
#pragma endregion

#pragma region Loop
public:
	virtual void Update(f32 dt) override;
public:
	virtual void Draw() override;
#pragma endregion

#pragma region Custom Draws
protected:
	virtual void DrawCustomOptions() override;
	virtual void DrawCustomViewport() override;
#pragma endregion

#pragma region Options Bar
protected:
	void DrawSceneRenderTargetMenu();
	void DrawGizmoMenu();
	void DrawDebugMenu();
	void DrawPostProcessMenu();
	void DrawRenderPassMenu();
	void DrawPickingMenu();
#pragma endregion

#pragma region Overlay
protected:
	void DrawImGuizmo();
	void DrawImViewGuizmo();
protected:
	void DrawLightOverlay();
	void DrawCameraOverlay();
	void DrawSelectionLocked();
#pragma endregion

#pragma region Resolution & Aspect
protected:
	EResult ResizeRenderTargets(uint32 width, uint32 height) override;
#pragma endregion

#pragma region Render Passes
private:
	bool SyncPostProcess(Engine::Scene& scene, entt::entity entity, const Engine::PostProcessComponent& source);
	void SubmitPostProcessPass(f32 dt, uint32& slot, std::wstring& currentRT);
	void SubmitUIOverlayPass(const wstring& currentRT);
#pragma endregion

#pragma region Inputs
private:
	void KeyboardInput();
private:
	void MouseInput();
	void CapturePickingRequest(bool imageHovered);
	void ProcessPickingRequest();
private:
	bool ScreenPosToRay(const ImVec2& mousePos, Engine::Ray& ray, float& maxDistance);
	class Entity* PickOverlayEntity(const ImVec2& mousePos) const;
#pragma endregion

private:
	std::optional<CameraBuffer> GetActiveCameraBuffer();
	std::optional<CameraBuffer> FindActiveSceneCameraBuffer();
	void HandleTerrainSculpting(bool imageHovered);

#pragma region Member Variables
private:
	vector<ViewportPassInfo> m_PassOptions;
	ViewportDeferredPipeline m_Pipeline;
	// Render Passes
	RenderPassID m_GeometryPassID = INVALID_PASS_ID;
	RenderPassID m_LightingPassID = INVALID_PASS_ID;
	RenderPassID m_DebugPassID = INVALID_PASS_ID;
	RenderPassID m_ShadowPassID = INVALID_PASS_ID;
	RenderPassID m_PointShadowPassID = INVALID_PASS_ID;
	RenderPassID m_ForwardPassID = INVALID_PASS_ID;
	RenderPassID m_ForwardTransparentPassID = INVALID_PASS_ID;
	RenderPassID m_ChannelPreviewPassID = INVALID_PASS_ID;
	RenderPassID m_UIOverlayPassID = INVALID_PASS_ID;
	// 하위 호환 및 RT 메뉴 캐시 (m_Pipeline에서 공급받음)
	wstring m_ShadowDepthName;
	wstring m_PointShadowDepthName;
	wstring m_LightingColorName;
	vector<wstring> m_OwnedRTNames;
	bool m_ShowFinalComposed = true;
private:
	PostProcessChain m_PostProcessChain;
	string m_DefaultPostProcessSettings;
	string m_LastPostProcessPreset;
	string m_LastPostProcessSettings;
	Engine::Scene* m_LastBoundScene = nullptr;
	entt::entity m_LastPostProcessEntity = entt::null;
	bool m_PostProcessStateUsable = true;
	// Grid
	Grid m_Grid;
	bool m_ShowGrid = { true };
private:
	vec2 m_InitialMousePos;
	EScenePickingMode m_PickingMode = EScenePickingMode::GPUId;
	struct PickingRequest
	{
		bool pending = false;
		bool toggle = false;
		EScenePickingMode mode = EScenePickingMode::GPUId;
		vec2 uv = vec2(0.0f);
		Engine::Ray ray{};
		float maxDistance = 0.0f;
		uint64 frameSerial = 0;
		uint32 width = 0;
		uint32 height = 0;
	};
	PickingRequest m_PickingRequest;

#pragma region Guizmo 
private://3D
	ImGuizmo::OPERATION m_GizmoOperation = { ImGuizmo::OPERATION::TRANSLATE };
	ImGuizmo::MODE m_GizmoMode = { ImGuizmo::MODE::LOCAL };
	bool m_GizmoUseSnap = { false };
	bool m_GizmoShowBounds = { false };
protected:
	DebugRenderer m_DebugRenderer;
protected:
	bool m_ShowLightOverlay = { true };
	bool m_ShowCameraOverlay = { true };
private:
	SceneEditSnapshot m_SculptBeforeSnapshot{};
	bool m_IsSculptStrokeActive = false;
	bool m_SculptModified = false;
#pragma endregion
};
END
