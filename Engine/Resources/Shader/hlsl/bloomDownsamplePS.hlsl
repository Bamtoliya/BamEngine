struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    uint width, height;
    inputTexture.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    float x = texelSize.x;
    float y = texelSize.y;

    // 13-tap 샘플링 (중심 1개, 안쪽 4개, 외곽 4개 사각 링)
    float3 a = inputTexture.Sample(inputSampler, input.TexCoord + float2(-2.0f * x,  2.0f * y)).rgb;
    float3 b = inputTexture.Sample(inputSampler, input.TexCoord + float2( 0.0f,      2.0f * y)).rgb;
    float3 c = inputTexture.Sample(inputSampler, input.TexCoord + float2( 2.0f * x,  2.0f * y)).rgb;

    float3 d = inputTexture.Sample(inputSampler, input.TexCoord + float2(-2.0f * x,  0.0f)).rgb;
    float3 e = inputTexture.Sample(inputSampler, input.TexCoord).rgb;
    float3 f = inputTexture.Sample(inputSampler, input.TexCoord + float2( 2.0f * x,  0.0f)).rgb;

    float3 g = inputTexture.Sample(inputSampler, input.TexCoord + float2(-2.0f * x, -2.0f * y)).rgb;
    float3 h = inputTexture.Sample(inputSampler, input.TexCoord + float2( 0.0f,     -2.0f * y)).rgb;
    float3 i = inputTexture.Sample(inputSampler, input.TexCoord + float2( 2.0f * x, -2.0f * y)).rgb;

    float3 j = inputTexture.Sample(inputSampler, input.TexCoord + float2(-x,  y)).rgb;
    float3 k = inputTexture.Sample(inputSampler, input.TexCoord + float2( x,  y)).rgb;
    float3 l = inputTexture.Sample(inputSampler, input.TexCoord + float2(-x, -y)).rgb;
    float3 m = inputTexture.Sample(inputSampler, input.TexCoord + float2( x, -y)).rgb;

    // Jimenez 가중치 합산 (박스 4개 0.5 + 내부 4개 0.5)
    float3 downsample = e * 0.125f;
    downsample += (a + c + g + i) * 0.03125f;
    downsample += (b + d + f + h) * 0.0625f;
    downsample += (j + k + l + m) * 0.125f;

    return float4(downsample, 1.0f);
}