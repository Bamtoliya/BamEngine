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

ConstantBuffer<ObjectData> ubo : register(b0, space2);

VS_OUTPUT main(VS_INPUT input)
{
	VS_OUTPUT output;
	float4 worldPosition = mul(ubo.WorldMatrix, float4(input.Position, 1.0f));

    output.Position = mul(commonData.ViewProjection, worldPosition);
	output.WorldPos = worldPosition.xyz;
	output.Normal = normalize(mul( ubo.WorldMatrix, float4(input.Normal, 0.0f)).xyz);
	output.TexCoord = input.TexCoord;
	output.Tangent = normalize(mul(ubo.WorldMatrix, float4(input.Tangent, 0.0f)).xyz);
	output.Color = input.Color;
	output.PickingId = ubo.PickingData.x;

	return output;
}
