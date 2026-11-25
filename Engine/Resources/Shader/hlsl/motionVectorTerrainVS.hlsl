#include "motionVector.hlsli"

// 움직인 지형의 모션 벡터. 높이맵으로 정점을 옮기는 terrainVS와 같은 식이어야 화면의 면과 위치가 일치한다.
// 높이맵 자체가 바뀌는 프레임은 이전 높이를 알 수 없으므로 호출하지 않는다.
struct TerrainMotionObject
{
	float4x4 World;
	float4x4 PreviousWorld;
	float4 HeightfieldData; // x 최대 높이, yz 칸 간격
};
ConstantBuffer<TerrainMotionObject> gObject : register(b0, space2);
Texture2D<float> gHeightMap : register(t8, space3); // terrainVS와 같은 슬롯

struct VS_INPUT
{
	float3 Position : POSITION;
};

MotionVertex main(VS_INPUT input, uint vertexId : SV_VertexID)
{
	uint width, height;
	gHeightMap.GetDimensions(width, height);

	const int2 lastIndex = int2(width, height) - 1;
	const float2 cellSpacing = max(gObject.HeightfieldData.yz, float2(1e-6f, 1e-6f));
	const float2 gridCoord = input.Position.xz / cellSpacing + 0.5f * float2(lastIndex);
	const int2 index = clamp(int2(round(gridCoord)), int2(0, 0), lastIndex);
	float3 displacedPosition = input.Position;
	displacedPosition.y = gHeightMap.Load(int3(index, 0)) * gObject.HeightfieldData.x + input.Position.y;

	const float4 position = float4(displacedPosition, 1.0f);
	return MakeMotionVertex(mul(gObject.World, position), mul(gObject.PreviousWorld, position));
}
