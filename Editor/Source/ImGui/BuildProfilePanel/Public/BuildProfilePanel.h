#pragma once

#include "ImGuiInterface.h"
#include <filesystem>
#include "AssetReferenceValidator.h"
#include "ClientBuildJob.h"

BEGIN(Editor)

enum class EBuildProfileCategory
{
    General,
    Scenes,
    Assets,
    Packaging,
    BuildLog,
    Count
};

class BuildProfilePanel final : public IImGui
{
public:
    BuildProfilePanel();
    EResult OnInitialize(void*) override;
    void SelectStartupScene(const std::filesystem::path& path);
    void SaveSelection();
    void Draw() override;
    void Update(f32 dt) override;
    bool ShouldUpdateWhenClosed() const override { return m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running; }
    EPanelMenuCategory GetPanelMenuCategory() const override { return EPanelMenuCategory::Hidden; }

    void SetActiveCategory(EBuildProfileCategory category) { m_ActiveCategory = category; }

private:
    void DrawHeaderBar();
    void DrawSidebar();
    void DrawContentArea();

    void DrawGeneralCategory();
    void DrawScenesCategory();
    void DrawAssetsCategory();
    void DrawPackagingCategory();
    void DrawBuildLogCategory();

    void DrawScenesInBuild();
    void DrawSceneReferenceCheck();
    void DrawPackagingOptions();
    void DrawAdditionalAssets();
    void DrawBamPakSummary() const;
    void DrawReplacePackageDialog();
    void StartExport(ClientPackageRequest request);

    std::size_t GetTotalSceneCount() const;
    std::string GetSceneUriAt(std::size_t index) const;
    void AddScene(const std::filesystem::path& path);
    bool MoveSceneUp(std::size_t index);
    bool MoveSceneDown(std::size_t index);
    void RemoveSceneAt(std::size_t index);
    void AddAssetsFromFolder(const std::filesystem::path& folderPath);

    EBuildProfileCategory m_ActiveCategory = EBuildProfileCategory::General;
    float m_SidebarWidth = 200.0f;

    std::filesystem::path m_StartupScene;
    std::vector<std::string> m_AdditionalScenes;
    std::vector<std::string> m_AdditionalAssets;
    std::filesystem::path m_OutputDirectory;
    std::string m_PackageDirectoryName = ClientPackageRequest::DefaultDirectoryName;
    std::string m_ProgramName = ClientPackageRequest::DefaultProgramName;
    bool m_PackAssetsIntoBamPak = false;
    bool m_OnlyReferencedAssets = true;
    std::optional<ClientPackageManifest> m_LastPakSummary;
    std::filesystem::path m_LastPakSummaryDirectory;
    std::optional<ClientPackageRequest> m_PendingReplacement;
    std::vector<AssetReferenceIssue> m_SceneReferenceIssues;
    bool m_HasSceneReferenceResult = false;
    ClientBuildJob m_ClientBuild;
    bool m_ScrollBuildLogToEnd = false;
    std::string m_BuildLogText;
};

END
