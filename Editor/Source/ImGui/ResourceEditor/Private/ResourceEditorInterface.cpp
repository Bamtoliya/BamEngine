#include "ResourceEditorInterface.h"
#include "IconsFontAwesome7.h"
#include "AssetPathRelocation.h"

bool ResourceEditorInterface::PrepareAssetPathRelocation(AssetPathRelocation& relocation)
{
    if (HasUnsavedResourceChanges())
        return false;
    for (const auto& path : GetRetainedResourcePaths())
    {
        if (relocation.Affects(path))
            return false;
    }
    return true;
}

bool ResourceEditorInterface::PrepareTargetPathRelocation(AssetPathRelocation& relocation)
{
    if (HasUnsavedResourceChanges())
        return false;
    relocation.Add(m_TargetResourcePath);
    return true;
}

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
    if (visible)
    {
        DrawResourceFrame();
        DrawDiscardConfirmation();
    }
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

void ResourceEditorInterface::RunWithDiscardConfirmation(std::function<void()> action)
{
    if (!action) return;
    if (!HasUnsavedResourceChanges())
    {
        action();
        return;
    }
    // 메뉴 안에서는 OpenPopup의 ID 범위가 달라지므로 여기서는 보관만 하고 창 범위에서 모달을 연다.
    m_PendingDiscardAction = std::move(action);
}

void ResourceEditorInterface::DrawDiscardConfirmation()
{
    if (!m_PendingDiscardAction) return;

    const auto title = LOCAL("ResourceEditor.DiscardTitle") + "###ResourceDiscardConfirm";
    if (!ImGui::IsPopupOpen(title.c_str())) ImGui::OpenPopup(title.c_str());
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) return;

    const auto targetName = m_TargetResourcePath.empty() ? LOCAL("EditorDocument.Untitled") : WStrToStr(m_TargetResourcePath.filename().wstring());
    ImGui::TextUnformatted(LOCAL_FMT("ResourceEditor.DiscardMessage", targetName).c_str());
    ImGui::Spacing();

    const EditorUI::DialogAction actions[] = { { "EditorCommon.Cancel" }, { "ResourceEditor.DiscardContinue", true, true } };
    const int choice = EditorUI::DialogButtonBar("ResourceDiscardActions", actions, IM_ARRAYSIZE(actions));

    std::function<void()> confirmed;
    if (choice == 0 || choice == 1)
    {
        if (choice == 1) confirmed = std::move(m_PendingDiscardAction);
        m_PendingDiscardAction = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();

    // 파일 대화상자처럼 블로킹하는 동작은 모달 범위를 벗어난 뒤 실행한다.
    if (confirmed) confirmed();
}