#include "RenderPipelineProfile.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace
{
    bool Expect(bool condition, std::string_view description)
    {
        if (!condition)
            std::cerr << description << '\n';
        return condition;
    }

    bool ExpectInvalid(const Engine::RenderPipelineProfile& profile, std::string_view field)
    {
        const auto result = Engine::ValidateRenderPipelineProfile(profile);
        if (!result && result.Field == field)
            return true;

        std::cerr << "Expected an error at " << field << ", got " << result.Field << ": " << result.Message << '\n';
        return false;
    }
}

int main(int argc, char** argv)
{
    using namespace Engine;

    if (argc != 2)
    {
        std::cerr << "Usage: RenderPipelineProfileCheck <default profile path>\n";
        return 1;
    }

    RenderPipelineProfile profile;
    const auto loaded = LoadRenderPipelineProfile(argv[1], profile);
    if (!loaded)
    {
        std::cerr << loaded.Path << ": " << loaded.Field << ": " << loaded.Message << '\n';
        return 1;
    }

    if (!Expect(profile.Passes.size() == 7 && profile.Output == "PostProcess.Color", "Unexpected default profile layout."))
        return 1;

    const auto roundTripPath = std::filesystem::path("round-trip.bamrenderpipeline.json");
    RenderPipelineProfile roundTrip;
    if (!Expect(static_cast<bool>(SaveRenderPipelineProfile(roundTripPath, profile)), "Could not save profile.") ||
        !Expect(static_cast<bool>(LoadRenderPipelineProfile(roundTripPath, roundTrip)), "Could not reload profile.") ||
        !Expect(roundTrip == profile, "The round trip changed the profile."))
        return 1;

    auto changed = profile;
    changed.Version++;
    if (!ExpectInvalid(changed, "Version"))
        return 1;

    changed = profile;
    changed.Passes[1].Id = changed.Passes[0].Id;
    if (!ExpectInvalid(changed, "Passes[1].Id"))
        return 1;

    changed = profile;
    changed.Passes[0].Resolution = 0;
    if (!ExpectInvalid(changed, "Passes[0].Settings.Resolution"))
        return 1;

    changed = profile;
    changed.Passes[3].Type = "Unregistered";
    if (!ExpectInvalid(changed, "Passes[3].Type"))
        return 1;

    changed = profile;
    changed.Passes[3].Inputs.erase("GBuffer");
    if (!ExpectInvalid(changed, "Passes[3].Inputs.GBuffer"))
        return 1;

    changed.Passes[3].Inputs["GBuffer"] = "Geometry.Depth";
    if (!ExpectInvalid(changed, "Passes[3].Inputs.GBuffer"))
        return 1;

    changed = profile;
    changed.Passes[3].Inputs["PointShadow"] = "Shadow.Depth";
    if (!ExpectInvalid(changed, "Passes[3].Inputs.PointShadow"))
        return 1;

    changed = profile;
    changed.Passes[4].Inputs["Color"] = "Transparent.Color";
    if (!ExpectInvalid(changed, "Passes[4].Inputs.Color"))
        return 1;

    changed = profile;
    changed.Passes[5].Inputs["Color"] = "Lighting.HdrColor";
    if (!ExpectInvalid(changed, "Passes[5].Inputs.Color"))
        return 1;

    changed = profile;
    changed.Output = "Lighting.HdrColor";
    if (!ExpectInvalid(changed, "Output"))
        return 1;

    changed = profile;
    changed.Output = "Transparent.Depth";
    if (!ExpectInvalid(changed, "Output"))
        return 1;

    changed = profile;
    changed.Passes.push_back(changed.Passes[5]);
    changed.Passes.back().Id = "Overlay";
    changed.Passes.back().Inputs = { { "Color", "Transparent.Color" }, { "Depth", "Transparent.Depth" } };
    if (!ExpectInvalid(changed, "Output"))
        return 1; // Scene post-process can bypass to an input that Overlay overwrites.

    const auto invalidPath = std::filesystem::path("invalid.bamrenderpipeline.json");
    {
        std::ofstream invalid(invalidPath, std::ios::binary | std::ios::trunc);
        invalid << R"({"Version":1,"Passes":[{"Id":"Shadow","Type":"DirectionalShadow","Settings":{"Unknown":1}}],"Output":"Shadow.Depth"})";
        invalid.close();
        if (!Expect(static_cast<bool>(invalid), "Could not write invalid profile fixture."))
            return 1;
    }

    const auto badLoad = LoadRenderPipelineProfile(invalidPath, roundTrip);
    if (!Expect(!badLoad && badLoad.Path == invalidPath && badLoad.Field == "Passes[0].Settings.Unknown",
            "Unknown settings field was not reported.") ||
        !Expect(roundTrip == profile, "A failed load changed the previous profile."))
        return 1;

    const auto badSave = SaveRenderPipelineProfile(roundTripPath, changed);
    if (!Expect(!badSave && badSave.Field == "Output", "An invalid profile was saved.") ||
        !Expect(static_cast<bool>(LoadRenderPipelineProfile(roundTripPath, roundTrip)) && roundTrip == profile,
            "A failed save changed the previous profile file."))
        return 1;

    std::cout << "Render pipeline profile checks passed.\n";
    return 0;
}
