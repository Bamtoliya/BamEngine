#pragma once

#include "Editor_Includes.h"
#include "ImGuiInterface.h"
#include "ReflectionEditState.h"


BEGIN(Engine)
class Entity;
END

BEGIN(Editor)
class InspectorPanel final : public ImGuiInterface
{
public:
    InspectorPanel() { m_Name = L"Inspector"; m_DisplayNameKey = "UI_INSPECTOR"; }
    virtual ~InspectorPanel() = default;

    void Draw() override;
    EResult SetSelectedEntity(Engine::Entity* entity);

private:
    void DrawEntityProperties(Engine::Entity& entity);
    void DrawAddComponentButton(Engine::Entity& entity);
    void DrawAddComponentPopup(Engine::Entity& entity);
};
END