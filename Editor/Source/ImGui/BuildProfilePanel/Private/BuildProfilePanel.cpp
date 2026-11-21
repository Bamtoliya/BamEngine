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
    const auto& startup = profile.StartupScene.empty()
        ? settings.GetProjectSettings().StartupScenePath : profile.StartupScene;

    if (!startup.empty())
        m_StartupScene = Engine::PlatformPath::GetProjectAssetRoot() / std::filesystem::u8path(startup);
    m_AdditionalScenes = profile.AdditionalScenes;
    m_AdditionalAssets = profile.AdditionalAssets;

    m_OutputDirectory = std::filesystem::u8path(profile.OutputDirectory);
    if (!profile.PackageDirectoryName.empty())
        m_PackageDirectoryName = profile.PackageDirectoryName;
    if (!profile.ProgramName.empty())
        m_ProgramName = profile.ProgramName;
    return EResult::Success;
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

    m_StartupScene = root / relative;
    m_SceneReferenceIssues.clear();
    m_HasSceneReferenceResult = false;
    SaveSelection();
}

void BuildProfilePanel::SaveSelection()
{
    const auto& root = Engine::PlatformPath::GetProjectAssetRoot();
    const auto scene = m_StartupScene.lexically_relative(root).generic_u8string();
    const auto output = m_OutputDirectory.generic_u8string();

    const BuildProfileWorkspace profile{
        std::string(scene.begin(), scene.end()), std::string(output.begin(), output.end()),
        m_PackageDirectoryName, m_ProgramName, m_AdditionalScenes, m_AdditionalAssets
    };
    if (EditorSettingsService::Get().SetBuildProfileWorkspace(profile) != EEditorSettingsSaveStatus::Saved)
    {
        NotificationManager::Get().AddNotification(
            LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.SelectionSaveFailed"), ELogLevel::Warn);
    }
}

void BuildProfilePanel::DrawPackageEntries(const char* labelKey, std::vector<std::string>& entries, bool scenes)
{
    EditorUI::PropertyRow(labelKey);
    ImGui::PushID(labelKey);
    if (ImGui::Button(LOCAL_LABEL("EditorCommon.Add").c_str()))
    {
        using Path = Engine::PlatformPath;
        const auto& assetRoot = Path::GetProjectAssetRoot();
        const vector<FileDialogFilter> filters = scenes
            ? vector<FileDialogFilter>{ { Engine::StrToWStr(LOCAL("BuildProfile.SceneFiles")), L"*.bamscene.json" } }
            : vector<FileDialogFilter>{ { Engine::StrToWStr(LOCAL("BuildProfile.AllFiles")), L"*.*" } };
        std::wstring selected;
        if (FileDialogs::OpenFileDialog(selected, filters, assetRoot.wstring(), labelKey))
        {
            std::error_code error;
            const auto relative = std::filesystem::relative(selected, assetRoot, error);
            const bool valid = !error && !relative.empty() && !relative.is_absolute() && *relative.begin() != L".." &&
                std::filesystem::is_regular_file(selected, error) &&
                (!scenes || Engine::ToLowerStr(Engine::WStrToStr(relative.filename().wstring())).ends_with(".bamscene.json"));
            if (valid)
            {
                const auto uri = "Project://" + Engine::WStrToStr(relative.generic_wstring());
                if (std::ranges::find(entries, uri) == entries.end())
                {
                    entries.push_back(uri);
                    if (scenes)
                        m_HasSceneReferenceResult = false;
                    SaveSelection();
                }
            }
            else
            {
                const auto message = scenes ? LOCAL("BuildProfile.InvalidAdditionalScene")
                    : LOCAL("BuildProfile.InvalidAdditionalAsset");
                NotificationManager::Get().AddNotification(LOCAL("BuildProfile.Title"),
                    message, ELogLevel::Warn);
            }
        }
    }

    if (entries.empty())
        ImGui::TextUnformatted(LOCAL_CSTR("EditorCommon.None"));
    for (std::size_t i = 0; i < entries.size(); ++i)
    {
        ImGui::PushID(static_cast<int>(i));
        const bool remove = ImGui::SmallButton(LOCAL_LABEL("BuildProfile.RemoveEntry").c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", entries[i].c_str());
        ImGui::PopID();
        if (remove)
        {
            entries.erase(entries.begin() + i);
            if (scenes)
                m_HasSceneReferenceResult = false;
            SaveSelection();
            break;
        }
    }
    ImGui::PopID();
}

void BuildProfilePanel::Draw()
{
    if (!m_Open)
        return;

    if (BeginPanelWindow(LOCAL_LABEL("BuildProfile.Title").c_str(), m_Open))
    {
        m_Focused = ImGui::IsWindowFocused();
        m_Hovered = ImGui::IsWindowHovered();

        if (EditorUI::BeginPropertyTable("##BuildProfileSettings"))
        {
            EditorUI::PropertyRow("BuildProfile.Target");
            ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.WindowsRelease"));

            EditorUI::PropertyRow("BuildProfile.Format");
            ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.Folder"));

            EditorUI::PropertyRow("BuildProfile.RenderPipelineProfile");
            const auto& profile = EditorSettingsService::Get().GetProjectSettings().RenderPipelineProfilePath;
            ImGui::TextWrapped("%s", profile.empty() ? LOCAL_CSTR("EditorCommon.None") : profile.c_str());

            EditorUI::PropertyRow("Settings.StartupScene");
            const bool running = m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running;
            ImGui::BeginDisabled(running);

            if (ImGui::Button(LOCAL_LABEL("Settings.BrowseScene").c_str()))
            {
                std::wstring selectedPath;
                const auto scenesDirectory = Engine::PlatformPath::GetProjectAssetRoot() / "Scenes";
                const vector<FileDialogFilter> filters = {
                    { Engine::StrToWStr(LOCAL("BuildProfile.SceneFiles")), L"*.bamscene.json" }
                };

                if (FileDialogs::OpenFileDialog(selectedPath, filters, scenesDirectory.wstring(), "BuildProfile.StartupScene"))
                    SelectStartupScene(selectedPath);
            }

            ImGui::SameLine();
            if (ImGui::Button(LOCAL_LABEL("BuildProfile.UseCurrentScene").c_str()))
            {
                const auto* document = Application::Get().GetSceneDocument();
                if (PIEManager::Get().IsEditing() && document && !document->GetPath().empty() &&
                    !document->HasUnsavedChanges())
                {
                    SelectStartupScene(document->GetPath());
                }
                else
                {
                    NotificationManager::Get().AddNotification(
                        LOCAL("BuildProfile.Title"), LOCAL("BuildProfile.SaveCurrentSceneFirst"), ELogLevel::Warn);
                }
            }

            ImGui::EndDisabled();
            ImGui::TextWrapped("%s", m_StartupScene.empty() ? LOCAL_CSTR("EditorCommon.None")
                : Engine::WStrToStr(m_StartupScene.wstring()).c_str());

            ImGui::BeginDisabled(running);
            DrawPackageEntries("BuildProfile.AdditionalScenes", m_AdditionalScenes, true);
            DrawPackageEntries("BuildProfile.AdditionalAssets", m_AdditionalAssets, false);
            ImGui::EndDisabled();

            EditorUI::PropertyRow("BuildProfile.OutputDirectory");
            ImGui::BeginDisabled(running);

            if (ImGui::Button(LOCAL_LABEL("Settings.BrowseFolder").c_str()))
            {
                std::wstring selectedPath;
                if (FileDialogs::OpenFolderDialog(selectedPath, m_OutputDirectory.wstring(), "BuildProfile.OutputFolder"))
                {
                    m_OutputDirectory = selectedPath;
                    SaveSelection();
                }
            }

            ImGui::EndDisabled();
            if (m_OutputDirectory.empty())
            {
                ImGui::TextUnformatted(LOCAL_CSTR("EditorCommon.None"));
            }
            else
            {
                ImGui::TextWrapped("%s", Engine::WStrToStr(m_OutputDirectory.wstring()).c_str());
            }

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

        DrawSceneReferenceCheck();
        DrawClientBuild();
        DrawReplacePackageDialog();
    }
    ImGui::End();
}

void BuildProfilePanel::DrawSceneReferenceCheck()
{
    ImGui::Separator();
    ImGui::BeginDisabled(m_StartupScene.empty() || m_ClientBuild.GetStatus() == ClientBuildJob::Status::Running);
    if (ImGui::Button(LOCAL_LABEL("BuildProfile.CheckSceneReferences").c_str()))
    {
        m_SceneReferenceIssues = AssetReferenceValidator::ValidateBuildScene(m_StartupScene);
        for (const auto& uri : m_AdditionalScenes)
        {
            const auto issues = AssetReferenceValidator::ValidateBuildScene(Engine::PlatformPath::ResolveUri(uri));
            m_SceneReferenceIssues.insert(m_SceneReferenceIssues.end(), issues.begin(), issues.end());
        }
        m_HasSceneReferenceResult = true;
    }
    ImGui::EndDisabled();

    if (!m_HasSceneReferenceResult)
        return;

    if (m_SceneReferenceIssues.empty())
    {
        ImGui::TextUnformatted(LOCAL_CSTR("BuildProfile.SceneReferencesPassed"));
        return;
    }

    ImGui::TextUnformatted(LOCAL_FMT("BuildProfile.SceneReferenceIssueCount", m_SceneReferenceIssues.size()).c_str());
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
}

void BuildProfilePanel::DrawClientBuild()
{
    using Status = ClientBuildJob::Status;

    ImGui::Separator();
    const auto& projectRoot = EditorSettingsService::Get().GetProjectRoot();
    const bool running = m_ClientBuild.GetStatus() == Status::Running;
    ImGui::BeginDisabled(running);
    if (ImGui::Button(LOCAL_LABEL("BuildProfile.BuildClient").c_str()))
    {
        m_ClientBuild.Start(projectRoot);
        m_ScrollBuildLogToEnd = true;
        m_BuildLogText.clear();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();
    ImGui::BeginDisabled(running || m_StartupScene.empty() || m_OutputDirectory.empty() ||
        m_PackageDirectoryName.empty() || m_ProgramName.empty());
    if (ImGui::Button(LOCAL_LABEL("BuildProfile.BuildAndExport").c_str()))
    {
        try
        {
            ClientPackageRequest request{ m_StartupScene, m_OutputDirectory, m_PackageDirectoryName, m_ProgramName };
            request.AdditionalScenes = m_AdditionalScenes;
            request.AdditionalAssets = m_AdditionalAssets;
            request.Destination = InspectClientPackageDestination(projectRoot, request);
            if (request.Destination.ExistingPackage)
            {
                m_PendingReplacement = std::move(request);
                ImGui::OpenPopup(LOCAL_LABEL("BuildProfile.ReplacePackageTitle").c_str());
            }
            else
                StartExport(std::move(request));
        }
        catch (const std::exception& error)
        {
            m_ClientBuild.SetExportPreparationError(LOCAL("BuildProfile.InvalidPackageDestination") + '\n' + error.what());
            m_BuildLogText.clear();
            m_ScrollBuildLogToEnd = true;
        }
    }
    ImGui::EndDisabled();

    const bool exporting = m_ClientBuild.IsExport();
    switch (m_ClientBuild.GetStatus())
    {
    case Status::Idle: break;
    case Status::Running:
        ImGui::TextUnformatted(LOCAL_CSTR(exporting ? "BuildProfile.ExportRunning" : "BuildProfile.BuildRunning"));
        break;
    case Status::Succeeded:
        ImGui::TextUnformatted(LOCAL_CSTR(exporting ? "BuildProfile.ExportSucceeded" : "BuildProfile.BuildSucceeded"));
        break;
    case Status::Failed:
        ImGui::TextUnformatted(LOCAL_CSTR(exporting ? "BuildProfile.ExportFailed" : "BuildProfile.BuildFailed"));
        break;
    }

    const auto& exportedDirectory = m_ClientBuild.GetExportedDirectory();
    if (!exportedDirectory.empty() && ImGui::Button(LOCAL_LABEL("BuildProfile.OpenExportFolder").c_str()))
    {
        const auto result = ShellExecuteW(nullptr, L"open", exportedDirectory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        if (reinterpret_cast<INT_PTR>(result) <= 32)
            EDITOR_LOG_WARN("Could not open exported folder: {}", Engine::WStrToStr(exportedDirectory.wstring()));
    }

    const auto& output = m_ClientBuild.GetOutput();
    if (m_BuildLogText.size() != output.size())
    {
        m_BuildLogText = output;
        m_ScrollBuildLogToEnd = true;
    }

    ImGui::PushID("BuildLog");
    ImGui::BeginDisabled(m_BuildLogText.empty());
    if (ImGui::Button(LOCAL_LABEL("EditorCommon.Copy").c_str()))
        ImGui::SetClipboardText(m_BuildLogText.c_str());
    ImGui::EndDisabled();
    ImGui::PopID();

    const ImVec2 logSize(std::max(1.0f, ImGui::GetContentRegionAvail().x), 220.0f);
    if (m_BuildLogText.empty())
    {
        if (ImGui::BeginChild("##EmptyClientBuildOutput", logSize, ImGuiChildFlags_Borders))
            ImGui::TextDisabled("%s", LOCAL_CSTR("BuildProfile.NoBuildOutput"));
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

void BuildProfilePanel::StartExport(ClientPackageRequest request)
{
    const auto& settings = EditorSettingsService::Get();
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
