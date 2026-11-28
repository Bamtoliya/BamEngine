#include "ProjectHubModal.h"
#include "EditorUI.h"
#include "FileDialogs.h"
#include "EditorSettingsService.h"
#include "ProjectSessionManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include "ProjectDescriptor.h"
#include "Application.h"
#include "ImGuiManager.h"
#include <algorithm>
#include <cstring>

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

	RefreshRecentProjectStatus();

	if (m_TargetFolderBuffer[0] == '\0')
	{
		const auto defaultRoot = Engine::PlatformPath::GetProjectRoot().parent_path();
		if (!defaultRoot.empty())
			CopyUtf8ToBuffer(Engine::Unicode::WideToUtf8(defaultRoot.wstring()), m_TargetFolderBuffer, sizeof(m_TargetFolderBuffer));
	}
}

void ProjectHubModal::OpenAsStartup()
{
	m_StartupPending = true;
	Open();
}

void ProjectHubModal::Dismiss()
{
	const bool startup = m_StartupPending;
	Close();
	if (startup)
	{
		m_StartupPending = false;
		Application::Get().RestoreLastEditedScene();
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
	const std::string modalTitle = LOCAL("ProjectHub.Title") + "###" + kProjectHubPopupId;
	if (m_OpenRequested)
	{
		ImGui::OpenPopup(modalTitle.c_str());
		m_OpenRequested = false;
	}

	if (!m_IsOpen && !ImGui::IsPopupOpen(modalTitle.c_str()))
		return;

	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	const ImVec2 workSize = ImGui::GetMainViewport()->WorkSize;
	ImGui::SetNextWindowSize(ImVec2(std::clamp(workSize.x * 0.62f, 900.0f, 1280.0f), std::clamp(workSize.y * 0.72f, 560.0f, 820.0f)), ImGuiCond_Appearing);

	bool open = m_IsOpen;
	ImVec4 modalBg = ImGui::GetStyleColorVec4(ImGuiCol_PopupBg);
	modalBg.w = 1.0f;
	ImGui::PushStyleColor(ImGuiCol_PopupBg, modalBg);
	if (!ImGui::BeginPopupModal(modalTitle.c_str(), &open, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		ImGui::PopStyleColor();
		if (!open)
			Dismiss();
		return;
	}

	if (!open)
	{
		ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
		ImGui::PopStyleColor();
		Dismiss();
		return;
	}

	const EditorUI::DialogAction recentActions[] = { { "ProjectHub.Cancel", true, false } };
	const EditorUI::DialogAction createActions[] = {
		{ "ProjectHub.Cancel", true, false },
		{ "ProjectHub.CreateAndOpen", true, false }
	};
	const int actionCount = (m_ActiveTab == 1) ? 2 : 1;
	const EditorUI::DialogAction* actions = (m_ActiveTab == 1) ? createActions : recentActions;

	// 본문은 고정 높이 자식 영역에 두어 모달 자체는 스크롤되지 않고, 푸터(오류·취소/확인)는 항상 같은 패널 안에 남는다.
	const ImGuiStyle& style = ImGui::GetStyle();
	const float errorReserve = m_ErrorMessage.empty() ? 0.0f : ImGui::GetTextLineHeightWithSpacing();
	const float footerReserve = EditorUI::GetDialogButtonBarHeight(actions, actionCount) + style.ItemSpacing.y * 3.0f + errorReserve;
	if (ImGui::BeginChild("ProjectHubBody", ImVec2(0.0f, -footerReserve), ImGuiChildFlags_None,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		if (ImGui::BeginTabBar("ProjectHubTabs"))
		{
			if (ImGui::BeginTabItem(LOCAL_LABEL("ProjectHub.RecentProjects").c_str()))
			{
				m_ActiveTab = 0;
				DrawRecentProjectsSection();
				ImGui::EndTabItem();
			}

			if (ImGui::BeginTabItem(LOCAL_LABEL("ProjectHub.CreateNewProject").c_str()))
			{
				m_ActiveTab = 1;
				DrawCreateProjectSection();
				ImGui::EndTabItem();
			}

			ImGui::EndTabBar();
		}
	}
	ImGui::EndChild();

	if (!m_ErrorMessage.empty())
		ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", m_ErrorMessage.c_str());

	ImGui::Separator();
	const int clicked = EditorUI::DialogButtonBar("ProjectHubFooter", actions, actionCount);
	if (clicked == 0)
	{
		ImGui::CloseCurrentPopup();
		Dismiss();
	}
	else if (clicked == 1 && m_ActiveTab == 1)
	{
		TryCreateProject();
	}

	ImGui::EndPopup();
	ImGui::PopStyleColor();
}

void ProjectHubModal::RefreshRecentProjectStatus()
{
	m_RecentProjectExists.clear();
	m_MissingProjectCount = 0;

	if (!EditorSettingsService::IsCreated())
		return;

	const auto& recentProjects = EditorSettingsService::Get().GetRecentProjects();
	m_RecentProjectExists.reserve(recentProjects.size());
	for (const auto& entry : recentProjects)
	{
		const std::filesystem::path entryTarget = !entry.ProjectFilePath.empty()
			? std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectFilePath))
			: std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectRoot));
		std::error_code ec;
		const bool exists = std::filesystem::exists(entryTarget, ec);
		m_RecentProjectExists.push_back(exists);
		if (!exists)
			++m_MissingProjectCount;
	}
}

