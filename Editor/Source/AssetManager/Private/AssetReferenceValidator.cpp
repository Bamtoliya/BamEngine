#include "AssetReferenceValidator.h"
#include "PlatformPath.h"
#include "Logger.h"
#include <fstream>
#include <regex>
#include <unordered_set>
#include <map>

namespace Editor
{
    namespace
    {
        // 내장 리소스 및 셰이더 내부 심볼릭 식별자 (검증 제외 목록)
        bool IsBuiltinResourceIdentifier(const std::string& pathStr)
        {
            static const std::unordered_set<std::string> builtins = {
                "FullscreenQuad", "FullscreenQuadVS", "LightingPS", "DefaultPS", "DefaultVS",
                "SphereMesh", "QuadMesh", "CubeMesh", "PlaneMesh", "CylinderMesh", "CapsuleMesh", "__SkyMesh_Sphere__", "MaterialPreviewPS",
                "ShadowDepthPS", "ShadowDepthVS", "ShadowDepthSkinningVS", "GBufferPS", "GBufferVS",
                "GBufferSkinningVS", "SkyPS", "SkyVS", "TerrainPS", "TerrainVS", "UIPS", "UIVS"
            };

            if (builtins.contains(pathStr)) return true;
            if (pathStr.starts_with("Resources/Shader/") && !pathStr.ends_with(".cso") && !pathStr.ends_with(".hlsl") && !pathStr.ends_with(".spv"))
                return true;

            return false;
        }

        bool ReadTextFile(const std::filesystem::path& path, std::string& outContent)
        {
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) return false;
            std::ostringstream ss;
            ss << file.rdbuf();
            outContent = ss.str();
            return true;
        }

