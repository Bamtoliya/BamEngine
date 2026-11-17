#include "AssetReferenceValidator.h"
#include "PlatformPath.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>

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
}

int main()
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

    const auto missingTarget = Editor::AssetReferenceValidator::AnalyzeDeletion({ assets / "missing.png" });
    if (missingTarget.IsComplete)
    {
        std::cerr << "A deleted selection target must not produce a complete empty result.\n";
        return 10;
    }
}
