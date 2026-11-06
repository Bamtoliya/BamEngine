#pragma once

#include "Editor_Includes.h"
#include "InputActionSettings.h"
#include "InputMap.h"
BEGIN(Editor)
class EditorSettingsService;

class InputActionSettingsWidget
{
public:
    void ReloadDraft(EditorSettingsService& settings);
    void Apply(EditorSettingsService& settings);
    void Draw();
    void HandleCapture(bool active);
    void CancelCapture();

    [[nodiscard]] bool IsDirty() const { return m_InputActionsDirty; }

private:
    struct InputBindingCapture
    {
        std::string MapId;
        std::string ActionId;
        std::optional<std::size_t> BindingIndex;
        bool SkipFirstFrame = true;
        bool Duplicate = false;
        Engine::EKeyCode PendingModifier = Engine::EKeyCode::Unknown;
        int MouseCaptureHoverFrame = -1;
    };
    struct ExpandedAction
    {
        std::string MapId;
        std::string ActionId;
    };

    std::optional<ExpandedAction> m_ExpandedAction;
    Engine::InputActionSettings m_DraftInputActions;
    bool m_InputActionsLoaded = false;
    bool m_InputActionsDirty = false;
    std::optional<InputBindingCapture> m_InputBindingCapture;
    std::map<std::string, std::string> m_NewInputActionIds;
};
END