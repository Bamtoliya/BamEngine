#include "ProjectDescriptor.h"
#include "Functions.h"
#include <glaze/glaze.hpp>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <system_error>

namespace Engine
{
    namespace ProjectDescriptorDetail
    {
        struct MountEntryDto
        {
            std::string Name{};
            std::string Path{};
            bool ReadOnly = true;
            bool ShowUnderAssets = false;
        };

        struct ModuleDescDto
        {
            std::string Name{};
            std::string Type = "Runtime";
        };

        struct ProjectDescriptorDto
        {
            uint32_t SchemaVersion = ProjectDescriptor::CurrentSchemaVersion;
            std::string ProjectName = "Untitled";
            std::string EngineVersion = BAMEGINE_VERSION;
            std::string AssetDirectory = "Assets";
            std::vector<MountEntryDto> ContentMounts{};
            std::vector<ModuleDescDto> Modules{};
        };

        struct DefaultProjectSettingsInnerDto
        {
            std::string AssetDirectory = "Assets";
            std::string EngineResourceDirectory = "Engine/Resources";
            std::string ProjectName = "Untitled";
            std::string RenderPipelineProfilePath = "Engine://RenderPipelines/Deferred.bamrenderpipeline.json";
            int32_t ResourceStreamingBudgetMB = 512;
            uint32_t SchemaVersion = 1;
            std::string ShaderSourceDirectory = "Engine/Resources/Shader/hlsl";
            std::string StartupManifestPath = "Config/Startup/StartupManifest.json";
            std::string StartupScenePath = "Project://Scenes/Main.bamscene.json";
            std::string WindowTitle{};
        };

        struct DefaultProjectSettingsFileDto
        {
            DefaultProjectSettingsInnerDto Settings{};
        };

        void SetError(std::string* outError, const std::string& message)
        {
            if (outError) *outError = message;
        }

        bool IsValidProjectFileStem(std::string_view name)
        {
            if (name.empty() || name == "." || name == "..") return false;
            bool hasNonSpace = false;
            for (char ch : name)
            {
                if (ch == '/' || ch == '\\' || ch == ':' || ch == '*' || ch == '?' || ch == '"' || ch == '<' || ch == '>' || ch == '|')
                    return false;
                if (ch != ' ' && ch != '\t' && ch != '\r' && ch != '\n')
                    hasNonSpace = true;
            }
            return hasNonSpace;
        }

        std::string TrimWhitespace(std::string_view text)
        {
            const size_t first = text.find_first_not_of(" \t\r\n");
            if (first == std::string_view::npos) return {};
            const size_t last = text.find_last_not_of(" \t\r\n");
            return std::string(text.substr(first, last - first + 1));
        }

        std::string BuildDefaultModuleName(std::string_view projectName)
        {
            std::string sanitized;
            sanitized.reserve(projectName.size() + 8);
            for (unsigned char ch : projectName)
            {
                if (std::isalnum(ch) || ch == '_')
                    sanitized.push_back(static_cast<char>(ch));
            }
            if (sanitized.empty())
                sanitized = "Game";
            else if (std::isdigit(static_cast<unsigned char>(sanitized.front())))
                sanitized.insert(0, "Game");
            if (!sanitized.ends_with("Module"))
                sanitized += "Module";
            return sanitized;
        }

        bool WriteUtf8File(const std::filesystem::path& filePath, std::string_view content, std::string* outError)
        {
            std::error_code ec;
            const auto parent = filePath.parent_path();
            if (!parent.empty())
            {
                std::filesystem::create_directories(parent, ec);
                if (ec)
                {
                    SetError(outError, "Failed to create parent directory: " + ec.message());
                    return false;
                }
            }

            std::ofstream ofs(filePath, std::ios::binary | std::ios::trunc);
            if (!ofs.is_open())
            {
                SetError(outError, "Failed to open file for writing: " + WStrToStr(filePath.wstring()));
                return false;
            }

            ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
            if (!ofs.good())
            {
                SetError(outError, "Failed to write file: " + WStrToStr(filePath.wstring()));
                return false;
            }
            return true;
        }
    }

