#include "motionVector.hlsli"

// 움직인 스킨드 메시의 모션 벡터. 뼈 행렬은 이번 프레임 256개 뒤에 이전 프레임 256개가 이어진다.
static const uint BONE_COUNT = 256u;
StructuredBuffer<float4x4> gBones : register(t0, space2);

struct MotionObject
{
	float4x4 World;
	float4x4 PreviousWorld;
};
ConstantBuffer<MotionObject> gObject : register(b0, space2);

struct VS_INPUT
{
	float3 Position : POSITION;
	uint4 BoneIDs : BONEIDS;
	float4 Weights : WEIGHTS;
};

// gbufferSkinningVS와 같은 식이어야 화면의 면과 위치가 일치한다.
float4 Skin(float4 position, uint4 boneIds, float4 weights, uint offset)
{
	if (weights.x + weights.y + weights.z + weights.w <= 0.0001f)
		return position;
	const uint4 bones = min(boneIds, uint4(255u, 255u, 255u, 255u)) + offset;
	const float4x4 skinMatrix =
		weights.x * gBones[bones.x] +
		weights.y * gBones[bones.y] +
		weights.z * gBones[bones.z] +
		weights.w * gBones[bones.w];
	return mul(skinMatrix, position);
}

MotionVertex main(VS_INPUT input)
{
	const float4 position = float4(input.Position, 1.0f);
	const float4 current = mul(gObject.World, Skin(position, input.BoneIDs, input.Weights, 0u));
	const float4 previous = mul(gObject.PreviousWorld, Skin(position, input.BoneIDs, input.Weights, BONE_COUNT));
	return MakeMotionVertex(current, previous);
}
