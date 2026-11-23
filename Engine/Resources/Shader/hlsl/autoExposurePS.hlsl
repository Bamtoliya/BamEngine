struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

Texture2D sceneTexture : register(t0, space3);            // 톤매핑 전 HDR 장면 색
Texture2D<float> exposureTexture : register(t1, space3);  // autoExposureAdaptCS가 쓴 노출(EV), 1x1
SamplerState defaultSampler : register(s0, space3);

float4 main(PS_INPUT input) : SV_TARGET
{
    float4 scene = sceneTexture.Sample(defaultSampler, input.TexCoord);
    scene.rgb *= exp2(exposureTexture.Load(int3(0, 0, 0)));
    return scene;
}
