#include "AssetReferenceValidator.h"
#include "PlatformPath.h"
#include "AnimatorStateMachine.h"
#include "GraphSystem.h"
#include "ResourceBinaryHeader.h"
#include "Logger.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>
#include <unordered_set>
#include <map>
#include "Functions.h"
#include <glaze/glaze.hpp>
#include <set>
#include <string_view>
#include <tuple>
#include <Windows.h>

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

        enum class ReferenceKind { Handle, AnimatorController, DirectFile, Graph };
        using ReferenceFields = std::map<std::pair<std::string, ReferenceKind>, const char*>;
        using ReferenceJson = glz::generic_u64;

        bool ParseReferenceDocument(std::string_view text, ReferenceJson& document)
        {
            if (text.starts_with("\xEF\xBB\xBF"))
                text.remove_prefix(3);
            return !text.empty() && !glz::validate_json(text) && !glz::read_json(document, text) && document.is_object();
        }

        bool ReadReferenceDocument(const std::filesystem::path& source, ReferenceJson& document)
        {
            std::string text;
            return ReadTextFile(source, text) && ParseReferenceDocument(text, document);
        }

        template<typename Value, typename Visitor>
        void VisitComponentAndHandleReferences(Value& value, const Visitor& visit)
        {
            using Json = glz::generic_u64;

            if (auto* object = value.template get_if<Json::object_t>())
            {
                const auto nullField = object->find("Null");
                if (nullField != object->end())
                {
                    const auto* isNull = nullField->second.template get_if<bool>();
                    if (isNull && *isNull)
                        return;
                }

                const auto readString = [](const auto& fields, const char* key) -> const std::string*
                    {
                        const auto field = fields.find(key);
                        return field == fields.end() ? nullptr : field->second.template get_if<std::string>();
                    };

                const auto* assetKey = readString(*object, "AssetKey");
                const auto* assetPath = readString(*object, "AssetPath");

                const std::string* reference = nullptr;
                if (assetKey && !assetKey->empty() && Engine::PlatformPath::IsVirtualUri(Engine::Utf8ToPath(*assetKey).wstring()))
                    reference = assetKey;
                else if (assetPath && !assetPath->empty())
                    reference = assetPath;
                else
                    reference = assetKey;

                if (reference && !reference->empty())
                    visit(*object, reference == assetKey ? "AssetKey" : "AssetPath", ReferenceKind::Handle);

                const auto* type = readString(*object, "Type");
                const auto data = object->find("Data");
                if (type && data != object->end())
                {
                    if (auto* fields = data->second.template get_if<Json::object_t>())
                    {
                        const auto visitPath = [&](const char* key, ReferenceKind kind)
                        {
                            if (const auto* path = readString(*fields, key); path && !path->empty())
                                visit(*fields, key, kind);
                        };
                        if (*type == "Engine::AnimatorComponent")
                            visitPath("controllerPath", ReferenceKind::AnimatorController);
                        else if (*type == "Engine::ActionTimelineComponent")
                            visitPath("timelinePath", ReferenceKind::DirectFile);
                        else if (*type == "Engine::PrefabInstanceComponent")
                            visitPath("prefabAssetPath", ReferenceKind::DirectFile);
                        else if (*type == "Engine::GraphBindingComponent")
                            visitPath("graphFile", ReferenceKind::Graph);
                    }
                }

                for (auto& field : *object)
                    VisitComponentAndHandleReferences(field.second, visit);
            }
            else if (auto* elements = value.template get_if<Json::array_t>())
            {
                for (auto& element : *elements)
                    VisitComponentAndHandleReferences(element, visit);
            }
        }

        template<typename Value, typename Visitor>
        void VisitDocumentReferences(Value& value, const Visitor& visit)
        {
            VisitComponentAndHandleReferences(value, visit);
            auto& root = value.get_object();
            const auto header = root.find("AssetHeader");
            if (header == root.end() || !header->second.is_object()) return;
            const auto& headerFields = header->second.get_object();
            const auto type = headerFields.find("ResourceType");
            const auto* tag = type == headerFields.end() ? nullptr : type->second.template get_if<std::uint64_t>();
            if (!tag || *tag != static_cast<std::uint64_t>(Engine::EResourceType::ActionTimeline)) return;

            const auto visitPath = [&visit](auto& fields, const char* key)
            {
                const auto entry = fields.find(key);
                if (entry == fields.end()) return;
                const auto* path = entry->second.template get_if<std::string>();
                if (path && !path->empty()) visit(fields, key, ReferenceKind::DirectFile);
            };
            visitPath(root, "m_ModelPath");

            const auto tracksField = root.find("m_NotifyTracks");
            if (tracksField == root.end()) return;
            auto* tracks = tracksField->second.template get_if<ReferenceJson::array_t>();
            if (!tracks) return;
            for (auto& track : *tracks)
            {
                auto* fields = track.template get_if<ReferenceJson::object_t>();
                if (!fields) continue;
                const auto notifiesField = fields->find("Notifies");
                if (notifiesField == fields->end()) continue;
                auto* notifies = notifiesField->second.template get_if<ReferenceJson::array_t>();
                if (!notifies) continue;
                for (auto& notify : *notifies)
                {
                    auto* notifyFields = notify.template get_if<ReferenceJson::object_t>();
                    if (!notifyFields) continue;
                    visitPath(*notifyFields, "SoundPath");
                    visitPath(*notifyFields, "VFXPath");
                }
            }
        }

        std::filesystem::path ResolveDocumentReference(const std::filesystem::path& rawPath, ReferenceKind kind)
        {
            using Path = Engine::PlatformPath;
            if (kind == ReferenceKind::Graph) return Engine::GraphSystem::ResolveGraphPath(rawPath);
            const auto wide = rawPath.wstring();
            auto resolved = rawPath;
            if (!rawPath.has_root_path() || Path::IsVirtualUri(wide))
            {
                resolved = kind == ReferenceKind::AnimatorController ? Engine::AnimatorStateMachine::ResolveControllerPath(rawPath)
                    : Path::ResolveUri(wide);
            }

            if (kind == ReferenceKind::Handle && resolved.extension().wstring().starts_with(L".bam"))
            {
                auto jsonPath = resolved;
                jsonPath += L".json";
                auto bevePath = resolved;
                bevePath += L".beve";
                std::error_code error;
                if (std::filesystem::is_regular_file(jsonPath, error))
                    return jsonPath;
                if (std::filesystem::is_regular_file(bevePath, error))
                    return bevePath;
                // A legacy relative alias can resolve to a missing ProjectRoot/Resources file.
                // Resolve the completed filename again so the Assets/Engine mounts can find it.
                if (!std::filesystem::is_regular_file(resolved, error))
                {
                    for (const auto* suffix : { L".json", L".beve" })
                    {
                        auto candidate = rawPath;
                        candidate += suffix;
                        candidate = Path::ResolveUri(candidate);
                        if (std::filesystem::is_regular_file(candidate, error))
                            return candidate;
                    }
                }
            }
            return resolved;
        }

        ReferenceFields ReadReferenceFields(const glz::generic_u64& document)
        {
            ReferenceFields references;
            VisitDocumentReferences(document, [&references](const auto& fields, const char* key, ReferenceKind kind)
            {
                const auto& reference = fields.at(key).get_string();
                references.try_emplace(std::pair{ reference, kind }, key);
            });
            return references;
        }

        bool IsInAssetTree(const std::filesystem::path& file, const std::filesystem::path& root)
        {
            const auto relative = file.lexically_relative(root);
            return !relative.empty() && !relative.has_root_path() && *relative.begin() != "..";
        }

        bool IsMountedReference(const std::filesystem::path& file)
        {
            using Path = Engine::PlatformPath;
            const auto uri = Path::ToVirtualUri(file);
            if (Path::IsVirtualUri(uri))
                return true;

            // 구형 프로젝트는 두 마운트가 같은 Resources 디렉터리를 사용할 수 있다.
            const auto& root = Path::GetProjectAssetRoot();
            if (root != Path::GetEngineResourceRoot() || root.empty())
                return false;
            return IsInAssetTree(Path::Normalize(file), Path::Normalize(root));
        }

        void ReportUnreadableDocument(const std::filesystem::path& source, AssetReferenceScan& scan)
        {
            AssetReferenceIssue issue;
            issue.SourceFile = Engine::PlatformPath::ToProjectRelative(source);
            issue.IssueType = EAssetReferenceIssueType::UnreadableDocument;
            scan.Issues.push_back(std::move(issue));
        }

        void InspectReferenceDocument(const std::filesystem::path& source, const ReferenceJson& document,
            AssetReferenceScan& scan)
        {
            using Path = Engine::PlatformPath;
            using Issue = EAssetReferenceIssueType;

            const auto relativeSource = Path::ToProjectRelative(source);
            std::set<std::filesystem::path> targets;
            for (const auto& [identity, property] : ReadReferenceFields(document))
            {
                const auto& [reference, kind] = identity;
                if (IsBuiltinResourceIdentifier(reference))
                    continue;

                AssetReferenceIssue issue;
                issue.SourceFile = relativeSource;
                issue.PropertyName = property;
                issue.TargetReference = reference;

                const auto rawPath = Engine::Utf8ToPath(reference);
                const bool hasUri = Path::IsVirtualUri(rawPath.wstring());
                if (!hasUri && rawPath.has_root_path() && !IsMountedReference(rawPath))
                {
                    issue.IssueType = Issue::ExternalAbsolutePath;
                }
                else
                {
                    const auto resolved = ResolveDocumentReference(rawPath, kind);
                    std::error_code error;
                    const auto target = std::filesystem::canonical(resolved, error);
                    if (!error && std::filesystem::is_regular_file(target, error))
                    {
                        if (IsMountedReference(target))
                        {
                            if (targets.insert(target).second)
                                scan.References.push_back({ source, target });
                            continue;
                        }
                        issue.IssueType = Issue::OutsideRuntimeRoots;
                    }
                    else
                    {
                        const auto ext = rawPath.extension();
                        issue.IssueType = kind == ReferenceKind::Handle && !hasUri && (!rawPath.has_extension() ||
                            ext == ".bammat" || ext == ".bammatinst") ? Issue::InvalidExtension : Issue::MissingFile;
                    }
                }

                if (!hasUri && kind == ReferenceKind::Handle && issue.IssueType != Issue::OutsideRuntimeRoots &&
                    IsInAssetTree(source, Path::Normalize(Path::GetProjectAssetRoot())) && !Path::IsReadOnlyTarget(source) &&
                    AssetReferenceValidator::TryResolveFallbackPath(reference, issue.SuggestedFix))
                {
                    issue.CanAutoFix = true;
                }
                scan.Issues.push_back(std::move(issue));
            }
        }

        void InspectProjectDocument(const std::filesystem::path& source, AssetReferenceScan& scan)
        {
            ReferenceJson document;
            if (ReadReferenceDocument(source, document))
                InspectReferenceDocument(source, document, scan);
            else
                ReportUnreadableDocument(source, scan);
        }

        using RepairRequests = std::map<std::string, const AssetReferenceIssue*>;

        size_t PrepareReferenceRepair(AssetReferenceEdit& edit, const RepairRequests& requests)
        {
            ReferenceJson document;
            if (!ReadTextFile(edit.BeforePath, edit.BeforeText) || !ParseReferenceDocument(edit.BeforeText, document))
                return 0;

            AssetReferenceScan current;
            InspectReferenceDocument(edit.BeforePath, document, current);
            std::map<std::string, std::string> replacements;
            for (const auto& issue : current.Issues)
            {
                const auto request = requests.find(issue.TargetReference);
                if (issue.CanAutoFix && request != requests.end() && request->second->IssueType == issue.IssueType &&
                    request->second->SuggestedFix == issue.SuggestedFix)
                {
                    replacements.emplace(issue.TargetReference, Engine::WStrToStr(issue.SuggestedFix.wstring()));
                }
            }
            if (replacements.empty())
                return 0;

            size_t count = 0;
            VisitDocumentReferences(document, [&](auto& fields, const char* key, ReferenceKind kind)
            {
                if (kind != ReferenceKind::Handle)
                    return;
                const auto replacement = replacements.find(fields.at(key).get_string());
                if (replacement == replacements.end())
                    return;

                for (const char* alias : { "AssetKey", "AssetPath" })
                {
                    const auto field = fields.find(alias);
                    if (field != fields.end())
                    {
                        const auto* value = field->second.template get_if<std::string>();
                        if (value && *value == replacement->first)
                            field->second = replacement->second;
                    }
                }
                ++count;
            });
            if (count == 0 || glz::write_json(document, edit.AfterText))
                return 0;
            if (edit.BeforeText.starts_with("\xEF\xBB\xBF"))
                edit.AfterText.insert(0, "\xEF\xBB\xBF");
            return count;
        }
    }

    bool AssetReferenceValidator::TryResolveFallbackPath(const std::string& brokenPath, std::filesystem::path& outResolvedPath)
    {
        namespace fs = std::filesystem;
        using Path = Engine::PlatformPath;
        if (brokenPath.empty())
            return false;

        const auto target = Engine::Utf8ToPath(brokenPath);
        const auto mountedFile = [](const fs::path& candidate) -> fs::path
        {
            std::error_code error;
            const auto file = fs::canonical(Path::ResolveUri(candidate), error);
            if (error || !fs::is_regular_file(file, error) || !IsMountedReference(file))
                return {};
            return file;
        };

        fs::path match;
        if (target.is_absolute())
            match = mountedFile(target);

        if (match.empty() && !target.has_extension())
        {
            for (const auto* extension : { L".bammat.json", L".bammatinst.json", L".bammesh.json",
                L".bammodel.json", L".bamsprite.json", L".json" })
            {
                auto candidate = target;
                candidate += extension;
                auto file = mountedFile(candidate);
                if (file.empty())
                    continue;
                if (!match.empty() && !Path::IsSamePath(match, file))
                    return false;
                match = std::move(file);
            }
        }

        if (match.empty())
        {
            const auto extension = target.extension().wstring();
            if (extension == L".bammat" || extension == L".bammatinst" ||
                extension == L".bammesh" || extension == L".bammodel")
            {
                auto candidate = target;
                candidate += L".json";
                match = mountedFile(candidate);
            }
        }

        if (match.empty())
            return false;
        const fs::path proposed(Path::ToVirtualUri(match));
        if (proposed == target)
            return false;
        outResolvedPath = proposed;
        return true;
    }

    std::vector<AssetReferenceIssue> AssetReferenceValidator::ValidateFile(const std::filesystem::path& filePath)
    {
        AssetReferenceScan scan;
        std::error_code error;
        const auto source = std::filesystem::canonical(filePath, error);
        InspectProjectDocument(error ? filePath : source, scan);
        return std::move(scan.Issues);
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

            glz::generic_u64 document;
            if (!ReadReferenceDocument(source, document))
            {
                report(source, {}, Issue::UnreadableDocument);
                continue;
            }

            if (source == scene && !document.get_object().contains("Scene"))
            {
                report(source, {}, Issue::UnreadableDocument);
                continue;
            }

            for (const auto& referenceField : ReadReferenceFields(document))
            {
                const auto& [reference, kind] = referenceField.first;
                if (builtInMeshes.contains(reference))
                    continue;

                const auto rawPath = Engine::Utf8ToPath(reference);
                if (!Path::IsVirtualUri(rawPath.wstring()) && rawPath.has_root_path())
                {
                    const auto virtualUri = Path::ToVirtualUri(rawPath);
                    if (!Path::IsProjectUri(virtualUri) && !Path::IsEngineUri(virtualUri))
                    {
                        report(source, reference, Issue::ExternalAbsolutePath);
                        continue;
                    }
                }

                const auto resolved = ResolveDocumentReference(rawPath, kind);
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
                if (kind == ReferenceKind::AnimatorController || IsReferenceDocumentPath(file))
                    pending.push_back(file);
            }
        }

        return issues;
    }

    bool AssetReferenceValidator::IsReferenceDocumentPath(const std::filesystem::path& path)
    {
        const auto extension = Engine::ToLowerStr(Engine::WStrToStr(path.extension().wstring()));
        return extension == ".json" || extension == ".animator" || extension == ".bamanimator" ||
            extension == ".actiontimeline";
    }

    AssetReferenceScan AssetReferenceValidator::ScanProject()
    {
        namespace fs = std::filesystem;
        using Path = Engine::PlatformPath;
        AssetReferenceScan scan;
        std::error_code error;
        const auto root = fs::canonical(Path::GetProjectAssetRoot(), error);
        if (error || !fs::is_directory(root, error))
        {
            ReportUnreadableDocument(Path::GetProjectAssetRoot(), scan);
            return scan;
        }

        // Keep scanning other directories if one subtree cannot be listed.
        std::set<fs::path> visited{ root };
        std::vector<fs::path> pending{ root };
        while (!pending.empty())
        {
            auto directory = std::move(pending.back());
            pending.pop_back();
            for (auto entry = fs::directory_iterator(directory, error);
                !error && entry != fs::directory_iterator{}; entry.increment(error))
            {
                std::error_code entryError;
                const auto status = entry->symlink_status(entryError);
                if (entryError)
                {
                    ReportUnreadableDocument(entry->path(), scan);
                    continue;
                }
                if (fs::is_symlink(status))
                    continue;

                const bool isDirectory = fs::is_directory(status);
                const bool isDocument = fs::is_regular_file(status) && IsReferenceDocumentPath(entry->path());
                if (!isDirectory && !isDocument)
                    continue;

                const auto source = fs::canonical(entry->path(), entryError);
                if (entryError)
                {
                    ReportUnreadableDocument(entry->path(), scan);
                    continue;
                }
                if (!IsInAssetTree(source, root))
                    continue;
                if (isDirectory && visited.insert(source).second)
                    pending.push_back(source);
                if (isDocument)
                    InspectProjectDocument(source, scan);
            }
            if (error)
                ReportUnreadableDocument(directory, scan);
        }
        return scan;
    }

    static AssetReferenceImpact AnalyzeFileChange(const std::vector<std::filesystem::path>& targets, bool includeInternal)
    {
        AssetReferenceImpact impact;
        std::vector<std::filesystem::path> affected;
        affected.reserve(targets.size());
        for (const auto& target : targets)
        {
            std::error_code error;
            auto resolved = std::filesystem::canonical(target, error);
            if (error)
                impact.IsComplete = false;
            else
                affected.push_back(std::move(resolved));
        }
        if (affected.empty())
            return impact;

        auto scan = AssetReferenceValidator::ScanProject();
        impact.IsComplete = impact.IsComplete && std::ranges::none_of(scan.Issues, [](const auto& issue)
        {
            return issue.IssueType == EAssetReferenceIssueType::UnreadableDocument;
        });

        const auto isAffected = [&affected](const auto& path)
        {
            return std::ranges::any_of(affected, [&path](const auto& root) { return IsInAssetTree(path, root); });
        };
        for (auto& link : scan.References)
        {
            if (isAffected(link.Target) && (includeInternal || !isAffected(link.Source)))
                impact.References.push_back(std::move(link));
        }
        std::ranges::sort(impact.References, [](const auto& lhs, const auto& rhs)
        {
            return std::tie(lhs.Source, lhs.Target) < std::tie(rhs.Source, rhs.Target);
        });
        return impact;
    }

    AssetReferenceImpact AssetReferenceValidator::AnalyzeDeletion(const std::vector<std::filesystem::path>& targets)
    {
        return AnalyzeFileChange(targets, false);
    }

    AssetReferenceImpact AssetReferenceValidator::AnalyzeMove(const std::vector<std::filesystem::path>& targets)
    {
        return AnalyzeFileChange(targets, true);
    }

    bool AssetReferenceValidator::PrepareMoveReferences(const std::vector<AssetPathMove>& moves,
        std::vector<AssetReferenceEdit>& edits, AssetReferenceImpact* impact)
    {
        namespace fs = std::filesystem;
        using Path = Engine::PlatformPath;
        edits.clear();
        if (impact) *impact = { {}, false };
        try
        {
            std::vector<AssetPathMove> canonicalMoves;
            const auto remap = [&canonicalMoves](const fs::path& path)
            {
                return Path::RemapFilePath(path, canonicalMoves);
            };

            for (const auto& move : moves)
            {
                const auto before = fs::canonical(move.Before);
                const auto after = Path::Normalize(move.After);
                if (!Path::IsProjectUri(Path::ToVirtualUri(before)) || Path::IsReadOnlyTarget(before) ||
                    !Path::IsProjectUri(Path::ToVirtualUri(after)) || Path::IsReadOnlyTarget(after))
                    return false;
                canonicalMoves.push_back({ before, after });
            }
            // Resolve casing and relative paths once, before using component-wise prefix matching.
            auto scan = ScanProject();
            const bool complete = std::ranges::none_of(scan.Issues, [](const auto& issue)
            {
                return issue.IssueType == EAssetReferenceIssueType::UnreadableDocument;
            });

            std::set<fs::path> documents;
            for (auto& link : scan.References)
            {
                if (remap(link.Target) == link.Target) continue;
                documents.insert(link.Source);
                if (impact) impact->References.push_back(std::move(link));
            }
            if (impact)
            {
                impact->IsComplete = complete;
                std::ranges::sort(impact->References, [](const auto& lhs, const auto& rhs)
                {
                    return std::tie(lhs.Source, lhs.Target) < std::tie(rhs.Source, rhs.Target);
                });
            }
            if (!complete) return false;
            std::vector<AssetReferenceEdit> prepared;
            for (const auto& source : documents)
            {
                AssetReferenceEdit edit{ source, remap(source), {}, {} };
                if (Path::IsReadOnlyTarget(source) || !ReadTextFile(source, edit.BeforeText))
                    return false;
                const bool bom = edit.BeforeText.starts_with("\xEF\xBB\xBF");
                ReferenceJson document;
                if (!ParseReferenceDocument(edit.BeforeText, document))
                    return false;
                bool changed = false;
                VisitDocumentReferences(document, [&](auto& fields, const char* key, ReferenceKind kind)
                {
                    const auto raw = Engine::Utf8ToPath(fields.at(key).get_string());
                    std::error_code error;
                    const auto resolved = fs::canonical(ResolveDocumentReference(raw, kind), error);
                    if (error)
                        return;
                    const auto target = remap(resolved);
                    if (target == resolved)
                        return;
                    const auto uri = Engine::WStrToStr(Path::ToVirtualUri(target));
                    fields.at(key) = uri;
                    if (kind == ReferenceKind::Handle)
                    {
                        // Keep the serialized handle's fallback path consistent with its preferred key.
                        for (const char* alias : { "AssetKey", "AssetPath" })
                        {
                            const auto field = fields.find(alias);
                            if (field != fields.end() && field->second.template get_if<std::string>())
                                field->second = uri;
                        }
                    }
                    changed = true;
                });
                if (changed)
                {
                    if (glz::write_json(document, edit.AfterText))
                        return false;
                    if (bom)
                        edit.AfterText.insert(0, "\xEF\xBB\xBF");
                    prepared.push_back(std::move(edit));
                }
            }
            edits = std::move(prepared);
            return true;
        }
        catch (const std::exception& error)
        {
            BAM_LOG(Warn, "Editor", "Asset", "Could not prepare moved references: {}", error.what());
            return false;
        }
    }

    std::vector<AssetReferenceIssue> AssetReferenceValidator::ValidateAllResources()
    {
        return ScanProject().Issues;
    }

    bool AssetReferenceValidator::FixIssue(const AssetReferenceIssue& issue)
    {
        return FixAllSafeIssues({ issue }) != 0;
    }

    size_t AssetReferenceValidator::FixAllSafeIssues(const std::vector<AssetReferenceIssue>& issues)
    {
        namespace fs = std::filesystem;
        using Path = Engine::PlatformPath;
        try
        {
            const auto root = Path::Normalize(Path::GetProjectAssetRoot());
            std::map<fs::path, RepairRequests> documents;
            for (const auto& issue : issues)
            {
                if (!issue.CanAutoFix || issue.SuggestedFix.empty())
                    continue;

                const auto source = Path::Normalize(Path::GetProjectRoot() / issue.SourceFile);
                if (IsInAssetTree(source, root) && !Path::IsReadOnlyTarget(source))
                    documents[source].try_emplace(issue.TargetReference, &issue);
            }

            size_t count = 0;
            std::vector<AssetReferenceEdit> edits;
            for (const auto& [source, requests] : documents)
            {
                AssetReferenceEdit edit{ source, source, {}, {} };
                if (const auto fixed = PrepareReferenceRepair(edit, requests); fixed != 0)
                {
                    count += fixed;
                    edits.push_back(std::move(edit));
                }
            }
            return ApplyReferenceRepairs(edits) ? count : 0;
        }
        catch (const std::exception& error)
        {
            BAM_LOG(Warn, "Editor", "Asset", "Could not prepare reference repairs: {}", error.what());
            return 0;
        }
    }
}
