#pragma once

#include "Editor_Includes.h"
#include <filesystem>

BEGIN(Editor)

struct StartupProjectHubResult
{
	bool Cancelled = false;
	bool UseDefaultProject = false;
	std::filesystem::path SelectedProjectFileOrRoot;
};

class StartupProjectHub
{
public:
	// 에디터 로딩 스플래쉬 전에 경량 SDL3 + ImGui(SDL_Renderer) 창으로 프로젝트 허브를 실행한다.
	static StartupProjectHubResult Run();
};

END
