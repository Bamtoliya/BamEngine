#include "ProjectHubModal.h"
#include "EditorUI.h"
#include "FileDialogs.h"
#include "EditorSettingsService.h"
#include "ProjectSessionManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include "ProjectDescriptor.h"
#include <algorithm>
#include <cstring>

BEGIN(Editor)

namespace
{
	constexpr const char* kProjectHubPopupId = "ProjectHubModalPopup";

	void CopyUtf8ToBuffer(const std::string& text, char* buffer, size_t bufferSize)
	{
		if (!buffer || bufferSize == 0)
			return;
		const size_t copyLen = std::min(text.size(), bufferSize - 1);
		std::memcpy(buffer, text.data(), copyLen);
		buffer[copyLen] = '\0';
	}

	std::string TrimCopy(std::string_view input)
	{
		size_t first = 0;
		while (first < input.size() && (input[first] == ' ' || input[first] == '\t' || input[first] == '\r' || input[first] == '\n'))
			++first;
		size_t last = input.size();
		while (last > first && (input[last - 1] == ' ' || input[last - 1] == '\t' || input[last - 1] == '\r' || input[last - 1] == '\n'))
			--last;
		return std::string(input.substr(first, last - first));
	}
}

void ProjectHubModal::Open()
{
	m_OpenRequested = true;
	m_IsOpen = true;
	m_ErrorMessage.clear();

	if (EditorSettingsService::IsCreated())
		EditorSettingsService::Get().LoadRecentProjects();

	if (m_TargetFolderBuffer[0] == '\0')
	{
		const auto defaultRoot = Engine::PlatformPath::GetProjectRoot().parent_path();
		if (!defaultRoot.empty())
			CopyUtf8ToBuffer(Engine::WStrToStr(defaultRoot.wstring()), m_TargetFolderBuffer, sizeof(m_TargetFolderBuffer));
	}
}

void ProjectHubModal::Close()
{
	m_OpenRequested = false;
	m_IsOpen = false;
	m_ErrorMessage.clear();
}

void ProjectHubModal::Draw()
{
	if (m_OpenRequested)
	{
		ImGui::OpenPopup(kProjectHubPopupId);
		m_OpenRequested = false;
	}

	if (!m_IsOpen && !ImGui::IsPopupOpen(kProjectHubPopupId))
		return;

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(760.0f, 520.0f), ImGuiCond_Appearing);

	const std::string modalTitle = LOCAL("ProjectHub.Title") + "###" + kProjectHubPopupId;
	bool open = m_IsOpen;
	if (!ImGui::BeginPopupModal(modalTitle.c_str(), &open, ImGuiWindowFlags_NoCollapse))
	{
		if (!open)
			Close();
		return;
	}

	if (!open)
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		Close();
		return;
	}

	if (ImGui::BeginTabBar("ProjectHubTabs"))
	{
		if (ImGui::BeginTabItem(LOCAL_LABEL("ProjectHub.RecentProjects").c_str()))
		{
			DrawRecentProjectsSection();
			ImGui::EndTabItem();
		}

		if (ImGui::BeginTabItem(LOCAL_LABEL("ProjectHub.CreateNewProject").c_str()))
		{
			DrawCreateProjectSection();
			ImGui::EndTabItem();
		}

		ImGui::EndTabBar();
	}

	if (!m_ErrorMessage.empty())
	{
		ImGui::Spacing();
		ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", m_ErrorMessage.c_str());
	}

	ImGui::Spacing();
	ImGui::Separator();
	const EditorUI::DialogAction actions[] = { { "ProjectHub.Cancel", true, false } };
	if (EditorUI::DialogButtonBar("ProjectHubFooter", actions, 1) == 0)
	{
		ImGui::CloseCurrentPopup();
		Close();
	}

	ImGui::EndPopup();
}

