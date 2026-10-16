# Shader Resource Binding Layout

This document defines the shared resource-group order for HLSL/D3D12 and
GLSL/Vulkan shaders.

## Terminology

- HLSL `space` corresponds to a Vulkan GLSL descriptor `set`.
- HLSL register numbers correspond to GLSL `binding` numbers.
- Register prefixes identify the resource class: `b` (CBV), `t` (SRV), `u`
  (UAV), and `s` (sampler).

The pair `(register number, space)` must match the pair `(binding, set)` in
the equivalent GLSL shader. For example, HLSL `register(b1, space1)` maps to
GLSL `layout(set = 1, binding = 1)`.

## Space Order

| Space / set | Update scope | Current resources |
| --- | --- | --- |
| `0` | Skinning data | `t0`: `boneMatrices` / `BoneSSBO` |
| `1` | Per-frame vertex data and per-object draw data | `b0`: `CommonVSData`; `b1`: `ObjectData` |
| `2` | Material textures, render targets, and samplers | `t0-t15`: SRVs; `s0-s7`: samplers |
| `3` | Per-frame pixel data and pass-specific constants | `b0`: `CommonPSData`, post-process, or viewport data; `b1`: shadow or sky data |

The numeric order is an engine convention, not an HLSL requirement. New
resource groups should use the lowest space whose update scope matches the
resource. Do not renumber an existing space solely to make its number reflect
an ideal update-frequency order; that changes the cross-backend shader ABI.

## D3D12 Root Signature Requirements

Each descriptor range must declare the same register space as its shader
resources. `CD3DX12_DESCRIPTOR_RANGE::Init` defaults to `RegisterSpace = 0`,
so pass the fourth argument for every non-zero space.

```cpp
CD3DX12_DESCRIPTOR_RANGE ranges[5];
ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0, 1);  // b0, space1
ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1, 1);  // b1, space1
ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 2, 0, 3);  // b0-b1, space3
ranges[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 16, 0, 2); // t0-t15, space2
ranges[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 8, 0, 2); // s0-s7, space2
```

Skinned vertex shaders additionally require an SRV range for `t0, space0`.
It must be separate from the `t0-t15, space2` texture range.

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
