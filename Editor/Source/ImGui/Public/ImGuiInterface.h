#pragma once

#include "Editor_Includes.h"

#include <SDL3/SDL_events.h>

BEGIN(Editor)
class ImGuiInterface : public Engine::Base
{
public:
	virtual ~ImGuiInterface() = default;
	virtual void Draw() BAM_PURE;
	virtual void Update(f32 dt) {}
	virtual void ProcessEvent(const SDL_Event* event) {}
public:
	bool IsOpen() const { return m_Open; }
	void SetOpen(bool open) { m_Open = open; }
	void ToggleOpen() { m_Open = !m_Open; }
	void Open() { m_Open = true; }
	void Close() { m_Open = false; }
public:
	const wstring& GetName() const { return m_Name; }
	void SetName(const wstring& name) { m_Name = name; }
protected:
	bool m_Open = true;
	wstring m_Name = L"ImGui Panel";
};
END