struct ClientPresentParams
{
    float4 FadeColor;
    float FadeAlpha;
    float _pad0;
    float _pad1;
    float _pad2;
};

Texture2D SceneColor : register(t0, space3);
SamplerState SceneSampler : register(s0, space3);
ConstantBuffer<ClientPresentParams> gParams : register(b0, space3);

float4 main(float4 position : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float4 color = SceneColor.Sample(SceneSampler, uv);
    if (gParams.FadeAlpha > 0.001f)
    {
        color.rgb = lerp(color.rgb, gParams.FadeColor.rgb, saturate(gParams.FadeAlpha));
    }
    return color;
}
