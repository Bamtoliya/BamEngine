#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include "ReflectionEditState.h"
#include "SceneEditHistory.h"
#include <entt/entity/entity.hpp>

BEGIN(Engine)
class Entity;
class Scene;
struct ComponentReflectionEntry;
END

BEGIN(Editor)
class InspectorPanel final : public IImGui
{
public:
    explicit InspectorPanel(uint32 panelId = 0);
    virtual ~InspectorPanel() = default;

    void Draw() override;
    EResult SetSelectedEntity(Engine::Entity* entity);
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::SceneEditing; }
    [[nodiscard]] bool IsLocked() const { return m_IsLocked; }
    void SetLocked(bool locked) { m_IsLocked = locked; }

private:
    void DrawHeaderBar(Engine::Entity* currentEntity, std::size_t selectionCount);
    void DrawPrefabHeaderBar(Engine::Entity& entity);
    // 그리기 중에는 요청만 기록하고, 대상 포인터를 쓰는 그리기가 모두 끝난 뒤 교체를 실행한다.
    void ExecutePendingPrefabRevert();
    void DrawModelMaterialRefresh(Engine::Entity& entity);
    void DrawCommonComponents(Engine::Scene& scene, std::span<const entt::entity> entities);
    void DrawCommonTransformVector(Engine::Scene& scene, std::span<const entt::entity> entities, TransformEditMode mode);
    void DrawCommonFlags(Engine::Scene& scene, std::span<const entt::entity> entities);
    void DrawCommonReflectedProperties(Engine::Scene& scene, std::span<const entt::entity> entities, const Engine::ComponentReflectionEntry& entry);
    void DrawEntityProperties(Engine::Entity& entity);
    void DrawAddComponentButton(Engine::Entity& entity);
    void DrawAddComponentPopup(Engine::Entity& entity);

private:
    uint32 m_PanelId = 0;
    char m_AddComponentSearch[128] = {};
    bool m_IsLocked = false;
    uint64 m_LockedEntityId = 0;
    uint64 m_CurrentDisplayId = 0;
    float m_LastScrollY = 0.0f;
    std::unordered_map<uint64, float> m_EntityScrollMap;
    entt::entity m_CachedEntity = entt::null;
    Engine::Scene* m_CachedScene = nullptr;
    uint64 m_CachedRevision = 0;
    SceneEditSnapshot m_CachedSnapshot;

    // Revert All 요청: 포인터 대신 씬과 루트 ID를 보관하고 실행 직전에 다시 찾는다.
    struct PrefabRevertRequest
    {
        Engine::Scene* Scene = nullptr;
        uint64 RootId = 0;
    };
    std::optional<PrefabRevertRequest> m_PendingPrefabRevert;
};
END
