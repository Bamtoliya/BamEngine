#pragma once

#include <initializer_list>

#include "ImGuiInterface.h"
#include "ChannelFilter.h"
#include "PostProcess.h"

enum class EViewportResolutionMode : uint8
{
	Free,       // ImGui 패널 크기에 자동 맞춤 (현재 동작)
	Preset,     // 고정 프리셋 해상도
	Custom,     // 사용자 입력 해상도
};
struct ResolutionPreset
{
	const char* NameKey;
	uint32 Width;
	uint32 Height;
};
// 프리셋 테이블 (static constexpr)
static constexpr ResolutionPreset g_ResolutionPresets[] =
{
	{ "Viewport.Resolution4K",     3840, 2160 },
	{ "Viewport.ResolutionQHD",        2560, 1440 },
	{ "Viewport.ResolutionFHD",        1920, 1080 },
	{ "Viewport.ResolutionHD",         1280,  720 },
	{ "Viewport.ResolutionSD",          854,  480 },
	{ "Viewport.ResolutionIPhone",  1170, 2532 },
	{ "Viewport.ResolutionPortrait", 1080, 1920 },
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

struct ViewportPanelDesc
{
	wstring Name = L"Viewport";
	string DisplayNameKey;
	uint32 RenderTargetWidth = 1920;
	uint32 RenderTargetHeight = 1080;
	wstring InitialRenderTargetName;
};

BEGIN(Editor)
struct ViewportWorkspace;
class BaseViewportPanel : public IImGui
{
	using DESC = ViewportPanelDesc;
#pragma region Constructor&Destructor
public:
	BaseViewportPanel() { m_Name = L"Base Viewport Panel"; }
	virtual ~BaseViewportPanel() = default;
	virtual void Free();
protected:
	EResult OnInitialize(void* arg) override;
#pragma endregion

#pragma region Loop
public:
	virtual void Update(f32 dt) override;
	void Draw();
#pragma endregion

#pragma region Get & Set
public:
	string GetDisplayName() const override { return m_IsRenderTargetViewer ? LOCAL_FMT("Viewport.ViewerTitle", WStrToStr(m_SelectedRTName)) : IImGui::GetDisplayName(); };
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::Viewport; }
	virtual PostProcessChain* GetPostProcessChain() { return nullptr; }
	virtual const PostProcessChain* GetPostProcessChain() const { return nullptr; }
	EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Viewport; }
#pragma endregion

#pragma region Draw Image
protected:
	virtual void DrawCustomViewport();
	void DrawRenderTargetImage(const wstring& rtName, bool nativePixels = false);
#pragma endregion

#pragma region Options Bar
protected:
	void DrawOptionsBar();
	virtual void DrawCustomOptions();
	void DrawRenderTargetMenu();
	virtual float GetCustomOptionsWidth() const;
	virtual float GetProjectionButtonsWidth() const { return 0.0f; }
	virtual void DrawProjectionButtons() {}
	static float MeasureOptionLabels(std::initializer_list<const char*> labels);
	void DrawRTItemContextMenu(const wstring& rtName);
#pragma endregion

#pragma region Resolution & Ratio
public:
	static constexpr int32 MinResolutionDimension = 64;
	static constexpr int32 MaxResolutionWidth = 7680;
	static constexpr int32 MaxResolutionHeight = 4320;

	[[nodiscard]] ViewportWorkspace CaptureResolutionState() const;
	[[nodiscard]] bool TrySetResolutionState(const ViewportWorkspace& state);

	[[nodiscard]] EResult GetLastResolutionUpdateResult() const { return m_LastResolutionUpdateResult; }
	[[nodiscard]] bool HasValidRenderTargets() const { return m_RenderTargetsValid; }

protected:
	EResult CalculateRenderResolution(uint32 panelWidth, uint32 panelHeight);
	f32 GetAspectRatio(EAspectRatioMode mode) const;
	virtual EResult ResizeRenderTargets(uint32 width, uint32 height);
	void DrawResolutionMenu();
#pragma endregion

#pragma region Workspace Settings
public:
	void CaptureState(EditorWorkspace& workspace) const override;
	void RestoreState(const EditorWorkspace& workspace) override;
	virtual void RestoreViewportState(const ViewportWorkspace& state);
	virtual ViewportWorkspace CaptureViewportState() const;
#pragma endregion

#pragma region Variables
protected: //Viewport Display
	ChannelFilter m_ChannelFilter;
	string m_WindowID;
	bool m_IsRenderTargetViewer = false;
	wstring m_SelectedRTName;
	wstring m_DisplayRTName;
	ImVec2 m_ImageScreenPos = { 0.f, 0.f }; //Screen Absolute Position
	ImVec2 m_ImageSize = { 0.f, 0.f }; //Actual Image Size after Letterbox
	EViewportResolutionMode m_ResolutionMode = EViewportResolutionMode::Free;
	EAspectRatioMode        m_AspectRatioMode = EAspectRatioMode::Free;
	int32                   m_PresetIndex = 2; // 기본: 1920x1080
	// 패널 크기 (ImGui 윈도우 크기)
	uint32 m_PanelWidth = 1920;
	uint32 m_PanelHeight = 1080;
	// 실제 렌더링 해상도 (RT 크기)
	uint32 m_RenderWidth = 1920;
	uint32 m_RenderHeight = 1080;
	bool m_RenderTargetsValid = true;
	EResult m_LastResolutionUpdateResult = EResult::Success;
	// Custom 모드용 사용자 입력 값
	int32 m_CustomWidth = 1920;
	int32 m_CustomHeight = 1080;
protected:
	bool m_IsVisibleOnScreen = true;       // 도킹 탭 뒤에 숨겨졌는지 여부
	float m_TimeSinceLastRender = 0.0f;     // 비활성 프레임 쓰로틀링 타이머
#pragma endregion
};
END
