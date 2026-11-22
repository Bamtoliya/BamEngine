#pragma once

#include "Editor_Includes.h"
#include <filesystem>
#include <vector>

BEGIN(Engine)
class Scene;
class Entity;
END

BEGIN(Editor)

class SceneEditHistory;

class PrefabModeManager final : public Base
{
    DECLARE_SINGLETON(PrefabModeManager)

private:
    PrefabModeManager() = default;
    virtual ~PrefabModeManager() override;
	EResult Initialize(void* arg = nullptr);
public:
    virtual void Free() override;

    [[nodiscard]] bool IsInPrefabMode() const { return m_IsInPrefabMode; }
    [[nodiscard]] const std::filesystem::path& GetPrefabPath() const { return m_CurrentPrefabPath; }
    [[nodiscard]] Scene* GetPrefabScene() const { return m_PrefabScene; }
    [[nodiscard]] Entity* GetRootEntity() const { return m_PrefabRootEntity; }
    [[nodiscard]] bool HasUnsavedChanges() const;
    void MarkDirty() { m_HasUnsavedChanges = true; }

    bool EnterPrefabMode(const std::filesystem::path& prefabPath);
    bool SavePrefab();
    bool ExitPrefabMode(bool discardChanges = false);
    bool ResolvePrefabModeBeforeQuit();

    // Prefab instance operations (Apply All / Unpack)
    static bool ApplyPrefabInstance(Engine::Scene& scene, entt::entity rootEntity);
    static bool UnpackPrefabInstance(Engine::Scene& scene, entt::entity rootEntity, SceneEditHistory* history = nullptr);

    // 헤더바(MenuBar) 내부 좌측 UI 및 종료 확인 모달
    void DrawMenuBarUI();
    void DrawExitConfirmModal();


private:
    bool SetupDefaultEnvironment(Scene* scene);

private:
    bool m_IsInPrefabMode = false;
    std::filesystem::path m_CurrentPrefabPath;
    Scene* m_SavedMainScene = nullptr;
    Scene* m_PrefabScene = nullptr;
    Entity* m_PrefabRootEntity = nullptr;
    bool m_HasUnsavedChanges = false;
    bool m_QuitAfterExit = false;
    bool m_ShowExitConfirmModal = false;
    bool m_MainSceneHadUnsavedChanges = false;
    std::vector<Entity::WeakRef> m_SavedSelection;
};

END