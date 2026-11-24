#include "BuildProfilePanel.h"
#include "EditorSettingsService.h"
#include "FileDialogs.h"
#include "Functions.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <exception>
#include <shellapi.h>
#include "Application.h"
#include "SceneDocument.h"
#include "PIEManager.h"
#include "NotificationManager.h"
#include "EditorUI.h"
#include "IconsFontAwesome7.h"
#include "ImGuiSplitter.h"

BEGIN(Editor)

BuildProfilePanel::BuildProfilePanel()
{
    m_Name = L"BuildProfile";
    m_DisplayNameKey = "BuildProfile.Title";
    m_Open = false;
}

EResult BuildProfilePanel::OnInitialize(void*)
{
    const auto& settings = EditorSettingsService::Get();
    const auto& profile = settings.GetWorkspace().BuildProfile;
    const auto& startup = profile.StartupScene.empty() ? settings.GetProjectSettings().StartupScenePath : profile.StartupScene;

    if (!startup.empty())
        m_StartupScene = Engine::PlatformPath::GetProjectAssetRoot() / std::filesystem::u8path(startup);
    m_AdditionalScenes = profile.AdditionalScenes;
    m_AdditionalAssets = profile.AdditionalAssets;

    if (m_StartupScene.empty() && !m_AdditionalScenes.empty())
    {
        m_StartupScene = Engine::PlatformPath::ResolveUri(m_AdditionalScenes.front());
        m_AdditionalScenes.erase(m_AdditionalScenes.begin());
    }

    m_OutputDirectory = std::filesystem::u8path(profile.OutputDirectory);
    if (!profile.PackageDirectoryName.empty())
        m_PackageDirectoryName = profile.PackageDirectoryName;
    if (!profile.ProgramName.empty())
        m_ProgramName = profile.ProgramName;
    return EResult::Success;
}

std::size_t BuildProfilePanel::GetTotalSceneCount() const
{
    return (m_StartupScene.empty() ? 0 : 1) + m_AdditionalScenes.size();
}

std::string BuildProfilePanel::GetSceneUriAt(std::size_t index) const
{
    if (m_StartupScene.empty())
        return index < m_AdditionalScenes.size() ? m_AdditionalScenes[index] : std::string();

    if (index == 0)
    {
        const auto& root = Engine::PlatformPath::GetProjectAssetRoot();
        std::error_code error;
        const auto relative = std::filesystem::relative(m_StartupScene, root, error);
        if (!error && !relative.empty())
            return "Project://" + Engine::WStrToStr(relative.generic_wstring());
        return Engine::WStrToStr(m_StartupScene.generic_wstring());
    }

    const std::size_t addIndex = index - 1;
    return addIndex < m_AdditionalScenes.size() ? m_AdditionalScenes[addIndex] : std::string();
}

