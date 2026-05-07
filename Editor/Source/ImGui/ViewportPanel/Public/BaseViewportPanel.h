#pragma once

#include "ImGuiInterface.h"
#include "ChannelFilter.h"
#include "PostProcess.h"

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

struct tagViewportPanelDesc
{
	wstring Name = L"Viewport";
	uint32 RenderTargetWidth = 1920;
	uint32 RenderTargetHeight = 1080;
};

BEGIN(Editor)

class BaseViewportPanel : public ImGuiInterface
{
	using DESC = tagViewportPanelDesc;
#pragma region Constructor&Destructor
public:
	BaseViewportPanel() { m_Name = L"Base Viewport Panel"; }
	virtual ~BaseViewportPanel() = default;
public:
	virtual void Initialize(void* arg = nullptr);
public:
	virtual void Free();
#pragma endregion

public:
	virtual void Update(f32 dt) override;
public:
	void Draw();

#pragma region Draw Image
protected:
	virtual void DrawCustomViewport();
protected:
	void DrawRenderTargetImage(const wstring& rtName);
#pragma endregion

#pragma region Options Bar
protected:
	void DrawOptionsBar();
	virtual void DrawCustomOptions();
protected:
	void DrawRenderTargetMenu();
	void DrawChannelViewButton();
#pragma endregion

#pragma region Resolution & Ratio
protected:
	void CalculateRenderResolution(uint32 panelWidth, uint32 panelHeight);
	f32 GetAspectRatio(EAspectRatioMode mode) const;
	virtual void ResizeRenderTargets(uint32 width, uint32 height);
	void DrawResolutionMenu();
#pragma endregion


#pragma region Members
protected: //Viewport Display
	ChannelFilter m_ChannelFilter;
	EViewportChannelView m_ChannelView = EViewportChannelView::RGBA;

	wstring m_SelectedRTName;
	wstring m_DisplayRTName;
protected:
	ImVec2 m_ImageScreenPos = { 0.f, 0.f }; //Screen Absolute Position
	ImVec2 m_ImageSize = { 0.f, 0.f }; //Actual Image Size after Letterbox
protected: //Resolution & Aspect Ratio
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
};
END