#pragma once

#include "Base.h"
#include "Types.h"

// PackedFlags 비트 레이아웃
// [0:5]   ELightFlags (CastShadow=0, Forward=1, Deferred=2, Volumetric=3, AffectDiffuse=4, AffectSpecular=5)
// [24:25] ELightType  (Point=0, Directional=1, Spot=2)
// [26:27] EAttenuationMode (Coefficients=0, InverseSquare=1, Disabled=2)
struct tagGPULight
{
	vec3	Position;
	f32		Intensity;

	vec3	Direction;
	f32		Range;

	vec3	Color;
	uint32	PackedFlags;

	vec3	AttenuationCoeff;
	f32		Pad0;

	f32		SpotInnerCos;
	f32		SpotOuterCos;
	f32		SpotFalloff;
	f32		Pad1;
};

struct tagLightBufferHeader
{
	uint32	NumLights;
	uint32	Padding[3];
};

inline uint32 PackLightFlags(uint32 flags, uint32 type, uint32 attenuationMode)
{
	return (flags & 0x3Fu)
		| ((type & 0x3u) << 24u)
		| ((attenuationMode & 0x3u) << 26u);
}