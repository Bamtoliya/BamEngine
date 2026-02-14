#pragma once

#include "Base.h"
#include "RenderTarget.h"
#include "ImGuizmo.h"
#include "EditorCamera.h"
#include "InspectorPanel.h"

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
};

BEGIN(Editor)

class ViewportPanel
{
	using DESC = tagViewportPanelDesc;

#pragma region Constructor&Destructor
public:
	ViewportPanel() {};
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
	void DrawGuizmo(ImVec2 size);
	void DrawOptionsBar();
private:
	void DrawEditorCameraInspector();
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
	InspectorPanel m_InspectorPanel;
private:
	wstring m_ViewportName = L"Viewport";
	uint32 m_PassID = { 0 };

#pragma endregion
};
END