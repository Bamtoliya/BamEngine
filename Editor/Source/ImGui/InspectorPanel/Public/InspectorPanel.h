#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include "ReflectionEditState.h"
#include <unordered_map>

BEGIN(Engine)
class Entity;
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
    void DrawHeaderBar(Engine::Entity* currentEntity);
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