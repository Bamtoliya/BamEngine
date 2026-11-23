# Shader Resource Binding Layout

- 최종 갱신: 2026-11-22 (Compute 쓰기 텍스처 UAV 테이블 `[11]` 추가, 루트 파라미터 가시성 정정)
- 관련 문서: [BuiltInResourcesAndShaders.md](../BuiltInResourcesAndShaders.md) (기본 리소스 및 셰이더 전체 명세서)

This document defines the shared resource-group order for HLSL/D3D12 and
GLSL/Vulkan shaders.

## Terminology

- HLSL `space` corresponds to a Vulkan GLSL descriptor `set`.
- HLSL register numbers correspond to GLSL `binding` numbers.
- Register prefixes identify the resource class: `b` (CBV), `t` (SRV), `u`
  (UAV), and `s` (sampler).

The pair `(register number, space)` must match the pair `(binding, set)` in
the equivalent GLSL shader. For example, HLSL `register(b0, space2)` maps to
GLSL `layout(set = 2, binding = 0)`.

## Space Order (Current DirectX12RHI & HLSL Contract)

| Space / set | Update scope | Root Parameter Index (`DirectX12RHI`) | Current resources |
| --- | --- | --- | --- |
| `0` | Global / Per-frame camera data | `[0]` Root CBV (`b0, space0`) | `b0`: `CommonVSData` / `CameraBuffer` (RHI CBV slot `0`) |
| `1` | Material textures, LUTs, and material samplers | `[1]` SRV Table (`t0-t15, space1`)<br>`[2]` Sampler Table (`s0-s7, space1`) | `t0-t15`: Material SRVs (or Cloud `shapeNoiseTex`/`weatherMapTex` in LightingPass)<br>`s0-s7`: Material Samplers (RHI texture slots `0..15`) |
| `2` | Per-object draw constants and instancing buffer | `[3]` Root CBV (`b0, space2`)<br>`[4]` Root SRV (`t0, space2`) | `b0`: `ObjectData` / `SceneUBO` (RHI CBV slot `1`)<br>`t0`: `StructuredBuffer<InstanceData> g_Instances` (`BindInstanceBuffer`) |
| `3` | Pass-specific constants, G-Buffer/Shadow/Light SRVs, Samplers, and Compute UAVs | `[5]` Root CBV (`b0, space3`)<br>`[6]` Root CBV (`b1, space3`)<br>`[7]` SRV Table (`t0-t15, space3`)<br>`[8]` Sampler Table (`s0-s7, space3`)<br>`[9]` Root UAV (`u0, space3`)<br>`[10]` Root UAV (`u1, space3`)<br>`[11]` UAV Table (`u2-u5, space3`) | `b0`: `FogData` / PostProcess params / `CullingConstants` / GeometryPass `MaterialConstants` (RHI CBV slot `2`)<br>`b1`: `LightingPassData` / Shadow / Sky data (RHI CBV slot `3`) ...<br>`u0`/`u1`: Compute 버퍼 UAV (`BindComputeStorageBuffer`)<br>`u2-u5`: Compute `RWTexture2D` (`BindComputeStorageTexture` slot `0..3`, D3D12 전용) |

> **Note on Skinning & Backend Parity (2026-10-30)**:
> - **2026-11-06 정정:** 현재 `skinningVS.hlsl`의 `boneMatrices`는 `t0, space2`를 사용한다. `Renderer.cpp`는 본 행렬을 `BindInstanceBuffer(..., sizeof(mat4))`로 제출하고 D3D12 root parameter `[4]`가 이를 받는다. 같은 슬롯은 정적 인스턴싱의 `InstanceData`에도 사용되므로 드로우의 셰이더와 stride 계약을 맞춘다. 두 데이터의 동시 사용 지원을 의미하지 않는다. 과거 `t0, space0` 미연결 설명을 현재 상태로 적용하지 않는다.
> - `SDLGPURHI` currently implements graphics CBV/texture/sampler bindings (`space0`, `space1`, `space2` CBV, `space3` CBV/SRV/Sampler), while `DrawIndexedInstanced`, `BindInstanceBuffer` (`t0, space2`), and Compute UAV/Dispatch (`u0-u1, space3`) are currently D3D12-only.

