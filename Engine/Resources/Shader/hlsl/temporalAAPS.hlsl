// 시간 누적 안티앨리어싱. 프레임마다 조금씩 다른 위치에서 그린 화면을 이전 결과와 섞는다.
// 이전 결과는 이번 프레임 주변 픽셀의 범위로 잘라 잔상을 막는다.

// DeferredSceneRenderer.cpp의 TemporalAAParams (b1, space3)
struct TemporalAAParams
{
	float4x4 ViewProjection;         // 지터를 뺀 행렬
	float4x4 PreviousViewProjection;
	float4x4 InvViewProjection;
	float3 CameraPosition;
	uint HasHistory;
	float2 JitterPixels;             // 이번 프레임이 옮겨진 거리(y는 아래 방향)
	float StillBlend;                // 멈춘 픽셀에 섞는 이번 프레임의 비율
	float MovingBlend;
};
ConstantBuffer<TemporalAAParams> gParams : register(b1, space3);

Texture2D gDiffuse : register(t0, space3);
Texture2D gColor : register(t1, space3);     // 이번 프레임의 HDR 장면
Texture2D gHistory : register(t2, space3);   // 이전 결과(밝기를 누른 값)
Texture2D gMotion : register(t3, space3);    // 움직인 물체의 UV 이동량
Texture2D gPosition : register(t4, space3);
SamplerState gSampler : register(s0, space3);

// 모션 벡터 타깃의 지우기 값. 움직인 물체가 없는 픽셀이다.
static const float NO_MOTION = 100.0f;

struct PS_INPUT
{
	float4 Position : SV_POSITION;
	float2 TexCoord : TEXCOORD0;
};

struct PS_OUTPUT
{
	float4 Color : SV_TARGET0;
	float4 History : SV_TARGET1;
};

// 아주 밝은 픽셀 하나가 결과를 흔들지 않도록 0~1로 눌러서 섞는다. NaN은 0이 된다.
float3 Compress(float3 color)
{
	color = clamp(color, 0.0f, 65000.0f);
	return color / (1.0f + max(color.r, max(color.g, color.b)));
}

float3 Expand(float3 color)
{
	return color / max(1.0f - max(color.r, max(color.g, color.b)), 0.00002f);
}

float3 ToYCoCg(float3 c)
{
	return float3(dot(c, float3(0.25f, 0.5f, 0.25f)), dot(c, float3(0.5f, 0.0f, -0.5f)), dot(c, float3(-0.25f, 0.5f, -0.25f)));
}

float3 FromYCoCg(float3 c)
{
	return float3(c.x + c.y - c.z, c.x + c.z, c.x - c.y - c.z);
}

float2 ClipToUV(float4 clip)
{
	return clip.xy / clip.w * float2(0.5f, -0.5f) + 0.5f;
}

// 이전 화면의 같은 점까지의 UV 이동량. 찾을 수 없으면 false.
bool FindMotion(int2 pixel, float2 uv, bool hasSurface, out float2 motion)
{
	motion = gMotion.Load(int3(pixel, 0)).xy;
	if (hasSurface && abs(motion.x) < NO_MOTION * 0.5f)
		return true;

	float3 position;
	if (hasSurface)
	{
		position = gPosition.Load(int3(pixel, 0)).xyz;
	}
	else
	{
		// 하늘은 카메라를 따라 움직이므로 방향만 쓴다. 두 프레임의 카메라 위치 차이가 보이지 않을 만큼 멀리 둔다.
		const float4 world = mul(gParams.InvViewProjection, float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 1.0f, 1.0f));
		position = gParams.CameraPosition + normalize(world.xyz / world.w - gParams.CameraPosition) * 1000000.0f;
	}
	const float4 current = mul(gParams.ViewProjection, float4(position, 1.0f));
	const float4 previous = mul(gParams.PreviousViewProjection, float4(position, 1.0f));
	motion = float2(0.0f, 0.0f);
	if (current.w <= 0.0f || previous.w <= 0.0f)
		return false;
	motion = ClipToUV(previous) - ClipToUV(current);
	return true;
}

