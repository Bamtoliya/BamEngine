#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Editor
{
    struct ClientPackageRequest;

    struct ClientPackageInput
    {
        std::vector<std::string> RequiredActions;
        std::string Contents;
    };

    ClientPackageInput PrepareClientPackageInput(const std::filesystem::path& projectRoot, const ClientPackageRequest& request);

    void ExportClientPackage(const std::filesystem::path& projectRoot, const std::filesystem::path& binDirectory,
        const ClientPackageRequest& request, const ClientPackageInput& input,
        const std::function<void(std::string_view)>& onOutput);
}
