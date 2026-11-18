#pragma once
#include "ImGuiInterface.h"
#include "AssetReferenceValidator.h"
#include <chrono>
#include <optional>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>

BEGIN(Editor)

class AssetPathRelocation;
enum class AssetReferenceTimingKind;

class AssetReferencePanel final : public IImGui
{
public:
    AssetReferencePanel();
    ~AssetReferencePanel() override = default;

    void Draw() override;
    void ShowReferences(const std::filesystem::path& asset);
    void RequestScanRefresh();
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
    void DrawReferenceToolbar();
    void DrawReferenceMatches(const char* labelKey, const ReferencePaths& matches, std::filesystem::path& next);
    void DrawReferenceTree(const char* labelKey, const ReferenceAdjacency& links, std::filesystem::path& next);
    void DrawReferenceChildren(const ReferenceChildren& children, const ReferenceAdjacency& links,
        ReferencePaths& ancestors, std::filesystem::path& next);
    void DrawReferenceBranch(const std::filesystem::path& path, const ReferenceAdjacency& links,
        ReferencePaths& ancestors, std::filesystem::path& next);
    void NavigateToReference(const std::filesystem::path& asset);
    void StepReferenceHistory(bool forward);
    void SetReferenceTarget(const std::filesystem::path& asset);
    void RebuildReferenceTrees(const std::vector<AssetReferenceLink>& references);
    void UpdateReferenceMatches();
    void CollectReferenceMatches(const ReferenceAdjacency& links, std::wstring_view query, ReferencePaths& matches) const;

    void RunValidation(AssetReferenceTimingKind kind);
    void FixSelectedIssue(size_t index);
    void FixAllSafe();
    void ReportRepairResult(size_t count);
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Assets; }
private:
    std::vector<AssetReferenceIssue> m_Issues;
    std::filesystem::path m_ReferenceTarget;
    ReferencePaths m_BackTargets;
    ReferencePaths m_ForwardTargets;
    ReferenceAdjacency m_Dependencies;
    ReferenceAdjacency m_Users;
    ReferencePaths m_MatchingDependencies;
    ReferencePaths m_MatchingUsers;
    char m_ReferenceSearch[256] = "";
    bool m_ReferenceSearchDirty = false;
    bool m_SelectReferencesTab = false;
    std::optional<std::chrono::steady_clock::time_point> m_ScanAfter;
    bool m_HasScanned = false;
    bool m_ReferenceTargetMissing = false;
    int m_FilterType = 0; // 0: All, 1: Missing, 2: External, 3: InvalidExt
    char m_SearchFilter[128] = "";
    std::string m_StatusMessage;
    size_t m_FixedCount = 0;
};

END