#pragma once

#include "Base.h"
#include "ImGuiInterface.h"
#include "ImGuizmo.h"
#include "RenderTarget.h"
#include "EditorCamera.h"
#include "InspectorPanel.h"
#include "Grid.h"
#include "Application.h"

enum class EViewportMode : uint8
{
	Wireframe,
	Solid,
	Textured,
};

enum class EViewportCameraType : uint8
{
	Perspective,
	Orthographic,
};

enum class EViewportType : uint8
{
	Scene,
	Game,
	Editor,
	Max
};

struct tagViewportPanelDesc
{
	wstring Name = L"Viewport";
	EViewportType Type = EViewportType::Scene;
	EViewportCameraType CameraType = EViewportCameraType::Orthographic;
	EViewportMode ViewportMode = EViewportMode::Textured;
	uint32 RenderTargetWidth = 1920;
	uint32 RenderTargetHeight = 1080;
};

ENUM()
enum class EViewportChannelView : uint8
{
	None = 0,
	R = 1 << 0,
	G = 1 << 1,
	B = 1 << 2,
	A = 1 << 3,
	RGB = R | G | B,
	RGBA = R | G | B | A
};

ENABLE_BITMASK_OPERATORS(EViewportChannelView)

struct ChannelViewData
{
	uint32 Flags = 0;
	vec3 Padding = vec3(0.0f);
};

BEGIN(Editor)

class ViewportPanel : public ImGuiInterface
{
	using DESC = tagViewportPanelDesc;

#pragma region Constructor&Destructor
public:
	ViewportPanel() { m_Name = L"Viewport Panel"; }
	virtual ~ViewportPanel() = default;
public:
	void Initialize(void* arg = nullptr);
	void Free();
#pragma endregion

public:
	void Update(f32 dt);
public:
	void Draw();
private:
	void DrawGuizmo(ImVec2 pos, ImVec2 size);
	void DrawLightOverlay(const ImVec2& imageScreenPos, const ImVec2& imageSize);
	void DrawCameraOverlay(const ImVec2& imageScreenPos, const ImVec2& imageSize);
#pragma region Options Bar
private:
	void DrawOptionsBar();
private:
	void DrawChannelViewButton();
	void DrawDimensionToggleButton();
#pragma endregion

#pragma region Input
private:
	void MouseInput(const ImVec2& mousePos, const ImVec2& imageMin, const ImVec2& imageSize);
private:
	Ray ScreenPosToRay(const ImVec2& mousePos, const ImVec2& imageMin, const ImVec2&imageSize);
#pragma endregion

#pragma region Camera & Play
private:
	EPlayState GetPlayState() const;
	bool CanControlEditorCamera() const;
	bool IsUsingSceneCamera() const;
	Camera* ResolveActiveCamera(bool allowPlayMain = true) const;
#pragma endregion



#pragma region Rendering
	// ─── Post Process ───────────────────────────────────────────────
public:
	struct tagPPEffect
	{
		wstring       Name;
		bool          Enabled = false;
		RHIPipeline* Pipeline = nullptr;
	};
	struct tagToneMappingParams
	{
		float exposure = 1.0f;
		float gamma = 2.2f;
		float _pad0 = 0.f;
		float _pad1 = 0.f;
	};
private:
	static constexpr uint32 MAX_PP_PASSES = 8;
	vector<tagPPEffect>     m_PPEffects;
	tagToneMappingParams    m_ToneMappingParams;

	wstring      m_PPRTNames[2];               // ping-pong A / B
	RenderPassID m_PPPassIDs[MAX_PP_PASSES] = {};
	// ────────────────────────────────────────────────────────────────
private:
	void SubmitLightingPass();
private:
	void SubmitChannelPreviewPass();
private:
	void SubmitPostProcessPass();
#pragma endregion


#pragma region Members
private:
	EditorCamera* m_EditorCamera = { nullptr };
	RenderTarget* m_RenderTarget = { nullptr };
	RenderTarget* m_DepthStencil = { nullptr };
private:
	bool m_IsFocused = { false };
	bool m_IsHovered = { false };
	bool m_IsOrthographic = { true };
private:
	ImGuizmo::OPERATION m_GizmoOperation = { ImGuizmo::OPERATION::TRANSLATE };
	ImGuizmo::MODE m_GizmoMode = { ImGuizmo::MODE::LOCAL };
	bool m_GizmoUseSnap = { false };
	vec3 m_GizmoSnapTranslation = { 1.0f, 1.0f, 1.0f };
	vec3 m_GizmoSnapRotation = { 15.0f, 15.0f, 15.0f };
	vec3 m_GizmoSnapScale = { 0.1f, 0.1f, 0.1f };
private:
	InspectorPanel* m_InspectorPanel = { nullptr };
private:
	EViewportCameraType m_CameraType = EViewportCameraType::Orthographic;
	EViewportMode m_ViewportMode = EViewportMode::Textured;
private:
	bool m_EditorMode = { true };
	//이걸 비트연산으로 바꿀까
	bool m_ShowGrid = { true };
	//bool m_ShowAxis = { true };
	//bool m_ShowBoundingBox = { true };
	//bool m_ShowLight = { true };
	//bool m_ShowCollider = { true };
	//bool m_ShowSkeleton = { true };
	//bool m_ShowDebugInfo = { true };
	bool m_ShowLightOverlay = { true };
	bool m_ShowCameraOverlay = { true };
	Camera* m_LockedSceneCamera = { nullptr };
#pragma region Rendering
private:
	RenderPassID m_GeometryPassID = INVALID_PASS_ID;
	RenderPassID m_LightingPassID = INVALID_PASS_ID;
	RenderPassID m_DebugPassID = INVALID_PASS_ID;
	RenderPassID m_ShadowPassID = INVALID_PASS_ID;
	RenderPassID m_ForwardTransparentPassID = INVALID_PASS_ID;
	wstring m_ShadowDepthName;
	wstring m_DisplayRenderTargetName;
	wstring m_FinalColorName;
	bool m_UsePostProcessOutputAsDisplay = false;
	wstring m_LastPostProcessOutputRT;
	vector<wstring> m_OwnedRTNames;    // Free()에서 해제할 RT 이름들
	class RHIPipeline* m_LightingPipeline = { nullptr };
private:
	EViewportChannelView m_ChannelView = EViewportChannelView::RGBA;
	wstring m_ChannelPreviewName;
	RenderPassID m_ChannelPreviewPassID = INVALID_PASS_ID;
	class RHIPipeline* m_ChannelPreviewPipeline = { nullptr };
#pragma endregion
private:
	vec2 m_InitialMousePos;
private:
	Grid m_Grid;

#pragma endregion
};
END