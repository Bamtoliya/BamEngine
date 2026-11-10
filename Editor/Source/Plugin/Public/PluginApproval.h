#pragma once

#include <filesystem>
#include <string>

namespace Editor
{
    struct PluginApproval
    {
        std::string project;
        std::string id;
        std::string version;
        std::string package;
        std::string binary;
        std::string sha256;

        bool operator==(const PluginApproval&) const = default;
    };

    bool HasPluginApproval(const std::filesystem::path& path, const PluginApproval& approval);
    bool SavePluginApproval(const std::filesystem::path& path, const PluginApproval& approval);
}