void BuildProfilePanel::AddScene(const std::filesystem::path& path)
{
    const auto& root = Engine::PlatformPath::GetProjectAssetRoot();
    std::error_code error;
    const auto relative = std::filesystem::relative(path, root, error);

    if (error || relative.empty() || relative.is_absolute() || *relative.begin() == L".." ||
        !Engine::ToLowerStr(Engine::WStrToStr(path.filename().wstring())).ends_with(".bamscene.json") ||
        !std::filesystem::is_regular_file(path, error))
    {
        NotificationManager::Get().AddNotification(
            LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.InvalidAdditionalScene"), ELogLevel::Warn);
        return;
    }

    const auto uri = "Project://" + Engine::WStrToStr(relative.generic_wstring());

    if (!m_StartupScene.empty())
    {
        std::error_code ec;
        if (std::filesystem::equivalent(m_StartupScene, path, ec))
            return;
    }
    if (std::ranges::find(m_AdditionalScenes, uri) != m_AdditionalScenes.end())
        return;

    if (m_StartupScene.empty())
        m_StartupScene = root / relative;
    else
        m_AdditionalScenes.push_back(uri);

    m_SceneReferenceIssues.clear();
    m_HasSceneReferenceResult = false;
    SaveSelection();
}

void BuildProfilePanel::SelectStartupScene(const std::filesystem::path& path)
{
    const auto& root = Engine::PlatformPath::GetProjectAssetRoot();
    std::error_code error;
    const auto relative = std::filesystem::relative(path, root, error);

    if (error || relative.empty() || relative.is_absolute() || *relative.begin() == L".." ||
        !Engine::ToLowerStr(Engine::WStrToStr(path.filename().wstring())).ends_with(".bamscene.json") ||
        !std::filesystem::is_regular_file(path, error))
    {
        NotificationManager::Get().AddNotification(
            LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.InvalidStartupScene"), ELogLevel::Warn);
        return;
    }

    const auto uri = "Project://" + Engine::WStrToStr(relative.generic_wstring());
    const auto it = std::ranges::find(m_AdditionalScenes, uri);
    if (it != m_AdditionalScenes.end())
        m_AdditionalScenes.erase(it);

    if (!m_StartupScene.empty())
    {
        std::error_code ec;
        if (!std::filesystem::equivalent(m_StartupScene, path, ec))
        {
            const auto oldRel = std::filesystem::relative(m_StartupScene, root, ec);
            if (!ec && !oldRel.empty())
            {
                const auto oldUri = "Project://" + Engine::WStrToStr(oldRel.generic_wstring());
                if (std::ranges::find(m_AdditionalScenes, oldUri) == m_AdditionalScenes.end())
                    m_AdditionalScenes.insert(m_AdditionalScenes.begin(), oldUri);
            }
        }
    }

    m_StartupScene = root / relative;
    m_SceneReferenceIssues.clear();
    m_HasSceneReferenceResult = false;
    SaveSelection();
}

bool BuildProfilePanel::MoveSceneUp(std::size_t index)
{
    if (index == 0 || GetTotalSceneCount() <= 1)
        return false;

    if (index == 1)
    {
        const auto nextStartupUri = m_AdditionalScenes[0];
        const auto& root = Engine::PlatformPath::GetProjectAssetRoot();
        std::error_code error;
        const auto oldStartupRel = std::filesystem::relative(m_StartupScene, root, error);
        const auto oldStartupUri = (!error && !oldStartupRel.empty())
            ? ("Project://" + Engine::WStrToStr(oldStartupRel.generic_wstring()))
            : ("Project://" + Engine::WStrToStr(m_StartupScene.filename().generic_wstring()));

        m_StartupScene = Engine::PlatformPath::ResolveUri(nextStartupUri);
        m_AdditionalScenes[0] = oldStartupUri;
    }
    else
    {
        const std::size_t addIndex = m_StartupScene.empty() ? index : (index - 1);
        std::swap(m_AdditionalScenes[addIndex], m_AdditionalScenes[addIndex - 1]);
    }

    m_SceneReferenceIssues.clear();
    m_HasSceneReferenceResult = false;
    SaveSelection();
    return true;
}

bool BuildProfilePanel::MoveSceneDown(std::size_t index)
{
    const auto total = GetTotalSceneCount();
    if (index + 1 >= total)
        return false;

    if (index == 0)
    {
        const auto nextStartupUri = m_AdditionalScenes[0];
        const auto& root = Engine::PlatformPath::GetProjectAssetRoot();
        std::error_code error;
        const auto oldStartupRel = std::filesystem::relative(m_StartupScene, root, error);
        const auto oldStartupUri = (!error && !oldStartupRel.empty())
            ? ("Project://" + Engine::WStrToStr(oldStartupRel.generic_wstring()))
            : ("Project://" + Engine::WStrToStr(m_StartupScene.filename().generic_wstring()));

        m_StartupScene = Engine::PlatformPath::ResolveUri(nextStartupUri);
        m_AdditionalScenes[0] = oldStartupUri;
    }
    else
    {
        const std::size_t addIndex = m_StartupScene.empty() ? index : (index - 1);
        std::swap(m_AdditionalScenes[addIndex], m_AdditionalScenes[addIndex + 1]);
    }

    m_SceneReferenceIssues.clear();
    m_HasSceneReferenceResult = false;
    SaveSelection();
    return true;
}

void BuildProfilePanel::RemoveSceneAt(std::size_t index)
{
    const auto total = GetTotalSceneCount();
    if (index >= total)
        return;

    if (index == 0 && !m_StartupScene.empty())
    {
        if (!m_AdditionalScenes.empty())
        {
            m_StartupScene = Engine::PlatformPath::ResolveUri(m_AdditionalScenes.front());
            m_AdditionalScenes.erase(m_AdditionalScenes.begin());
        }
        else
            m_StartupScene.clear();
    }
    else
    {
        const std::size_t addIndex = m_StartupScene.empty() ? index : (index - 1);
        if (addIndex < m_AdditionalScenes.size())
            m_AdditionalScenes.erase(m_AdditionalScenes.begin() + addIndex);
    }

    m_SceneReferenceIssues.clear();
    m_HasSceneReferenceResult = false;
    SaveSelection();
}

void BuildProfilePanel::SaveSelection()
{
    const auto& root = Engine::PlatformPath::GetProjectAssetRoot();
    std::string scene;
    if (!m_StartupScene.empty())
    {
        std::error_code error;
        const auto rel = std::filesystem::relative(m_StartupScene, root, error);
        if (!error && !rel.empty())
        {
            const auto u8 = rel.generic_u8string();
            scene.assign(u8.begin(), u8.end());
        }
    }

    const auto output = m_OutputDirectory.generic_u8string();

    const BuildProfileWorkspace profile{
        scene, std::string(output.begin(), output.end()),
        m_PackageDirectoryName, m_ProgramName, m_AdditionalScenes, m_AdditionalAssets
    };
    if (EditorSettingsService::Get().SetBuildProfileWorkspace(profile) != EEditorSettingsSaveStatus::Saved)
    {
        NotificationManager::Get().AddNotification(
            LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.SelectionSaveFailed"), ELogLevel::Warn);
    }
}

void BuildProfilePanel::AddAssetsFromFolder(const std::filesystem::path& folderPath)
{
    using Path = Engine::PlatformPath;
    const auto& assetRoot = Path::GetProjectAssetRoot();
    std::error_code error;
    const auto relativeFolder = std::filesystem::relative(folderPath, assetRoot, error);
    if (error || relativeFolder.empty() || relativeFolder.is_absolute() || *relativeFolder.begin() == L".." ||
        !std::filesystem::is_directory(folderPath, error))
    {
        NotificationManager::Get().AddNotification(
            LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.InvalidAssetFolder"), ELogLevel::Warn);
        return;
    }

    size_t addedCount = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(folderPath, error))
    {
        if (error) break;
        if (!entry.is_regular_file(error)) continue;

        const auto relFile = std::filesystem::relative(entry.path(), assetRoot, error);
        if (error || relFile.empty() || relFile.is_absolute() || *relFile.begin() == L"..")
            continue;

        const auto uri = "Project://" + Engine::WStrToStr(relFile.generic_wstring());
        if (std::ranges::find(m_AdditionalAssets, uri) == m_AdditionalAssets.end())
        {
            m_AdditionalAssets.push_back(uri);
            ++addedCount;
        }
    }

    if (addedCount > 0)
    {
        SaveSelection();
        NotificationManager::Get().AddNotification(
            LOCAL("BuildProfile.Title"), LOCAL_FMT("BuildProfile.AdditionalAssetsFolderSuccess", addedCount), ELogLevel::Info);
    }
    else
    {
        NotificationManager::Get().AddNotification(
            LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.NoAssetsFoundInFolder"), ELogLevel::Warn);
    }
}

void BuildProfilePanel::Draw()
{
    if (!m_Open)
        return;

    ImGui::SetNextWindowSize(ImVec2(860.0f, 600.0f), ImGuiCond_FirstUseEver);

    if (BeginPanelWindow(LOCAL_LABEL("BuildProfile.Title").c_str(), m_Open))
    {
        m_Focused = ImGui::IsWindowFocused();
        m_Hovered = ImGui::IsWindowHovered();

        DrawHeaderBar();

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const float contentHeight = std::max(1.0f, ImGui::GetContentRegionAvail().y);
        Editor::DrawSidebarLayout("BuildProfileSidebarRegion", "##BuildProfileSidebarSplitter", "BuildProfileContentRegion",
            m_SidebarWidth, contentHeight, 160.0f, 250.0f,
            [this] { DrawSidebar(); },
            [this] { DrawContentArea(); });

        DrawReplacePackageDialog();
    }
    ImGui::End();
}

void BuildProfilePanel::DrawHeaderBar()
{
    using Status = ClientBuildJob::Status;
    const auto& projectRoot = EditorSettingsService::Get().GetProjectRoot();
    const Status buildStatus = m_ClientBuild.GetStatus();
    const bool running = (buildStatus == Status::Running);
    const bool exporting = m_ClientBuild.IsExport();
    const auto totalScenes = GetTotalSceneCount();

    const bool canExport = !running && !m_StartupScene.empty() && !m_OutputDirectory.empty() &&
        !m_PackageDirectoryName.empty() && !m_ProgramName.empty();

    // 1. 핵심 액션 버튼 줄
    EditorUI::TextButtonOptions buildClientOpt;
    buildClientOpt.Enabled = !running;
    if (EditorUI::TextButton("BuildClient", "BuildProfile.BuildClient", buildClientOpt))
    {
        m_ClientBuild.Start(projectRoot);
        m_ScrollBuildLogToEnd = true;
        m_BuildLogText.clear();
        m_ActiveCategory = EBuildProfileCategory::BuildLog;
    }

    EditorUI::TextButtonOptions exportOpt;
    exportOpt.SameLine = true;
    exportOpt.Enabled = canExport;
    exportOpt.Style = EditorUI::EButtonStyle::Primary;
    if (!canExport && !running)
        exportOpt.TooltipKey = "BuildProfile.BuildAndExportTooltipMissing";
    if (EditorUI::TextButton("BuildAndExport", "BuildProfile.BuildAndExport", exportOpt))
    {
        try
        {
            ClientPackageRequest request{ m_StartupScene, m_OutputDirectory, m_PackageDirectoryName, m_ProgramName };
            request.AdditionalScenes = m_AdditionalScenes;
            request.AdditionalAssets = m_AdditionalAssets;
            request.PackAssetsIntoBamPak = m_PackAssetsIntoBamPak;
            request.OnlyReferencedAssets = m_OnlyReferencedAssets;
            request.Destination = InspectClientPackageDestination(projectRoot, request);
            if (request.Destination.ExistingPackage)
            {
                m_PendingReplacement = std::move(request);
                ImGui::OpenPopup(LOCAL_LABEL("BuildProfile.ReplacePackageTitle").c_str());
            }
            else
            {
                StartExport(std::move(request));
                m_ActiveCategory = EBuildProfileCategory::BuildLog;
            }
        }
        catch (const std::exception& error)
        {
            m_ClientBuild.SetExportPreparationError(LOCAL("BuildProfile.InvalidPackageDestination") + '\n' + error.what());
            m_BuildLogText.clear();
            m_ScrollBuildLogToEnd = true;
            m_ActiveCategory = EBuildProfileCategory::BuildLog;
        }
    }

    EditorUI::TextButtonOptions checkRefOpt;
    checkRefOpt.SameLine = true;
    checkRefOpt.Enabled = (totalScenes > 0 && !running);
    if (totalScenes == 0)
        checkRefOpt.TooltipKey = "BuildProfile.NoScenesInBuild";
    if (EditorUI::TextButton("HeaderCheckSceneReferences", "BuildProfile.CheckSceneReferences", checkRefOpt))
    {
        m_SceneReferenceIssues.clear();
        if (!m_StartupScene.empty())
        {
            const auto issues = AssetReferenceValidator::ValidateBuildScene(m_StartupScene);
            m_SceneReferenceIssues.insert(m_SceneReferenceIssues.end(), issues.begin(), issues.end());
        }
        for (const auto& uri : m_AdditionalScenes)
        {
            const auto issues = AssetReferenceValidator::ValidateBuildScene(Engine::PlatformPath::ResolveUri(uri));
            m_SceneReferenceIssues.insert(m_SceneReferenceIssues.end(), issues.begin(), issues.end());
        }
        m_HasSceneReferenceResult = true;
        m_ActiveCategory = EBuildProfileCategory::Scenes;
    }

    const auto& exportedDirectory = m_ClientBuild.GetExportedDirectory();
    if (!exportedDirectory.empty())
    {
        EditorUI::TextButtonOptions openFolderOpt;
        openFolderOpt.SameLine = true;
        if (EditorUI::TextButton("HeaderOpenExportFolder", "BuildProfile.OpenExportFolder", openFolderOpt))
        {
            const auto result = ShellExecuteW(nullptr, L"open", exportedDirectory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) <= 32)
                EDITOR_LOG_WARN("Could not open exported folder: {}", Engine::WStrToStr(exportedDirectory.wstring()));
        }
    }

    // 상태 배지 표시
    ImGui::SameLine();
    switch (buildStatus)
    {
    case Status::Idle:
        if (canExport)
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[%s]", LOCAL_CSTR("BuildProfile.StatusIdle"));
        else
            ImGui::TextDisabled("[%s]", LOCAL_CSTR("BuildProfile.MissingBuildSettings"));
        break;
    case Status::Running:
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "[%s]",
            LOCAL_CSTR(exporting ? "BuildProfile.ExportRunning" : "BuildProfile.BuildRunning"));
        break;
    case Status::Succeeded:
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[%s]",
            LOCAL_CSTR(exporting ? "BuildProfile.ExportSucceeded" : "BuildProfile.BuildSucceeded"));
        break;
    case Status::Failed:
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "[%s]",
            LOCAL_CSTR(exporting ? "BuildProfile.ExportFailed" : "BuildProfile.BuildFailed"));
        break;
    }

    // 2. 핵심 요약 바 (Target, Startup Scene, Output Destination, Executable)
    ImGui::Spacing();
    if (ImGui::BeginTable("##HeaderSummaryTable", 4, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerV))
    {
        ImGui::TableSetupColumn(LOCAL_CSTR("BuildProfile.Target"), ImGuiTableColumnFlags_WidthStretch, 0.22f);
        ImGui::TableSetupColumn(LOCAL_CSTR("BuildProfile.StartupScene"), ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableSetupColumn(LOCAL_CSTR("BuildProfile.OutputDirectory"), ImGuiTableColumnFlags_WidthStretch, 0.28f);
        ImGui::TableSetupColumn(LOCAL_CSTR("BuildProfile.PackageExecutablePath"), ImGuiTableColumnFlags_WidthStretch, 0.25f);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();

        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.WindowsRelease"));

        ImGui::TableSetColumnIndex(1);
        if (m_StartupScene.empty())
            ImGui::TextDisabled("%s", LOCAL_CSTR("EditorCommon.None"));
        else
        {
            const auto sceneName = Engine::WStrToStr(m_StartupScene.filename().wstring());
            ImGui::TextUnformatted(sceneName.c_str());
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s", Engine::WStrToStr(m_StartupScene.wstring()).c_str());
        }

        ImGui::TableSetColumnIndex(2);
        if (m_OutputDirectory.empty())
            ImGui::TextDisabled("%s", LOCAL_CSTR("EditorCommon.None"));
        else
        {
            const auto dirName = Engine::WStrToStr(m_OutputDirectory.filename().wstring());
            ImGui::TextUnformatted(dirName.empty() ? Engine::WStrToStr(m_OutputDirectory.wstring()).c_str() : dirName.c_str());
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                ImGui::SetTooltip("%s", Engine::WStrToStr(m_OutputDirectory.wstring()).c_str());
        }

        ImGui::TableSetColumnIndex(3);
        if (m_ProgramName.empty())
            ImGui::TextDisabled("%s", LOCAL_CSTR("EditorCommon.None"));
        else
        {
            const std::string exeName = m_ProgramName + ".exe";
            ImGui::TextUnformatted(exeName.c_str());
            if (!m_OutputDirectory.empty() && !m_PackageDirectoryName.empty())
            {
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                {
                    const auto fullExe = Engine::WStrToStr(m_OutputDirectory.generic_wstring()) + "/" +
                        m_PackageDirectoryName + "/" + m_ProgramName + ".exe";
                    ImGui::SetTooltip("%s", fullExe.c_str());
                }
            }
        }

        ImGui::EndTable();
    }
}

void BuildProfilePanel::DrawSidebar()
{
    auto drawCategoryItem = [this](EBuildProfileCategory cat, const char* icon, const char* labelKey, const std::string& badge = "")
    {
        const bool isSelected = (m_ActiveCategory == cat);
        std::string label = std::string(icon) + "  " + LOCAL(labelKey);
        if (!badge.empty())
            label += " " + badge;

        ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen | ImGuiTreeNodeFlags_SpanAvailWidth;
        if (isSelected) flags |= ImGuiTreeNodeFlags_Selected;

        ImGui::TreeNodeEx((void*)(intptr_t)cat, flags, "%s", label.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
            m_ActiveCategory = cat;
    };

    // 1. 빌드 설정 그룹 (General, Scenes, Assets, Packaging)
    if (ImGui::TreeNodeEx("SidebarGroupConfiguration", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", LOCAL_CSTR("BuildProfile.GroupConfiguration")))
    {
        drawCategoryItem(EBuildProfileCategory::General, ICON_FA_GEAR, "BuildProfile.CategoryGeneral");

        const size_t sceneCount = GetTotalSceneCount();
        std::string sceneBadge;
        if (sceneCount > 0)
            sceneBadge = "(" + std::to_string(sceneCount) + ")";
        if (m_HasSceneReferenceResult && !m_SceneReferenceIssues.empty())
            sceneBadge += " [!]";
        drawCategoryItem(EBuildProfileCategory::Scenes, ICON_FA_FILM, "BuildProfile.CategoryScenes", sceneBadge);

        const size_t assetCount = m_AdditionalAssets.size();
        std::string assetBadge;
        if (assetCount > 0)
            assetBadge = "(" + std::to_string(assetCount) + ")";
        drawCategoryItem(EBuildProfileCategory::Assets, ICON_FA_FOLDER, "BuildProfile.CategoryAssets", assetBadge);

        std::string pakBadge;
        if (m_PackAssetsIntoBamPak)
            pakBadge = "(BamPak)";
        drawCategoryItem(EBuildProfileCategory::Packaging, ICON_FA_BOX, "BuildProfile.CategoryPackaging", pakBadge);

        ImGui::TreePop();
    }

    // 2. 출력 및 진단 그룹 (Build Log)
    if (ImGui::TreeNodeEx("SidebarGroupOutput", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", LOCAL_CSTR("BuildProfile.GroupOutput")))
    {
        std::string logBadge;
        if (m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running)
            logBadge = "...";
        else if (m_ClientBuild.GetStatus() == ClientBuildJob::Status::Failed)
            logBadge = "[X]";
        else if (m_ClientBuild.GetStatus() == ClientBuildJob::Status::Succeeded)
            logBadge = "[OK]";
        drawCategoryItem(EBuildProfileCategory::BuildLog, ICON_FA_TERMINAL, "BuildProfile.CategoryBuildLog", logBadge);

        ImGui::TreePop();
    }
}

void BuildProfilePanel::DrawContentArea()
{
    switch (m_ActiveCategory)
    {
    case EBuildProfileCategory::General:
        DrawGeneralCategory();
        break;
    case EBuildProfileCategory::Scenes:
        DrawScenesCategory();
        break;
    case EBuildProfileCategory::Assets:
        DrawAssetsCategory();
        break;
    case EBuildProfileCategory::Packaging:
        DrawPackagingCategory();
        break;
    case EBuildProfileCategory::BuildLog:
        DrawBuildLogCategory();
        break;
    default:
        break;
    }
}

void BuildProfilePanel::DrawGeneralCategory()
{
    const bool running = m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running;

    ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.GeneralSection"));
    ImGui::Separator();
    ImGui::Spacing();

    DrawPackagingOptions();
}

void BuildProfilePanel::DrawScenesCategory()
{
    DrawScenesInBuild();
    ImGui::Spacing();
    DrawSceneReferenceCheck();
}

void BuildProfilePanel::DrawAssetsCategory()
{
    DrawAdditionalAssets();
}

void BuildProfilePanel::DrawPackagingCategory()
{
    const bool running = m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running;

    ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.PackagingSettings"));
    ImGui::Separator();
    ImGui::Spacing();

    if (EditorUI::BeginPropertyTable("##PackagingOptionsTable"))
    {
        EditorUI::PropertyRow("BuildProfile.Format");
        if (m_PackAssetsIntoBamPak)
            ImGui::Text("%s + .bampak", LOCAL_CSTR("BuildProfile.Folder"));
        else
            ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.Folder"));

        EditorUI::PropertyLabel(LOCAL_CSTR("BuildProfile.BamPakPackaging"), LOCAL_CSTR("BuildProfile.BamPakPackagingTooltip"));
        EditorUI::PropertyValue();
        ImGui::BeginDisabled(running);
        if (ImGui::Checkbox(LOCAL_LABEL("BuildProfile.BamPakPackagingCheckbox").c_str(), &m_PackAssetsIntoBamPak))
            SaveSelection();
        ImGui::EndDisabled();

        EditorUI::PropertyLabel(LOCAL_CSTR("BuildProfile.OnlyReferencedAssets"), LOCAL_CSTR("BuildProfile.OnlyReferencedAssetsTooltip"));
        EditorUI::PropertyValue();
        ImGui::BeginDisabled(running);
        if (ImGui::Checkbox(LOCAL_LABEL("BuildProfile.OnlyReferencedAssetsCheckbox").c_str(), &m_OnlyReferencedAssets))
            SaveSelection();
        ImGui::EndDisabled();

        EditorUI::EndPropertyTable();
    }

    DrawBamPakSummary();
}

void BuildProfilePanel::DrawBuildLogCategory()
{
    using Status = ClientBuildJob::Status;
    const Status status = m_ClientBuild.GetStatus();
    const bool exporting = m_ClientBuild.IsExport();

    switch (status)
    {
    case Status::Idle:
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", LOCAL_CSTR("BuildProfile.StatusIdle"));
        break;
    case Status::Running:
        ImGui::TextColored(ImVec4(0.35f, 0.75f, 1.0f, 1.0f), "%s",
            LOCAL_CSTR(exporting ? "BuildProfile.ExportRunning" : "BuildProfile.BuildRunning"));
        break;
    case Status::Succeeded:
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "%s",
            LOCAL_CSTR(exporting ? "BuildProfile.ExportSucceeded" : "BuildProfile.BuildSucceeded"));
        break;
    case Status::Failed:
        ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s",
            LOCAL_CSTR(exporting ? "BuildProfile.ExportFailed" : "BuildProfile.BuildFailed"));
        break;
    }

    const auto& exportedDirectory = m_ClientBuild.GetExportedDirectory();
    if (!exportedDirectory.empty())
    {
        EditorUI::TextButtonOptions openFolderOpt;
        openFolderOpt.SameLine = true;
        if (EditorUI::TextButton("LogOpenExportFolder", "BuildProfile.OpenExportFolder", openFolderOpt))
        {
            const auto result = ShellExecuteW(nullptr, L"open", exportedDirectory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
            if (reinterpret_cast<INT_PTR>(result) <= 32)
                EDITOR_LOG_WARN("Could not open exported folder: {}", Engine::WStrToStr(exportedDirectory.wstring()));
        }
    }

    EditorUI::TextButtonOptions logActionOpt;
    logActionOpt.SameLine = true;
    logActionOpt.Enabled = !m_BuildLogText.empty();
    if (EditorUI::TextButton("CopyBuildLog", "EditorCommon.Copy", logActionOpt))
        ImGui::SetClipboardText(m_BuildLogText.c_str());

    if (EditorUI::TextButton("ClearBuildLog", "BuildProfile.ClearBuildLog", logActionOpt))
        m_BuildLogText.clear();

    ImGui::Spacing();

    const ImVec2 logSize(std::max(1.0f, ImGui::GetContentRegionAvail().x), std::max(100.0f, ImGui::GetContentRegionAvail().y));
    if (m_BuildLogText.empty())
    {
        if (ImGui::BeginChild("##EmptyClientBuildOutput", logSize, ImGuiChildFlags_Borders))
            EditorUI::EmptyState("BuildProfile.NoBuildOutput");
        ImGui::EndChild();
        return;
    }

    if (ImGui::BeginChild("##ClientBuildOutputScroll", logSize, ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar))
    {
        ImGui::TextUnformatted(m_BuildLogText.c_str());
        if (m_ScrollBuildLogToEnd)
        {
            ImGui::SetScrollHereY(1.0f);
            m_ScrollBuildLogToEnd = false;
        }
    }
    ImGui::EndChild();
}

void BuildProfilePanel::DrawScenesInBuild()
{
    const bool running = m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running;
    const auto totalScenes = GetTotalSceneCount();

    ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.ScenesInBuild"));

    EditorUI::TextButtonOptions addCurrentOpt;
    addCurrentOpt.SameLine = true;
    addCurrentOpt.Enabled = !running;
    addCurrentOpt.TooltipKey = "BuildProfile.AddCurrentSceneTooltip";
    if (EditorUI::TextButton("AddCurrentScene", "BuildProfile.AddCurrentScene", addCurrentOpt))
    {
        const auto* document = Application::Get().GetSceneDocument();
        if (PIEManager::Get().IsEditing() && document && !document->GetPath().empty() && !document->HasUnsavedChanges())
            AddScene(document->GetPath());
        else
            NotificationManager::Get().AddNotification(
                LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.SaveCurrentSceneFirst"), ELogLevel::Warn);
    }

    EditorUI::TextButtonOptions addSceneOpt;
    addSceneOpt.SameLine = true;
    addSceneOpt.Enabled = !running;
    if (EditorUI::TextButton("AddSceneFile", "BuildProfile.AddSceneFile", addSceneOpt))
    {
        std::wstring selectedPath;
        const auto scenesDirectory = Engine::PlatformPath::GetProjectAssetRoot() / "Scenes";
        const vector<FileDialogFilter> filters = {
            { Engine::StrToWStr(LOCAL("BuildProfile.SceneFiles")), L"*.bamscene.json" }
        };

        if (FileDialogs::OpenFileDialog(selectedPath, filters, scenesDirectory.wstring(), "BuildProfile.AddScene"))
            AddScene(selectedPath);
    }

    const float childHeight = totalScenes == 0 ? 80.0f : std::min(220.0f, std::max(80.0f, totalScenes * 28.0f + 32.0f));
    if (ImGui::BeginChild("##ScenesInBuildChild", ImVec2(0.0f, childHeight), ImGuiChildFlags_Borders))
    {
        if (totalScenes == 0)
        {
            EditorUI::EmptyState("BuildProfile.NoScenesInBuild");
        }
        else
        {
            const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##ScenesTable", 4, tableFlags))
            {
                ImGui::TableSetupColumn(LOCAL_CSTR("BuildProfile.SceneIndex"), ImGuiTableColumnFlags_WidthFixed, 45.0f);
                ImGui::TableSetupColumn(LOCAL_CSTR("BuildProfile.StartupScene"), ImGuiTableColumnFlags_WidthFixed, 65.0f);
                ImGui::TableSetupColumn(LOCAL_CSTR("BuildProfile.SceneFiles"), ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, 85.0f);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < totalScenes; ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::Text("%zu", i);

                    ImGui::TableSetColumnIndex(1);
                    if (i == 0)
                    {
                        ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[%s]", LOCAL_CSTR("BuildProfile.StartupSceneBadge"));
                        if (ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
                            ImGui::SetTooltip("%s", LOCAL_CSTR("BuildProfile.StartupSceneTooltip"));
                    }

                    ImGui::TableSetColumnIndex(2);
                    const auto uri = GetSceneUriAt(i);
                    ImGui::TextUnformatted(uri.c_str());

                    ImGui::TableSetColumnIndex(3);
                    ImGui::BeginDisabled(running);

                    const bool canMoveUp = (i > 0);
                    if (EditorUI::SmallIconButton("MoveUp", ICON_FA_ARROW_UP, "EditorCommon.MoveUp", canMoveUp))
                    {
                        MoveSceneUp(i);
                        ImGui::EndDisabled();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::SameLine();
                    const bool canMoveDown = (i + 1 < totalScenes);
                    if (EditorUI::SmallIconButton("MoveDown", ICON_FA_ARROW_DOWN, "EditorCommon.MoveDown", canMoveDown))
                    {
                        MoveSceneDown(i);
                        ImGui::EndDisabled();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::SameLine();
                    if (EditorUI::SmallIconButton("RemoveScene", ICON_FA_TRASH, "BuildProfile.RemoveEntry"))
                    {
                        RemoveSceneAt(i);
                        ImGui::EndDisabled();
                        ImGui::PopID();
                        break;
                    }

                    ImGui::EndDisabled();
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();
}

void BuildProfilePanel::DrawPackagingOptions()
{
    const bool running = m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running;

    if (EditorUI::BeginPropertyTable("##PackagingOptions"))
    {
        EditorUI::PropertyRow("BuildProfile.Target");
        ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.WindowsRelease"));

        EditorUI::PropertyRow("BuildProfile.RenderPipelineProfile");
        const auto& profile = EditorSettingsService::Get().GetProjectSettings().RenderPipelineProfilePath;
        ImGui::TextWrapped("%s", profile.empty() ? LOCAL_CSTR("EditorCommon.None") : profile.c_str());

        EditorUI::PropertyRow("BuildProfile.OutputDirectory");
        EditorUI::TextButtonOptions browseOpt;
        browseOpt.Enabled = !running;
        if (EditorUI::TextButton("BrowseOutputDirectory", "Settings.BrowseFolder", browseOpt))
        {
            std::wstring selectedPath;
            if (FileDialogs::OpenFolderDialog(selectedPath, m_OutputDirectory.wstring(), "BuildProfile.OutputFolder"))
            {
                m_OutputDirectory = selectedPath;
                SaveSelection();
            }
        }
        if (m_OutputDirectory.empty())
            ImGui::TextUnformatted(LOCAL_CSTR("EditorCommon.None"));
        else
            ImGui::TextWrapped("%s", Engine::WStrToStr(m_OutputDirectory.wstring()).c_str());

        EditorUI::PropertyRow("BuildProfile.ProgramName");
        ImGui::BeginDisabled(running);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##ProgramName", &m_ProgramName);
        if (ImGui::IsItemDeactivatedAfterEdit())
            SaveSelection();
        ImGui::EndDisabled();

        EditorUI::PropertyRow("BuildProfile.PackageDirectoryName");
        ImGui::BeginDisabled(running);
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##PackageDirectoryName", &m_PackageDirectoryName);
        if (ImGui::IsItemDeactivatedAfterEdit())
            SaveSelection();
        ImGui::EndDisabled();

        EditorUI::PropertyRow("BuildProfile.PackageExecutablePath");
        if (m_OutputDirectory.empty() || m_PackageDirectoryName.empty() || m_ProgramName.empty())
            ImGui::TextUnformatted(LOCAL_CSTR("EditorCommon.None"));
        else
        {
            const auto output = Engine::WStrToStr(m_OutputDirectory.generic_wstring()) + "/" +
                m_PackageDirectoryName + "/" + m_ProgramName + ".exe";
            ImGui::TextWrapped("%s", output.c_str());
        }

        EditorUI::EndPropertyTable();
    }
}

void BuildProfilePanel::DrawAdditionalAssets()
{
    const bool running = m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running;
    const auto totalAssets = m_AdditionalAssets.size();

    ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.AdditionalAssets"));

    EditorUI::TextButtonOptions assetBtnOpt;
    assetBtnOpt.SameLine = true;
    assetBtnOpt.Enabled = !running;

    if (EditorUI::TextButton("AddAssetFile", "BuildProfile.AddAssetFile", assetBtnOpt))
    {
        using Path = Engine::PlatformPath;
        const auto& assetRoot = Path::GetProjectAssetRoot();
        const vector<FileDialogFilter> filters = {
            { Engine::StrToWStr(LOCAL("BuildProfile.AllFiles")), L"*.*" }
        };
        std::wstring selected;
        if (FileDialogs::OpenFileDialog(selected, filters, assetRoot.wstring(), "BuildProfile.AddAsset"))
        {
            std::error_code error;
            const auto relative = std::filesystem::relative(selected, assetRoot, error);
            const bool valid = !error && !relative.empty() && !relative.is_absolute() && *relative.begin() != L".." &&
                std::filesystem::is_regular_file(selected, error);
            if (valid)
            {
                const auto uri = "Project://" + Engine::WStrToStr(relative.generic_wstring());
                if (std::ranges::find(m_AdditionalAssets, uri) == m_AdditionalAssets.end())
                {
                    m_AdditionalAssets.push_back(uri);
                    SaveSelection();
                }
            }
            else
            {
                NotificationManager::Get().AddNotification(
                    LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.InvalidAdditionalAsset"), ELogLevel::Warn);
            }
        }
    }

    if (EditorUI::TextButton("AddAssetFolder", "BuildProfile.AddAssetFolder", assetBtnOpt))
    {
        using Path = Engine::PlatformPath;
        const auto& assetRoot = Path::GetProjectAssetRoot();
        std::wstring selectedFolder;
        if (FileDialogs::OpenFolderDialog(selectedFolder, assetRoot.wstring(), "BuildProfile.AddFolder"))
            AddAssetsFromFolder(selectedFolder);
    }

    if (totalAssets > 0)
    {
        EditorUI::TextButtonOptions clearAssetsOpt = assetBtnOpt;
        clearAssetsOpt.Style = EditorUI::EButtonStyle::Danger;
        if (EditorUI::TextButton("ClearAllAssets", "BuildProfile.ClearAllAssets", clearAssetsOpt))
        {
            m_AdditionalAssets.clear();
            SaveSelection();
        }
    }

    if (totalAssets > 0)
    {
        ImGui::SameLine();
        ImGui::TextDisabled("(%s)", LOCAL_FMT("BuildProfile.AssetCount", totalAssets).c_str());
    }

    const float childHeight = totalAssets == 0 ? 80.0f : std::min(220.0f, std::max(80.0f, totalAssets * 28.0f + 32.0f));
    if (ImGui::BeginChild("##AdditionalAssetsChild", ImVec2(0.0f, childHeight), ImGuiChildFlags_Borders))
    {
        if (totalAssets == 0)
        {
            EditorUI::EmptyState("BuildProfile.NoAdditionalAssets");
        }
        else
        {
            const ImGuiTableFlags tableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
                ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_ScrollY;
            if (ImGui::BeginTable("##AssetsTable", 2, tableFlags))
            {
                ImGui::TableSetupColumn(LOCAL_CSTR("BuildProfile.AdditionalAssets"), ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed, 36.0f);
                ImGui::TableHeadersRow();

                for (size_t i = 0; i < m_AdditionalAssets.size(); ++i)
                {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);
                    ImGui::TextUnformatted(m_AdditionalAssets[i].c_str());

                    ImGui::TableSetColumnIndex(1);
                    ImGui::BeginDisabled(running);
                    if (EditorUI::SmallIconButton("RemoveAsset", ICON_FA_TRASH, "BuildProfile.RemoveEntry"))
                    {
                        m_AdditionalAssets.erase(m_AdditionalAssets.begin() + i);
                        SaveSelection();
                        ImGui::EndDisabled();
                        ImGui::PopID();
                        break;
                    }
                    ImGui::EndDisabled();

                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndChild();
}

void BuildProfilePanel::DrawSceneReferenceCheck()
{
    const bool running = m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running;
    const auto totalScenes = GetTotalSceneCount();

    EditorUI::TextButtonOptions checkRefOpt;
    checkRefOpt.Enabled = (totalScenes > 0 && !running);
    if (totalScenes == 0)
        checkRefOpt.TooltipKey = "BuildProfile.NoScenesInBuild";
    if (EditorUI::TextButton("CheckSceneReferences", "BuildProfile.CheckSceneReferences", checkRefOpt))
    {
        m_SceneReferenceIssues.clear();
        if (!m_StartupScene.empty())
        {
            const auto issues = AssetReferenceValidator::ValidateBuildScene(m_StartupScene);
            m_SceneReferenceIssues.insert(m_SceneReferenceIssues.end(), issues.begin(), issues.end());
        }
        for (const auto& uri : m_AdditionalScenes)
        {
            const auto issues = AssetReferenceValidator::ValidateBuildScene(Engine::PlatformPath::ResolveUri(uri));
            m_SceneReferenceIssues.insert(m_SceneReferenceIssues.end(), issues.begin(), issues.end());
        }
        m_HasSceneReferenceResult = true;
    }

    if (!m_HasSceneReferenceResult)
        return;

    if (m_SceneReferenceIssues.empty())
    {
        ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "%s", LOCAL_CSTR("BuildProfile.SceneReferencesPassed"));
        return;
    }

    ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s",
        LOCAL_FMT("BuildProfile.SceneReferenceIssueCount", m_SceneReferenceIssues.size()).c_str());
    for (const auto& issue : m_SceneReferenceIssues)
    {
        using Issue = EAssetReferenceIssueType;

        const char* reasonKey = "BuildProfile.ReferenceMissing";
        if (issue.IssueType == Issue::ExternalAbsolutePath)
            reasonKey = "BuildProfile.ReferenceAbsolute";
        else if (issue.IssueType == Issue::UnreadableDocument)
            reasonKey = "BuildProfile.ReferenceUnreadable";
        else if (issue.IssueType == Issue::OutsideRuntimeRoots)
            reasonKey = "BuildProfile.ReferenceOutsideRoots";

        ImGui::BulletText("%s", LOCAL_CSTR(reasonKey));
        ImGui::TextWrapped("%s", Engine::WStrToStr(issue.SourceFile.wstring()).c_str());
        if (!issue.TargetReference.empty())
            ImGui::TextWrapped("  -> %s", issue.TargetReference.c_str());
    }
}

void BuildProfilePanel::Update(f32)
{
    m_ClientBuild.Update();
    if (m_ClientBuild.GetStatus() == ClientBuildJob::Status::Succeeded)
    {
        const auto& exportedDir = m_ClientBuild.GetExportedDirectory();
        if (!exportedDir.empty() && m_LastPakSummaryDirectory != exportedDir)
        {
            m_LastPakSummaryDirectory = exportedDir;
            m_LastPakSummary = ReadClientPackageManifestSummary(exportedDir);
        }
    }

    const auto& output = m_ClientBuild.GetOutput();
    if (m_BuildLogText.size() != output.size())
    {
        m_BuildLogText = output;
        m_ScrollBuildLogToEnd = true;
    }
}

void BuildProfilePanel::DrawBamPakSummary() const
{
    if (!m_LastPakSummary.has_value())
        return;

    const auto& manifest = *m_LastPakSummary;
    std::uint64_t totalEntries = 0;
    std::uint64_t totalBytes = 0;
    for (const auto& pak : manifest.Paks)
    {
        totalEntries += pak.EntryCount;
        totalBytes += pak.ByteSize;
    }

    ImGui::Spacing();
    if (EditorUI::BeginPropertyTable("##BamPakSummary"))
    {
        EditorUI::PropertyRow("BuildProfile.BamPakSummary");
        ImGui::TextUnformatted(LOCAL_FMT("BuildProfile.BamPakSummaryValue",
            manifest.Paks.size(),
            static_cast<unsigned long long>(totalEntries),
            static_cast<unsigned long long>(totalBytes)).c_str());

        EditorUI::PropertyRow("BuildProfile.IntegrityCheck");
        if (manifest.IntegrityVerified)
            ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "%s", LOCAL_CSTR("BuildProfile.IntegrityVerified"));
        else
            ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", LOCAL_CSTR("BuildProfile.IntegrityFailed"));

        for (const auto& pak : manifest.Paks)
        {
            EditorUI::PropertyLabel(pak.Path.c_str());
            EditorUI::PropertyValue();
            ImGui::Text("%llu entries / %llu bytes / 0x%016llX",
                static_cast<unsigned long long>(pak.EntryCount),
                static_cast<unsigned long long>(pak.ByteSize),
                static_cast<unsigned long long>(pak.Hash64));
        }

        if (!manifest.DirectFileExceptions.empty())
        {
            EditorUI::PropertyRow("BuildProfile.DirectFileExceptions");
            ImGui::TextUnformatted(LOCAL_FMT("BuildProfile.DirectFileExceptionsValue",
                manifest.DirectFileExceptions.size()).c_str());
        }

        EditorUI::EndPropertyTable();
    }
}

void BuildProfilePanel::StartExport(ClientPackageRequest request)
{
    const auto& settings = EditorSettingsService::Get();
    m_LastPakSummary.reset();
    m_LastPakSummaryDirectory.clear();
    m_ClientBuild.Start(settings.GetProjectRoot(), std::move(request));
    m_ScrollBuildLogToEnd = true;
    m_BuildLogText.clear();
}

void BuildProfilePanel::DrawReplacePackageDialog()
{
    const auto title = LOCAL_LABEL("BuildProfile.ReplacePackageTitle");
    if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        return;

    if (m_PendingReplacement)
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + 34.f * ImGui::GetFontSize());
        ImGui::TextWrapped("%s", LOCAL_CSTR("BuildProfile.ReplacePackageMessage"));
        const auto path = Engine::WStrToStr(m_PendingReplacement->Destination.Directory.wstring());
        ImGui::TextWrapped("%s", path.c_str());
        ImGui::PopTextWrapPos();
        ImGui::Separator();

        const EditorUI::DialogAction actions[] = {
            { "EditorCommon.Cancel" }, { "BuildProfile.ReplacePackage", true, true }
        };
        const int action = EditorUI::DialogButtonBar("ReplacePackageActions", actions, IM_ARRAYSIZE(actions));
        if (action >= 0)
        {
            if (action == 1)
                StartExport(std::move(*m_PendingReplacement));
            m_PendingReplacement.reset();
            ImGui::CloseCurrentPopup();
        }
    }
    else
        ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

END
