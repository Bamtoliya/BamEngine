#include "PluginCatalog.h"
#include "Functions.h"

#include <algorithm>
#include <system_error>
#include <unordered_map>
#include <utility>

namespace Editor
{
    namespace
    {
        using CatalogStatus = EPluginCatalogStatus;
        namespace fs = std::filesystem;

        bool IsInsideDirectory(const fs::path& file, const fs::path& directory)
        {
            // 구성 요소의 문자열 접두사 대신 실제 상위 디렉터리의 동일성을 비교한다.
            for (auto parent = file.parent_path(); !parent.empty(); parent = parent.parent_path())
            {
                std::error_code error;
                if (fs::equivalent(parent, directory, error))
                    return true;
                if (error || parent == parent.root_path())
                    break;
            }

            return false;
        }

        CatalogStatus ResolvePluginFile(const fs::path& directory, const fs::path& relative, fs::path& binary)
        {
            const fs::path parentComponent("..");
            if (relative.empty() || relative.has_root_path() ||
                std::find(relative.begin(), relative.end(), parentComponent) != relative.end())
            {
                return CatalogStatus::UnsafeBinaryPath;
            }

            std::error_code error;
            const auto root = fs::canonical(directory, error);
            if (error)
                return CatalogStatus::UnsafeBinaryPath;

            const auto candidate = fs::canonical(root / relative, error);
            if (error == std::errc::no_such_file_or_directory)
                return CatalogStatus::MissingBinary;
            if (error || !IsInsideDirectory(candidate, root) || !fs::is_regular_file(candidate, error) || error)
                return CatalogStatus::UnsafeBinaryPath;

            binary = candidate;
            return CatalogStatus::Ready;
        }

        CatalogStatus InspectPackageModule(const PluginManifest& manifest, std::string_view sdkCompatibility,
            const fs::path& directory, fs::path& binary)
        {
            const PluginManifestModule* editorModule = nullptr;
            for (const auto& module : manifest.modules)
            {
                if (module.environment != "Editor")
                    continue;
                if (editorModule)
                    return CatalogStatus::UnsupportedEditorModules;

                editorModule = &module;
            }

            if (!editorModule)
                return CatalogStatus::UnsupportedEditorModules;
            if (sdkCompatibility.empty() || editorModule->apiVersion != 1 ||
                editorModule->sdkCompatibility != sdkCompatibility)
            {
                return CatalogStatus::IncompatibleSdk;
            }

            try
            {
                return ResolvePluginFile(directory, Engine::Utf8ToPath(editorModule->binary), binary);
            }
            catch (const std::system_error&)
            {
                return CatalogStatus::UnsafeBinaryPath;
            }
        }

        PluginCatalogEntry InspectPackage(const PluginPackageLocation& location,
            std::string_view sdkCompatibility)
        {
            PluginCatalogEntry entry;
            entry.Origin = location.IsBuiltin ? EPluginOrigin::Builtin : EPluginOrigin::Custom;
            entry.PackageDirectory = location.Directory;

            PluginManifest manifest;
            std::filesystem::path manifestPath;
            if (ResolvePluginFile(location.Directory, "plugin.json", manifestPath) == CatalogStatus::Ready)
                entry.ManifestResult = ReadPluginManifest(manifestPath, manifest);
            else
                entry.ManifestResult = EPluginManifestResult::ReadFailed;
            if (entry.ManifestResult != EPluginManifestResult::Success)
            {
                entry.DisplayName = Engine::WStrToStr(location.Directory.filename().wstring());
                entry.Status = CatalogStatus::InvalidManifest;
                return entry;
            }

            entry.Id = manifest.id;
            entry.DisplayName = manifest.name;
            entry.Version = manifest.version;
            entry.Status = InspectPackageModule(manifest, sdkCompatibility, location.Directory, entry.BinaryPath);
            if (entry.Status == CatalogStatus::Ready && entry.Origin == EPluginOrigin::Custom)
                entry.Status = CatalogStatus::NeedsApproval;

            return entry;
        }

