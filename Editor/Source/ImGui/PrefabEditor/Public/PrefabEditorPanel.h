#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceDocumentHistory.h"
#include "EntityHierarchyView.h"
#include <memory>
#include <filesystem>
#include <optional>

namespace Engine { class Scene; class Entity; }

BEGIN(Editor)

class PrefabEditorPanel final : public ResourceEditorInterface
{
public:
    PrefabEditorPanel();
    virtual ~PrefabEditorPanel() override;

    virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
    virtual void SetTargetResource(const std::filesystem::path& assetPath) override;
    virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::PrefabEditor; }
    bool HasUnsavedResourceChanges() const override { return m_History.IsDirty(); }
    bool CanSaveDocument() const;
    bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override
    {
        // A separate prefab scene can retain string references even when its window is closed.
        return !m_PrefabScene && ResourceEditorInterface::PrepareAssetPathRelocation(relocation);
    }
    bool SaveDocument();

private:
    void LoadPrefab(const std::filesystem::path& path);
    bool SavePrefab(const std::filesystem::path& path);
    void CloseSession();
    void ReleaseResourceDocument() override { CloseSession(); }
    const char* GetSaveActionId() const override { return "Prefab.Save"; }

    void DrawCustomQuickAccess() override;
    void DrawCustomContent() override;
    ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(800.0f, 600.0f); }
    void DrawCustomToolbar() override;
    string GetResourceWindowId() const override { return "PrefabEditorPanel"; }
    void DrawPrefabHierarchy();
    void DrawPrefabInspector();

private:
    struct SceneDeleter { void operator()(Engine::Scene* s) const; };
    std::unique_ptr<Engine::Scene, SceneDeleter> m_PrefabScene;
    Engine::Entity* m_RootEntity = nullptr;
    entt::entity m_SelectedEntity = entt::null;
    EntityHierarchyView m_HierarchyView;

    ResourceDocumentHistory<std::string> m_History;
    bool CommitDocumentEdit();
    std::optional<std::string> CaptureDocument() const;
    bool ApplyDocumentSnapshot(const std::string& snapshot);
    bool CanUndoDocument() const override;
    bool CanRedoDocument() const override;
    void UndoDocument() override;
    void RedoDocument() override;
    char m_SearchFilter[128] = "";
};

END