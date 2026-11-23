// 피사계 심도 공용 선언. DepthOfFieldEffect.cpp의 DepthOfFieldShaderParams, DepthOfField.h의 ComputeDepthOfFieldCoC와 맞춘다.
#ifndef DEPTH_OF_FIELD_HLSLI
#define DEPTH_OF_FIELD_HLSLI

struct PS_INPUT
{
    float4 Position : SV_POSITION;
    float2 TexCoord : TEXCOORD0;
};

struct DepthOfFieldParams
{
    float3 CameraPosition;
    float FocusDistance;   // m
    float3 CameraForward;  // 카메라가 바라보는 방향(월드)
    float HalfRange;       // FocusRange 식: 선명한 폭의 절반 (m)
    float InvTransition;   // FocusRange 식: 1 / 흐림이 최대가 되기까지의 거리
    float LensScale;       // PhysicalLens 식: 무한히 먼 곳의 CoC (전체 해상도 픽셀)
    float MaxRadius;       // CoC 상한 (전체 해상도 픽셀)
    uint Mode;             // 0: FocusRange, 1: PhysicalLens
    float2 SourceSize;     // 전체 해상도
    float2 InvSourceSize;
    float2 HalfSize;       // 절반 해상도
    float2 InvHalfSize;
    uint NearPass;         // 흐림 패스: 0이면 뒤쪽, 1이면 앞쪽
    uint DilateTiles;      // 앞쪽 CoC 타일을 넓히는 반지름 (타일)
    float2 Padding;
};

ConstantBuffer<DepthOfFieldParams> gDof : register(b0, space3);
SamplerState gLinearClamp : register(s0, space3);

// 하늘은 무한히 먼 곳으로 본다.
static const float DOF_SKY_DEPTH = 1.0e9f;

// G-Buffer 알베도의 a가 0.01보다 작으면 표면이 없다(temporalAAPS와 같은 기준).
float DofViewDepth(float3 worldPosition, float diffuseAlpha)
{
    if (diffuseAlpha < 0.01f)
        return DOF_SKY_DEPTH;
    return dot(worldPosition - gDof.CameraPosition, gDof.CameraForward);
}

// 시선 방향 거리 z(m)의 흐림 반지름. 앞은 음수, 뒤는 양수다.
float DofComputeCoC(float z)
{
    z = max(z, 0.001f);
    float coc;
    if (gDof.Mode == 1u)
    {
        coc = gDof.LensScale * (1.0f - gDof.FocusDistance / z);
    }
    else
    {
        const float offset = z - gDof.FocusDistance;
        const float blur = saturate((abs(offset) - gDof.HalfRange) * gDof.InvTransition);
        coc = (offset < 0.0f ? -1.0f : 1.0f) * gDof.MaxRadius * blur;
    }
    return clamp(coc, -gDof.MaxRadius, gDof.MaxRadius);
}

#endif
