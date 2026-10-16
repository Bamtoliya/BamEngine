#ifndef BAM_COMMON_HLSL
#define BAM_COMMON_HLSL

struct CommonData
{
	float4x4 View;
	float4x4 InvView;
	float4x4 Projection;
	float4x4 InvProjection;
	float4x4 ViewProjection;
	float4x4 InvViewProjection;
	float3 CameraPos;
	float Time;
};

ConstantBuffer<CommonData> commonData : register(b0, space0);

#endif