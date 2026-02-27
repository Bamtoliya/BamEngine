#pragma once

#include "Base.h"
#include "ViewportPanel.h"

BEGIN(Editor)
class ToolBar
{
public:
	void Draw();
private:
	void DrawFileMenu();
	void DrawEditMenu();
	void DrawSceneMenu();
	void DrawWindowMenu();
	void DrawHelpMenu();

#pragma region Window Menu
private:
	void DrawDisplaySettingsWindow();
private:
	void AddNewViewportPanel();
	void DrawNewViewportPopup();
#pragma endregion

#pragma region Variables
private:
	bool m_DisplaySettingsWindow = false;
private:
	bool m_ShowNewViewportPopup = false;
	tagViewportPanelDesc m_newViewportDesc;
	char m_NewViewportNameBuf[256] = "Viewport";
#pragma endregion

};
END