void ProjectHubModal::DrawRecentProjectsSection()
{
	const float footerReserve = ImGui::GetFrameHeightWithSpacing() * 2.4f;
	const float listHeight = std::max(140.0f, ImGui::GetContentRegionAvail().y - footerReserve - ImGui::GetFrameHeightWithSpacing() * 1.6f);

	if (ImGui::Button(LOCAL_LABEL("ProjectHub.OpenProject").c_str()))
	{
		std::wstring selectedFile;
		const std::vector<FileDialogFilter> filters = { { L"BamEngine Project (*.bamproject)", L"*.bamproject" } };
		if (FileDialogs::OpenFileDialog(selectedFile, filters, L"", "ProjectHub.OpenProject") && !selectedFile.empty())
			TrySwitchProject(std::filesystem::path(selectedFile));
	}
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("ProjectHub.OpenFolder").c_str()))
	{
		std::wstring selectedFolder;
		if (FileDialogs::OpenFolderDialog(selectedFolder, L"", "ProjectHub.OpenFolder") && !selectedFolder.empty())
			TrySwitchProject(std::filesystem::path(selectedFolder));
	}

	ImGui::Spacing();
	ImGui::BeginChild("RecentProjectsList", ImVec2(0.0f, listHeight), ImGuiChildFlags_Borders);

	const std::vector<RecentProjectEntry> emptyEntries;
	const auto& recentProjects = EditorSettingsService::IsCreated() ? EditorSettingsService::Get().GetRecentProjects() : emptyEntries;
	const auto activeRoot = Engine::PlatformPath::Normalize(Engine::PlatformPath::GetProjectRoot());

	if (recentProjects.empty())
	{
		EditorUI::EmptyState("ProjectHub.NoRecentProjects");
	}
	else
	{
		for (size_t i = 0; i < recentProjects.size(); ++i)
		{
			const auto& entry = recentProjects[i];
			const std::filesystem::path entryTarget = !entry.ProjectFilePath.empty()
				? std::filesystem::path(Engine::StrToWStr(entry.ProjectFilePath))
				: std::filesystem::path(Engine::StrToWStr(entry.ProjectRoot));
			const auto entryRoot = Engine::PlatformPath::Normalize(
				!entry.ProjectRoot.empty() ? std::filesystem::path(Engine::StrToWStr(entry.ProjectRoot)) : entryTarget.parent_path());
			const bool isActive = !activeRoot.empty() && Engine::PlatformPath::IsSamePath(activeRoot, entryRoot);

			ImGui::PushID(static_cast<int>(i));
			ImGui::BeginGroup();
			ImGui::TextUnformatted(entry.ProjectName.empty() ? entry.ProjectRoot.c_str() : entry.ProjectName.c_str());
			if (isActive)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[%s]", LOCAL_CSTR("ProjectHub.ActiveProject"));
			}
			ImGui::TextDisabled("%s", !entry.ProjectFilePath.empty() ? entry.ProjectFilePath.c_str() : entry.ProjectRoot.c_str());
			ImGui::EndGroup();

			const float buttonWidth = ImGui::CalcTextSize(LOCAL_CSTR("ProjectHub.Open")).x + ImGui::GetStyle().FramePadding.x * 2.0f + 12.0f;
			ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - buttonWidth));
			ImGui::BeginDisabled(isActive);
			if (ImGui::Button(LOCAL_LABEL("ProjectHub.Open").c_str(), ImVec2(buttonWidth, 0.0f)))
				TrySwitchProject(entryTarget);
			ImGui::EndDisabled();

			ImGui::Separator();
			ImGui::PopID();
		}
	}

	ImGui::EndChild();
}

