#include "PrefabModeManager.h"
#include "EditorUI.h"
#include "SceneManager.h"
#include "PlatformPath.h"
#include "Scene.h"
#include "Entity.h"
#include "EntityFactory.h"
#include "WorldComponents.h"
#include "UIComponents.h"
#include "RenderComponents.h"
#include "TransformSystem.h"
#include "SelectionManager.h"
#include "NotificationManager.h"
#include "ImGuiManager.h"
#include <utility>
#include "LocalizationManager.h"
#include <archive/JsonArchive.h>
#include "Application.h"
#include "AssetCache.h"
#include "PrefabInstanceComponent.h"
#include "SceneEditHistory.h"

IMPLEMENT_LAZY_SINGLETON(PrefabModeManager)

namespace
{
    struct PrefabSceneDeleter
    {
        void operator()(Engine::Scene* scene) const { if (scene != nullptr) scene->Release(); }
    };

    bool PreparePrefabUICanvas(Engine::Scene& scene, Engine::Entity& root)
    {
        auto& world = scene.GetRegistry();
        const auto rootHandle = root.GetEntityHandle();
        if (world.all_of<Engine::CanvasComponent>(rootHandle))
            return true;
        if (!world.all_of<Engine::RectTransformComponent>(rootHandle))
            return false;

        // Preview a reusable UI subtree at the default canvas size. Only the original root's
        // subtree is written by SavePrefab(), so this canvas remains local to the edit scene.
        const auto canvas = scene.CreateEntity().GetEntityHandle();
        world.emplace<Engine::NameComponent>(canvas, Engine::StrToWStr(LOCAL("Viewport.PrefabPreviewCanvas")));
        world.emplace<Engine::FlagComponent>(canvas);
        world.emplace<Engine::CanvasComponent>(canvas);
        world.emplace<Engine::HierarchyComponent>(canvas).children.push_back(rootHandle);
        world.get_or_emplace<Engine::HierarchyComponent>(rootHandle).parent = canvas;
        return true;
    }
}

PrefabModeManager::~PrefabModeManager()
{
    Free();
}

EResult PrefabModeManager::Initialize(void* arg)
{
    return EResult();
}

void PrefabModeManager::Free()
{
    if (m_IsInPrefabMode)
    {
        ExitPrefabMode(true);
    }
}

bool PrefabModeManager::SetupDefaultEnvironment(Engine::Scene* scene)
{
    if (!scene) return false;

    bool hasLight = false;
    auto lightView = scene->GetRegistry().view<Engine::LightComponent>();
    if (!lightView.empty()) hasLight = true;

    if (!hasLight)
    {
        auto& lightEnt = EntityFactory::CreateLightEntity(scene, L"__PrefabPreviewLight", L"Directional");
        if (auto* tr = scene->GetRegistry().try_get<Engine::TransformComponent>(lightEnt.GetEntityHandle()))
        {
            tr->eulerRotation = glm::vec3(45.0f, -45.0f, 0.0f);
            (void)tr->SynchronizeRotation();
            Engine::TransformSystem::UpdateHierarchy(scene->GetRegistry());
        }
    }
    return true;
}