void ProjectHubModal::DrawRecentProjectsSection()
{
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

	const std::vector<RecentProjectEntry> emptyEntries;
	const auto& recentProjects = EditorSettingsService::IsCreated() ? EditorSettingsService::Get().GetRecentProjects() : emptyEntries;
	if (m_RecentProjectExists.size() != recentProjects.size())
		RefreshRecentProjectStatus();

	const auto activeRoot = Engine::PlatformPath::Normalize(Engine::PlatformPath::GetProjectRoot());

	if (m_MissingProjectCount > 0 && EditorSettingsService::IsCreated())
	{
		ImGui::SameLine();
		if (ImGui::Button(LOCAL_LABEL("ProjectHub.CleanMissing").c_str()))
		{
			std::vector<std::filesystem::path> missingTargets;
			missingTargets.reserve(m_MissingProjectCount);
			for (size_t i = 0; i < recentProjects.size() && i < m_RecentProjectExists.size(); ++i)
			{
				if (!m_RecentProjectExists[i])
				{
					const auto& entry = recentProjects[i];
					missingTargets.push_back(!entry.ProjectFilePath.empty()
						? std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectFilePath))
						: std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectRoot)));
				}
			}
			for (const auto& target : missingTargets)
				EditorSettingsService::Get().RemoveRecentProject(target);
			RefreshRecentProjectStatus();
		}
	}

	ImGui::Spacing();
	ImGui::BeginChild("RecentProjectsList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);

	const auto& currentRecentProjects = EditorSettingsService::IsCreated() ? EditorSettingsService::Get().GetRecentProjects() : emptyEntries;
	std::filesystem::path pendingRemoveTarget;

	if (currentRecentProjects.empty())
	{
		EditorUI::EmptyState("ProjectHub.NoRecentProjects");
	}
	else
	{
		for (size_t i = 0; i < currentRecentProjects.size(); ++i)
		{
			const auto& entry = currentRecentProjects[i];
			const std::filesystem::path entryTarget = !entry.ProjectFilePath.empty()
				? std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectFilePath))
				: std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectRoot));
			const auto entryRoot = Engine::PlatformPath::Normalize(
				!entry.ProjectRoot.empty() ? std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectRoot)) : entryTarget.parent_path());
			const bool isActive = !activeRoot.empty() && Engine::PlatformPath::IsSamePath(activeRoot, entryRoot);
			const bool exists = (i < m_RecentProjectExists.size()) ? m_RecentProjectExists[i] : false;

			ImGui::PushID(static_cast<int>(i));
			ImGui::BeginGroup();
			ImGui::TextUnformatted(entry.ProjectName.empty() ? entry.ProjectRoot.c_str() : entry.ProjectName.c_str());
			if (isActive)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[%s]", LOCAL_CSTR("ProjectHub.ActiveProject"));
			}
			if (!exists)
			{
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.35f, 1.0f), "[%s]", LOCAL_CSTR("ProjectHub.MissingProject"));
			}
			ImGui::TextDisabled("%s", !entry.ProjectFilePath.empty() ? entry.ProjectFilePath.c_str() : entry.ProjectRoot.c_str());
			ImGui::EndGroup();

			const ImGuiStyle& style = ImGui::GetStyle();
			const float openButtonWidth = ImGui::CalcTextSize(LOCAL_CSTR("ProjectHub.Open")).x + style.FramePadding.x * 2.0f + 12.0f;
			const float removeButtonWidth = !exists
				? (ImGui::CalcTextSize(LOCAL_CSTR("ProjectHub.RemoveFromList")).x + style.FramePadding.x * 2.0f + 12.0f)
				: 0.0f;
			const float totalButtonsWidth = openButtonWidth + (!exists ? (removeButtonWidth + style.ItemSpacing.x) : 0.0f);

			ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - totalButtonsWidth));
			if (!exists)
			{
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.22f, 0.22f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.28f, 0.28f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
				if (ImGui::Button(LOCAL_LABEL("ProjectHub.RemoveFromList").c_str(), ImVec2(removeButtonWidth, 0.0f)))
					pendingRemoveTarget = entryTarget;
				ImGui::PopStyleColor(3);
				ImGui::SameLine();
			}

			ImGui::BeginDisabled(isActive || !exists);
			if (ImGui::Button(LOCAL_LABEL("ProjectHub.Open").c_str(), ImVec2(openButtonWidth, 0.0f)))
				TrySwitchProject(entryTarget);
			ImGui::EndDisabled();

			ImGui::Separator();
			ImGui::PopID();
		}
	}

	if (!pendingRemoveTarget.empty() && EditorSettingsService::IsCreated())
	{
		EditorSettingsService::Get().RemoveRecentProject(pendingRemoveTarget);
		RefreshRecentProjectStatus();
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
			const std::wstring initialDir = Engine::Unicode::Utf8ToWide(m_TargetFolderBuffer);
			if (FileDialogs::OpenFolderDialog(selectedFolder, initialDir, "ProjectHub.TargetFolder") && !selectedFolder.empty())
				CopyUtf8ToBuffer(Engine::Unicode::WideToUtf8(selectedFolder), m_TargetFolderBuffer, sizeof(m_TargetFolderBuffer));
		}

		EditorUI::PropertyRow("ProjectHub.Template");
		const std::string templateEmpty = LOCAL("ProjectHub.TemplateEmpty");
		const std::string template3D = LOCAL("ProjectHub.Template3D");
		const std::string template2D = LOCAL("ProjectHub.Template2D");
		const char* templateLabels[3] = { templateEmpty.c_str(), template3D.c_str(), template2D.c_str() };
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::Combo("##ProjectTemplateCombo", &m_SelectedTemplateIndex, templateLabels, 3);

		EditorUI::PropertyRow("ProjectHub.CreateCppModule");
		ImGui::Checkbox("##CreateCppModuleCheckbox", &m_CreateCppModule);

		EditorUI::EndPropertyTable();
	}
}

