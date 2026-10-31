struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct BloomUpsampleParams
{
    float Scatter; // 블러 확산 반경 배수 (기본 1.0, 범위 0.1 ~ 2.0)
    float _pad0;
    float _pad1;
    float _pad2;
};

Texture2D inputTexture : register(t0, space3);
SamplerState inputSampler : register(s0, space3);
ConstantBuffer<BloomUpsampleParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    uint width, height;
    inputTexture.GetDimensions(width, height);
    float2 texelSize = 1.0f / float2(width, height);
    float x = texelSize.x * gParams.Scatter;
    float y = texelSize.y * gParams.Scatter;

    // 3x3 텐트 필터 샘플링
    float3 a = inputTexture.Sample(inputSampler, input.TexCoord + float2(-x,  y)).rgb;
    float3 b = inputTexture.Sample(inputSampler, input.TexCoord + float2( 0,  y)).rgb;
    float3 c = inputTexture.Sample(inputSampler, input.TexCoord + float2( x,  y)).rgb;

    float3 d = inputTexture.Sample(inputSampler, input.TexCoord + float2(-x,  0)).rgb;
    float3 e = inputTexture.Sample(inputSampler, input.TexCoord).rgb;
    float3 f = inputTexture.Sample(inputSampler, input.TexCoord + float2( x,  0)).rgb;

    float3 g = inputTexture.Sample(inputSampler, input.TexCoord + float2(-x, -y)).rgb;
    float3 h = inputTexture.Sample(inputSampler, input.TexCoord + float2( 0, -y)).rgb;
    float3 i = inputTexture.Sample(inputSampler, input.TexCoord + float2( x, -y)).rgb;

    // 텐트 필터 가중치 (중심 4/16, 변 2/16, 모서리 1/16)
    float3 upsample = e * (4.0f / 16.0f);
    upsample += (b + d + f + h) * (2.0f / 16.0f);
    upsample += (a + c + g + i) * (1.0f / 16.0f);

    return float4(upsample, 1.0f);
}