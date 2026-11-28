#include "PrefabEditorPanel.h"
#include "EditorUI.h"
#include "Scene.h"
#include "Entity.h"
#include "WorldComponents.h"
#include "TransformSystem.h"
#include "ReflectionPropertyDrawer.h"
#include "FileDialogs.h"
#include "NotificationManager.h"
#include "LocalizationManager.h"
#include <archive/JsonArchive.h>
#include "ShortcutManager.h"
#include "AssetCache.h"

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

    std::string filename = Engine::Unicode::WideToUtf8(assetPath.filename().wstring());
    for (char& c : filename) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return filename.ends_with(".bamprefab.json") || filename.ends_with(".prefab.json");
}

void PrefabEditorPanel::SetTargetResource(const std::filesystem::path& assetPath)
{
    LoadPrefab(assetPath);
}

void PrefabEditorPanel::CloseSession()
{
    m_SelectedEntity = entt::null;
    m_RootEntity = nullptr;
    m_PrefabScene.reset();
    m_History.Clear();
}

void PrefabEditorPanel::LoadPrefab(const std::filesystem::path& path)
{
    archive::JsonArchive ar;
    std::unique_ptr<Engine::Scene, SceneDeleter> scene(Engine::Scene::Create());
    Engine::Entity* root = nullptr;
    if (!scene || !ar.LoadFromFile(path) || !scene->InstantiatePrefabArchive(ar, root, nullptr) || !root)
    {
        NotificationManager::Get().AddNotification(GetDisplayName(),
            LOCAL_FMT("Notification.ResourceLoadFailed", Engine::Unicode::WideToUtf8(path.filename().wstring())), ELogLevel::Error);
        return;
    }

    CloseSession();
    m_TargetResourcePath = path;
    m_PrefabScene = std::move(scene);
    m_RootEntity = root;
    m_SelectedEntity = m_RootEntity->GetEntityHandle();
    if (auto snapshot = CaptureDocument()) m_History.Reset(std::move(*snapshot));
}

