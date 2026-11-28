#include "ResourceEditorInterface.h"
#include "AsyncResourceOpenService.h"
#include "IconsFontAwesome7.h"
#include "AssetPathRelocation.h"
#include "NotificationManager.h"
#include "InputManager.h"
#include "TimelineView.h"
#include <utility>
#include <algorithm>
#include <cmath>

ResourceEditorInterface::~ResourceEditorInterface()
{
    AsyncResourceOpenService::Get().CancelRequestsForEditor(this);
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
    AsyncResourceOpenService::Get().CancelRequestsForEditor(this);
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
        title += " - " + Engine::Unicode::WideToUtf8(m_TargetResourcePath.filename().wstring());
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
    DrawStandardAsyncLoadingOverlay(m_TargetResourcePath, 0.0f, true, false);
}

void ResourceEditorInterface::DrawStandardAsyncLoadingOverlay(
    const std::filesystem::path& targetPath, float progress, bool indeterminate, bool failed)
{
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x <= 1.0f || avail.y <= 1.0f) return;

    const ImVec2 origin = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddRectFilled(
        origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(22, 24, 28, 255), 4.0f);

    if (failed)
    {
        EditorUI::EmptyState("ModelImport.PreviewFailed");
        return;
    }

    const float barWidth = std::clamp(avail.x * 0.65f, 140.0f, 320.0f);
    const auto& displayPath = !targetPath.empty() ? targetPath : m_TargetResourcePath;
    const std::string filename = !displayPath.empty() ? Engine::Unicode::WideToUtf8(displayPath.filename().wstring()) : "";
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
    const float displayProgress = indeterminate
        ? 0.08f + 0.15f * (0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 4.0f))
        : std::clamp(progress, 0.0f, 1.0f);
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
        ImGui::SetClipboardText(Engine::Unicode::WideToUtf8(m_TargetResourcePath.wstring()).c_str());
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

    const auto targetName = m_TargetResourcePath.empty() ? LOCAL("EditorDocument.Untitled") : Engine::Unicode::WideToUtf8(m_TargetResourcePath.filename().wstring());
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

void ResourceEditorInterface::NotifyMissingResources(
    const std::filesystem::path& assetPath, const std::vector<Engine::MissingResourceEntry>& entries) const
{
    if (entries.empty()) return;
    const auto fileName = assetPath.empty() ? LOCAL("EditorDocument.Untitled") : Engine::Unicode::WideToUtf8(assetPath.filename().wstring());
    NotificationManager::Get().AddNotification(GetDisplayName(),
        LOCAL_FMT("ResourceEditor.MissingResourcesNotification", fileName, entries.size()), ELogLevel::Warn);
}

std::filesystem::path ResourceEditorInterface::FindNearbyReplacementAsset(
    const std::filesystem::path& ownerAssetPath, const Engine::MissingResourceEntry& entry)
{
    if (ownerAssetPath.empty())
        return {};

    const std::wstring& refText = !entry.AssetPath.empty() ? entry.AssetPath : entry.AssetKey;
    if (refText.empty())
        return {};

    // 가상 URI("Project://.../foo.png") 또는 물리 경로에서 파일명만 추출
    std::wstring refName = refText;
    if (const auto slash = refName.find_last_of(L"/\\"); slash != std::wstring::npos)
        refName = refName.substr(slash + 1);
    if (refName.empty())
        return {};

    std::wstring targetLower = refName;
    std::ranges::transform(targetLower, targetLower.begin(), ::towlower);

    std::vector<std::filesystem::path> searchRoots;
    const auto ownerDir = ownerAssetPath.parent_path();
    const auto parentDir = ownerDir.parent_path();
    auto addCandidateDir = [&searchRoots](const std::filesystem::path& dir)
    {
        if (!dir.empty())
            searchRoots.push_back(dir);
    };

    for (const auto& base : { ownerDir, parentDir })
    {
        if (base.empty()) continue;
        addCandidateDir(base);
        addCandidateDir(base / L"Textures");
        addCandidateDir(base / L"Materials");
        addCandidateDir(base / L"Meshes");
        addCandidateDir(base / L"Animations");
    }

    std::error_code ec;
    for (const auto& dir : searchRoots)
    {
        if (!std::filesystem::is_directory(dir, ec))
            continue;
        for (const auto& item : std::filesystem::directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec))
        {
            if (ec || !item.is_regular_file(ec)) continue;
            std::wstring candidateLower = item.path().filename().wstring();
            std::ranges::transform(candidateLower, candidateLower.begin(), ::towlower);
            if (candidateLower == targetLower)
                return item.path();
        }
    }
    return {};
}

