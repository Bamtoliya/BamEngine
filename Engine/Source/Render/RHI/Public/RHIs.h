#pragma once

#if !defined(__EMSCRIPTEN__)
#include "SDLGPURHI.h"
#include "VulkanRHI.h"
#include "MetalRHI.h"
#if defined(_WIN32)
#include "DirectX12RHI.h"
#endif
#endif