struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct BloomCompositeParams
{
    float Intensity; // 블룸 합성 강도 (기본 1.0, 범위 0.0 ~ 5.0)
    float _pad0;     // 16바이트 정렬
    float _pad1;
    float _pad2;
};

Texture2D sceneTexture : register(t0, space3);       // 원본 씬 컬러
Texture2D bloomTexture : register(t1, space3);       // 최종 누적된 블룸 텍스처
SamplerState defaultSampler : register(s0, space3);
ConstantBuffer<BloomCompositeParams> gParams : register(b0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 scene = sceneTexture.Sample(defaultSampler, input.TexCoord);
    float3 bloom = bloomTexture.Sample(defaultSampler, input.TexCoord).rgb;

    // 원본 색상에 블룸 빛 번짐을 가산 합성
    scene.rgb += bloom * gParams.Intensity;

    return scene;
}