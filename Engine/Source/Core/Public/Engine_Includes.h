#pragma once

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
#pragma endregion

using namespace std;

#pragma region ThirdParty Library
// 2. Third Party Library (변경 빈도가 가장 낮음)
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

#pragma endregion


#pragma region BamEngineHeader
#include "Engine_API.h"
#include "Types.h"
#include "Result.h"
#include "Macro.h"
#include "Functions.h"
#include "Delegate.h"
#include "EnumBit.h"
#pragma endregion
using namespace Engine;


#if defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

//#ifndef DBG_NEW 
//#define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ ) 
//#define new DBG_NEW 
//#endif

#endif