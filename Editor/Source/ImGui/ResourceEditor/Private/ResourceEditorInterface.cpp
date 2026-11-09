#include "ResourceEditorInterface.h"
#include "IconsFontAwesome7.h"

BEGIN(Editor)

string ResourceEditorInterface::BuildResourceWindowTitle() const
{
    auto title = GetDisplayName();
    if (!m_TargetResourcePath.empty())
        title += " - " + WStrToStr(m_TargetResourcePath.filename().wstring());
    return title + "###" + GetResourceWindowId();
}

void ResourceEditorInterface::Draw()
{
    m_Focused = false;
    m_Hovered = false;
    if (!m_Open)
    {
        OnResourceWindowVisibility(false);
        return;
    }

    const auto initialSize = GetInitialResourceWindowSize();
    if (initialSize.x > 0.0f && initialSize.y > 0.0f)
        ImGui::SetNextWindowSize(initialSize, ImGuiCond_FirstUseEver);

    const auto title = BuildResourceWindowTitle();
    auto flags = GetResourceWindowFlags() | ImGuiWindowFlags_MenuBar;
    if (HasUnsavedResourceChanges()) flags |= ImGuiWindowFlags_UnsavedDocument;

    bool open = m_Open;
    const bool visible = BeginPanelWindow(title.c_str(), open, flags);
    if (!open) OnResourceCloseRequested();

    if (visible)
    {
        m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    }
    OnResourceWindowVisibility(visible);
    if (visible) DrawResourceFrame();
    ImGui::End();
    AfterResourceWindow();
}

void ResourceEditorInterface::DrawResourceFrame()
{
    if (ImGui::BeginMenuBar())
    {
        if (ImGui::GetContentRegionAvail().x >= ImGui::GetFontSize() * 24.0f)
            DrawCustomToolbar();
        else if (ImGui::BeginMenu(ICON_FA_BARS "###ResourceToolbar"))
        {
            DrawCustomToolbar();
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    const float startY = ImGui::GetCursorPosY();
    DrawCustomQuickAccess();
    const float missingHeight = startY + ImGui::GetFrameHeight() - ImGui::GetCursorPosY();
    if (missingHeight > 0.0f)
        ImGui::Dummy(ImVec2(0.0f, missingHeight));
    ImGui::Separator();
    DrawCustomContent();
}

void ResourceEditorInterface::DrawCustomToolbar()
{
    if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
    {
        if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Close").c_str())) OnResourceCloseRequested();
        ImGui::EndMenu();
    }
}

void ResourceEditorInterface::DrawCopyResourcePath()
{
    EditorUI::IconButtonOptions options;
    options.Enabled = !m_TargetResourcePath.empty();
    if (EditorUI::IconButton("CopyResourcePath", ICON_FA_COPY, "EditorCommon.CopyPath", options))
        ImGui::SetClipboardText(WStrToStr(m_TargetResourcePath.wstring()).c_str());
}

END
