#include "common.hlsli"

struct VS_INPUT
{
	float3 Position : POSITION;
	float3 Normal : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float3 Tangent : TANGENT;
	float3 Binormal : BINORMAL;
	float4 Color : COLOR;
};

struct VS_OUTPUT
{
	float4 Position : SV_POSITION;
	float3 WorldPos : WORLD_POSITION;
	float3 Normal : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float3 Tangent : TANGENT;
	float4 Color : COLOR0;
	nointerpolation float PickingId : TEXCOORD5;
};

struct ObjectData
{
	float4x4 WorldMatrix;
	float4 PickingData;
};

// 공간 2의 슬롯 b0: 오브젝트 데이터 (월드 행렬 등)
ConstantBuffer<ObjectData> ubo : register(b0, space2);

// 공간 1의 슬롯 t1: 높이맵 텍스처 (R32_FLOAT)
Texture2D heightMap : register(t1, space1);
SamplerState heightSampler : register(s1, space1);

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;

	// 1. 높이맵에서 현재 정점의 높이 샘플링 (정점 셰이더에서는 SampleLevel 사용)
	float height = heightMap.SampleLevel(heightSampler, input.TexCoord, 0).r;
	const float maxHeight = 30.0f; // 터레인 기본 최대 높이

	// 2. 높이 변위(Displacement) 적용
	float3 displacedPos = input.Position;
	displacedPos.y += height * maxHeight;

	float4 worldPosition = mul(ubo.WorldMatrix, float4(displacedPos, 1.0f));
	output.Position = mul(commonData.ViewProjection, worldPosition);
	output.WorldPos = worldPosition.xyz;

	// 3. 인접 텍셀(좌/우/상/하) 차분(Central Difference)으로 실시간 노멀 계산
	const float texelDelta = 1.0f / 128.0f; // 해상도 기반 텍셀 간격
	float hL = heightMap.SampleLevel(heightSampler, input.TexCoord + float2(-texelDelta, 0.0f), 0).r * maxHeight;
	float hR = heightMap.SampleLevel(heightSampler, input.TexCoord + float2(texelDelta, 0.0f), 0).r * maxHeight;
	float hD = heightMap.SampleLevel(heightSampler, input.TexCoord + float2(0.0f, -texelDelta), 0).r * maxHeight;
	float hU = heightMap.SampleLevel(heightSampler, input.TexCoord + float2(0.0f, texelDelta), 0).r * maxHeight;

	// X축/Z축 기울기를 통한 법선 벡터 도출
	float3 localNormal = normalize(float3(hL - hR, 2.0f, hD - hU));
	output.Normal = normalize(mul(ubo.WorldMatrix, float4(localNormal, 0.0f)).xyz);

	output.TexCoord = input.TexCoord;
	output.Tangent = normalize(mul(ubo.WorldMatrix, float4(1.0f, 0.0f, 0.0f, 0.0f)).xyz);
	output.Color = input.Color;
	output.PickingId = ubo.PickingData.x;

	return output;
}