struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct BloomPrefilterParams
{
    float Threshold; // 빛 번짐 임계값 (기본 1.0, 범위 0.0 ~ 5.0)
    float SoftKnee;  // 부드러운 전이 폭 (기본 0.5, 범위 0.0 ~ 1.0)
    float _pad0;     // 16바이트 패딩
    float _pad1;
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<BloomPrefilterParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = inputTexture.Sample(inputSampler, input.TexCoord);

    // 휘도(Luminance) 계산 (Rec.709)
    float brightness = max(color.r, max(color.g, color.b));

    // Soft Knee 커브 적용 (임계값 근처에서 부드럽게 감쇠)
    float knee = gParams.Threshold * gParams.SoftKnee;
    float soft = brightness - gParams.Threshold + knee;
    soft = clamp(soft, 0.0f, 2.0f * knee);
    soft = (soft * soft) / (4.0f * knee + 1e-5f);

    float contribution = max(soft, brightness - gParams.Threshold);
    contribution /= max(brightness, 1e-5f);

    return float4(color.rgb * max(0.0f, contribution), color.a);
}