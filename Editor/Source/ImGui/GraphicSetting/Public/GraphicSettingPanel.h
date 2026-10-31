#pragma once

#include "ImGuiInterface.h"
#include "EditorSettings.h"

BEGIN(Editor)
struct GraphicSettingPanelDesc
{
	SDL_Window* Window = nullptr;
};

class GraphicSettingPanel final : public IImGui
{
public:
	GraphicSettingPanel();
	~GraphicSettingPanel() override = default;

	EResult Initialize(void* arg = nullptr);
	void Free() override;
	void Update(f32 dt) override;
	void Draw() override;
	void CaptureState(EditorWorkspace& workspace) const override;
	void RestoreState(const EditorWorkspace& workspace) override;

private:
	void RefreshDrafts();
	void ClearDraftChanges();
	void DrawWindowConfirmation();
	void DrawVSyncSettings();
	void DrawViewportSettings();
	void DrawSettingsStorageStatus();

private:
	SDL_Window* m_Window = nullptr;

	int32 m_WindowWidth = 1280;
	int32 m_WindowHeight = 720;
	bool m_Fullscreen = false;
	bool m_WindowDirty = false;

	float m_UIScale = 1.0f;
	bool m_UIScaleDirty = false;

	int32 m_TargetFPS = 0;
	bool m_TargetFPSDirty = false;

	bool m_VSync = true;
	bool m_VSyncDirty = false;

	wstring m_SelectedViewportName;
	std::map<wstring, ViewportWorkspace> m_ViewportDrafts;
	bool m_StartupSettingsNoticePending = true;
};

END