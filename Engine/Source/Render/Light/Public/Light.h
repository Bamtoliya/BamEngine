#pragma once

#include "Base.h"
#include "Types.h"

// PackedFlags 비트 레이아웃
// [0:5]   ELightFlags (CastShadow=0, Forward=1, Deferred=2, Volumetric=3, AffectDiffuse=4, AffectSpecular=5)
// [24:25] ELightType  (Point=0, Directional=1, Spot=2)
// [26:27] EAttenuationMode (Coefficients=0, InverseSquare=1, Disabled=2)
struct GPULight
{
	Engine::vec3 Position;
	Engine::f32 Intensity;

	Engine::vec3 Direction;
	Engine::f32 Range;

	Engine::vec3 Color;
	Engine::uint32 PackedFlags;

	Engine::vec3 AttenuationCoeff;
	Engine::f32 Pad0;

	Engine::f32 SpotInnerCos;
	Engine::f32 SpotOuterCos;
	Engine::f32 SpotFalloff;
	Engine::f32 Pad1;
};

struct LightBufferHeader
{
	Engine::uint32 NumLights;
	Engine::uint32 Padding[3];
};

inline Engine::uint32 PackLightFlags(Engine::uint32 flags, Engine::uint32 type, Engine::uint32 attenuationMode)
{
	return (flags & 0x3Fu) | ((type & 0x3u) << 24u) | ((attenuationMode & 0x3u) << 26u);
}

struct LightShadowData
{
	Engine::mat4 LightViewProjMatrix;
	Engine::vec4 ShadowParams; // Bias, SlopeBias, NormalBias, ShadowMapIndex
};

// lightingPS.hlsl의 데이터 크기와 필드 위치를 확인합니다.
static_assert(sizeof(GPULight) == 80);
static_assert(offsetof(GPULight, PackedFlags) == 44);
static_assert(offsetof(GPULight, SpotInnerCos) == 64);
static_assert(sizeof(LightBufferHeader) == 16);
static_assert(sizeof(LightShadowData) == 80);