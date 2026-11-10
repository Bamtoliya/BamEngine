#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace Editor
{
    struct PluginDevelopmentEntry
    {
        std::string Target;
        std::filesystem::path SourceDirectory; // 프로젝트 루트 기준 상대 경로
    };

    struct PluginManifestModule
    {
        std::string id;
        std::string environment;
        std::uint32_t apiVersion = 0;
        std::string sdkCompatibility;
        std::string binary;
    };

    struct PluginManifest
    {
        std::uint32_t manifestVersion = 0;
        std::string id;
        std::string name;
        std::string version;
        std::vector<PluginManifestModule> modules;
        std::string contentRoot;
    };

    enum class EPluginManifestResult
    {
        Success,
        ReadFailed,
        InvalidJson,
        UnsupportedVersion,
        InvalidMetadata
    };

    std::vector<PluginDevelopmentEntry> ReadPluginDevelopmentList(const std::filesystem::path& path);
    EPluginManifestResult ReadPluginManifest(const std::filesystem::path& path, PluginManifest& manifest);
}