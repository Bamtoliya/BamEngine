#pragma once
#include "ImGuiInterface.h"
#include "AssetReferenceValidator.h"
#include <unordered_map>
#include <vector>
#include <string>

BEGIN(Editor)

class AssetPathRelocation;

class AssetReferencePanel final : public IImGui
{
public:
    AssetReferencePanel();
    ~AssetReferencePanel() override = default;

    void Draw() override;
    void ShowReferences(const std::filesystem::path& asset);
    void PrepareAssetPathRelocation(AssetPathRelocation& relocation);

private:
    using ReferencePaths = std::vector<std::filesystem::path>;
    struct ReferenceChildren
    {
        ReferencePaths Paths;
        bool AllLeaves = false;
    };
    using ReferenceAdjacency = std::unordered_map<std::filesystem::path, ReferenceChildren>;

    void DrawToolbar();
    void DrawSummaryStats();
    void DrawIssuesTable();
    void DrawReferences();
    void DrawReferenceTree(const char* labelKey, const ReferenceAdjacency& links, std::filesystem::path& next);
    void DrawReferenceChildren(const ReferenceChildren& children, const ReferenceAdjacency& links,
        ReferencePaths& ancestors, std::filesystem::path& next);
    void DrawReferenceBranch(const std::filesystem::path& path, const ReferenceAdjacency& links,
        ReferencePaths& ancestors, std::filesystem::path& next);
    void SetReferenceTarget(const std::filesystem::path& asset);
    void RebuildReferenceTrees(const std::vector<AssetReferenceLink>& references);

    void RunValidation();
    void FixSelectedIssue(size_t index);
    void FixAllSafe();
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Assets; }
private:
    std::vector<AssetReferenceIssue> m_Issues;
    std::filesystem::path m_ReferenceTarget;
    ReferenceAdjacency m_Dependencies;
    ReferenceAdjacency m_Users;
    bool m_SelectReferencesTab = false;
    bool m_ScanRequested = false;
    bool m_HasScanned = false;
    int m_FilterType = 0; // 0: All, 1: Missing, 2: External, 3: InvalidExt
    char m_SearchFilter[128] = "";
    std::string m_StatusMessage;
    size_t m_FixedCount = 0;
};

END