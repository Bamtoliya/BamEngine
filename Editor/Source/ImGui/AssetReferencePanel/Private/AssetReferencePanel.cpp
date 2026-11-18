#include "AssetReferencePanel.h"
#include "AssetFileActions.h"
#include "AssetPathRelocation.h"
#include "EditorUI.h"
#include "Functions.h"
#include "PlatformPath.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include "InputManager.h"
#include <imgui.h>
#include <algorithm>
#include <utility>

namespace Editor
{
    static void DrawReferenceAssetActions(const char* popupId, const std::filesystem::path& path)
    {
        if (!ImGui::BeginPopupContextItem(popupId))
            return;

        const bool open = ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.Open")).c_str());
        const bool reveal = ImGui::MenuItem(
            (ICON_FA_MAGNIFYING_GLASS " " + LOCAL_LABEL("EditorCommon.RevealInContentBrowser")).c_str());
        ImGui::EndPopup();

        if (open)
            AssetFileActions::Open(path);
        if (reveal)
            AssetFileActions::RevealInContentBrowser(path);
    }

    AssetReferencePanel::AssetReferencePanel()
    {
        m_Name = L"AssetReferenceChecker";
        m_DisplayNameKey = "AssetRef.Title";
    }

    void AssetReferencePanel::Draw()
    {
        if (!m_Open) return;

        ImGui::SetNextWindowSize(ImVec2(800.0f, 500.0f), ImGuiCond_FirstUseEver);
        const std::string title = LOCAL_CSTR(m_DisplayNameKey.c_str()) + std::string("###AssetRefPanel");

        if (BeginPanelWindow(title.c_str(), m_Open))
        {
            if (m_ScanAfter && std::chrono::steady_clock::now() >= *m_ScanAfter)
                RunValidation();

            if (ImGui::BeginTabBar("AssetReferenceViews"))
            {
                if (ImGui::BeginTabItem(LOCAL_LABEL("AssetRef.ProblemsTab").c_str()))
                {
                    DrawToolbar();
                    ImGui::Separator();
                    DrawSummaryStats();
                    ImGui::Separator();
                    DrawIssuesTable();
                    ImGui::EndTabItem();
                }

                const auto flags = m_SelectReferencesTab ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None;
                if (ImGui::BeginTabItem(LOCAL_LABEL("AssetRef.ReferencesTab").c_str(), nullptr, flags))
                {
                    DrawReferences();
                    ImGui::EndTabItem();
                }
                m_SelectReferencesTab = false;
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }

    void AssetReferencePanel::ShowReferences(const std::filesystem::path& asset)
    {
        NavigateToReference(asset);
        if (!m_HasScanned)
            m_ScanAfter = std::chrono::steady_clock::now();
        m_SelectReferencesTab = true;
        Focus();
    }

    void AssetReferencePanel::RequestScanRefresh()
    {
        if (!m_HasScanned)
            return;

        // File writes can arrive as a burst of change notifications.
        m_ScanAfter = std::chrono::steady_clock::now() + std::chrono::milliseconds(200);
    }

    void AssetReferencePanel::PrepareAssetPathRelocation(AssetPathRelocation& relocation)
    {
        relocation.Add(m_ReferenceTarget);
        for (auto& target : m_BackTargets)
            relocation.Add(target);
        for (auto& target : m_ForwardTargets)
            relocation.Add(target);
        relocation.AfterCommit([this]() { RequestScanRefresh(); });
    }

    void AssetReferencePanel::NavigateToReference(const std::filesystem::path& asset)
    {
        if (asset.empty())
            return;

        auto previous = m_ReferenceTarget;
        SetReferenceTarget(asset);
        if (previous.empty() || Engine::PlatformPath::IsSamePath(previous, m_ReferenceTarget))
            return;

        constexpr size_t maxHistory = 64;
        if (m_BackTargets.size() == maxHistory)
            m_BackTargets.erase(m_BackTargets.begin());
        m_BackTargets.push_back(std::move(previous));
        m_ForwardTargets.clear();
    }

    void AssetReferencePanel::StepReferenceHistory(bool forward)
    {
        auto& from = forward ? m_ForwardTargets : m_BackTargets;
        auto& to = forward ? m_BackTargets : m_ForwardTargets;
        while (!from.empty())
        {
            auto next = std::move(from.back());
            from.pop_back();
            if (Engine::PlatformPath::IsSamePath(next, m_ReferenceTarget))
                continue;

            to.push_back(std::move(m_ReferenceTarget));
            SetReferenceTarget(next);
            return;
        }
    }

    void AssetReferencePanel::SetReferenceTarget(const std::filesystem::path& asset)
    {
        if (asset.empty())
        {
            m_ReferenceTarget.clear();
            m_ReferenceTargetMissing = false;
            return;
        }

        const auto physical = Engine::PlatformPath::ResolveUri(asset);
        std::error_code error;
        const auto resolved = std::filesystem::canonical(physical, error);
        m_ReferenceTarget = error ? physical.lexically_normal() : resolved;
        m_ReferenceTargetMissing = !std::filesystem::is_regular_file(m_ReferenceTarget, error);
    }

    void AssetReferencePanel::RebuildReferenceTrees(const std::vector<AssetReferenceLink>& references)
    {
        m_Dependencies.clear();
        m_Users.clear();
        for (const auto& link : references)
        {
            m_Dependencies[link.Source].Paths.push_back(link.Target);
            m_Users[link.Target].Paths.push_back(link.Source);
        }

        const auto prepareChildren = [](ReferenceAdjacency& links)
        {
            for (auto& entry : links)
            {
                auto& children = entry.second;
                std::ranges::sort(children.Paths);
                children.AllLeaves = std::ranges::none_of(children.Paths,
                    [&links](const auto& path) { return links.contains(path); });
            }
        };
        prepareChildren(m_Dependencies);
        prepareChildren(m_Users);
    }

    void AssetReferencePanel::DrawReferenceToolbar()
    {
        EditorUI::ToolbarLayout toolbar;
        EditorUI::IconButtonOptions options;
        options.Enabled = !m_BackTargets.empty();
        options.ShortcutHint = "X1";
        const bool back = toolbar.Icon("ReferenceBack", ICON_FA_ARROW_LEFT, "EditorCommon.NavigateBack", options);
        options.Enabled = !m_ForwardTargets.empty();
        options.ShortcutHint = "X2";
        const bool forward = toolbar.Icon("ReferenceForward", ICON_FA_ARROW_RIGHT, "EditorCommon.NavigateForward", options);
        const bool scan = toolbar.Button("ReferenceScan", ICON_FA_MAGNIFYING_GLASS, "AssetRef.ScanAll");

        const bool allowMouse = ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) &&
            !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() && !ImGui::GetDragDropPayload() &&
            !ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);
        auto& input = Engine::InputManager::Get();
        if (back || (allowMouse && input.IsMouseButtonDown(Engine::EMouseButton::X1)))
            StepReferenceHistory(false);
        else if (forward || (allowMouse && input.IsMouseButtonDown(Engine::EMouseButton::X2)))
            StepReferenceHistory(true);
        if (scan)
            RunValidation();
    }

