#pragma once

#include "Editor_Includes.h"

struct ImGuiTextFilter;
BEGIN(Editor)
class ShortcutSettingsWidget
{
public:
    void Draw(const ImGuiTextFilter& searchFilter);
    void HandleCapture();
    void CancelCapture();

private:
    string m_ListeningActionId;
    string m_ConflictWarning;
};
END