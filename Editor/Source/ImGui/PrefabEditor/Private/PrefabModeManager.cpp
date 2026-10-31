#include "PrefabModeManager.h"
#include "SceneManager.h"
#include "Scene.h"
#include "Entity.h"
#include "EntityFactory.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "TransformSystem.h"
#include "SelectionManager.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"
#include <archive/JsonArchive.h>
#include "Application.h"

IMPLEMENT_LAZY_SINGLETON(PrefabModeManager)

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

bool PrefabModeManager::EnterPrefabMode(const std::filesystem::path& prefabPath)
{
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

    m_SavedSelection = SelectionManager::Get().GetSelectedEntities();
    SelectionManager::Get().ClearSelection();

    m_PrefabScene = Engine::Scene::Create();
    if (!m_PrefabScene)
    {
        Safe_Release(m_SavedMainScene);
        m_SavedMainScene = nullptr;
        return false;
    }
    m_PrefabScene->SetName(StrToWStr(prefabPath.stem().string()) + L" [Prefab]");

    archive::JsonArchive ar;
    if (!ar.LoadFromFile(prefabPath))
    {
        Safe_Release(m_SavedMainScene);
        m_SavedMainScene = nullptr;
        Safe_Release(m_PrefabScene);
        m_PrefabScene = nullptr;
        NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabInstantiateFailed", prefabPath.stem().string()), ELogLevel::Error, 3.0f);
        return false;
    }

    m_PrefabRootEntity = nullptr;
    const auto res = m_PrefabScene->InstantiatePrefabArchive(ar, m_PrefabRootEntity, nullptr);
    (void)res;

    if (!m_PrefabRootEntity)
    {
        Safe_Release(m_SavedMainScene);
        m_SavedMainScene = nullptr;
        Safe_Release(m_PrefabScene);
        m_PrefabScene = nullptr;
        return false;
    }

    SetupDefaultEnvironment(m_PrefabScene);

    sm.OpenScene(m_PrefabScene);

    if (sm.GetCurrentScene() == m_PrefabScene)
        history.ResetDocument(m_PrefabScene, history.Enabled());

    Safe_Release(m_PrefabScene);

    SelectionManager::Get().SetSelectedEntity(m_PrefabRootEntity);

    m_IsInPrefabMode = true;
    m_CurrentPrefabPath = prefabPath;
    m_HasUnsavedChanges = false;

    NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.EnterPrefabMode", prefabPath.stem().string()), ELogLevel::Info, 3.0f);
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
        auto& history = Application::Get().GetEditHistory();
        if (history.CanFinalizeEditsFor(m_PrefabScene))
            history.MarkSaved();

        m_HasUnsavedChanges = false;
        NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaved", m_CurrentPrefabPath.filename().string()), ELogLevel::Info, 3.0f);
        return true;
    }
    else
    {
        NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaveFailed", m_CurrentPrefabPath.filename().string()), ELogLevel::Error, 3.0f);
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

        auto& history = Application::Get().GetEditHistory();
        history.ResetDocument(m_SavedMainScene, history.Enabled());

        if (m_MainSceneHadUnsavedChanges)
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

    SelectionManager::Get().ClearSelection();
    for (auto* ent : m_SavedSelection)
    {
        SelectionManager::Get().AddToSelection(ent);
    }
    m_SavedSelection.clear();

    return true;
}

void PrefabModeManager::DrawViewportBanner()
{
    if (!m_IsInPrefabMode) return;
    const std::string exitPopupTitle = LOCAL("PrefabEditor.ExitConfirmTitle") + "###PrefabExitConfirmPopup";

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.20f, 0.32f, 0.90f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 4.0f));

    const float bannerHeight = ImGui::GetFrameHeight() + 8.0f;
    if (ImGui::BeginChild("##PrefabViewportBanner", ImVec2(0.0f, bannerHeight), false, ImGuiWindowFlags_NoScrollbar))
    {
        if (ImGui::Button((std::string(ICON_FA_ARROW_LEFT " ") + LOCAL("PrefabMode.BackToScene")).c_str()))
        {
            ExitPrefabMode(false);
        }

        ImGui::SameLine();
        ImGui::TextDisabled("|");
        ImGui::SameLine();

        std::string title = std::string(ICON_FA_CUBES " ") + LOCAL("PrefabMode.Title") + ": " + m_CurrentPrefabPath.filename().string();
        if (HasUnsavedChanges()) title += " *";
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s", title.c_str());

        const float saveWidth = 80.0f;
        const float alignX = ImGui::GetWindowContentRegionMax().x - saveWidth;
        if (alignX > ImGui::GetCursorPosX())
        {
            ImGui::SameLine(alignX);
            if (ImGui::Button((std::string(ICON_FA_FLOPPY_DISK " ") + LOCAL("PrefabEditor.Save")).c_str(), ImVec2(saveWidth, 0.0f)))
            {
                SavePrefab();
            }
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    if (m_ShowExitConfirmModal)
    {
        ImGui::OpenPopup(exitPopupTitle.c_str());
        m_ShowExitConfirmModal = false;
    }
    if (ImGui::BeginPopupModal(exitPopupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(LOCAL_CSTR("PrefabEditor.UnsavedChangesPrompt"));
        ImGui::Spacing();

        if (ImGui::Button(LOCAL_CSTR("PrefabEditor.Save"), ImVec2(100, 0)) && SavePrefab())
        {
            ImGui::CloseCurrentPopup();
            ExitPrefabMode(true);
        }
        ImGui::SameLine();
        if (ImGui::Button(LOCAL_CSTR("EditorDocument.Discard"), ImVec2(100, 0)))
        {
            m_HasUnsavedChanges = false;
            ImGui::CloseCurrentPopup();
            ExitPrefabMode(true);
        }
        ImGui::SameLine();
        if (ImGui::Button(LOCAL_CSTR("EditorCommon.Cancel"), ImVec2(100, 0)))
        {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}
