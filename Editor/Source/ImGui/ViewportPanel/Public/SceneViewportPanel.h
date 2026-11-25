#pragma once

#include "CameraViewportPanel.h"
#include "DebugRenderer.h"
#include "Grid.h"
#include "ViewportDeferredPipeline.h"
#include "SceneEditHistory.h"
#include "SplineComponent.h"
#include "GizmoHelper.h"

BEGIN(Engine)
struct PostProcessComponent;
struct TerrainComponent;
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

class SceneViewportPanel : public CameraViewportPanel, public IGizmoHost
{
	using DESC = SceneViewportPanelDesc;
#pragma region Constructor&Destructor
public:
	SceneViewportPanel() { m_Name = L"Scene Viewport"; }
	virtual ~SceneViewportPanel() = default;
protected:
	EResult OnInitialize(void* arg) override;
public:
	virtual void Free();
	virtual PostProcessChain* GetPostProcessChain() override { return &m_PostProcessChain; }
	virtual const PostProcessChain* GetPostProcessChain() const override { return &m_PostProcessChain; }
public:
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::Viewport; }
	static SceneViewportPanel* GetActiveSceneViewport() { return s_ActiveSceneViewport; }
	bool IsRealtime() const { return m_IsRealtime; }
	void SetRealtime(bool realtime) { m_IsRealtime = realtime; }
	void ToggleRealtime() { m_IsRealtime = !m_IsRealtime; }
	void ToggleSnap() { ToggleGizmoSnap(); } // 기존 호출부 하위 호환
	void ToggleGrid() { m_Grid.Toggle(); }
	void ToggleNavGridOverlay() { m_DebugRenderer.ToggleShowNavGridOverlay(); }
	void BakeSceneNavGrid();
	bool HasActiveNavGrid() const;
	void ToggleNavMeshOverlay() { m_DebugRenderer.ToggleShowNavMeshOverlay(); }
	void BakeSceneNavMesh();
	bool HasActiveNavMesh() const;
	void FocusSelectedEntity();
	void RequestHDRCaptureFromCurrentLocation();

	void SelectSplinePoint(Engine::Scene* scene, entt::entity entity, int index);
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
	float GetCustomOptionsWidth() const override;
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
	// 데칼·프로브 GI 볼륨은 자기 표면이 없어 메시 피킹으로 고를 수 없으므로 상자 중심에 아이콘을 두고 그 아이콘으로 고른다.
	void DrawVolumeOverlay();
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
	void MouseInput();
	void CapturePickingRequest(bool imageHovered);
	void ProcessPickingRequest();
private:
	bool ScreenPosToRay(const ImVec2& mousePos, Engine::Ray& ray, float& maxDistance);
	class Entity* PickOverlayEntity(const ImVec2& mousePos) const;
#pragma endregion

#pragma region Camera
protected:
	void RequestProjectionMode(bool perspective) override;
#pragma endregion


private:
	std::optional<CameraBuffer> GetActiveCameraBuffer();
	void HandleTerrainSculpting(bool imageHovered);
	// 속성 페인트 중 지형 속성 맵(이동 불가·물)을 정점 점으로 그린다.
	void DrawTerrainAttributeOverlay(const Engine::TerrainComponent& terrain, const glm::mat4& worldMatrix);
	bool HandleTilemapPainting(bool imageHovered);

private:
	bool m_IsTilemapPaintingActive = false;
	entt::entity m_TilemapStrokeEntity = entt::null;
	uint64 m_TilemapStrokeRevision = 0;
	vec2 m_TileBoxFillStartCell{ 0.0f, 0.0f };
	bool m_IsTileBoxFillDragging = false;

#pragma region Spline
private:
	void DrawSelectedSpline(Engine::Scene* scene);
	// 선택한 DecalComponent의 투영 상자와 투영 방향(상자의 로컬 z)
	void DrawSelectedDecal(Engine::Scene* scene);
	// 선택한 프로브 GI 볼륨의 상자와 프로브 위치(벽 밖으로 옮기기 전)
	void DrawSelectedProbeVolume(Engine::Scene* scene);
	bool DrawSplinePointEditor(bool imageHovered);
	void ResetSplinePointSelection();
	void StopSplinePointDrag();

	ImGuizmo::OPERATION m_SplinePointDragOperation = static_cast<ImGuizmo::OPERATION>(0);
	mat4 m_SplinePointDragOwnerWorld{ 1.0f };

	Engine::Scene* m_SplinePointScene = nullptr;
	entt::entity m_SplinePointEntity = entt::null;
	uint64 m_SplinePointOwnerId = 0;
	uint64 m_SplinePointRevision = 0;
	int m_SplinePointIndex = -1;

	std::vector<Engine::SplinePoint> m_SplinePointExpected;
	bool m_SplinePointWasUsing = false;
	bool m_SplinePointBlockUntilRelease = false;

	static constexpr size_t MaxSplinePreviewSegments = 256;

	std::vector<Engine::SplinePoint> m_SplinePreviewSource;
	std::vector<vec3> m_SplinePreviewSamples;
	bool m_SplinePreviewClosed = false;
	bool m_SplinePreviewCached = false;
#pragma endregion

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
	bool m_IsRealtime = false;
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
	bool m_GizmoShowBounds = { false };
protected:
	DebugRenderer m_DebugRenderer;
protected:
	bool m_ShowLightOverlay = { true };
	bool m_ShowCameraOverlay = { true };
	bool m_ShowVolumeOverlay = { true };
private:
	SceneEditSnapshot m_SculptBeforeSnapshot{};
	bool m_IsSculptStrokeActive = false;
	bool m_SculptModified = false;
#pragma endregion

#pragma region Camera
private:
	struct PerspectiveView
	{
		vec3 Position;
		vec3 Rotation;
		vec3 Pivot;
	};

	std::optional<PerspectiveView> m_LastPerspectiveView;
	std::optional<vec3> m_LastXYPosition;
#pragma endregion

#pragma region HDR Capture
private:
	void ProcessHDRCapture(Engine::Scene* activeScene);
	void CancelHDRCapture();

	struct HDRCaptureState
	{
		static constexpr uint32 FaceSize = 512;
		static constexpr uint32 EquirectWidth = 1024;
		static constexpr uint32 EquirectHeight = 512;

		bool Active = false;
		vec3 Origin{ 0.0f };
		f32 FarPlane = 10000.0f;
		std::wstring OutputPath;
		Engine::Scene* TargetScene = nullptr;
		int32 SubmittedFace = -1;
		int32 NextFace = 0;
		ViewportDeferredPipeline Pipeline;
		std::array<std::vector<uint8>, 6> FacePixels;
	};
	HDRCaptureState m_HDRCapture;
#pragma endregion

};
END
