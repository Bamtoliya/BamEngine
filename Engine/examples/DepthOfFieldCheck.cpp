#include "DepthOfField.h"
#include "PostProcessOrder.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "DepthOfFieldCheck failed: %s\n", message);
        ++g_Failures;
    }

    bool Near(f32 a, f32 b, f32 tolerance = 0.001f)
    {
        return std::abs(a - b) <= tolerance;
    }

    // 하늘에 쓰는 거리(depthOfField.hlsli의 DOF_SKY_DEPTH)
    constexpr f32 SkyDepth = 1.0e9f;

    void CheckFocusRange()
    {
        DepthOfFieldSettings settings;
        settings.Mode = EDepthOfFieldMode::FocusRange;
        settings.FocusDistance = 10.0f;
        settings.FocusRange = 4.0f;
        settings.TransitionRange = 6.0f;
        settings.MaxBlurRadius = 12.0f;
        const auto params = MakeDepthOfFieldCoCParams(settings, glm::perspectiveLH_ZO(1.0f, 1.0f, 0.1f, 100.0f), 1080);

        Expect(ComputeDepthOfFieldCoC(params, 10.0f) == 0.0f, "the focus plane must be sharp");
        Expect(ComputeDepthOfFieldCoC(params, 8.0f) == 0.0f && ComputeDepthOfFieldCoC(params, 12.0f) == 0.0f, "the focus range must stay sharp");
        Expect(Near(ComputeDepthOfFieldCoC(params, 15.0f), 6.0f), "far blur must grow linearly through the transition");
        Expect(Near(ComputeDepthOfFieldCoC(params, 5.0f), -6.0f), "near blur must be negative");
        Expect(Near(ComputeDepthOfFieldCoC(params, 18.0f), 12.0f) && Near(ComputeDepthOfFieldCoC(params, 500.0f), 12.0f), "far blur must stop at the maximum radius");
        Expect(Near(ComputeDepthOfFieldCoC(params, 0.0f), -12.0f), "a surface at the camera must get the full near blur");
        Expect(Near(ComputeDepthOfFieldCoC(params, SkyDepth), 12.0f), "the sky must get the full far blur");

        f32 previous = ComputeDepthOfFieldCoC(params, 0.1f);
        for (f32 z = 0.2f; z < 40.0f; z += 0.1f)
        {
            const f32 coc = ComputeDepthOfFieldCoC(params, z);
            Expect(coc >= previous - 0.0001f, "CoC must not decrease with distance");
            previous = coc;
        }

        settings.MaxBlurRadius = 100.0f;
        Expect(MakeDepthOfFieldCoCParams(settings, mat4(1.0f), 1080).MaxRadius == DepthOfFieldMaxBlurRadiusLimit, "the radius must stay within the shader limit");
    }

    void CheckPhysicalLens()
    {
        // 센서 높이 24mm에서 초점 거리 50mm가 되는 세로 시야각
        const f32 fovY = 2.0f * std::atan(12.0f / 50.0f);
        const mat4 projection = glm::perspectiveLH_ZO(fovY, 16.0f / 9.0f, 0.1f, 1000.0f);
        Expect(Near(ComputeLensFocalLength(projection, 24.0f), 50.0f, 0.01f), "focal length must follow the vertical field of view");
        Expect(Near(ComputeLensFocalLength(glm::perspectiveRH_ZO(fovY, 1.0f, 0.1f, 1000.0f), 24.0f), 50.0f, 0.01f), "right-handed projections must give the same focal length");

        DepthOfFieldSettings settings;
        settings.Mode = EDepthOfFieldMode::PhysicalLens;
        settings.FocusDistance = 5.0f;
        settings.FStop = 2.8f;
        settings.SensorHeight = 24.0f;
        settings.MaxBlurRadius = 32.0f;
        const auto params = MakeDepthOfFieldCoCParams(settings, projection, 1080);

        // 반지름 = 0.5 × f² / (N (S - f)) / 센서 높이 × 화면 높이
        const f32 expected = 0.5f * 2500.0f / (2.8f * 4950.0f) / 24.0f * 1080.0f;
        Expect(Near(params.LensScale, expected, 0.01f), "lens scale must follow the thin lens formula");
        Expect(ComputeDepthOfFieldCoC(params, 5.0f) == 0.0f, "the focus plane must be sharp");
        Expect(Near(ComputeDepthOfFieldCoC(params, SkyDepth), expected, 0.01f), "the sky must get the blur at infinity");
        Expect(Near(ComputeDepthOfFieldCoC(params, 10.0f), expected * 0.5f, 0.01f), "twice the focus distance must get half the blur at infinity");
        Expect(Near(ComputeDepthOfFieldCoC(params, 2.5f), -expected, 0.01f), "half the focus distance must blur as much as infinity, in front");

        settings.FStop = 5.6f;
        Expect(Near(MakeDepthOfFieldCoCParams(settings, projection, 1080).LensScale, expected * 0.5f, 0.01f), "doubling the f-number must halve the blur");
        settings.FStop = 2.8f;
        Expect(Near(MakeDepthOfFieldCoCParams(settings, projection, 2160).LensScale, expected * 2.0f, 0.01f), "blur in pixels must scale with the viewport height");

        // 초점 거리보다 가까운 초점면은 공식이 성립하지 않는다. 값이 유한해야 한다.
        settings.FocusDistance = 0.01f;
        Expect(std::isfinite(MakeDepthOfFieldCoCParams(settings, projection, 1080).LensScale), "focusing inside the focal length must stay finite");

        const mat4 orthographic = glm::orthoLH_ZO(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
        Expect(!IsPerspectiveProjection(orthographic), "orthographic projections must be detected");
        settings.FocusDistance = 5.0f;
        Expect(MakeDepthOfFieldCoCParams(settings, orthographic, 1080).LensScale == 0.0f, "orthographic cameras must not use the lens formula");
    }

    void CheckCameraForward()
    {
        const vec3 eye(1.0f, 2.0f, -5.0f);
        const vec3 target(3.0f, 0.0f, 4.0f);
        const vec3 expected = glm::normalize(target - eye);
        const vec3 point(-2.0f, 1.0f, 30.0f);

        const mat4 leftView = glm::lookAtLH(eye, target, vec3(0.0f, 1.0f, 0.0f));
        const auto left = CameraBuffer::FromMatrices(leftView, glm::perspectiveLH_ZO(1.0f, 1.0f, 0.1f, 100.0f), eye, 0.0f);
        Expect(glm::distance(GetCameraForward(left), expected) < 0.001f, "left-handed forward must point at the target");
        Expect(Near(glm::dot(point - eye, GetCameraForward(left)), (leftView * vec4(point, 1.0f)).z), "left-handed depth must match view space");

        const mat4 rightView = glm::lookAtRH(eye, target, vec3(0.0f, 1.0f, 0.0f));
        const auto right = CameraBuffer::FromMatrices(rightView, glm::perspectiveRH_ZO(1.0f, 1.0f, 0.1f, 100.0f), eye, 0.0f);
        Expect(glm::distance(GetCameraForward(right), expected) < 0.001f, "right-handed forward must point at the target");
        Expect(Near(glm::dot(point - eye, GetCameraForward(right)), -(rightView * vec4(point, 1.0f)).z), "right-handed depth must match view space");

        const auto orthographic = CameraBuffer::FromMatrices(leftView, glm::orthoLH_ZO(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f), eye, 0.0f);
        Expect(glm::distance(GetCameraForward(orthographic), expected) < 0.001f, "orthographic forward must point at the target");
    }

    void CheckEffectOrder()
    {
        using Order = std::vector<const char*>;
        const char* dof = "DepthOfField";
        const char* bloom = "Bloom";
        const char* tone = "ToneMapping";
        const char* vignette = "Vignette";
        const char* sharpen = "Sharpen";
        const Order current = { dof, bloom, tone, vignette, sharpen };

        Expect(MergeEffectOrder(current, Order{ bloom, tone, vignette, sharpen }) == Order{ dof, bloom, tone, vignette, sharpen },
            "an effect missing from an old preset must keep its default place");
        Expect(MergeEffectOrder(current, Order{ tone, vignette }) == Order{ dof, bloom, tone, vignette, sharpen },
            "consecutive missing effects must keep their relative order");
        // 사용자가 바꾼 순서는 유지하고, 빠진 효과는 현재 순서에서 뒤따르는 효과(톤매핑) 앞에 둔다.
        Expect(MergeEffectOrder(current, Order{ vignette, tone }) == Order{ vignette, dof, bloom, tone, sharpen },
            "the order read from the file must be kept");
        Expect(MergeEffectOrder(current, Order{}) == current, "an empty file must give the current order");
        Expect(MergeEffectOrder(current, current) == current, "a complete file must not change");

        for (uint32 slot = 0; slot + 1 < 16u; ++slot)
        {
            Expect(PostProcessPrePassPriority(slot) < PostProcessSlotPriority(slot), "pre-passes must run before their slot");
            Expect(PostProcessSlotPriority(slot) < PostProcessPrePassPriority(slot + 1), "pre-passes must run after the previous slot");
        }
        Expect(PostProcessSlotPriority(15u) < 200u, "post-processing must finish before the UI pass");
    }
}

int main()
{
    CheckFocusRange();
    CheckPhysicalLens();
    CheckCameraForward();
    CheckEffectOrder();

    if (g_Failures == 0)
        std::printf("DepthOfFieldCheck passed\n");
    return g_Failures == 0 ? 0 : 1;
}
