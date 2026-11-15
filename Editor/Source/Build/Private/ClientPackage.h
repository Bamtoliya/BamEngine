#pragma once

#include <filesystem>
#include <functional>
#include <string_view>

namespace Editor
{
    struct ClientPackageRequest;

    void ExportClientPackage(const std::filesystem::path& projectRoot, const std::filesystem::path& binDirectory,
        const ClientPackageRequest& request, const std::function<void(std::string_view)>& onOutput);
}