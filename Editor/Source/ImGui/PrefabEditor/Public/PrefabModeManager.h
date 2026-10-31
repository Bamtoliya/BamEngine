#pragma once

#include "Editor_Includes.h"
#include <filesystem>
#include <vector>

BEGIN(Engine)
class Scene;
class Entity;
END

BEGIN(Editor)

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
    [[nodiscard]] Engine::Scene* GetPrefabScene() const { return m_PrefabScene; }
    [[nodiscard]] Engine::Entity* GetRootEntity() const { return m_PrefabRootEntity; }
    [[nodiscard]] bool HasUnsavedChanges() const;
    void MarkDirty() { m_HasUnsavedChanges = true; }

    bool EnterPrefabMode(const std::filesystem::path& prefabPath);
    bool SavePrefab();
    bool ExitPrefabMode(bool discardChanges = false);

    void DrawViewportBanner();

private:
    bool SetupDefaultEnvironment(Engine::Scene* scene);

private:
    bool m_IsInPrefabMode = false;
    std::filesystem::path m_CurrentPrefabPath;
    Engine::Scene* m_SavedMainScene = nullptr;
    Engine::Scene* m_PrefabScene = nullptr;
    Engine::Entity* m_PrefabRootEntity = nullptr;
    bool m_HasUnsavedChanges = false;
    bool m_ShowExitConfirmModal = false;
    bool m_MainSceneHadUnsavedChanges = false;
    std::vector<Engine::Entity*> m_SavedSelection;
};

END