#pragma once
#include "EnumBit.h"

BEGIN(Engine)
#define MAX_RENDER_TARGET_COUNT 8
#define MAX_TEXTURE_SLOTS 128

using RenderPassID = uint32;
constexpr RenderPassID INVALID_PASS_ID = 0xFFFFFFFF;

enum class ERenderSortType
{
	None = 0,
	FrontToBack = 1,
	BackToFront = 2,
};

enum class ERenderTargetFormat : uint8
{
	RTF_RGBA8,
	RTF_RGBA16F,
	RTF_RGBA32F,
	RTF_DEPTH24STENCIL8,
	RTF_UINT32,
};

enum class ERenderTargetUsage : uint8
{
	RTU_Color,
	RTU_DepthStencil,
};

enum class ERenderTargetBindFlag : uint8
{
	RTBF_None = 0,
	RTBF_ShaderResource = 1 << 0,
	RTBF_RenderTarget = 1 << 1,
	RTBF_DepthStencil = 1 << 2,
};

ENABLE_BITMASK_OPERATORS(ERenderTargetBindFlag)

enum class ERenderTargetClearFlag : uint8
{
	RTCF_None = 0,
	RTCF_Color = 1 << 0,
	RTCF_Depth = 1 << 1,
	RTCF_Stencil = 1 << 2,
};

enum class ERenderTargetType : uint8
{
	Color,
	GBuffer,
	DepthStencil,
};

enum class ERenderTargetTextureType : uint8
{
	RTTT_2D,
	RTTT_2DArray,
	RTTT_3D,
	RTTT_Cube,
	RTTT_CubeArray,
};

enum class ERenderPassLoadOperation : uint8
{
	RPLO_Load,
	RPLO_Clear,
	RPLO_Discard,
};

enum class ERenderPassStoreOperation : uint8
{
	RPSO_Store,
	RPSO_Discard,
	RPSO_Resolve,
	RPSO_ResolveAndStore,
};

struct tagRenderPassDesc
{
	uint32 Priority = { 0 };
	RenderPassID ID = INVALID_PASS_ID;
	wstring Name = { L"" };
	wstring DepthStencilName = { L"" };
	vector<wstring> RenderTargetNames;
	ERenderSortType SortType = ERenderSortType::None;
	ERenderPassLoadOperation LoadOperation = ERenderPassLoadOperation::RPLO_Load;
	ERenderPassStoreOperation StoreOperation = ERenderPassStoreOperation::RPSO_Store;
	ERenderPassLoadOperation StencilLoadOperation = ERenderPassLoadOperation::RPLO_Load;
	ERenderPassStoreOperation StencilStoreOperation = ERenderPassStoreOperation::RPSO_Store;
	vec4 OverrideClearColor = vec4(0.0f, 0.0f, 0.0f, -1.0f);
};
END