## UI Image Draw Data

The UI object constant buffer uses RHI slot 1 (`b0, space2` in HLSL and
`set = 1, binding = 1` in the SDL GPU GLSL vertex layout). The 112-byte image
prefix is followed by eight `float4`/`vec4` rows for four ellipse masks. At
offset 240, two 64-byte image mask records bring the total to 368 bytes. Each
record supplies two canvas-to-mask rows, the atlas UV region and its sampling
bounds. An inactive record has a zero UV span.

The built-in pixel shader uses texture slots 0–2 for the image and masks. In
HLSL these are `t0`–`t2, space1` with sampler `s0, space1`. In SDL GPU GLSL
these are combined samplers at `set = 2, bindings 0–2`. Masks use bilinear mip 0
sampling within their atlas bounds. For Tint conversion, the UI fragment
shader uses separate texture bindings 0-2 and a shared sampler at binding 3.
Rebuild both shader stages after changing this interface.

## D3D12 Root Signature Layout (`DirectX12RHI.cpp`)

Each descriptor range and root descriptor must declare the same register space as its shader resources:

```cpp
CD3DX12_DESCRIPTOR_RANGE matSrvRange;
matSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 1);     // t0-t15, space1
CD3DX12_DESCRIPTOR_RANGE matSampRange;
matSampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 8, 0, 1); // s0-s7, space1

CD3DX12_DESCRIPTOR_RANGE passSrvRange;
passSrvRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 3);    // t0-t15, space3
CD3DX12_DESCRIPTOR_RANGE passSampRange;
passSampRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 8, 0, 3);// s0-s7, space3

CD3DX12_DESCRIPTOR_RANGE computeUavRange;
computeUavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 2, 3);  // u2-u5, space3

CD3DX12_ROOT_PARAMETER rootParams[14];
rootParams[0].InitAsConstantBufferView(0, 0);                    // b0, space0 (Global CBV)
rootParams[1].InitAsDescriptorTable(1, &matSrvRange, D3D12_SHADER_VISIBILITY_ALL);
rootParams[2].InitAsDescriptorTable(1, &matSampRange, D3D12_SHADER_VISIBILITY_ALL);
rootParams[3].InitAsConstantBufferView(0, 2);                    // b0, space2 (Object CBV)
rootParams[4].InitAsShaderResourceView(0, 2, D3D12_SHADER_VISIBILITY_VERTEX); // t0, space2 (Instance SRV)
rootParams[5].InitAsConstantBufferView(0, 3);                    // b0, space3 (Pass CBV #0)
rootParams[6].InitAsConstantBufferView(1, 3);                    // b1, space3 (Pass CBV #1)
rootParams[7].InitAsDescriptorTable(1, &passSrvRange, D3D12_SHADER_VISIBILITY_ALL);
rootParams[8].InitAsDescriptorTable(1, &passSampRange, D3D12_SHADER_VISIBILITY_PIXEL);
rootParams[9].InitAsUnorderedAccessView(0, 3);                   // u0, space3 (Pass UAV #0)
rootParams[10].InitAsUnorderedAccessView(1, 3);                  // u1, space3 (Pass UAV #1)
rootParams[11].InitAsDescriptorTable(1, &computeUavRange, D3D12_SHADER_VISIBILITY_ALL); // u2-u5, space3
// Resource Binding Tier 2 이상에서만 추가한다.
rootParams[12].InitAsDescriptorTable(1, &bindlessTextureRange, D3D12_SHADER_VISIBILITY_ALL); // t0.., space4 (SRV 힙 전체)
rootParams[13].InitAsDescriptorTable(1, &bindlessBufferRange, D3D12_SHADER_VISIBILITY_ALL);  // t0.., space5 (SRV 힙 전체)
// 정적 샘플러 s0, space4 (linear wrap)
```

