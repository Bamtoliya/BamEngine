#pragma once

#include "CameraViewportPanel.h"
#include "ImGuizmo.h"
#include "Grid.h"


enum class E2DGuizmoHandle {
	None, Body,
	TopLeft, TopCenter, TopRight,
	MiddleLeft, MiddleRight,
	BottomLeft, BottomCenter, BottomRight,
	Rotate
};

BEGIN(Engine)
class UICanvas;
class GameObject;
END

BEGIN(Editor)
class UIViewportPanel : public CameraViewportPanel
{
#pragma region Constructor&Destructor
public:
	UIViewportPanel() { m_Name = L"UI Viewport Panel"; };
	virtual ~UIViewportPanel() = default;
public:
	virtual void Initialize(void* arg = nullptr);
public:
	virtual void Free();
#pragma endregion

#pragma region Prepare
private:
	void PrepareRenderTargetsAndPasses(uint32 width, uint32 height);
#pragma endregion

#pragma region Loop
public:
	virtual void Update(f32 dt) override;
public:
	void Draw();
#pragma endregion

#pragma region Custom Draws
protected:
	virtual void DrawCustomViewport() override;
	virtual void DrawCustomOptions() override;
#pragma endregion

#pragma region Viewport
protected:
	void ClampViewOffset();
#pragma endregion


#pragma region Options Bar
protected:
	void DrawGizmoMenu();
	void DrawLayoutMenu();
#pragma endregion

#pragma region Overlays
private:
	void DrawCanvasBounds(ImDrawList* drawList, float displayRTWidth, float displayRTHeight, const mat4& view, const mat4& proj);
private:
	void DrawSelectionLocked();
	void DrawUIPreviewOverlay();
	void DrawUIPreviewMenu();
	void Draw2DGuizmo();
	void DrawDebugMenu();
	void DrawCanvasOverlay();
#pragma endregion

#pragma region Resolution & Aspect
protected:
	virtual void ResizeRenderTargets(uint32 width, uint32 height) override;
#pragma endregion

#pragma region Rendering
protected:
	void SubmitUIOverlayPass(const wstring& currentRT);
#pragma endregion

#pragma region Inputs
private:
	void KeyboardInput();
	void MouseInput();
private:
	Engine::Ray ScreenPosToRay(const ImVec2& mousePos);
#pragma endregion

#pragma region Canvas
private:
	class UICanvas* FindCanvasFromObject(class GameObject* object) const;
	class UICanvas* ResolveTargetCanvas() const;
	void SyncCanvasLayoutOverride();
	void ClearCanvasLayoutOverride();
#pragma endregion


#pragma region Member Variable
private:
	wstring m_UIColorRTName;
	wstring m_VirtualCanvasRTName;
	wstring m_CanvasToDraw; // 실제로 그려지는 캔버스 (UI Preview용 가상 캔버스)
private:
	Grid m_Grid;
	bool m_ShowGrid = true;

	bool m_EnableUIPreview = true;
	int32 m_UIDesignWidth = 1920;
	int32 m_UIDesignHeight = 1080;
	bool m_ShowSafeArea = true;
	float m_UISafeMarginX = 0.05f;
	float m_UISafeMarginY = 0.05f;
private:
	vec2 m_InitialMousePos;
private:
	RenderPassID m_UIOverlayPassID = INVALID_PASS_ID;
	RenderPassID m_ClearPassID = INVALID_PASS_ID;
private://Guizmo
	ImGuizmo::OPERATION m_GizmoOperation = { ImGuizmo::OPERATION::TRANSLATE };
	ImGuizmo::MODE m_GizmoMode = { ImGuizmo::MODE::LOCAL };
	bool m_GizmoUseSnap = { false };
	vec3 m_GizmoSnapTranslation = { 1.0f, 1.0f, 1.0f };
	vec3 m_GizmoSnapRotation = { 15.0f, 15.0f, 15.0f };
	vec3 m_GizmoSnapScale = { 0.1f, 0.1f, 0.1f };

	E2DGuizmoHandle m_ActiveHandle2D = E2DGuizmoHandle::None;
	bool            m_IsDragging2D = false;
private:
	bool   m_IsPanning = false;
	ImVec2 m_PanStartMouse = ImVec2(0.0f, 0.0f);
	vec3   m_PanStartCameraPos = vec3(0.0f, 0.0f, 0.0f);
private:
	class UICanvas* m_TargetCanvas = nullptr;
	uint64 m_LayoutOwnerToken = 0;
	bool m_AutoBindSelectedCanvas = true;

#pragma endregion
};
END