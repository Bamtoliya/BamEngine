#include "PrefabEditorPanel.h"
#include "Scene.h"
#include "Entity.h"
#include "CoreComponents.h"
#include "TransformSystem.h"
#include "ReflectionPropertyDrawer.h"
#include "FileDialogs.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"
#include <archive/JsonArchive.h>
#include "ShortcutManager.h"

void PrefabEditorPanel::SceneDeleter::operator()(Engine::Scene* s) const
{
    if (s) s->Release();
}

PrefabEditorPanel::PrefabEditorPanel()
{
    m_Name = L"Prefab Editor";
    m_DisplayNameKey = "ResourceEditor.PrefabTitle";
}

PrefabEditorPanel::~PrefabEditorPanel()
{
    CloseSession();
}

bool PrefabEditorPanel::IsSupported(const std::filesystem::path& assetPath) const
{
    std::string ext = assetPath.extension().string();
    for (char& c : ext) c = static_cast<char>(tolower(c));
    if (ext == ".bamprefab" || ext == ".prefab") return true;
    if (ext != ".json") return false;

    std::string filename = assetPath.filename().string();
    for (char& c : filename) c = static_cast<char>(tolower(c));
    return filename.ends_with(".bamprefab.json") || filename.ends_with(".prefab.json");
}

void PrefabEditorPanel::SetTargetResource(const std::filesystem::path& assetPath)
{
    if (m_HasUnsavedChanges && m_TargetResourcePath != assetPath)
    {
        m_PendingOpenPath = assetPath;
        m_ShowCloseConfirmModal = true;
        return;
    }

    m_TargetResourcePath = assetPath;
    LoadPrefab();
}

void PrefabEditorPanel::CloseSession()
{
    m_SelectedEntity = entt::null;
    m_RootEntity = nullptr;
    m_PrefabScene.reset();
    m_HasUnsavedChanges = false;
}

void PrefabEditorPanel::LoadPrefab()
{
    CloseSession();
    if (m_TargetResourcePath.empty() || !std::filesystem::exists(m_TargetResourcePath)) return;

    m_PrefabScene.reset(Engine::Scene::Create());
    if (!m_PrefabScene) return;

    archive::JsonArchive ar;
    if (ar.LoadFromFile(m_TargetResourcePath))
    {
        const auto res = m_PrefabScene->InstantiatePrefabArchive(ar, m_RootEntity, nullptr);
        (void)res;
        if (m_RootEntity) m_SelectedEntity = m_RootEntity->GetEntityHandle();
    }
}

bool PrefabEditorPanel::SavePrefab(const std::filesystem::path& path)
{
    if (!m_PrefabScene || !m_RootEntity) return false;

    archive::JsonArchive ar;
    auto result = m_PrefabScene->WritePrefabArchive(ar, *m_RootEntity);
    if (result) result = ar.SaveToFile(path);

    if (result)
    {
        m_TargetResourcePath = path;
        m_HasUnsavedChanges = false;
        NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaved", path.filename().string()), ELogLevel::Info, 3.0f);
        return true;
    }
    else
    {
        NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabSaveFailed", path.filename().string()), ELogLevel::Error, 3.0f);
        return false;
    }
}

bool PrefabEditorPanel::CanSaveDocument() const
{
    return m_Open && m_PrefabScene && m_RootEntity && !m_TargetResourcePath.empty() &&
        !m_ShowCloseConfirmModal && m_PendingOpenPath.empty();
}

bool PrefabEditorPanel::SaveDocument()
{
    if (!CanSaveDocument()) return false;

    return SavePrefab(m_TargetResourcePath);
}

