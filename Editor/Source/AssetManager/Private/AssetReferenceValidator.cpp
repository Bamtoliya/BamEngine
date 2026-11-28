#include "AssetReferenceValidator.h"
#include "AssetReferenceTimings.h"
#include "PlatformPath.h"
#include "AnimatorStateMachine.h"
#include "GraphSystem.h"
#include "ResourceBinaryHeader.h"
#include "Logger.h"
#include <algorithm>
#include <chrono>
#include <iterator>
#include <fstream>
#include <sstream>
#include <utility>
#include <unordered_set>
#include <map>
#include "Functions.h"
#include <archive/ArchiveValue.h>
#include <glaze/beve.hpp>
#include <glaze/glaze.hpp>
#include <set>
#include <string_view>
#include <tuple>
#include <Windows.h>


namespace Editor
{
    namespace
    {
        // 내장 리소스 및 셰이더 내부 심볼릭 식별자 (코드 생성 메모리 전용 리소스 포함, 검증 제외 목록)
        bool IsBuiltinResourceIdentifier(const std::string& pathStr)
        {
            std::string normalized = pathStr;
            std::replace(normalized.begin(), normalized.end(), '\\', '/');
            const std::string lower = Engine::ToLowerStr(normalized);

            static const std::unordered_set<std::string> builtins = {
                "fullscreenquad", "fullscreenquadvs", "lightingps", "defaultps", "defaultvs",
                "spheremesh", "quadmesh", "cubemesh", "planemesh", "cylindermesh", "capsulemesh",
                "watergridmesh", "terraingridmesh", "splineteststrip", "__skymesh_sphere__",
                "engine://mesh/quad", "engine://texture/defaultbrdflut",
                "materialpreviewps", "clientpresentps", "viewportchannelps",
                "shadowdepthps", "shadowdepthvs", "shadowdepthskinningvs", "gbufferps", "gbuffervs",
                "gbufferskinningvs", "skyps", "skyvs", "terrainps", "terrainvs", "uips", "uivs",
                "resources/material/tonemappingmaterial", "resources/material/coloradjustmentsmaterial",
                "resources/material/vignettematerial", "resources/material/chromaticaberrationmaterial",
                "resources/material/filmgrainmaterial", "resources/material/lensdistortionmaterial",
                "resources/material/sharpenmaterial", "vfxpreviewgroundmatinst"
            };

            if (builtins.contains(lower)) return true;
            if (lower.starts_with("postprocess_") && !lower.ends_with(".cso") && !lower.ends_with(".hlsl") &&
                !lower.ends_with(".spv") && !lower.ends_with(".wgsl"))
                return true;
            if (lower.starts_with("liveshader/"))
                return true;
            if (lower.starts_with("resources/shader/") && !lower.ends_with(".cso") && !lower.ends_with(".hlsl") &&
                !lower.ends_with(".spv") && !lower.ends_with(".wgsl"))
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

        bool IsBeveReferenceDocumentPath(const std::filesystem::path& path)
        {
            const auto extension = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(path.extension().wstring()));
            return extension == ".bammatinst" || extension == ".bammat" || extension == ".bammodel" ||
                extension == ".bamsprite" || extension == ".bamterrainlayer" ||
                extension == ".bameffect" || extension == ".bamvfx" || extension == ".beve";
        }

