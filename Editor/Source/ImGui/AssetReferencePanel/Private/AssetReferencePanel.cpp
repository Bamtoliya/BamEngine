#include "AssetReferencePanel.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include <imgui.h>

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
            DrawToolbar();
            ImGui::Separator();
            DrawSummaryStats();
            ImGui::Separator();
            DrawIssuesTable();
        }
        ImGui::End();
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
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##SearchFilter", LOCAL_CSTR("AssetRef.SearchHint"), m_SearchFilter, sizeof(m_SearchFilter));

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
                    if (issue.SourceFile.string().find(sFilter) == std::string::npos &&
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
                else
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), LOCAL_CSTR("AssetRef.InvalidExtensionStatus"), ICON_FA_FILE_CIRCLE_QUESTION);

                // Column 1: Source Asset
                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(issue.SourceFile.string().c_str());

                // Column 2: Property
                ImGui::TableSetColumnIndex(2);
                ImGui::TextUnformatted(issue.PropertyName.c_str());

                // Column 3: Target Reference
                ImGui::TableSetColumnIndex(3);
                ImGui::TextUnformatted(issue.TargetReference.c_str());

                // Column 4: Suggested Fix
                ImGui::TableSetColumnIndex(4);
                if (!issue.SuggestedFix.empty())
                    ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", issue.SuggestedFix.string().c_str());
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
        m_Issues = AssetReferenceValidator::ValidateAllResources();
        m_HasScanned = true;
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