#pragma once

#include <filesystem>
#include <string>

namespace Editor
{
    struct PanelPluginOptions
    {
        std::string Identifier;
        std::string DisplayName;
        std::string Version;
        int Width = 640;
        int Height = 480;
    };

    enum class EPanelPluginCreateResult
    {
        Success,
        InvalidIdentifier,
        InvalidName,
        InvalidVersion,
        InvalidSize,
        UnsupportedEnvironment,
        DirectoryExists,
        FileSystemError
    };

    EPanelPluginCreateResult CreatePanelPlugin(const std::filesystem::path& projectRoot,
        const PanelPluginOptions& options, std::filesystem::path& createdDirectory);
}
