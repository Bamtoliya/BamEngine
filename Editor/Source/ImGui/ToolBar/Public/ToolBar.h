#pragma once

#include "Base.h"
#include "ViewportPanel.h"

BEGIN(Editor)
class ToolBar
{
public:
	void Draw();
private:
#pragma region File Menu
private:
	void DrawFileMenu();
private:
	void ImportAsset();
#pragma endregion

#pragma region Edit Menu
private:
	void DrawEditMenu();
#pragma endregion

#pragma region Scene Menu
private:
	void DrawSceneMenu();
private:
	void NewScene();
	void SaveScene();
	void LoadScene();
#pragma endregion

#pragma region Window Menu
private:
	void DrawWindowMenu();
private:
	void DrawDisplaySettingsWindow();
private:
	void AddNewViewportPanel();
	void DrawNewViewportPopup();
#pragma endregion

#pragma region Help Menu
private:
	void DrawHelpMenu();
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