#include "AssetReferencePanel.h"
#include "AssetFileActions.h"
#include "AssetManager.h"
#include "AssetPathRelocation.h"
#include "EditorUI.h"
#include "Functions.h"
#include "PlatformPath.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include "InputManager.h"
#include "ImGuiManager.h"
#include "NotificationManager.h"
#include <imgui.h>
#include <algorithm>
#include <unordered_set>
#include <utility>

namespace Editor
{
    struct AssetReferenceIssueFilter
    {
        std::optional<EAssetReferenceIssueType> Type;
        const char* LabelKey;
    };

    static constexpr AssetReferenceIssueFilter issueFilters[] = {
        { std::nullopt, "AssetRef.FilterAll" },
        { EAssetReferenceIssueType::MissingFile, "AssetRef.FilterMissing" },
        { EAssetReferenceIssueType::ExternalAbsolutePath, "AssetRef.FilterExternal" },
        { EAssetReferenceIssueType::InvalidExtension, "AssetRef.FilterInvalid" },
        { EAssetReferenceIssueType::UnreadableDocument, "AssetRef.UnreadableDocument" },
        { EAssetReferenceIssueType::OutsideRuntimeRoots, "AssetRef.OutsideMounts" },
    };

    static std::wstring NormalizeReferenceSearch(std::wstring text)
    {
        text = Engine::ToLowerWStr(std::move(text));
        std::ranges::replace(text, L'\\', L'/');
        return text;
    }

    static void DrawReferenceAssetActions(const char* popupId, const std::filesystem::path& path)
    {
        if (!ImGui::BeginPopupContextItem(popupId))
            return;

        std::error_code error;
        const bool open = ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.Open")).c_str(),
            nullptr, false, !std::filesystem::is_directory(path, error));
        const bool reveal = ImGui::MenuItem(
            (ICON_FA_MAGNIFYING_GLASS " " + LOCAL_LABEL("EditorCommon.RevealInContentBrowser")).c_str());
        ImGui::EndPopup();

