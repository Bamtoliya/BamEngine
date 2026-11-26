// 화면 공간 깊이·노멀 기반 Roberts Cross 엣지 검출 아웃라인 셰이더
struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct OutlineParams
{
    float4 OutlineColor;     // rgb: 아웃라인 색상, a: 불투명도
    float3 CameraPosition;   // 월드 공간 카메라 위치
    float  Thickness;        // 아웃라인 두께 (픽셀 단위)
    float3 CameraForward;    // 월드 공간 카메라 전방 벡터
    float  DepthThreshold;   // 상대 깊이 불연속성 임계값
    float2 InvSourceSize;    // 1.0 / (너비, 높이)
    float  NormalThreshold;  // 법선 불연속성 임계값
    float  Intensity;        // 아웃라인 합성 강도 [0, 1]
    uint   HasNormalMap;     // 1이면 gNormal(G-Buffer) 사용, 0이면 gPosition에서 법선 근사
    float3 Padding;          // 16바이트 정렬 패딩 (총 80바이트)
};

Texture2D gScene    : register(t0, space3);
Texture2D gPosition : register(t1, space3);
Texture2D gDiffuse  : register(t2, space3);
Texture2D gNormal   : register(t3, space3);

SamplerState gPointClamp : register(s0, space3);
ConstantBuffer<OutlineParams> gOutline : register(b0, space3);

static const float OUTLINE_SKY_DEPTH = 1.0e9f;

float SampleViewDepth(int2 p, int2 maxCoord)
{
    int3 coord = int3(clamp(p, int2(0, 0), maxCoord), 0);
    if (gDiffuse.Load(coord).a < 0.01f)
        return OUTLINE_SKY_DEPTH;

    float3 worldPos = gPosition.Load(coord).xyz;
    return max(dot(worldPos - gOutline.CameraPosition, gOutline.CameraForward), 0.001f);
}

float3 SampleWorldNormal(int2 p, int2 maxCoord)
{
    int3 coord = int3(clamp(p, int2(0, 0), maxCoord), 0);
    if (gDiffuse.Load(coord).a < 0.01f)
        return float3(0.0f, 0.0f, 0.0f);

    if (gOutline.HasNormalMap != 0u)
    {
        float3 n = gNormal.Load(coord).xyz * 2.0f - 1.0f;
        float lenSq = dot(n, n);
        return lenSq > 1e-6f ? n * rsqrt(lenSq) : float3(0.0f, 0.0f, 0.0f);
    }

    // NormalRT가 전달되지 않은 경로에서는 인접 위치 차분으로 기하 법선을 근사한다.
    int3 cR = int3(clamp(p + int2(1, 0), int2(0, 0), maxCoord), 0);
    int3 cD = int3(clamp(p + int2(0, 1), int2(0, 0), maxCoord), 0);
    float3 p0 = gPosition.Load(coord).xyz;
    float3 pR = (gDiffuse.Load(cR).a >= 0.01f) ? gPosition.Load(cR).xyz : p0;
    float3 pD = (gDiffuse.Load(cD).a >= 0.01f) ? gPosition.Load(cD).xyz : p0;
    float3 n = cross(pR - p0, pD - p0);
    float lenSq = dot(n, n);
    return lenSq > 1e-10f ? n * rsqrt(lenSq) : float3(0.0f, 0.0f, 0.0f);
}

float4 main(PS_INPUT input) : SV_TARGET
{
    uint width = 1, height = 1;
    gScene.GetDimensions(width, height);
    const int2 maxCoord = int2(int(width) - 1, int(height) - 1);
    const int2 pixel = clamp(int2(input.Position.xy), int2(0, 0), maxCoord);

    const float4 scene = gScene.Load(int3(pixel, 0));

    // Thickness=1.0일 때 (0,0), (1,1), (1,0), (0,1)의 정확한 1픽셀 Roberts Cross 커널을 사용한다.
    const float halfThickness = max(gOutline.Thickness, 0.5f) * 0.5f;
    const int halfFloor = int(floor(halfThickness));
    const int halfCeil  = max(1, int(ceil(halfThickness)));

    const int2 p00 = pixel + int2(-halfFloor, -halfFloor);
    const int2 p11 = pixel + int2( halfCeil,   halfCeil);
    const int2 p10 = pixel + int2( halfCeil,  -halfFloor);
    const int2 p01 = pixel + int2(-halfFloor,  halfCeil);

    // 1. Roberts Cross 깊이 불연속성 검출
    const float z00 = SampleViewDepth(p00, maxCoord);
    const float z11 = SampleViewDepth(p11, maxCoord);
    const float z10 = SampleViewDepth(p10, maxCoord);
    const float z01 = SampleViewDepth(p01, maxCoord);

    const float d0 = abs(z00 - z11) / max(min(z00, z11), 1e-4f);
    const float d1 = abs(z10 - z01) / max(min(z10, z01), 1e-4f);
    const float depthEdge = sqrt(d0 * d0 + d1 * d1);

    // 2. Roberts Cross 법선 불연속성 검출
    const float3 n00 = SampleWorldNormal(p00, maxCoord);
    const float3 n11 = SampleWorldNormal(p11, maxCoord);
    const float3 n10 = SampleWorldNormal(p10, maxCoord);
    const float3 n01 = SampleWorldNormal(p01, maxCoord);

    const float3 nDiff0 = n00 - n11;
    const float3 nDiff1 = n10 - n01;
    const float normalEdge = sqrt(dot(nDiff0, nDiff0) + dot(nDiff1, nDiff1));

    const float depthMask  = depthEdge  > gOutline.DepthThreshold  ? 1.0f : 0.0f;
    const float normalMask = normalEdge > gOutline.NormalThreshold ? 1.0f : 0.0f;
    const float edge = max(depthMask, normalMask);

    const float alpha = edge * saturate(gOutline.Intensity) * saturate(gOutline.OutlineColor.a);
    const float3 finalColor = lerp(scene.rgb, gOutline.OutlineColor.rgb, alpha);

    return float4(finalColor, scene.a);
}
