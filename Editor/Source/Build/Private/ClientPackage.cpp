#include "ClientPackage.h"

#include "AssetReferenceValidator.h"
#include "ClientBuildJob.h"
#include "Config.h"
#include "DeferredProfilePasses.h"
#include "DeferredProfileShaders.h"
#include "Functions.h"
#include "InputActionSettings.h"
#include "InputManager.h"
#include "PakArchive.h"
#include "PlatformPath.h"
#include "ProjectDescriptor.h"
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
#include <cctype>
#include <cwctype>
#include <fstream>
#include <iterator>
#include <map>
#include <set>
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

    constexpr const wchar_t* CoreRuntimeBinaries[] = {
        L"Client.exe", L"Engine.dll", L"GameplayScore.dll", L"SDL3.dll", L"fmod.dll",
        L"fmt.dll", L"lz4.dll"
    };
    constexpr auto PackageMarker = L".bam-client-package";

    std::vector<std::pair<fs::path, fs::path>> CollectRuntimeBinarySources(
        const fs::path& projectRoot, const fs::path& binDirectory, const fs::path& executableName,
        fs::path& outProjectFileToStage)
    {
        std::vector<std::pair<fs::path, fs::path>> binaries;
        for (const auto* name : CoreRuntimeBinaries)
        {
            const auto targetName = std::wstring_view(name) == L"Client.exe" ? executableName : fs::path(name);
            binaries.emplace_back(binDirectory / name, targetName);
        }

        const fs::path projectFile = Engine::ProjectDescriptor::FindProjectFile(projectRoot);
        Engine::ProjectDescriptor desc;
        if (!projectFile.empty() && Engine::ProjectDescriptor::LoadFromFile(projectFile, desc) && !desc.Modules.empty())
        {
            outProjectFileToStage = projectFile;
            std::error_code ec;
            for (const auto& mod : desc.ResolveModules(projectRoot))
            {
                std::string lowerType = mod.Type;
                std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
                if (!lowerType.empty() && lowerType != "runtime")
                    continue;

                const fs::path dllName = Engine::Utf8ToPath(mod.Name).wstring() + L".dll";
                const fs::path sourceDll = fs::is_regular_file(mod.BinaryPath, ec)
                    ? mod.BinaryPath : (binDirectory / dllName);
                binaries.emplace_back(sourceDll, dllName);
            }
        }
        else
        {
            binaries.emplace_back(binDirectory / L"GameModule.dll", fs::path(L"GameModule.dll"));
        }

        return binaries;
    }

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
        constexpr int kMaxAttempts = 10;
        DWORD lastError = ERROR_SUCCESS;
        for (int attempt = 0; attempt < kMaxAttempts; ++attempt)
        {
            if (MoveFileW(source.c_str(), target.c_str()))
                return;
            lastError = GetLastError();
            if (lastError != ERROR_ACCESS_DENIED && lastError != ERROR_SHARING_VIOLATION)
                break;
            Sleep(100);
        }
        std::string fullMessage = message ? message : "Move failed";
        if (lastError == ERROR_ACCESS_DENIED || lastError == ERROR_SHARING_VIOLATION)
            fullMessage += " (folder or file is locked by Explorer, terminal, log viewer, or running Client.exe)";
        throw fs::filesystem_error(fullMessage, source, target, { static_cast<int>(lastError), std::system_category() });
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

    struct MountedPackagePath
    {
        std::string MountName;
        fs::path RelativePath;
        fs::path StagedRelative;
    };

    std::optional<MountedPackagePath> TryResolveContentMountPath(const fs::path& file)
    {
        const auto uri = Path::ToVirtualUri(file);
        if (!Path::IsMountUri(uri))
            return std::nullopt;

        const std::wstring_view view(uri);
        constexpr std::wstring_view prefix = L"Mount://";
        if (view.size() <= prefix.size())
            return std::nullopt;
        const auto afterScheme = view.substr(prefix.size());
        const auto slash = afterScheme.find(L'/');
        if (slash == std::wstring_view::npos || slash == 0 || slash + 1 >= afterScheme.size())
            return std::nullopt;

        const std::string mountName = Engine::WStrToStr(std::wstring(afterScheme.substr(0, slash)));
        const fs::path relPath(afterScheme.substr(slash + 1));
        return MountedPackagePath{ mountName, relPath, Engine::Utf8ToPath(mountName) / relPath };
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

        void RecordExcluded(fs::path relative, std::uintmax_t size)
        {
            Excluded.Add(size);
            if (ExcludedExamples.size() < ExampleLimit)
                ExcludedExamples.push_back(DisplayPath(relative));
        }

        void Append(std::string_view label, std::string& output, bool dependencyBased = false) const
        {
            output += fmt::format("{}: {} files / {} bytes staged\n", label,
                Identified.Count + Unclassified.Count, Identified.Bytes + Unclassified.Bytes);
            output += fmt::format("  Known dependencies: {} files / {} bytes\n", Identified.Count, Identified.Bytes);
            for (const auto& example : IdentifiedExamples)
                output += "    " + example + '\n';
            if (dependencyBased)
            {
                if (Unclassified.Count > 0)
                {
                    output += fmt::format("  Unclassified staged files: {} files / {} bytes\n",
                        Unclassified.Count, Unclassified.Bytes);
                    for (const auto& example : UnclassifiedExamples)
                        output += "    " + example + '\n';
                }
                output += fmt::format("  Excluded unreferenced runtime resources: {} files / {} bytes\n",
                    Excluded.Count, Excluded.Bytes);
                for (const auto& example : ExcludedExamples)
                    output += "    " + example + '\n';
                return;
            }
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
        PackageFileStats Excluded;
        std::vector<std::string> IdentifiedExamples;
        std::vector<std::string> UnclassifiedExamples;
        std::vector<std::string> ExcludedExamples;
    };

    struct FileCopyTask
    {
        fs::path Source;
        fs::path Target;
        fs::path Relative;
        std::uintmax_t Size = 0;
    };

    void CopyFileTasksParallel(const std::vector<FileCopyTask>& tasks)
    {
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
    }

    bool IsRegularNonReparseFile(const fs::path& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) == 0;
    }

    bool IsRegularNonReparseDirectory(const fs::path& path)
    {
        const DWORD attributes = GetFileAttributesW(path.c_str());
        return attributes != INVALID_FILE_ATTRIBUTES &&
            (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
            (attributes & FILE_ATTRIBUTE_REPARSE_POINT) == 0;
    }

    bool IsEditorOnlyEngineShader(const fs::path& fileName)
    {
        const auto lower = Engine::ToLowerStr(DisplayPath(fileName));
        constexpr std::string_view editorShaders[] = {
            "editorgridvs.cso", "editorgridps.cso", "infinitegridvs.cso", "infinitegridps.cso",
            "infinitegrid2dps.cso", "materialpreviewps.cso", "viewport_channelps.cso"
        };
        return std::ranges::find(editorShaders, std::string_view(lower)) != std::end(editorShaders);
    }

    void CollectProjectLocalizationCatalogs(const fs::path& assetRoot, std::vector<fs::path>& outFiles,
        PackageResourceInventory& projectInventory)
    {
        const auto locDir = assetRoot / "Data" / "Localization";
        if (!IsRegularNonReparseDirectory(locDir))
            return;

        for (const auto& entry : fs::directory_iterator(locDir))
        {
            const auto& path = entry.path();
            if (!IsRegularNonReparseFile(path))
                continue;
            const auto extension = Engine::ToLowerWStr(path.extension().wstring());
            if (extension != L".csv" && extension != L".json")
                continue;
            const auto canonicalPath = fs::canonical(path);
            if (!IsWithin(canonicalPath, assetRoot))
                continue;
            projectInventory.Expect(RelativeToInput(canonicalPath, assetRoot), "project localization");
            outFiles.push_back(canonicalPath);
        }
    }

    void CollectEssentialEngineResources(const fs::path& engineRoot, std::vector<fs::path>& outFiles,
        PackageResourceInventory& engineInventory)
    {
        const auto addIfPresent = [&](const fs::path& relative, std::string_view reason)
        {
            const auto fullPath = engineRoot / relative;
            if (!IsRegularNonReparseFile(fullPath))
                return;
            const auto canonicalPath = fs::canonical(fullPath);
            if (!IsWithin(canonicalPath, engineRoot))
                return;
            engineInventory.Expect(RelativeToInput(canonicalPath, engineRoot), reason);
            outFiles.push_back(canonicalPath);
        };

        const auto locDir = engineRoot / "Data" / "Localization";
        if (IsRegularNonReparseDirectory(locDir))
        {
            for (const auto& entry : fs::directory_iterator(locDir))
            {
                const auto& path = entry.path();
                if (!IsRegularNonReparseFile(path))
                    continue;
                if (Engine::ToLowerWStr(path.extension().wstring()) != L".json")
                    continue;
                const auto canonicalPath = fs::canonical(path);
                if (!IsWithin(canonicalPath, engineRoot))
                    continue;
                engineInventory.Expect(RelativeToInput(canonicalPath, engineRoot), "engine localization");
                outFiles.push_back(canonicalPath);
            }
        }

        constexpr const wchar_t* essentialTextures[] = {
            L"Texture/white1x1.png", L"Texture/white1x1.bamtex.json", L"Texture/black1x1.png",
            L"Texture/blue1x1.png", L"Texture/green1x1.png", L"Texture/magenta1x1.png",
            L"Texture/DefaultChecker.png", L"Texture/DefaultNormal.png", L"Texture/DefaultPBR.png",
            L"Texture/MissingTexture.png", L"Texture/Particle_Ring.png", L"Texture/Particle_Smoke.png",
            L"Texture/Particle_SoftCircle.png", L"Texture/Particle_Spark.png"
        };
        for (const auto* relative : essentialTextures)
            addIfPresent(relative, "engine fallback resource");

        constexpr const wchar_t* essentialMaterials[] = {
            L"Material/DefaultMaterial.bammat.json", L"Material/MissingMaterial.bammat.json",
            L"Material/UIMaterial.bammat.json", L"Material/SkinningMaterial.bammat.json",
            L"Material/SkyMaterial.bammat.json", L"Material/SpriteMaterial.bammat.json",
            L"Material/SpriteMaterial_AlphaBlend.bammat.json", L"Material/SpriteMaterialInstance.bammatinst.json",
            L"Material/TransparentMaterial.bammat.json", L"Material/TerrainMaterial.bammat.json",
            L"Material/ParticleMaterial_Additive.bammat.json", L"Material/ParticleMaterial_AlphaBlend.bammat.json",
            L"Material/PostProcessDefaultMaterial.bammat.json", L"Material/GlassMaterial.bammat.json",
            L"Material/WaterMaterial.bammat.json", L"Material/WaveMaterial.bammat.json"
        };
        for (const auto* relative : essentialMaterials)
            addIfPresent(relative, "engine fallback resource");

        const auto dxilDir = engineRoot / "Shader" / "bin" / "dxil";
        if (IsRegularNonReparseDirectory(dxilDir))
        {
            for (const auto& entry : fs::directory_iterator(dxilDir))
            {
                const auto& path = entry.path();
                if (!IsRegularNonReparseFile(path))
                    continue;
                if (Engine::ToLowerWStr(path.extension().wstring()) != L".cso")
                    continue;
                if (IsEditorOnlyEngineShader(path.filename()))
                    continue;
                const auto canonicalPath = fs::canonical(path);
                if (!IsWithin(canonicalPath, engineRoot))
                    continue;
                engineInventory.Expect(RelativeToInput(canonicalPath, engineRoot), "engine runtime shader");
                outFiles.push_back(canonicalPath);
            }
        }
    }

    void StageSelectedResources(const fs::path& sourceRoot, const fs::path& targetRoot,
        const std::vector<fs::path>& selectedFiles, PackageResourceInventory& inventory,
        std::set<fs::path, PackageResourceInventory::PathLess>& outStagedRelatives)
    {
        fs::create_directories(targetRoot);
        std::vector<FileCopyTask> tasks;
        std::unordered_set<std::wstring> dirsToCreate;
        dirsToCreate.insert(targetRoot.wstring());

        for (const auto& source : selectedFiles)
        {
            const auto relative = RelativeToInput(source, sourceRoot);
            auto key = relative;
            key.make_preferred();
            if (!outStagedRelatives.insert(key).second)
                continue;

            if (!IsRegularNonReparseFile(source))
                throw std::runtime_error("Selected package resource is missing or not a regular file: " + DisplayPath(source));

            const auto target = targetRoot / relative;
            dirsToCreate.insert(target.parent_path().wstring());
            tasks.push_back({ source, target, relative, fs::file_size(source) });
        }

        for (const auto& dir : dirsToCreate)
            fs::create_directories(dir);

        CopyFileTasksParallel(tasks);

        for (const auto& task : tasks)
            inventory.Record(task.Relative, task.Size);
    }

    void CountExcludedRuntimeResources(const fs::path& sourceRoot,
        const std::set<fs::path, PackageResourceInventory::PathLess>& stagedRelatives,
        PackageResourceInventory& inventory, bool includeProjectCatalogs = false)
    {
        for (auto entry = fs::recursive_directory_iterator(sourceRoot); entry != fs::recursive_directory_iterator{}; ++entry)
        {
            const auto& source = entry->path();
            const DWORD attributes = GetFileAttributesW(source.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
                continue;

            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
                entry.disable_recursion_pending();
                continue;
            }

            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                continue;

            const auto relative = source.lexically_relative(sourceRoot);
            const auto extension = Engine::ToLowerWStr(source.extension().wstring());
            const bool projectCatalog = includeProjectCatalogs &&
                Engine::ToLowerWStr(relative.parent_path().generic_wstring()) == L"data/localization" &&
                (extension == L".csv" || extension == L".json");
            if (!projectCatalog && !IsRuntimeResource(source))
                continue;

            auto key = relative;
            key.make_preferred();
            if (!stagedRelatives.contains(key))
                inventory.RecordExcluded(relative, entry->file_size());
        }
    }

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

        CopyFileTasksParallel(tasks);

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
        const bool isProject = Path::IsProjectUri(uri);
        const bool isMount = !scene && Path::IsMountUri(uri);
        if (!isProject && !isMount)
        {
            throw std::runtime_error(scene
                ? "Additional scene entries must use Project://: " + uri
                : "Additional package entries must use Project:// or Mount://: " + uri);
        }

        std::error_code error;
        const auto file = fs::canonical(Path::ResolveUri(uri), error);
        if (error || !fs::is_regular_file(file, error))
            throw std::runtime_error("Additional package file is missing or not a regular file: " + uri);
        if (isProject && !IsWithin(file, assetRoot))
            throw std::runtime_error("Additional package file is outside the project asset folder: " + uri);
        if (isMount && !TryResolveContentMountPath(file))
            throw std::runtime_error("Additional package file is outside registered content mounts: " + uri);
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

    Engine::EPakPayloadFormat DetectPakPayloadFormat(const fs::path& path)
    {
        const auto extension = Engine::ToLowerStr(DisplayPath(path.extension()));
        if (extension == ".json")
            return Engine::EPakPayloadFormat::Json;
        if (extension == ".beve")
            return Engine::EPakPayloadFormat::Beve;
        if (extension == ".dxil")
            return Engine::EPakPayloadFormat::Dxil;
        if (extension == ".dds")
            return Engine::EPakPayloadFormat::Dds;
        return Engine::EPakPayloadFormat::Raw;
    }

    bool IsDirectFileException(const fs::path& path)
    {
        const auto extension = Engine::ToLowerStr(DisplayPath(path.extension()));
        constexpr std::string_view exceptionExtensions[] = { ".ogg", ".wav", ".mp3", ".ttf", ".otf" };
        return std::ranges::find(exceptionExtensions, std::string_view(extension)) != std::end(exceptionExtensions);
    }

    std::uint64_t ComputeFileHash64(const fs::path& path)
    {
        std::ifstream input(path, std::ios::binary);
        if (!input.is_open())
            throw std::runtime_error("Could not open archive for hashing: " + DisplayPath(path));

        constexpr std::uint64_t FnvOffsetBasis = 14695981039346656037ULL;
        constexpr std::uint64_t FnvPrime = 1099511628211ULL;
        std::uint64_t hash = FnvOffsetBasis;

        constexpr std::size_t ChunkSize = 64 * 1024;
        std::vector<std::uint8_t> buffer(ChunkSize);

        while (input.good())
        {
            input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(ChunkSize));
            const std::streamsize bytesRead = input.gcount();
            if (bytesRead <= 0)
                break;

            for (std::streamsize i = 0; i < bytesRead; ++i)
            {
                hash ^= static_cast<std::uint64_t>(buffer[static_cast<std::size_t>(i)]);
                hash *= FnvPrime;
            }
        }

        if (input.bad())
            throw std::runtime_error("I/O error while hashing archive: " + DisplayPath(path));

        return hash;
    }

    void CollectDirectoryIntoPak(const fs::path& sourceRoot, const fs::path& stagingRoot, std::string_view mountPrefix,
        Engine::PakWriter& writer, std::vector<std::string>& directExceptions, std::vector<fs::path>& packedFiles)
    {
        std::error_code ec;
        if (!fs::exists(sourceRoot, ec) || !fs::is_directory(sourceRoot, ec))
            return;

        struct StagedFileCandidate
        {
            std::string Uri;
            fs::path PhysicalPath;
        };
        std::vector<StagedFileCandidate> candidates;

        for (auto entry = fs::recursive_directory_iterator(sourceRoot); entry != fs::recursive_directory_iterator{}; ++entry)
        {
            const auto& filePath = entry->path();
            const DWORD attributes = GetFileAttributesW(filePath.c_str());
            if (attributes == INVALID_FILE_ATTRIBUTES)
                throw fs::filesystem_error("Could not inspect staged resource for .bampak", filePath,
                    std::error_code(static_cast<int>(GetLastError()), std::system_category()));

            if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
                entry.disable_recursion_pending();
                continue;
            }

            if ((attributes & FILE_ATTRIBUTE_DIRECTORY) != 0)
                continue;

            const auto relativeToRoot = filePath.lexically_relative(sourceRoot);
            if (IsDirectFileException(filePath))
            {
                const auto relativeToStaging = filePath.lexically_relative(stagingRoot);
                directExceptions.push_back(DisplayPath(relativeToStaging));
                continue;
            }

            const std::string rawUri = std::string(mountPrefix) + DisplayPath(relativeToRoot);
            std::string normalizedUri;
            if (!Engine::NormalizePakUri(rawUri, normalizedUri))
                throw std::runtime_error("Invalid URI for .bampak entry: " + rawUri);

            candidates.push_back({ std::move(normalizedUri), filePath });
        }

        std::sort(candidates.begin(), candidates.end(), [](const StagedFileCandidate& lhs, const StagedFileCandidate& rhs)
            {
                return lhs.Uri < rhs.Uri;
            });

        for (const auto& candidate : candidates)
        {
            const Engine::EResult addRes = writer.AddFile(
                candidate.Uri, candidate.PhysicalPath, DetectPakPayloadFormat(candidate.PhysicalPath));
            if (addRes != Engine::EResult::Success)
                throw std::runtime_error("Could not add entry to .bampak: " + candidate.Uri);
            packedFiles.push_back(candidate.PhysicalPath);
        }
    }

    void PruneEmptyDirectories(const fs::path& root)
    {
        std::error_code ec;
        if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
            return;

        std::vector<fs::path> directories;
        for (const auto& entry : fs::recursive_directory_iterator(root, ec))
        {
            if (entry.is_directory(ec))
                directories.push_back(entry.path());
        }

        std::sort(directories.begin(), directories.end(), [](const fs::path& lhs, const fs::path& rhs)
            {
                return lhs.wstring().size() > rhs.wstring().size();
            });

        for (const auto& dir : directories)
        {
            if (fs::is_empty(dir, ec) && !ec)
                fs::remove(dir, ec);
        }

        if (fs::is_empty(root, ec) && !ec)
            fs::remove(root, ec);
    }

    void VerifyPakArchive(const fs::path& pakPath, std::uint64_t expectedEntryCount,
        const std::vector<std::string>& requiredUris, const fs::path& stagingRoot)
    {
        Engine::PakReader reader;
        const Engine::EResult mountRes = reader.Mount(pakPath);
        if (mountRes != Engine::EResult::Success)
            throw std::runtime_error("Failed to mount .bampak for integrity verification: " + DisplayPath(pakPath));

        if (static_cast<std::uint64_t>(reader.GetEntries().size()) != expectedEntryCount)
        {
            reader.Unmount();
            throw std::runtime_error(fmt::format(
                ".bampak entry count mismatch for {}: expected {}, actual {}",
                DisplayPath(pakPath), expectedEntryCount, reader.GetEntries().size()));
        }

        std::unordered_set<std::string> verifiedLowerUris;
        std::vector<std::uint8_t> verifyBytes;
        for (const auto& uri : requiredUris)
        {
            if (uri.empty())
                continue;

            std::string normalized;
            if (!Engine::NormalizePakUri(uri, normalized))
            {
                reader.Unmount();
                throw std::runtime_error("Invalid required URI during .bampak verification: " + uri);
            }

            const std::string lower = Engine::ToLowerPakUri(normalized);
            if (!verifiedLowerUris.insert(lower).second)
                continue;

            // Direct-file exceptions (audio/fonts) remain staged on disk rather than inside .bampak.
            fs::path exceptionPath;
            if (normalized.starts_with("Engine://"))
                exceptionPath = stagingRoot / "Engine" / "Resources" / Engine::Utf8ToPath(normalized.substr(9));
            else if (normalized.starts_with("Project://"))
                exceptionPath = stagingRoot / "Assets" / Engine::Utf8ToPath(normalized.substr(10));
            else if (normalized.starts_with("Mount://"))
                exceptionPath = stagingRoot / "Mounts" / Engine::Utf8ToPath(normalized.substr(8));

            std::error_code ec;
            if (!exceptionPath.empty() && IsDirectFileException(exceptionPath) && fs::is_regular_file(exceptionPath, ec))
                continue;

            if (!reader.Contains(normalized))
            {
                reader.Unmount();
                throw std::runtime_error("Required entry missing from .bampak (" + DisplayPath(pakPath) + "): " + normalized);
            }

            const Engine::EResult readRes = reader.ReadEntry(normalized, verifyBytes);
            if (readRes != Engine::EResult::Success)
            {
                reader.Unmount();
                throw std::runtime_error("Integrity verification failed for .bampak entry (" + DisplayPath(pakPath) + "): " + normalized);
            }
        }

        reader.Unmount();
    }
}

