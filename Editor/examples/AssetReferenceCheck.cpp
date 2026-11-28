#include "AssetReferenceValidator.h"
#include "AssetReferenceTimings.h"
#include "ContentBrowserRename.h"
#include "PlatformPath.h"
#include "Engine_Includes.h"
#include "ResourceManager.h"
#include "AssetPathRelocation.h"
#include "ActionTimelineAsset.h"
#include "ReflectionRegistry.h"
#include "GraphFile.h"
#include "GraphSystem.h"
#include "GraphBindingComponent.h"
#include "PrefabInstanceComponent.h"
#include "SceneManager.h"
#include "Entity.h"
#include <archive/JsonArchive.h>

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <Windows.h>

namespace
{
    namespace fs = std::filesystem;

    struct Fixture
    {
        fs::path Root;

        ~Fixture()
        {
            std::error_code error;
            if (!Root.empty())
                fs::remove_all(Root, error);
        }
    };

    // 문자열 리터럴과 std::string을 모두 받도록 string_view를 사용한다
    bool WriteFixture(const fs::path& path, std::string_view text)
    {
        std::ofstream file(path, std::ios::binary);
        file << text;
        file.close();
        return !!file;
    }

    std::string ReadFixture(const fs::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        return { std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
    }

    bool ExpectValidBuildScene(const fs::path& scene)
    {
        const auto issues = Editor::AssetReferenceValidator::ValidateBuildScene(scene);
        for (const auto& issue : issues)
            std::cerr << "Reference issue: type=" << static_cast<int>(issue.IssueType)
                << ", source=" << Engine::Unicode::WideToUtf8(issue.SourceFile.wstring())
                << ", target=" << issue.TargetReference << '\n';
        return issues.empty();
    }

    bool CheckReferenceTimings()
    {
        using enum Editor::AssetReferenceTimingKind;
        Editor::AssetReferenceTimings timings;
        Editor::AssetReferenceScanTiming scan;
        scan.Documents = 12;
        scan.ReadParseMs = 0.5;
        timings.Record(Automatic, 7, 1.0, &scan);
        timings.Record(MovePreparation, 8, 3.0);
        timings.Record(MoveRecheck, 8, 2.0);
        if (timings.LastFrame != 8 || timings.LastFrameMs != 5.0 || timings.PeakFrameMs != 5.0)
            return false;

        timings.Record(MovePreparation, 9, 1.0);
        const auto& preparation = timings.ByKind[static_cast<std::size_t>(MovePreparation)];
        if (preparation.Samples != 2 || preparation.LastMs != 1.0 || preparation.AverageMs() != 2.0 ||
            preparation.PeakMs != 3.0 || timings.LastFrameMs != 1.0 || timings.PeakFrameMs != 5.0)
            return false;

        timings.RecordBackground(MovePreparation, 15.0);
        if (preparation.Samples != 3 || preparation.LastMs != 15.0 || timings.LastFrame != 9 ||
            timings.LastFrameMs != 1.0 || timings.PeakFrameMs != 5.0)
            return false;

        timings.RecordBackground(DeleteImpact, 20.0);
        const auto& deletion = timings.ByKind[static_cast<std::size_t>(DeleteImpact)];
        if (deletion.Samples != 1 || deletion.LastMs != 20.0 || timings.LastFrame != 9 ||
            timings.LastFrameMs != 1.0 || timings.PeakFrameMs != 5.0)
            return false;

        const auto& latest = timings.LastScan;
        if (latest.Kind != Automatic || latest.Frame != 7 || latest.Documents != 12 || latest.ReadParseMs != 0.5)
            return false;

        timings = {};
        return timings.LastFrame == -1 && timings.PeakFrameMs == 0.0 && preparation.Samples == 0 &&
            deletion.Samples == 0 && latest.Frame == -1;
    }

    bool CheckReferenceRepair(const fs::path& assets)
    {
        using Editor::AssetReferenceValidator;
        const auto directory = assets / "ReferenceRepair";
        if (!fs::create_directory(directory))
            return false;
        const auto material = directory / "Material.bammat.json";
        const auto mesh = directory / "Mesh.bammesh.json";
        const auto first = directory / "A.bamscene.json";
        const auto second = directory / "B.bamscene.json";
        constexpr auto original = "\xEF\xBB\xBF" R"json({
            "Id":18446744073709551614,
            "Memo":"Assets/ReferenceRepair/Material",
            "First":{"AssetPath":"Assets/ReferenceRepair/Material","AssetKey":"Assets/ReferenceRepair/Material"},
            "Second":{"AssetPath":"Assets\\ReferenceRepair\\Mesh","AssetKey":"OriginalAlias"},
            "NullHandle":{"Null":true,"AssetPath":"Assets/ReferenceRepair/Material"},
            "Binding":{"Type":"Engine::GraphBindingComponent","Data":{"graphFile":"Assets/ReferenceRepair/Material"}}
        })json";
        constexpr auto other = R"json({"AssetPath":"Assets/ReferenceRepair/Material"})json";
        if (!WriteFixture(material, "{}") || !WriteFixture(mesh, "{}") ||
            !WriteFixture(first, original) || !WriteFixture(second, other))
            return false;

        auto issues = AssetReferenceValidator::ValidateFile(first);
        const auto otherIssues = AssetReferenceValidator::ValidateFile(second);
        issues.insert(issues.end(), otherIssues.begin(), otherIssues.end());
        if (std::ranges::count_if(issues, [](const auto& issue) { return issue.CanAutoFix; }) != 3)
            return false;

