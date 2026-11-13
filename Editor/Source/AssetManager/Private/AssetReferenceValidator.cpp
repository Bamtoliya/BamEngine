#include "AssetReferenceValidator.h"
#include "PlatformPath.h"
#include "AnimatorStateMachine.h"
#include "Logger.h"
#include <fstream>
#include <regex>
#include <unordered_set>
#include <map>
#include "Functions.h"
#include <glaze/glaze.hpp>
#include <set>
#include <string_view>

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

        void CollectBuildReferences(const glz::generic_u64& value, std::set<std::string>& references,
            std::set<std::string>& controllers)
        {
            using Json = glz::generic_u64;

            if (const auto* object = value.get_if<Json::object_t>())
            {
                const auto nullField = object->find("Null");
                if (nullField != object->end())
                {
                    const auto* isNull = nullField->second.get_if<bool>();
                    if (isNull && *isNull)
                        return;
                }

                const auto readString = [](const Json::object_t& fields, const char* key) -> const std::string*
                    {
                        const auto field = fields.find(key);
                        return field == fields.end() ? nullptr : field->second.get_if<std::string>();
                    };

                const auto* reference = readString(*object, "AssetPath");
                if (!reference || reference->empty())
                    reference = readString(*object, "AssetKey");

                if (reference && !reference->empty())
                    references.insert(*reference);

                const auto* type = readString(*object, "Type");
                const auto data = object->find("Data");
                if (type && *type == "Engine::AnimatorComponent" && data != object->end())
                {
                    if (const auto* fields = data->second.get_if<Json::object_t>())
                    {
                        if (const auto* controller = readString(*fields, "controllerPath"); controller && !controller->empty())
                            controllers.insert(*controller);
                    }
                }

                for (const auto& field : *object)
                    CollectBuildReferences(field.second, references, controllers);
            }
            else if (const auto* elements = value.get_if<Json::array_t>())
            {
                for (const auto& element : *elements)
                    CollectBuildReferences(element, references, controllers);
            }
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

    std::vector<AssetReferenceIssue> AssetReferenceValidator::ValidateBuildScene(const std::filesystem::path& scenePath,
        std::vector<std::filesystem::path>* referencedFiles)
    {
        namespace fs = std::filesystem;
        using Path = Engine::PlatformPath;
        using Issue = EAssetReferenceIssueType;

        static const std::set<std::string> builtInMeshes = {
            "QuadMesh", "CubeMesh", "SphereMesh", "PlaneMesh", "CylinderMesh", "CapsuleMesh"
        };

        std::vector<AssetReferenceIssue> issues;
        if (referencedFiles)
            referencedFiles->clear();
        const auto report = [&](const fs::path& source, const std::string& target, Issue type)
            {
                AssetReferenceIssue issue{};
                issue.SourceFile = source;
                issue.TargetReference = target;
                issue.IssueType = type;
                issues.push_back(std::move(issue));
            };

        std::error_code ec;
        const auto scene = fs::canonical(scenePath, ec);
        if (ec || !fs::is_regular_file(scene, ec))
        {
            report(scenePath, {}, Issue::UnreadableDocument);
            return issues;
        }

        if (!Path::IsProjectUri(Path::ToVirtualUri(scene)))
        {
            report(scene, {}, Issue::OutsideRuntimeRoots);
            return issues;
        }

        std::vector<fs::path> pending{ scene };
        std::set<fs::path> visited;

        while (!pending.empty())
        {
            const auto source = std::move(pending.back());
            pending.pop_back();
            if (!visited.insert(source).second)
                continue;

            std::string text;
            glz::generic_u64 document;
            if (ReadTextFile(source, text) && text.starts_with("\xEF\xBB\xBF"))
                text.erase(0, 3);

            if (text.empty() || glz::validate_json(text) || glz::read_json(document, text) || !document.is_object())
            {
                report(source, {}, Issue::UnreadableDocument);
                continue;
            }

            if (source == scene && !document.get_object().contains("Scene"))
            {
                report(source, {}, Issue::UnreadableDocument);
                continue;
            }

            std::set<std::string> references;
            std::set<std::string> controllers;
            CollectBuildReferences(document, references, controllers);
            references.insert(controllers.begin(), controllers.end());
            for (const auto& reference : references)
            {
                if (builtInMeshes.contains(reference))
                    continue;

                const auto rawPath = Engine::Utf8ToPath(reference);
                const auto wideReference = rawPath.wstring();
                if (!Path::IsVirtualUri(wideReference) && rawPath.has_root_path())
                {
                    report(source, reference, Issue::ExternalAbsolutePath);
                    continue;
                }

                const bool isController = controllers.contains(reference);
                auto resolved = isController ? Engine::AnimatorStateMachine::ResolveControllerPath(rawPath)
                    : Path::ResolveUri(wideReference);
                if (!isController && resolved.extension().wstring().starts_with(L".bam"))
                {
                    auto jsonPath = resolved;
                    jsonPath += L".json";
                    auto bevePath = resolved;
                    bevePath += L".beve";
                    if (fs::is_regular_file(jsonPath, ec))
                        resolved = jsonPath;
                    else if (fs::is_regular_file(bevePath, ec))
                        resolved = bevePath;
                }

                const auto file = fs::canonical(resolved, ec);
                if (ec || !fs::is_regular_file(file, ec))
                {
                    report(source, reference, Issue::MissingFile);
                    continue;
                }

                const auto uri = Path::ToVirtualUri(file);
                if (!Path::IsProjectUri(uri) && !Path::IsEngineUri(uri))
                {
                    report(source, reference, Issue::OutsideRuntimeRoots);
                    continue;
                }

                if (referencedFiles)
                    referencedFiles->push_back(file);
                const auto extension = Engine::ToLowerStr(Engine::WStrToStr(file.extension().wstring()));
                if (isController || extension == ".json" || extension == ".animator" || extension == ".bamanimator")
                    pending.push_back(file);
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
