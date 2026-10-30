#pragma once

#include "GraphAnnotations.h"
#include "Logger.h"

namespace Game::Gameplay
{
	FUNCTION(GRAPH_CALLABLE("Test.HotReloadGreeting3"), DISPLAY_NAME("Hot Reload Greeting4"), CATEGORY("Test"))
		inline void HotReloadGreeting()
	{
		BAM_LOG(Info, "Test", "HotReload", "Hello from Live Hot-Reloaded C++ Node in GameModule!");
	}

	FUNCTION(GRAPH_CALLABLE("Test.HotReloadGreeting2"), DISPLAY_NAME("Hot Reload Greeting2"), CATEGORY("Test"))
		inline void HotReloadGreeting2()
	{
		BAM_LOG(Info, "Test", "HotReload", "Hello from Live Hot-Reloaded C++ Node in GameModule!");
	}
}