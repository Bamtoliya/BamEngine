#pragma once
#include "ImGuiInterface.h"
#include "AssetReferenceValidator.h"
#include <vector>
#include <string>

BEGIN(Editor)

class AssetReferencePanel final : public IImGui
{
public:
    AssetReferencePanel();
    ~AssetReferencePanel() override = default;

    void Draw() override;
    void ShowReferences(const std::filesystem::path& asset);

private:
    void DrawToolbar();
    void DrawSummaryStats();
    void DrawIssuesTable();
    void DrawReferences();
    std::filesystem::path DrawReferenceList(const char* labelKey, const std::vector<std::filesystem::path>& paths);
    void SetReferenceTarget(const std::filesystem::path& asset);
    void UpdateReferenceLists();

    void RunValidation();
    void FixSelectedIssue(size_t index);
    void FixAllSafe();
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Assets; }
private:
    AssetReferenceScan m_Scan;
    std::filesystem::path m_ReferenceTarget;
    std::vector<std::filesystem::path> m_Dependencies;
    std::vector<std::filesystem::path> m_Users;
    bool m_SelectReferencesTab = false;
    bool m_ScanRequested = false;
    bool m_HasScanned = false;
    int m_FilterType = 0; // 0: All, 1: Missing, 2: External, 3: InvalidExt
    char m_SearchFilter[128] = "";
    std::string m_StatusMessage;
    size_t m_FixedCount = 0;
};

END