# Shader Resource Binding Layout

- 최종 갱신: 2026-10-30 (`DirectX12RHI::CreateRootSignature` 및 현행 HLSL 셰이더 바인딩 반영)

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
| `3` | Pass-specific constants, G-Buffer/Shadow/Light SRVs, Samplers, and Compute UAVs | `[5]` Root CBV (`b0, space3`)<br>`[6]` Root CBV (`b1, space3`)<br>`[7]` SRV Table (`t0-t15, space3`)<br>`[8]` Sampler Table (`s0-s7, space3`)<br>`[9]` Root UAV (`u0, space3`)<br>`[10]` Root UAV (`u1, space3`) | `b0`: `FogData` / PostProcess params / `CullingConstants` / GeometryPass `MaterialConstants` (RHI CBV slot `2`)<br>`b1`: `LightingPassData` / Shadow / Sky data (RHI CBV slot `3`) ... |

> **Note on Skinning & Backend Parity (2026-10-30)**:
> - Legacy `boneMatrices` (`t0, space0`) is not currently declared in `DirectX12RHI::CreateRootSignature()`. When skeletal skinning (P2-D) is connected, its SRV/CBV slot must be assigned without colliding with `space0`–`space3` above.
> - `SDLGPURHI` currently implements graphics CBV/texture/sampler bindings (`space0`, `space1`, `space2` CBV, `space3` CBV/SRV/Sampler), while `DrawIndexedInstanced`, `BindInstanceBuffer` (`t0, space2`), and Compute UAV/Dispatch (`u0-u1, space3`) are currently D3D12-only.

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

CD3DX12_ROOT_PARAMETER rootParams[11];
rootParams[0].InitAsConstantBufferView(0, 0);                    // b0, space0 (Global CBV)
rootParams[1].InitAsDescriptorTable(1, &matSrvRange, D3D12_SHADER_VISIBILITY_ALL);
rootParams[2].InitAsDescriptorTable(1, &matSampRange, D3D12_SHADER_VISIBILITY_ALL);
rootParams[3].InitAsConstantBufferView(0, 2);                    // b0, space2 (Object CBV)
rootParams[4].InitAsShaderResourceView(0, 2);                    // t0, space2 (Instance SRV)
rootParams[5].InitAsConstantBufferView(0, 3);                    // b0, space3 (Pass CBV #0)
rootParams[6].InitAsConstantBufferView(1, 3);                    // b1, space3 (Pass CBV #1)
rootParams[7].InitAsDescriptorTable(1, &passSrvRange, D3D12_SHADER_VISIBILITY_ALL);
rootParams[8].InitAsDescriptorTable(1, &passSampRange, D3D12_SHADER_VISIBILITY_ALL);
rootParams[9].InitAsUnorderedAccessView(0, 3);                   // u0, space3 (Pass UAV #0)
rootParams[10].InitAsUnorderedAccessView(1, 3);                  // u1, space3 (Pass UAV #1)
```

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
