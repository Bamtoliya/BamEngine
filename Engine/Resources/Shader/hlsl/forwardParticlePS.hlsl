// Match the interpolants emitted by gbufferSpriteVS.
struct PS_INPUT
{
    float4 Position                 : SV_POSITION;
    float3 WorldPos                 : WORLD_POSITION;
    float3 Normal                   : NORMAL;
    float2 TexCoord                 : TEXCOORD0;
    float3 Tangent                  : TANGENT;
    float4 Color                    : COLOR0;
    nointerpolation float PickingId : TEXCOORD5;
};

Texture2D particleTexture : register(t0, space1);
SamplerState particleSampler : register(s0, space1);

float4 main(PS_INPUT input) : SV_TARGET0
{
    return particleTexture.Sample(particleSampler, input.TexCoord) * input.Color;
}
