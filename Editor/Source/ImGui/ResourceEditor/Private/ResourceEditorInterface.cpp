#include "ResourceEditorInterface.h"
#include "IconsFontAwesome7.h"
#include "AssetPathRelocation.h"
#include "NotificationManager.h"
#include "InputManager.h"
#include "TimelineView.h"
#include <utility>
#include <algorithm>

ResourceEditorInterface::~ResourceEditorInterface()
{
    CancelAsyncOpen();
}

void ResourceEditorInterface::SetOpen(bool open)
{
    if (open) IImGui::SetOpen(true);
    else if (m_Open) RequestResourceClose();
}

void ResourceEditorInterface::RequestResourceClose()
{
    if (m_PendingDiscardAction || m_ConfirmedResourceAction) return;
    if (HasUnsavedResourceChanges())
        RunWithDiscardConfirmation([this] { CloseResourceDocument(); }, true);
    else
        m_ConfirmedResourceAction = [this] { CloseResourceDocument(); };
}

bool ResourceEditorInterface::RestoreSavedChanges()
{
    if (!HasUnsavedResourceChanges() || RestoreSavedResource()) return true;
    NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL("ResourceEditor.DiscardFailed"), ELogLevel::Error);
    return false;
}

void ResourceEditorInterface::CloseResourceDocument()
{
    CancelAsyncOpen();
    if (!RestoreSavedChanges()) return;
    ReleaseResourceDocument();
    m_PendingDiscardAction = {};
    m_ConfirmedResourceAction = {};
    m_TargetResourcePath.clear();
    m_Open = false;
    m_Focused = false;
    m_Hovered = false;
}

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
    if (!open) RequestResourceClose();

    if (visible)
    {
        m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
        m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    }
    OnResourceWindowVisibility(visible);
    if (visible)
    {
        DrawResourceFrame();
    }
    DrawDiscardConfirmation();
    ImGui::End();
    if (auto action = std::exchange(m_ConfirmedResourceAction, {})) action();
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
    if (IsAsyncLoading())
    {
        DrawAsyncLoadingOverlay();
    }
    else
    {
        DrawCustomContent();
    }
}

void ResourceEditorInterface::DrawAsyncLoadingOverlay()
{
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 1.0f || avail.y <= 1.0f) return;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(22, 24, 28, 255), 4.0f);

    const float barWidth = std::clamp(avail.x * 0.65f, 140.0f, 320.0f);
    const std::string filename = !m_TargetResourcePath.empty() ? WStrToStr(m_TargetResourcePath.filename().wstring()) : "";
    const std::string pendingText = filename.empty()
        ? LOCAL("ModelImport.PreviewPending")
        : LOCAL("ModelImport.PreviewPending") + " (" + filename + ")";

    const ImVec2 textSize = ImGui::CalcTextSize(pendingText.c_str());
    const float blockHeight = textSize.y + ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.y * 2.0f;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + std::max(12.0f, (avail.y - blockHeight) * 0.5f));
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(12.0f, (avail.x - textSize.x) * 0.5f));
    ImGui::TextDisabled("%s", pendingText.c_str());

    ImGui::Spacing();
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + std::max(12.0f, (avail.x - barWidth) * 0.5f));
    const float displayProgress = 0.15f + 0.35f * (0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f));
    ImGui::ProgressBar(displayProgress, ImVec2(barWidth, 0.0f));
}

void ResourceEditorInterface::DrawCustomToolbar()
{
    if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
    {
        DrawCloseMenuItem();
        ImGui::EndMenu();
    }
}

void ResourceEditorInterface::DrawCloseMenuItem()
{
    const auto* hint = ShortcutManager::Get().GetShortcutHint("Window.CloseFocusedPanel");
    if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Close").c_str(), hint)) RequestResourceClose();
}

void ResourceEditorInterface::DrawSaveMenuItem()
{
    if (const auto* action = GetSaveActionId())
    {
        ACTION_MENU_ITEM(LOCAL_LABEL("EditorCommon.Save").c_str(), action);
    }
}

