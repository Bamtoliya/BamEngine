#pragma once

#ifdef WIN32
#define _HAS_STD_BYTE 0

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shobjidl.h>
#include <shellapi.h> 
#define _HAS_STD_BYTE 1
#endif

#pragma warning(push)
#pragma warning(disable: 4251) 

#pragma region C++ Headers
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <functional>
#include <map>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <filesystem>
#include <typeindex>
#include <fstream>
#include <sstream>
#include <any>
#include <array>
#include <thread>
#include <atomic>
#pragma endregion

#pragma region ThirdParty Library
// 2. Third Party Library (변경 빈도가 가장 낮음)
#include "imgui.h"

// fmt
#include <fmt/core.h>
#include <fmt/color.h>

// SDL3
#include <SDL3/SDL.h>

// GLM
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/quaternion.hpp>
using namespace glm;

// Vulkan
#include <vulkan/vulkan.h>

// Glaze
#include <glaze/glaze.hpp>

#include <stb_image.h>
#pragma endregion


#pragma region BamEngineHeader
#include "Engine_API.h"
#include "Types.h"
#include "Result.h"
#include "Macro.h"
#include "Functions.h"
#include "Delegate.h"
#include "EnumBit.h"
#include "RenderTypes.h"
#include "Base.h"
#pragma endregion
using namespace Engine;


#if defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifndef DBG_NEW 
#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ ) 
#define new DBG_NEW 
#endif

#endif