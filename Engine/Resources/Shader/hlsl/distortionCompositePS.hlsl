// 디스토션 V2 합성 패스: R16G16_FLOAT 오프셋 버퍼를 읽어 HDR 씬 컬러를 screenUV + offset으로 왜곡한다.
// 오프셋이 0이면 무왜곡이며, 화면 밖 UV는 원본 screenUV로 폴백해 가장자리 늘어짐을 방지한다.

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

Texture2D gSceneColor : register(t0, space3);
Texture2D gDistortionOffset : register(t1, space3);
SamplerState gSampler : register(s0, space3);

float4 main(PS_INPUT input) : SV_TARGET0
{
	const float2 screenUV = input.TexCoord;
	const float2 offset = gDistortionOffset.SampleLevel(gSampler, screenUV, 0.0f).rg;

	if (dot(offset, offset) <= 1e-10f)
		return gSceneColor.SampleLevel(gSampler, screenUV, 0.0f);

	const float2 distortedUV = screenUV + offset;
	const bool outOfBounds = any(distortedUV < 0.0f) || any(distortedUV > 1.0f);
	const float2 sampleUV = outOfBounds ? screenUV : distortedUV;
	return gSceneColor.SampleLevel(gSampler, sampleUV, 0.0f);
}
