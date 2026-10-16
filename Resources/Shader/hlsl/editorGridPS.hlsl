struct PS_INPUT
{
    float2 TexCoord : TEXCOORD0;
};

Texture2D texSampler : register(t0, space1);
SamplerState textureSampler : register(s0, space1);

float4 main(PS_INPUT input) : SV_TARGET
{
    return texSampler.Sample(textureSampler, input.TexCoord);
}