void ProjectHubModal::DrawCreateProjectSection()
{
	if (EditorUI::BeginPropertyTable("ProjectHubCreateTable"))
	{
		EditorUI::PropertyRow("ProjectHub.ProjectName");
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputText("##ProjectNameInput", m_ProjectNameBuffer, sizeof(m_ProjectNameBuffer));

		EditorUI::PropertyRow("ProjectHub.TargetFolder");
		const float browseWidth = ImGui::CalcTextSize(LOCAL_CSTR("ProjectHub.Browse")).x + ImGui::GetStyle().FramePadding.x * 2.0f + 10.0f;
		ImGui::SetNextItemWidth(std::max(80.0f, ImGui::GetContentRegionAvail().x - browseWidth - ImGui::GetStyle().ItemSpacing.x));
		ImGui::InputText("##TargetFolderInput", m_TargetFolderBuffer, sizeof(m_TargetFolderBuffer));
		ImGui::SameLine();
		if (ImGui::Button(LOCAL_LABEL("ProjectHub.Browse").c_str(), ImVec2(browseWidth, 0.0f)))
		{
			std::wstring selectedFolder;
			const std::wstring initialDir = Engine::StrToWStr(m_TargetFolderBuffer);
			if (FileDialogs::OpenFolderDialog(selectedFolder, initialDir, "ProjectHub.TargetFolder") && !selectedFolder.empty())
				CopyUtf8ToBuffer(Engine::WStrToStr(selectedFolder), m_TargetFolderBuffer, sizeof(m_TargetFolderBuffer));
		}

		EditorUI::PropertyRow("ProjectHub.Template");
		const std::string templateEmpty = LOCAL("ProjectHub.TemplateEmpty");
		const std::string template3D = LOCAL("ProjectHub.Template3D");
		const std::string template2D = LOCAL("ProjectHub.Template2D");
		const char* templateLabels[3] = { templateEmpty.c_str(), template3D.c_str(), template2D.c_str() };
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::Combo("##ProjectTemplateCombo", &m_SelectedTemplateIndex, templateLabels, 3);

		EditorUI::EndPropertyTable();
	}

	ImGui::Spacing();
	if (ImGui::Button(LOCAL_LABEL("ProjectHub.CreateAndOpen").c_str()))
		TryCreateProject();
}

void ProjectHubModal::TrySwitchProject(const std::filesystem::path& projectFileOrRoot)
{
	m_ErrorMessage.clear();
	if (!ProjectSessionManager::IsCreated())
		ProjectSessionManager::Create();

	const auto result = ProjectSessionManager::Get().SwitchProject(projectFileOrRoot);
	if (result.Succeeded)
	{
		ImGui::CloseCurrentPopup();
		Close();
		return;
	}

	if (!result.Cancelled)
	{
		m_ErrorMessage = !result.ErrorMessage.empty() ? result.ErrorMessage : LOCAL("ProjectHub.SwitchFailed");
		NotificationManager::Get().AddNotification(LOCAL("ProjectHub.Title"), m_ErrorMessage, ELogLevel::Error);
	}
}

void ProjectHubModal::TryCreateProject()
{
	m_ErrorMessage.clear();
	const std::string projectName = TrimCopy(m_ProjectNameBuffer);
	const std::string targetFolderUtf8 = TrimCopy(m_TargetFolderBuffer);

	if (projectName.empty())
	{
		m_ErrorMessage = LOCAL("ProjectHub.InvalidProjectName");
		return;
	}
	if (targetFolderUtf8.empty())
	{
		m_ErrorMessage = LOCAL("ProjectHub.InvalidTargetFolder");
		return;
	}

	const std::filesystem::path targetDir = std::filesystem::path(Engine::StrToWStr(targetFolderUtf8)) / Engine::StrToWStr(projectName);
	Engine::EProjectTemplate projectTemplate = Engine::EProjectTemplate::Empty;
	if (m_SelectedTemplateIndex == 1)
		projectTemplate = Engine::EProjectTemplate::Default3D;
	else if (m_SelectedTemplateIndex == 2)
		projectTemplate = Engine::EProjectTemplate::Default2D;

	if (!ProjectSessionManager::IsCreated())
		ProjectSessionManager::Create();

	const auto result = ProjectSessionManager::Get().CreateProject(targetDir, projectName, projectTemplate, true);
	if (result.Succeeded)
	{
		ImGui::CloseCurrentPopup();
		Close();
		return;
	}

	if (!result.Cancelled)
	{
		m_ErrorMessage = !result.ErrorMessage.empty() ? result.ErrorMessage : LOCAL("ProjectHub.CreateFailed");
		NotificationManager::Get().AddNotification(LOCAL("ProjectHub.Title"), m_ErrorMessage, ELogLevel::Error);
	}
}

END