bool PrefabEditorPanel::SavePrefab(const std::filesystem::path& path)
{
    if (!m_PrefabScene || !m_RootEntity || !CommitDocumentEdit()) return false;

    archive::JsonArchive ar;
    auto result = m_PrefabScene->WritePrefabArchive(ar, *m_RootEntity);
    if (result) result = ar.SaveToFile(path);

    if (result)
    {
        if (AssetCache::IsCreated()) AssetCache::Get().RequestInvalidate(path);
        m_TargetResourcePath = path;
        m_History.MarkSaved();
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
    return m_Open && m_PrefabScene && m_RootEntity && !m_TargetResourcePath.empty();
}

bool PrefabEditorPanel::SaveDocument()
{
    if (!CanSaveDocument()) return false;

    return SavePrefab(m_TargetResourcePath);
}

void PrefabEditorPanel::DrawCustomContent()
{
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
    if (!IsHistoryGestureActive()) CommitDocumentEdit();
}

std::optional<std::string> PrefabEditorPanel::CaptureDocument() const
{
    archive::JsonArchive ar;
    std::string snapshot;
    if (!m_PrefabScene || !m_RootEntity || !m_PrefabScene->WritePrefabArchive(ar, *m_RootEntity) || !ar.ToJson(snapshot))
        return std::nullopt;
    return snapshot;
}

bool PrefabEditorPanel::CommitDocumentEdit()
{
    if (!m_History.HasPendingEdit()) return true;
    auto snapshot = CaptureDocument();
    return snapshot && m_History.Record(std::move(*snapshot),
        [this](const auto& value) { return ApplyDocumentSnapshot(value); }, LOCAL("History.Command.EditProperty"));
}

bool PrefabEditorPanel::ApplyDocumentSnapshot(const std::string& snapshot)
{
    archive::JsonArchive ar;
    std::unique_ptr<Engine::Scene, SceneDeleter> scene(Engine::Scene::Create());
    Engine::Entity* root = nullptr;
    if (!scene || !ar.Parse(snapshot) || !scene->InstantiatePrefabArchive(ar, root, nullptr) || !root) return false;

    m_PrefabScene = std::move(scene);
    m_RootEntity = root;
    m_SelectedEntity = root->GetEntityHandle();
    return true;
}

bool PrefabEditorPanel::CanUndoDocument() const
{
    return m_PrefabScene && m_History.CanUndo() && (!m_History.HasPendingEdit() || !IsHistoryGestureActive());
}

bool PrefabEditorPanel::CanRedoDocument() const
{
    return m_PrefabScene && m_History.CanRedo();
}

void PrefabEditorPanel::UndoDocument()
{
    if (CanUndoDocument() && CommitDocumentEdit()) m_History.Undo();
}

void PrefabEditorPanel::RedoDocument()
{
    if (CanRedoDocument()) m_History.Redo();
}

void PrefabEditorPanel::DrawCustomToolbar()
{
    if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
    {
        DrawSaveMenuItem();

        if (ImGui::MenuItem(LOCAL_LABEL("PrefabEditor.SaveAs").c_str(), nullptr, false, m_RootEntity != nullptr))
        {
            std::wstring outPath;
            std::vector<FileDialogFilter> filters = {
                { Engine::Unicode::Utf8ToWide(LOCAL("FileDialog.PrefabFiles")), L"*.bamprefab.json" }
            };
            if (FileDialogs::SaveFileDialog(outPath, filters, m_TargetResourcePath.filename().wstring(), L"bamprefab.json"))
                SavePrefab(std::filesystem::path(outPath));
        }

        if (ImGui::MenuItem(LOCAL_LABEL("PrefabEditor.Revert").c_str(), nullptr, false, HasUnsavedResourceChanges()))
            RunWithDiscardConfirmation([this] { LoadPrefab(m_TargetResourcePath); }, false);

        DrawCloseMenuItem();
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Edit").c_str()))
    {
        DrawHistoryMenuItems("Prefab");
        ImGui::EndMenu();
    }
}

void PrefabEditorPanel::DrawCustomQuickAccess()
{
    EditorUI::ToolbarLayout toolbar;
    DrawSaveButton(toolbar);
    DrawHistoryButtons(toolbar, "Prefab");
    if (toolbar.Icon("PrefabEditor.Revert", ICON_FA_ROTATE_LEFT, "PrefabEditor.Revert", { .Enabled = HasUnsavedResourceChanges() }))
        RunWithDiscardConfirmation([this] { LoadPrefab(m_TargetResourcePath); }, false);

    toolbar.Text(m_TargetResourcePath.string().c_str());
}

void PrefabEditorPanel::DrawPrefabHierarchy()
{
    ImGui::TextUnformatted(LOCAL_CSTR("PrefabEditor.Hierarchy"));
    ImGui::Separator();

    // 표준 검색 바 (EditorUI::DrawSearchBar) 도입 - m_SearchFilter 연동
    EditorUI::DrawSearchBar("PrefabEntitySearch", m_SearchFilter, sizeof(m_SearchFilter), "PrefabEditor.SearchEntities");
    ImGui::Spacing();

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
            if (m_PrefabScene->DestroyEntity(*entity)) m_History.MarkEdited();
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
        std::string curName = Engine::Unicode::WideToUtf8(nameComp->name);
        strncpy_s(nameBuf, curName.c_str(), sizeof(nameBuf));
        if (ImGui::InputText(LOCAL_CSTR("PROP_NAME"), nameBuf, sizeof(nameBuf)))
        {
            nameComp->name = Engine::Unicode::Utf8ToWide(nameBuf);
            m_History.MarkEdited();
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
                m_History.MarkEdited();
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

        if (!entry.Has || !entry.Has(registry, m_SelectedEntity) || !entry.Edit || !entry.ReflectionType) continue;

        auto object = entry.Edit(registry, m_SelectedEntity);
        ComponentHeaderActions actions;
        actions.Enabled = entry.IsEnabled(registry, m_SelectedEntity);
        actions.EnableProperty = entry.FindEnableProperty();
        std::optional<bool> requestedEnable;
        if (entry.CanDisable() && !object.IsReadOnly())
            actions.SetEnabled = [&](bool enabled) { requestedEnable = enabled; };
        if (!entry.CanDisable()) actions.EnableTooltipKey = "Inspector.ComponentAlwaysEnabled";

        const auto& reflection = Engine::ReflectionRegistry::Get().GetRegistry();
        bool changed = ReflectionPropertyDrawer::DrawObject(*entry.ReflectionType, object, &reflection, nullptr, &actions);
        if (requestedEnable) changed |= entry.SetEnabled(registry, m_SelectedEntity, *requestedEnable);
        if (changed) m_History.MarkEdited();
    }
}
