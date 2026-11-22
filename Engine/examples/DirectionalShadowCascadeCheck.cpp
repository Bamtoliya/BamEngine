#include "DirectionalShadowCascades.h"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "DirectionalShadowCascadeCheck failed: %s\n", message);
        ++g_Failures;
    }

    CameraBuffer MakeCamera(const vec3& position, const vec3& target, f32 fov = 60.0f)
    {
        const mat4 view = glm::lookAtLH(position, target, vec3(0.0f, 1.0f, 0.0f));
        return CameraBuffer::FromMatrices(view, glm::perspectiveLH_ZO(glm::radians(fov), 16.0f / 9.0f, 0.1f, 1000.0f), position, 0.0f);
    }

    // 뷰 공간 깊이 구간의 절두체 꼭짓점이 모두 캐스케이드 안에 있는지 확인한다.
    bool Covers(const CameraBuffer& camera, const CameraBuffer& cascade, f32 splitNear, f32 splitFar, bool orthographic = false)
    {
        for (const f32 depth : { splitNear, splitFar })
        {
            const f32 scale = orthographic ? 1.0f : depth;
            for (const f32 x : { -1.0f, 1.0f })
            {
                for (const f32 y : { -1.0f, 1.0f })
                {
                    const vec4 corner(x * scale / camera.projMatrix[0][0], y * scale / camera.projMatrix[1][1], depth, 1.0f);
                    const vec4 projected = cascade.viewProjMatrix * (camera.invViewMatrix * corner);
                    // 격자에 맞추며 최대 한 텍셀만큼 옮겨질 수 있다.
                    const f32 limit = 1.0f + 2.0f / SHADOW_CASCADE_RESOLUTION;
                    if (std::abs(projected.x) > limit || std::abs(projected.y) > limit || projected.z < -0.01f || projected.z > 1.01f) return false;
                }
            }
        }
        return true;
    }

    f32 TexelFraction(f32 ndc)
    {
        const f32 texel = ndc * SHADOW_CASCADE_RESOLUTION * 0.5f;
        return texel - std::round(texel);
    }
}

int main()
{
    DirectionalShadowSettings settings;
    settings.Direction = glm::normalize(vec3(0.3f, -0.8f, 0.5f));

    const CameraBuffer first = MakeCamera(vec3(0.0f, 2.0f, 0.0f), vec3(0.0f, 2.0f, 10.0f));
    const CameraBuffer second = MakeCamera(vec3(300.0f, 20.0f, -150.0f), vec3(310.0f, 10.0f, -150.0f), 40.0f);
    DirectionalShadowCascades a, b;
    Expect(BuildDirectionalShadowCascades(settings, &first, a) && BuildDirectionalShadowCascades(settings, &second, b), "valid views must build cascades");

    // 두 뷰는 서로의 캐스케이드를 쓰지 않는다.
    const f32 nearZ = 0.1f;
    const f32 splits[] = { nearZ, a.Splits.x, a.Splits.y, a.Splits.z };
    Expect(splits[1] < splits[2] && splits[2] < splits[3] && std::abs(splits[3] - settings.MaxDistance) < 0.01f, "splits must reach the maximum distance in order");
    for (uint32 c = 0; c < MAX_SHADOW_CASCADES; ++c)
    {
        Expect(Covers(first, a.Cameras[c], splits[c], splits[c + 1]), "cascades must cover the first view");
        Expect(Covers(second, b.Cameras[c], splits[c], splits[c + 1]), "cascades must cover the second view");
    }
    Expect(!Covers(second, a.Cameras[0], splits[0], splits[1]), "another view's cascade must not be usable");

    // 같은 카메라는 항상 같은 결과를 얻는다.
    DirectionalShadowCascades again;
    Expect(BuildDirectionalShadowCascades(settings, &first, again) && again.Cameras[1].viewProjMatrix == a.Cameras[1].viewProjMatrix, "the same view must build identical cascades");

    // 카메라를 조금씩 옮겨도 고정된 점은 텍셀 격자의 같은 위치에 남아야 한다.
    const vec4 point(1.0f, 0.5f, 6.0f, 1.0f);
    for (int step = 1; step <= 20; ++step)
    {
        const vec3 offset(0.013f * step, 0.004f * step, 0.021f * step);
        const CameraBuffer moved = MakeCamera(vec3(0.0f, 2.0f, 0.0f) + offset, vec3(0.0f, 2.0f, 10.0f) + offset);
        DirectionalShadowCascades cascades;
        if (!BuildDirectionalShadowCascades(settings, &moved, cascades)) { Expect(false, "moved view must build cascades"); break; }
        const vec4 before = a.Cameras[0].viewProjMatrix * point;
        const vec4 after = cascades.Cameras[0].viewProjMatrix * point;
        const f32 dx = std::abs(TexelFraction(after.x - before.x));
        const f32 dy = std::abs(TexelFraction(after.y - before.y));
        if (dx > 0.05f || dy > 0.05f) { Expect(false, "camera movement must keep the shadow texel grid"); break; }
    }

    // 회전해도 캐스케이드 크기는 같다.
    const CameraBuffer turned = MakeCamera(vec3(0.0f, 2.0f, 0.0f), vec3(8.0f, 4.0f, -3.0f));
    DirectionalShadowCascades rotated;
    Expect(BuildDirectionalShadowCascades(settings, &turned, rotated) && rotated.Cameras[0].projMatrix == a.Cameras[0].projMatrix, "rotation must not resize cascades");

    const mat4 view = glm::lookAtLH(vec3(5.0f), vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
    const CameraBuffer orthographic = CameraBuffer::FromMatrices(view, glm::orthoLH_ZO(-40.0f, 40.0f, -20.0f, 20.0f, 0.1f, 200.0f), vec3(5.0f), 0.0f);
    DirectionalShadowCascades ortho;
    Expect(BuildDirectionalShadowCascades(settings, &orthographic, ortho) && Covers(orthographic, ortho.Cameras[0], splits[0], splits[1], true),
        "orthographic views must be covered");

    DirectionalShadowCascades fallback;
    settings.Position = vec3(7.0f, 8.0f, 9.0f);
    Expect(BuildDirectionalShadowCascades(settings, nullptr, fallback), "a missing view must use the light position");
    const vec4 projected = fallback.Cameras[0].viewProjMatrix * vec4(settings.Position, 1.0f);
    Expect(std::abs(projected.x) < 0.01f && std::abs(projected.y) < 0.01f, "fallback cascades must be centered on the light");

    CameraBuffer broken = first;
    broken.projMatrix[0][0] = std::numeric_limits<f32>::quiet_NaN();
    Expect(!BuildDirectionalShadowCascades(settings, &broken, fallback), "invalid views must be rejected");
    settings.MaxDistance = std::numeric_limits<f32>::infinity();
    Expect(!BuildDirectionalShadowCascades(settings, &first, fallback), "invalid settings must be rejected");

    return g_Failures == 0 ? 0 : 1;
}
