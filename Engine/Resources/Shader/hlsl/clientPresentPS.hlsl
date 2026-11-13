Texture2D SceneColor : register(t0, space3);
SamplerState SceneSampler : register(s0, space3);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return SceneColor.Sample(SceneSampler, uv);
}
