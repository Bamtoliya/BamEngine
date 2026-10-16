struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct ToneMappingParams
{
	float Exposure;
	float Gamma;
	float _pad0;
	float _pad1;
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<ToneMappingParams> gParams : register(b0, space3);

float3 ACESFilmic(float3 color)
{
	const float a = 2.51f;
	const float b = 0.03f;
	const float c = 2.43f;
	const float d = 0.59f;
	const float e = 0.14f;

	return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

float4 main(PS_INPUT input) : SV_TARGET
{
	float3 color = inputTexture.Sample(inputSampler, input.TexCoord).rgb;
	color *= gParams.Exposure;
	color = ACESFilmic(color);
	color = pow(color, 1.0f / gParams.Gamma);

	return float4(color, 1.0f);
}
