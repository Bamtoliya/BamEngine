#pragma once

#include "ImGuiInterface.h"
#include "EditorCamera.h"
#include "InspectorPanel.h"
#include "Grid.h"
#include "PostProcess.h"
#include "DebugRenderer.h"

enum class EViewportResolutionMode : uint8
{
	Free,       // ImGui 패널 크기에 자동 맞춤 (현재 동작)
	Preset,     // 고정 프리셋 해상도
	Custom,     // 사용자 입력 해상도
};
struct tagResolutionPreset
{
	const char* Name;
	uint32 Width;
	uint32 Height;
};
// 프리셋 테이블 (static constexpr)
static constexpr tagResolutionPreset g_ResolutionPresets[] =
{
	{ "3840 x 2160 (4K UHD)",     3840, 2160 },
	{ "2560 x 1440 (QHD)",        2560, 1440 },
	{ "1920 x 1080 (FHD)",        1920, 1080 },
	{ "1280 x 720  (HD)",         1280,  720 },
	{ "854  x 480  (SD)",          854,  480 },
	{ "1170 x 2532 (iPhone 14)",  1170, 2532 },
	{ "1080 x 1920 (Portrait FHD)", 1080, 1920 },
};
static constexpr uint32 g_PresetCount = sizeof(g_ResolutionPresets) / sizeof(g_ResolutionPresets[0]);
enum class EAspectRatioMode : uint8
{
	Free,       // 종횡비 제한 없음
	Ratio_16_9,
	Ratio_16_10,
	Ratio_4_3,
	Ratio_21_9,
	Ratio_1_1,
	Ratio_9_16, // 세로 모드
};

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
protected:
	using DESC = tagViewportPanelDesc;
#pragma region Constructor&Destructor
public:
	BaseViewportPanel() { m_Name = L"Base Viewport Panel"; }
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
	virtual void Update(f32 dt) override;
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

#pragma region Resolution & Ratio
protected:
	void CalculateRenderResolution(uint32 panelWidth, uint32 panelHeight);
	f32 GetAspectRatio(EAspectRatioMode mode) const;
	void ResizeRenderTargets(uint32 width, uint32 height);
	void DrawResolutionMenu();
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


#pragma region Resolution Control
protected:
	EViewportResolutionMode m_ResolutionMode = EViewportResolutionMode::Free;
	EAspectRatioMode        m_AspectRatioMode = EAspectRatioMode::Free;
	int32                   m_PresetIndex = 2; // 기본: 1920x1080
	// 패널 크기 (ImGui 윈도우 크기)
	uint32 m_PanelWidth = 1920;
	uint32 m_PanelHeight = 1080;
	// 실제 렌더링 해상도 (RT 크기)
	uint32 m_RenderWidth = 1920;
	uint32 m_RenderHeight = 1080;
	// Custom 모드용 사용자 입력 값
	int32 m_CustomWidth = 1920;
	int32 m_CustomHeight = 1080;
#pragma endregion

	//Debug Renderer
#ifdef _DEBUG 
protected:
	DebugRenderer m_DebugRenderer;
#endif // _DEBUG

	
#pragma endregion
};
END