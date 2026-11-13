#include "ClientPackage.h"

#include "AssetReferenceValidator.h"
#include "ClientBuildJob.h"
#include "Config.h"
#include "DeferredProfilePasses.h"
#include "DeferredProfileShaders.h"
#include "Functions.h"
#include "PlatformPath.h"

#include <glaze/glaze.hpp>
#include <windows.h>
#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
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

    void CopyRuntimeResources(const fs::path& sourceRoot, const fs::path& targetRoot)
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
                CopyPackageFile(source, targetRoot / source.lexically_relative(sourceRoot));
        }
    }

    struct PackageProfile
    {
        Engine::RenderPipelineProfile Profile;
        fs::path RelativePath;
        bool InEngine = false;
    };

    std::string MakePackageSettings(const fs::path& source, const fs::path& startupSceneRelative,
        const fs::path& assetRoot, const fs::path& engineRoot, PackageProfile& packageProfile)
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

    void VerifyRenderProfileResources(const PackageProfile& packageProfile, const fs::path& engineRoot)
    {
        if (packageProfile.RelativePath.empty())
            return;

        const auto require = [](const fs::path& path)
        {
            if (!fs::is_regular_file(path) || fs::file_size(path) == 0)
                throw std::runtime_error("Required render profile resource is missing or empty: " + DisplayPath(path));
        };
        for (const auto& shader : Engine::GetDeferredProfileShaders(packageProfile.Profile))
            require(engineRoot / "Shader" / "bin" / "dxil" / shader.DxilFile);
        require(engineRoot / "Material" / "UIMaterial.bammat.json");
    }
}

namespace Editor
{
    fs::path ResolveClientPackageDestination(const ClientPackageRequest& request)
    {
        if (request.OutputParent.empty() || request.StartupScene.empty())
            throw std::runtime_error("Select a startup scene and an export location.");

        const auto parent = fs::canonical(request.OutputParent);
        if (!fs::is_directory(parent))
            throw std::runtime_error("Export location is not a directory: " + DisplayPath(parent));

        const auto destination = parent / ClientPackageRequest::DirectoryName;
        if (fs::exists(fs::symlink_status(destination)))
            throw std::runtime_error("Export destination already exists: " + DisplayPath(destination));

        if (IsWithin(destination, fs::canonical(Path::GetProjectAssetRoot()))
            || IsWithin(destination, fs::canonical(Path::GetEngineResourceRoot())))
            throw std::runtime_error("Export destination overlaps runtime resource inputs: " + DisplayPath(destination));

        return destination;
    }

    void ExportClientPackage(const fs::path& projectRoot, const fs::path& binDirectory, const fs::path& startupScene,
        const fs::path& destination, std::string& output)
    {
        const auto assetRoot = fs::canonical(Path::GetProjectAssetRoot());
        const auto engineRoot = fs::canonical(Path::GetEngineResourceRoot());
        const auto scene = fs::canonical(startupScene);
        if (IsWithin(destination, assetRoot) || IsWithin(destination, engineRoot)
            || IsWithin(destination, fs::canonical(binDirectory)))
            throw std::runtime_error("Export destination overlaps package inputs: " + DisplayPath(destination));

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

        PackageProfile packageProfile;
        const auto settings = MakePackageSettings(projectRoot / "Config" / "Project.json",
            RelativeToInput(scene, assetRoot), assetRoot, engineRoot, packageProfile);
        VerifyRenderProfileResources(packageProfile, engineRoot);
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        auto staging = destination;
        staging += L".staging-" + std::to_wstring(suffix);
        if (!fs::create_directory(staging))
            throw std::runtime_error("Could not create a fresh staging folder: " + DisplayPath(staging));

        output += "Staging: " + DisplayPath(staging) + '\n';
        for (const auto* name : RuntimeBinaries)
            CopyPackageFile(binDirectory / name, staging / name);

        const auto stagedAssets = staging / "Assets";
        const auto stagedEngine = staging / "Engine" / "Resources";
        CopyRuntimeResources(assetRoot, stagedAssets);
        CopyRuntimeResources(engineRoot, stagedEngine);

        for (const auto& source : references)
        {
            const bool inProject = IsWithin(source, assetRoot);
            const auto relative = RelativeToInput(source, inProject ? assetRoot : engineRoot);
            const auto target = (inProject ? stagedAssets : stagedEngine) / relative;
            if (!fs::is_regular_file(target))
                CopyPackageFile(source, target);
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
            }

            Engine::RenderPipelineProfile reloaded;
            auto checked = Engine::LoadRenderPipelineProfile(stagedProfile, reloaded);
            if (checked)
                checked = Engine::ValidateDeferredProfile(reloaded);
            if (!checked)
                throw std::runtime_error("Staged render profile did not validate: " + DisplayPath(stagedProfile) + ": " + checked.Message);
            if (reloaded != packageProfile.Profile)
                throw std::runtime_error("The render profile changed during export: " + DisplayPath(stagedProfile));
            VerifyRenderProfileResources(packageProfile, stagedEngine);
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

        if (!MoveFileW(staging.c_str(), destination.c_str()))
            throw fs::filesystem_error("Could not publish the package", staging, destination,
                std::error_code(GetLastError(), std::system_category()));

        output += "Exported package: " + DisplayPath(destination);
    }
}
