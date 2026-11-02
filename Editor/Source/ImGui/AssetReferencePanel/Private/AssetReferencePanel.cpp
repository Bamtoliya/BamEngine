#include "AssetReferencePanel.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include <imgui.h>

namespace Editor
{
    AssetReferencePanel::AssetReferencePanel()
    {
        m_Name = L"AssetReferenceChecker";
        m_DisplayNameKey = "UI_ASSET_REFERENCE_PANEL";
    }

    void AssetReferencePanel::Draw()
    {
        if (!m_Open) return;

        ImGui::SetNextWindowSize(ImVec2(800.0f, 500.0f), ImGuiCond_FirstUseEver);
        const std::string title = LOCAL_CSTR(m_DisplayNameKey.c_str()) + std::string("###AssetRefPanel");

        if (ImGui::Begin(title.c_str(), &m_Open))
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
        const char* filterItems[] = { "All", "Missing", "External", "Invalid Ext" };
        ImGui::Combo("##FilterType", &m_FilterType, filterItems, IM_ARRAYSIZE(filterItems));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(200.0f);
        ImGui::InputTextWithHint("##SearchFilter", "Search path...", m_SearchFilter, sizeof(m_SearchFilter));

        if (!m_StatusMessage.empty())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "%s", m_StatusMessage.c_str());
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

        ImGui::Text("Total Issues: %zu | ", m_Issues.size());
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.3f, 0.3f, 1.0f), "Missing: %zu", missingCount);
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.2f, 1.0f), "External: %zu", extCount);
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), "Invalid Ext: %zu", invalidExtCount);
        ImGui::SameLine();
        ImGui::Text(" | ");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.4f, 1.0f), "Auto-Fixable: %zu", fixableCount);
    }

    void AssetReferencePanel::DrawIssuesTable()
    {
        static const ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollY;

        if (ImGui::BeginTable("##IssuesTable", 6, flags))
        {
            ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthFixed, 90.0f);
            ImGui::TableSetupColumn("Source Asset", ImGuiTableColumnFlags_WidthStretch, 0.25f);
            ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, 80.0f);
            ImGui::TableSetupColumn("Target Reference", ImGuiTableColumnFlags_WidthStretch, 0.35f);
            ImGui::TableSetupColumn("Suggested Fix", ImGuiTableColumnFlags_WidthStretch, 0.30f);
            ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthFixed, 65.0f);
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
                    ImGui::TextColored(ImVec4(0.95f, 0.3f, 0.3f, 1.0f), "%s Missing", ICON_FA_TRIANGLE_EXCLAMATION);
                else if (issue.IssueType == EAssetReferenceIssueType::ExternalAbsolutePath)
                    ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.2f, 1.0f), "%s External", ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE);
                else
                    ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.3f, 1.0f), "%s InvalidExt", ICON_FA_FILE_CIRCLE_QUESTION);

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
                    ImGui::TextDisabled("None");

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
        m_StatusMessage = "Scan completed.";
    }

    void AssetReferencePanel::FixSelectedIssue(size_t index)
    {
        if (index >= m_Issues.size()) return;
        if (AssetReferenceValidator::FixIssue(m_Issues[index]))
        {
            m_StatusMessage = "Fixed 1 issue.";
            RunValidation();
        }
    }

    void AssetReferencePanel::FixAllSafe()
    {
        const size_t fixedCount = AssetReferenceValidator::FixAllSafeIssues(m_Issues);
        m_StatusMessage = "Fixed " + std::to_string(fixedCount) + " issues.";
        RunValidation();
    }
}