bool PrefabModeManager::EnterPrefabMode(const std::filesystem::path& pathOrUri)
{
    if (pathOrUri.empty()) return false;
    const auto prefabPath = Engine::PlatformPath::ResolveUri(pathOrUri);
    if (m_IsInPrefabMode)
    {
        if (m_CurrentPrefabPath == prefabPath) return true;
        if (!ExitPrefabMode(false)) return false;
    }

    auto& sm = Engine::SceneManager::Get();
    m_SavedMainScene = sm.GetCurrentScene();
    if (!m_SavedMainScene) return false;
    auto& history = Application::Get().GetEditHistory();
    if (!history.CanFinalizeEditsFor(m_SavedMainScene))
        return false;

    m_MainSceneHadUnsavedChanges = history.HasUnsavedChanges();

    Safe_AddRef(m_SavedMainScene);

    auto& selection = SelectionManager::Get();
    m_SavedSelection.clear();

    for (auto* entity : selection.GetSelectedEntities())
        m_SavedSelection.push_back(entity->GetWeakRef());

    const auto restoreSelectionOnFailure = [&]()
    {
        selection.ClearSelection();
        for (const auto& ref : m_SavedSelection)
            if (auto* entity = Engine::Entity::Resolve(ref))
                selection.AddToSelection(entity);
        m_SavedSelection.clear();
    };

    m_PrefabScene = Engine::Scene::Create();
    if (!m_PrefabScene)
    {
        restoreSelectionOnFailure();
        Safe_Release(m_SavedMainScene);
        m_SavedMainScene = nullptr;
        return false;
    }
    m_PrefabScene->SetName(prefabPath.stem().wstring() + L" [Prefab]");

    archive::JsonArchive ar;
    if (!ar.LoadFromFile(prefabPath))
    {
        restoreSelectionOnFailure();
        Safe_Release(m_SavedMainScene);
        m_SavedMainScene = nullptr;
        Safe_Release(m_PrefabScene);
        m_PrefabScene = nullptr;
        NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabInstantiateFailed", WStrToStr(prefabPath.stem().wstring())), ELogLevel::Error, 3.0f);
        return false;
    }

    m_PrefabRootEntity = nullptr;
    const auto res = m_PrefabScene->InstantiatePrefabArchive(ar, m_PrefabRootEntity, nullptr);
    (void)res;

    if (!m_PrefabRootEntity)
    {
        restoreSelectionOnFailure();
        Safe_Release(m_SavedMainScene);
        m_SavedMainScene = nullptr;
        Safe_Release(m_PrefabScene);
        m_PrefabScene = nullptr;
        return false;
    }

    const bool hasUI = PreparePrefabUICanvas(*m_PrefabScene, *m_PrefabRootEntity);
    SetupDefaultEnvironment(m_PrefabScene);

    if (sm.OpenScene(m_PrefabScene) != Engine::EResult::Success)
    {
        restoreSelectionOnFailure();
        Safe_Release(m_SavedMainScene);
        m_SavedMainScene = nullptr;
        Safe_Release(m_PrefabScene);
        m_PrefabScene = nullptr;
        m_PrefabRootEntity = nullptr;
        return false;
    }

    // Preserve main scene undo history across prefab mode switch
    Safe_Release(m_PrefabScene);

    SelectionManager::Get().SetSelectedEntity(m_PrefabRootEntity);

    m_IsInPrefabMode = true;
    m_CurrentPrefabPath = prefabPath;
    m_HasUnsavedChanges = false;

    if (hasUI && ImGuiManager::IsCreated())
        if (auto* view = ImGuiManager::Get().GetImGuiPanel(L"UI View"))
            view->Focus();

    NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.EnterPrefabMode", WStrToStr(prefabPath.stem().wstring())), ELogLevel::Info, 3.0f);
    return true;
}

bool PrefabModeManager::HasUnsavedChanges() const
{
    if (!m_IsInPrefabMode) return false;

    const auto& history = Application::Get().GetEditHistory();
    if (history.CanFinalizeEditsFor(m_PrefabScene))
        return history.HasUnsavedChanges();

    return m_HasUnsavedChanges;
}

bool PrefabModeManager::SavePrefab()
{
    if (!m_IsInPrefabMode || !m_PrefabScene || !m_PrefabRootEntity || m_CurrentPrefabPath.empty())
        return false;

    archive::JsonArchive ar;
    auto result = m_PrefabScene->WritePrefabArchive(ar, *m_PrefabRootEntity);
    if (result)
        result = ar.SaveToFile(m_CurrentPrefabPath);

    if (result)
    {
        if (AssetCache::IsCreated()) AssetCache::Get().RequestInvalidate(m_CurrentPrefabPath);
        auto& history = Application::Get().GetEditHistory();
        if (history.CanFinalizeEditsFor(m_PrefabScene))
            history.MarkSaved();

        m_HasUnsavedChanges = false;
        NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaved", WStrToStr(m_CurrentPrefabPath.filename().wstring())), ELogLevel::Info, 3.0f);
        return true;
    }
    else
    {
        NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaveFailed", WStrToStr(m_CurrentPrefabPath.filename().wstring())), ELogLevel::Error, 3.0f);
        return false;
    }
}