void PrefabEditorPanel::Draw()
{
    m_Focused = false;
    m_Hovered = false;
    if (!m_Open) return;

    std::string title = (m_TargetResourcePath.empty() ? LOCAL("ResourceEditor.PrefabTitle") : m_TargetResourcePath.filename().string());
    if (m_HasUnsavedChanges) title += " *";
    title += "###PrefabEditorPanel";

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if (!BeginPanelWindow(title.c_str(), m_Open, ImGuiWindowFlags_MenuBar))
    {
        ImGui::End();
        return;
    }

    m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
    m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);

    DrawToolbar();

    if (m_PrefabScene && m_RootEntity)
    {
        const float hierarchyWidth = 260.0f;
        ImGui::BeginChild("PrefabHierarchyRegion", ImVec2(hierarchyWidth, 0.0f), true);
        DrawPrefabHierarchy();
        ImGui::EndChild();

        ImGui::SameLine();

        ImGui::BeginChild("PrefabInspectorRegion", ImVec2(0.0f, 0.0f), true);
        DrawPrefabInspector();
        ImGui::EndChild();
    }
    else
    {
        ImGui::TextDisabled("%s", LOCAL_CSTR("PrefabEditor.NoPrefabLoaded"));
    }

    DrawCloseConfirmModal();
    ImGui::End();
}

void PrefabEditorPanel::DrawToolbar()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
        {
            ACTION_MENU_ITEM(LOCAL_LABEL("PrefabEditor.Save").c_str(), "Prefab.Save");

            if (ImGui::MenuItem(LOCAL_LABEL("PrefabEditor.SaveAs").c_str(), nullptr, false, m_RootEntity != nullptr))
            {
                std::wstring outPath;
                std::vector<FileDialogFilter> filters = {
                    { StrToWStr(LOCAL("FileDialog.PrefabFiles")), L"*.bamprefab.json" }
                };
                if (FileDialogs::SaveFileDialog(outPath, filters, m_TargetResourcePath.filename().wstring(), L"bamprefab.json"))
                    SavePrefab(std::filesystem::path(outPath));
            }

            if (ImGui::MenuItem(LOCAL_LABEL("PrefabEditor.Revert").c_str(), nullptr, false, m_HasUnsavedChanges))
                LoadPrefab();

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // 상단 단축 버튼 행
    ImGui::BeginDisabled(!CanSaveDocument());
    if (ImGui::Button((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("PrefabEditor.Save")).c_str()))
        SaveDocument();
    ImGui::EndDisabled();

    ImGui::SameLine();
    if (ImGui::Button((ICON_FA_ROTATE_LEFT " " + LOCAL_LABEL("PrefabEditor.Revert")).c_str()))
        LoadPrefab();

    ImGui::SameLine();
    ImGui::TextDisabled("| %s", m_TargetResourcePath.string().c_str());
    ImGui::Separator();
}

void PrefabEditorPanel::DrawPrefabHierarchy()
{
    ImGui::TextUnformatted(LOCAL_CSTR("PrefabEditor.Hierarchy"));
    ImGui::Separator();

    std::vector<entt::entity> selected;
    if (m_SelectedEntity != entt::null) selected.push_back(m_SelectedEntity);

    const auto action = m_HierarchyView.Draw(*m_PrefabScene, selected, entt::null, false, m_SearchFilter);
    if (action.Clicked != entt::null)
    {
        m_SelectedEntity = action.Clicked;
    }
    for (auto delHandle : action.DeleteEntities)
    {
        auto* entity = m_PrefabScene->FindEntity(delHandle);
        if (entity && delHandle != m_RootEntity->GetEntityHandle())
        {
            if (m_SelectedEntity == delHandle) m_SelectedEntity = m_RootEntity->GetEntityHandle();
            if (m_PrefabScene->DestroyEntity(*entity)) m_HasUnsavedChanges = true;
        }
    }
}

