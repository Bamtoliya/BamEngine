#pragma once

#include "Editor_Includes.h"
#include <SDL3/SDL_events.h>
#include "ShortcutTypes.h"
#include "ImGuiSplitter.h"
#include "EditorUI.h"
#include "ShortcutManager.h"

BEGIN(Editor)
enum class EPanelMenuCategory
{
	Viewport,
	SceneEditing,
	Assets,
	GraphAnimation,
	Diagnostics,
	Other,
	Hidden
};
struct EditorWorkspace;
class IImGui : public Engine::Base
{
public:
	virtual ~IImGui() = default;
	[[nodiscard]] EResult Initialize(void* arg = nullptr);
	virtual void Draw() BAM_PURE;
	virtual void Update(f32 dt) {}
	virtual bool ShouldUpdateWhenClosed() const { return false; }
	virtual void ProcessEvent(const SDL_Event* event) {}
public:
	bool IsOpen() const { return m_Open; }
	void SetOpen(bool open) { m_Open = open; }
	void ToggleOpen() { m_Open = !m_Open; }
	void Open() { m_Open = true; }
	void Close() { m_Open = false; }
	void Focus() { m_Open = true; m_FocusRequested = true; }
	bool IsFocused() const { return m_Focused; }
	virtual EShortcutContext GetShortcutContext() const { return EShortcutContext::Global; }
public:
	const wstring& GetName() const { return m_Name; }
	void SetName(const wstring& name) { m_Name = name; m_DisplayNameKey.clear(); }
	virtual string GetDisplayName() const { return m_DisplayNameKey.empty() ? WStrToStr(m_Name) : LOCAL(m_DisplayNameKey); }
	virtual EPanelMenuCategory GetPanelMenuCategory() const { return EPanelMenuCategory::Other; }
public:
	virtual void CaptureState(EditorWorkspace& workspace) const;
	virtual void RestoreState(const EditorWorkspace& workspace);
protected:
	bool BeginPanelWindow(const char* title, bool& open, ImGuiWindowFlags flags = ImGuiWindowFlags_None);
	virtual EResult OnInitialize(void*) { return EResult::Success; }
protected:
	bool m_Open = true;
	bool m_Focused = false;
	bool m_Hovered = false;
	bool m_FocusRequested = false;
	wstring m_Name = L"ImGui Panel";
	string m_DisplayNameKey;
private:
	friend class ImGuiManager;
	bool m_Registered = false;
	ImGuiID m_PanelWindowId = 0;
	int32 m_CloseRequestFrame = -1;
};
END
