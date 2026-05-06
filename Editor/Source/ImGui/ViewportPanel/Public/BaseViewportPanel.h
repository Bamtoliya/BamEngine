#pragma once

#include "ImGuiInterface.h"
#include "EditorCamera.h"
#include "InspectorPanel.h"
#include "Grid.h"
#include "PostProcess.h"
#include "DebugRenderer.h"

enum class EViewportCameraType : uint8
{
	Perspective,
	Orthographic,
};

struct tagViewportPanelDesc
{
	wstring Name = L"Viewport";
	EViewportCameraType CameraType = EViewportCameraType::Orthographic;
	uint32 RenderTargetWidth = 1920;
	uint32 RenderTargetHeight = 1080;
};

BEGIN(Editor)

class BaseViewportPanel : public ImGuiInterface
{
	using DESC = tagViewportPanelDesc;
#pragma region Constructor&Destructor
public:
	BaseViewportPanel() { m_Name = L"Viewport Panel"; }
	virtual ~BaseViewportPanel() = default;
public:
	virtual void Initialize(void* arg = nullptr);
protected:
	void PrepareRenderTargetsAndPasses(uint32 width, uint32 height);
	void PreparePipelines();
public:
	virtual void Free();
#pragma endregion

public:
	void Update(f32 dt);
public:
	void Draw();
#pragma region Options Bar
protected:
	void DrawOptionsBar();
protected:
	void DrawChannelViewButton();
	void DrawDimensionToggleButton();
#pragma endregion

#pragma region Rendering
protected:
	void SubmitLightingPass(Camera* camera);
	void SubmitPostProcessPass(uint32& slot, std::wstring& currentRT);
	void SubmitUIOverlayPass(const wstring& currentRT);
	void SubmitChannelPreviewPass(uint32& slot, wstring& currentRT);
#pragma endregion

#pragma region Input
protected:
	virtual void HandleInput(f32 dt);
#pragma endregion

#pragma region Camera
public:
	bool IsPerspective() const { return m_EditorCamera->GetCamera()->GetIsPerspective(); }
	bool IsOrthographic() const { return m_EditorCamera->GetCamera()->GetIsOrthographic(); }
	void SetPerspective(bool perspective = true) { m_EditorCamera->GetCamera()->SetPerspective(perspective); }
	void SetOrthographic(bool orthographic = true) { m_EditorCamera->GetCamera()->SetOrthographic(orthographic); }
#pragma endregion

#pragma region Members
protected:
	EditorCamera* m_EditorCamera = { nullptr };
	EViewportCameraType m_CameraType = EViewportCameraType::Orthographic;
	InspectorPanel* m_InspectorPanel = { nullptr };
	
	vector<tagViewportPassInfo> m_PassOptions;

	//Render Passes & Render Targets
	RenderPassID m_GeometryPassID			= INVALID_PASS_ID;
	RenderPassID m_LightingPassID			= INVALID_PASS_ID;
	RenderPassID m_DebugPassID				= INVALID_PASS_ID;
	RenderPassID m_ShadowPassID				= INVALID_PASS_ID;
	RenderPassID m_ForwardTransparentPassID = INVALID_PASS_ID;
	RenderPassID m_ChannelPreviewPassID		= INVALID_PASS_ID;
	RenderPassID m_UIOverlayPassID			= INVALID_PASS_ID;

	wstring m_ShadowDepthName;
	wstring m_LightingColorName;

	wstring m_SelectedRTName;
	wstring m_DisplayRTName;

	vector<wstring> m_OwnedRTNames;
	
	bool m_ShowFinalComposed = true;


	vector<tagPPEffect>     m_PPEffects;
	tagToneMappingParams    m_ToneMappingParams;

	wstring					m_PPRTNames[2]; // ping-pong A / B
	vector<RenderPassID>	m_PPPassIDs;

	//Pipelines
	class RHIPipeline* m_LightingPipeline = { nullptr };
	class RHIPipeline* m_ChannelPreviewPipeline = { nullptr };
	EViewportChannelView m_ChannelView = EViewportChannelView::RGBA;

	//Grid
	Grid m_Grid;
	bool m_ShowGrid = { true };


	//Debug Renderer
#ifdef _DEBUG 
protected:
	DebugRenderer m_DebugRenderer;
#endif // _DEBUG

	
#pragma endregion
};
END