        // The second document can be read during preparation, but the writer cannot lock it.
        const HANDLE locked = CreateFileW(second.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (locked == INVALID_HANDLE_VALUE)
            return false;
        const auto lockedCount = AssetReferenceValidator::FixAllSafeIssues(issues);
        CloseHandle(locked);
        if (lockedCount != 0 || ReadFixture(first) != original || ReadFixture(second) != other)
            return false;

        if (AssetReferenceValidator::FixAllSafeIssues(issues) != 3)
            return false;
        const auto repaired = ReadFixture(first);
        if (!repaired.starts_with("\xEF\xBB\xBF") ||
            repaired.find(R"("Id":18446744073709551614)") == std::string::npos ||
            repaired.find(R"("Memo":"Assets/ReferenceRepair/Material")") == std::string::npos ||
            repaired.find(R"("graphFile":"Assets/ReferenceRepair/Material")") == std::string::npos ||
            repaired.find(R"("NullHandle":{"AssetPath":"Assets/ReferenceRepair/Material","Null":true})") == std::string::npos ||
            repaired.find(R"("First":{"AssetKey":"Project://ReferenceRepair/Material.bammat.json",)"
                R"("AssetPath":"Project://ReferenceRepair/Material.bammat.json"})") == std::string::npos ||
            repaired.find(R"("AssetKey":"OriginalAlias","AssetPath":"Project://ReferenceRepair/Mesh.bammesh.json")") == std::string::npos ||
            ReadFixture(second).find("Project://ReferenceRepair/Material.bammat.json") == std::string::npos)
            return false;

        // The old issue must never replace a matching string that is no longer a reference field.
        constexpr auto stale = R"json({"Memo":"Assets/ReferenceRepair/Material"})json";
        if (otherIssues.size() != 1 || !WriteFixture(second, stale) ||
            AssetReferenceValidator::FixIssue(otherIssues.front()) || ReadFixture(second) != stale)
            return false;

        if (!WriteFixture(directory / "Material.json", "{}") || !WriteFixture(second, other))
            return false;
        const auto ambiguous = AssetReferenceValidator::ValidateFile(second);
        if (ambiguous.size() != 1 || ambiguous.front().CanAutoFix ||
            AssetReferenceValidator::FixIssue(otherIssues.front()) || ReadFixture(second) != other)
            return false;

        if (!fs::remove(directory / "Material.json") || !AssetReferenceValidator::FixIssue(otherIssues.front()) ||
            ReadFixture(second).find("Project://ReferenceRepair/Material.bammat.json") == std::string::npos)
            return false;

        const auto outside = assets.parent_path() / "OutsideReferenceRepair.json";
        std::filesystem::path suggestion;
        return WriteFixture(outside, "{}") &&
            !AssetReferenceValidator::TryResolveFallbackPath(Engine::Unicode::WideToUtf8(outside.wstring()), suggestion);
    }

    class RelocationProbe final : public Engine::Resource
    {
    public:
        RelocationProbe() : Resource(Engine::EResourceType::Unknown) {}
        void Free() override {}
        bool PrepareFilePathRelocation(const std::vector<Engine::FilePathChange>&,
            std::vector<Engine::Utf8FilePathUpdate>&) override { return AllowRelocation; }
        bool AllowRelocation = true;
    };

    bool CheckPrefabGraphRelocation(const fs::path& assets, const fs::path& recovery)
    {
        using Path = Engine::PlatformPath;
        using Validator = Editor::AssetReferenceValidator;
        if (!Engine::SceneManager::Create()) return false;
        struct SceneManagerScope { ~SceneManagerScope() { Engine::SceneManager::Destroy(); } } sceneManagerScope;
        auto& scenes = Engine::SceneManager::Get();
        if (scenes.NewScene() != Engine::EResult::Success) return false;
        auto* scene = scenes.GetCurrentScene();
        auto& registry = scene->GetRegistry();

        const auto folder = assets / L"MovePrefabGraph";
        const auto target = assets / L"프리팹 그래프";
        const auto graphFile = folder / L"Behavior.bamgraph.json";
        const auto prefabFile = folder / L"Actor.bamprefab.json";
        const auto sceneFile = assets / L"PrefabGraph.bamscene.json";
        const std::wstring legacyGraph = L"Resources/MovePrefabGraph/Behavior.json";
        fs::create_directory(folder);
        fs::create_directory(recovery);

        Engine::GraphRegistry nodes;
        Engine::Graph graph;
        const auto& reflection = Engine::ReflectionRegistry::Get();
        if (!nodes.RegisterBuiltins().Succeeded() || !graph.AddNode(nodes, "Event.Start").Succeeded() ||
            !Engine::SaveGraphJson(graphFile, graph, nodes, reflection.GetRegistry(), reflection.GetPropertyCodecs()).Succeeded())
            return false;
        auto& root = scene->CreateEntity();
        registry.emplace<Engine::GraphBindingComponent>(root.GetEntityHandle()).graphFile = legacyGraph;
        archive::JsonArchive prefabWriter;
        if (scenes.SavePrefab(prefabWriter, prefabFile.wstring(), root) != Engine::EResult::Success) return false;
        archive::JsonArchive prefabReader;
        Engine::Entity* instance = nullptr;
        if (scenes.InstantiatePrefab(prefabReader, Path::ToVirtualUri(prefabFile), instance) != Engine::EResult::Success ||
            !instance) return false;
        archive::JsonArchive sceneWriter;
        if (!scene->WriteArchive(sceneWriter) || !sceneWriter.SaveToFile(sceneFile)) return false;

        const auto release = [](Engine::GraphSystem* system) { if (system) system->Release(); };
        std::unique_ptr<Engine::GraphSystem, decltype(release)> graphSystem(Engine::GraphSystem::Create(), release);
        if (!graphSystem) return false;
        const auto* cached = graphSystem->GetOrLoadGraph(legacyGraph);
        if (!cached || cached != graphSystem->GetOrLoadGraph(Path::ToVirtualUri(graphFile))) return false;
        auto& runtime = registry.emplace<Engine::GraphRuntimeState>(root.GetEntityHandle());
        runtime.loadedPath = legacyGraph;
        runtime.sourcePath = Engine::GraphSystem::ResolveGraphPath(legacyGraph).wstring();
        runtime.hasStarted = true;

        const std::vector<Editor::AssetPathMove> moves{ { folder, target } };
        Editor::AssetReferenceImpact impact;
        std::vector<Editor::AssetReferenceEdit> edits;
        const auto originalScene = ReadFixture(sceneFile);
        const auto originalPrefab = ReadFixture(prefabFile);
        if (!ExpectValidBuildScene(sceneFile) || !Validator::PrepareMoveReferences(moves, edits, &impact) ||
            !impact.IsComplete || impact.References.size() != 3 || edits.size() != 2) return false;

        const auto replay = [&](bool forward)
        {
            const auto& before = forward ? folder : target;
            const auto& after = forward ? target : folder;
            Editor::AssetPathRelocation relocation({ { before, after } }, { sceneFile, before / prefabFile.filename() });
            if (!relocation.PrepareScenePaths(registry) || !Validator::ValidateMoveReferences(edits, forward)) return false;
            fs::rename(before, after);
            if (Validator::ApplyMoveReferences(edits, forward, recovery) != Editor::EAssetReferenceWriteResult::Success) return false;
            relocation.Apply();
            graphSystem->InvalidateGraphCache([&](const fs::path& path) { return relocation.Affects(path); });
            return true;
        };
        const auto movedGraph = target / graphFile.filename();
        const auto movedPrefab = target / prefabFile.filename();
        if (!replay(true) || !ExpectValidBuildScene(sceneFile) || graphSystem->GetOrLoadGraph(legacyGraph) ||
            !graphSystem->GetOrLoadGraph(Path::ToVirtualUri(movedGraph)) || !runtime.hasStarted ||
            runtime.loadedPath != Path::ToVirtualUri(movedGraph) || !Path::IsSamePath(runtime.sourcePath, movedGraph) ||
            registry.get<Engine::PrefabInstanceComponent>(instance->GetEntityHandle()).prefabAssetPath !=
                Path::ToVirtualUri(movedPrefab)) return false;

        archive::JsonArchive reopened;
        Engine::Entity* reloaded = nullptr;
        if (scenes.InstantiatePrefab(reopened, Path::ToVirtualUri(movedPrefab), reloaded) != Engine::EResult::Success ||
            !reloaded || registry.get<Engine::GraphBindingComponent>(reloaded->GetEntityHandle()).graphFile !=
                Path::ToVirtualUri(movedGraph)) return false;
        return replay(false) && ReadFixture(sceneFile) == originalScene && ReadFixture(prefabFile) == originalPrefab &&
            runtime.loadedPath == Path::ToVirtualUri(graphFile) && runtime.hasStarted &&
            replay(true) && runtime.loadedPath == Path::ToVirtualUri(movedGraph);
    }

    // 파일에서 다시 읽은 노티파이 트랙과 메모리의 트랙이 다를 때, 다른 필드만 출력한다.
    void ReportNotifyDifferences(const std::vector<Engine::TimelineNotifyTrack>& file,
        const std::vector<Engine::TimelineNotifyTrack>& memory)
    {
        std::cerr << "    tracks file=" << file.size() << " memory=" << memory.size() << '\n';
        const auto print = [](const char* field, const auto& lhs, const auto& rhs)
        {
            if (lhs == rhs) return;
            std::cerr << "    " << field << ": file=";
            if constexpr (requires { lhs.x; })
            {
                for (int i = 0; i < static_cast<int>(std::remove_cvref_t<decltype(lhs)>::length()); ++i)
                    std::cerr << (i ? "," : "") << lhs[i];
                std::cerr << " memory=";
                for (int i = 0; i < static_cast<int>(std::remove_cvref_t<decltype(rhs)>::length()); ++i)
                    std::cerr << (i ? "," : "") << rhs[i];
            }
            else if constexpr (std::is_enum_v<std::remove_cvref_t<decltype(lhs)>>)
                std::cerr << static_cast<int>(lhs) << " memory=" << static_cast<int>(rhs);
            else
                std::cerr << lhs << " memory=" << rhs;
            std::cerr << '\n';
        };
        for (size_t t = 0; t < (std::min)(file.size(), memory.size()); ++t)
        {
            const auto& a = file[t];
            const auto& b = memory[t];
            print("TrackName", a.TrackName, b.TrackName);
            print("Kind", a.Kind, b.Kind);
            print("IsMuted", a.IsMuted, b.IsMuted);
            std::cerr << "    track " << t << " notifies file=" << a.Notifies.size() << " memory=" << b.Notifies.size() << '\n';
            for (size_t n = 0; n < (std::min)(a.Notifies.size(), b.Notifies.size()); ++n)
            {
                const auto& x = a.Notifies[n];
                const auto& y = b.Notifies[n];
                print("Name", x.Name, y.Name);
                print("TriggerTime", x.TriggerTime, y.TriggerTime);
                print("Duration", x.Duration, y.Duration);
                print("Type", x.Type, y.Type);
                print("SocketBoneName", x.SocketBoneName, y.SocketBoneName);
                print("OffsetPosition", x.OffsetPosition, y.OffsetPosition);
                print("OffsetRotation", x.OffsetRotation, y.OffsetRotation);
                print("ParticleColor", x.ParticleColor, y.ParticleColor);
                print("ParticleSize", x.ParticleSize, y.ParticleSize);
                print("ParticleSpeed", x.ParticleSpeed, y.ParticleSpeed);
                print("ParticleLifetime", x.ParticleLifetime, y.ParticleLifetime);
                print("BurstCount", x.BurstCount, y.BurstCount);
                print("SoundPath", x.SoundPath, y.SoundPath);
                print("SoundVolume", x.SoundVolume, y.SoundVolume);
                print("EventTag", x.EventTag, y.EventTag);
                print("VFXPath", x.VFXPath, y.VFXPath);
                print("VFXScale", x.VFXScale, y.VFXScale);
                print("AttachToSocket", x.AttachToSocket, y.AttachToSocket);
                print("VFXPlayRate", x.VFXPlayRate, y.VFXPlayRate);
                print("AutoDestroy", x.AutoDestroy, y.AutoDestroy);
            }
        }
    }

    bool CheckTimelineRelocation(const fs::path& assets, const fs::path& recovery)
    {
        using Validator = Editor::AssetReferenceValidator;
        using Path = Engine::PlatformPath;
        auto& manager = Engine::ResourceManager::Get();
        const auto scenario = assets / L"TimelineScene.bamscene.json";
        const auto folder = assets / L"TimelineDependencies";
        const auto target = assets / L"타임라인 이동";
        const auto timeline = folder / L"Attack.actiontimeline.json";
        const auto model = folder / L"Actor.bammodel";
        const auto sound = folder / L"Hit.wav";
        const auto vfx = folder / L"Impact.bamvfx";
        fs::create_directory(folder);
        fs::create_directory(recovery);
        const auto uri = [](const fs::path& path) { return Engine::Unicode::WideToUtf8(Path::ToVirtualUri(path)); };
        // 실패 단계를 남겨 실행 결과만으로 원인을 구분한다.
        const auto fail = [](const char* step) { std::cerr << "  timeline relocation: " << step << '\n'; return false; };

        if (!WriteFixture(scenario, R"({"Scene":{"Components":[
            {"Type":"Engine::ActionTimelineComponent","Data":{"timelinePath":"Resources/TimelineDependencies/Attack.actiontimeline.json"}},
            {"Type":"Custom::Component","Data":{"timelinePath":"Project://TimelineDependencies/Attack.actiontimeline.json"}}
            ]}})") || !WriteFixture(model, "{}") || !WriteFixture(sound, "") || !WriteFixture(vfx, "{}")) return fail("fixture write");

        std::unique_ptr<Engine::Resource> fixture(Engine::ActionTimelineAsset::CreateEmpty());
        auto& source = static_cast<Engine::ActionTimelineAsset&>(*fixture);
        source.SetModelPath(uri(model));
        auto& notify = source.GetNotifyTracks().emplace_back().Notifies.emplace_back();
        notify.SoundPath = uri(sound);
        notify.VFXPath = uri(vfx);
        notify.EventTag = uri(sound);
        if (!source.SaveToFile(uri(timeline))) return fail("timeline save");

        auto handle = Engine::ResourceHandle<Engine::ActionTimelineAsset>(manager.LoadFile(uri(timeline)));
        auto* loaded = handle.Get();
        if (!loaded) return fail("timeline load through ResourceManager");
        const auto originalTimeline = ReadFixture(timeline);
        const auto originalScene = ReadFixture(scenario);
        const std::vector<Editor::AssetPathMove> changes{ { folder, target } };
        std::vector<Editor::AssetReferenceEdit> edits;
        const auto impact = Validator::AnalyzeMove({ folder });
        if (!impact.IsComplete || impact.References.size() != 4)
        {
            std::cerr << "  timeline relocation: complete=" << impact.IsComplete << " references=" << impact.References.size() << " (expected 4)\n";
            return false;
        }
        if (!ExpectValidBuildScene(scenario)) return fail("scene build validation before move");
        if (!Validator::PrepareMoveReferences(changes, edits) || edits.size() != 2)
        {
            std::cerr << "  timeline relocation: prepared edits=" << edits.size() << " (expected 2)\n";
            return false;
        }

        const auto checkPaths = [&](const fs::path& directory)
        {
            const auto& movedNotify = loaded->GetNotifyTracks().front().Notifies.front();
            return handle.Get() == loaded && loaded->GetModelPath() == uri(directory / model.filename()) &&
                movedNotify.SoundPath == uri(directory / sound.filename()) && movedNotify.VFXPath == uri(directory / vfx.filename()) &&
                movedNotify.EventTag == uri(sound);
        };
        bool attempted = false;
        if (manager.RelocateFileResources(changes, [&]() { attempted = true; return false; }) || !attempted)
            return fail("failed file move must be rejected after the move callback runs");
        if (!checkPaths(folder)) return fail("paths changed after a rejected move");
        if (ReadFixture(timeline) != originalTimeline) return fail("timeline file changed after a rejected move");

        const auto replay = [&](bool forward)
        {
            const auto& before = forward ? folder : target;
            const auto& after = forward ? target : folder;
            return manager.RelocateFileResources({ { before, after } }, [&]()
            {
                fs::rename(before, after);
                if (Validator::ApplyMoveReferences(edits, forward, recovery) == Editor::EAssetReferenceWriteResult::Success)
                    return true;
                fs::rename(after, before);
                return false;
            });
        };
        const auto movedTimeline = target / timeline.filename();
        if (!replay(true)) return fail("first forward move");
        if (!ExpectValidBuildScene(scenario)) return fail("scene build validation after move");
        if (!checkPaths(target)) return fail("in-memory timeline paths after move");
        if (manager.FindHandleByFilePath(movedTimeline) != handle.GetRawHandle()) return fail("handle lookup by moved path");
        if (!source.LoadFromFile(uri(movedTimeline))) return fail("reload moved timeline file");
        if (source.GetModelPath() != loaded->GetModelPath()) return fail("moved file model path differs from memory");
        if (source.GetNotifyTracks() != loaded->GetNotifyTracks())
        {
            ReportNotifyDifferences(source.GetNotifyTracks(), loaded->GetNotifyTracks());
            return fail("moved file notify tracks differ from memory");
        }
        const auto movedScene = ReadFixture(scenario);
        const auto movedContents = ReadFixture(movedTimeline);
        if (movedScene.find(uri(timeline)) == std::string::npos) return fail("scene no longer contains the original timeline URI");
        if (movedScene.find(uri(movedTimeline)) == std::string::npos) return fail("scene does not contain the moved timeline URI");

        if (!replay(false)) return fail("move back");
        if (!checkPaths(folder)) return fail("in-memory timeline paths after moving back");
        if (ReadFixture(scenario) != originalScene) return fail("scene text after moving back");
        if (ReadFixture(timeline) != originalTimeline) return fail("timeline text after moving back");
        if (!replay(true)) return fail("second forward move");
        if (!checkPaths(target)) return fail("in-memory timeline paths after second move");
        if (ReadFixture(scenario) != movedScene) return fail("scene text after second move");
        if (ReadFixture(movedTimeline) != movedContents) return fail("timeline text after second move");
        return true;
    }
}

