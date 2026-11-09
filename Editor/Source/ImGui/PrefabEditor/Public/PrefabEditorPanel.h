#pragma once

#include "ResourceEditorInterface.h"
#include "EntityHierarchyView.h"
#include <memory>
#include <filesystem>

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
    bool HasUnsavedResourceChanges() const override { return m_HasUnsavedChanges; }
    bool CanSaveDocument() const;
    bool SaveDocument();

private:
    void LoadPrefab();
    bool SavePrefab(const std::filesystem::path& path);
    void CloseSession();

    void DrawCustomQuickAccess() override;
    void DrawCustomContent() override;
    ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(800.0f, 600.0f); }
    void DrawCustomToolbar() override;
    string GetResourceWindowId() const override { return "PrefabEditorPanel"; }
    void DrawPrefabHierarchy();
    void DrawPrefabInspector();
    void DrawCloseConfirmModal();

private:
    struct SceneDeleter { void operator()(Engine::Scene* s) const; };
    std::unique_ptr<Engine::Scene, SceneDeleter> m_PrefabScene;
    Engine::Entity* m_RootEntity = nullptr;
    entt::entity m_SelectedEntity = entt::null;
    EntityHierarchyView m_HierarchyView;

    bool m_HasUnsavedChanges = false;
    bool m_ShowCloseConfirmModal = false;
    std::filesystem::path m_PendingOpenPath;
    char m_SearchFilter[128] = "";
};

END