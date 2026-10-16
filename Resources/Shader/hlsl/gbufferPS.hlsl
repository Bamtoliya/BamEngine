struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float3 WorldPos : WORLD_POSITION;
	float3 Normal : NORMAL;
	float2 TexCoord : TEXCOORD0;
	float3 Tangent : TANGENT;
	float4 Color : COLOR0;
	nointerpolation float PickingId : TEXCOORD5;
};

struct PS_OUTPUT
{
	float4 Diffuse : SV_TARGET0;
	float4 Normal : SV_TARGET1;
	float4 PBR : SV_TARGET2;
	float4 Emission : SV_TARGET3;
	float4 Position : SV_TARGET4;
};

Texture2D diffuseMap : register(t0, space1);
SamplerState diffuseSampler : register(s0, space1);

PS_OUTPUT main(PS_INPUT input)
{
	PS_OUTPUT output;
	float4 diffuse = diffuseMap.Sample(diffuseSampler, input.TexCoord) * input.Color;

	output.Diffuse = diffuse;
	output.Normal = float4(input.Normal * 0.5f + 0.5f, 0.0f);
	output.PBR = float4(0.5f, 0.0f, 0.5f, 1.0f);
	output.Emission = float4(0.0f, 0.0f, 0.0f, 0.0f);
	output.Position = float4(input.WorldPos, input.PickingId);

	return output;
}
