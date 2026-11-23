// 데칼 상자를 정점 버퍼 없이 SV_VertexID로 만든다(Draw(36)).
#include "common.hlsli"
#include "decal.hlsli"

// DecalProjection.h의 DecalBoxIndices와 같다. 바깥에서 볼 때 엔진 기본 메시와 같은 감김이다.
static const uint BoxIndices[36] = {
	1, 3, 7, 1, 7, 5,
	0, 6, 2, 0, 4, 6,
	2, 6, 7, 2, 7, 3,
	0, 1, 5, 0, 5, 4,
	4, 5, 7, 4, 7, 6,
	0, 2, 3, 0, 3, 1,
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
};

VS_OUTPUT main(uint vertexID : SV_VertexID)
{
	const uint corner = BoxIndices[vertexID % 36u];
	const float3 local = float3((corner & 1u) ? 0.5f : -0.5f, (corner & 2u) ? 0.5f : -0.5f, (corner & 4u) ? 0.5f : -0.5f);

	VS_OUTPUT output;
	const float4 worldPosition = mul(gDecal.BoxToWorld, float4(local, 1.0f));
	output.Position = mul(commonData.ViewProjection, worldPosition);
	return output;
}
