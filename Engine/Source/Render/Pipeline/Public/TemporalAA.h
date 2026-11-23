#pragma once

#include "CameraBuffer.h"
#include "Types.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Engine
{
	// 0과 1 사이에 고르게 퍼지는 수열. index는 1부터 쓴다.
	inline f32 HaltonSequence(uint32 index, uint32 base)
	{
		f32 result = 0.0f;
		f32 fraction = 1.0f;
		while (index > 0u)
		{
			fraction /= static_cast<f32>(base);
			result += fraction * static_cast<f32>(index % base);
			index /= base;
		}
		return result;
	}

	inline constexpr uint32 TemporalJitterCount = 8u;

	// 프레임마다 화면을 옮기는 픽셀 안쪽 거리(-0.5~0.5, y는 아래 방향)
	inline vec2 TemporalJitterPixels(uint32 frameIndex)
	{
		const uint32 index = frameIndex % TemporalJitterCount + 1u;
		return vec2(HaltonSequence(index, 2u) - 0.5f, HaltonSequence(index, 3u) - 0.5f);
	}

	// 직교 투영(2D·픽셀 퍼펙트)은 흔들면 픽셀이 흐려지므로 TAA를 쓰지 않는다.
	inline bool SupportsTemporalAA(const CameraBuffer& camera)
	{
		return camera.projMatrix[3][3] == 0.0f;
	}

	// 지터를 NDC로 옮긴 거리(y는 위 방향). 지터 카메라로 그린 화면의 NDC p에는 원래 카메라의 p - 이 값이 보인다.
	inline vec2 TemporalJitterNDC(const vec2& jitterPixels, uint32 width, uint32 height)
	{
		return vec2(2.0f * jitterPixels.x / static_cast<f32>(glm::max(width, 1u)), -2.0f * jitterPixels.y / static_cast<f32>(glm::max(height, 1u)));
	}

	// 투영 결과만 jitterPixels만큼 옮긴 카메라. 원근 방향(LH·RH)과 무관하게 클립 공간에서 옮긴다.
	inline CameraBuffer MakeJitteredCamera(const CameraBuffer& camera, const vec2& jitterPixels, uint32 width, uint32 height)
	{
		const vec3 offset(TemporalJitterNDC(jitterPixels, width, height), 0.0f);
		CameraBuffer jittered = camera;
		jittered.projMatrix = glm::translate(mat4(1.0f), offset) * camera.projMatrix;
		jittered.invProjMatrix = glm::inverse(jittered.projMatrix);
		jittered.viewProjMatrix = jittered.projMatrix * camera.viewMatrix;
		jittered.invViewProjMatrix = glm::inverse(jittered.viewProjMatrix);
		return jittered;
	}
}
