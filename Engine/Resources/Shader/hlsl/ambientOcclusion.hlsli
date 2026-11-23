#ifndef BAM_AMBIENT_OCCLUSION_HLSLI
#define BAM_AMBIENT_OCCLUSION_HLSLI
// 앰비언트 오클루전 공통 정의 (DirectX12 Compute 전용)
// 상수 구조는 DeferredSceneRenderer.cpp의 AmbientOcclusionParams(176바이트)와 일치해야 한다.
#include "shading_flags.hlsli"

struct AmbientOcclusionParams
{
	float4x4 PrevViewProjection; // 이전 프레임의 지터 없는 카메라
	float3 CameraPosition;
	float Radius;                // 차폐를 찾는 반경(m)
	float3 ScreenRight;          // 화면 x(NDC)가 늘어나는 월드 방향
	float PixelsPerMeter;        // 카메라 거리 1m에서 1m가 차지하는 픽셀 수
	float3 ScreenUp;             // 화면 y(NDC, 위)가 늘어나는 월드 방향
	float MaxRadiusPixels;
	float3 PrevCameraPosition;
	float Power;                 // 화면 공간 AO의 대비
	float2 PrevJitterUV;         // 이전 G-Buffer의 TAA 지터(텍스처 좌표)
	uint FrameIndex;
	uint HistoryValid;
	uint SliceCount;
	uint StepCount;
	uint RayCount;
	float MaxHistory;
	float NormalBias;
	float DistanceBiasScale;
	float2 Pad;
};

ConstantBuffer<AmbientOcclusionParams> gAOParams : register(b1, space3);

Texture2D gDiffuse : register(t0, space3);
Texture2D gNormal : register(t1, space3);
Texture2D gPosition : register(t4, space3);

static const float AO_PI = 3.14159265359f;

// 화면 공간 AO를 받을 수 있는 픽셀. 위치가 없거나 Unlit이거나 재질이 FLAG_APPLY_SSAO를 끈 픽셀은 1을 쓴다.
bool ReceivesAmbientOcclusion(int3 pixel, out float3 normal, out float3 position)
{
	normal = float3(0.0f, 1.0f, 0.0f);
	position = float3(0.0f, 0.0f, 0.0f);
	if (gDiffuse.Load(pixel).a < 0.01f)
		return false;
	const float4 normalSample = gNormal.Load(pixel);
	const uint packedData = UnpackShadingData(normalSample.a);
	if ((packedData & SHADING_MODEL_MASK) == SHADING_MODEL_UNLIT || (packedData & FLAG_APPLY_SSAO) == 0u)
		return false;
	normal = normalize(normalSample.rgb * 2.0f - 1.0f);
	position = gPosition.Load(pixel).xyz;
	return true;
}

// G-Buffer에 표면이 있는 픽셀(차폐하는 쪽은 플래그와 무관하다)
bool HasSurface(int3 pixel)
{
	return gDiffuse.Load(pixel).a >= 0.01f;
}

uint HashAO(uint value)
{
	value ^= value >> 16u;
	value *= 0x7feb352du;
	value ^= value >> 15u;
	value *= 0x846ca68bu;
	value ^= value >> 16u;
	return value;
}

float RandomAO(uint2 pixel, uint frameIndex, uint index)
{
	const uint seed = HashAO(pixel.x + HashAO(pixel.y + HashAO(frameIndex * 16u + index)));
	return (float)(seed & 0x00FFFFFFu) / 16777216.0f;
}

#endif
