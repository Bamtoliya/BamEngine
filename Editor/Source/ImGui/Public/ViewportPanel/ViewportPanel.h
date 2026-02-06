#pragma once

#include "Base.h"
#include "RenderTarget.h"
#include "ImGuizmo.h"

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

BEGIN(Editor)

class ViewportPanel
{
public:
	ViewportPanel() {};
	virtual ~ViewportPanel() = default;
private:
	void Initialize(void* arg = nullptr);

public:
	void Draw();
private:
	void DrawGuizmo(ImVec2 size);
	void DrawOptionsBar();
#pragma region Members
private:
	bool m_IsFocused = { false };
	bool m_IsHovered = { false };
	bool m_IsOrthographic = { true };
private:
	ImGuizmo::OPERATION m_GizmoOperation = { ImGuizmo::OPERATION::TRANSLATE };
	ImGuizmo::MODE m_GizmoMode = { ImGuizmo::MODE::LOCAL };
	bool m_GizmoUseSnap = { false };
#pragma endregion
};
END