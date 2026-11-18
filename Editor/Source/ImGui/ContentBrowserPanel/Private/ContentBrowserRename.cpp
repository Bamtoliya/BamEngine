#include "ContentBrowserRename.h"
#include "InputManager.h"
#include <imgui.h>

namespace Editor::ContentBrowser
{
    ERenameInputResult DrawRenameInput(const char* id, char* buffer, std::size_t bufferSize, bool& requestFocus)
    {
        const bool focusRequested = requestFocus;
        if (focusRequested)
            ImGui::SetKeyboardFocusHere();
        const auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
        const bool submit = ImGui::InputText(id, buffer, bufferSize, flags);
        if (focusRequested)
            requestFocus = !ImGui::IsItemActive();
        if (Engine::InputManager::Get().IsKeyButtonDown(Engine::EKeyCode::Escape))
            return ERenameInputResult::Cancel;
        if (submit)
            return ERenameInputResult::Submit;
        if (ImGui::IsItemDeactivated())
            return ERenameInputResult::Cancel;
        return ERenameInputResult::Editing;
    }
}