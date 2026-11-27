#ifndef BAM_INSTANCE_DATA_HLSL
#define BAM_INSTANCE_DATA_HLSL

// C++ Engine/Source/Render/Public/InstanceData.h와 크기·순서가 같아야 한다 (BindInstanceBuffer stride = sizeof(InstanceData)).
// t0, space2는 스키닝 본 행렬·모션 벡터도 다른 구조체로 쓰므로 common.hlsli가 아닌 별도 파일로 둔다.
struct InstanceData
{
    float4x4 WorldMatrix;
    float4 PickingData; // x: pickingId, y: 1이면 데칼을 받지 않음
    float4 CustomData;  // StaticMeshRendererComponent::customData. 의미는 머티리얼 셰이더가 정한다.
};

StructuredBuffer<InstanceData> g_Instances : register(t0, space2);

#endif
