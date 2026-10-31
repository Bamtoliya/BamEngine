struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// 머티리얼 파라미터 버퍼 (b0, space3)
struct ColorAdjustmentsParams
{
    float Saturation; // 채도: 0.0 (흑백) ~ 1.0 (원본) ~ 2.0 (과포화)
    float Contrast;   // 대비: 0.5 (저대비) ~ 1.0 (원본) ~ 2.0 (고대비)
    float Brightness; // 밝기 오프셋: -1.0 ~ 1.0 (기본 0.0)
    float _pad0;      // 16바이트 정렬
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<ColorAdjustmentsParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 color = inputTexture.Sample(inputSampler, input.TexCoord);

    // 1. 밝기 (Brightness)
    color.rgb += gParams.Brightness;

    // 2. 대비 (Contrast: 0.5 중간값을 기준으로 확장/수축)
    color.rgb = (color.rgb - 0.5f) * gParams.Contrast + 0.5f;

    // 3. 채도 (Saturation: Rec.709 휘도 가중치 기준 보간)
    float luminance = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    color.rgb = lerp(float3(luminance, luminance, luminance), color.rgb, gParams.Saturation);

    return saturate(color);
}