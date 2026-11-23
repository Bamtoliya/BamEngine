#pragma once

#include "Types.h"
#include <cstdint>
#include <initializer_list>

namespace Engine
{
	// 앰비언트 오클루전 방식. 설정 파일에는 int32로 저장한다.
	enum class EAmbientOcclusionMode : std::int32_t
	{
		Inherit = 0,     // 상위 설정 따름
		Off = 1,
		ScreenSpace = 2, // G-Buffer로 계산(GTAO)
		Raytraced = 3,   // TLAS로 추적. 쓸 수 없으면 ScreenSpace로 되돌린다.
	};

	// 알 수 없는 저장값은 상위 설정을 따른다.
	constexpr EAmbientOcclusionMode ToAmbientOcclusionMode(std::int32_t value)
	{
		switch (value)
		{
		case 1: return EAmbientOcclusionMode::Off;
		case 2: return EAmbientOcclusionMode::ScreenSpace;
		case 3: return EAmbientOcclusionMode::Raytraced;
		default: return EAmbientOcclusionMode::Inherit;
		}
	}

	constexpr bool IsValidAmbientOcclusionMode(std::int32_t value)
	{
		return value >= 0 && value <= 3;
	}

	// 넓은 범위에서 좁은 범위 순서. 뒤의 설정이 앞의 결과를 덮어쓴다.
	struct AmbientOcclusionOverrides
	{
		EAmbientOcclusionMode Project = EAmbientOcclusionMode::Inherit;  // 프로젝트 설정 (에디터·Client 공통)
		EAmbientOcclusionMode Graphics = EAmbientOcclusionMode::Inherit; // 에디터 개인 그래픽 설정
		EAmbientOcclusionMode Viewport = EAmbientOcclusionMode::Inherit; // 개별 뷰포트
	};

	// 요청된 방식. 아무도 정하지 않으면 꺼짐이다. 반환값은 Inherit가 아니다.
	constexpr EAmbientOcclusionMode ResolveAmbientOcclusion(const AmbientOcclusionOverrides& overrides)
	{
		EAmbientOcclusionMode mode = EAmbientOcclusionMode::Off;
		for (const EAmbientOcclusionMode candidate : { overrides.Project, overrides.Graphics, overrides.Viewport })
		{
			if (candidate != EAmbientOcclusionMode::Inherit)
				mode = candidate;
		}
		return mode;
	}

	// 이번 프레임에 실제로 쓸 방식. 레이트레이싱을 쓸 수 없으면 화면 공간으로, 화면 공간도 쓸 수 없으면 끈다.
	constexpr EAmbientOcclusionMode SelectAmbientOcclusionPath(EAmbientOcclusionMode requested, bool screenSpaceAvailable, bool raytracingAvailable)
	{
		if (!screenSpaceAvailable || requested == EAmbientOcclusionMode::Off || requested == EAmbientOcclusionMode::Inherit)
			return EAmbientOcclusionMode::Off;
		if (requested == EAmbientOcclusionMode::Raytraced && raytracingAvailable)
			return EAmbientOcclusionMode::Raytraced;
		return EAmbientOcclusionMode::ScreenSpace;
	}

	// 화면 x·y(NDC)가 늘어나는 월드 방향. 화면 공간 AO가 화면에서 고른 방향을 월드 방향으로 바꿀 때 쓴다.
	// 뷰 행렬의 행은 카메라 축이고, 투영의 부호로 y 뒤집기 같은 관례 차이를 맞춘다(LH·RH 무관).
	struct ScreenAxes
	{
		vec3 Right{ 1.0f, 0.0f, 0.0f };
		vec3 Up{ 0.0f, 1.0f, 0.0f };
	};

	inline ScreenAxes GetScreenAxes(const mat4& view, const mat4& projection)
	{
		ScreenAxes axes;
		axes.Right = glm::normalize(vec3(view[0][0], view[1][0], view[2][0])) * (projection[0][0] < 0.0f ? -1.0f : 1.0f);
		axes.Up = glm::normalize(vec3(view[0][1], view[1][1], view[2][1])) * (projection[1][1] < 0.0f ? -1.0f : 1.0f);
		return axes;
	}

	// 카메라 거리 1m에 놓인 길이 1m가 화면에서 차지하는 세로 픽셀 수(원근 카메라)
	inline f32 PixelsPerMeterAtUnitDistance(const mat4& projection, uint32 height)
	{
		return 0.5f * glm::abs(projection[1][1]) * static_cast<f32>(height);
	}

	// NDC 이동(y 위)을 텍스처 좌표 이동(y 아래)으로 바꾼다. TAA 지터만큼 어긋난 이전 화면 위치를 찾을 때 쓴다.
	inline vec2 NDCOffsetToUV(const vec2& ndcOffset)
	{
		return vec2(0.5f * ndcOffset.x, -0.5f * ndcOffset.y);
	}

	// 화면 공간 AO는 원근 카메라에서만 쓴다(직교 카메라는 거리로 반경을 정할 수 없다).
	inline bool SupportsScreenSpaceAmbientOcclusion(const mat4& projection)
	{
		return projection[3][3] == 0.0f;
	}
}
