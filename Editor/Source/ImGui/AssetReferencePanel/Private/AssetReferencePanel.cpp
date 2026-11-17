#include "AssetReferencePanel.h"
#include "EditorUI.h"
#include "Functions.h"
#include "PlatformPath.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include <imgui.h>
#include <algorithm>

namespace Editor
{
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
            if (m_ScanRequested)
            {
                RunValidation();
                m_ScanRequested = false;
            }

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
        SetReferenceTarget(asset);
        m_ScanRequested = !m_HasScanned;
        m_SelectReferencesTab = true;
        Focus();
    }

    void AssetReferencePanel::SetReferenceTarget(const std::filesystem::path& asset)
    {
        std::error_code error;
        const auto resolved = std::filesystem::canonical(asset, error);
        m_ReferenceTarget = error ? asset.lexically_normal() : resolved;
        UpdateReferenceLists();
    }

    void AssetReferencePanel::UpdateReferenceLists()
    {
        m_Dependencies.clear();
        m_Users.clear();
        for (const auto& link : m_Scan.References)
        {
            if (link.Source == m_ReferenceTarget)
                m_Dependencies.push_back(link.Target);
            if (link.Target == m_ReferenceTarget)
                m_Users.push_back(link.Source);
        }
        std::ranges::sort(m_Dependencies);
        std::ranges::sort(m_Users);
    }

    void AssetReferencePanel::DrawReferences()
    {
        if (ImGui::Button(LOCAL_LABEL("AssetRef.ScanAll").c_str()))
            RunValidation();

        if (m_ReferenceTarget.empty())
        {
            EditorUI::EmptyState("AssetRef.ReferenceTargetRequired");
            return;
        }

        const auto target = Engine::WStrToStr(Engine::PlatformPath::ToVirtualUri(m_ReferenceTarget));
        ImGui::TextWrapped("%s", target.c_str());
        ImGui::Separator();

        std::filesystem::path next;
        if (ImGui::BeginChild("AssetReferenceLists"))
        {
            const auto user = DrawReferenceList("AssetRef.UsedBy", m_Users);
            ImGui::Separator();
            const auto dependency = DrawReferenceList("AssetRef.Uses", m_Dependencies);
            next = user.empty() ? dependency : user;
        }
        ImGui::EndChild();
        if (!next.empty())
            SetReferenceTarget(next);
    }

    std::filesystem::path AssetReferencePanel::DrawReferenceList(const char* labelKey,
        const std::vector<std::filesystem::path>& paths)
    {
        ImGui::TextUnformatted(LOCAL_CSTR(labelKey));
        if (paths.empty())
        {
            ImGui::TextDisabled("%s", LOCAL_CSTR("AssetRef.NoDirectReferences"));
            return {};
        }

        std::filesystem::path next;
        ImGui::PushID(labelKey);
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(paths.size()));
        while (clipper.Step())
        {
            for (int index = clipper.DisplayStart; index < clipper.DisplayEnd; ++index)
            {
                const auto& path = paths[index];
                const auto display = Engine::WStrToStr(Engine::PlatformPath::ToVirtualUri(path));
                ImGui::PushID(index);
                if (ImGui::Selectable(display.c_str()))
                    next = path;
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("%s", display.c_str());
                ImGui::PopID();
            }
        }
        ImGui::PopID();
        return next;
    }

    void AssetReferencePanel::DrawToolbar()
    {
        if (ImGui::Button(LOCAL_CSTR("AssetRef.ScanAll")))
        {
            RunValidation();
        }

        ImGui::SameLine();
        const bool canFixAny = std::ranges::any_of(m_Scan.Issues, [](const auto& i) { return i.CanAutoFix; });
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

        for (const auto& issue : m_Scan.Issues)
        {
            if (issue.IssueType == EAssetReferenceIssueType::MissingFile) ++missingCount;
            else if (issue.IssueType == EAssetReferenceIssueType::ExternalAbsolutePath) ++extCount;
            else if (issue.IssueType == EAssetReferenceIssueType::InvalidExtension) ++invalidExtCount;
            if (issue.CanAutoFix) ++fixableCount;
        }

        ImGui::Text(LOCAL_CSTR("AssetRef.TotalIssues"), m_Scan.Issues.size());
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

            for (size_t i = 0; i < m_Scan.Issues.size(); ++i)
            {
                const auto& issue = m_Scan.Issues[i];

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
        m_Scan = AssetReferenceValidator::ScanProject();
        UpdateReferenceLists();
        m_HasScanned = true;
        m_StatusMessage = "AssetRef.ScanCompleted";
    }

    void AssetReferencePanel::FixSelectedIssue(size_t index)
    {
        if (index >= m_Scan.Issues.size()) return;
        if (AssetReferenceValidator::FixIssue(m_Scan.Issues[index]))
        {
            RunValidation();
            m_FixedCount = 1;
            m_StatusMessage = "AssetRef.FixedIssues";
        }
    }

    void AssetReferencePanel::FixAllSafe()
    {
        const size_t fixedCount = AssetReferenceValidator::FixAllSafeIssues(m_Scan.Issues);
        RunValidation();
        m_FixedCount = fixedCount;
        m_StatusMessage = "AssetRef.FixedIssues";
    }
}