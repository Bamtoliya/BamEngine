#pragma once

#include "Editor_Includes.h"
#include <SDL3/SDL_events.h>
#include "ShortcutTypes.h"

BEGIN(Editor)
struct EditorWorkspace;
class IImGui : public Engine::Base
{
public:
	virtual ~IImGui() = default;
	virtual void Draw() BAM_PURE;
	virtual void Update(f32 dt) {}
	virtual void ProcessEvent(const SDL_Event* event) {}
public:
	bool IsOpen() const { return m_Open; }
	void SetOpen(bool open) { m_Open = open; }
	void ToggleOpen() { m_Open = !m_Open; }
	void Open() { m_Open = true; }
	void Close() { m_Open = false; }
	bool IsFocused() const { return m_Focused; }
	virtual EShortcutContext GetShortcutContext() const { return EShortcutContext::Global; }
public:
	const wstring& GetName() const { return m_Name; }
	void SetName(const wstring& name) { m_Name = name; m_DisplayNameKey.clear(); }
	virtual string GetDisplayName() const { return m_DisplayNameKey.empty() ? WStrToStr(m_Name) : LOCAL(m_DisplayNameKey); }
public:
	virtual void CaptureState(EditorWorkspace& workspace) const;
	virtual void RestoreState(const EditorWorkspace& workspace);
protected:
	bool m_Open = true;
	bool m_Focused = false;
	bool m_Hovered = false;
	wstring m_Name = L"ImGui Panel";
	string m_DisplayNameKey;
};
END
