#include "RaytracedShadowSettings.h"

#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "RaytracedShadowSettingsCheck failed: %s\n", message);
        ++g_Failures;
    }
}

int main()
{
    constexpr auto Inherit = ERaytracedShadowMode::Inherit;
    constexpr auto On = ERaytracedShadowMode::On;
    constexpr auto Off = ERaytracedShadowMode::Off;

    Expect(!ResolveRaytracedShadow(false, {}), "the light default must stay off");
    Expect(ResolveRaytracedShadow(true, {}), "an enabled light must be used when nothing overrides it");

    Expect(ResolveRaytracedShadow(false, { On, Inherit, Inherit }), "project must override the light");
    Expect(!ResolveRaytracedShadow(true, { Off, Inherit, Inherit }), "project must be able to disable the light");
    Expect(!ResolveRaytracedShadow(true, { On, Off, Inherit }), "graphics must override the project");
    Expect(ResolveRaytracedShadow(false, { Off, On, Inherit }), "graphics must be able to enable over the project");
    Expect(ResolveRaytracedShadow(false, { Off, Off, On }), "viewport must override graphics");
    Expect(!ResolveRaytracedShadow(true, { On, On, Off }), "viewport must be able to disable");
    Expect(ResolveRaytracedShadow(false, { On, Inherit, Inherit }) == ResolveRaytracedShadow(false, { Inherit, On, Inherit }),
        "inherit must pass the previous result through");

    for (int value = 0; value <= 2; ++value)
    {
        Expect(IsValidRaytracedShadowMode(value), "stored modes 0-2 must be valid");
        Expect(static_cast<int>(ToRaytracedShadowMode(value)) == value, "stored modes must round-trip");
    }
    Expect(!IsValidRaytracedShadowMode(-1) && !IsValidRaytracedShadowMode(3), "out-of-range modes must be rejected");
    Expect(ToRaytracedShadowMode(-1) == Inherit && ToRaytracedShadowMode(99) == Inherit, "unknown modes must inherit");

    return g_Failures == 0 ? 0 : 1;
}
