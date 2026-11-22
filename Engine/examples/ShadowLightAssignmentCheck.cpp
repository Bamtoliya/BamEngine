#include "ShadowLightAssignment.h"

#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "ShadowLightAssignmentCheck failed: %s\n", message);
        ++g_Failures;
    }

    RaytracedShadowOverrides Project(ERaytracedShadowMode mode)
    {
        RaytracedShadowOverrides overrides;
        overrides.Project = mode;
        return overrides;
    }
}

int main()
{
    Expect(AssignRaytracedShadows({}, Project(ERaytracedShadowMode::On)).Empty(), "no shadow lights must leave the mask empty");
    Expect(!RaytracedShadowAssignment{}.Contains(INVALID_SHADOW_LIGHT), "an empty channel must not match a missing light");

    // 광원 버퍼 순서: 점광원 2, 방향광 5, 스포트 6, 점광원 8, 방향광 9, 스포트 11
    ShadowLightSet set;
    set.Lights = { { 2, true }, { 5, false }, { 6, true }, { 8, true }, { 9, true }, { 11, true } };
    set.CascadeLight = 5;
    set.SpotLight = 6;
    set.PointLight = 2;

    const auto requested = AssignRaytracedShadows(set, {});
    Expect(!requested.Contains(5), "a light that did not ask for raytracing must stay on shadow maps");
    Expect(requested.Lights[0] == 8 && requested.Lights[1] == 9 && requested.Lights[2] == 11, "lights without shadow maps must come first");
    Expect(requested.Lights[3] == 2 && !requested.Contains(6), "remaining channels must follow buffer order");

    const auto forced = AssignRaytracedShadows(set, Project(ERaytracedShadowMode::On));
    Expect(forced.Lights[0] == 5, "the cascade light must use the first channel");
    Expect(forced.Lights[1] == 8 && forced.Lights[2] == 9 && forced.Lights[3] == 11, "lights without shadow maps must take the other channels");

    Expect(AssignRaytracedShadows(set, Project(ERaytracedShadowMode::Off)).Empty(), "overrides must be able to disable every light");

    ShadowLightSet single;
    single.Lights = { { 0, true } };
    single.SpotLight = 0;
    const auto spot = AssignRaytracedShadows(single, {});
    Expect(spot.Lights[0] == 0 && spot.Lights[1] == INVALID_SHADOW_LIGHT && spot.Contains(single.SpotLight), "a single spot light must use one channel");
    Expect(!spot.Contains(single.CascadeLight) && !spot.Contains(single.PointLight), "missing lights must not be traced");

    return g_Failures == 0 ? 0 : 1;
}
