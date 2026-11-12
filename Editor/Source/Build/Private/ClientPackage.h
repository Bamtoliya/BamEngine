#pragma once

#include <filesystem>
#include <string>

namespace Editor
{
    struct ClientPackageRequest;

    std::filesystem::path ResolveClientPackageDestination(const ClientPackageRequest& request);
    void ExportClientPackage(const std::filesystem::path& projectRoot, const std::filesystem::path& binDirectory,
        const std::filesystem::path& startupScene, const std::filesystem::path& destination, std::string& output);
}
