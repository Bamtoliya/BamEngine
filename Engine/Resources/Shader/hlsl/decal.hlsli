#ifndef BAM_DECAL_HLSLI
#define BAM_DECAL_HLSLI

// DeferredSceneRenderer.cpp의 DecalParams (b1, space3). 계산식은 DecalProjection.h와 같다.
struct DecalParams
{
	float4x4 BoxToWorld;     // 단위 상자(-0.5~0.5) → 월드
	float4x4 WorldToBox;
	float4 BaseColor;
	float3 AxisX;            // 상자 로컬 축의 월드 방향(정규화). z가 투영 방향이다.
	float Opacity;
	float3 AxisY;
	float NormalOpacity;
	float3 AxisZ;
	float PBROpacity;
	float2 AngleFadeRange;   // (cos(최대 각도), cos(최대 각도 - 띠 폭))
	float DepthFade;
	float Roughness;
	float Metallic;
	uint TextureFlags;
	float2 Padding;
	float4 UVScaleOffset;    // xy: UV 스케일, zw: UV 오프셋(스크롤 포함)
};
ConstantBuffer<DecalParams> gDecal : register(b1, space3);

static const uint DECAL_HAS_BASE_COLOR_TEXTURE = 1u << 0u;
static const uint DECAL_HAS_NORMAL_TEXTURE     = 1u << 1u;
static const uint DECAL_HAS_PBR_TEXTURE        = 1u << 2u;

#endif
