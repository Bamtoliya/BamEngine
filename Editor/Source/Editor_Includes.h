#pragma once

#include "Runtime.h"

namespace Editor
{
	static uint32 g_WindowWidth = 1920;
	static uint32 g_WindowHeight = 1080;
}

using namespace Editor;

#pragma region ImGui
#undef DBG_NEW
#undef new

#include "imgui.h"
#include "imgui_internal.h"

#ifndef DBG_NEW 
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ ) 
#define new DBG_NEW 
#endif
#pragma endregion

#include "IconsFontAwesome7.h"