void ResourceEditorInterface::DrawSaveButton(EditorUI::ToolbarLayout& toolbar)
{
    if (const auto* action = GetSaveActionId())
    {
        auto& shortcuts = ShortcutManager::Get();
        EditorUI::IconButtonOptions options;
        options.Enabled = shortcuts.CanExecuteAction(action);
        options.Active = HasUnsavedResourceChanges();
        options.ShortcutHint = shortcuts.GetShortcutHint(action);
        if (toolbar.Icon("ResourceSave", ICON_FA_FLOPPY_DISK, "EditorCommon.Save", options))
            shortcuts.ExecuteAction(action);
    }
}

void ResourceEditorInterface::DrawCopyResourcePath()
{
    EditorUI::IconButtonOptions options;
    options.Enabled = !m_TargetResourcePath.empty();
    if (EditorUI::IconButton("CopyResourcePath", ICON_FA_COPY, "EditorCommon.CopyPath", options))
        ImGui::SetClipboardText(WStrToStr(m_TargetResourcePath.wstring()).c_str());
}

void ResourceEditorInterface::DrawHistoryMenuItems(std::string_view actionPrefix)
{
    const auto prefix = std::string(actionPrefix);
    ACTION_MENU_ITEM(LOCAL_LABEL("EditorCommon.Undo").c_str(), prefix + ".Undo");
    ACTION_MENU_ITEM(LOCAL_LABEL("EditorCommon.Redo").c_str(), prefix + ".Redo");
}

void ResourceEditorInterface::DrawHistoryButtons(EditorUI::ToolbarLayout& toolbar, std::string_view actionPrefix)
{
    EditorUI::DrawTimelineAction(toolbar, actionPrefix, "Undo", ICON_FA_ROTATE_LEFT, "EditorCommon.Undo");
    EditorUI::DrawTimelineAction(toolbar, actionPrefix, "Redo", ICON_FA_ROTATE_RIGHT, "EditorCommon.Redo");
}

bool ResourceEditorInterface::IsHistoryGestureActive()
{
    return ImGui::IsAnyItemActive() || Engine::InputManager::Get().IsMouseButtonPressed(Engine::EMouseButton::Left);
}

void ResourceEditorInterface::RunWithDiscardConfirmation(std::function<void()> action, bool offerSave)
{
    if (!action || m_PendingDiscardAction || m_ConfirmedResourceAction) return;
    if (!HasUnsavedResourceChanges())
    {
        action();
        return;
    }
    // 메뉴 안에서는 OpenPopup의 ID 범위가 달라지므로 여기서는 보관만 하고 창 범위에서 모달을 연다.
    m_PendingDiscardAction = std::move(action);
    m_OfferSave = offerSave;
}

bool ResourceEditorInterface::SavePendingResourceChanges()
{
    const auto* action = GetSaveActionId();
    return action && ShortcutManager::Get().ExecuteAction(action) && !HasUnsavedResourceChanges();
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

    const auto* saveAction = m_OfferSave ? GetSaveActionId() : nullptr;
    const bool canSave = saveAction && ShortcutManager::Get().CanExecuteAction(saveAction);
    const EditorUI::DialogAction actions[] = {
        { "EditorCommon.Cancel" }, { "EditorDocument.Discard", true, true }, { "EditorCommon.Save", canSave }
    };
    const int choice = EditorUI::DialogButtonBar("ResourceDiscardActions", actions, saveAction ? IM_ARRAYSIZE(actions) : 2);

    if (choice >= 0)
    {
        if (choice == 1) m_ConfirmedResourceAction = std::move(m_PendingDiscardAction);
        if (choice == 2)
        {
            m_ConfirmedResourceAction = [this, action = std::move(m_PendingDiscardAction)] {
                if (SavePendingResourceChanges()) action();
            };
        }
        m_PendingDiscardAction = nullptr;
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}
