#pragma once

#include "Base.h"
#include "BaseViewportPanel.h"
#include "MainMenuBarLayout.h"

BEGIN(Editor)
class ToolBar
{
public:
	void Draw(SDL_Window* window);
	void ApplyPendingLanguageChange();
	bool IsMainWindowDragArea(const SDL_Point& point) const;
	float GetPlayBarHeight() const;
	float GetFooterBarHeight() const;

#pragma region File Menu
private:
	void DrawFileMenu();
public:
	static void ImportAsset();
	static void LoadResourceFile();
#pragma endregion

#pragma region Edit Menu
private:
	void DrawEditMenu();
#pragma endregion

#pragma region Scene Menu
private:
	void DrawSceneMenu();
public:
	static void NewScene();
	static void SaveScene(bool saveAs = false);
	static void LoadScene();
#pragma endregion

#pragma region Window Menu
private:
	void DrawWindowMenu();
	void AddNewViewportPanel();
	void DrawNewViewportPopup();
#pragma endregion

#pragma region Help Menu
private:
	void DrawHelpMenu();
#pragma endregion

#pragma region Play & Footer Controls
private:
	void DrawPlayControls();
	void DrawFooterControls();
#pragma endregion

#pragma region Window Control
private:
	void DrawWindowControls(SDL_Window* window);
#pragma endregion

#pragma region Settings
public:
#pragma endregion

#pragma region Variables
private:
	MainMenuBarLayout m_MainMenuBarLayout;
	std::optional<ELocalizationLanguage> m_PendingLanguage;
	bool m_ShowNewViewportPopup = false;
	ViewportPanelDesc m_newViewportDesc;
	char m_NewViewportNameBuf[256] = "Viewport";
#pragma endregion

};
END
