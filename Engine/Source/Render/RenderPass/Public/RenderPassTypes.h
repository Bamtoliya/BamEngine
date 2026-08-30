#pragma once

#include "Base.h"
#include "RHIBlendTypes.h"

BEGIN(Engine)
using RenderPassID = uint32;
constexpr RenderPassID INVALID_PASS_ID = 0xFFFFFFFF;

ENUM()
enum class ERenderSortType
{
	None = 0,
	FrontToBack = 1,
	BackToFront = 2,
};

ENUM()
enum class ERenderPassLoadOperation : uint8
{
	RPLO_Load,
	RPLO_Clear,
	RPLO_Discard,
};

ENUM()
enum class ERenderPassStoreOperation : uint8
{
	RPSO_Store,
	RPSO_Discard,
	RPSO_Resolve,
	RPSO_ResolveAndStore,
};

ENUM()
enum class ERenderPassType : uint8
{
	Custom = 0,
	Geometry,
	Lighting,
	Shadow,
	Forward,
	ForwardTransparent,
	PostProcess,
	UI,
};

STRUCT()
struct RenderPassDesc
{
	uint32 priority = { 0 };
	RenderPassID ID = INVALID_PASS_ID;
	wstring name = { L"" };
	wstring depthStencilName = { L"" };
	std::vector<wstring> renderTargetNames;
	ERenderSortType sortType = ERenderSortType::None;
	ERenderPassLoadOperation loadOperation = ERenderPassLoadOperation::RPLO_Load;
	ERenderPassStoreOperation storeOperation = ERenderPassStoreOperation::RPSO_Store;
	ERenderPassLoadOperation stencilLoadOperation = ERenderPassLoadOperation::RPLO_Load;
	ERenderPassStoreOperation stencilStoreOperation = ERenderPassStoreOperation::RPSO_Store;
	vec4 overrideClearColor = vec4(0.0f, 0.0f, 0.0f, -1.0f);
	ERenderPassType passType = ERenderPassType::Custom;
	EBlendMode acceptedBlendModes = EBlendMode::ALL;
};
END