#pragma once

#include "CameraBuffer.h"
#include "ComponentAnnotations.h"
#include "Types.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
	// 흐림 정도(CoC)를 정하는 식. 값은 후처리 파라미터 Mode(정수)로도 저장된다.
	ENUM(DISPLAY_NAME("ENUM_DEPTH_OF_FIELD_MODE"))
	enum class EDepthOfFieldMode
	{
		FocusRange = 0,   // 초점 거리 앞뒤의 선명한 폭과 흐려지는 거리로 정한다.
		PhysicalLens = 1  // 얇은 렌즈 공식. 렌즈 초점 거리는 세로 시야각과 센서 높이로 구한다.
	};

	struct DepthOfFieldSettings
	{
		EDepthOfFieldMode Mode = EDepthOfFieldMode::FocusRange;
		f32 FocusDistance = 10.0f;   // 카메라에서 초점면까지 (m)
		f32 FocusRange = 4.0f;       // FocusRange 식: 초점면을 중심으로 선명하게 남는 폭 (m)
		f32 TransitionRange = 10.0f; // FocusRange 식: 선명한 폭 밖에서 흐림이 최대가 되기까지의 거리 (m)
		f32 FStop = 2.8f;            // PhysicalLens 식: 조리개 f값
		f32 SensorHeight = 24.0f;    // PhysicalLens 식: 센서 세로 길이 (mm)
		f32 MaxBlurRadius = 12.0f;   // 흐림 반지름의 상한 (전체 해상도 픽셀)
	};

	inline constexpr f32 DepthOfFieldMaxBlurRadiusLimit = 32.0f;
	// 앞쪽 흐림을 넓히는 타일 한 변의 길이 (전체 해상도 픽셀)
	inline constexpr uint32 DepthOfFieldTileSize = 8u;

	// 셰이더가 시선 방향 거리 z로 CoC를 구하는 값. CoC는 전체 해상도 픽셀 반지름이며 앞은 음수, 뒤는 양수다.
	struct DepthOfFieldCoCParams
	{
		EDepthOfFieldMode Mode = EDepthOfFieldMode::FocusRange;
		f32 FocusDistance = 10.0f;
		f32 HalfRange = 2.0f;
		f32 InvTransition = 0.1f;
		f32 LensScale = 0.0f;        // PhysicalLens: 무한히 먼 곳의 CoC
		f32 MaxRadius = 12.0f;
	};

	// 원근 투영이면 true. 직교 투영은 시야각이 없어 PhysicalLens 식을 쓸 수 없다.
	inline bool IsPerspectiveProjection(const mat4& projection)
	{
		return projection[3][3] == 0.0f;
	}

	// 카메라가 바라보는 방향(월드). 왼손·오른손 좌표계를 모두 처리한다.
	inline vec3 GetCameraForward(const CameraBuffer& camera)
	{
		const mat4& p = camera.projMatrix;
		// 왼손 좌표계의 투영은 시선 방향 깊이에 양의 계수를 곱한다.
		const f32 handedness = IsPerspectiveProjection(p) ? p[2][3] : p[2][2];
		const vec3 axis(camera.invViewMatrix[2]);
		const f32 length = glm::length(axis);
		if (length <= 0.0f)
			return vec3(0.0f, 0.0f, 1.0f);
		return axis / length * (handedness < 0.0f ? -1.0f : 1.0f);
	}

	// 세로 시야각과 센서 높이로 구한 렌즈 초점 거리 (mm)
	inline f32 ComputeLensFocalLength(const mat4& projection, f32 sensorHeight)
	{
		if (!IsPerspectiveProjection(projection))
			return 0.0f;
		// projection[1][1] = 1 / tan(세로 시야각 / 2)
		return 0.5f * sensorHeight * std::abs(projection[1][1]);
	}

	inline DepthOfFieldCoCParams MakeDepthOfFieldCoCParams(const DepthOfFieldSettings& settings, const mat4& projection, uint32 viewportHeight)
	{
		DepthOfFieldCoCParams params;
		params.Mode = settings.Mode;
		params.FocusDistance = std::max(settings.FocusDistance, 0.01f);
		params.HalfRange = std::max(settings.FocusRange, 0.0f) * 0.5f;
		params.InvTransition = 1.0f / std::max(settings.TransitionRange, 0.001f);
		params.MaxRadius = std::clamp(settings.MaxBlurRadius, 0.0f, DepthOfFieldMaxBlurRadiusLimit);

		const f32 focalLength = ComputeLensFocalLength(projection, settings.SensorHeight);
		if (focalLength > 0.0f && settings.FStop > 0.0f && settings.SensorHeight > 0.0f)
		{
			// 센서 위 CoC 지름(mm) = f² / (N (S - f)) × |1 - S / z|. 반지름 픽셀로 바꾼다.
			const f32 focusMm = std::max(params.FocusDistance * 1000.0f, focalLength * 1.01f);
			const f32 diameterAtInfinity = focalLength * focalLength / (settings.FStop * (focusMm - focalLength));
			params.LensScale = 0.5f * diameterAtInfinity / settings.SensorHeight * static_cast<f32>(viewportHeight);
		}
		return params;
	}

	// 시선 방향 거리 z(m)의 CoC. depthOfField.hlsli의 ComputeCoC와 같은 식이다.
	inline f32 ComputeDepthOfFieldCoC(const DepthOfFieldCoCParams& params, f32 viewDepth)
	{
		const f32 z = std::max(viewDepth, 0.001f);
		f32 coc = 0.0f;
		if (params.Mode == EDepthOfFieldMode::PhysicalLens)
		{
			coc = params.LensScale * (1.0f - params.FocusDistance / z);
		}
		else
		{
			const f32 offset = z - params.FocusDistance;
			const f32 blur = std::clamp((std::abs(offset) - params.HalfRange) * params.InvTransition, 0.0f, 1.0f);
			coc = (offset < 0.0f ? -1.0f : 1.0f) * params.MaxRadius * blur;
		}
		return std::clamp(coc, -params.MaxRadius, params.MaxRadius);
	}
}