        bool ReadReferenceDocument(const std::filesystem::path& source, ReferenceJson& document)
        {
            std::string text;
            if (!ReadTextFile(source, text))
                return false;
            if (IsBeveReferenceDocumentPath(source))
            {
                archive::detail::ArchiveValue beveValue;
                const auto error = glz::read_beve(beveValue, text);
                if (!error && error.count == text.size() && beveValue.is_object())
                {
                    std::string jsonText;
                    if (!glz::write_json(beveValue, jsonText) && ParseReferenceDocument(jsonText, document))
                        return true;
                }
            }
            return ParseReferenceDocument(text, document);
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
                        else if (*type == "Engine::TerrainComponent")
                            visitPath("DataPath", ReferenceKind::DirectFile);
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
            if (!tag) return;

            const auto visitPath = [&visit](auto& fields, const char* key, ReferenceKind kind = ReferenceKind::DirectFile)
            {
                const auto entry = fields.find(key);
                if (entry == fields.end()) return;
                const auto* path = entry->second.template get_if<std::string>();
                if (path && !path->empty()) visit(fields, key, kind);
            };

            if (*tag == static_cast<std::uint64_t>(Engine::EResourceType::Material) ||
                *tag == static_cast<std::uint64_t>(Engine::EResourceType::MaterialInstance))
            {
                visitPath(root, "m_ShaderGraphPath", ReferenceKind::Graph);
                return;
            }

            if (*tag == static_cast<std::uint64_t>(Engine::EResourceType::Shader))
            {
                visitPath(root, "m_DxilPath");
                visitPath(root, "m_SpirvPath");
                visitPath(root, "m_WgslPath");
                return;
            }

            if (*tag != static_cast<std::uint64_t>(Engine::EResourceType::ActionTimeline)) return;

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

        bool IsEmptyDirectory(const std::filesystem::path& path)
        {
            std::error_code error;
            if (!std::filesystem::is_directory(path, error) || error)
                return false;
            const std::filesystem::directory_iterator entries(path, error);
            return !error && entries == std::filesystem::directory_iterator{};
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

        void InspectProjectDocument(const std::filesystem::path& source, AssetReferenceScan& scan,
            AssetReferenceScanTiming* timing)
        {
            using Clock = std::chrono::steady_clock;
            using Milliseconds = std::chrono::duration<double, std::milli>;
            const auto started = timing ? Clock::now() : Clock::time_point{};
            Clock::time_point parsed, inspected;
            {
                ReferenceJson document;
                const bool read = ReadReferenceDocument(source, document);
                if (timing) parsed = Clock::now();

                if (read)
                    InspectReferenceDocument(source, document, scan);
                else
                    ReportUnreadableDocument(source, scan);
                if (timing) inspected = Clock::now();
            }

            if (timing)
            {
                ++timing->Documents;
                timing->ReadParseMs += Milliseconds((parsed - started) + (Clock::now() - inspected)).count();
                timing->ResolveMs += Milliseconds(inspected - parsed).count();
            }
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
                    replacements.emplace(issue.TargetReference, Engine::Unicode::WideToUtf8(issue.SuggestedFix.wstring()));
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
        InspectProjectDocument(error ? filePath : source, scan, nullptr);
        return std::move(scan.Issues);
    }

    std::vector<AssetReferenceIssue> AssetReferenceValidator::ValidateBuildScene(const std::filesystem::path& scenePath,
        std::vector<std::filesystem::path>* referencedFiles)
    {
        namespace fs = std::filesystem;
        using Path = Engine::PlatformPath;
        using Issue = EAssetReferenceIssueType;

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
        std::set<fs::path> collected;

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
                if (IsBuiltinResourceIdentifier(reference))
                    continue;

                const auto rawPath = Engine::Utf8ToPath(reference);
                if (!Path::IsVirtualUri(rawPath.wstring()) && rawPath.has_root_path())
                {
                    const auto virtualUri = Path::ToVirtualUri(rawPath);
                    if (!Path::IsProjectUri(virtualUri) && !Path::IsEngineUri(virtualUri) && !Path::IsMountUri(virtualUri))
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
                if (!Path::IsProjectUri(uri) && !Path::IsEngineUri(uri) && !Path::IsMountUri(uri))
                {
                    report(source, reference, Issue::OutsideRuntimeRoots);
                    continue;
                }

                if (referencedFiles && collected.insert(file).second)
                    referencedFiles->push_back(file);
                if (kind == ReferenceKind::AnimatorController || kind == ReferenceKind::Graph ||
                    IsReferenceDocumentPath(file) || IsBeveReferenceDocumentPath(file))
                    pending.push_back(file);
            }
        }

        return issues;
    }

    bool AssetReferenceValidator::IsReferenceDocumentPath(const std::filesystem::path& path)
    {
        const auto extension = Engine::ToLowerStr(Engine::Unicode::WideToUtf8(path.extension().wstring()));
        return extension == ".json" || extension == ".animator" || extension == ".bamanimator" ||
            extension == ".actiontimeline" || extension == ".bamgraph";
    }

    namespace
    {
        bool IsSaveStagingPath(const std::filesystem::path& path)
        {
            const auto matches = [](std::wstring_view name, std::wstring_view prefix)
            {
                if (!name.starts_with(prefix)) return false;
                const auto suffix = name.substr(prefix.size());
                const auto separator = suffix.find(L'-');
                if (separator == 0 || separator == suffix.npos || separator + 1 >= suffix.size()) return false;
                const auto isDigit = [](wchar_t ch) { return ch >= L'0' && ch <= L'9'; };
                return std::ranges::all_of(suffix.substr(0, separator), isDigit) &&
                    std::ranges::all_of(suffix.substr(separator + 1), isDigit);
            };
            const auto name = path.filename().wstring();
            const auto parent = path.parent_path().filename().wstring();
            return matches(name, L".archive-save-") || matches(name, L".bamgraph-save-") ||
                (name == L"document.tmp" && matches(parent, L".archive-save-")) ||
                (name == L"graph.tmp" && matches(parent, L".bamgraph-save-"));
        }
    }

    void AssetReferenceChanges::ObserveFile(std::filesystem::path path, bool inProject, bool membershipChanged)
    {
        if (RequiresFullScan) return;
        const bool isDocument = inProject && AssetReferenceValidator::IsReferenceDocumentPath(path);
        if ((!isDocument && !membershipChanged) || IsSaveStagingPath(path)) return;

        if (isDocument) AddDocument(std::move(path));
        else RequireFullScan();
    }

    void AssetReferenceChanges::RequireFullScan()
    {
        RequiresFullScan = true;
        Documents.clear();
    }

    void AssetReferenceChanges::AddDocument(std::filesystem::path path)
    {
        if (RequiresFullScan || std::ranges::find(Documents, path) != Documents.end()) return;
        constexpr std::size_t maxDocumentsPerRefresh = 64;
        if (Documents.size() == maxDocumentsPerRefresh)
        {
            RequireFullScan();
            return;
        }
        Documents.push_back(std::move(path));
    }

    void AssetReferenceChanges::Merge(AssetReferenceChanges changes)
    {
        if (changes.RequiresFullScan)
            RequireFullScan();
        else
            for (auto& path : changes.Documents)
                AddDocument(std::move(path));
    }

    AssetReferenceScan AssetReferenceValidator::ScanProject(AssetReferenceScanTiming* timing,
        AssetReferenceScanIndex* index)
    {
        namespace fs = std::filesystem;
        using Path = Engine::PlatformPath;
        using Clock = std::chrono::steady_clock;
        using Milliseconds = std::chrono::duration<double, std::milli>;
        AssetReferenceScan scan;
        if (timing) *timing = {};
        if (index) *index = {};

        const auto started = timing ? Clock::now() : Clock::time_point{};
        const auto finishTiming = [&]
        {
            if (!timing) return;
            const auto elapsedMs = Milliseconds(Clock::now() - started).count();
            timing->TraversalMs = (std::max)(0.0, elapsedMs - timing->ReadParseMs - timing->ResolveMs);
            timing->References = scan.References.size();
            timing->Issues = scan.Issues.size();
        };

        std::error_code error;
        const auto root = fs::canonical(Path::GetProjectAssetRoot(), error);
        if (error || !fs::is_directory(root, error))
        {
            ReportUnreadableDocument(Path::GetProjectAssetRoot(), scan);
            finishTiming();
            return scan;
        }

        if (index)
        {
            index->Root = root;
            index->ProjectRoot = Path::GetProjectRoot();
            index->EngineRoot = Path::GetEngineResourceRoot();
            index->EditorRoot = Path::GetVirtualSchemeRoot(L"Editor");
        }
        // Keep scanning other directories if one subtree cannot be listed.
        std::set<fs::path> visited{ root };
        std::vector<fs::path> pending{ root };
        while (!pending.empty())
        {
            auto directory = std::move(pending.back());
            pending.pop_back();
            if (timing) ++timing->Directories;
            for (auto entry = fs::directory_iterator(directory, error);
                !error && entry != fs::directory_iterator{}; entry.increment(error))
            {
                std::error_code entryError;
                const auto status = entry->symlink_status(entryError);
                if (entryError)
                {
                    if (index) index->Root.clear();
                    ReportUnreadableDocument(entry->path(), scan);
                    continue;
                }
                if (fs::is_symlink(status))
                    continue;

                const bool isDirectory = fs::is_directory(status);
                if (isDirectory)
                {
                    const auto dirName = entry->path().filename().wstring();
                    if (dirName == L".git" || dirName == L"Temp" || dirName == L"Logs" || dirName == L"out")
                        continue;
                }
                const bool isDocument = fs::is_regular_file(status) && IsReferenceDocumentPath(entry->path());
                if (!isDirectory && !isDocument)
                    continue;

                // directory is already canonical and symlinks are skipped above, so entry->path() is canonical.
                const auto& source = entry->path();
                if (!IsInAssetTree(source, root))
                    continue;
                if (isDirectory && visited.insert(source).second)
                    pending.push_back(source);
                if (isDocument)
                {
                    if (index) index->Documents.insert(source);
                    InspectProjectDocument(source, scan, timing);
                }
            }
            if (error)
            {
                if (index) index->Root.clear();
                ReportUnreadableDocument(directory, scan);
            }
        }
        finishTiming();
        return scan;
    }

    bool AssetReferenceValidator::RefreshProjectDocuments(const AssetReferenceScanIndex& index,
        std::span<const std::filesystem::path> paths, AssetReferenceScan& scan, AssetReferenceScanDelta& delta,
        AssetReferenceScanTiming* timing)
    {
        namespace fs = std::filesystem;
        using Path = Engine::PlatformPath;
        using Clock = std::chrono::steady_clock;
        using Milliseconds = std::chrono::duration<double, std::milli>;
        const auto started = timing ? Clock::now() : Clock::time_point{};
        if (timing) *timing = {};

        if (paths.empty() || index.Root.empty() || index.ProjectRoot != Path::GetProjectRoot() ||
            index.EngineRoot != Path::GetEngineResourceRoot() || index.EditorRoot != Path::GetVirtualSchemeRoot(L"Editor"))
            return false;

        std::error_code error;
        const auto root = fs::canonical(Path::GetProjectAssetRoot(), error);
        if (error || root != index.Root) return false;

        std::unordered_set<fs::path> sources, relativeSources;
        for (const auto& path : paths)
        {
            if (!IsReferenceDocumentPath(path) || !fs::is_regular_file(fs::symlink_status(path, error)) || error)
                return false;
            auto source = fs::canonical(path, error);
            if (error || !index.Documents.contains(source)) return false;
            relativeSources.insert(Path::ToProjectRelative(source));
            sources.insert(std::move(source));
        }
        if (timing) timing->TraversalMs = Milliseconds(Clock::now() - started).count();

        AssetReferenceScan refreshed;
        for (const auto& source : sources)
            InspectProjectDocument(source, refreshed, timing);

        const auto mergeStarted = timing ? Clock::now() : Clock::time_point{};
        delta = {};
        scan.Issues.reserve(scan.Issues.size() + refreshed.Issues.size());
        scan.References.reserve(scan.References.size() + refreshed.References.size());

        size_t keptIssues = 0;
        for (size_t index = 0; index < scan.Issues.size(); ++index)
        {
            auto& issue = scan.Issues[index];
            if (relativeSources.contains(issue.SourceFile))
            {
                delta.RemovedIssues.push_back({ index, issue.IssueType, issue.CanAutoFix });
                continue;
            }
            if (keptIssues != index)
                scan.Issues[keptIssues] = std::move(issue);
            ++keptIssues;
        }
        scan.Issues.resize(keptIssues);
        delta.FirstAddedIssue = keptIssues;
        scan.Issues.insert(scan.Issues.end(), std::make_move_iterator(refreshed.Issues.begin()),
            std::make_move_iterator(refreshed.Issues.end()));

        std::vector<AssetReferenceLink> previousLinks;
        std::erase_if(scan.References, [&](const AssetReferenceLink& link)
        {
            if (!sources.contains(link.Source))
                return false;
            previousLinks.push_back(link);
            return true;
        });
        const auto byEndpoints = [](const AssetReferenceLink& left, const AssetReferenceLink& right)
        {
            return std::tie(left.Source, left.Target) < std::tie(right.Source, right.Target);
        };
        std::ranges::sort(previousLinks, byEndpoints);
        std::ranges::sort(refreshed.References, byEndpoints);
        std::ranges::set_difference(previousLinks, refreshed.References,
            std::back_inserter(delta.RemovedReferences), byEndpoints);
        std::ranges::set_difference(refreshed.References, previousLinks,
            std::back_inserter(delta.AddedReferences), byEndpoints);
        scan.References.insert(scan.References.end(), std::make_move_iterator(refreshed.References.begin()),
            std::make_move_iterator(refreshed.References.end()));

        if (timing)
        {
            timing->RefreshViewMs = Milliseconds(Clock::now() - mergeStarted).count();
            timing->References = scan.References.size();
            timing->Issues = scan.Issues.size();
        }
        return true;
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
        if (includeInternal && impact.IsComplete &&
            std::ranges::all_of(affected, IsEmptyDirectory))
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
            const bool movesOnlyEmptyDirectories = !canonicalMoves.empty() &&
                std::ranges::all_of(canonicalMoves, [](const auto& move) { return IsEmptyDirectory(move.Before); });
            if (movesOnlyEmptyDirectories)
            {
                if (impact) impact->IsComplete = true;
                return true;
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
                    const auto uri = Engine::Unicode::WideToUtf8(Path::ToVirtualUri(target));
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