        bool WriteTextFileAtomic(const std::filesystem::path& targetPath, const std::string& content)
        {
            std::error_code ec;
            const auto tempPath = targetPath.string() + ".tmp_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
            {
                std::ofstream file(tempPath, std::ios::binary | std::ios::trunc);
                if (!file.is_open()) return false;
                file.write(content.data(), static_cast<std::streamsize>(content.size()));
                file.close();
                if (!file) { std::filesystem::remove(tempPath, ec); return false; }
            }
            std::filesystem::rename(tempPath, targetPath, ec);
            if (ec) { std::filesystem::remove(tempPath, ec); return false; }
            return true;
        }
    }

    bool AssetReferenceValidator::TryResolveFallbackPath(const std::string& brokenPath, std::filesystem::path& outResolvedPath)
    {
        if (brokenPath.empty()) return false;

        std::filesystem::path target(brokenPath);
        const auto root = Engine::PlatformPath::GetProjectRoot();

        // 1. 절대 경로인 경우 프로젝트 루트 하위면 상대 경로로 정규화
        if (target.is_absolute())
        {
            auto rel = Engine::PlatformPath::ToProjectRelative(target);
            if (std::filesystem::exists(root / rel))
            {
                outResolvedPath = rel;
                return true;
            }
        }

        // 2. 확장자가 없는 경우 (.bammat.json, .bammatinst.json, .bammesh.json 등 순차 탐색)
        if (!target.has_extension())
        {
            static const std::vector<std::string> candidateExts = {
                ".bammat.json", ".bammatinst.json", ".bammesh.json", ".bammodel.json", ".bamsprite.json", ".json"
            };
            for (const auto& ext : candidateExts)
            {
                std::filesystem::path candidate = brokenPath + ext;
                if (std::filesystem::exists(root / candidate))
                {
                    outResolvedPath = candidate;
                    return true;
                }
            }
        }

        // 3. 구형 .bammat / .bammatinst 바이너리 참조인 경우 최신 .json 원본 존재 여부 확인
        const std::string ext = target.extension().string();
        if (ext == ".bammat" || ext == ".bammatinst" || ext == ".bammesh" || ext == ".bammodel")
        {
            std::filesystem::path candidate = brokenPath + ".json";
            if (std::filesystem::exists(root / candidate))
            {
                outResolvedPath = candidate;
                return true;
            }
        }

        return false;
    }

    std::vector<AssetReferenceIssue> AssetReferenceValidator::ValidateFile(const std::filesystem::path& filePath)
    {
        std::vector<AssetReferenceIssue> issues;
        std::string content;
        if (!ReadTextFile(filePath, content)) return issues;

        const auto root = Engine::PlatformPath::GetProjectRoot();
        const auto relSource = Engine::PlatformPath::ToProjectRelative(filePath);

        // "AssetPath": "...", "AssetKey": "...", "Path": "..." 패턴 정규식 매칭
        static const std::regex pattern(R"(\"(AssetPath|AssetKey|Path)\"\s*:\s*\"([^\"]+)\")");
        auto begin = std::sregex_iterator(content.begin(), content.end(), pattern);
        auto end = std::sregex_iterator();

        for (auto it = begin; it != end; ++it)
        {
            const std::string propName = (*it)[1].str();
            const std::string refPathStr = (*it)[2].str();

            if (refPathStr.empty() || IsBuiltinResourceIdentifier(refPathStr)) continue;

            std::filesystem::path target(refPathStr);
            AssetReferenceIssue issue{};
            issue.SourceFile = relSource;
            issue.PropertyName = propName;
            issue.TargetReference = refPathStr;

            if (target.is_absolute())
            {
                // 외부 절대 경로 하드코딩 검출
                issue.IssueType = EAssetReferenceIssueType::ExternalAbsolutePath;
                issue.Description = "Hardcoded absolute path detected";
                if (TryResolveFallbackPath(refPathStr, issue.SuggestedFix))
                {
                    issue.CanAutoFix = true;
                }
                issues.push_back(std::move(issue));
            }
            else
            {
                const auto resolved = root / target;
                if (!std::filesystem::exists(resolved))
                {
                    // 누락 또는 확장자 결함 검출
                    if (!target.has_extension() || target.extension() == ".bammat" || target.extension() == ".bammatinst")
                    {
                        issue.IssueType = EAssetReferenceIssueType::InvalidExtension;
                        issue.Description = "Missing or legacy binary extension";
                    }
                    else
                    {
                        issue.IssueType = EAssetReferenceIssueType::MissingFile;
                        issue.Description = "Referenced file does not exist on disk";
                    }

                    if (TryResolveFallbackPath(refPathStr, issue.SuggestedFix))
                    {
                        issue.CanAutoFix = true;
                    }
                    issues.push_back(std::move(issue));
                }
            }
        }
        return issues;
    }

    std::vector<AssetReferenceIssue> AssetReferenceValidator::ValidateAllResources()
    {
        std::vector<AssetReferenceIssue> allIssues;
        const auto root = Engine::PlatformPath::GetProjectRoot();
        const auto resDir = root / "Resources";

        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(resDir, ec))
        {
            if (!entry.is_regular_file()) continue;

            const auto ext = entry.path().extension().string();
            const auto filename = entry.path().filename().string();

            // 씬, 프리팹, 머티리얼, 모델 JSON 파일 대상
            if (ext == ".json" || filename.ends_with(".bamscene.json") || filename.ends_with(".bamprefab.json") ||
                filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json") || filename.ends_with(".bammodel.json"))
            {
                auto fileIssues = ValidateFile(entry.path());
                allIssues.insert(allIssues.end(), fileIssues.begin(), fileIssues.end());
            }
        }
        return allIssues;
    }

    bool AssetReferenceValidator::FixIssue(const AssetReferenceIssue& issue)
    {
        if (!issue.CanAutoFix || issue.SuggestedFix.empty()) return false;

        const auto root = Engine::PlatformPath::GetProjectRoot();
        const auto fullSource = root / issue.SourceFile;

        std::string content;
        if (!ReadTextFile(fullSource, content)) return false;

        const std::string targetStr = "\"" + issue.TargetReference + "\"";
        const std::string replacementStr = "\"" + issue.SuggestedFix.generic_string() + "\"";

        size_t pos = 0;
        bool modified = false;
        while ((pos = content.find(targetStr, pos)) != std::string::npos)
        {
            content.replace(pos, targetStr.length(), replacementStr);
            pos += replacementStr.length();
            modified = true;
        }

        if (modified)
        {
            return WriteTextFileAtomic(fullSource, content);
        }
        return false;
    }

    size_t AssetReferenceValidator::FixAllSafeIssues(const std::vector<AssetReferenceIssue>& issues)
    {
        // 동일 파일 내 다중 참조 치환을 위해 파일별로 묶어 처리
        std::map<std::filesystem::path, std::vector<std::pair<std::string, std::string>>> fileReplacements;
        for (const auto& issue : issues)
        {
            if (issue.CanAutoFix && !issue.SuggestedFix.empty())
            {
                fileReplacements[issue.SourceFile].emplace_back(issue.TargetReference, issue.SuggestedFix.generic_string());
            }
        }

        size_t fixedCount = 0;
        const auto root = Engine::PlatformPath::GetProjectRoot();

        for (const auto& [srcRel, repls] : fileReplacements)
        {
            const auto fullSource = root / srcRel;
            std::string content;
            if (!ReadTextFile(fullSource, content)) continue;

            bool fileModified = false;
            for (const auto& [target, replacement] : repls)
            {
                const std::string targetStr = "\"" + target + "\"";
                const std::string replacementStr = "\"" + replacement + "\"";

                size_t pos = 0;
                while ((pos = content.find(targetStr, pos)) != std::string::npos)
                {
                    content.replace(pos, targetStr.length(), replacementStr);
                    pos += replacementStr.length();
                    fileModified = true;
                    ++fixedCount;
                }
            }

            if (fileModified)
            {
                WriteTextFileAtomic(fullSource, content);
            }
        }
        return fixedCount;
    }
}