#pragma once

#include "ImGuiInterface.h"
#include "ShortcutManager.h"
#include "imgui.h"

BEGIN(Editor)

class ShortcutSettingsPanel : public ImGuiInterface
{
public:
    ShortcutSettingsPanel();
    virtual ~ShortcutSettingsPanel() override = default;

    virtual void Draw() override;

private:
    void DrawHeader();
    void DrawActionTable();
    void HandleKeyCapture();

private:
    ImGuiTextFilter m_Filter;
    std::string m_ListeningActionId;      // 현재 키 입력을 대기 중인 액션 ID
    std::string m_ConflictWarning;        // 충돌 경고 메시지
};

END