    std::filesystem::path ProjectDescriptor::ResolveAssetRoot(const std::filesystem::path& projectRoot) const
    {
        const std::filesystem::path assetRel = AssetDirectory.empty() ? std::filesystem::path("Assets") : Utf8ToPath(AssetDirectory);
        if (assetRel.is_absolute()) return PlatformPath::Normalize(assetRel);
        return PlatformPath::Normalize(projectRoot / assetRel);
    }

    std::vector<ContentMountDesc> ProjectDescriptor::ResolveContentMounts(const std::filesystem::path& projectRoot) const
    {
        std::vector<ContentMountDesc> result;
        result.reserve(ContentMounts.size());
        for (const auto& entry : ContentMounts)
        {
            if (entry.Name.empty() || entry.Path.empty()) continue;

            std::filesystem::path mountPath = Utf8ToPath(entry.Path);
            if (mountPath.is_relative())
            {
                mountPath = projectRoot / mountPath;
            }

            ContentMountDesc desc;
            desc.Name = entry.Name;
            desc.RootPath = PlatformPath::Normalize(mountPath);
            desc.ReadOnly = entry.ReadOnly;
            desc.ShowUnderAssets = entry.ShowUnderAssets;
            result.push_back(std::move(desc));
        }
        return result;
    }

    std::vector<ResolvedProjectModule> ProjectDescriptor::ResolveModules(const std::filesystem::path& projectRoot) const
    {
        std::vector<ResolvedProjectModule> result;
        if (projectRoot.empty() || Modules.empty())
            return result;

        const std::filesystem::path normRoot = PlatformPath::Normalize(projectRoot);
        const std::filesystem::path cmakeSourceRoot = PlatformPath::Normalize(normRoot / "Source");
        const std::filesystem::path binariesDir = PlatformPath::Normalize(normRoot / "Binaries" / "Win64");

#ifdef _DEBUG
        constexpr const char* kConfigPreset = "x64-debug";
#else
        constexpr const char* kConfigPreset = "x64-release";
#endif

        std::error_code ec;
        std::filesystem::path buildDir;
        if (std::filesystem::is_regular_file(normRoot / "Build" / "CMakeCache.txt", ec))
            buildDir = PlatformPath::Normalize(normRoot / "Build");
        else if (std::filesystem::is_regular_file(normRoot / "out" / "build" / kConfigPreset / "CMakeCache.txt", ec))
            buildDir = PlatformPath::Normalize(normRoot / "out" / "build" / kConfigPreset);
        else
            buildDir = PlatformPath::Normalize(normRoot / "Build" / kConfigPreset);

        result.reserve(Modules.size());
        for (const auto& entry : Modules)
        {
            if (entry.Name.empty()) continue;

            ResolvedProjectModule resolved;
            resolved.Name = entry.Name;
            resolved.Type = entry.Type.empty() ? std::string("Runtime") : entry.Type;
            resolved.CMakeSourceRoot = cmakeSourceRoot;
            resolved.BuildDirectory = buildDir;

            const std::filesystem::path moduleSubDir = PlatformPath::Normalize(cmakeSourceRoot / Utf8ToPath(entry.Name));
            if (std::filesystem::is_directory(moduleSubDir, ec) || !std::filesystem::is_directory(cmakeSourceRoot, ec))
                resolved.SourceDirectory = moduleSubDir;
            else
                resolved.SourceDirectory = cmakeSourceRoot;

            resolved.BinaryPath = PlatformPath::Normalize(binariesDir / (Utf8ToPath(entry.Name).wstring() + L".dll"));
            result.push_back(std::move(resolved));
        }

        return result;
    }

    std::optional<ResolvedProjectModule> ProjectDescriptor::ResolvePrimaryRuntimeModule(const std::filesystem::path& projectRoot) const
    {
        const auto resolved = ResolveModules(projectRoot);
        for (const auto& mod : resolved)
        {
            std::string lowerType = mod.Type;
            std::transform(lowerType.begin(), lowerType.end(), lowerType.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lowerType.empty() || lowerType == "runtime")
                return mod;
        }
        return std::nullopt;
    }

