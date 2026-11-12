#include "RenderPipelineProfile.h"

#include <algorithm>
#include <array>
#include <set>
#include <span>
#include <string_view>
#include <utility>

namespace Engine
{
    namespace
    {
        enum class ResourceKind
        {
            DirectionalShadow,
            PointShadow,
            GBuffer,
            SceneDepth,
            HdrColor,
            PostProcessColor
        };

        struct InputSlot
        {
            std::string_view Name;
            ResourceKind Kind;
        };

        struct OutputSlot
        {
            std::string_view Name;
            ResourceKind Kind;
            std::string_view AliasedInput{};
            bool OverwritesInput = false;
        };

        struct PassContract
        {
            std::string_view Type;
            std::span<const InputSlot> Inputs;
            std::span<const OutputSlot> Outputs;
            bool RequiresResolution = false;
        };

        constexpr std::array DirectionalShadowOutputs{ OutputSlot{ "Depth", ResourceKind::DirectionalShadow } };
        constexpr std::array PointShadowOutputs{ OutputSlot{ "Depth", ResourceKind::PointShadow } };
        constexpr std::array GBufferOutputs{
            OutputSlot{ "GBuffer", ResourceKind::GBuffer }, OutputSlot{ "Depth", ResourceKind::SceneDepth }
        };
        constexpr std::array LightingInputs{
            InputSlot{ "GBuffer", ResourceKind::GBuffer }, InputSlot{ "DirectionalShadow", ResourceKind::DirectionalShadow },
            InputSlot{ "PointShadow", ResourceKind::PointShadow }
        };
        constexpr std::array LightingOutputs{ OutputSlot{ "HdrColor", ResourceKind::HdrColor } };
        constexpr std::array ForwardInputs{
            InputSlot{ "Color", ResourceKind::HdrColor }, InputSlot{ "Depth", ResourceKind::SceneDepth }
        };
        constexpr std::array ForwardOutputs{
            OutputSlot{ "Color", ResourceKind::HdrColor, "Color", true },
            OutputSlot{ "Depth", ResourceKind::SceneDepth, "Depth", true }
        };
        constexpr std::array PostProcessInputs{ InputSlot{ "Color", ResourceKind::HdrColor } };
        constexpr std::array PostProcessOutputs{ OutputSlot{ "Color", ResourceKind::PostProcessColor, "Color" } };
        constexpr std::array PassContracts{
            PassContract{ "DirectionalShadow", {}, DirectionalShadowOutputs, true },
            PassContract{ "PointShadow", {}, PointShadowOutputs, true },
            PassContract{ "GBuffer", {}, GBufferOutputs },
            PassContract{ "DeferredLighting", LightingInputs, LightingOutputs },
            PassContract{ "ForwardOpaque", ForwardInputs, ForwardOutputs },
            PassContract{ "ForwardTransparent", ForwardInputs, ForwardOutputs },
            PassContract{ "ScenePostProcess", PostProcessInputs, PostProcessOutputs }
        };

        struct ProfileOutput
        {
            ResourceKind Kind;
            std::size_t Version;
        };

        RenderPipelineProfileResult InvalidProfileField(std::string field, std::string message)
        {
            return { {}, std::move(field), std::move(message) };
        }
    }

    RenderPipelineProfileResult ValidateRenderPipelineProfile(const RenderPipelineProfile& profile)
    {
        if (profile.Version != RenderPipelineProfile::CurrentVersion)
            return InvalidProfileField("Version", "Unsupported render pipeline profile version.");
        if (profile.Passes.empty())
            return InvalidProfileField("Passes", "At least one pass is required.");

        std::set<std::string_view> passIds;
        std::map<std::string, ProfileOutput, std::less<>> outputs;
        std::vector<bool> availableVersions;

        for (std::size_t index = 0; index < profile.Passes.size(); ++index)
        {
            const auto& pass = profile.Passes[index];
            const auto field = "Passes[" + std::to_string(index) + "]";
            const bool hasInvalidIdCharacter = std::ranges::any_of(pass.Id,
                [](unsigned char ch) { return ch <= 0x20 || ch == 0x7f || ch == '.'; });
            if (pass.Id.empty() || hasInvalidIdCharacter)
                return InvalidProfileField(field + ".Id", "A pass ID must be nonempty and contain no dots or whitespace.");
            if (!passIds.insert(pass.Id).second)
                return InvalidProfileField(field + ".Id", "Duplicate pass ID: " + pass.Id);

            const auto contract = std::ranges::find(PassContracts, std::string_view(pass.Type), &PassContract::Type);
            if (contract == PassContracts.end())
                return InvalidProfileField(field + ".Type", "Unknown pass type: " + pass.Type);
            if (contract->RequiresResolution && pass.Resolution.value_or(0) == 0)
                return InvalidProfileField(field + ".Settings.Resolution", "A positive shadow resolution is required.");
            if (!contract->RequiresResolution && pass.Resolution)
                return InvalidProfileField(field + ".Settings.Resolution", "Resolution is supported only on shadow passes.");

            for (const auto& [name, reference] : pass.Inputs)
            {
                if (std::ranges::find(contract->Inputs, std::string_view(name), &InputSlot::Name) == contract->Inputs.end())
                    return InvalidProfileField(field + ".Inputs." + name, "Unknown input slot for " + pass.Type + ".");
            }

            std::map<std::string_view, ProfileOutput> resolvedInputs;
            for (const auto& input : contract->Inputs)
            {
                const auto inputField = field + ".Inputs." + std::string(input.Name);
                const auto binding = pass.Inputs.find(input.Name);
                if (binding == pass.Inputs.end())
                    return InvalidProfileField(inputField, "Required input is missing.");

                const auto source = outputs.find(binding->second);
                if (source == outputs.end())
                    return InvalidProfileField(inputField, "Expected an output of an earlier pass: " + binding->second);
                if (!availableVersions[source->second.Version])
                    return InvalidProfileField(inputField, "The referenced output was overwritten: " + binding->second);
                if (source->second.Kind != input.Kind)
                    return InvalidProfileField(inputField, "Incompatible output type: " + binding->second);
                resolvedInputs.emplace(input.Name, source->second);
            }

            for (const auto& output : contract->Outputs)
            {
                std::size_t version = availableVersions.size();
                if (!output.AliasedInput.empty())
                {
                    const auto sourceVersion = resolvedInputs.at(output.AliasedInput).Version;
                    if (output.OverwritesInput)
                        availableVersions[sourceVersion] = false;
                    else
                        version = sourceVersion; // An inactive post-process chain can return its input target.
                }

                if (version == availableVersions.size())
                    availableVersions.push_back(true);
                outputs.emplace(pass.Id + "." + std::string(output.Name), ProfileOutput{ output.Kind, version });
            }
        }

        const auto output = outputs.find(profile.Output);
        if (output == outputs.end() || !availableVersions[output->second.Version])
            return InvalidProfileField("Output", "The final output must reference an available pass output: " + profile.Output);
        if (output->second.Kind != ResourceKind::HdrColor && output->second.Kind != ResourceKind::PostProcessColor)
            return InvalidProfileField("Output", "The final output must be a scene color texture.");
        return {};
    }
}
