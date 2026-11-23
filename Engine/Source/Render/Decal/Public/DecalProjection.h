#pragma once

#include "Types.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

// 상자 부피 안의 G-Buffer 표면에 텍스처를 투영하는 데칼의 계산.
// decalVS.hlsl·decalPS.hlsl이 같은 식을 쓰므로 한쪽을 바꾸면 다른 쪽도 함께 바꾼다.
namespace Engine
{
	class Texture;

	// 한 뷰에서 그리는 데칼 수의 상한. 넘는 데칼은 정렬 순서가 뒤인 것부터 빠진다.
	inline constexpr uint32 MaxDecalsPerView = 512u;

	// 단위 상자(-0.5~0.5)의 꼭짓점 번호: 비트 0은 x, 1은 y, 2는 z가 +0.5인지
	inline vec3 GetDecalBoxCorner(uint32 corner)
	{
		return vec3((corner & 1u) ? 0.5f : -0.5f, (corner & 2u) ? 0.5f : -0.5f, (corner & 4u) ? 0.5f : -0.5f);
	}

	// 상자 여섯 면의 삼각형. 바깥에서 볼 때 dot(cross(b - a, c - a), 바깥 법선) > 0인 엔진 기본 메시와 같은 감김이다.
	// 셰이더는 앞면(바깥면)을 버리고 안쪽 면만 그려 카메라가 상자 안에 있어도 화면을 덮는다.
	inline constexpr std::array<uint32, 36> DecalBoxIndices = {
		1, 3, 7, 1, 7, 5,	// +X
		0, 6, 2, 0, 4, 6,	// -X
		2, 6, 7, 2, 7, 3,	// +Y
		0, 1, 5, 0, 5, 4,	// -Y
		4, 5, 7, 4, 7, 6,	// +Z
		0, 2, 3, 0, 3, 1,	// -Z
	};

	// 한 데칼을 그리는 데 필요한 값. DecalComponent와 VFX 데칼 트랙이 같은 형식으로 만든다.
	struct DecalDrawData
	{
		// 단위 상자를 월드로 옮기는 행렬. 상자의 로컬 z가 투영 방향이고 xy가 텍스처 좌표다.
		mat4 BoxToWorld{ 1.0f };
		vec4 BaseColor{ 1.0f };
		Texture* BaseColorTexture = nullptr;
		Texture* NormalTexture = nullptr;
		Texture* PBRTexture = nullptr;
		f32 Opacity = 1.0f;
		f32 NormalOpacity = 1.0f;
		f32 PBROpacity = 0.0f;
		f32 Roughness = 0.5f;
		f32 Metallic = 0.0f;
		f32 MaxAngle = 70.0f;
		f32 DepthFade = 0.1f;
		int32 SortOrder = 0;
	};

	inline mat4 MakeDecalBoxMatrix(const mat4& worldMatrix, const vec3& size)
	{
		return worldMatrix * glm::scale(mat4(1.0f), size);
	}

	// 부피가 없거나 값이 깨진 상자는 그리지 않는다.
	inline bool TryGetDecalWorldToBox(const mat4& boxToWorld, mat4& worldToBox)
	{
		const f32 determinant = glm::determinant(boxToWorld);
		if (!std::isfinite(determinant) || std::abs(determinant) < 1e-9f)
			return false;
		worldToBox = glm::inverse(boxToWorld);
		return true;
	}

	// 음수 크기로 뒤집힌 상자는 감김이 반대가 되므로 반대쪽 면을 버린다.
	inline bool IsDecalBoxMirrored(const mat4& boxToWorld)
	{
		return glm::determinant(mat3(boxToWorld)) < 0.0f;
	}

	inline bool IsInsideDecalBox(const vec3& local)
	{
		return std::abs(local.x) <= 0.5f && std::abs(local.y) <= 0.5f && std::abs(local.z) <= 0.5f;
	}

	// 텍스처 좌표. 상자의 +y가 텍스처의 위쪽이다.
	inline vec2 GetDecalUV(const vec3& local)
	{
		return vec2(local.x + 0.5f, 0.5f - local.y);
	}

	// 투영 방향과 표면 법선이 이루는 각도에 따른 페이드 구간 (cos(maxAngle), cos(maxAngle - 띠 폭))
	inline vec2 GetDecalAngleFadeRange(f32 maxAngleDegrees)
	{
		const f32 maxAngle = std::clamp(std::isfinite(maxAngleDegrees) ? maxAngleDegrees : 90.0f, 0.0f, 90.0f);
		const f32 band = std::min(15.0f, maxAngle);
		// float의 cos(90°)는 0보다 조금 작아 수직 표면이 아주 옅게 남으므로 0에서 자른다.
		return vec2(std::max(0.0f, std::cos(glm::radians(maxAngle))), std::cos(glm::radians(maxAngle - band)));
	}

	// cosAngle은 투영 축과 표면 법선의 내적이다. 상자의 양쪽 면을 함께 받으므로 부호는 보지 않는다.
	inline f32 GetDecalAngleFade(f32 cosAngle, const vec2& fadeRange)
	{
		const f32 x = std::abs(cosAngle);
		const f32 width = fadeRange.y - fadeRange.x;
		if (width <= 1e-5f)
			return x >= fadeRange.x ? 1.0f : 0.0f;
		const f32 t = std::clamp((x - fadeRange.x) / width, 0.0f, 1.0f);
		return t * t * (3.0f - 2.0f * t);
	}

	// 투영 방향의 양 끝에서 depthFade(반 깊이에 대한 비율, 0~1)만큼 서서히 사라진다.
	inline f32 GetDecalDepthFade(f32 localZ, f32 depthFade)
	{
		const f32 distanceToEnd = 0.5f - std::abs(localZ);
		const f32 width = std::clamp(depthFade, 0.0f, 1.0f) * 0.5f;
		if (width <= 1e-5f)
			return distanceToEnd >= 0.0f ? 1.0f : 0.0f;
		return std::clamp(distanceToEnd / width, 0.0f, 1.0f);
	}

	// VFX 데칼 트랙의 재생 시각별 불투명도. fadeStartTime 이후 duration까지 선형으로 사라진다.
	inline f32 GetDecalTrackAlpha(f32 trackLocalTime, f32 duration, f32 fadeStartTime)
	{
		if (trackLocalTime <= fadeStartTime)
			return 1.0f;
		const f32 fadeSpan = std::max(duration - fadeStartTime, 0.001f);
		return std::clamp(1.0f - (trackLocalTime - fadeStartTime) / fadeSpan, 0.0f, 1.0f);
	}

	// 정렬 순서가 작은 데칼부터 그린다. 나중에 그린 데칼이 위에 덮인다. 같은 값은 모은 순서를 지킨다.
	inline void SortDecals(std::vector<DecalDrawData>& decals)
	{
		std::stable_sort(decals.begin(), decals.end(), [](const DecalDrawData& a, const DecalDrawData& b) { return a.SortOrder < b.SortOrder; });
		if (decals.size() > MaxDecalsPerView)
			decals.resize(MaxDecalsPerView);
	}
}
