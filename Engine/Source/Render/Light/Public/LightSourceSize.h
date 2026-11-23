#pragma once

#include "LightTypes.h"
#include "Types.h"

#include <algorithm>
#include <cmath>

namespace Engine
{
	// 광원 크기의 편집 범위. 방향광은 겉보기 지름(도, 실제 태양 약 0.53), 점·스포트는 반지름(m)이다.
	constexpr f32 MaxLightSourceAngle = 10.0f;
	constexpr f32 MaxLightSourceRadius = 5.0f;

	// GPULight::SourceSize에 넣는 값. 방향광은 겉보기 반각의 탄젠트, 점·스포트는 반지름(m), 하늘은 0이다.
	// 0이면 그림자 경계가 선명하다. 잘못된 값은 0으로, 범위를 넘는 값은 최대값으로 바꾼다.
	[[nodiscard]] inline f32 GetLightSourceSize(ELightType type, f32 sourceAngleDegrees, f32 sourceRadius)
	{
		const auto sanitize = [](f32 value, f32 maxValue) { return std::isfinite(value) ? std::clamp(value, 0.0f, maxValue) : 0.0f; };
		switch (type)
		{
		case ELightType::Directional: return std::tan(glm::radians(sanitize(sourceAngleDegrees, MaxLightSourceAngle) * 0.5f));
		case ELightType::Point:
		case ELightType::Spot: return sanitize(sourceRadius, MaxLightSourceRadius);
		default: return 0.0f;
		}
	}

	// 받는 면에서 본 반그림자의 반지름(m). blockerDistance는 받는 면에서 가리는 물체까지, lightDistance는 받는 면에서
	// 광원까지의 거리다(방향광은 쓰지 않는다). raytracedShadowMaskRTCS.hlsl과 lightingPS.hlsl의 PCSS가 같은 식을 쓴다.
	[[nodiscard]] inline f32 GetPenumbraRadius(bool directional, f32 sourceSize, f32 blockerDistance, f32 lightDistance)
	{
		if (!(sourceSize > 0.0f) || !(blockerDistance > 0.0f))
			return 0.0f;
		if (directional)
			return blockerDistance * sourceSize;
		// 광원 원판과 가리는 물체가 만드는 닮은 삼각형
		return sourceSize * blockerDistance / std::max(lightDistance - blockerDistance, 1e-3f);
	}
}