    void AssetReferencePanel::DrawReferences()
    {
        DrawReferenceToolbar();

        if (m_ReferenceTarget.empty())
        {
            EditorUI::EmptyState("AssetRef.ReferenceTargetRequired");
            return;
        }

        const auto target = Engine::WStrToStr(Engine::PlatformPath::ToVirtualUri(m_ReferenceTarget));
        ImGui::TextWrapped("%s", target.c_str());
        if (!m_ReferenceTargetMissing)
            DrawReferenceAssetActions("ReferenceTargetActions", m_ReferenceTarget);
        ImGui::Separator();

        if (m_ReferenceTargetMissing)
        {
            EditorUI::EmptyState("AssetRef.ReferenceTargetMissing");
            return;
        }

        std::filesystem::path next;
        if (ImGui::BeginChild("AssetReferenceLists"))
        {
            DrawReferenceTree("AssetRef.UsedBy", m_Users, next);
            ImGui::Separator();
            DrawReferenceTree("AssetRef.Uses", m_Dependencies, next);
        }
        ImGui::EndChild();
        NavigateToReference(next);
    }

    void AssetReferencePanel::DrawReferenceTree(const char* labelKey, const ReferenceAdjacency& links,
        std::filesystem::path& next)
    {
        ImGui::TextUnformatted(LOCAL_CSTR(labelKey));
        const auto root = links.find(m_ReferenceTarget);
        if (root == links.end())
        {
            ImGui::TextDisabled("%s", LOCAL_CSTR("AssetRef.NoDirectReferences"));
            return;
        }

        ImGui::PushID(labelKey);
        ImGui::PushID(static_cast<int>(std::filesystem::hash_value(m_ReferenceTarget)));
        ReferencePaths ancestors{ m_ReferenceTarget };
        DrawReferenceChildren(root->second, links, ancestors, next);
        ImGui::PopID();
        ImGui::PopID();
    }

