#pragma once

#include "Engine_API.h"
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace Engine
{
    struct RenderPipelinePassProfile
    {
        std::string Id;
        std::string Type;
        std::map<std::string, std::string, std::less<>> Inputs;
        std::optional<std::uint32_t> Resolution;

        bool operator==(const RenderPipelinePassProfile&) const = default;
    };

    struct RenderPipelineProfile
    {
        static constexpr std::uint32_t CurrentVersion = 1;

        std::uint32_t Version = CurrentVersion;
        std::vector<RenderPipelinePassProfile> Passes;
        std::string Output;

        bool operator==(const RenderPipelineProfile&) const = default;
    };

    struct RenderPipelineProfileResult
    {
        std::filesystem::path Path;
        std::string Field;
        std::string Message;

        explicit operator bool() const noexcept { return Message.empty(); }
    };

    [[nodiscard]] ENGINE_API RenderPipelineProfileResult ValidateRenderPipelineProfile(const RenderPipelineProfile& profile);
    [[nodiscard]] ENGINE_API RenderPipelineProfileResult LoadRenderPipelineProfile(
        const std::filesystem::path& path, RenderPipelineProfile& profile);
    [[nodiscard]] ENGINE_API RenderPipelineProfileResult SaveRenderPipelineProfile(
        const std::filesystem::path& path, const RenderPipelineProfile& profile);
}
