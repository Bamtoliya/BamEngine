#include "AssetFileActions.h"
#include "ContentBrowserPanel.h"
#include "Editor_Macros.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include "PrefabModeManager.h"
#include "SelectionManager.h"
#include <archive/JsonArchive.h>
#include <Windows.h>
#include <shellapi.h>

namespace Editor::AssetFileActions
{
    bool IsSceneFile(const std::filesystem::path& path)
    {
        const auto ext = Engine::ToLowerWStr(path.extension().wstring());
        if (ext != L".json") return false;
        const auto stem = Engine::ToLowerWStr(path.stem().wstring());
        if (stem.ends_with(L".scene") || stem.ends_with(L".bamscene")) return true;
        // 다른 복합 에셋 확장자(.bammat.json 등)인 경우 불필요한 JSON 파싱 없이 즉시 제외
        if (stem.find(L".bam") != std::wstring::npos || stem.ends_with(L".prefab") ||
            stem.ends_with(L".spriteanim") || stem.ends_with(L".animator") ||
            stem.ends_with(L".graph") || stem.ends_with(L".postfx"))
        {
            return false;
        }
        // 일반 .json 파일인 경우 루트의 "Scene" 키 존재 여부 확인
        archive::JsonArchive ar;
        if (!ar.LoadFromFile(path)) return false;
        bool hasScene = false;
        return ar.HasField("Scene", hasScene) && hasScene;
    }

    bool IsPrefabFile(const std::filesystem::path& path)
    {
        const auto ext = Engine::ToLowerWStr(path.extension().wstring());
        if (ext == L".bamprefab" || ext == L".prefab") return true;
        if (ext != L".json") return false;
        const auto filename = Engine::ToLowerWStr(path.filename().wstring());
        if (filename.ends_with(L".bamprefab.json") || filename.ends_with(L".prefab.json")) return true;
        const auto stem = Engine::ToLowerWStr(path.stem().wstring());
        if (stem.find(L".bam") != std::wstring::npos || stem.ends_with(L".scene") ||
            stem.ends_with(L".spriteanim") || stem.ends_with(L".animator") ||
            stem.ends_with(L".graph") || stem.ends_with(L".postfx"))
        {
            return false;
        }
        archive::JsonArchive ar;
        if (!ar.LoadFromFile(path)) return false;
        bool hasPrefab = false;
        return ar.HasField("Prefab", hasPrefab) && hasPrefab;
    }

    void Open(const std::filesystem::path& filePath)
    {
        const auto path = Engine::PlatformPath::ResolveUri(filePath);
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error))
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorCommon.Open"),
                LOCAL_FMT("Notification.OpenAssetMissing", Engine::Unicode::WideToUtf8(path.filename().wstring())), ELogLevel::Warn);
            return;
        }

        if (IsSceneFile(path))
        {
            ImGuiManager::Get().RequestOpenScene(path);
            return;
        }
        if (IsPrefabFile(path))
        {
            PrefabModeManager::Get().EnterPrefabMode(path);
            return;
        }

        SelectionManager::Get().SetSelectedAsset(path);
        if (ImGuiManager::Get().RequestOpenResourceEditor(path))
            return;

        const auto result = ShellExecuteW(nullptr, L"open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorCommon.Open"),
                LOCAL_FMT("Notification.OpenExternalFailed", Engine::Unicode::WideToUtf8(path.filename().wstring())), ELogLevel::Warn);
        }
    }

    void RevealInContentBrowser(const std::filesystem::path& path)
    {
        auto* panel = ImGuiManager::Get().GetImGuiPanel(L"Content Browser");
        auto* browser = dynamic_cast<ContentBrowserPanel*>(panel);
        if (!browser || !browser->RequestRevealAsset(path))
        {
            NotificationManager::Get().AddNotification(LOCAL("EditorCommon.RevealInContentBrowser"),
                LOCAL("EditorCommon.RevealInContentBrowserFailed"), ELogLevel::Warn);
        }
    }
}
