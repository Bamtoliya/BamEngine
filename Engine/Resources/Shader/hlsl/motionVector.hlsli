// 움직인 물체의 모션 벡터 패스가 함께 쓰는 값
#ifndef BAM_MOTION_VECTOR_HLSLI
#define BAM_MOTION_VECTOR_HLSLI

// Renderer::RenderMotionVectors의 MotionConstants (b1, space3)
struct MotionConstants
{
	float4x4 JitteredViewProjection; // G-Buffer를 그린 행렬
	float4x4 ViewProjection;         // 지터를 뺀 이번 프레임
	float4x4 PreviousViewProjection; // 지터를 뺀 이전 프레임
};
ConstantBuffer<MotionConstants> gMotion : register(b1, space3);

struct MotionVertex
{
	float4 Position : SV_POSITION;
	float3 WorldPosition : TEXCOORD0;
	float4 CurrentClip : TEXCOORD1;
	float4 PreviousClip : TEXCOORD2;
};

MotionVertex MakeMotionVertex(float4 worldPosition, float4 previousWorldPosition)
{
	MotionVertex output;
	output.Position = mul(gMotion.JitteredViewProjection, worldPosition);
	output.WorldPosition = worldPosition.xyz;
	output.CurrentClip = mul(gMotion.ViewProjection, worldPosition);
	output.PreviousClip = mul(gMotion.PreviousViewProjection, previousWorldPosition);
	return output;
}

#endif
