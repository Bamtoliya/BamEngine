struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord    : TEXCOORD0;
};

struct ViewportChannelData
{
    uint Flags;
    float3 Padding;
};

ConstantBuffer<ViewportChannelData> g_ViewportChannelData : register(b0, space3);
Texture2D g_Texture : register(t0, space3);
SamplerState g_Sampler : register(s0, space3);

static const uint CHANNEL_R = 1u << 0u;
static const uint CHANNEL_G = 1u << 1u;
static const uint CHANNEL_B = 1u << 2u;
static const uint CHANNEL_A = 1u << 3u;
static const uint CHANNEL_RGBA = CHANNEL_R | CHANNEL_G | CHANNEL_B | CHANNEL_A;

bool HasChannel(uint flags, uint bit)
{
    return (flags & bit) != 0u;
}

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 sampledColor = g_Texture.Sample(g_Sampler, input.TexCoord);
    uint flags = g_ViewportChannelData.Flags;

    // RGBA: 원본 색과 알파 유지
    if (flags == CHANNEL_RGBA)
    {
        return sampledColor;
    }

    // A 단독: 알파 값을 회색조로 표시
    if (flags == CHANNEL_A)
    {
        return float4(sampledColor.aaa, 1.0f);
    }

    // RGB 채널 검사: 원본 알파로 RGB를 가리지 않음
    float3 rgb = float3(
        HasChannel(flags, CHANNEL_R) ? sampledColor.r : 0.0f,
        HasChannel(flags, CHANNEL_G) ? sampledColor.g : 0.0f,
        HasChannel(flags, CHANNEL_B) ? sampledColor.b : 0.0f);

    return float4(rgb, 1.0f);
}