    bool ProjectDescriptor::LoadFromFile(const std::filesystem::path& filePath, ProjectDescriptor& outDescriptor,
        std::string* outError)
    {
        std::error_code ec;
        if (filePath.empty() || !std::filesystem::is_regular_file(filePath, ec))
        {
            ProjectDescriptorDetail::SetError(outError, "Project descriptor file does not exist.");
            return false;
        }

        std::ifstream ifs(filePath, std::ios::binary);
        if (!ifs.is_open())
        {
            ProjectDescriptorDetail::SetError(outError, "Failed to open project descriptor file.");
            return false;
        }

        std::ostringstream buffer;
        buffer << ifs.rdbuf();
        const std::string json = buffer.str();
        if (json.empty())
        {
            ProjectDescriptorDetail::SetError(outError, "Project descriptor file is empty.");
            return false;
        }

        ProjectDescriptorDetail::ProjectDescriptorDto dto;
        const auto parseEc = glz::read<glz::opts{ .error_on_unknown_keys = false }>(dto, json);
        if (parseEc)
        {
            ProjectDescriptorDetail::SetError(outError, glz::format_error(parseEc, json));
            return false;
        }

        if (dto.SchemaVersion == 0 || dto.SchemaVersion > CurrentSchemaVersion)
        {
            ProjectDescriptorDetail::SetError(outError, "Unsupported project descriptor SchemaVersion.");
            return false;
        }

        ProjectDescriptor loaded;
        loaded.SchemaVersion = dto.SchemaVersion;
        loaded.ProjectName = dto.ProjectName.empty() ? WStrToStr(filePath.stem().wstring()) : std::move(dto.ProjectName);
        loaded.EngineVersion = dto.EngineVersion.empty() ? std::string(BAMEGINE_VERSION) : std::move(dto.EngineVersion);
        loaded.AssetDirectory = dto.AssetDirectory.empty() ? std::string("Assets") : std::move(dto.AssetDirectory);

        loaded.ContentMounts.reserve(dto.ContentMounts.size());
        for (auto& mountDto : dto.ContentMounts)
        {
            if (mountDto.Name.empty() || mountDto.Path.empty()) continue;
            ProjectMountEntry entry;
            entry.Name = std::move(mountDto.Name);
            entry.Path = std::move(mountDto.Path);
            entry.ReadOnly = mountDto.ReadOnly;
            entry.ShowUnderAssets = mountDto.ShowUnderAssets;
            loaded.ContentMounts.push_back(std::move(entry));
        }

        loaded.Modules.reserve(dto.Modules.size());
        for (auto& modDto : dto.Modules)
        {
            if (modDto.Name.empty()) continue;
            ProjectModuleDesc mod;
            mod.Name = std::move(modDto.Name);
            mod.Type = modDto.Type.empty() ? std::string("Runtime") : std::move(modDto.Type);
            loaded.Modules.push_back(std::move(mod));
        }

        outDescriptor = std::move(loaded);
        return true;
    }

    bool ProjectDescriptor::SaveToFile(const std::filesystem::path& filePath, const ProjectDescriptor& descriptor,
        std::string* outError)
    {
        if (filePath.empty())
        {
            ProjectDescriptorDetail::SetError(outError, "Project descriptor output path is empty.");
            return false;
        }

        ProjectDescriptorDetail::ProjectDescriptorDto dto;
        dto.SchemaVersion = descriptor.SchemaVersion == 0 ? CurrentSchemaVersion : descriptor.SchemaVersion;
        dto.ProjectName = descriptor.ProjectName.empty() ? std::string("Untitled") : descriptor.ProjectName;
        dto.EngineVersion = descriptor.EngineVersion.empty() ? std::string(BAMEGINE_VERSION) : descriptor.EngineVersion;
        dto.AssetDirectory = descriptor.AssetDirectory.empty() ? std::string("Assets") : descriptor.AssetDirectory;

        dto.ContentMounts.reserve(descriptor.ContentMounts.size());
        for (const auto& entry : descriptor.ContentMounts)
        {
            ProjectDescriptorDetail::MountEntryDto mountDto;
            mountDto.Name = entry.Name;
            mountDto.Path = entry.Path;
            mountDto.ReadOnly = entry.ReadOnly;
            mountDto.ShowUnderAssets = entry.ShowUnderAssets;
            dto.ContentMounts.push_back(std::move(mountDto));
        }

        dto.Modules.reserve(descriptor.Modules.size());
        for (const auto& mod : descriptor.Modules)
        {
            ProjectDescriptorDetail::ModuleDescDto modDto;
            modDto.Name = mod.Name;
            modDto.Type = mod.Type.empty() ? std::string("Runtime") : mod.Type;
            dto.Modules.push_back(std::move(modDto));
        }

        std::string outJson;
        const auto writeEc = glz::write<glz::opts{ .prettify = true, .indentation_width = 4 }>(dto, outJson);
        if (writeEc)
        {
            ProjectDescriptorDetail::SetError(outError, "Failed to serialize project descriptor to JSON.");
            return false;
        }

        return ProjectDescriptorDetail::WriteUtf8File(filePath, outJson, outError);
    }

