#pragma once
#include "Base.h"

using RenderPassID = uint32;
constexpr RenderPassID INVALID_PASS_ID = 0xFFFFFFFF;


enum class ERenderSortType
{
	None = 0,
	FrontToBack = 1,
	BackToFront = 2,
};

typedef struct RenderPassInfo
{
	RenderPassID ID = INVALID_PASS_ID;
	wstring Name = { L"" };
	uint32 Priority = { 0 };
	ERenderSortType SortType = ERenderSortType::None;
} RENDERPASSDESC;