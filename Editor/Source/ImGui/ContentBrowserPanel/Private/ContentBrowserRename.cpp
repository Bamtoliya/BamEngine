#include "ContentBrowserRename.h"
#include "Functions.h"
#include "InputManager.h"
#include <imgui.h>

namespace Editor::ContentBrowser
{
    void DrawRenameInput(const char* id, const std::filesystem::path& path, std::filesystem::path& renamingPath,
        char* renameBuffer, std::size_t renameBufferSize, std::filesystem::path& currentDirectory)
    {
        const auto flags = ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll;
        if (ImGui::InputText(id, renameBuffer, renameBufferSize, flags))
        {
            if (renameBuffer[0] != '\0')
            {
                try
                {
                    const auto newPath = path.parent_path() / Engine::Utf8ToPath(renameBuffer);
                    if (!std::filesystem::exists(newPath))
                    {
                        std::filesystem::rename(path, newPath);
                        if (currentDirectory == path)
                            currentDirectory = newPath;
                    }
                }
                catch (const std::filesystem::filesystem_error&)
                {
                    // 파일 작업 실패 시 현재 경로를 유지하고 인라인 편집을 닫습니다.
                }
            }
            renamingPath.clear();
        }

        if (ImGui::IsItemVisible() && !ImGui::IsAnyItemActive())
            ImGui::SetKeyboardFocusHere(-1);

        const bool abandoned = ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit();
        if (abandoned || Engine::InputManager::Get().IsKeyButtonDown(Engine::EKeyCode::Escape))
            renamingPath.clear();
    }
}