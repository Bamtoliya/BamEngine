// 레이트레이싱 반사의 추적·누적·필터 패스가 함께 쓰는 값
#ifndef BAM_REFLECTION_HLSLI
#define BAM_REFLECTION_HLSLI

static const float REFLECTION_NOT_TRACED = -1.0f;
static const float REFLECTION_MISS_DISTANCE = 10000.0f;

// DeferredSceneRenderer.cpp의 ReflectionDenoiseParams (b1, space3). 텍스처는 gBindlessTextures의 번호다.
struct ReflectionDenoiseParams
{
	float4x4 PrevViewProjection;
	float3 PrevCameraPosition;
	uint HistoryState;          // 0 이력 없음, 1 카메라가 움직임, 2 카메라가 멈춤
	uint InputTexture;          // 누적: 이번 프레임의 추적 결과, 필터: 누적 결과
	uint HitDistanceTexture;
	uint HistoryTexture;        // 누적: 이전 프레임의 누적 결과
	uint GeometryTexture;       // 누적: 이전 프레임의 표면, 필터: 이번 프레임의 표면. r 카메라 거리, gb 법선, a 누적 횟수
	float MirrorRoughness;
	float RoughnessFadeEnd;
	float MaxHistory;
	uint FrameIndex;
};

static const uint REFLECTION_HISTORY_NONE = 0u;
static const uint REFLECTION_HISTORY_STATIC = 2u;

// 드물게 나오는 아주 밝은 표본이 평균을 흔들지 않도록 누적과 필터는 밝기를 0~1로 눌러서 계산한다.
float3 CompressRadiance(float3 color)
{
	return color / (1.0f + max(color.r, max(color.g, color.b)));
}

float3 ExpandRadiance(float3 color)
{
	return color / max(1.0f - max(color.r, max(color.g, color.b)), 0.001f);
}

// 픽셀마다 다른 0~1 값 두 개 (pcg2d)
float2 HashPixel(uint2 pixel)
{
	uint2 v = pixel * 1664525u + 1013904223u;
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v ^= v >> 16u;
	v.x += v.y * 1664525u;
	v.y += v.x * 1664525u;
	v ^= v >> 16u;
	return float2(v) * (1.0f / 4294967296.0f);
}

float2 EncodeNormal(float3 normal)
{
	normal /= abs(normal.x) + abs(normal.y) + abs(normal.z);
	if (normal.z >= 0.0f)
		return normal.xy;
	return float2((1.0f - abs(normal.y)) * (normal.x >= 0.0f ? 1.0f : -1.0f), (1.0f - abs(normal.x)) * (normal.y >= 0.0f ? 1.0f : -1.0f));
}

float3 DecodeNormal(float2 encoded)
{
	float3 normal = float3(encoded, 1.0f - abs(encoded.x) - abs(encoded.y));
	const float fold = saturate(-normal.z);
	normal.x += normal.x >= 0.0f ? -fold : fold;
	normal.y += normal.y >= 0.0f ? -fold : fold;
	return normalize(normal);
}

#endif
