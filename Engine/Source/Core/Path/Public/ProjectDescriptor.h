#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "Engine_API.h"
#include "PlatformPath.h"

namespace Engine
{
    struct ProjectMountEntry
    {
        std::string Name;
        std::string Path;
        bool ReadOnly = true;
        bool ShowUnderAssets = false;
    };

    struct ProjectModuleDesc
    {
        std::string Name;
        std::string Type = "Runtime";
    };

    enum class EProjectModuleRole : uint8_t
    {
        Editor,
        Client,
        Shipping
    };

    struct ProjectModuleBuildVariant
    {
        EProjectModuleRole Role = EProjectModuleRole::Client;
        bool Debug = false;
    };

    struct ResolvedProjectModule
    {
        std::string Name;
        std::string Type = "Runtime";
        std::filesystem::path SourceDirectory;
        std::filesystem::path CMakeSourceRoot;
        std::filesystem::path BuildDirectory;
        std::filesystem::path BinaryPath;
    };

    enum class EProjectTemplate : uint8_t
    {
        Empty = 0,
        Default3D = 1,
        Default2D = 2
    };

    struct ProjectDescriptor
    {
        static constexpr uint32_t CurrentSchemaVersion = 1;

        uint32_t SchemaVersion = CurrentSchemaVersion;
        std::string ProjectName = "Untitled";
        std::string EngineVersion = BAMEGINE_VERSION;
        std::string AssetDirectory = "Assets";
        std::vector<ProjectMountEntry> ContentMounts;
        std::vector<ProjectModuleDesc> Modules;

        // 프로젝트 루트 기준 에셋 루트 물리 경로 계산
        ENGINE_API std::filesystem::path ResolveAssetRoot(const std::filesystem::path& projectRoot) const;

        // ContentMounts의 상대/절대 경로를 물리 경로로 해석해 ContentMountDesc 목록으로 변환
        ENGINE_API std::vector<ContentMountDesc> ResolveContentMounts(const std::filesystem::path& projectRoot) const;

        // Modules의 소스/빌드/바이너리 물리 경로를 프로젝트 루트 기준으로 해석
        ENGINE_API std::vector<ResolvedProjectModule> ResolveModules(const std::filesystem::path& projectRoot) const;
        ENGINE_API std::vector<ResolvedProjectModule> ResolveModules(const std::filesystem::path& projectRoot, ProjectModuleBuildVariant variant) const;
        ENGINE_API std::optional<ResolvedProjectModule> ResolvePrimaryRuntimeModule(const std::filesystem::path& projectRoot) const;

        // .bamproject JSON 로드 / 저장 (Glaze 기반)
        ENGINE_API static bool LoadFromFile(const std::filesystem::path& filePath, ProjectDescriptor& outDescriptor,
            std::string* outError = nullptr);
        ENGINE_API static bool SaveToFile(const std::filesystem::path& filePath, const ProjectDescriptor& descriptor,
            std::string* outError = nullptr);

        // 디렉터리 또는 파일 경로에서 .bamproject 파일 경로 탐색
        ENGINE_API static std::filesystem::path FindProjectFile(const std::filesystem::path& directoryOrFile);

        // 독립 프로젝트 디렉터리 스캐폴딩 (.bamproject, Config/Project.json, Assets/Scenes/Main.bamscene.json, 선택적 C++ 모듈 생성)
        ENGINE_API static bool ScaffoldNewProject(const std::filesystem::path& projectDir,
            const std::string& projectName, std::filesystem::path* outProjectFile = nullptr,
            std::string* outError = nullptr, EProjectTemplate projectTemplate = EProjectTemplate::Empty,
            bool createCppModule = false);
    };
}
