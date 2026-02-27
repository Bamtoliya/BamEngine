#pragma once

#include "Base.h"
#include "RenderTarget.h"
#include "ImGuizmo.h"
#include "EditorCamera.h"
#include "InspectorPanel.h"
#include "ImGuiInterface.h"

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
	void DrawOptionsBar();
private:
	void DrawEditorCameraInspector();
private:
	void MouseInput(const ImVec2& mousePos, const ImVec2& imageMin, const ImVec2& imageSize);

#pragma region MyRegion
private:
	Ray ScreenPosToRay(const ImVec2& mousePos, const ImVec2& imageMin, const ImVec2&imageSize);
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
private:
	InspectorPanel* m_InspectorPanel = { nullptr };
private:
	EViewportCameraType m_CameraType = EViewportCameraType::Orthographic;
	EViewportMode m_ViewportMode = EViewportMode::Textured;
	uint32 m_PassID = { 0 };
	uint32 m_DebugPassID = { 0 };

#pragma endregion
};
END