void PrefabEditorPanel::DrawPrefabInspector()
{
    ImGui::TextUnformatted(LOCAL_CSTR("PrefabEditor.Inspector"));
    ImGui::Separator();

    if (m_SelectedEntity == entt::null || !m_PrefabScene->GetRegistry().valid(m_SelectedEntity))
    {
        ImGui::TextDisabled("%s", LOCAL_CSTR("PrefabEditor.NoEntitySelected"));
        return;
    }

    auto& registry = m_PrefabScene->GetRegistry();

    // 1. 엔티티 이름(NameComponent) 편집
    if (auto* nameComp = registry.try_get<Engine::NameComponent>(m_SelectedEntity))
    {
        char nameBuf[128] = "";
        std::string curName = WStrToStr(nameComp->name);
        strncpy_s(nameBuf, curName.c_str(), sizeof(nameBuf));
        if (ImGui::InputText(LOCAL_CSTR("PROP_NAME"), nameBuf, sizeof(nameBuf)))
        {
            nameComp->name = StrToWStr(nameBuf);
            m_HasUnsavedChanges = true;
        }
    }

    // 2. TransformComponent 편집
    if (auto* tr = registry.try_get<Engine::TransformComponent>(m_SelectedEntity))
    {
        if (ImGui::CollapsingHeader(LOCAL_CSTR("STRUCT_TRANSFORM"), ImGuiTreeNodeFlags_DefaultOpen))
        {
            bool trChanged = false;
            if (ImGui::DragFloat3(LOCAL_LABEL("PROP_POSITION").c_str(), &tr->position.x, 0.1f)) trChanged = true;
            if (ImGui::DragFloat3(LOCAL_LABEL("PROP_EULER_ROTATION").c_str(), &tr->eulerRotation.x, 1.0f)) trChanged = true;
            if (ImGui::DragFloat3(LOCAL_LABEL("PROP_SCALE").c_str(), &tr->scale.x, 0.05f)) trChanged = true;

            if (trChanged)
            {
                (void)tr->SynchronizeRotation();
                Engine::TransformSystem::UpdateHierarchy(registry);
                m_HasUnsavedChanges = true;
            }
        }
    }

    // 3. 리플렉션 등록 컴포넌트 편집
    for (const auto& entry : Engine::ReflectionRegistry::Get().GetComponentEntries())
    {
        if (entry.Type == std::type_index(typeid(Engine::NameComponent)) ||
            entry.Type == std::type_index(typeid(Engine::TransformComponent)) ||
            entry.Type == std::type_index(typeid(Engine::HierarchyComponent)) ||
            entry.Type == std::type_index(typeid(Engine::IDComponent)))
            continue;

        if (entry.Has && entry.Has(registry, m_SelectedEntity) && entry.Edit && entry.ReflectionType)
        {
            auto object = entry.Edit(registry, m_SelectedEntity);
            if (ReflectionPropertyDrawer::DrawObject(*entry.ReflectionType, object, &Engine::ReflectionRegistry::Get().GetRegistry()))
            {
                m_HasUnsavedChanges = true;
            }
        }
    }
}

void PrefabEditorPanel::DrawCloseConfirmModal()
{
    const std::string popupTitle = LOCAL("PrefabEditor.CloseConfirmTitle") + "###PrefabCloseConfirmPopup";
    if (m_ShowCloseConfirmModal)
    {
        ImGui::OpenPopup(popupTitle.c_str());
        m_ShowCloseConfirmModal = false;
    }

    if (ImGui::BeginPopupModal(popupTitle.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::TextUnformatted(LOCAL_CSTR("PrefabEditor.UnsavedChangesPrompt"));
        ImGui::Spacing();

        if (ImGui::Button(LOCAL_CSTR("PrefabEditor.Save"), ImVec2(100, 0)))
        {
            SavePrefab(m_TargetResourcePath);
            ImGui::CloseCurrentPopup();
            if (!m_PendingOpenPath.empty()) { SetTargetResource(m_PendingOpenPath); m_PendingOpenPath.clear(); }
        }

        ImGui::SameLine();
        if (ImGui::Button(LOCAL_CSTR("EditorDocument.Discard"), ImVec2(100, 0)))
        {
            m_HasUnsavedChanges = false;
            ImGui::CloseCurrentPopup();
            if (!m_PendingOpenPath.empty()) { SetTargetResource(m_PendingOpenPath); m_PendingOpenPath.clear(); }
        }

        ImGui::SameLine();
        if (ImGui::Button(LOCAL_CSTR("EditorCommon.Cancel"), ImVec2(100, 0)))
        {
            m_PendingOpenPath.clear();
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}
