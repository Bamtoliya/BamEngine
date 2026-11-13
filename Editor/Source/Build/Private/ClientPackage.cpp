#include "ClientPackage.h"

#include "AssetReferenceValidator.h"
#include "ClientBuildJob.h"
#include "Config.h"
#include "DeferredProfilePasses.h"
#include "DeferredProfileShaders.h"
#include "Functions.h"
#include "PlatformPath.h"

#include <fmt/format.h>
#include <glaze/glaze.hpp>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <map>
#include <vector>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace
{
    namespace fs = std::filesystem;
    using Path = Engine::PlatformPath;

    constexpr const wchar_t* RuntimeBinaries[] = {
        L"Client.exe", L"Engine.dll", L"GameModule.dll", L"GameplayScore.dll", L"SDL3.dll", L"fmod.dll",
        L"fmt.dll", L"lz4.dll"
    };
    constexpr auto PackageMarker = L".bam-client-package";

    std::string DisplayPath(const fs::path& path)
    {
        return Engine::WStrToStr(path.generic_wstring());
    }

    bool IsWithin(const fs::path& path, const fs::path& root)
    {
        const auto sameComponent = [](const fs::path& lhs, const fs::path& rhs)
            {
                return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_EQUAL;
            };
        return std::mismatch(path.begin(), path.end(), root.begin(), root.end(), sameComponent).second == root.end();
    }

    bool IsValidWindowsName(const std::wstring& name, std::size_t maxLength)
    {
        if (name.empty() || name.size() > maxLength || name.back() == L'.' || name.back() == L' ' ||
            name.find_first_of(L"<>:\"/\\|?*") != std::wstring::npos ||
            std::ranges::any_of(name, [](wchar_t c) { return c < 32; }))
            return false;

        auto stem = name.substr(0, name.find(L'.'));
        const auto last = stem.find_last_not_of(L' ');
        if (last == std::wstring::npos)
            return false;
        stem.resize(last + 1);
        std::transform(stem.begin(), stem.end(), stem.begin(), [](wchar_t c) { return static_cast<wchar_t>(std::towupper(c)); });
        if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL" ||
            stem == L"CONIN$" || stem == L"CONOUT$")
            return false;
        return !(stem.size() == 4 && (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) &&
            std::wstring_view(L"123456789\u00b9\u00b2\u00b3").find(stem.back()) != std::wstring_view::npos);
    }

    std::optional<Editor::ClientPackageIdentity> ReadPackageIdentity(const fs::path& directory)
    {
        const DWORD attributes = GetFileAttributesW(directory.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES)
        {
            const DWORD error = GetLastError();
            if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
                return std::nullopt;
            throw fs::filesystem_error("Could not inspect export destination", directory, { static_cast<int>(error), std::system_category() });
        }
        if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            throw std::runtime_error("Export destination is not a regular directory: " + DisplayPath(directory));

        const auto regularFile = [&](const fs::path& relative)
        {
            const DWORD value = GetFileAttributesW((directory / relative).c_str());
            return value != INVALID_FILE_ATTRIBUTES && (value & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
        };
        if ((!regularFile(PackageMarker) && !regularFile(L"Client.exe")) ||
            !regularFile(L"Engine.dll") || !regularFile(L"Config/Project.json"))
            throw std::runtime_error("The existing folder is not a BamEngine Client package: " + DisplayPath(directory));

        const HANDLE handle = CreateFileW(directory.c_str(), FILE_READ_ATTRIBUTES,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
        if (handle == INVALID_HANDLE_VALUE)
            throw fs::filesystem_error("Could not open existing package", directory, { static_cast<int>(GetLastError()), std::system_category() });
        BY_HANDLE_FILE_INFORMATION info{};
        const BOOL read = GetFileInformationByHandle(handle, &info);
        const DWORD error = read ? ERROR_SUCCESS : GetLastError();
        CloseHandle(handle);
        if (!read)
            throw fs::filesystem_error("Could not identify existing package", directory, { static_cast<int>(error), std::system_category() });
        if ((info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            throw std::runtime_error("Export destination changed: " + DisplayPath(directory));
        return Editor::ClientPackageIdentity{
            static_cast<std::uint32_t>(info.dwVolumeSerialNumber),
            (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) | info.nFileIndexLow
        };
    }

    void MovePackage(const fs::path& source, const fs::path& target, const char* message)
    {
        if (!MoveFileW(source.c_str(), target.c_str()))
            throw fs::filesystem_error(message, source, target, { static_cast<int>(GetLastError()), std::system_category() });
    }

    void PublishPackage(const Editor::ClientPackageDestination& destination, const fs::path& staging,
        const fs::path& backup, std::string& output)
    {
        if (ReadPackageIdentity(destination.Directory) != destination.ExistingPackage)
            throw std::runtime_error("Export destination changed after selection: " + DisplayPath(destination.Directory));

        if (destination.ExistingPackage)
        {
            output += "Previous package: " + DisplayPath(backup) + '\n';
            MovePackage(destination.Directory, backup, "Could not move the previous package aside");
        }
        try
        {
            if (destination.ExistingPackage && ReadPackageIdentity(backup) != destination.ExistingPackage)
                throw std::runtime_error("The previous package changed during publication.");
            MovePackage(staging, destination.Directory, "Could not publish the package");
        }
        catch (...)
        {
            if (destination.ExistingPackage)
            {
                if (MoveFileW(backup.c_str(), destination.Directory.c_str()))
                    output += "Previous package restored.\n";
                else
                    output += "Could not restore previous package. It is available at: " + DisplayPath(backup) + '\n';
            }
            throw;
        }
    }

    fs::path RelativeToInput(const fs::path& path, const fs::path& root)
    {
        if (!IsWithin(path, root))
            throw std::runtime_error("Resource is outside its input folder: " + DisplayPath(path));

        fs::path relative;
        for (auto component = std::next(path.begin(), std::distance(root.begin(), root.end()));
            component != path.end(); ++component)
            relative /= *component;
        return relative;
    }

    bool IsRuntimeResource(const fs::path& path)
    {
        const auto extension = Engine::ToLowerStr(DisplayPath(path.extension()));
        constexpr std::string_view runtimeExtensions[] = {
            ".json", ".beve", ".cso", ".dxil", ".spv", ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".dds",
            ".ogg", ".wav", ".mp3", ".ttf", ".otf", ".animator", ".spriteanim", ".actiontimeline", ".effect", ".vfx"
        };
        return extension.starts_with(".bam")
            || std::ranges::find(runtimeExtensions, std::string_view(extension)) != std::end(runtimeExtensions);
    }

    void CopyPackageFile(const fs::path& source, const fs::path& target)
    {
        fs::create_directories(target.parent_path());
        fs::copy_file(source, target);
    }

    struct PackageFileStats
    {
        std::size_t Count = 0;
        std::uintmax_t Bytes = 0;

        void Add(std::uintmax_t size)
        {
            ++Count;
            Bytes += size;
        }
    };

    struct PackageResourceInventory
    {
        struct PathLess
        {
            bool operator()(const fs::path& lhs, const fs::path& rhs) const
            {
                return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_LESS_THAN;
            }
        };

        void Expect(fs::path relative, std::string_view reason)
        {
            Reasons.try_emplace(relative.make_preferred(), reason);
        }

        void Record(fs::path relative, std::uintmax_t size)
        {
            const auto reason = Reasons.find(relative.make_preferred());
            if (reason != Reasons.end())
            {
                Identified.Add(size);
                if (IdentifiedExamples.size() < ExampleLimit)
                    IdentifiedExamples.push_back(fmt::format("{} [{}]", DisplayPath(relative), reason->second));
                return;
            }

            Unclassified.Add(size);
            if (UnclassifiedExamples.size() < ExampleLimit)
                UnclassifiedExamples.push_back(DisplayPath(relative));
        }

        void Append(std::string_view label, std::string& output) const
        {
            output += fmt::format("{}: {} files / {} bytes staged\n", label,
                Identified.Count + Unclassified.Count, Identified.Bytes + Unclassified.Bytes);
            output += fmt::format("  Known dependencies: {} files / {} bytes\n", Identified.Count, Identified.Bytes);
            for (const auto& example : IdentifiedExamples)
                output += "    " + example + '\n';
            output += fmt::format("  Dependencies not yet identified: {} files / {} bytes\n",
                Unclassified.Count, Unclassified.Bytes);
            for (const auto& example : UnclassifiedExamples)
                output += "    " + example + '\n';
        }

    private:
        static constexpr std::size_t ExampleLimit = 6;
        std::map<fs::path, std::string_view, PathLess> Reasons;
        PackageFileStats Identified;
        PackageFileStats Unclassified;
        std::vector<std::string> IdentifiedExamples;
        std::vector<std::string> UnclassifiedExamples;
    };

    void CopyRuntimeResources(const fs::path& sourceRoot, const fs::path& targetRoot, PackageResourceInventory& inventory)
    {
        fs::create_directories(targetRoot);
        for (auto entry = fs::recursive_directory_iterator(sourceRoot); entry != fs::recursive_directory_iterator{}; ++entry)
        {
            const auto& source = entry->path();
            const DWORD attributes = GetFileAttributesW(source.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
                throw fs::filesystem_error("Could not read resource attributes", source,
                    std::error_code(GetLastError(), std::system_category()));

            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
                entry.disable_recursion_pending();
                continue;
            }

            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0 && IsRuntimeResource(source))
            {
                const auto relative = source.lexically_relative(sourceRoot);
                const auto target = targetRoot / relative;
                CopyPackageFile(source, target);
                inventory.Record(relative, fs::file_size(target));
            }
        }
    }

    struct PackageProfile
    {
        Engine::RenderPipelineProfile Profile;
        fs::path RelativePath;
        bool InEngine = false;
    };

    std::string MakePackageSettings(const fs::path& source, const fs::path& startupSceneRelative,
        const fs::path& assetRoot, const fs::path& engineRoot, std::string_view programName, PackageProfile& packageProfile)
    {
        std::ifstream input(source, std::ios::binary);
        if (!input)
            throw std::runtime_error("Could not read project settings: " + DisplayPath(source));

        std::string text{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
        if (text.starts_with("\xef\xbb\xbf"))
            text.erase(0, 3);

        glz::generic_u64 config;
        if (glz::validate_json(text) || glz::read_json(config, text) || !config.is_object())
            throw std::runtime_error("Invalid project settings: " + DisplayPath(source));

        auto settings = config.get_object().find("Settings");
        if (settings == config.get_object().end() || !settings->second.is_object())
            throw std::runtime_error("Project settings object is missing: " + DisplayPath(source));

        auto& values = settings->second.get_object();
        const auto version = values.find("SchemaVersion");
        const auto* schema = version == values.end() ? nullptr : version->second.get_if<std::uint64_t>();
        if (!schema || *schema == 0 || *schema > Engine::ProjectSettings::CurrentSchemaVersion)
            throw std::runtime_error("Unsupported project settings schema: " + DisplayPath(source));

        values["StartupScenePath"] = DisplayPath(startupSceneRelative);
        values["AssetDirectory"] = std::string("Assets");
        values["EngineResourceDirectory"] = std::string("Engine/Resources");
        values["WindowTitle"] = std::string(programName);

        if (const auto entry = values.find("RenderPipelineProfilePath"); entry != values.end())
        {
            const auto* path = entry->second.get_if<std::string>();
            if (!path)
                throw std::runtime_error("RenderPipelineProfilePath must be a string: " + DisplayPath(source));
            if (!path->empty())
            {
                const auto file = fs::canonical(Path::ResolveUri(Engine::Utf8ToPath(*path).wstring()));
                packageProfile.InEngine = IsWithin(file, engineRoot);
                packageProfile.RelativePath = RelativeToInput(file, packageProfile.InEngine ? engineRoot : assetRoot);
                auto result = Engine::LoadRenderPipelineProfile(file, packageProfile.Profile);
                if (result)
                    result = Engine::ValidateDeferredProfile(packageProfile.Profile);
                if (!result)
                    throw std::runtime_error("Render profile " + DisplayPath(file) + " [" + result.Field + "]: " + result.Message);

                values["RenderPipelineProfilePath"] =
                    std::string(packageProfile.InEngine ? "Engine://" : "Project://") + DisplayPath(packageProfile.RelativePath);
            }
        }

        std::string output;
        if (glz::write_json(config, output))
            throw std::runtime_error("Could not serialize package settings.");
        return output;
    }

    std::vector<fs::path> GetRenderProfileResources(const PackageProfile& packageProfile)
    {
        std::vector<fs::path> resources;
        if (!packageProfile.RelativePath.empty())
        {
            for (const auto& shader : Engine::GetDeferredProfileShaders(packageProfile.Profile))
                resources.push_back(fs::path("Shader") / "bin" / "dxil" / shader.DxilFile);
            resources.emplace_back("Material/UIMaterial.bammat.json");
        }
        return resources;
    }

    void VerifyRenderProfileResources(const std::vector<fs::path>& resources, const fs::path& engineRoot)
    {
        for (const auto& relative : resources)
        {
            const auto path = engineRoot / relative;
            if (!fs::is_regular_file(path) || fs::file_size(path) == 0)
                throw std::runtime_error("Required render profile resource is missing or empty: " + DisplayPath(path));
        }
    }
}

namespace Editor
{
    ClientPackageDestination InspectClientPackageDestination(const fs::path& projectRoot, const ClientPackageRequest& request)
    {
        if (request.OutputParent.empty() || request.StartupScene.empty())
            throw std::runtime_error("Select a startup scene and an export location.");

        const auto parent = fs::canonical(request.OutputParent);
        if (!fs::is_directory(parent))
            throw std::runtime_error("Export location is not a directory: " + DisplayPath(parent));

        const auto directoryName = Engine::Utf8ToPath(request.DirectoryName).wstring();
        const auto programName = Engine::Utf8ToPath(request.ProgramName).wstring();
        if (!IsValidWindowsName(directoryName, 255))
            throw std::runtime_error("Invalid Windows package folder name: " + request.DirectoryName);
        if (!IsValidWindowsName(programName, 251) || Engine::ToLowerStr(request.ProgramName).ends_with(".exe"))
            throw std::runtime_error("Enter a valid Windows program name without the .exe extension: " + request.ProgramName);

        const auto destination = parent / directoryName;
        const auto source = fs::canonical(projectRoot);
        const auto assetRoot = fs::canonical(Path::GetProjectAssetRoot());
        const auto engineRoot = fs::canonical(Path::GetEngineResourceRoot());
        const auto buildRoot = fs::weakly_canonical(source / "out" / "build");
        if (IsWithin(source, destination) || IsWithin(destination, assetRoot) || IsWithin(assetRoot, destination) ||
            IsWithin(destination, engineRoot) || IsWithin(engineRoot, destination) ||
            IsWithin(destination, buildRoot) || IsWithin(buildRoot, destination))
            throw std::runtime_error("Export destination overlaps the project or package inputs: " + DisplayPath(destination));

        return { destination, ReadPackageIdentity(destination) };
    }

    void ExportClientPackage(const fs::path& projectRoot, const fs::path& binDirectory,
        const ClientPackageRequest& request, std::string& output)
    {
        const auto assetRoot = fs::canonical(Path::GetProjectAssetRoot());
        const auto engineRoot = fs::canonical(Path::GetEngineResourceRoot());
        const auto scene = fs::canonical(request.StartupScene);
        const auto& destination = request.Destination.Directory;

        if (!IsWithin(scene, assetRoot) || !Engine::ToLowerStr(DisplayPath(scene.filename())).ends_with(".bamscene.json"))
            throw std::runtime_error("Choose a saved scene inside the project asset folder: " + DisplayPath(scene));

        std::vector<fs::path> references;
        const auto issues = AssetReferenceValidator::ValidateBuildScene(scene, &references);
        if (!issues.empty())
        {
            for (const auto& issue : issues)
            {
                output += "Scene reference issue: " + DisplayPath(issue.SourceFile);
                if (!issue.TargetReference.empty())
                    output += " -> " + issue.TargetReference;
                output += '\n';
            }
            throw std::runtime_error("Scene reference validation failed.");
        }

        for (const auto* name : RuntimeBinaries)
        {
            if (!fs::is_regular_file(binDirectory / name))
                throw std::runtime_error("Required binary is missing: " + DisplayPath(binDirectory / name));
        }

        const auto startupSceneRelative = RelativeToInput(scene, assetRoot);
        PackageProfile packageProfile;
        const auto settings = MakePackageSettings(projectRoot / "Config" / "Project.json",
            startupSceneRelative, assetRoot, engineRoot, request.ProgramName, packageProfile);
        const auto profileResources = GetRenderProfileResources(packageProfile);
        VerifyRenderProfileResources(profileResources, engineRoot);

        PackageResourceInventory projectInventory;
        PackageResourceInventory engineInventory;
        projectInventory.Expect(startupSceneRelative, "startup scene");
        if (!packageProfile.RelativePath.empty())
        {
            auto& profileInventory = packageProfile.InEngine ? engineInventory : projectInventory;
            profileInventory.Expect(packageProfile.RelativePath, "render pipeline profile");
            for (const auto& relative : profileResources)
                engineInventory.Expect(relative, "render pipeline dependency");
        }
        for (const auto& source : references)
        {
            const bool inProject = IsWithin(source, assetRoot);
            auto& inventory = inProject ? projectInventory : engineInventory;
            inventory.Expect(RelativeToInput(source, inProject ? assetRoot : engineRoot), "startup scene dependency");
        }
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto staging = destination.parent_path() / (L".bam-staging-" + std::to_wstring(suffix));
        const auto backup = destination.parent_path() / (L".bam-previous-" + std::to_wstring(suffix));
        if (!fs::create_directory(staging))
            throw std::runtime_error("Could not create a fresh staging folder: " + DisplayPath(staging));

        output += "Staging: " + DisplayPath(staging) + '\n';
        const auto executableName = Engine::Utf8ToPath(request.ProgramName + ".exe");
        PackageFileStats binaries;
        for (const auto* name : RuntimeBinaries)
        {
            const auto fileName = std::wstring_view(name) == L"Client.exe" ? executableName : fs::path(name);
            const auto target = staging / fileName;
            CopyPackageFile(binDirectory / name, target);
            binaries.Add(fs::file_size(target));
        }

        const auto stagedAssets = staging / "Assets";
        const auto stagedEngine = staging / "Engine" / "Resources";
        CopyRuntimeResources(assetRoot, stagedAssets, projectInventory);
        CopyRuntimeResources(engineRoot, stagedEngine, engineInventory);

        for (const auto& source : references)
        {
            const bool inProject = IsWithin(source, assetRoot);
            const auto relative = RelativeToInput(source, inProject ? assetRoot : engineRoot);
            const auto target = (inProject ? stagedAssets : stagedEngine) / relative;
            if (!fs::is_regular_file(target))
            {
                CopyPackageFile(source, target);
                auto& inventory = inProject ? projectInventory : engineInventory;
                inventory.Record(relative, fs::file_size(target));
            }
        }

        if (!packageProfile.RelativePath.empty())
        {
            const auto stagedProfile = (packageProfile.InEngine ? stagedEngine : stagedAssets) / packageProfile.RelativePath;
            fs::create_directories(stagedProfile.parent_path());
            if (!fs::is_regular_file(stagedProfile))
            {
                const auto saved = Engine::SaveRenderPipelineProfile(stagedProfile, packageProfile.Profile);
                if (!saved)
                    throw std::runtime_error("Could not stage render profile: " + DisplayPath(stagedProfile) + ": " + saved.Message);
                auto& inventory = packageProfile.InEngine ? engineInventory : projectInventory;
                inventory.Record(packageProfile.RelativePath, fs::file_size(stagedProfile));
            }

            Engine::RenderPipelineProfile reloaded;
            auto checked = Engine::LoadRenderPipelineProfile(stagedProfile, reloaded);
            if (checked)
                checked = Engine::ValidateDeferredProfile(reloaded);
            if (!checked)
                throw std::runtime_error("Staged render profile did not validate: " + DisplayPath(stagedProfile) + ": " + checked.Message);
            if (reloaded != packageProfile.Profile)
                throw std::runtime_error("The render profile changed during export: " + DisplayPath(stagedProfile));
            VerifyRenderProfileResources(profileResources, stagedEngine);
            output += "Render profile: " + std::string(packageProfile.InEngine ? "Engine://" : "Project://") +
                DisplayPath(packageProfile.RelativePath) + '\n';
        }

        const auto configDirectory = staging / "Config";
        fs::create_directory(configDirectory);
        std::ofstream configFile;
        configFile.exceptions(std::ios::failbit | std::ios::badbit);
        configFile.open(configDirectory / "Project.json", std::ios::binary);
        configFile << settings << '\n';
        configFile.close();

        const auto inputActions = projectRoot / "Config" / "InputActions.json";
        if (fs::is_regular_file(inputActions))
            CopyPackageFile(inputActions, configDirectory / "InputActions.json");

        std::ofstream marker;
        marker.exceptions(std::ios::failbit | std::ios::badbit);
        marker.open(staging / PackageMarker, std::ios::binary);
        marker << "BamEngine Windows Client\n" << request.ProgramName << ".exe\n";
        marker.close();

        output += "\n[Package inventory]\n";
        output += "Current collection policy: full runtime resource scan. Listed paths are examples.\n";
        projectInventory.Append("Project assets", output);
        engineInventory.Append("Engine resources", output);
        output += fmt::format("Runtime binaries (fixed list): {} files / {} bytes\n", binaries.Count, binaries.Bytes);
        for (const auto* name : RuntimeBinaries)
        {
            const auto fileName = std::wstring_view(name) == L"Client.exe" ? executableName : fs::path(name);
            output += "    " + DisplayPath(fileName) + '\n';
        }

        const auto inspected = InspectClientPackageDestination(projectRoot, request);
        if (inspected.Directory != destination || inspected.ExistingPackage != request.Destination.ExistingPackage)
            throw std::runtime_error("Export destination changed during the build: " + DisplayPath(destination));
        PublishPackage(request.Destination, staging, backup, output);

        output += "Exported package: " + DisplayPath(destination) + '\n';
        output += "Executable: " + DisplayPath(destination / executableName);
    }
}
