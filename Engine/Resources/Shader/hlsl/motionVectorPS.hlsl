#include "motionVector.hlsli"

Texture2D gPosition : register(t4, space3);

// 화면에 보이는 면과 위치가 같은 픽셀에만 이전 화면까지의 UV 이동량을 쓴다.
// 깊이 대신 G-Buffer 위치를 비교하므로 알파로 뚫린 구멍과 정점을 옮기는 재질은 자연히 빠진다.
float2 main(MotionVertex input) : SV_TARGET0
{
	const float3 surface = gPosition.Load(int3(int2(input.Position.xy), 0)).xyz;
	const float tolerance = 0.002f + 0.0005f * input.CurrentClip.w;
	if (distance(surface, input.WorldPosition) > tolerance || input.CurrentClip.w <= 0.0f || input.PreviousClip.w <= 0.0f)
		discard;

	const float2 current = input.CurrentClip.xy / input.CurrentClip.w;
	const float2 previous = input.PreviousClip.xy / input.PreviousClip.w;
	return (previous - current) * float2(0.5f, -0.5f);
}
