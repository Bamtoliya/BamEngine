#pragma once

#include "CameraViewportPanel.h"
#include "DebugRenderer.h"
#include "EditorCamera.h"
#include "Grid.h"
#include "ImGuizmo.h"

BEGIN(Editor)

struct tagSceneViewportPanelDesc : public tagCameraViewportPanelDesc
{

};
class SceneViewportPanel : public CameraViewportPanel
{
	using DESC = tagSceneViewportPanelDesc;
#pragma region Constructor&Destructor
public:
	SceneViewportPanel() { m_Name = L"Scene Viewport"; }
	virtual ~SceneViewportPanel() = default;
public:
	virtual void Initialize(void* arg = nullptr) override;
public:
	virtual void Free();
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
	virtual void DrawCustomViewport() override;
private:
protected:
	void DrawImGuizmo();
	void DrawImViewGuizmo();
protected:
	void DrawLightOverlay();
	void DrawCameraOverlay();
protected:
	virtual void DrawCustomOptions() override;
protected:
	void DrawSceneRenderTargetMenu();
	void DrawGizmoMenu();
	void DrawDebugMenu();
	void DrawPostProcessMenu();
	void DrawRenderPassMenu();
#pragma endregion

#pragma region Resolution & Aspect
protected:
	virtual void ResizeRenderTargets(uint32 width, uint32 height) override;
#pragma endregion


#pragma region Render Passes
private:
	void SubmitLightingPass(class Camera* camera);
	void SubmitPostProcessPass(f32 dt, uint32& slot, std::wstring& currentRT);
	void SubmitUIOverlayPass(const wstring& currentRT);
#pragma endregion

#pragma region Inputs
private:
	void KeyboardInput();
private:
	void MouseInput();
private:
	Engine::Ray ScreenPosToRay(const ImVec2& mousePos);
#pragma endregion

#pragma region Member Variables
private:
	vector<tagViewportPassInfo> m_PassOptions;
	//Render Passes & Render Targets
	RenderPassID m_GeometryPassID = INVALID_PASS_ID;
	RenderPassID m_LightingPassID = INVALID_PASS_ID;
	RenderPassID m_DebugPassID = INVALID_PASS_ID;
	RenderPassID m_ShadowPassID = INVALID_PASS_ID;
	RenderPassID m_ForwardPassID = INVALID_PASS_ID;
	RenderPassID m_ForwardTransparentPassID = INVALID_PASS_ID;
	RenderPassID m_ChannelPreviewPassID = INVALID_PASS_ID;
	RenderPassID m_UIOverlayPassID = INVALID_PASS_ID;

	wstring m_ShadowDepthName;
	wstring m_LightingColorName;

	vector<wstring> m_OwnedRTNames;

	bool m_ShowFinalComposed = true;
private:
	PostProcessChain		m_PostProcessChain;

	wstring					m_PPRTNames[2]; // ping-pong A / B
	vector<RenderPassID>	m_PPPassIDs;

	//Pipelines
	class RHIPipeline* m_LightingPipeline = { nullptr };

	//Grid
	Grid m_Grid;
	bool m_ShowGrid = { true };
private:
	vec2 m_InitialMousePos;

#pragma region Guizmo 
private:
	ImGuizmo::OPERATION m_GizmoOperation = { ImGuizmo::OPERATION::TRANSLATE };
	ImGuizmo::MODE m_GizmoMode = { ImGuizmo::MODE::LOCAL };
	bool m_GizmoUseSnap = { false };
	vec3 m_GizmoSnapTranslation = { 1.0f, 1.0f, 1.0f };
	vec3 m_GizmoSnapRotation = { 15.0f, 15.0f, 15.0f };
	vec3 m_GizmoSnapScale = { 0.1f, 0.1f, 0.1f };
#pragma endregion

#ifdef _DEBUG 
protected:
	DebugRenderer m_DebugRenderer;
	bool m_ShowLightOverlay = { true };
	bool m_ShowCameraOverlay = { true };
#endif // _DEBUG
#pragma endregion
};
END