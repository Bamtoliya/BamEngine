#include "shading_flags.hlsli"

// gbufferPS와 같은 알파 식과 기준값으로 픽셀을 버려, 보이는 구멍과 그림자 구멍을 일치시킨다.
struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
	float4 Color : COLOR0;
};

struct MaterialConstants
{
	uint   materialFlags;
	float  roughness;
	float  metallic;
	float  cavity;
	float4 baseColor;
	float3 emissiveColor;
	float  ao;
};

ConstantBuffer<MaterialConstants> gMaterial : register(b0, space3);
Texture2D diffuseMap : register(t0, space1);
SamplerState materialSampler : register(s0, space1);

void main(PS_INPUT input)
{
	float alpha = gMaterial.baseColor.a * input.Color.a;
	[branch]
	if (gMaterial.materialFlags & FLAG_HAS_DIFFUSE_MAP)
	{
		alpha *= diffuseMap.Sample(materialSampler, input.TexCoord).a;
	}
	if (alpha < GetAlphaDiscardThreshold(gMaterial.materialFlags))
		discard;
}
