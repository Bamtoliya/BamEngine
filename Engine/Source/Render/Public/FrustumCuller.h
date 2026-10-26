#pragma once

#include "Types.h"
#include "Structs.h"

#include <cfloat>
#include <optional>


namespace Engine
{
    struct FrustumPlane
    {
        vec3 Normal;
        f32  Distance;
    };

    struct Frustum
    {
        FrustumPlane Planes[6]; // Left, Right, Bottom, Top, Near, Far
    };

    class ENGINE_API FrustumCuller
    {
    public:
        // Gribb/Hartmann 방식으로 View-Projection 행렬에서 6개 평면을 추출합니다.
        // Vulkan NDC 기준: Z in [0,1] → near = row2, far = row3 - row2
        // GLM column-major: m[col][row] → row(i) = (m[0][i], m[1][i], m[2][i], m[3][i])
        static Frustum ExtractFrustum(const mat4& viewProj)
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

            Frustum f;
            for (int i = 0; i < 6; ++i)
            {
                const float len = glm::length(vec3(raw[i]));
                const float invLen = (len > 1e-6f) ? (1.f / len) : 1.f;
                f.Planes[i].Normal = vec3(raw[i]) * invLen;
                f.Planes[i].Distance = raw[i].w * invLen;
            }
            return f;
        }

        // BoundingBox를 padding만큼 균등하게 확장합니다.
        // Shadow Pass에서 큰 오브젝트의 그림자가 잘리는 것을 방지하기 위해 사용합니다.
        static BoundingBox ExpandBoundingBox(const BoundingBox& aabb, float padding)
        {
            return BoundingBox(aabb.min - vec3(padding), aabb.max + vec3(padding));
        }

        // worldBoundingBox가 프러스텀 내부에 (부분적으로라도) 존재하면 true를 반환합니다.
        // 판정 공식: dot(plane.Normal, positiveVertex) + plane.Distance >= 0 → 내부
        static bool TestBoundingBox(const Frustum& frustum, const BoundingBox& worldBoundingBox)
        {
            for (const FrustumPlane& plane : frustum.Planes)
            {
                // 법선 방향으로 가장 멀리 있는 코너 (Positive Vertex)
                const vec3 pv =
                {
                    (plane.Normal.x >= 0.f) ? worldBoundingBox.max.x : worldBoundingBox.min.x,
                    (plane.Normal.y >= 0.f) ? worldBoundingBox.max.y : worldBoundingBox.min.y,
                    (plane.Normal.z >= 0.f) ? worldBoundingBox.max.z : worldBoundingBox.min.z,
                };

                if (glm::dot(plane.Normal, pv) + plane.Distance < 0.f)
                    return false; // 이 평면 완전히 밖 → 컬링
            }
            return true;
        }
    };
}