> **2026-11-22 갱신:** `[4]`는 VERTEX, `[8]`은 PIXEL 가시성이다(이전 문서의 ALL 표기는 코드와 달랐다).
> `[11]`은 Compute Dispatch마다 SRV 테이블 뒤에 4개 UAV 디스크립터로 기록되며, 빈 슬롯은 null UAV다.
> 쓰기 텍스처 바인딩은 다음 `Dispatch` 한 번에만 적용된다. 구성이 바뀌면 `DirectX12RHI::GLOBAL_ROOT_SIGNATURE_LAYOUT_VERSION`을 올려 PSO 디스크 캐시 키를 분리한다.

### 레이트레이싱 그림자 바인딩 (D3D12 전용)

| Register | 셰이더 | 내용 |
| --- | --- | --- |
| `t13, space3` | `raytracedShadowMaskRTCS` | TLAS (`BindAccelerationStructure`, 다음 `Dispatch` 한 번에만 적용) |
| `u2, space3` | `raytracedShadowMaskRTCS` | RGBA8 그림자 마스크 출력. 채널마다 광원 하나 (`BindComputeStorageTexture` slot `0`) |
| `b1, space3` | `raytracedShadowMaskRTCS` | `RaytracedShadowParams` 32바이트. 채널별 광원 버퍼 인덱스 포함 (이후 조명 패스가 같은 슬롯에 `LightingPassData`를 다시 묶음) |
| `t14, space3` | `lightingPS` | 그림자 마스크. `LightingPassData.RaytracedShadowLights`의 채널과 광원 인덱스가 같을 때만 사용 |

### 레이트레이싱 반사와 번호로 읽는 리소스 (D3D12 전용)

| Register | 셰이더 | 내용 |
| --- | --- | --- |
| `t0.., space4` | `raytracing.hlsli` | `Texture2D` 배열. 번호는 `RHI::GetBindlessIndex(RHITexture*)` |
| `t0.., space5` | `raytracing.hlsli` | `ByteAddressBuffer` 배열. 정점·인덱스 버퍼와 인스턴스 표. 번호는 `RHI::GetBindlessIndex(RHIBuffer*)` |
| `s0, space4` | `raytracing.hlsli` | 정적 샘플러 |
| `t13, space3` | `raytracedReflectionRTCS` | TLAS. 그림자와 공유하며 광선 마스크로 구분 |
| `u2, space3` | `raytracedReflectionRTCS` | RGBA16F 추적 결과. rgb는 비율을 곱한 밝기, a는 합성 비율 |
| `u3, space3` | `raytracedReflectionRTCS` | R16F 닿은 거리 |
| `u2`·`u3, space3` | `raytracedReflectionTemporalRTCS` | RGBA16F 누적 결과와 표면(카메라 거리, 법선, 누적 횟수) |
| `u2, space3` | `raytracedReflectionFilterRTCS` | RGBA16F 필터 결과. `lightingPS`가 읽음 |
| `b1, space3` | `raytracedReflectionRTCS` | `RaytracedReflectionParams` 64바이트 |
| `b1, space3` | `raytracedReflectionTemporalRTCS`, `raytracedReflectionFilterRTCS` | `ReflectionDenoiseParams` 112바이트. 앞 패스의 출력은 번호로 읽음 |
| `t15, space3` | `lightingPS` | 반사. `LightingPassData.UseRaytracedReflections`가 1일 때만 읽음 |

`space4`·`space5`는 Compute에서만 묶는다. `RaytracingInstanceData`(80바이트)는 `raytracing.hlsli`의 `RaytracedInstance`와,
광선 마스크는 세 셰이더 파일의 상수와 일치해야 한다. 버퍼 번호는 최대 16,384개이며 버퍼가 해제되면 지연 반환한다.

파일명이 `RTCS`로 끝나는 셰이더는 `cs_6_5`로 컴파일하고 DXIL만 생성한다.
`LightingPassData`는 544바이트이며 `DeferredSceneRenderer.cpp`의 `DeferredLightingPassData`와 일치해야 한다.

### 재질 알파 기준값

