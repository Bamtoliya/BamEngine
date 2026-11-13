#pragma once

#include "ImGuiInterface.h"
#include <filesystem>
#include "AssetReferenceValidator.h"
#include "ClientBuildJob.h"

BEGIN(Editor)

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
private:
    void DrawSceneReferenceCheck();
    void DrawClientBuild();
    void DrawReplacePackageDialog();
    void StartExport(ClientPackageRequest request);

    std::filesystem::path m_StartupScene;
    std::filesystem::path m_OutputDirectory;
    std::string m_PackageDirectoryName = ClientPackageRequest::DefaultDirectoryName;
    std::string m_ProgramName = ClientPackageRequest::DefaultProgramName;
    std::optional<ClientPackageRequest> m_PendingReplacement;
    std::vector<AssetReferenceIssue> m_SceneReferenceIssues;
    bool m_HasSceneReferenceResult = false;
    ClientBuildJob m_ClientBuild;
    bool m_ScrollBuildLogToEnd = false;
    std::string m_BuildLogText;
};

END
