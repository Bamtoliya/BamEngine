#pragma once

#include "Base.h"
#include "EnumBit.h"
#include "Reflection/ReflectionMacro.h"

BEGIN(Engine)

#define MAX_RENDER_TARGET_COUNT 8
#define MAX_TEXTURE_SLOTS 128
#define MAX_STORAGE_BUFFERS 8
#define MAX_BUFFER_SLOTS 4
#define MAX_SWAPCHAIN_BUFFERS 3

ENUM()
enum class ERenderTargetBindFlag : uint8
{
	RTBF_None = 0,
	RTBF_ShaderResource = 1 << 0,
	RTBF_RenderTarget = 1 << 1,
	RTBF_DepthStencil = 1 << 2,
};

ENABLE_BITMASK_OPERATORS(ERenderTargetBindFlag)

ENUM()
enum class ERenderTargetClearFlag : uint8
{
	RTCF_None = 0,
	RTCF_Color = 1 << 0,
	RTCF_Depth = 1 << 1,
	RTCF_Stencil = 1 << 2,
};

ENUM()
enum class ERenderTargetType : uint8
{
	Color,
	GBuffer,
	DepthStencil,
};

END