bool ProjectHubModal::TrySwitchProject(const std::filesystem::path& projectFileOrRoot)
{
	m_ErrorMessage.clear();

	// 종료와 같은 미저장 확인 흐름(리소스 편집기 목록 -> 씬 문서 저장/버리기/취소)을 거친 뒤에만 전환한다.
	const bool accepted = ImGuiManager::Get().RequestGuardedAction([this, projectFileOrRoot]() { ExecuteSwitch(projectFileOrRoot); });
	if (!accepted)
		m_ErrorMessage = LOCAL("ProjectHub.SwitchBusy");
	return accepted;
}

void ProjectHubModal::ExecuteSwitch(const std::filesystem::path& projectFileOrRoot)
{
	if (!ProjectSessionManager::IsCreated())
		ProjectSessionManager::Create();

	ProjectSwitchOptions options{};
	options.StopPIEAutomatically = true;
	options.AllowUnsavedDiscardWithoutPrompt = true; // 확인은 이미 끝났다.

	const auto result = ProjectSessionManager::Get().SwitchProject(projectFileOrRoot, options);
	if (result.Succeeded)
	{
		m_StartupPending = false;
		Close();
		return;
	}

	m_ErrorMessage = LOCAL("ProjectHub.SwitchFailed");
	if (!result.ErrorMessage.empty())
		m_ErrorMessage += ": " + result.ErrorMessage;
	NotificationManager::Get().AddNotification(LOCAL("ProjectHub.Title"), m_ErrorMessage, ELogLevel::Error);
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

	const std::filesystem::path targetDir = std::filesystem::path(Engine::Unicode::Utf8ToWide(targetFolderUtf8)) / Engine::Unicode::Utf8ToWide(projectName);
	Engine::EProjectTemplate projectTemplate = Engine::EProjectTemplate::Empty;
	if (m_SelectedTemplateIndex == 1)
		projectTemplate = Engine::EProjectTemplate::Default3D;
	else if (m_SelectedTemplateIndex == 2)
		projectTemplate = Engine::EProjectTemplate::Default2D;

	if (!ProjectSessionManager::IsCreated())
		ProjectSessionManager::Create();

	const auto result =
		ProjectSessionManager::Get().CreateProject(
			targetDir,
			projectName,
			projectTemplate,
			false, // 생성만 수행
			{},
			m_CreateCppModule
		);

	if (!result.Succeeded)
	{
		m_ErrorMessage = !result.ErrorMessage.empty()
			? result.ErrorMessage
			: LOCAL("ProjectHub.CreateFailed");

		NotificationManager::Get().AddNotification(
			LOCAL("ProjectHub.Title"),
			m_ErrorMessage,
			ELogLevel::Error
		);

		return;
	}

	// 이미 생성된 프로젝트를 전환 대상으로 사용한다.
	// 생성은 끝났지만 전환이 취소/실패하면 팝업을 닫지 않고 사유(미저장 확인 등)를 표시한다.
	TrySwitchProject(result.ProjectFilePath);
}
