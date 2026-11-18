#include "AssetReferenceValidator.h"
#include "ContentBrowserRename.h"
#include "PlatformPath.h"
#include "Engine_Includes.h"
#include "ResourceManager.h"
#include "AssetPathRelocation.h"
#include "ActionTimelineAsset.h"
#include "ReflectionRegistry.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
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

    bool WriteFixture(const fs::path& path, const char* text)
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
                << ", source=" << Engine::WStrToStr(issue.SourceFile.wstring())
                << ", target=" << issue.TargetReference << '\n';
        return issues.empty();
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
        const auto uri = [](const fs::path& path) { return Engine::WStrToStr(Path::ToVirtualUri(path)); };

        if (!WriteFixture(scenario, R"({"Scene":{"Components":[
            {"Type":"Engine::ActionTimelineComponent","Data":{"timelinePath":"Resources/TimelineDependencies/Attack.actiontimeline.json"}},
            {"Type":"Custom::Component","Data":{"timelinePath":"Project://TimelineDependencies/Attack.actiontimeline.json"}}
            ]}})") || !WriteFixture(model, "") || !WriteFixture(sound, "") || !WriteFixture(vfx, "")) return false;

        std::unique_ptr<Engine::Resource> fixture(Engine::ActionTimelineAsset::CreateEmpty());
        auto& source = static_cast<Engine::ActionTimelineAsset&>(*fixture);
        source.SetModelPath(uri(model));
        auto& notify = source.GetNotifyTracks().emplace_back().Notifies.emplace_back();
        notify.SoundPath = uri(sound);
        notify.VFXPath = uri(vfx);
        notify.EventTag = uri(sound);
        if (!source.SaveToFile(uri(timeline))) return false;

        auto handle = Engine::ResourceHandle<Engine::ActionTimelineAsset>(manager.LoadFile(uri(timeline)));
        auto* loaded = handle.Get();
        if (!loaded) return false;
        const auto originalTimeline = ReadFixture(timeline);
        const auto originalScene = ReadFixture(scenario);
        const std::vector<Editor::AssetPathMove> changes{ { folder, target } };
        std::vector<Editor::AssetReferenceEdit> edits;
        const auto impact = Validator::AnalyzeMove({ folder });
        if (!impact.IsComplete || impact.References.size() != 4 || !ExpectValidBuildScene(scenario) ||
            !Validator::PrepareMoveReferences(changes, edits) || edits.size() != 2) return false;

        const auto checkPaths = [&](const fs::path& directory)
        {
            const auto& movedNotify = loaded->GetNotifyTracks().front().Notifies.front();
            return handle.Get() == loaded && loaded->GetModelPath() == uri(directory / model.filename()) &&
                movedNotify.SoundPath == uri(directory / sound.filename()) && movedNotify.VFXPath == uri(directory / vfx.filename()) &&
                movedNotify.EventTag == uri(sound);
        };
        bool attempted = false;
        if (manager.RelocateFileResources(changes, [&]() { attempted = true; return false; }) || !attempted ||
            !checkPaths(folder) || ReadFixture(timeline) != originalTimeline) return false;

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
        if (!replay(true) || !ExpectValidBuildScene(scenario) || !checkPaths(target) ||
            manager.FindHandleByFilePath(movedTimeline) != handle.GetRawHandle()) return false;
        if (!source.LoadFromFile(uri(movedTimeline)) || source.GetModelPath() != loaded->GetModelPath() ||
            source.GetNotifyTracks() != loaded->GetNotifyTracks()) return false;
        const auto movedScene = ReadFixture(scenario);
        const auto movedContents = ReadFixture(movedTimeline);
        if (movedScene.find(uri(timeline)) == std::string::npos ||
            movedScene.find(uri(movedTimeline)) == std::string::npos) return false;

        return replay(false) && checkPaths(folder) && ReadFixture(scenario) == originalScene &&
            ReadFixture(timeline) == originalTimeline && replay(true) && checkPaths(target) &&
            ReadFixture(scenario) == movedScene && ReadFixture(movedTimeline) == movedContents;
    }
}

int RunAssetReferenceChecks()
{
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
    Engine::PlatformPath::SetEditorResourceRoot(root / "EditorResources");

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

    const auto scan = Editor::AssetReferenceValidator::ScanProject();
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
    auto editorUtf8 = Engine::WStrToStr(material.wstring());
    auto controllerUri = Engine::WStrToStr(oldKey);
    bool applied = false;
    Editor::AssetPathRelocation editorChanges(renamePaths, { scene });
    editorChanges.Add(editorPath);
    editorChanges.AddUtf8(editorUtf8);
    editorChanges.AddUtf8(controllerUri, true);
    editorChanges.AfterCommit([&]() { applied = true; });
    if (editorPath != material || controllerUri != Engine::WStrToStr(oldKey) || applied ||
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
        editorUtf8 != Engine::WStrToStr(Engine::PlatformPath::Normalize(renamedMaterial).wstring()) ||
        controllerUri != Engine::WStrToStr(newKey))
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
    return 0;
}

int main()
{
    const int result = RunAssetReferenceChecks();
    if (result != 0)
        std::cerr << "AssetReferenceCheck failed at check " << result << ".\n";
    return result;
}
