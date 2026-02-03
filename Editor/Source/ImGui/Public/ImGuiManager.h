#pragma once
#include "Base.h"
#include <SDL3/SDL.h>

#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "ToolBar.h"

BEGIN(Editor)

struct tagImGuiManagerDesc
{
	SDL_Window* Window = { nullptr };
	class RHI* RHI = { nullptr };
};

class ImGuiManager final : public Base
{
	DECLARE_SINGLETON(ImGuiManager)
	using DESC = tagImGuiManagerDesc;
#pragma region Constructor&Destructor
private:
	ImGuiManager() {}
	virtual ~ImGuiManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free()override;
#pragma endregion

public:
	void Begin();
	void End();
	void ProcessEvent(const SDL_Event* event);
	void Draw();
#pragma region DrawUI
private:
	void MainDockspace();
#pragma endregion

#pragma region Style
private:
	void SetCustomStyle();
#pragma endregion

#pragma region Variables
private:
	SDL_Window* m_Window = { nullptr };
	class RHI* m_RHI = { nullptr };
#pragma region ImGui
private:
	ToolBar m_ToolBar;
	HierarchyPanel m_HierarchyPanel;
	InspectorPanel m_InspectorPanel;
#pragma endregion
#pragma endregion



};

END