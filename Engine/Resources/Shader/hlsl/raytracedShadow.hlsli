// 레이트레이싱 그림자의 추적·누적·필터 패스가 함께 쓰는 값
#ifndef BAM_RAYTRACED_SHADOW_HLSLI
#define BAM_RAYTRACED_SHADOW_HLSLI

#include "shading_flags.hlsli"

static const uint MAX_RAYTRACED_SHADOW_LIGHTS = 4u;
static const uint INVALID_LIGHT = 0xFFFFFFFFu;

// DeferredSceneRenderer.cpp의 RaytracedShadowDenoiseParams (b1, space3). 텍스처는 gBindlessTextures의 번호다.
struct RaytracedShadowDenoiseParams
{
	float4x4 PrevViewProjection;
	float3 PrevCameraPosition;
	uint HistoryState;              // 0 이력 없음, 1 카메라가 움직임, 2 카메라가 멈춤
	uint InputTexture;              // 누적: 이번 프레임의 추적 결과, 필터: 누적 결과
	uint PenumbraTexture;           // 누적: 이번 프레임의 반그림자 반지름(m), 필터: 누적한 반지름
	uint HistoryTexture;            // 누적: 이전 프레임의 누적 결과
	uint PenumbraHistoryTexture;    // 누적: 이전 프레임의 반그림자 반지름
	uint GeometryTexture;           // 누적: 이전 프레임의 표면, 필터: 이번 프레임의 표면. r 카메라 거리, gb 법선, a 누적 횟수
	float MaxHistory;               // 카메라가 멈췄을 때 이어 쓰는 최대 프레임 수
	float PixelSizeConstant;        // 픽셀 하나의 월드 크기(m) = PixelSizeConstant + 카메라 거리 * PixelSizePerMeter
	float PixelSizePerMeter;
	uint4 Lights;                   // 채널별 광원 버퍼 인덱스
};

static const uint SHADOW_HISTORY_NONE = 0u;
static const uint SHADOW_HISTORY_STATIC = 2u;

// 그림자를 받는 픽셀인지. 하늘, Unlit, 그림자를 받지 않는 면은 추적하지 않는다.
bool IsShadowReceiver(float4 diffuse, float4 normalSample)
{
	const uint packedData = UnpackShadingData(normalSample.a);
	return diffuse.a >= 0.01f && (packedData & FLAG_RECEIVE_SHADOWS) != 0u && (packedData & SHADING_MODEL_MASK) != SHADING_MODEL_UNLIT;
}

// LightSourceSize.h의 GetPenumbraRadius와 같은 식이다.
float GetPenumbraRadius(bool directional, float sourceSize, float blockerDistance, float lightDistance)
{
	if (sourceSize <= 0.0f || blockerDistance <= 0.0f)
		return 0.0f;
	if (directional)
		return blockerDistance * sourceSize;
	return sourceSize * blockerDistance / max(lightDistance - blockerDistance, 1e-3f);
}

#endif
