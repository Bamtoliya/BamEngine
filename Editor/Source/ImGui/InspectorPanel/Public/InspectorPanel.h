#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include "ReflectionEditState.h"
#include <unordered_map>
#include <cstddef>
#include <span>
#include <entt/entity/entity.hpp>

BEGIN(Engine)
class Entity;
class Scene;
struct ComponentReflectionEntry;
END

BEGIN(Editor)
class InspectorPanel final : public ImGuiInterface
{
public:
    explicit InspectorPanel(uint32 panelId = 0);
    virtual ~InspectorPanel() = default;

    void Draw() override;
    EResult SetSelectedEntity(Engine::Entity* entity);

    [[nodiscard]] bool IsLocked() const { return m_IsLocked; }
    void SetLocked(bool locked) { m_IsLocked = locked; }

private:
    void DrawHeaderBar(Engine::Entity* currentEntity, std::size_t selectionCount);
    void DrawCommonComponents(Engine::Scene& scene, std::span<const entt::entity> entities);
    void DrawCommonTransformVector(Engine::Scene& scene, std::span<const entt::entity> entities, TransformEditMode mode);
    void DrawCommonFlags(Engine::Scene& scene, std::span<const entt::entity> entities);
    void DrawCommonReflectedProperties(Engine::Scene& scene, std::span<const entt::entity> entities, const Engine::ComponentReflectionEntry& entry);
    void DrawEntityProperties(Engine::Entity& entity);
    void DrawAddComponentButton(Engine::Entity& entity);
    void DrawAddComponentPopup(Engine::Entity& entity);

private:
    uint32 m_PanelId = 0;
    bool m_IsLocked = false;
    uint64 m_LockedEntityId = 0;
    uint64 m_CurrentDisplayId = 0;
    float m_LastScrollY = 0.0f;
    std::unordered_map<uint64, float> m_EntityScrollMap;
};
END