namespace Editor
{
    std::optional<ClientPackageManifest> ReadClientPackageManifestSummary(const fs::path& packageDirectory)
    {
        if (packageDirectory.empty())
            return std::nullopt;

        std::error_code ec;
        const auto manifestPath = packageDirectory / "Config" / "PackageManifest.json";
        if (!fs::is_regular_file(manifestPath, ec) || ec)
            return std::nullopt;

        try
        {
            auto text = ReadPackageConfigFile(manifestPath);
            if (text.starts_with("\xef\xbb\xbf"))
                text.erase(0, 3);

            if (glz::validate_json(text))
                return std::nullopt;

            ClientPackageManifest manifest;
            if (glz::read_json(manifest, text) || manifest.SchemaVersion == 0)
                return std::nullopt;
            return manifest;
        }
        catch (...)
        {
            return std::nullopt;
        }
    }

    ClientPackageManifest BuildClientPakArchives(const ClientPakBuildOptions& options,
        const std::function<void(std::string_view)>& onOutput)
    {
        const auto emit = [&](std::string_view text)
            {
                if (onOutput) onOutput(text);
            };

        const auto& staging = options.StagingDirectory;
        std::error_code ec;
        if (staging.empty() || !fs::is_directory(staging, ec))
            throw std::runtime_error("Invalid staging directory for .bampak packaging: " + DisplayPath(staging));

        const auto stagedEngine = staging / "Engine" / "Resources";
        const auto stagedAssets = staging / "Assets";
        const auto stagedMounts = staging / "Mounts";
        const auto contentDirectory = staging / "Content";
        fs::create_directories(contentDirectory);

        Engine::PakWriter engineWriter;
        Engine::PakWriter projectWriter;
        std::vector<std::string> directExceptions;
        std::vector<fs::path> packedLooseFiles;

        CollectDirectoryIntoPak(stagedEngine, staging, "Engine://", engineWriter, directExceptions, packedLooseFiles);
        CollectDirectoryIntoPak(stagedAssets, staging, "Project://", projectWriter, directExceptions, packedLooseFiles);
        CollectDirectoryIntoPak(stagedMounts, staging, "Mount://", projectWriter, directExceptions, packedLooseFiles);

        const auto enginePakPath = contentDirectory / "Engine.bampak";
        const auto projectPakPath = contentDirectory / "Project.bampak";

        emit(fmt::format("Writing Content/Engine.bampak ({} entries)...\n", engineWriter.GetEntryCount()));
        if (engineWriter.WriteToFile(enginePakPath) != Engine::EResult::Success)
            throw std::runtime_error("Failed to write Content/Engine.bampak: " + DisplayPath(enginePakPath));

        emit(fmt::format("Writing Content/Project.bampak ({} entries)...\n", projectWriter.GetEntryCount()));
        if (projectWriter.WriteToFile(projectPakPath) != Engine::EResult::Success)
            throw std::runtime_error("Failed to write Content/Project.bampak: " + DisplayPath(projectPakPath));

        std::vector<std::string> requiredEngineUris = options.RequiredEngineUris;
        std::vector<std::string> requiredProjectUris = options.RequiredProjectUris;

        const auto routeRequiredUri = [&](const std::string& uri)
            {
                if (uri.empty())
                    return;
                std::string normalized;
                if (!Engine::NormalizePakUri(uri, normalized))
                    return;
                if (normalized.starts_with("Engine://"))
                    requiredEngineUris.push_back(std::move(normalized));
                else if (normalized.starts_with("Project://") || normalized.starts_with("Mount://"))
                    requiredProjectUris.push_back(std::move(normalized));
            };

        routeRequiredUri(options.StartupSceneUri);
        routeRequiredUri(options.StartupManifestUri);
        routeRequiredUri(options.RenderPipelineProfileUri);

        emit("Verifying .bampak archives (header, index, required entries)...\n");
        VerifyPakArchive(enginePakPath, static_cast<std::uint64_t>(engineWriter.GetEntryCount()), requiredEngineUris, staging);
        VerifyPakArchive(projectPakPath, static_cast<std::uint64_t>(projectWriter.GetEntryCount()), requiredProjectUris, staging);

        ClientPakArchiveEntry enginePakEntry;
        enginePakEntry.Path = "Content/Engine.bampak";
        enginePakEntry.MountScheme = "Engine://";
        enginePakEntry.FormatVersion = Engine::PAK_FORMAT_VERSION;
        enginePakEntry.EntryCount = static_cast<std::uint64_t>(engineWriter.GetEntryCount());
        enginePakEntry.ByteSize = static_cast<std::uint64_t>(fs::file_size(enginePakPath));
        enginePakEntry.Hash64 = ComputeFileHash64(enginePakPath);
        enginePakEntry.IntegrityVerified = true;

        ClientPakArchiveEntry projectPakEntry;
        projectPakEntry.Path = "Content/Project.bampak";
        projectPakEntry.MountScheme = "Project://";
        projectPakEntry.FormatVersion = Engine::PAK_FORMAT_VERSION;
        projectPakEntry.EntryCount = static_cast<std::uint64_t>(projectWriter.GetEntryCount());
        projectPakEntry.ByteSize = static_cast<std::uint64_t>(fs::file_size(projectPakPath));
        projectPakEntry.Hash64 = ComputeFileHash64(projectPakPath);
        projectPakEntry.IntegrityVerified = true;

        if (options.PrunePackedLooseFiles)
        {
            for (const auto& file : packedLooseFiles)
            {
                std::error_code removeErr;
                fs::remove(file, removeErr);
            }
            PruneEmptyDirectories(stagedEngine);
            PruneEmptyDirectories(staging / "Engine");
            PruneEmptyDirectories(stagedAssets);
            PruneEmptyDirectories(stagedMounts);
        }

        std::sort(directExceptions.begin(), directExceptions.end());
        directExceptions.erase(std::unique(directExceptions.begin(), directExceptions.end()), directExceptions.end());

        ClientPackageManifest manifest;
        manifest.SchemaVersion = 1;
        manifest.StartupSceneUri = options.StartupSceneUri;
        manifest.StartupManifestUri = options.StartupManifestUri;
        manifest.RenderPipelineProfileUri = options.RenderPipelineProfileUri;
        manifest.BootstrapFiles.push_back("Config/Project.json");
        if (options.HasInputActionsConfig || fs::is_regular_file(staging / "Config" / "InputActions.json", ec))
            manifest.BootstrapFiles.push_back("Config/InputActions.json");
        std::sort(manifest.BootstrapFiles.begin(), manifest.BootstrapFiles.end());
        manifest.DirectFileExceptions = std::move(directExceptions);
        manifest.Paks.push_back(std::move(enginePakEntry));
        manifest.Paks.push_back(std::move(projectPakEntry));
        manifest.IntegrityVerified = true;

        std::string manifestJson;
        if (glz::write_json(manifest, manifestJson))
            throw std::runtime_error("Could not serialize Config/PackageManifest.json.");

        const auto configDirectory = staging / "Config";
        fs::create_directories(configDirectory);
        const auto manifestPath = configDirectory / "PackageManifest.json";
        std::ofstream manifestFile;
        manifestFile.exceptions(std::ios::failbit | std::ios::badbit);
        manifestFile.open(manifestPath, std::ios::binary | std::ios::trunc);
        manifestFile << manifestJson << '\n';
        manifestFile.close();

        const auto verifiedManifest = ReadClientPackageManifestSummary(staging);
        if (!verifiedManifest || !verifiedManifest->IntegrityVerified || verifiedManifest->Paks.size() != manifest.Paks.size())
            throw std::runtime_error("Staged Config/PackageManifest.json failed round-trip validation.");

        emit(fmt::format("  Content/Engine.bampak: {} entries / {} bytes (FNV64: 0x{:016X})\n",
            manifest.Paks[0].EntryCount, manifest.Paks[0].ByteSize, manifest.Paks[0].Hash64));
        emit(fmt::format("  Content/Project.bampak: {} entries / {} bytes (FNV64: 0x{:016X})\n",
            manifest.Paks[1].EntryCount, manifest.Paks[1].ByteSize, manifest.Paks[1].Hash64));
        emit(fmt::format("  Direct-file exceptions preserved: {}\n", manifest.DirectFileExceptions.size()));
        emit("Wrote and verified Config/PackageManifest.json.\n");

        return manifest;
    }

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