    void AssetReferencePanel::DrawReferenceChildren(const ReferenceChildren& children, const ReferenceAdjacency& links,
        ReferencePaths& ancestors, std::filesystem::path& next)
    {
        if (!children.AllLeaves)
        {
            for (const auto& path : children.Paths)
                DrawReferenceBranch(path, links, ancestors, next);
            return;
        }

        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(children.Paths.size()));
        while (clipper.Step())
        {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
                DrawReferenceBranch(children.Paths[index], links, ancestors, next);
        }
    }

    void AssetReferencePanel::DrawReferenceBranch(const std::filesystem::path& path, const ReferenceAdjacency& links,
        ReferencePaths& ancestors, std::filesystem::path& next)
    {
        constexpr size_t maxDepth = 24;
        const auto children = links.find(path);
        const bool cycle = std::ranges::find(ancestors, path) != ancestors.end();
        const bool atDepthLimit = !cycle && children != links.end() && ancestors.size() >= maxDepth;
        const bool expandable = children != links.end() && !cycle && !atDepthLimit;
        const auto display = Engine::WStrToStr(Engine::PlatformPath::ToVirtualUri(path));
        const char* noteKey = cycle ? "AssetRef.ReferenceCycle" :
            atDepthLimit ? "AssetRef.ReferenceDepthLimit" : nullptr;
        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (!expandable)
            flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

        ImGui::PushID(static_cast<int>(std::filesystem::hash_value(path)));
        const bool open = ImGui::TreeNodeEx("Reference", flags, "%s%s%s", display.c_str(),
            noteKey ? "  " : "", noteKey ? LOCAL_CSTR(noteKey) : "");
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            next = path;
        const bool hovered = ImGui::IsItemHovered();
        DrawReferenceAssetActions("ReferenceRowActions", path);
        if (hovered)
            ImGui::SetTooltip("%s", display.c_str());

        if (open && expandable)
        {
            ancestors.push_back(path);
            DrawReferenceChildren(children->second, links, ancestors, next);
            ancestors.pop_back();
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    void AssetReferencePanel::DrawToolbar()
    {
        if (ImGui::Button(LOCAL_CSTR("AssetRef.ScanAll")))
        {
            RunValidation();
        }

        ImGui::SameLine();
        const bool canFixAny = std::ranges::any_of(m_Issues, [](const auto& i) { return i.CanAutoFix; });
        if (!canFixAny) ImGui::BeginDisabled();
        if (ImGui::Button(LOCAL_CSTR("AssetRef.FixSafe")))
        {
            FixAllSafe();
        }
        if (!canFixAny) ImGui::EndDisabled();

        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        const std::string filterLabels[] = { LOCAL("AssetRef.FilterAll"), LOCAL("AssetRef.FilterMissing"), LOCAL("AssetRef.FilterExternal"), LOCAL("AssetRef.FilterInvalid") };
        const char* filterItems[] = { filterLabels[0].c_str(), filterLabels[1].c_str(), filterLabels[2].c_str(), filterLabels[3].c_str() };
        ImGui::Combo("##FilterType", &m_FilterType, filterItems, IM_ARRAYSIZE(filterItems));

        ImGui::SameLine();
        EditorUI::SearchBarOptions searchOptions;
        searchOptions.Width = 200.0f;
        EditorUI::DrawSearchBar("AssetReferenceSearch", m_SearchFilter, sizeof(m_SearchFilter),
            "AssetRef.SearchHint", searchOptions);

        if (!m_StatusMessage.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", LOCAL_FMT(m_StatusMessage, m_FixedCount).c_str());
        }
    }

    void AssetReferencePanel::DrawSummaryStats()
    {
        if (!m_HasScanned)
        {
            ImGui::TextDisabled("%s", LOCAL_CSTR("AssetRef.ScanRequired"));
            return;
        }

        size_t missingCount = 0;
        size_t extCount = 0;
        size_t invalidExtCount = 0;
        size_t fixableCount = 0;

        for (const auto& issue : m_Issues)
        {
            if (issue.IssueType == EAssetReferenceIssueType::MissingFile) ++missingCount;
            else if (issue.IssueType == EAssetReferenceIssueType::ExternalAbsolutePath) ++extCount;
            else if (issue.IssueType == EAssetReferenceIssueType::InvalidExtension) ++invalidExtCount;
            if (issue.CanAutoFix) ++fixableCount;
        }

        ImGui::Text(LOCAL_CSTR("AssetRef.TotalIssues"), m_Issues.size());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.3f, 0.3f, 1.0f), LOCAL_CSTR("AssetRef.Missing"), missingCount);
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.2f, 1.0f), LOCAL_CSTR("AssetRef.External"), extCount);
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), LOCAL_CSTR("AssetRef.InvalidExt"), invalidExtCount);
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), LOCAL_CSTR("AssetRef.AutoFixable"), fixableCount);
    }

    void AssetReferencePanel::DrawIssuesTable()
    {
        static const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##IssuesTable", 6, flags))
        {
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.IssueType").c_str(), ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.SourceAsset").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn(LOCAL_LABEL("EditorCommon.Property").c_str(), ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.TargetReference").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.SuggestedFix").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.30f);
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.FixAction").c_str(), ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableHeadersRow();

            for (size_t i = 0; i < m_Issues.size(); ++i)
            {
                const auto& issue = m_Issues[i];

                // 1. 타입 필터
                if (m_FilterType == 1 && issue.IssueType != EAssetReferenceIssueType::MissingFile) continue;
                if (m_FilterType == 2 && issue.IssueType != EAssetReferenceIssueType::ExternalAbsolutePath) continue;
                if (m_FilterType == 3 && issue.IssueType != EAssetReferenceIssueType::InvalidExtension) continue;

                // 2. 검색어 필터
                if (m_SearchFilter[0] != '\0')
                {
                    const std::string sFilter = m_SearchFilter;
                    if (Engine::WStrToStr(issue.SourceFile.generic_wstring()).find(sFilter) == std::string::npos &&
                        issue.TargetReference.find(sFilter) == std::string::npos)
                        continue;
                }

                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(i));

                // Column 0: Type
                ImGui::TableSetColumnIndex(0);
                if (issue.IssueType == EAssetReferenceIssueType::MissingFile)
                    ImGui::TextColored(ImVec4(0.95f, 0.3f, 0.3f, 1.0f), LOCAL_CSTR("AssetRef.MissingStatus"), ICON_FA_TRIANGLE_EXCLAMATION);
                else if (issue.IssueType == EAssetReferenceIssueType::ExternalAbsolutePath)
                    ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.2f, 1.0f), LOCAL_CSTR("AssetRef.ExternalStatus"), ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE);
                else if (issue.IssueType == EAssetReferenceIssueType::InvalidExtension)
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), LOCAL_CSTR("AssetRef.InvalidExtensionStatus"), ICON_FA_FILE_CIRCLE_QUESTION);
                else if (issue.IssueType == EAssetReferenceIssueType::UnreadableDocument)
                    ImGui::TextDisabled("%s", LOCAL_CSTR("AssetRef.UnreadableDocument"));
                else
                    ImGui::TextDisabled("%s", LOCAL_CSTR("AssetRef.OutsideMounts"));

                // Column 1: Source Asset
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(Engine::WStrToStr(issue.SourceFile.generic_wstring()).c_str());

                // Column 2: Property
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(issue.PropertyName.c_str());

                // Column 3: Target Reference
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(issue.TargetReference.c_str());

                // Column 4: Suggested Fix
                ImGui::TableSetColumnIndex(4);
                if (!issue.SuggestedFix.empty())
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", Engine::WStrToStr(issue.SuggestedFix.generic_wstring()).c_str());
                else
                    ImGui::TextDisabled(LOCAL_CSTR("EditorCommon.None"));

                // Column 5: Action
                ImGui::TableSetColumnIndex(5);
                if (issue.CanAutoFix)
                {
                    if (ImGui::SmallButton(LOCAL_CSTR("AssetRef.BtnFix")))
                    {
                        FixSelectedIssue(i);
                    }
                }
                else
                {
                    ImGui::TextDisabled("-");
                }

                ImGui::PopID();
            }

            ImGui::EndTable();
        }
    }

    void AssetReferencePanel::RunValidation()
    {
        auto scan = AssetReferenceValidator::ScanProject();
        m_Issues.swap(scan.Issues);
        RebuildReferenceTrees(scan.References);
        SetReferenceTarget(m_ReferenceTarget);
        m_HasScanned = true;
        m_ScanAfter.reset();
        m_StatusMessage = "AssetRef.ScanCompleted";
    }

    void AssetReferencePanel::FixSelectedIssue(size_t index)
    {
        if (index >= m_Issues.size()) return;
        if (AssetReferenceValidator::FixIssue(m_Issues[index]))
        {
            RunValidation();
            m_FixedCount = 1;
            m_StatusMessage = "AssetRef.FixedIssues";
        }
    }

    void AssetReferencePanel::FixAllSafe()
    {
        const size_t fixedCount = AssetReferenceValidator::FixAllSafeIssues(m_Issues);
        RunValidation();
        m_FixedCount = fixedCount;
        m_StatusMessage = "AssetRef.FixedIssues";
    }
}