bool PrefabModeManager::ExitPrefabMode(bool discardChanges)
{
    if (!m_IsInPrefabMode) return true;

    if (!discardChanges && HasUnsavedChanges())
    {
        m_ShowExitConfirmModal = true;
        return false;
    }

    auto& sm = Engine::SceneManager::Get();
    if (m_SavedMainScene)
    {
        if (sm.OpenScene(m_SavedMainScene) != Engine::EResult::Success)
            return false;

        // Preserve main scene undo history: do NOT call history.ResetDocument
        auto& history = Application::Get().GetEditHistory();
        if (m_MainSceneHadUnsavedChanges && !history.HasUnsavedChanges())
            history.NotifyUntrackedChange();

        Safe_Release(m_SavedMainScene);
        m_SavedMainScene = nullptr;
        m_MainSceneHadUnsavedChanges = false;
    }

    m_PrefabScene = nullptr;
    m_PrefabRootEntity = nullptr;
    m_IsInPrefabMode = false;
    m_CurrentPrefabPath.clear();
    m_HasUnsavedChanges = false;

    auto& selection = SelectionManager::Get();
    selection.ClearSelection();

    for (const auto& ref : m_SavedSelection)
    {
        if (auto* entity = Engine::Entity::Resolve(ref))
            selection.AddToSelection(entity);
    }

    m_SavedSelection.clear();

    return true;
}

bool PrefabModeManager::ApplyPrefabInstance(Engine::Scene& scene, entt::entity rootEntity)
{
    auto& world = scene.GetRegistry();
    if (!world.valid(rootEntity))
        return false;

    const auto* prefabComp = world.try_get<Engine::PrefabInstanceComponent>(rootEntity);
    if (!prefabComp || prefabComp->prefabAssetPath.empty())
        return false;

    const auto targetPath = Engine::PlatformPath::ResolveUri(prefabComp->prefabAssetPath);
    if (targetPath.empty())
        return false;

    if (Engine::PlatformPath::IsReadOnlyTarget(targetPath.wstring()))
    {
        NotificationManager::Get().AddNotification(
            LOCAL("ResourceType.Prefab"),
            LOCAL_FMT("Notification.PrefabSaveFailed", Engine::WStrToStr(targetPath.filename().wstring())),
            ELogLevel::Error, 4.0f);
        return false;
    }

    auto* liveEntity = scene.FindEntity(rootEntity);
    if (!liveEntity)
        return false;

    // Preserve the instance's placement transform:
    // If original prefab asset exists, read its root transform to write back to the prefab asset.
    std::optional<Engine::TransformComponent> originalRootTransform;
    if (std::filesystem::exists(targetPath))
    {
        archive::JsonArchive origAr;
        if (origAr.LoadFromFile(targetPath))
        {
            std::unique_ptr<Engine::Scene, PrefabSceneDeleter> staging(Engine::Scene::Create());
            Engine::Entity* stagingRoot = nullptr;
            if (staging && staging->InstantiatePrefabArchive(origAr, stagingRoot, nullptr) && stagingRoot)
            {
                if (const auto* origTr = staging->GetRegistry().try_get<Engine::TransformComponent>(stagingRoot->GetEntityHandle()))
                {
                    originalRootTransform = *origTr;
                }
            }
        }
    }

    // Temporarily swap root entity transform to the original prefab root transform (or default identity if new)
    auto* liveTransform = world.try_get<Engine::TransformComponent>(rootEntity);
    std::optional<Engine::TransformComponent> savedLiveTransform;
    if (liveTransform)
    {
        savedLiveTransform = *liveTransform;
        if (originalRootTransform)
        {
            *liveTransform = *originalRootTransform;
        }
        else
        {
            *liveTransform = Engine::TransformComponent{};
        }
        (void)liveTransform->SynchronizeRotation();
    }

    archive::JsonArchive ar;
    auto result = scene.WritePrefabArchive(ar, *liveEntity);
    if (result)
        result = ar.SaveToFile(targetPath);

    // Restore live instance's placement transform
    if (liveTransform && savedLiveTransform)
    {
        *liveTransform = *savedLiveTransform;
        (void)liveTransform->SynchronizeRotation();
    }

    const std::string prefabFileName = Engine::WStrToStr(targetPath.filename().wstring());
    if (result)
    {
        if (AssetCache::IsCreated())
            AssetCache::Get().RequestInvalidate(targetPath);

        NotificationManager::Get().AddNotification(
            LOCAL("ResourceType.Prefab"),
            LOCAL_FMT("Notification.PrefabApplied", prefabFileName),
            ELogLevel::Info, 3.0f);
        return true;
    }
    else
    {
        NotificationManager::Get().AddNotification(
            LOCAL("ResourceType.Prefab"),
            LOCAL_FMT("Notification.PrefabSaveFailed", prefabFileName),
            ELogLevel::Error, 4.0f);
        return false;
    }
}