        if (open)
            AssetFileActions::Open(path);
        if (reveal)
            AssetFileActions::RevealInContentBrowser(path);
    }

    static void DrawReferenceRowActions(const std::filesystem::path& path, const std::string& display)
    {
        const bool hovered = ImGui::IsItemHovered();
        DrawReferenceAssetActions("ReferenceRowActions", path);
        if (hovered)
            ImGui::SetTooltip("%s", display.c_str());
    }

    static void DrawIssueSourceActions(const std::filesystem::path& sourceFile, const std::string& display)
    {
        ImGui::TextUnformatted(display.c_str());
        if (sourceFile.empty() || !ImGui::IsItemVisible())
            return;

        const bool hovered = ImGui::IsItemHovered();
        const auto source = Engine::PlatformPath::GetProjectRoot() / sourceFile;
        // A refreshed issue at the same row index must not inherit another file's open menu.
        ImGui::PushID(static_cast<int>(std::filesystem::hash_value(source)));
        DrawReferenceAssetActions("IssueSourceActions", source);
        ImGui::PopID();

        if (!hovered)
            return;

        ImGui::SetTooltip("%s\n%s", display.c_str(), LOCAL_CSTR("AssetRef.SourceActionsHint"));
        if (Engine::InputManager::Get().IsMouseButtonDoubleClicked(Engine::EMouseButton::Left))
        {
            std::error_code error;
            if (std::filesystem::is_directory(source, error))
                AssetFileActions::RevealInContentBrowser(source);
            else
                AssetFileActions::Open(source);
        }
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
                RunValidation(m_HasScanned ? AssetReferenceTimingKind::Automatic : AssetReferenceTimingKind::Initial);

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

    void AssetReferencePanel::RequestScanRefresh(AssetReferenceChanges changes)
    {
        if (!m_HasScanned || changes.Empty())
            return;

        m_PendingScanChanges.Merge(std::move(changes));
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
            m_ReferenceSearchDirty |= !m_ReferenceTarget.empty();
            m_ReferenceTarget.clear();
            m_ReferenceTargetMissing = false;
            return;
        }

        const auto physical = Engine::PlatformPath::ResolveUri(asset);
        std::error_code error;
        auto resolved = std::filesystem::canonical(physical, error);
        if (error)
            resolved = physical.lexically_normal();
        m_ReferenceSearchDirty |= resolved != m_ReferenceTarget;
        m_ReferenceTarget = std::move(resolved);
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
                UpdateReferenceLeaves(children, links);
            }
        };
        prepareChildren(m_Dependencies);
        prepareChildren(m_Users);
        m_ReferenceSearchDirty = true;
    }

    void AssetReferencePanel::UpdateReferenceLeaves(ReferenceChildren& children, const ReferenceAdjacency& links)
    {
        children.AllLeaves = std::ranges::none_of(children.Paths,
            [&links](const auto& path) { return links.contains(path); });
    }

    void AssetReferencePanel::ApplyReferenceDelta(const AssetReferenceScanDelta& delta)
    {
        if (delta.RemovedReferences.empty() && delta.AddedReferences.empty())
            return;

        const auto updatePaths = [&](ReferenceAdjacency& links, auto ownerPath, auto childPath)
        {
            std::unordered_map<std::filesystem::path, bool> hadChildren;
            for (const auto& link : delta.RemovedReferences)
            {
                const auto& owner = link.*ownerPath;
                auto entry = links.find(owner);
                hadChildren.try_emplace(owner, true);

                auto& paths = entry->second.Paths;
                paths.erase(std::ranges::lower_bound(paths, link.*childPath));
                if (paths.empty())
                    links.erase(entry);
            }
            for (const auto& link : delta.AddedReferences)
            {
                const auto& owner = link.*ownerPath;
                auto [entry, inserted] = links.try_emplace(owner);
                hadChildren.try_emplace(owner, !inserted);

                auto& paths = entry->second.Paths;
                paths.insert(std::ranges::lower_bound(paths, link.*childPath), link.*childPath);
            }
            return hadChildren;
        };
        const auto changedDependencies = updatePaths(m_Dependencies, &AssetReferenceLink::Source,
            &AssetReferenceLink::Target);
        const auto changedUsers = updatePaths(m_Users, &AssetReferenceLink::Target, &AssetReferenceLink::Source);

        const auto refreshLeaves = [](ReferenceAdjacency& links, const ReferenceAdjacency& reverse, const auto& changed)
        {
            std::unordered_set<std::filesystem::path> owners;
            for (const auto& [path, hadChildren] : changed)
            {
                const bool hasChildren = links.contains(path);
                if (hasChildren)
                    owners.insert(path);
                if (hasChildren == hadChildren)
                    continue;

                // A child that gains or loses its last branch changes how its parents are drawn.
                if (const auto parents = reverse.find(path); parents != reverse.end())
                    owners.insert(parents->second.Paths.begin(), parents->second.Paths.end());
            }
            for (const auto& owner : owners)
                UpdateReferenceLeaves(links.at(owner), links);
        };
        refreshLeaves(m_Dependencies, m_Users, changedDependencies);
        refreshLeaves(m_Users, m_Dependencies, changedUsers);
        m_ReferenceSearchDirty = true;
    }

    void AssetReferencePanel::CollectReferenceMatches(const ReferenceAdjacency& links, std::wstring_view query,
        ReferencePaths& matches) const
    {
        std::unordered_set<std::filesystem::path> visited{ m_ReferenceTarget };
        ReferencePaths pending{ m_ReferenceTarget };
        while (!pending.empty())
        {
            const auto asset = std::move(pending.back());
            pending.pop_back();
            const auto children = links.find(asset);
            if (children == links.end())
                continue;

            for (const auto& path : children->second.Paths)
            {
                if (!visited.insert(path).second)
                    continue;

                const auto uri = Engine::ToLowerWStr(Engine::PlatformPath::ToVirtualUri(path));
                if (uri.find(query) != std::wstring::npos ||
                    Engine::ToLowerWStr(path.generic_wstring()).find(query) != std::wstring::npos)
                    matches.push_back(path);

                if (links.contains(path))
                    pending.push_back(path);
            }
        }
        std::ranges::sort(matches);
    }

    void AssetReferencePanel::UpdateReferenceMatches()
    {
        m_ReferenceSearchDirty = false;
        m_MatchingUsers.clear();
        m_MatchingDependencies.clear();
        if (m_ReferenceSearch[0] == '\0')
            return;

        const auto query = NormalizeReferenceSearch(Engine::StrToWStr(m_ReferenceSearch));
        CollectReferenceMatches(m_Users, query, m_MatchingUsers);
        CollectReferenceMatches(m_Dependencies, query, m_MatchingDependencies);
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
            RunValidation(AssetReferenceTimingKind::Manual);
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
        {
            DrawReferenceAssetActions("ReferenceTargetActions", m_ReferenceTarget);
            if (EditorUI::SearchBar("ReferencePathSearch", m_ReferenceSearch, sizeof(m_ReferenceSearch),
                "AssetRef.SearchHint"))
                m_ReferenceSearchDirty = true;
        }
        ImGui::Separator();

        if (m_ReferenceTargetMissing)
        {
            EditorUI::EmptyState("AssetRef.ReferenceTargetMissing");
            return;
        }

        if (m_ReferenceSearchDirty)
            UpdateReferenceMatches();
        const bool searching = m_ReferenceSearch[0] != '\0';
        if (searching && m_MatchingUsers.empty() && m_MatchingDependencies.empty())
        {
            EditorUI::EmptyState("AssetRef.NoMatchingReferences");
            return;
        }

        std::filesystem::path next;
        if (ImGui::BeginChild(searching ? "AssetReferenceSearchResults" : "AssetReferenceLists"))
        {
            if (searching)
            {
                DrawReferenceMatches("AssetRef.UsedBy", m_MatchingUsers, next);
                ImGui::Separator();
                DrawReferenceMatches("AssetRef.Uses", m_MatchingDependencies, next);
            }
            else
            {
                DrawReferenceTree("AssetRef.UsedBy", m_Users, next);
                ImGui::Separator();
                DrawReferenceTree("AssetRef.Uses", m_Dependencies, next);
            }
        }
        ImGui::EndChild();
        NavigateToReference(next);
    }

    void AssetReferencePanel::DrawReferenceMatches(const char* labelKey, const ReferencePaths& matches,
        std::filesystem::path& next)
    {
        ImGui::TextUnformatted(LOCAL_CSTR(labelKey));
        if (matches.empty())
        {
            ImGui::TextDisabled("%s", LOCAL_CSTR("AssetRef.NoMatchingReferences"));
            return;
        }

        ImGui::PushID(labelKey);
        ImGui::PushID(static_cast<int>(std::filesystem::hash_value(m_ReferenceTarget)));
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(matches.size()));
        while (clipper.Step())
        {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
            {
                const auto& path = matches[index];
                const auto display = Engine::WStrToStr(Engine::PlatformPath::ToVirtualUri(path));
                ImGui::PushID(static_cast<int>(std::filesystem::hash_value(path)));
                if (ImGui::Selectable((display + "###ReferenceMatch").c_str()))
                    next = path;
                DrawReferenceRowActions(path, display);
                ImGui::PopID();
            }
        }
        ImGui::PopID();
        ImGui::PopID();
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
        DrawReferenceRowActions(path, display);

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
        EditorUI::ToolbarLayout toolbar;
        if (toolbar.Button("IssueScan", ICON_FA_MAGNIFYING_GLASS, "AssetRef.ScanAll"))
            RunValidation(AssetReferenceTimingKind::Manual);

        ImGui::BeginDisabled(m_FixableIssueCount == 0);
        const bool repair = toolbar.Button("IssueRepairAll", ICON_FA_WRENCH, "AssetRef.FixSafe");
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", LOCAL_CSTR("AssetRef.FixSafeScope"));
        if (repair)
            FixAllSafe();

        const bool filterChanged = DrawIssueTypeFilter();
        const bool searchChanged = EditorUI::SearchBar("AssetReferenceSearch", m_SearchFilter,
            sizeof(m_SearchFilter), "AssetRef.IssueSearchHint");
        if (filterChanged || searchChanged)
            UpdateVisibleIssues();

        if (!m_StatusMessage.empty())
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", LOCAL_FMT(m_StatusMessage, m_FixedCount).c_str());
    }

    bool AssetReferencePanel::DrawIssueTypeFilter()
    {
        const auto getLabel = [this](const AssetReferenceIssueFilter& option)
        {
            size_t count = m_Scan.Issues.size();
            if (option.Type)
            {
                const auto found = m_IssueTypeCounts.find(*option.Type);
                count = found == m_IssueTypeCounts.end() ? 0 : found->second;
            }
            return LOCAL_FMT("AssetRef.FilterCount", LOCAL(option.LabelKey), count);
        };

        const auto preview = getLabel(issueFilters[m_IssueFilterIndex]);
        const float width = std::max(ImGui::GetFontSize() * 12.0f, ImGui::CalcTextSize(preview.c_str()).x +
            ImGui::GetFrameHeight() + ImGui::GetStyle().FramePadding.x * 2.0f);
        EditorUI::SameLineIfFits(width);
        ImGui::SetNextItemWidth(std::min(width, ImGui::GetContentRegionAvail().x));
        if (!ImGui::BeginCombo("##ReferenceIssueType", preview.c_str()))
            return false;

        const auto previous = m_IssueFilterIndex;
        for (size_t index = 0; index < IM_ARRAYSIZE(issueFilters); ++index)
        {
            const bool selected = index == m_IssueFilterIndex;
            ImGui::PushID(static_cast<int>(index));
            if (ImGui::Selectable((getLabel(issueFilters[index]) + "###IssueFilterOption").c_str(), selected))
                m_IssueFilterIndex = index;
            if (selected)
                ImGui::SetItemDefaultFocus();
            ImGui::PopID();
        }
        ImGui::EndCombo();
        return previous != m_IssueFilterIndex;
    }

    void AssetReferencePanel::UpdateVisibleIssues()
    {
        m_VisibleIssueIndices.clear();
        AppendVisibleIssues(0);
    }

    void AssetReferencePanel::AppendVisibleIssues(size_t first)
    {
        if (first == m_Scan.Issues.size())
            return;

        const auto& type = issueFilters[m_IssueFilterIndex].Type;
        const auto query = NormalizeReferenceSearch(Engine::StrToWStr(m_SearchFilter));
        const auto matches = [&query](std::wstring text)
        {
            return NormalizeReferenceSearch(std::move(text)).find(query) != std::wstring::npos;
        };

        for (size_t index = first; index < m_Scan.Issues.size(); ++index)
        {
            const auto& issue = m_Scan.Issues[index];
            if (type && issue.IssueType != *type)
                continue;

            if (query.empty() || matches(issue.SourceFile.generic_wstring()) ||
                matches(Engine::StrToWStr(issue.PropertyName)) || matches(Engine::StrToWStr(issue.TargetReference)))
                m_VisibleIssueIndices.push_back(index);
        }
    }

    void AssetReferencePanel::CountAddedIssues(size_t first)
    {
        for (size_t index = first; index < m_Scan.Issues.size(); ++index)
        {
            const auto& issue = m_Scan.Issues[index];
            ++m_IssueTypeCounts[issue.IssueType];
            if (issue.CanAutoFix)
                ++m_FixableIssueCount;
        }
    }

    void AssetReferencePanel::ApplyIssueDelta(const AssetReferenceScanDelta& delta)
    {
        for (const auto& issue : delta.RemovedIssues)
        {
            --m_IssueTypeCounts.at(issue.IssueType);
            if (issue.CanAutoFix)
                --m_FixableIssueCount;
        }

        if (!delta.RemovedIssues.empty())
        {
            size_t kept = 0;
            size_t removedBefore = 0;
            for (const size_t index : m_VisibleIssueIndices)
            {
                while (removedBefore < delta.RemovedIssues.size() &&
                    delta.RemovedIssues[removedBefore].PreviousIndex < index)
                    ++removedBefore;
                if (removedBefore < delta.RemovedIssues.size() && delta.RemovedIssues[removedBefore].PreviousIndex == index)
                    continue;

                m_VisibleIssueIndices[kept++] = index - removedBefore;
            }
            m_VisibleIssueIndices.resize(kept);
        }
        CountAddedIssues(delta.FirstAddedIssue);
        AppendVisibleIssues(delta.FirstAddedIssue);
    }

    void AssetReferencePanel::DrawSummaryStats()
    {
        if (!m_HasScanned)
        {
            ImGui::TextDisabled("%s", LOCAL_CSTR("AssetRef.ScanRequired"));
            return;
        }

        const auto count = LOCAL_FMT("AssetRef.VisibleIssueCount", m_VisibleIssueIndices.size(), m_Scan.Issues.size());
        ImGui::TextUnformatted(count.c_str());
        const auto fixable = LOCAL_FMT("AssetRef.AutoFixable", m_FixableIssueCount);
        EditorUI::SameLineIfFits(ImGui::CalcTextSize(fixable.c_str()).x);
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", fixable.c_str());
    }

    void AssetReferencePanel::DrawIssuesTable()
    {
        if (!m_HasScanned)
            return;
        if (m_VisibleIssueIndices.empty())
        {
            EditorUI::EmptyState(m_Scan.Issues.empty() ? "AssetRef.NoIssues" : "AssetRef.NoMatchingIssues");
            return;
        }

        static const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        std::optional<size_t> requestedRepair;
        if (ImGui::BeginTable("##IssuesTable", 6, flags))
        {
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.IssueType").c_str(), ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.SourceAsset").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn(LOCAL_LABEL("EditorCommon.Property").c_str(), ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.TargetReference").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.SuggestedFix").c_str(), ImGuiTableColumnFlags_WidthStretch, 0.30f);
            ImGui::TableSetupColumn(LOCAL_LABEL("AssetRef.FixAction").c_str(), ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableHeadersRow();

            for (const size_t i : m_VisibleIssueIndices)
            {
                const auto& issue = m_Scan.Issues[i];
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
                if (ImGui::TableSetColumnIndex(1))
                    DrawIssueSourceActions(issue.SourceFile, Engine::WStrToStr(issue.SourceFile.generic_wstring()));

                // Column 2: Property
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(issue.PropertyName.c_str());

                // Column 3: Target Reference
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(issue.TargetReference.c_str());

                // Column 4: Suggested Fix
                ImGui::TableSetColumnIndex(4);
                if (!issue.SuggestedFix.empty())
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", Engine::WStrToStr(issue.SuggestedFix.wstring()).c_str());
                else
                    ImGui::TextDisabled(LOCAL_CSTR("EditorCommon.None"));

                // Column 5: Action
                ImGui::TableSetColumnIndex(5);
                if (issue.CanAutoFix)
                {
                    if (ImGui::SmallButton(LOCAL_CSTR("AssetRef.BtnFix")))
                    {
                        requestedRepair = i;
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
        if (requestedRepair)
            FixSelectedIssue(*requestedRepair);
    }

    void AssetReferencePanel::RunValidation(AssetReferenceTimingKind kind)
    {
        using Clock = std::chrono::steady_clock;
        using Milliseconds = std::chrono::duration<double, std::milli>;
        const auto start = Clock::now();
        AssetReferenceScanTiming timing;
        AssetReferenceScanDelta delta;
        constexpr std::size_t maxIncrementalScans = 32;
        const bool incremental = kind == AssetReferenceTimingKind::Automatic && !m_PendingScanChanges.RequiresFullScan &&
            m_IncrementalScans < maxIncrementalScans &&
            AssetReferenceValidator::RefreshProjectDocuments(m_ScanIndex, m_PendingScanChanges.Documents, m_Scan, delta, &timing);
        if (incremental)
        {
            kind = AssetReferenceTimingKind::Incremental;
            ++m_IncrementalScans;
        }
        else
        {
            m_Scan = AssetReferenceValidator::ScanProject(&timing, &m_ScanIndex);
            m_IncrementalScans = 0;
        }
        const auto scanned = Clock::now();
        m_PendingScanChanges = {};

        if (incremental)
        {
            ApplyIssueDelta(delta);
            ApplyReferenceDelta(delta);
        }
        else
        {
            m_IssueTypeCounts.clear();
            m_FixableIssueCount = 0;
            CountAddedIssues(0);
            UpdateVisibleIssues();
            RebuildReferenceTrees(m_Scan.References);
        }
        SetReferenceTarget(m_ReferenceTarget);
        m_HasScanned = true;
        m_ScanAfter.reset();
        m_StatusMessage = "AssetRef.ScanCompleted";
        timing.RefreshViewMs += Milliseconds(Clock::now() - scanned).count();
        AssetManager::Get().RecordAssetReferenceTiming(kind, ImGui::GetFrameCount(), start, &timing);
    }

    void AssetReferencePanel::FixSelectedIssue(size_t index)
    {
        if (index >= m_Scan.Issues.size())
            return;

        const auto& issue = m_Scan.Issues[index];
        if (!ImGuiManager::Get().CanRepairAssetReferenceSource(issue.SourceFile))
        {
            NotificationManager::Get().AddNotification(LOCAL("AssetRef.Title"), LOCAL("AssetRef.RepairBlocked"),
                ELogLevel::Warn);
            return;
        }

        ReportRepairResult(AssetReferenceValidator::FixAllSafeIssues({ issue }));
    }

    void AssetReferencePanel::FixAllSafe()
    {
        std::vector<AssetReferenceIssue> eligible;
        bool blocked = false;
        for (const auto& issue : m_Scan.Issues)
        {
            if (!issue.CanAutoFix)
                continue;
            if (ImGuiManager::Get().CanRepairAssetReferenceSource(issue.SourceFile))
                eligible.push_back(issue);
            else
                blocked = true;
        }
        if (blocked)
        {
            NotificationManager::Get().AddNotification(LOCAL("AssetRef.Title"), LOCAL("AssetRef.RepairBlocked"),
                ELogLevel::Warn);
        }
        if (!eligible.empty())
            ReportRepairResult(AssetReferenceValidator::FixAllSafeIssues(eligible));
    }

    void AssetReferencePanel::ReportRepairResult(size_t count)
    {
        RunValidation(AssetReferenceTimingKind::AfterRepair);
        if (count == 0)
        {
            NotificationManager::Get().AddNotification(LOCAL("AssetRef.Title"), LOCAL("AssetRef.RepairFailed"),
                ELogLevel::Warn);
            return;
        }

        m_FixedCount = count;
        m_StatusMessage = "AssetRef.FixedIssues";
    }
}
