#pragma once

#include "Types.h"
#include "Structs.h"

#include <cfloat>
#include <optional>


BEGIN(Engine)

struct tagFrustumPlane
{
    vec3 Normal;
    f32  Distance;
};

struct tagFrustum
{
    tagFrustumPlane Planes[6]; // Left, Right, Bottom, Top, Near, Far
};

class ENGINE_API FrustumCuller
{
public:
    // Gribb/Hartmann 방식으로 View-Projection 행렬에서 6개 평면을 추출합니다.
    // Vulkan NDC 기준: Z in [0,1] → near = row2, far = row3 - row2
    // GLM column-major: m[col][row] → row(i) = (m[0][i], m[1][i], m[2][i], m[3][i])
    static tagFrustum ExtractFrustum(const mat4& viewProj)
    {
        auto makeRow = [&](int i) -> vec4
            {
                return vec4(viewProj[0][i], viewProj[1][i], viewProj[2][i], viewProj[3][i]);
            };

        const vec4 r0 = makeRow(0);
        const vec4 r1 = makeRow(1);
        const vec4 r2 = makeRow(2);
        const vec4 r3 = makeRow(3);

        const vec4 raw[6] =
        {
            r3 + r0,  // Left
            r3 - r0,  // Right
            r3 + r1,  // Bottom
            r3 - r1,  // Top
            r2,       // Near  (Vulkan: Z >= 0)
            r3 - r2,  // Far   (Vulkan: Z <= 1)
        };

        tagFrustum f;
        for (int i = 0; i < 6; ++i)
        {
            const float len = glm::length(vec3(raw[i]));
            const float invLen = (len > 1e-6f) ? (1.f / len) : 1.f;
            f.Planes[i].Normal = vec3(raw[i]) * invLen;
            f.Planes[i].Distance = raw[i].w * invLen;
        }
        return f;
    }

    // 로컬 공간 AABB를 월드 공간으로 변환합니다 (8 코너 변환 후 재계산).
    static AABB TransformAABB(const AABB& localAABB, const mat4& worldMatrix)
    {
        const vec3 corners[8] =
        {
            { localAABB.Min.x, localAABB.Min.y, localAABB.Min.z },
            { localAABB.Max.x, localAABB.Min.y, localAABB.Min.z },
            { localAABB.Min.x, localAABB.Max.y, localAABB.Min.z },
            { localAABB.Max.x, localAABB.Max.y, localAABB.Min.z },
            { localAABB.Min.x, localAABB.Min.y, localAABB.Max.z },
            { localAABB.Max.x, localAABB.Min.y, localAABB.Max.z },
            { localAABB.Min.x, localAABB.Max.y, localAABB.Max.z },
            { localAABB.Max.x, localAABB.Max.y, localAABB.Max.z },
        };

        vec3 worldMin(FLT_MAX);
        vec3 worldMax(-FLT_MAX);
        for (const vec3& c : corners)
        {
            const vec3 wc = vec3(worldMatrix * vec4(c, 1.f));
            worldMin = glm::min(worldMin, wc);
            worldMax = glm::max(worldMax, wc);
        }
        return AABB(worldMin, worldMax);
    }

    // AABB를 padding만큼 균등하게 확장합니다.
    // Shadow Pass에서 큰 오브젝트의 그림자가 잘리는 것을 방지하기 위해 사용합니다.
    static AABB ExpandAABB(const AABB& aabb, float padding)
    {
        return AABB(aabb.Min - vec3(padding), aabb.Max + vec3(padding));
    }

    // worldAABB가 프러스텀 내부에 (부분적으로라도) 존재하면 true를 반환합니다.
    // 판정 공식: dot(plane.Normal, positiveVertex) + plane.Distance >= 0 → 내부
    static bool TestAABB(const tagFrustum& frustum, const AABB& worldAABB)
    {
        for (const tagFrustumPlane& plane : frustum.Planes)
        {
            // 법선 방향으로 가장 멀리 있는 코너 (Positive Vertex)
            const vec3 pv =
            {
                (plane.Normal.x >= 0.f) ? worldAABB.Max.x : worldAABB.Min.x,
                (plane.Normal.y >= 0.f) ? worldAABB.Max.y : worldAABB.Min.y,
                (plane.Normal.z >= 0.f) ? worldAABB.Max.z : worldAABB.Min.z,
            };

            if (glm::dot(plane.Normal, pv) + plane.Distance < 0.f)
                return false; // 이 평면 완전히 밖 → 컬링
        }
        return true;
    }
};

END