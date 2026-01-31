#pragma once

#include "Base.h"

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
};
END