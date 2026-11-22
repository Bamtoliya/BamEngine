#pragma once

#include "Types.h"

#include <cstddef>

namespace Engine
{
	// TLAS 인스턴스의 광선 마스크
	constexpr uint8 RAYTRACING_MASK_SHADOW = 1u << 0u;             // 섀도맵 대신 그림자를 맡는 불투명 캐스터
	constexpr uint8 RAYTRACING_MASK_REFLECTION = 1u << 1u;         // 반사에 보이는 메시
	constexpr uint8 RAYTRACING_MASK_REFLECTION_SHADOW = 1u << 2u;  // 반사된 면에 그림자를 드리우는 메시

	// 반사가 닿은 면의 정점과 재질을 찾는 표의 한 줄. TLAS의 InstanceID가 줄 번호다.
	// raytracing.hlsli의 RaytracedInstance와 일치해야 한다. 번호는 RHI::GetBindlessIndex이며 없으면 0xFFFFFFFF.
	struct RaytracingInstanceData
	{
		uint32 PositionBuffer = 0xFFFFFFFFu;
		uint32 MaterialBuffer = 0xFFFFFFFFu;
		uint32 IndexBuffer = 0xFFFFFFFFu;
		uint32 IndexStride = 0;                 // 2, 4. 인덱스가 없으면 0
		uint32 PositionStride = 0;
		uint32 MaterialStride = 0;
		uint32 MaterialFlags = 0;
		uint32 BaseColorTexture = 0xFFFFFFFFu;
		uint32 PbrTexture = 0xFFFFFFFFu;
		uint32 EmissiveTexture = 0xFFFFFFFFu;
		f32 Roughness = 1.0f;
		f32 Metallic = 0.0f;
		vec4 BaseColor{ 1.0f };
		vec3 EmissiveColor{ 0.0f };
		f32 AmbientOcclusion = 1.0f;
	};
	static_assert(sizeof(RaytracingInstanceData) == 80);
	static_assert(offsetof(RaytracingInstanceData, BaseColor) == 48);
}