        for (const auto& mount : Path::GetContentMounts())
        {
            std::error_code mountEc;
            const auto mountRoot = fs::canonical(mount.RootPath, mountEc);
            if (!mountEc && (IsWithin(destination, mountRoot) || IsWithin(mountRoot, destination)))
                throw std::runtime_error("Export destination overlaps an external content mount: " + DisplayPath(destination));
        }

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

        const auto executableName = Engine::Utf8ToPath(request.ProgramName + ".exe");
        fs::path projectFileToStage;
        const auto runtimeBinarySources = CollectRuntimeBinarySources(projectRoot, binDirectory, executableName, projectFileToStage);
        for (const auto& [sourcePath, targetName] : runtimeBinarySources)
        {
            if (!fs::is_regular_file(sourcePath))
                throw std::runtime_error("Required binary is missing: " + DisplayPath(sourcePath));
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
        PackageResourceInventory mountInventory;
        const auto expectFile = [&](const fs::path& file, std::string_view reason)
        {
            if (IsWithin(file, assetRoot))
                projectInventory.Expect(RelativeToInput(file, assetRoot), reason);
            else if (IsWithin(file, engineRoot))
                engineInventory.Expect(RelativeToInput(file, engineRoot), reason);
            else if (const auto mounted = TryResolveContentMountPath(file))
                mountInventory.Expect(mounted->StagedRelative, reason);
        };

        projectInventory.Expect(startupSceneRelative, "startup scene");
        for (std::size_t i = 0; i < additionalFiles.size(); ++i)
        {
            expectFile(additionalFiles[i],
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
            expectFile(source, "startup scene dependency");
        for (const auto& source : additionalSceneReferences)
            expectFile(source, "additional scene dependency");
        references.insert(references.end(), additionalFiles.begin(), additionalFiles.end());
        references.insert(references.end(), additionalSceneReferences.begin(), additionalSceneReferences.end());
        for (const auto& file : startupPipeline.Files)
        {
            expectFile(file,
                file == startupPipeline.ManifestPath ? "startup resource manifest" : "startup resource dependency");
        }
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        const auto staging = destination.parent_path() / (L".bam-staging-" + std::to_wstring(suffix));
        const auto backup = destination.parent_path() / (L".bam-previous-" + std::to_wstring(suffix));
        if (!fs::create_directory(staging))
            throw std::runtime_error("Could not create a fresh staging folder: " + DisplayPath(staging));

        emit(fmt::format("Staging: {}\n", DisplayPath(staging)));
        emit("Staging runtime binaries...\n");
        PackageFileStats binaries;
        for (const auto& [sourcePath, targetName] : runtimeBinarySources)
        {
            const auto target = staging / targetName;
            CopyPackageFile(sourcePath, target);
            binaries.Add(fs::file_size(target));
        }
        if (!projectFileToStage.empty() && fs::is_regular_file(projectFileToStage))
        {
            CopyPackageFile(projectFileToStage, staging / projectFileToStage.filename());
        }

        const auto stagedAssets = staging / "Assets";
        const auto stagedEngine = staging / "Engine" / "Resources";
        const auto stagedMounts = staging / "Mounts";
        if (request.OnlyReferencedAssets)
        {
            std::vector<fs::path> selectedProjectFiles;
            std::vector<fs::path> selectedEngineFiles;
            std::vector<std::pair<fs::path, fs::path>> selectedMountFiles;

            const auto routeFile = [&](const fs::path& file)
            {
                if (IsWithin(file, assetRoot))
                    selectedProjectFiles.push_back(file);
                else if (IsWithin(file, engineRoot))
                    selectedEngineFiles.push_back(file);
                else if (const auto mounted = TryResolveContentMountPath(file))
                    selectedMountFiles.emplace_back(file, mounted->StagedRelative);
            };

            selectedProjectFiles.push_back(scene);
            for (const auto& source : references)
                routeFile(source);
            for (const auto& source : startupPipeline.Files)
                routeFile(source);
            if (!packageProfile.RelativePath.empty())
            {
                const auto profileSource = (packageProfile.InEngine ? engineRoot : assetRoot) / packageProfile.RelativePath;
                if (IsRegularNonReparseFile(profileSource))
                    routeFile(fs::canonical(profileSource));
                for (const auto& relative : profileResources)
                    selectedEngineFiles.push_back(fs::canonical(engineRoot / relative));
            }

            CollectProjectLocalizationCatalogs(assetRoot, selectedProjectFiles, projectInventory);
            CollectEssentialEngineResources(engineRoot, selectedEngineFiles, engineInventory);

            std::set<fs::path, PackageResourceInventory::PathLess> stagedProjectRelatives;
            std::set<fs::path, PackageResourceInventory::PathLess> stagedEngineRelatives;

            emit("Staging referenced project assets (dependency-based, parallel)...\n");
            StageSelectedResources(assetRoot, stagedAssets, selectedProjectFiles, projectInventory, stagedProjectRelatives);
            emit("Staging required engine resources (dependency-based, parallel)...\n");
            StageSelectedResources(engineRoot, stagedEngine, selectedEngineFiles, engineInventory, stagedEngineRelatives);

            if (!selectedMountFiles.empty())
            {
                emit("Staging referenced external mount assets (dependency-based, parallel)...\n");
                std::set<fs::path, PackageResourceInventory::PathLess> stagedMountRelatives;
                std::vector<FileCopyTask> mountTasks;
                std::unordered_set<std::wstring> mountDirs;
                for (const auto& [source, stagedRelative] : selectedMountFiles)
                {
                    auto key = stagedRelative;
                    key.make_preferred();
                    if (!stagedMountRelatives.insert(key).second)
                        continue;
                    if (!IsRegularNonReparseFile(source))
                        throw std::runtime_error("Selected external mount resource is missing or not a regular file: " + DisplayPath(source));
                    const auto target = stagedMounts / stagedRelative;
                    mountDirs.insert(target.parent_path().wstring());
                    mountTasks.push_back({ source, target, stagedRelative, fs::file_size(source) });
                }
                for (const auto& dir : mountDirs)
                    fs::create_directories(dir);
                CopyFileTasksParallel(mountTasks);
                for (const auto& task : mountTasks)
                    mountInventory.Record(task.Relative, task.Size);
            }

            CountExcludedRuntimeResources(assetRoot, stagedProjectRelatives, projectInventory, true);
            CountExcludedRuntimeResources(engineRoot, stagedEngineRelatives, engineInventory, false);
        }
        else
        {
            emit("Staging project assets (full scan, parallel)...\n");
            CopyRuntimeResources(assetRoot, stagedAssets, projectInventory, true);
            emit("Staging engine resources (full scan, parallel)...\n");
            CopyRuntimeResources(engineRoot, stagedEngine, engineInventory);
        }

        const auto ensureStaged = [&](const fs::path& source)
        {
            if (IsWithin(source, assetRoot))
            {
                const auto relative = RelativeToInput(source, assetRoot);
                const auto target = stagedAssets / relative;
                if (!fs::is_regular_file(target))
                {
                    CopyPackageFile(source, target);
                    projectInventory.Record(relative, fs::file_size(target));
                }
            }
            else if (IsWithin(source, engineRoot))
            {
                const auto relative = RelativeToInput(source, engineRoot);
                const auto target = stagedEngine / relative;
                if (!fs::is_regular_file(target))
                {
                    CopyPackageFile(source, target);
                    engineInventory.Record(relative, fs::file_size(target));
                }
            }
            else if (const auto mounted = TryResolveContentMountPath(source))
            {
                const auto target = stagedMounts / mounted->StagedRelative;
                if (!fs::is_regular_file(target))
                {
                    CopyPackageFile(source, target);
                    mountInventory.Record(mounted->StagedRelative, fs::file_size(target));
                }
            }
        };

        for (const auto& source : references)
            ensureStaged(source);
        for (const auto& source : startupPipeline.Files)
            ensureStaged(source);

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

        if (request.PackAssetsIntoBamPak)
        {
            emit("\n[BamPak Packaging]\n");
            ClientPakBuildOptions pakOptions;
            pakOptions.StagingDirectory = staging;
            pakOptions.StartupSceneUri = "Project://" + DisplayPath(startupSceneRelative);
            pakOptions.StartupManifestUri =
                std::string(startupPipeline.InEngine ? "Engine://" : "Project://") + DisplayPath(startupPipeline.RelativePath);
            if (!packageProfile.RelativePath.empty())
            {
                pakOptions.RenderPipelineProfileUri =
                    std::string(packageProfile.InEngine ? "Engine://" : "Project://") + DisplayPath(packageProfile.RelativePath);
            }

            for (const auto& relative : profileResources)
                pakOptions.RequiredEngineUris.push_back("Engine://" + DisplayPath(relative));
            for (const auto& file : startupPipeline.Files)
            {
                const bool inProject = IsWithin(file, assetRoot);
                const auto relative = RelativeToInput(file, inProject ? assetRoot : engineRoot);
                if (inProject)
                    pakOptions.RequiredProjectUris.push_back("Project://" + DisplayPath(relative));
                else
                    pakOptions.RequiredEngineUris.push_back("Engine://" + DisplayPath(relative));
            }
            for (const auto& uri : request.AdditionalScenes)
                pakOptions.RequiredProjectUris.push_back(uri);
            for (const auto& uri : request.AdditionalAssets)
                pakOptions.RequiredProjectUris.push_back(uri);
            for (const auto& source : references)
            {
                if (!IsWithin(source, assetRoot) && !IsWithin(source, engineRoot))
                {
                    if (const auto mounted = TryResolveContentMountPath(source))
                        pakOptions.RequiredProjectUris.push_back("Mount://" + DisplayPath(mounted->StagedRelative));
                }
            }

            pakOptions.HasInputActionsConfig = fs::is_regular_file(stagedInput);
            pakOptions.PrunePackedLooseFiles = true;

            const auto manifest = BuildClientPakArchives(pakOptions, emit);
            std::uint64_t totalEntries = 0;
            std::uint64_t totalBytes = 0;
            for (const auto& pak : manifest.Paks)
            {
                totalEntries += pak.EntryCount;
                totalBytes += pak.ByteSize;
            }
            emit(fmt::format("BamPak summary: {} archives, {} entries, {} bytes, integrity: {}\n",
                manifest.Paks.size(), totalEntries, totalBytes,
                manifest.IntegrityVerified ? "VERIFIED" : "FAILED"));
        }

        output += "\n[Package inventory]\n";
        output += request.OnlyReferencedAssets
            ? "Current collection policy: dependency-based selective packaging. Listed paths are examples.\n"
            : "Current collection policy: full runtime resource scan. Listed paths are examples.\n";
        projectInventory.Append("Project assets", output, request.OnlyReferencedAssets);
        engineInventory.Append("Engine resources", output, request.OnlyReferencedAssets);
        if (!Path::GetContentMounts().empty())
            mountInventory.Append("External mount assets", output, request.OnlyReferencedAssets);
        output += fmt::format("Runtime binaries: {} files / {} bytes\n", binaries.Count, binaries.Bytes);
        for (const auto& [sourcePath, targetName] : runtimeBinarySources)
        {
            output += "    " + DisplayPath(targetName) + '\n';
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
