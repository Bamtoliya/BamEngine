#include "ClientPackage.h"

#include "AssetReferenceValidator.h"
#include "ClientBuildJob.h"
#include "Config.h"
#include "DeferredProfilePasses.h"
#include "DeferredProfileShaders.h"
#include "Functions.h"
#include "InputActionSettings.h"
#include "InputManager.h"
#include "PlatformPath.h"
#include "ReflectionRegistry.h"
#include "Renderer.h"
#include "StartupResourceLoader.h"

#include <archive/JsonArchive.h>
#include <fmt/format.h>
#include <glaze/glaze.hpp>
#include <reflection_archive/ReflectionArchiveAdapter.h>
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
#include <atomic>
#include <thread>
#include <unordered_set>

namespace
{
    namespace fs = std::filesystem;
    using Path = Engine::PlatformPath;
    using PackageJson = glz::generic_u64;

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

    struct FileCopyTask
    {
        fs::path Source;
        fs::path Target;
        fs::path Relative;
        std::uintmax_t Size = 0;
    };
    void CopyRuntimeResources(const fs::path& sourceRoot, const fs::path& targetRoot, PackageResourceInventory& inventory,
        bool includeProjectCatalogs = false)
    {
        fs::create_directories(targetRoot);
        std::vector<FileCopyTask> tasks;
        std::unordered_set<std::wstring> dirsToCreate;
        dirsToCreate.insert(targetRoot.wstring());

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

            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) == 0)
            {
                const auto relative = source.lexically_relative(sourceRoot);
                const auto extension = Engine::ToLowerWStr(source.extension().wstring());
                const bool projectCatalog = includeProjectCatalogs &&
                    Engine::ToLowerWStr(relative.parent_path().generic_wstring()) == L"data/localization" &&
                    (extension == L".csv" || extension == L".json");
                if (!projectCatalog && !IsRuntimeResource(source)) continue;
                if (projectCatalog) inventory.Expect(relative, "project localization");
                const auto target = targetRoot / relative;
                dirsToCreate.insert(target.parent_path().wstring());
                tasks.push_back({ source, target, relative, entry->file_size() });
            }
        }

        for (const auto& dir : dirsToCreate)
            fs::create_directories(dir);

        if (tasks.empty())
            return;

        std::atomic<DWORD> copyError = ERROR_SUCCESS;
        fs::path failedSource;
        fs::path failedTarget;
        std::mutex errorMutex;

        const unsigned int hardwareThreads = std::thread::hardware_concurrency();
        const unsigned int threadCount = std::min(8u, std::max(1u, hardwareThreads));
        const size_t totalTasks = tasks.size();
        const size_t chunkSize = (totalTasks + threadCount - 1) / threadCount;

        std::vector<std::thread> workers;
        workers.reserve(threadCount);

        for (unsigned int t = 0; t < threadCount; ++t)
        {
            const size_t start = t * chunkSize;
            const size_t end = std::min(start + chunkSize, totalTasks);
            if (start >= end)
                break;

            workers.emplace_back([&tasks, start, end, &copyError, &failedSource, &failedTarget, &errorMutex]() {
                for (size_t i = start; i < end; ++i)
                {
                    if (copyError.load(std::memory_order_relaxed) != ERROR_SUCCESS)
                        return;

                    const auto& task = tasks[i];
                    if (!CopyFileW(task.Source.c_str(), task.Target.c_str(), FALSE))
                    {
                        const DWORD err = GetLastError();
                        DWORD expected = ERROR_SUCCESS;
                        if (copyError.compare_exchange_strong(expected, err))
                        {
                            std::lock_guard lock(errorMutex);
                            failedSource = task.Source;
                            failedTarget = task.Target;
                        }
                        return;
                    }
                }
                });
        }

        for (auto& worker : workers)
        {
            if (worker.joinable())
                worker.join();
        }

        if (copyError.load() != ERROR_SUCCESS)
        {
            throw fs::filesystem_error("Could not copy package file", failedSource, failedTarget,
                std::error_code(copyError.load(), std::system_category()));
        }

        for (const auto& task : tasks)
            inventory.Record(task.Relative, task.Size);
    }

    struct PackageProfile
    {
        Engine::RenderPipelineProfile Profile;
        fs::path RelativePath;
        bool InEngine = false;
    };

    struct PackageStartupPipeline
    {
        fs::path ManifestPath;
        fs::path RelativePath;
        bool InEngine = false;
        std::vector<fs::path> Files;
    };

    std::string ReadPackageConfigFile(const fs::path& source)
    {
        std::ifstream input(source, std::ios::binary);
        if (!input)
            throw std::runtime_error("Could not read package settings file: " + DisplayPath(source));
        input.exceptions(std::ios::badbit);
        return { std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
    }

    PackageJson ReadPackageProjectSettings(const fs::path& source)
    {
        auto text = ReadPackageConfigFile(source);
        if (text.starts_with("\xef\xbb\xbf"))
            text.erase(0, 3);

        PackageJson config;
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

        return config;
    }

    std::vector<std::string> ReadRequiredGameplayActions(const fs::path& projectSettings, const fs::path& startupScene)
    {
        const auto config = ReadPackageProjectSettings(projectSettings);
        const auto& values = config.get_object().at("Settings").get_object();
        const auto requiredEntry = values.find("RequiredGameplayActions");
        if (requiredEntry == values.end())
            return {};
        const auto* required = requiredEntry->second.get_if<PackageJson::array_t>();
        if (!required)
            throw std::runtime_error("RequiredGameplayActions must be an array: " + DisplayPath(projectSettings));
        if (required->empty())
            return {};

        if (const auto entry = values.find("RequiredGameplayActionsScene"); entry != values.end())
        {
            const auto* scope = entry->second.get_if<std::string>();
            if (!scope)
                throw std::runtime_error("RequiredGameplayActionsScene must be a string: " + DisplayPath(projectSettings));
            if (!scope->empty())
            {
                const auto scopePath = Engine::Utf8ToPath(*scope);
                const bool projectUri = Path::IsProjectUri(*scope);
                if (!projectUri && (scopePath.has_root_name() || scopePath.has_root_directory()))
                    throw std::runtime_error("RequiredGameplayActionsScene must use a project URI or asset-relative path: " + *scope);
                const auto assetRoot = fs::weakly_canonical(Path::GetProjectAssetRoot());
                const auto scopedScene = fs::weakly_canonical(projectUri ? Path::ResolveUri(scopePath.wstring()) : assetRoot / scopePath);
                if (!IsWithin(scopedScene, assetRoot) ||
                    !Engine::ToLowerStr(DisplayPath(scopedScene.filename())).ends_with(".bamscene.json"))
                    throw std::runtime_error("RequiredGameplayActionsScene must identify a project scene: " + *scope);
                const auto canonicalScene = fs::weakly_canonical(startupScene);
                if (CompareStringOrdinal(scopedScene.c_str(), -1, canonicalScene.c_str(), -1, TRUE) != CSTR_EQUAL)
                    return {};
            }
        }

        std::vector<std::string> actions;
        actions.reserve(required->size());
        for (const auto& entry : *required)
        {
            const auto* action = entry.get_if<std::string>();
            if (!action || action->empty() || std::ranges::find(actions, *action) != actions.end())
                throw std::runtime_error("RequiredGameplayActions must contain unique non-empty action IDs: " + DisplayPath(projectSettings));
            actions.push_back(*action);
        }
        return actions;
    }

    fs::path ResolveAdditionalPackageFile(const std::string& uri, const fs::path& assetRoot, bool scene)
    {
        if (!Path::IsProjectUri(uri))
            throw std::runtime_error("Additional package entries must use Project://: " + uri);

        std::error_code error;
        const auto file = fs::canonical(Path::ResolveUri(uri), error);
        if (error || !IsWithin(file, assetRoot) || !fs::is_regular_file(file, error))
            throw std::runtime_error("Additional package file is missing or outside the project asset folder: " + uri);
        if (scene && !Engine::ToLowerStr(DisplayPath(file.filename())).ends_with(".bamscene.json"))
            throw std::runtime_error("Additional scene must be a .bamscene.json file: " + uri);
        return file;
    }

    std::vector<std::string> ReadPackageGameplayActions(const fs::path& settings, const Editor::ClientPackageRequest& request)
    {
        auto actions = ReadRequiredGameplayActions(settings, request.StartupScene);
        if (request.AdditionalScenes.empty())
            return actions;

        const auto assetRoot = fs::canonical(Path::GetProjectAssetRoot());
        for (const auto& uri : request.AdditionalScenes)
        {
            const auto scene = ResolveAdditionalPackageFile(uri, assetRoot, true);
            for (auto& action : ReadRequiredGameplayActions(settings, scene))
            {
                if (std::ranges::find(actions, action) == actions.end())
                    actions.push_back(std::move(action));
            }
        }
        return actions;
    }

    std::string MakePackageSettings(const fs::path& source, const fs::path& startupSceneRelative,
        const fs::path& assetRoot, const fs::path& engineRoot, std::string_view programName,
        PackageProfile& packageProfile, PackageStartupPipeline& startupPipeline)
    {
        auto config = ReadPackageProjectSettings(source);
        auto& values = config.get_object().at("Settings").get_object();
        Engine::ProjectSettings defaults;
        std::string startupManifestUri = defaults.StartupManifestPath;
        if (const auto entry = values.find("StartupManifestPath"); entry != values.end())
        {
            const auto* path = entry->second.get_if<std::string>();
            if (!path || path->empty())
                throw std::runtime_error("StartupManifestPath must be a non-empty string: " + DisplayPath(source));
            startupManifestUri = *path;
        }

        const auto startupManifest = Path::ResolveUri(Engine::Utf8ToPath(startupManifestUri).wstring());
        std::string startupError;
        if (!Engine::StartupResourceLoader::CollectPipelineFiles(startupManifest, Engine::ERHIType::DirectX12,
            startupPipeline.Files, startupError))
            throw std::runtime_error("Startup resource pipeline validation failed: " + startupError);

        startupPipeline.ManifestPath = fs::canonical(startupManifest);
        startupPipeline.InEngine = IsWithin(startupPipeline.ManifestPath, engineRoot);
        if (!startupPipeline.InEngine && !IsWithin(startupPipeline.ManifestPath, assetRoot))
            throw std::runtime_error("Startup manifest is outside project and engine resource folders: " +
                DisplayPath(startupPipeline.ManifestPath));
        startupPipeline.RelativePath = RelativeToInput(startupPipeline.ManifestPath,
            startupPipeline.InEngine ? engineRoot : assetRoot);

        for (auto& file : startupPipeline.Files)
        {
            file = fs::canonical(file);
            if (!IsWithin(file, assetRoot) && !IsWithin(file, engineRoot))
                throw std::runtime_error("Startup resource is outside project and engine resource folders: " + DisplayPath(file));
        }

        values["StartupScenePath"] = DisplayPath(startupSceneRelative);
        values["AssetDirectory"] = std::string("Assets");
        values["EngineResourceDirectory"] = std::string("Engine/Resources");
        values["StartupManifestPath"] =
            std::string(startupPipeline.InEngine ? "Engine://" : "Project://") + DisplayPath(startupPipeline.RelativePath);
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

    ClientPackageInput PrepareClientPackageInput(const fs::path& projectRoot, const ClientPackageRequest& request)
    {
        ClientPackageInput input;
        input.RequiredActions = ReadPackageGameplayActions(projectRoot / "Config" / "Project.json", request);
        if (!request.AdditionalAssets.empty())
        {
            const auto assetRoot = fs::canonical(Path::GetProjectAssetRoot());
            for (const auto& uri : request.AdditionalAssets)
                ResolveAdditionalPackageFile(uri, assetRoot, false);
        }
        if (input.RequiredActions.empty())
            return input;

        const auto inputPath = projectRoot / "Config" / "InputActions.json";
        if (!fs::is_regular_file(inputPath))
            throw std::runtime_error("Required input settings file is missing: " + DisplayPath(inputPath));
        input.Contents = ReadPackageConfigFile(inputPath);
        Engine::InputActionSettings settings;
        archive::JsonArchive archive;
        const auto& registry = Engine::ReflectionRegistry::Get().GetRegistry();
        const auto* type = registry.FindType<Engine::InputActionSettings>();
        if (!archive.Parse(input.Contents) || !type ||
            !archive_reflection::ReadObject(archive, "Settings", *type, settings, &registry) ||
            !Engine::InputManager::ValidateInputActionSettings(settings))
            throw std::runtime_error("Invalid package input settings: " + DisplayPath(inputPath));

        const auto gameplay = std::ranges::find(settings.Maps, std::string_view{ "Gameplay" }, &Engine::InputActionMap::Id);
        if (gameplay == settings.Maps.end())
            throw std::runtime_error("Required input map 'Gameplay' is missing: " + DisplayPath(inputPath));

        for (const auto& action : input.RequiredActions)
        {
            if (std::ranges::find(gameplay->Actions, action, &Engine::InputAction::Id) == gameplay->Actions.end())
                throw std::runtime_error("Required input action 'Gameplay/" + action + "' is missing: " + DisplayPath(inputPath));
            if (std::ranges::find(gameplay->Bindings, action, &Engine::InputBinding::ActionId) == gameplay->Bindings.end())
                throw std::runtime_error("Required input action 'Gameplay/" + action + "' has no binding: " + DisplayPath(inputPath));
        }
        return input;
    }

    void ExportClientPackage(const fs::path& projectRoot, const fs::path& binDirectory,
        const ClientPackageRequest& request, const ClientPackageInput& input, const std::function<void(std::string_view)>& onOutput)
    {
        const auto emit = [&](std::string_view text) {
            if (onOutput) onOutput(text);
            };
        std::string output;

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
            emit(output);
            throw std::runtime_error("Scene reference validation failed.");
        }

        std::vector<fs::path> additionalFiles;
        std::vector<fs::path> additionalSceneReferences;
        for (const auto& uri : request.AdditionalScenes)
        {
            const auto additionalScene = ResolveAdditionalPackageFile(uri, assetRoot, true);
            additionalFiles.push_back(additionalScene);
            std::vector<fs::path> dependencies;
            const auto sceneIssues = AssetReferenceValidator::ValidateBuildScene(additionalScene, &dependencies);
            for (const auto& issue : sceneIssues)
                emit(fmt::format("Additional scene reference issue: {} -> {}\n", DisplayPath(issue.SourceFile), issue.TargetReference));
            if (!sceneIssues.empty())
                throw std::runtime_error("Additional scene reference validation failed: " + uri);
            additionalSceneReferences.insert(additionalSceneReferences.end(), dependencies.begin(), dependencies.end());
            emit(fmt::format("Additional scene: {}\n", uri));
        }
        for (const auto& uri : request.AdditionalAssets)
        {
            additionalFiles.push_back(ResolveAdditionalPackageFile(uri, assetRoot, false));
            emit(fmt::format("Explicit runtime asset: {}\n", uri));
        }

        for (const auto* name : RuntimeBinaries)
        {
            if (!fs::is_regular_file(binDirectory / name))
                throw std::runtime_error("Required binary is missing: " + DisplayPath(binDirectory / name));
        }

        const auto startupSceneRelative = RelativeToInput(scene, assetRoot);
        PackageProfile packageProfile;
        PackageStartupPipeline startupPipeline;
        const auto settings = MakePackageSettings(projectRoot / "Config" / "Project.json",
            startupSceneRelative, assetRoot, engineRoot, request.ProgramName, packageProfile, startupPipeline);
        const auto profileResources = GetRenderProfileResources(packageProfile);
        VerifyRenderProfileResources(profileResources, engineRoot);

        PackageResourceInventory projectInventory;
        PackageResourceInventory engineInventory;
        projectInventory.Expect(startupSceneRelative, "startup scene");
        for (std::size_t i = 0; i < additionalFiles.size(); ++i)
        {
            projectInventory.Expect(RelativeToInput(additionalFiles[i], assetRoot),
                i < request.AdditionalScenes.size() ? "additional scene" : "explicit runtime asset");
        }
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
        for (const auto& source : additionalSceneReferences)
        {
            const bool inProject = IsWithin(source, assetRoot);
            auto& inventory = inProject ? projectInventory : engineInventory;
            inventory.Expect(RelativeToInput(source, inProject ? assetRoot : engineRoot), "additional scene dependency");
        }
        references.insert(references.end(), additionalFiles.begin(), additionalFiles.end());
        references.insert(references.end(), additionalSceneReferences.begin(), additionalSceneReferences.end());
        for (const auto& file : startupPipeline.Files)
        {
            const bool inProject = IsWithin(file, assetRoot);
            auto& inventory = inProject ? projectInventory : engineInventory;
            inventory.Expect(RelativeToInput(file, inProject ? assetRoot : engineRoot),
                file == startupPipeline.ManifestPath ? "startup resource manifest" : "startup resource dependency");
        }
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto staging = destination.parent_path() / (L".bam-staging-" + std::to_wstring(suffix));
        const auto backup = destination.parent_path() / (L".bam-previous-" + std::to_wstring(suffix));
        if (!fs::create_directory(staging))
            throw std::runtime_error("Could not create a fresh staging folder: " + DisplayPath(staging));

        emit(fmt::format("Staging: {}\n", DisplayPath(staging)));
        emit("Staging runtime binaries...\n");
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
        emit("Staging project assets (parallel)...\n");
        CopyRuntimeResources(assetRoot, stagedAssets, projectInventory, true);
        emit("Staging engine resources (parallel)...\n");
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

        for (const auto& source : startupPipeline.Files)
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

        output += "Startup manifest: " + std::string(startupPipeline.InEngine ? "Engine://" : "Project://") +
            DisplayPath(startupPipeline.RelativePath) + '\n';
        emit(output);
        output.clear();

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
            emit(output);
            output.clear();
        }

        const auto configDirectory = staging / "Config";
        fs::create_directory(configDirectory);
        std::ofstream configFile;
        configFile.exceptions(std::ios::failbit | std::ios::badbit);
        configFile.open(configDirectory / "Project.json", std::ios::binary);
        configFile << settings << '\n';
        configFile.close();

        if (ReadPackageGameplayActions(configDirectory / "Project.json", request) != input.RequiredActions)
            throw std::runtime_error("Required gameplay actions changed during export. Start the export again.");

        const auto stagedInput = configDirectory / "InputActions.json";
        if (!input.RequiredActions.empty())
        {
            std::ofstream inputFile;
            inputFile.exceptions(std::ios::failbit | std::ios::badbit);
            inputFile.open(stagedInput, std::ios::binary);
            inputFile << input.Contents;
            inputFile.close();

            if (ReadPackageConfigFile(stagedInput) != input.Contents)
                throw std::runtime_error("Staged input settings differ from the validated input: " + DisplayPath(stagedInput));
            emit("Verified required Gameplay actions in staged Config/InputActions.json.\n");
        }
        else if (const auto sourceInput = projectRoot / "Config" / "InputActions.json"; fs::is_regular_file(sourceInput))
        {
            CopyPackageFile(sourceInput, stagedInput);
        }

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
        emit(output);
        output.clear();

        const auto inspected = InspectClientPackageDestination(projectRoot, request);
        if (inspected.Directory != destination || inspected.ExistingPackage != request.Destination.ExistingPackage)
            throw std::runtime_error("Export destination changed during the build: " + DisplayPath(destination));
        emit("Publishing package...\n");
        PublishPackage(request.Destination, staging, backup, output);
        emit(output);
        output.clear();

        emit(fmt::format("Exported package: {}\n", DisplayPath(destination)));
        emit(fmt::format("Executable: {}\n", DisplayPath(destination / executableName)));
    }
}