void ResourceEditorInterface::DrawMissingResourcesBanner(
    std::vector<Engine::MissingResourceEntry>& entries, std::function<void()> onAutoFix)
{
    if (entries.empty()) return;

    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.32f, 0.23f, 0.06f, 0.45f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.95f, 0.68f, 0.18f, 0.85f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 6.0f));

    bool dismissed = false;
    bool triggerAutoFix = false;
    if (ImGui::BeginChild("##MissingResourcesBanner", ImVec2(0.0f, 0.0f),
        ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar))
    {
        const std::string header = std::string(ICON_FA_TRIANGLE_EXCLAMATION " ") +
            LOCAL_FMT("ResourceEditor.MissingResourcesTitle", entries.size());
        ImGui::TextColored(ImVec4(1.0f, 0.80f, 0.30f, 1.0f), "%s", header.c_str());

        const std::string dismissLabel = LOCAL("ResourceEditor.Dismiss") + "##DismissMissing";
        const float dismissW = ImGui::CalcTextSize(LOCAL_CSTR("ResourceEditor.Dismiss")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        float buttonsW = dismissW;
        std::string autoFixLabel;
        if (onAutoFix)
        {
            autoFixLabel = LOCAL("ResourceEditor.AutoFixMissing") + "##AutoFixMissing";
            const float autoFixW = ImGui::CalcTextSize(LOCAL_CSTR("ResourceEditor.AutoFixMissing")).x + ImGui::GetStyle().FramePadding.x * 2.0f;
            buttonsW += autoFixW + ImGui::GetStyle().ItemSpacing.x;
        }
        const float rightX = ImGui::GetContentRegionAvail().x - buttonsW;
        if (rightX > 0.0f)
        {
            ImGui::SameLine(ImGui::GetCursorPosX() + rightX);
            if (onAutoFix)
            {
                if (ImGui::SmallButton(autoFixLabel.c_str()))
                    triggerAutoFix = true;
                ImGui::SameLine();
            }
            if (ImGui::SmallButton(dismissLabel.c_str()))
                dismissed = true;
        }

        ImGui::TextWrapped("%s", LOCAL_CSTR("ResourceEditor.MissingResourcesHint"));
        auto ellipsizeUtf8 = [](const std::string& text, float maxWidth) -> std::pair<std::string, bool>
        {
            if (maxWidth <= 0.0f || ImGui::CalcTextSize(text.c_str()).x <= maxWidth)
                return { text, false };

            const float ellipsisW = ImGui::CalcTextSize("...").x;
            if (maxWidth <= ellipsisW)
                return { "...", true };

            const float targetW = maxWidth - ellipsisW;
            std::size_t bestLen = 0;
            std::size_t i = 0;
            while (i < text.size())
            {
                std::size_t charLen = 1;
                const unsigned char c = static_cast<unsigned char>(text[i]);
                if ((c & 0xE0) == 0xC0) charLen = 2;
                else if ((c & 0xF0) == 0xE0) charLen = 3;
                else if ((c & 0xF8) == 0xF0) charLen = 4;
                if (i + charLen > text.size()) charLen = text.size() - i;

                const std::string prefix = text.substr(0, i + charLen);
                if (ImGui::CalcTextSize(prefix.c_str()).x > targetW)
                    break;
                bestLen = i + charLen;
                i += charLen;
            }
            return { text.substr(0, bestLen) + "...", true };
        };

        const float bulletReserve = ImGui::GetFontSize() + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        for (const auto& entry : entries)
        {
            const std::wstring& refText = !entry.AssetKey.empty() ? entry.AssetKey : entry.AssetPath;
            const std::string refStr = Engine::Unicode::WideToUtf8(refText);
            const std::string fullLine = !entry.PropertyName.empty()
                ? (entry.PropertyName + " (" + entry.ExpectedResourceType + "): " + refStr)
                : (entry.ExpectedResourceType + ": " + refStr);

            const float maxTextW = std::max(40.0f, ImGui::GetContentRegionAvail().x - bulletReserve);
            const auto [displayLine, truncated] = ellipsizeUtf8(fullLine, maxTextW);
            ImGui::Bullet();
            ImGui::TextUnformatted(displayLine.c_str());
            if (truncated && ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", fullLine.c_str());
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
    ImGui::Spacing();

    if (dismissed)
        entries.clear();
    if (triggerAutoFix && onAutoFix)
        onAutoFix();
}
