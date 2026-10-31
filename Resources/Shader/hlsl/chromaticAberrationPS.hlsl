struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

// 머티리얼 파라미터 상수 버퍼 (b0, space3)
struct ChromaticAberrationParams
{
    float Intensity; // 수차 강도 (기본 0.005, 범위 0.0 ~ 0.05)
    float Falloff;   // 중심부 감쇠 지수 (기본 1.5, 범위 0.5 ~ 3.0)
    float _pad0;     // 16바이트 패딩
    float _pad1;
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<ChromaticAberrationParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    // 화면 중심 (0.5, 0.5)을 기준으로 방향과 거리 계산
    float2 dir = input.TexCoord - 0.5f;
    float dist = length(dir);

    // 외곽으로 갈수록 수차 효과를 지수 곡선으로 증폭
    float2 offset = dir * pow(dist * 2.0f, gParams.Falloff) * gParams.Intensity;

    // R 채널은 외곽 방향(+), B 채널은 중심 방향(-)으로 분리 샘플링
    float r = inputTexture.Sample(inputSampler, input.TexCoord + offset).r;
    float g = inputTexture.Sample(inputSampler, input.TexCoord).g;
    float b = inputTexture.Sample(inputSampler, input.TexCoord - offset).b;
    float a = inputTexture.Sample(inputSampler, input.TexCoord).a;

    return float4(r, g, b, a);
}