    std::filesystem::path ProjectDescriptor::FindProjectFile(const std::filesystem::path& directoryOrFile)
    {
        if (directoryOrFile.empty()) return {};

        std::error_code ec;
        if (std::filesystem::is_regular_file(directoryOrFile, ec))
        {
            if (ToLowerWStr(directoryOrFile.extension().wstring()) == L".bamproject")
                return PlatformPath::Normalize(directoryOrFile);
            return {};
        }

        if (!std::filesystem::is_directory(directoryOrFile, ec))
            return {};

        std::vector<std::filesystem::path> matches;
        for (const auto& entry : std::filesystem::directory_iterator(directoryOrFile, ec))
        {
            if (!entry.is_regular_file(ec)) continue;
            if (ToLowerWStr(entry.path().extension().wstring()) == L".bamproject")
            {
                matches.push_back(PlatformPath::Normalize(entry.path()));
            }
        }

        if (matches.empty()) return {};
        std::sort(matches.begin(), matches.end());
        return matches.front();
    }

    bool ProjectDescriptor::ScaffoldNewProject(const std::filesystem::path& projectDir, const std::string& projectName,
        std::filesystem::path* outProjectFile, std::string* outError, EProjectTemplate projectTemplate, bool createCppModule)
    {
        if (projectDir.empty())
        {
            ProjectDescriptorDetail::SetError(outError, "Target project directory is empty.");
            return false;
        }

        const std::string trimmedName = ProjectDescriptorDetail::TrimWhitespace(projectName);
        if (!ProjectDescriptorDetail::IsValidProjectFileStem(trimmedName))
        {
            ProjectDescriptorDetail::SetError(outError, "Invalid project name.");
            return false;
        }

        const std::filesystem::path normDir = PlatformPath::Normalize(projectDir);
        const std::filesystem::path configDir = normDir / "Config";
        const std::filesystem::path assetsDir = normDir / "Assets";
        const std::filesystem::path scenesDir = assetsDir / "Scenes";
        const std::filesystem::path savedDir = normDir / "Saved";

        std::error_code ec;
        std::filesystem::create_directories(configDir, ec);
        if (ec)
        {
            ProjectDescriptorDetail::SetError(outError, "Failed to create Config directory: " + ec.message());
            return false;
        }
        std::filesystem::create_directories(scenesDir, ec);
        std::filesystem::create_directories(assetsDir / "Materials", ec);
        std::filesystem::create_directories(assetsDir / "Models", ec);
        std::filesystem::create_directories(assetsDir / "Textures", ec);
        std::filesystem::create_directories(savedDir, ec);
        if (ec)
        {
            ProjectDescriptorDetail::SetError(outError, "Failed to create project asset directories: " + ec.message());
            return false;
        }

        const std::string moduleName = createCppModule ? ProjectDescriptorDetail::BuildDefaultModuleName(trimmedName) : std::string{};

        // 1. Write <ProjectName>.bamproject
        ProjectDescriptor desc;
        desc.SchemaVersion = CurrentSchemaVersion;
        desc.ProjectName = trimmedName;
        desc.EngineVersion = BAMEGINE_VERSION;
        desc.AssetDirectory = "Assets";
        if (createCppModule)
            desc.Modules.push_back({ moduleName, "Runtime" });

        const std::filesystem::path bamProjectPath = PlatformPath::Normalize(normDir / (Utf8ToPath(trimmedName).wstring() + L".bamproject"));
        if (!SaveToFile(bamProjectPath, desc, outError))
        {
            return false;
        }

        // 2. Write Config/Project.json
        (void)projectTemplate;
        ProjectDescriptorDetail::DefaultProjectSettingsFileDto settingsFile;
        settingsFile.Settings.ProjectName = trimmedName;
        settingsFile.Settings.WindowTitle = trimmedName;
        settingsFile.Settings.AssetDirectory = "Assets";
        settingsFile.Settings.EngineResourceDirectory = "Engine/Resources";
        settingsFile.Settings.StartupScenePath = "Project://Scenes/Main.bamscene.json";
        settingsFile.Settings.RenderPipelineProfilePath = "Engine://RenderPipelines/Deferred.bamrenderpipeline.json";

        std::string settingsJson;
        const auto settingsEc = glz::write<glz::opts{ .prettify = true, .indentation_width = 4 }>(settingsFile, settingsJson);
        if (settingsEc)
        {
            ProjectDescriptorDetail::SetError(outError, "Failed to serialize default Config/Project.json.");
            return false;
        }

        if (!ProjectDescriptorDetail::WriteUtf8File(configDir / "Project.json", settingsJson, outError))
        {
            return false;
        }

        // 3. Write Assets/Scenes/Main.bamscene.json
        constexpr std::string_view kEmptySceneJson = "{\n    \"Scene\": {\n        \"Entities\": []\n    }\n}\n";
        if (!ProjectDescriptorDetail::WriteUtf8File(scenesDir / "Main.bamscene.json", kEmptySceneJson, outError))
        {
            return false;
        }

        // 4. Optional Phase 5 C++ module scaffolding (Source/CMakeLists.txt + Source/<ModuleName>/...)
        if (createCppModule)
        {
            const std::filesystem::path sourceDir = normDir / "Source";
            const std::filesystem::path moduleDir = sourceDir / Utf8ToPath(moduleName);

            const std::string rootCMake =
                "cmake_minimum_required(VERSION 3.24)\n"
                "project(" + moduleName + "Project LANGUAGES CXX)\n"
                "set(CMAKE_CXX_STANDARD 20)\n"
                "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n\n"
                "if(NOT DEFINED BAMENGINE_ROOT AND DEFINED ENV{BAMENGINE_ROOT})\n"
                "    set(BAMENGINE_ROOT \"$ENV{BAMENGINE_ROOT}\")\n"
                "endif()\n\n"
                "find_package(BamEngine REQUIRED PATHS \"${BAMENGINE_ROOT}/Engine/CMake\")\n"
                "add_subdirectory(" + moduleName + ")\n";

            if (!ProjectDescriptorDetail::WriteUtf8File(sourceDir / "CMakeLists.txt", rootCMake, outError))
                return false;

            const std::string moduleCMake =
                "add_library(" + moduleName + " SHARED\n"
                "    " + moduleName + ".cpp\n"
                ")\n"
                "bamengine_configure_game_module(" + moduleName + " \"${CMAKE_CURRENT_SOURCE_DIR}/../..\")\n";

            if (!ProjectDescriptorDetail::WriteUtf8File(moduleDir / "CMakeLists.txt", moduleCMake, outError))
                return false;

            const std::string moduleCpp =
                "#include \"ModuleInterface.h\"\n"
                "#include \"Logger.h\"\n\n"
                "class " + moduleName + " final : public Engine::IModule\n"
                "{\n"
                "public:\n"
                "    void OnLoad() override\n"
                "    {\n"
                "        BAM_LOG(Info, \"" + moduleName + "\", \"Lifecycle\", \"[" + moduleName + "] Loaded.\");\n"
                "    }\n\n"
                "    void OnUnload() override\n"
                "    {\n"
                "        BAM_LOG(Info, \"" + moduleName + "\", \"Lifecycle\", \"[" + moduleName + "] Unloaded.\");\n"
                "    }\n"
                "};\n\n"
                "IMPLEMENT_MODULE(" + moduleName + ")\n";

            if (!ProjectDescriptorDetail::WriteUtf8File(moduleDir / (Utf8ToPath(moduleName).wstring() + L".cpp"), moduleCpp, outError))
                return false;
        }

        if (outProjectFile) *outProjectFile = bamProjectPath;
        return true;
    }
}