int RunAssetReferenceChecks()
{
    if (!CheckReferenceTimings()) return 57;
    using Editor::AssetReferenceValidator;
    if (!AssetReferenceValidator::IsReferenceDocumentPath(L"씬.BAMSCENE.JSON") ||
        !AssetReferenceValidator::IsReferenceDocumentPath(L"Controller.ANIMATOR") ||
        !AssetReferenceValidator::IsReferenceDocumentPath(L"Controller.bamanimator") ||
        !AssetReferenceValidator::IsReferenceDocumentPath(L"Timeline.ACTIONTIMELINE") ||
        AssetReferenceValidator::IsReferenceDocumentPath(L"Material.bammat.json.tmp") ||
        AssetReferenceValidator::IsReferenceDocumentPath(L"Texture.png"))
        return 55;

    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto root = fs::temp_directory_path() / ("BamAssetReferenceCheck-" + std::to_string(stamp));
    if (!fs::create_directory(root))
        return 1;
    const Fixture fixture{ root };

    const auto assets = root / "Assets";
    const auto materials = assets / "Materials";
    const auto engine = root / "EngineResources";
    const auto legacy = root / "Resources";
    fs::create_directory(assets);
    fs::create_directory(materials);
    fs::create_directory(engine);
    fs::create_directory(legacy);
    Engine::PlatformPath::SetProjectRoot(root);
    Engine::PlatformPath::SetProjectAssetRoot(assets);
    Engine::PlatformPath::SetEngineResourceRoot(engine);
    Engine::PlatformPath::RegisterVirtualScheme({ L"Editor", root / "EditorResources", true, true });

    const auto scene = assets / "Sample.bamscene.json";
    const auto material = materials / "Brick.bammat.json";
    const auto texture = materials / L"벽.png";
    const auto invalid = assets / "Invalid.bamprefab.json";
    fs::path renamed;
    using Editor::ContentBrowser::GetRenameBaseName;
    using Editor::ContentBrowser::TryBuildRenamePath;
    if (GetRenameBaseName(material, false) != "Brick" ||
        GetRenameBaseName(assets / "Folder.old", true) != "Folder.old" ||
        GetRenameBaseName(materials / "Brick.BAMMAT.JSON", false) != "Brick" ||
        !TryBuildRenamePath(material, false, "새 머티리얼", renamed) ||
        renamed != materials / L"새 머티리얼.bammat.json" ||
        !TryBuildRenamePath(material, false, "Other.bammat.json", renamed) ||
        renamed != materials / "Other.bammat.json" ||
        !TryBuildRenamePath(assets / "Folder.old", true, "Folder.new", renamed) ||
        renamed != assets / "Folder.new")
        return 29;
    for (const char* invalidName : { "", ".", "..", "../outside", "bad\\name", "bad:name", "bad?name",
        "trailing.", "trailing ", "CON", "nul.txt", "CON .txt", "COM1", "Lpt9.backup", "COM¹" })
    {
        if (TryBuildRenamePath(material, false, invalidName, renamed))
            return 30;
    }
    if (TryBuildRenamePath(material, false, std::string(256, 'a'), renamed))
        return 34;
    if (!WriteFixture(scene, R"({"Scene":{"Material":{"AssetPath":"Resources/Materials/Brick.bammat.json"}}})") ||
        !WriteFixture(material, R"({"Texture":{"AssetKey":"Project://Materials/벽.png","AssetPath":"old.png"},
            "AnotherUse":{"AssetPath":"Asset://Materials/벽.png"},"Unset":{"Null":true,"AssetPath":"unused.png"},
            "Metadata":{"Path":"not-a-reference.png"}})") ||
        !WriteFixture(texture, "") ||
        !WriteFixture(engine / "builtIn.png", "") ||
        !WriteFixture(assets / "Broken.bamprefab.json", R"({"Texture":{"AssetPath":"Project://missing.png"}})") ||
        !WriteFixture(assets / "EngineUser.bammat.json", R"({"Texture":{"AssetPath":"Engine://builtIn.png"}})") ||
        !WriteFixture(invalid, "{") ||
        !WriteFixture(legacy / "Ignored.bamscene.json", R"({"Texture":{"AssetPath":"ignored.png"}})"))
        return 2;

    Editor::AssetReferenceScanTiming scanTiming;
    Editor::AssetReferenceScanIndex scanIndex;
    const auto scan = Editor::AssetReferenceValidator::ScanProject(&scanTiming, &scanIndex);
    if (scanTiming.Directories != 2 || scanTiming.Documents != 5 ||
        scanTiming.References != scan.References.size() || scanTiming.Issues != scan.Issues.size())
        return 67;
    const auto uses = [&](const fs::path& user, const fs::path& asset)
    {
        const auto canonicalUser = fs::canonical(user);
        const auto canonicalAsset = fs::canonical(asset);
        return std::ranges::count_if(scan.References, [&](const auto& reference)
        {
            return reference.Source == canonicalUser && reference.Target == canonicalAsset;
        });
    };
    if (scan.References.size() != 3 || uses(scene, material) != 1 || uses(material, texture) != 1 ||
        uses(assets / "EngineUser.bammat.json", engine / "builtIn.png") != 1)
    {
        std::cerr << "Expected three distinct scene, material and engine asset links.\n";
        return 3;
    }

    using Issue = Editor::EAssetReferenceIssueType;
    if (scan.Issues.size() != 2 ||
        !std::ranges::any_of(scan.Issues, [](const auto& issue)
            { return issue.IssueType == Issue::MissingFile && issue.TargetReference == "Project://missing.png"; }) ||
        !std::ranges::any_of(scan.Issues, [](const auto& issue)
            { return issue.IssueType == Issue::UnreadableDocument && issue.SourceFile.filename() == "Invalid.bamprefab.json"; }))
    {
        std::cerr << "Expected only the missing reference and unreadable JSON.\n";
        return 4;
    }

    Editor::AssetReferenceChanges changes;
    changes.ObserveFile(assets / ".archive-save-100-0", true, true);
    changes.ObserveFile(assets / ".archive-save-100-0" / "document.tmp", true, true);
    if (!changes.Empty()) return 68;
    changes.ObserveFile(scene, true, true);
    changes.ObserveFile(scene, true, false);
    if (changes.RequiresFullScan || changes.Documents.size() != 1) return 68;

    const auto originalScene = ReadFixture(scene);
    if (!WriteFixture(scene, R"({"Scene":{"Texture":{"AssetPath":"Project://missing-after-save.png"},
        "Valid":{"AssetPath":"Asset://Materials/벽.png"}}})"))
        return 69;
    auto refreshed = scan;
    Editor::AssetReferenceScanDelta delta;
    if (!AssetReferenceValidator::RefreshProjectDocuments(scanIndex, changes.Documents, refreshed, delta, &scanTiming) ||
        scanTiming.Documents != 1 || scanTiming.Directories != 0 ||
        delta.FirstAddedIssue != scan.Issues.size() || !delta.RemovedIssues.empty() ||
        delta.RemovedReferences.size() != 1 || delta.AddedReferences.size() != 1 ||
        delta.RemovedReferences.front().Source != fs::canonical(scene) ||
        delta.RemovedReferences.front().Target != fs::canonical(material) ||
        delta.AddedReferences.front().Source != fs::canonical(scene) ||
        delta.AddedReferences.front().Target != fs::canonical(texture))
        return 69;

    const auto rescanned = AssetReferenceValidator::ScanProject();
    if (rescanned.References.size() != 3 || rescanned.Issues.size() != 3 ||
        rescanned.References.size() != refreshed.References.size() || rescanned.Issues.size() != refreshed.Issues.size() ||
        !std::ranges::all_of(refreshed.References, [&](const auto& link)
        {
            return std::ranges::any_of(rescanned.References, [&](const auto& other)
            {
                return link.Source == other.Source && link.Target == other.Target;
            });
        }) || !std::ranges::all_of(refreshed.Issues, [&](const auto& issue)
        {
            return std::ranges::any_of(rescanned.Issues, [&](const auto& other)
            {
                return issue.SourceFile == other.SourceFile && issue.TargetReference == other.TargetReference &&
                    issue.IssueType == other.IssueType;
            });
        }))
        return 69;

    const auto newDocument = assets / "NewDocument.json";
    if (!WriteFixture(newDocument, "{}")) return 70;
    auto withNewDocument = changes;
    withNewDocument.ObserveFile(newDocument, true, true);
    if (AssetReferenceValidator::RefreshProjectDocuments(scanIndex, withNewDocument.Documents, refreshed, delta) ||
        refreshed.References.size() != 3 || refreshed.Issues.size() != 3 ||
        delta.RemovedReferences.size() != 1 || delta.AddedReferences.size() != 1)
        return 70;
    if (!fs::remove(newDocument) || !WriteFixture(scene, originalScene)) return 70;

    // Restoring the scene while re-reading an unchanged material must not emit the material's link.
    changes.ObserveFile(material, true, false);
    if (!AssetReferenceValidator::RefreshProjectDocuments(scanIndex, changes.Documents, refreshed, delta) ||
        delta.RemovedIssues.size() != 1 || delta.RemovedIssues.front().PreviousIndex != scan.Issues.size() ||
        delta.RemovedIssues.front().IssueType != Issue::MissingFile || delta.RemovedIssues.front().CanAutoFix ||
        delta.FirstAddedIssue != scan.Issues.size() || refreshed.Issues.size() != scan.Issues.size() ||
        delta.RemovedReferences.size() != 1 || delta.AddedReferences.size() != 1 ||
        delta.RemovedReferences.front().Target != fs::canonical(texture) ||
        delta.AddedReferences.front().Target != fs::canonical(material))
        return 72;
    if (!AssetReferenceValidator::RefreshProjectDocuments(scanIndex, changes.Documents, refreshed, delta) ||
        !delta.RemovedIssues.empty() || delta.FirstAddedIssue != refreshed.Issues.size() ||
        !delta.RemovedReferences.empty() || !delta.AddedReferences.empty())
        return 73;

    changes.ObserveFile(texture, true, true);
    if (!changes.RequiresFullScan || !changes.Documents.empty()) return 71;
    changes = {};
    changes.ObserveFile(engine / "builtIn.png", false, true);
    if (!changes.RequiresFullScan) return 71;

    std::vector<fs::path> packaged;
    if (!Editor::AssetReferenceValidator::ValidateBuildScene(scene, &packaged).empty() ||
        std::ranges::find(packaged, fs::canonical(material)) == packaged.end() ||
        std::ranges::find(packaged, fs::canonical(texture)) == packaged.end())
    {
        std::cerr << "The package scan must still traverse scene -> material -> texture.\n";
        return 5;
    }

    const auto folderImpact = Editor::AssetReferenceValidator::AnalyzeDeletion({ materials });
    if (folderImpact.IsComplete || folderImpact.References.size() != 1 ||
        folderImpact.References.front().Source != fs::canonical(scene) ||
        folderImpact.References.front().Target != fs::canonical(material))
    {
        std::cerr << "Folder deletion must keep only external usage and report the unreadable document.\n";
        return 6;
    }

    const auto incompleteMove = Editor::AssetReferenceValidator::AnalyzeMove({ materials });
    if (incompleteMove.IsComplete || incompleteMove.References.size() != 2)
    {
        std::cerr << "Move analysis must keep internal references and report an incomplete scan.\n";
        return 11;
    }

    Editor::AssetReferenceImpact incompletePlan;
    std::vector<Editor::AssetReferenceEdit> discardedEdits;
    if (Editor::AssetReferenceValidator::PrepareMoveReferences({ { materials, assets / L"MovedMaterials" } },
        discardedEdits, &incompletePlan) || incompletePlan.IsComplete || incompletePlan.References.size() != 2 ||
        !discardedEdits.empty()) return 53;

    const auto emptyFolder = assets / L"EmptyFolderMove";
    if (!fs::create_directory(emptyFolder))
        return 74;
    const auto emptyFolderDeletion = Editor::AssetReferenceValidator::AnalyzeDeletion({ emptyFolder });
    if (emptyFolderDeletion.IsComplete || !emptyFolderDeletion.References.empty())
    {
        std::cerr << "Empty folder deletion must preserve the project scan's incomplete status.\n";
        return 74;
    }

    Editor::AssetReferenceImpact emptyFolderImpact;
    std::vector<Editor::AssetReferenceEdit> emptyFolderEdits;
    if (!Editor::AssetReferenceValidator::PrepareMoveReferences(
        { { emptyFolder, assets / L"RenamedEmptyFolder" } }, emptyFolderEdits, &emptyFolderImpact) ||
        !emptyFolderImpact.IsComplete || !emptyFolderImpact.References.empty() || !emptyFolderEdits.empty())
    {
        std::cerr << "An empty folder move must not depend on unrelated unreadable project documents.\n";
        return 74;
    }
    const auto emptyFolderRecheck = Editor::AssetReferenceValidator::AnalyzeMove({ emptyFolder });
    if (!emptyFolderRecheck.IsComplete || !emptyFolderRecheck.References.empty())
    {
        std::cerr << "An empty folder move recheck must not scan unrelated project documents.\n";
        return 74;
    }

    if (!fs::remove(invalid))
        return 7;
    const auto textureImpact = Editor::AssetReferenceValidator::AnalyzeDeletion({ texture });
    if (!textureImpact.IsComplete || textureImpact.References.size() != 1 ||
        textureImpact.References.front().Source != fs::canonical(material) ||
        textureImpact.References.front().Target != fs::canonical(texture))
    {
        std::cerr << "A complete scan must report the material that uses the selected texture.\n";
        return 8;
    }

    const auto groupImpact = Editor::AssetReferenceValidator::AnalyzeDeletion({ scene, materials });
    if (!groupImpact.IsComplete || !groupImpact.References.empty())
    {
        std::cerr << "References between files being deleted together do not affect surviving assets.\n";
        return 9;
    }

    const auto groupMove = Editor::AssetReferenceValidator::AnalyzeMove({ scene, materials, texture });
    if (!groupMove.IsComplete || groupMove.References.size() != 2 ||
        !std::ranges::any_of(groupMove.References, [&](const auto& link)
            { return link.Source == fs::canonical(material) && link.Target == fs::canonical(texture); }))
    {
        std::cerr << "Moving a document with its dependencies must still report their stored references once.\n";
        return 12;
    }

    const auto textureMove = Editor::AssetReferenceValidator::AnalyzeMove({ texture });
    if (!textureMove.IsComplete || textureMove.References.size() != 1 ||
        textureMove.References.front().Source != fs::canonical(material))
        return 13;

    const auto unusedMove = Editor::AssetReferenceValidator::AnalyzeMove({ scene });
    if (!unusedMove.IsComplete || !unusedMove.References.empty())
        return 14;

    const auto missingTarget = Editor::AssetReferenceValidator::AnalyzeDeletion({ assets / "missing.png" });
    if (missingTarget.IsComplete)
    {
        std::cerr << "A deleted selection target must not produce a complete empty result.\n";
        return 10;
    }
    if (Editor::AssetReferenceValidator::AnalyzeMove({ assets / "missing.png" }).IsComplete)
        return 15;

    const auto destination = assets / L"옮긴 폴더";
    fs::create_directory(destination);
    const auto controller = assets / "Walk.bamanimator";
    if (!WriteFixture(controller, "\xEF\xBB\xBF" R"({"Clip":{"AssetPath":"Project://Materials/벽.png"}})") ||
        !WriteFixture(scene, R"({"Scene":{"Material":{"AssetPath":"Resources/Materials/Brick.bammat"},
            "Components":[{"Type":"Engine::AnimatorComponent","Data":{"controllerPath":"Project://Walk.bamanimator"}}],
            "Note":"Project://Materials/벽.png","Id":18446744073709551615,"Negative":-9223372036854775808}})"))
        return 16;

    using Validator = Editor::AssetReferenceValidator;
    if (!ExpectValidBuildScene(scene))
    {
        std::cerr << "Legacy Resources/*.bammat aliases must resolve before planning the move.\n";
        return 49;
    }
    const std::vector<Editor::AssetPathMove> moves{
        { materials, destination / "Materials" }, { controller, destination / controller.filename() }
    };
    std::vector<Editor::AssetReferenceEdit> edits;
    if (!Validator::PrepareMoveReferences(moves, edits) || edits.size() != 3)
    {
        std::cerr << "A folder/controller move must prepare all three referencing documents.\n";
        return 17;
    }
    const auto sceneBefore = ReadFixture(scene);
    const auto materialBefore = ReadFixture(material);
    const auto controllerBefore = ReadFixture(controller);
    const auto recovery = root / "Recovery";
    fs::create_directory(recovery);

    // An external edit after confirmation must reject the whole operation.
    WriteFixture(scene, "{\"external\":true}");
    if (Validator::ValidateMoveReferences(edits, true))
        return 18;
    WriteFixture(scene, sceneBefore.c_str());
    if (!Validator::ValidateMoveReferences(edits, true))
        return 19;

    for (const auto& move : moves)
        fs::rename(move.Before, move.After);
    if (Validator::ApplyMoveReferences(edits, true, recovery) != Editor::EAssetReferenceWriteResult::Success)
        return 20;
    const auto sceneAfter = ReadFixture(scene);
    const auto materialAfter = ReadFixture(destination / "Materials/Brick.bammat.json");
    const auto expect = [](bool valid, const char* message)
    {
        if (!valid) std::cerr << message << '\n';
        return valid;
    };
    bool validMove = true;
    validMove &= expect(sceneAfter.find("Project://Materials/벽.png") != std::string::npos, "Unrelated note changed.");
    validMove &= expect(sceneAfter.find("Project://옮긴 폴더/Walk.bamanimator") != std::string::npos,
        "Controller reference was not remapped.");
    validMove &= expect(sceneAfter.find("Project://옮긴 폴더/Materials/Brick.bammat.json") != std::string::npos,
        "Legacy material reference was not remapped.");
    validMove &= expect(sceneAfter.find("18446744073709551615") != std::string::npos, "Unsigned 64-bit ID changed.");
    validMove &= expect(sceneAfter.find("-9223372036854775808") != std::string::npos, "Signed 64-bit value changed.");
    validMove &= expect(materialAfter.find("old.png") == std::string::npos, "Fallback resource path was not remapped.");
    validMove &= expect(materialAfter.find("Project://옮긴 폴더/Materials/벽.png") != std::string::npos,
        "Texture reference was not remapped.");
    validMove &= expect(ReadFixture(destination / controller.filename()).starts_with("\xEF\xBB\xBF"), "BOM changed.");
    validMove &= ExpectValidBuildScene(scene);
    if (!validMove)
    {
        std::cerr << "Only reference fields must change; all moved dependencies must still resolve.\n";
        return 21;
    }

    // Undo restores exact bytes, including the original legacy paths and formatting.
    for (auto move = moves.rbegin(); move != moves.rend(); ++move)
        fs::rename(move->After, move->Before);
    if (Validator::ApplyMoveReferences(edits, false, recovery) != Editor::EAssetReferenceWriteResult::Success ||
        ReadFixture(scene) != sceneBefore || ReadFixture(material) != materialBefore ||
        ReadFixture(controller) != controllerBefore)
        return 22;
    for (const auto& move : moves)
        fs::rename(move.Before, move.After);
    if (Validator::ApplyMoveReferences(edits, true, recovery) != Editor::EAssetReferenceWriteResult::Success ||
        ReadFixture(scene) != sceneAfter)
        return 23;

    // All documents are checked before any write; a changed last document preserves the others.
    const auto controllerAfterPath = destination / controller.filename();
    const auto controllerAfter = ReadFixture(controllerAfterPath);
    for (auto move = moves.rbegin(); move != moves.rend(); ++move)
        fs::rename(move->After, move->Before);
    WriteFixture(controller, "{\"external\":true}");
    if (Validator::ApplyMoveReferences(edits, false, recovery) == Editor::EAssetReferenceWriteResult::Success ||
        ReadFixture(scene) != sceneAfter || ReadFixture(material) != materialAfter)
        return 24;
    WriteFixture(controller, controllerAfter.c_str());

    const auto locked = CreateFileW(controller.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (locked == INVALID_HANDLE_VALUE)
        return 26;
    const auto lockedResult = Validator::ApplyMoveReferences(edits, false, recovery);
    CloseHandle(locked);
    if (lockedResult != Editor::EAssetReferenceWriteResult::Failed || ReadFixture(scene) != sceneAfter ||
        ReadFixture(material) != materialAfter)
        return 27;
    if (Validator::ApplyMoveReferences(edits, false, recovery) != Editor::EAssetReferenceWriteResult::Success)
        return 28;

    // A name change uses the same plan and exact-byte Undo path as a folder move.
    const auto renamedMaterial = materials / L"새 머티리얼.bammat.json";
    if (!Validator::PrepareMoveReferences({ { material, renamedMaterial } }, edits) || edits.size() != 1)
        return 31;
    const auto renameRecovery = root / "RenameRecovery";
    fs::create_directory(renameRecovery);
    fs::rename(material, renamedMaterial);
    if (Validator::ApplyMoveReferences(edits, true, renameRecovery) != Editor::EAssetReferenceWriteResult::Success ||
        !Validator::ValidateBuildScene(scene).empty())
        return 32;
    fs::rename(renamedMaterial, material);
    if (Validator::ApplyMoveReferences(edits, false, renameRecovery) != Editor::EAssetReferenceWriteResult::Success ||
        ReadFixture(scene) != sceneBefore)
        return 33;

    if (!WriteFixture(invalid, "{") || Validator::PrepareMoveReferences({ { scene, destination / scene.filename() } }, edits))
        return 25;

    if (!Engine::ResourceManager::Create())
        return 35;
    struct ResourceScope { ~ResourceScope() { Engine::ResourceManager::Destroy(); } } resources;
    auto& manager = Engine::ResourceManager::Get();
    const auto oldKey = Engine::PlatformPath::ToVirtualUri(material);
    const auto newKey = Engine::PlatformPath::ToVirtualUri(renamedMaterial);
    auto* probe = new RelocationProbe();
    probe->SetPath(oldKey);
    probe->IncreaseVersion();
    auto live = manager.AddResource<Engine::Resource>(oldKey, probe);
    const auto raw = live.GetRawHandle();
    const std::vector<Engine::FilePathChange> renamePaths{ { material, renamedMaterial } };
    auto editorPath = material;
    auto editorUtf8 = Engine::Unicode::WideToUtf8(material.wstring());
    auto controllerUri = Engine::Unicode::WideToUtf8(oldKey);
    bool applied = false;
    Editor::AssetPathRelocation editorChanges(renamePaths, { scene });
    editorChanges.Add(editorPath);
    editorChanges.AddUtf8(editorUtf8);
    editorChanges.AddUtf8(controllerUri, true);
    editorChanges.AfterCommit([&]() { applied = true; });
    if (editorPath != material || controllerUri != Engine::Unicode::WideToUtf8(oldKey) || applied ||
        !editorChanges.Rewrites(scene) || editorChanges.Affects(materials / L"Other.bammat.json"))
        return 41;

    bool called = false;
    if (manager.RelocateFileResources(renamePaths, [&]() { called = true; return false; }) || !called ||
        manager.FindHandleByKey(oldKey) != raw || live.Get() != probe || probe->GetPath() != oldKey)
        return 36;

    auto collision = manager.AddResource<Engine::Resource>(newKey, new RelocationProbe());
    called = false;
    if (manager.RelocateFileResources(renamePaths, [&]() { called = true; return true; }) || called ||
        manager.FindHandleByKey(oldKey) != raw)
        return 37;
    manager.DestroyResource(collision.GetRawHandle());
    collision.Reset();

    auto* destinationProbe = new RelocationProbe();
    destinationProbe->SetPath(newKey);
    auto physicalCollision = manager.AddResource<Engine::Resource>(L"CachedDestination", destinationProbe);
    called = false;
    if (manager.RelocateFileResources(renamePaths, [&]() { called = true; return true; }) || called)
        return 42;
    if (manager.RelocateFileResources({ { assets / L"Uncached.bammat.json", renamedMaterial } },
        [&]() { called = true; return true; }) || called)
        return 48;
    manager.DestroyResource(physicalCollision.GetRawHandle());
    physicalCollision.Reset();
    probe->AllowRelocation = false;
    if (manager.RelocateFileResources(renamePaths, [&]() { called = true; return true; }) || called)
        return 43;
    probe->AllowRelocation = true;

    if (!manager.RelocateFileResources(renamePaths, [&]() { fs::rename(material, renamedMaterial); return true; }) ||
        live.Get() != probe || live.GetRawHandle() != raw || manager.FindHandleByKey(newKey) != raw ||
        manager.FindHandleByKey(oldKey).IsValid() || probe->GetPath() != newKey || probe->GetVersion() != 1)
        return 38;
    editorChanges.Apply();
    if (!applied || editorPath != Engine::PlatformPath::Normalize(renamedMaterial) ||
        editorUtf8 != Engine::Unicode::WideToUtf8(Engine::PlatformPath::Normalize(renamedMaterial).wstring()) ||
        controllerUri != Engine::Unicode::WideToUtf8(newKey))
        return 44;
    if (!manager.RelocateFileResources({ { renamedMaterial, material } },
        [&]() { fs::rename(renamedMaterial, material); return true; }) || manager.FindHandleByKey(oldKey) != raw)
        return 39;

    auto* customProbe = new RelocationProbe();
    customProbe->SetPath(Engine::PlatformPath::ToVirtualUri(texture));
    auto custom = manager.AddResource<Engine::Resource>(L"SemanticTextureKey", customProbe);
    const auto newTexture = destination / texture.filename();
    if (!manager.RelocateFileResources({ { texture, newTexture } },
        [&]() { fs::rename(texture, newTexture); return true; }) || customProbe->GetKey() != L"SemanticTextureKey" ||
        manager.FindHandleByFilePath(newTexture) != custom.GetRawHandle() || custom.Get() != customProbe)
        return 40;
    // Path lookup must ignore name casing and still reject the same file name in another directory.
    auto upperName = newTexture.filename().wstring();
    std::transform(upperName.begin(), upperName.end(), upperName.begin(), ::towupper);
    if (manager.FindHandleByFilePath(newTexture.parent_path() / upperName) != custom.GetRawHandle() ||
        manager.FindHandleByFilePath(destination / L"OtherFolder" / newTexture.filename()).IsValid())
        return 58;

    // Prefixes compare by component, support directory casing, and never match a sibling name.
    const auto remapped = Engine::PlatformPath::RemapFilePath(material,
        { { materials / L"", destination / L"Materials" } });
    if (!Engine::PlatformPath::IsSamePath(remapped, destination / L"Materials" / material.filename()) ||
        Engine::PlatformPath::IsFilePathAffected(assets / L"MaterialsExtra/a.png", { { materials, destination } }))
        return 45;

    // Case-only metadata changes also update the lookup hash, then reverse without replacing the object.
    auto upperPath = material.parent_path() / L"BRICK.BAMMAT.JSON";
    if (!manager.RelocateFileResources({ { material, upperPath } }, []() { return true; }) || live.Get() != probe ||
        manager.FindHandleByFilePath(upperPath) != raw || manager.FindHandleByKey(oldKey).IsValid())
        return 46;
    if (!manager.RelocateFileResources({ { upperPath, material } }, []() { return true; }) ||
        manager.FindHandleByKey(oldKey) != raw)
        return 47;
    if (!fs::remove(invalid) || !Engine::ReflectionRegistry::Create()) return 50;
    struct ReflectionScope { ~ReflectionScope() { Engine::ReflectionRegistry::Destroy(); } } reflection;
    if (!CheckTimelineRelocation(assets, root / L"TimelineRecovery")) return 51;
    if (!CheckPrefabGraphRelocation(assets, root / L"PrefabGraphRecovery")) return 52;
    if (!CheckReferenceRepair(assets)) return 56;
    return 0;
}

int main()
{
    const int result = RunAssetReferenceChecks();
    if (result != 0)
        std::cerr << "AssetReferenceCheck failed at check " << result << ".\n";
    return result;
}
