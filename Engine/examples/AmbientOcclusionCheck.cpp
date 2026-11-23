#include "AmbientOcclusion.h"
#include "TemporalAA.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "AmbientOcclusionCheck failed: %s\n", message);
        ++g_Failures;
    }

    bool Near(f32 a, f32 b, f32 tolerance = 1e-4f)
    {
        return std::abs(a - b) <= tolerance;
    }

    vec2 ProjectToNDC(const mat4& viewProjection, const vec3& position)
    {
        const vec4 clip = viewProjection * vec4(position, 1.0f);
        return vec2(clip) / clip.w;
    }

    vec2 NDCToUV(const vec2& ndc)
    {
        return vec2(ndc.x * 0.5f + 0.5f, ndc.y * -0.5f + 0.5f);
    }

    void CheckSettings()
    {
        constexpr auto Inherit = EAmbientOcclusionMode::Inherit;
        constexpr auto Off = EAmbientOcclusionMode::Off;
        constexpr auto Screen = EAmbientOcclusionMode::ScreenSpace;
        constexpr auto Raytraced = EAmbientOcclusionMode::Raytraced;

        Expect(ResolveAmbientOcclusion({}) == Off, "the default must stay off");
        Expect(ResolveAmbientOcclusion({ Screen, Inherit, Inherit }) == Screen, "project must be used when nothing overrides it");
        Expect(ResolveAmbientOcclusion({ Screen, Raytraced, Inherit }) == Raytraced, "graphics must override the project");
        Expect(ResolveAmbientOcclusion({ Raytraced, Screen, Off }) == Off, "viewport must be able to disable");
        Expect(ResolveAmbientOcclusion({ Off, Inherit, Screen }) == Screen, "viewport must be able to enable over the project");
        Expect(ResolveAmbientOcclusion({ Raytraced, Inherit, Inherit }) == ResolveAmbientOcclusion({ Inherit, Raytraced, Inherit }),
            "inherit must pass the previous result through");

        for (int value = 0; value <= 3; ++value)
        {
            Expect(IsValidAmbientOcclusionMode(value), "stored modes 0-3 must be valid");
            Expect(static_cast<int>(ToAmbientOcclusionMode(value)) == value, "stored modes must round-trip");
        }
        Expect(!IsValidAmbientOcclusionMode(-1) && !IsValidAmbientOcclusionMode(4), "out-of-range modes must be rejected");
        Expect(ToAmbientOcclusionMode(-1) == Inherit && ToAmbientOcclusionMode(99) == Inherit, "unknown modes must inherit");

        // 레이트레이싱을 쓸 수 없으면 화면 공간으로, 화면 공간도 쓸 수 없으면 끈다.
        Expect(SelectAmbientOcclusionPath(Raytraced, true, true) == Raytraced, "available raytracing must be used");
        Expect(SelectAmbientOcclusionPath(Raytraced, true, false) == Screen, "unavailable raytracing must fall back to screen space");
        Expect(SelectAmbientOcclusionPath(Raytraced, false, true) == Off, "missing screen-space shaders must disable occlusion");
        Expect(SelectAmbientOcclusionPath(Screen, true, true) == Screen, "screen space must not switch to raytracing");
        Expect(SelectAmbientOcclusionPath(Off, true, true) == Off && SelectAmbientOcclusionPath(Inherit, true, true) == Off,
            "off and unresolved modes must stay off");
    }

    // 화면 축으로 옮긴 점은 NDC x·y가 늘어나야 한다(LH·RH, y를 뒤집은 투영 모두).
    void CheckScreenAxes(const mat4& view, const mat4& projection, const char* message)
    {
        const vec3 target(0.0f, 0.0f, 0.0f);
        const ScreenAxes axes = GetScreenAxes(view, projection);
        const mat4 viewProjection = projection * view;
        const vec2 center = ProjectToNDC(viewProjection, target);
        const vec2 right = ProjectToNDC(viewProjection, target + axes.Right * 0.1f);
        const vec2 up = ProjectToNDC(viewProjection, target + axes.Up * 0.1f);
        Expect(right.x > center.x && Near(right.y, center.y), message);
        Expect(up.y > center.y && Near(up.x, center.x), message);
        Expect(Near(glm::length(axes.Right), 1.0f) && Near(glm::length(axes.Up), 1.0f) && Near(glm::dot(axes.Right, axes.Up), 0.0f),
            "screen axes must be orthonormal");
    }

    void CheckCameraMath()
    {
        const vec3 eye(3.0f, 2.0f, -5.0f);
        const vec3 target(0.0f, 0.0f, 0.0f);
        const vec3 worldUp(0.0f, 1.0f, 0.0f);
        const f32 fov = glm::radians(60.0f);
        const mat4 viewLH = glm::lookAtLH(eye, target, worldUp);
        const mat4 projectionLH = glm::perspectiveLH_ZO(fov, 16.0f / 9.0f, 0.1f, 1000.0f);
        const mat4 viewRH = glm::lookAtRH(eye, target, worldUp);
        const mat4 projectionRH = glm::perspectiveRH_ZO(fov, 16.0f / 9.0f, 0.1f, 1000.0f);
        mat4 flippedRH = projectionRH;
        flippedRH[1][1] = -flippedRH[1][1];

        CheckScreenAxes(viewLH, projectionLH, "left-handed screen axes must follow NDC");
        CheckScreenAxes(viewRH, projectionRH, "right-handed screen axes must follow NDC");
        CheckScreenAxes(viewRH, flippedRH, "a flipped projection must flip the up axis");

        // 화면 중앙에서 거리 d에 놓인 1m는 PixelsPerMeter / d 픽셀이다.
        constexpr uint32 height = 1080u;
        const f32 distance = glm::length(target - eye);
        const ScreenAxes axes = GetScreenAxes(viewLH, projectionLH);
        const mat4 viewProjection = projectionLH * viewLH;
        const f32 pixels = (ProjectToNDC(viewProjection, target + axes.Up).y - ProjectToNDC(viewProjection, target).y) * 0.5f * static_cast<f32>(height);
        Expect(Near(pixels, PixelsPerMeterAtUnitDistance(projectionLH, height) / distance, 0.5f), "projected radius must match the screen size");

        Expect(SupportsScreenSpaceAmbientOcclusion(projectionLH), "perspective cameras must be supported");
        Expect(!SupportsScreenSpaceAmbientOcclusion(glm::orthoLH_ZO(-10.0f, 10.0f, -5.0f, 5.0f, 0.1f, 100.0f)), "orthographic cameras must be skipped");

        // 지터 카메라로 그린 위치는 원래 위치에서 NDCOffsetToUV만큼 옮겨진다(픽셀로는 x 오른쪽·y 아래).
        constexpr uint32 width = 1920u;
        const CameraBuffer camera = CameraBuffer::FromMatrices(viewLH, projectionLH, eye, 0.0f);
        const vec2 jitterPixels = TemporalJitterPixels(3u);
        const CameraBuffer jittered = MakeJitteredCamera(camera, jitterPixels, width, height);
        const vec3 point(0.4f, -0.3f, 1.2f);
        const vec2 shift = NDCToUV(ProjectToNDC(jittered.viewProjMatrix, point)) - NDCToUV(ProjectToNDC(camera.viewProjMatrix, point));
        const vec2 expected = NDCOffsetToUV(TemporalJitterNDC(jitterPixels, width, height));
        Expect(Near(shift.x, expected.x, 1e-5f) && Near(shift.y, expected.y, 1e-5f), "jitter must move texture coordinates by NDCOffsetToUV");
        Expect(Near(expected.x * width, jitterPixels.x, 1e-3f) && Near(expected.y * height, jitterPixels.y, 1e-3f),
            "the jitter shift in pixels must equal the jitter");
    }
}

int main()
{
    CheckSettings();
    CheckCameraMath();
    return g_Failures == 0 ? 0 : 1;
}