bool PrefabModeManager::UnpackPrefabInstance(Engine::Scene& scene, entt::entity rootEntity, SceneEditHistory* history)
{
    auto& world = scene.GetRegistry();
    if (!world.valid(rootEntity) || !world.all_of<Engine::PrefabInstanceComponent>(rootEntity))
        return false;

    if (!history && Application::IsCreated())
    {
        history = &Application::Get().GetEditHistory();
    }

    auto before = (history && history->Enabled()) ? SceneEditSnapshot::Capture(scene) : SceneEditSnapshot{};
    world.remove<Engine::PrefabInstanceComponent>(rootEntity);

    if (before && history)
    {
        history->Record(std::move(before), SceneEditSnapshot::Capture(scene), "History.Command.UnpackPrefab");
    }

    NotificationManager::Get().AddNotification(
        LOCAL("ResourceType.Prefab"),
        LOCAL("Notification.PrefabUnpacked"),
        ELogLevel::Info, 3.0f);

    return true;
}

void PrefabModeManager::DrawMenuBarUI()
{
    if (!m_IsInPrefabMode) return;

    EditorUI::ToolbarLayout toolbar;
    if (toolbar.Button("BackToScene", ICON_FA_ARROW_LEFT, "PrefabMode.BackToScene"))
    {
        ExitPrefabMode(false);
    }


    // 프리팹 이름 및 수정 여부 표시
    std::string title = std::string(ICON_FA_CUBES " ") + LOCAL("PrefabMode.Title") + ": " + m_CurrentPrefabPath.filename().string();
    if (HasUnsavedChanges()) title += " *";
    toolbar.Text(title.c_str());

}

bool PrefabModeManager::ResolvePrefabModeBeforeQuit()
{
    if (ExitPrefabMode(false)) return true;
    m_QuitAfterExit = m_ShowExitConfirmModal;
    return false;
}

void PrefabModeManager::DrawExitConfirmModal()
{
    const std::string exitPopupTitle = LOCAL("PrefabEditor.ExitConfirmTitle") + "###PrefabExitConfirmPopup";

    if (m_ShowExitConfirmModal)
    {
        ImGui::OpenPopup(exitPopupTitle.c_str());
        m_ShowExitConfirmModal = false;
    }
    if (ImGui::BeginPopupModal(exitPopupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(LOCAL_CSTR("PrefabEditor.UnsavedChangesPrompt"));
        ImGui::Spacing();

        const EditorUI::DialogAction actions[] = {
            { "EditorCommon.Cancel" }, { "EditorDocument.Discard", true, true }, { "PrefabEditor.Save" }
        };
        const int action = EditorUI::DialogButtonBar("PrefabCloseActions", actions, IM_ARRAYSIZE(actions));
        bool continueQuit = false;
        if ((action == 2 && SavePrefab() && ExitPrefabMode(true)) ||
            (action == 1 && ExitPrefabMode(true)))
        {
            continueQuit = std::exchange(m_QuitAfterExit, false);
            ImGui::CloseCurrentPopup();
        }
        if (action == 0)
        {
            m_QuitAfterExit = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
        if (continueQuit) ImGuiManager::Get().RequestQuit();
    }
}
