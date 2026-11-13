#pragma once

#include <filesystem>
#include <string>

namespace Editor
{
    struct ClientPackageRequest;

    void ExportClientPackage(const std::filesystem::path& projectRoot, const std::filesystem::path& binDirectory,
        const ClientPackageRequest& request, std::string& output);
}