재질 상수 `materialFlags`(`b0, space3`)의 상위 8비트는 Masked 재질의 알파 기준값이다(1~255, 0은 Masked 아님).
`PackMaterialAlphaCutoff`(`MaterialInterface.h`)와 `GetAlphaDiscardThreshold`(`shading_flags.hlsli`)가 일치해야 한다.
`shadowDepthMaskedPS`는 `gbufferPS`와 같은 재질 상수, `t0`·`s0, space1`을 읽는다.

## Change Checklist

When adding or moving a binding:

1. Assign a resource group and register number without overlapping an existing
   binding of the same resource class and space.
2. Update the HLSL `register(..., spaceN)` declaration.
3. Update the matching GLSL `layout(set = N, binding = M)` declaration.
4. Update the D3D12 root-signature descriptor range, including `RegisterSpace`.
5. Update descriptor-set creation/binding code for Vulkan and descriptor-heap
   binding code for D3D12.
6. Rebuild and validate both shader backends.

### 모션 벡터와 TAA 바인딩 (D3D12 전용)

| 바인딩 | 셰이더 | 내용 |
| --- | --- | --- |
| `t0, space2` | `motionVectorVS` | 이번·이전 월드 행렬 쌍(128바이트) |
| `b0, space2` | `motionVectorSkinningVS` | 이번·이전 월드 행렬 |
| `t0, space2` | `motionVectorSkinningVS` | 이번 프레임 뼈 256개 뒤에 이전 프레임 256개 |
| `b1, space3` | `motionVector*` | `MotionConstants` 192바이트 |
| `t4, space3` | `motionVectorPS` | G-Buffer 위치 |
| `b1, space3` | `temporalAAPS` | `TemporalAAParams` 224바이트 |
| `t0`~`t4`, `s0`, `space3` | `temporalAAPS` | 디퓨즈, 장면 색 복사본, 이전 결과, 모션 벡터, 위치 |
| `b1, space3` | `infiniteGridPS` | `GridDepthParams` 16바이트. 깊이 타깃의 TAA 지터(NDC), 끄면 0 |

### 디퍼드 데칼 바인딩 (D3D12 전용)

| 바인딩 | 셰이더 | 내용 |
| --- | --- | --- |
| `b0, space0` | `decalVS` | G-Buffer를 그린 카메라(TAA를 켜면 지터 포함) |
| `b1, space3` | `decalVS`, `decalPS` | `DecalParams` 224바이트(`decal.hlsli`). `DeferredSceneRenderer.cpp`의 `DecalParams`와 일치해야 한다 |
| `t0`, `t1, space3` | `decalPS` | G-Buffer 위치, 데칼 패스 직전에 복사한 G-Buffer 법선(a에 셰이딩 플래그) |
| `t0`~`t2`, `s0, space1` | `decalPS` | 기본 색, 접선 공간 법선, 표면(R 거칠기·G 금속성·B 캐비티) 텍스처. 없으면 흰 텍스처를 묶고 `TextureFlags`로 무시한다 |

`decalPS`는 `SV_Target0~2`로 G-Buffer 디퓨즈·법선·PBR에 쓴다. 쓰기 마스크가 RGB라 각 타깃의 a는 바뀌지 않고, 출력 알파는 섞는 비율로만 쓰인다.
`decalVS`는 정점 버퍼 없이 `Draw(36)`로 단위 상자를 만들며 상자 삼각형 순서는 `DecalProjection.h`의 `DecalBoxIndices`와 같다.
G-Buffer 정점 셰이더(`gbufferVS`, `gbufferSkinningVS`, 셰이더 그래프 VS)는 마지막 출력 `NoDecals : TEXCOORD6`(인스턴스·오브젝트 `PickingData.y`)를 낸다.
`gbufferPS`와 셰이더 그래프 G-Buffer PS는 이 값이 1이면 `FLAG_RECEIVE_DECALS`를 빼고 기록한다. 맨 뒤 출력이라 읽지 않는 픽셀 셰이더와도 연결된다.

`temporalAAPS`는 `SV_Target0`에 HDR 장면 색, `SV_Target1`에 다음 프레임의 이력을 쓴다. G-Buffer의 발광 타깃은 1보다 밝은 값을 담도록 `R11G11B10_UFLOAT`이다(알파 없음).