        void MarkDuplicatePackageIds(std::vector<PluginCatalogEntry>& entries)
        {
            struct PackageIdCount
            {
                std::size_t Builtin = 0;
                std::size_t Custom = 0;
            };

            std::unordered_map<std::string, PackageIdCount> counts;
            for (const auto& entry : entries)
            {
                if (entry.Origin == EPluginOrigin::Development || entry.Id.empty())
                    continue;

                auto& count = counts[entry.Id];
                if (entry.Origin == EPluginOrigin::Builtin)
                    ++count.Builtin;
                else
                    ++count.Custom;
            }

            for (auto& entry : entries)
            {
                if (entry.Origin == EPluginOrigin::Development || entry.Id.empty())
                    continue;

                const auto& count = counts.at(entry.Id);
                const bool duplicate = entry.Origin == EPluginOrigin::Builtin
                    ? count.Builtin > 1
                    : count.Builtin > 0 || count.Custom > 1;
                if (duplicate)
                    entry.Status = CatalogStatus::DuplicatePackageId;
            }
        }
    }

    std::filesystem::path ResolvePluginSourceDirectory(const std::filesystem::path& projectRoot,
        const std::filesystem::path& relativeSource)
    {
        if (projectRoot.empty() || relativeSource.empty() || relativeSource.has_root_path())
            return {};

        std::error_code error;
        const auto root = std::filesystem::canonical(projectRoot, error);
        if (error)
            return {};

        const auto source = std::filesystem::canonical(root / relativeSource, error);
        if (error || !IsInsideDirectory(source, root) || !std::filesystem::is_directory(source, error) || error)
            return {};

        return source;
    }

    std::vector<PluginPackageLocation> DiscoverPluginPackages(const std::filesystem::path& executableDirectory,
        const std::filesystem::path& projectRoot)
    {
        std::vector<PluginPackageLocation> packages;

        const auto appendPackages = [&packages](const std::filesystem::path& base,
            const std::filesystem::path& folder, bool isBuiltin)
            {
                const auto root = ResolvePluginSourceDirectory(base, folder);
                if (root.empty())
                    return;

                std::error_code error;
                auto it = std::filesystem::directory_iterator(
                    root, std::filesystem::directory_options::skip_permission_denied, error);
                const std::filesystem::directory_iterator end;

                for (; !error && it != end; it.increment(error))
                {
                    const auto directory = ResolvePluginSourceDirectory(root, it->path().filename());
                    std::error_code manifestError;
                    if (directory.empty() || !std::filesystem::exists(directory / "plugin.json", manifestError))
                        continue;

                    const bool alreadyDiscovered = std::any_of(packages.begin(), packages.end(),
                        [&directory](const PluginPackageLocation& package) { return package.Directory == directory; });
                    if (!alreadyDiscovered)
                        packages.push_back({ directory, isBuiltin });
                }
            };

        appendPackages(executableDirectory, "BuiltinPlugins", true);
        appendPackages(projectRoot, "Plugins", false);

        std::sort(packages.begin(), packages.end(),
            [](const PluginPackageLocation& a, const PluginPackageLocation& b) { return a.Directory < b.Directory; });
        return packages;
    }

    EPluginCatalogStatus ResolveDevelopmentPluginBinary(const std::filesystem::path& executableDirectory,
        const PluginDevelopmentEntry& entry, std::filesystem::path& binary)
    {
        binary.clear();
        const auto dllName = Engine::Utf8ToPath(entry.Target + ".dll");
        return ResolvePluginFile(executableDirectory, dllName, binary);
    }

    std::vector<PluginCatalogEntry> BuildPluginCatalog(const std::filesystem::path& executableDirectory,
        std::span<const PluginPackageLocation> packages, std::string_view sdkCompatibility)
    {
        auto development = ReadPluginDevelopmentList(executableDirectory / "Plugins.txt");
        std::vector<PluginCatalogEntry> entries;
        entries.reserve(development.size() + packages.size());

        for (auto& item : development)
        {
            PluginCatalogEntry entry;
            entry.Id = item.Target;
            entry.DisplayName = item.Target;
            const auto dllName = Engine::Utf8ToPath(item.Target + ".dll");
            entry.Status = ResolveDevelopmentPluginBinary(executableDirectory, item, entry.BinaryPath);
            entry.Development = std::move(item);
            entries.push_back(std::move(entry));
        }

        for (const auto& package : packages)
            entries.push_back(InspectPackage(package, sdkCompatibility));

        MarkDuplicatePackageIds(entries);
        return entries;
    }
}