#pragma once

#include "Editor_Includes.h"
#include "EnumBit.h"

namespace
{
	static constexpr uint32 MAX_PP_PASSES = 16;

	struct tagViewportPassInfo
	{
		wstring Name;
		bool	Enabled;
	};

	struct tagPPEffect
	{
		wstring Name;
		bool Enabled = false;
		RHIPipeline* Pipeline = nullptr;
		vector<uint8> Parameters; // 이펙트별로 필요한 파라미터들을 바이트 배열 형태로 저장 (예: 노출, 감마 등)
	};

	struct tagToneMappingParams
	{
		f32 exposure = 1.0f;
		f32 gamma = 2.2f;
		f32 _pad0 = 0.f;
		f32 _pad1 = 0.f;
	};

	ENUM()
	enum class EViewportChannelView : uint8
	{
		None = 0,
		R = 1 << 0,
		G = 1 << 1,
		B = 1 << 2,
		A = 1 << 3,
		RGB = R | G | B,
		RGBA = RGB | A
	};
	ENABLE_BITMASK_OPERATORS(EViewportChannelView)

	struct ChannelViewData
	{
		uint32 Flags;
		float pad[3];
	};
}