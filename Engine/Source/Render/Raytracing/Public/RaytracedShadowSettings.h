#pragma once

#include <cstdint>
#include <initializer_list>

namespace Engine
{
	// 설정 파일에는 int32로 저장한다(0: 상위 따름, 1: 켜기, 2: 끄기).
	enum class ERaytracedShadowMode : std::int32_t
	{
		Inherit = 0,
		On = 1,
		Off = 2,
	};

	// 알 수 없는 저장값은 상위 설정을 따른다.
	constexpr ERaytracedShadowMode ToRaytracedShadowMode(std::int32_t value)
	{
		switch (value)
		{
		case 1: return ERaytracedShadowMode::On;
		case 2: return ERaytracedShadowMode::Off;
		default: return ERaytracedShadowMode::Inherit;
		}
	}

	constexpr bool IsValidRaytracedShadowMode(std::int32_t value)
	{
		return value >= 0 && value <= 2;
	}

	// 넓은 범위에서 좁은 범위 순서. 뒤의 설정이 앞의 결과를 덮어쓴다.
	struct RaytracedShadowOverrides
	{
		ERaytracedShadowMode Project = ERaytracedShadowMode::Inherit;  // 프로젝트 설정 (에디터·Client 공통)
		ERaytracedShadowMode Graphics = ERaytracedShadowMode::Inherit; // 에디터 개인 그래픽 설정
		ERaytracedShadowMode Viewport = ERaytracedShadowMode::Inherit; // 개별 뷰포트
	};

	constexpr bool ResolveRaytracedShadow(bool lightEnabled, const RaytracedShadowOverrides& overrides)
	{
		bool enabled = lightEnabled;
		for (const ERaytracedShadowMode mode : { overrides.Project, overrides.Graphics, overrides.Viewport })
		{
			if (mode != ERaytracedShadowMode::Inherit)
				enabled = mode == ERaytracedShadowMode::On;
		}
		return enabled;
	}
}
