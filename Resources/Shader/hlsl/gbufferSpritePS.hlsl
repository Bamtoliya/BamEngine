struct PS_INPUT
{
    float3 WorldPos : TEXCOORD0;
    float3 Normal : TEXCOORD1;
    float2 TexCoord : TEXCOORD2;
    float3 Tangent : TEXCOORD3;
    float4 Color : TEXCOORD4;
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

Texture2D texSampler : register(t0, space1);
SamplerState textureSampler : register(s0, space1);

PS_OUTPUT main(PS_INPUT input)
{
    float4 texColor = texSampler.Sample(textureSampler, input.TexCoord) * input.Color;

    clip(texColor.a - 0.01f);
    if (texColor.a < 0.01f) discard;

    PS_OUTPUT output;
    output.Diffuse = texColor;
    output.Normal = float4(normalize(input.Normal) * 0.5f + 0.5f, 0.0f);
    output.PBR = float4(1.0f, 0.0f, 0.0f, 1.0f);
    output.Emission = float4(0.0f, 0.0f, 0.0f, 0.0f);
    output.Position = float4(input.WorldPos, input.PickingId);
    return output;
}
