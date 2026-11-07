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

private:
    void DrawToolbar();
    void DrawSummaryStats();
    void DrawIssuesTable();

    void RunValidation();
    void FixSelectedIssue(size_t index);
    void FixAllSafe();
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Assets; }
private:
    std::vector<AssetReferenceIssue> m_Issues;
    bool m_HasScanned = false;
    int m_FilterType = 0; // 0: All, 1: Missing, 2: External, 3: InvalidExt
    char m_SearchFilter[128] = "";
    std::string m_StatusMessage;
    size_t m_FixedCount = 0;
};

END