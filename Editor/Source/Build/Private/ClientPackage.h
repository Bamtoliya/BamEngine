#pragma once

#include "ClientBuildJob.h"

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Editor
{
    struct ClientPackageInput
    {
        std::vector<std::string> RequiredActions;
        std::string Contents;
    };

    struct ClientPakBuildOptions
    {
        std::filesystem::path StagingDirectory;
        std::string StartupSceneUri;
        std::string StartupManifestUri;
        std::string RenderPipelineProfileUri;
        std::vector<std::string> RequiredEngineUris;
        std::vector<std::string> RequiredProjectUris;
        bool HasInputActionsConfig = false;
        bool PrunePackedLooseFiles = true;
    };

    [[nodiscard]] ClientPackageInput PrepareClientPackageInput(
        const std::filesystem::path& projectRoot, const ClientPackageRequest& request);

    [[nodiscard]] ClientPackageManifest BuildClientPakArchives(
        const ClientPakBuildOptions& options, const std::function<void(std::string_view)>& onOutput = nullptr);

    void ExportClientPackage(const std::filesystem::path& projectRoot, const std::filesystem::path& binDirectory,
        const ClientPackageRequest& request, const ClientPackageInput& input,
        const std::function<void(std::string_view)>& onOutput);
}
