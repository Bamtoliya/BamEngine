#include "motionVector.hlsli"

// 움직인 정적 메시의 모션 벡터
struct MotionInstance
{
	float4x4 World;
	float4x4 PreviousWorld;
};
StructuredBuffer<MotionInstance> gInstances : register(t0, space2);

struct VS_INPUT
{
	float3 Position : POSITION;
};

MotionVertex main(VS_INPUT input, uint instanceId : SV_InstanceID)
{
	const float4 position = float4(input.Position, 1.0f);
	return MakeMotionVertex(mul(gInstances[instanceId].World, position), mul(gInstances[instanceId].PreviousWorld, position));
}
