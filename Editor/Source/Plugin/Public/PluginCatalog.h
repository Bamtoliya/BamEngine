#pragma once

#include "PluginDiscovery.h"

#include <optional>
#include <span>
#include <string_view>

namespace Editor
{
    enum class EPluginOrigin
    {
        Development,
        Builtin,
        Custom
    };

    enum class EPluginCatalogStatus
    {
        Ready,
        NeedsApproval,
        InvalidManifest,
        UnsupportedEditorModules,
        IncompatibleSdk,
        MissingBinary,
        UnsafeBinaryPath,
        DuplicatePackageId
    };

    struct PluginPackageLocation
    {
        std::filesystem::path Directory;
        bool IsBuiltin = false; // 호출자가 설치본 목록을 근거로 지정
    };

    struct PluginCatalogEntry
    {
        EPluginOrigin Origin = EPluginOrigin::Development;
        EPluginCatalogStatus Status = EPluginCatalogStatus::MissingBinary;
        std::string Id;
        std::string DisplayName;
        std::string Version;
        std::filesystem::path PackageDirectory;
        std::filesystem::path BinaryPath;
        std::optional<PluginDevelopmentEntry> Development;
        EPluginManifestResult ManifestResult = EPluginManifestResult::Success;
    };
    std::filesystem::path ResolvePluginSourceDirectory(const std::filesystem::path& projectRoot,
        const std::filesystem::path& relativeSource);
    std::vector<PluginPackageLocation> DiscoverPluginPackages(const std::filesystem::path& executableDirectory,
        const std::filesystem::path& projectRoot);
    EPluginCatalogStatus ResolveDevelopmentPluginBinary(const std::filesystem::path& executableDirectory,
        const PluginDevelopmentEntry& entry, std::filesystem::path& binary);
    std::vector<PluginCatalogEntry> BuildPluginCatalog(const std::filesystem::path& executableDirectory,
        std::span<const PluginPackageLocation> packages, std::string_view sdkCompatibility);
}