// 선형 표본 5개로 근사한 Catmull-Rom. 선형 보간만 되풀이하면 화면이 계속 흐려진다.
float3 SampleHistory(float2 uv, float2 size)
{
	const float2 position = uv * size;
	const float2 center = floor(position - 0.5f) + 0.5f;
	const float2 f = position - center;
	const float2 w0 = f * (-0.5f + f * (1.0f - 0.5f * f));
	const float2 w1 = 1.0f + f * f * (-2.5f + 1.5f * f);
	const float2 w2 = f * (0.5f + f * (2.0f - 1.5f * f));
	const float2 w3 = f * f * (-0.5f + 0.5f * f);
	const float2 w12 = w1 + w2;
	const float2 uv0 = (center - 1.0f) / size;
	const float2 uv3 = (center + 2.0f) / size;
	const float2 uv12 = (center + w2 / w12) / size;

	float4 sum = float4(gHistory.SampleLevel(gSampler, float2(uv12.x, uv0.y), 0.0f).rgb, 1.0f) * w12.x * w0.y;
	sum += float4(gHistory.SampleLevel(gSampler, float2(uv0.x, uv12.y), 0.0f).rgb, 1.0f) * w0.x * w12.y;
	sum += float4(gHistory.SampleLevel(gSampler, float2(uv12.x, uv12.y), 0.0f).rgb, 1.0f) * w12.x * w12.y;
	sum += float4(gHistory.SampleLevel(gSampler, float2(uv3.x, uv12.y), 0.0f).rgb, 1.0f) * w3.x * w12.y;
	sum += float4(gHistory.SampleLevel(gSampler, float2(uv12.x, uv3.y), 0.0f).rgb, 1.0f) * w12.x * w3.y;
	return max(sum.rgb / sum.a, 0.0f);
}

PS_OUTPUT main(PS_INPUT input)
{
	uint2 size;
	gColor.GetDimensions(size.x, size.y);
	const int2 pixel = int2(input.Position.xy);
	const float2 uv = (float2(pixel) + 0.5f) / float2(size);
	const float4 center = gColor.Load(int3(pixel, 0));

	// 1. 주변 9픽셀의 범위와, 지터를 되돌린 이번 프레임의 색
	float3 mean = float3(0.0f, 0.0f, 0.0f);
	float3 squared = float3(0.0f, 0.0f, 0.0f);
	float3 minimum = float3(1e6f, 1e6f, 1e6f);
	float3 maximum = float3(-1e6f, -1e6f, -1e6f);
	float3 filtered = float3(0.0f, 0.0f, 0.0f);
	float filteredWeight = 0.0f;
	// 움직이는 물체의 가장자리도 물체와 함께 움직이도록 가장 가까운 면의 이동량을 쓴다.
	int2 nearestPixel = pixel;
	float nearestDistance = 1e30f;
	bool hasSurface = false;
	for (int y = -1; y <= 1; ++y)
	{
		for (int x = -1; x <= 1; ++x)
		{
			const int2 tap = clamp(pixel + int2(x, y), int2(0, 0), int2(size) - 1);
			const float3 value = ToYCoCg(Compress(gColor.Load(int3(tap, 0)).rgb));
			mean += value;
			squared += value * value;
			minimum = min(minimum, value);
			maximum = max(maximum, value);

			const float2 offset = float2(x, y) - gParams.JitterPixels;
			const float weight = exp(-2.29f * dot(offset, offset));
			filtered += value * weight;
			filteredWeight += weight;

			if (gDiffuse.Load(int3(tap, 0)).a >= 0.01f)
			{
				const float surfaceDistance = distance(gPosition.Load(int3(tap, 0)).xyz, gParams.CameraPosition);
				if (surfaceDistance < nearestDistance)
				{
					nearestDistance = surfaceDistance;
					nearestPixel = tap;
					hasSurface = true;
				}
			}
		}
	}
	filtered /= filteredWeight;
	mean /= 9.0f;
	const float3 deviation = sqrt(max(squared / 9.0f - mean * mean, 0.0f));

	// 2. 이전 결과
	float3 result = filtered;
	float2 motion;
	if (gParams.HasHistory != 0u && FindMotion(nearestPixel, uv, hasSurface, motion))
	{
		const float2 previousUV = uv + motion;
		if (all(previousUV >= 0.0f) && all(previousUV <= 1.0f))
		{
			float3 history = ToYCoCg(SampleHistory(previousUV, float2(size)));
			const float3 low = max(minimum, mean - deviation * 1.25f);
			const float3 high = min(maximum, mean + deviation * 1.25f);
			// 범위의 중심 쪽으로 당겨 자른다. 축마다 따로 자르면 없던 색이 생긴다.
			const float3 boxCenter = (low + high) * 0.5f;
			const float3 boxExtent = max((high - low) * 0.5f, 0.0001f);
			const float3 direction = history - boxCenter;
			const float3 units = abs(direction) / boxExtent;
			const float outside = max(units.x, max(units.y, units.z));
			if (outside > 1.0f)
				history = boxCenter + direction / outside;

			const float speed = length(motion * float2(size));
			result = lerp(history, filtered, lerp(gParams.StillBlend, gParams.MovingBlend, saturate(speed / 8.0f)));
		}
	}

	const float3 compressed = clamp(FromYCoCg(result), 0.0f, 0.99998f);
	PS_OUTPUT output;
	output.Color = float4(Expand(compressed), center.a);
	output.History = float4(compressed, 1.0f);
	return output;
}
