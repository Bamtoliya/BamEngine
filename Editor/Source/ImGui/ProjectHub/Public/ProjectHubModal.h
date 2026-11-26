#pragma once

#include "Editor_Includes.h"
#include <filesystem>
#include <string>

BEGIN(Editor)

class ProjectHubModal
{
public:
	ProjectHubModal() = default;
	~ProjectHubModal() = default;

	void Open();
	// 에디터 시작 시: 프로젝트를 고르기 전에는 씬을 로드하지 않고, 취소하면 기본 프로젝트의 마지막 씬을 로드한다.
	void OpenAsStartup();
	void Close();
	bool IsOpen() const { return m_IsOpen; }
	void Draw();

private:
	void DrawRecentProjectsSection();
	void DrawCreateProjectSection();
	bool TrySwitchProject(const std::filesystem::path& projectFileOrRoot);
	void ExecuteSwitch(const std::filesystem::path& projectFileOrRoot);
	void Dismiss();
	void TryCreateProject();

private:
	bool m_OpenRequested = false;
	bool m_IsOpen = false;
	bool m_StartupPending = false;
	int m_ActiveTab = 0;
	char m_ProjectNameBuffer[128] = {};
	char m_TargetFolderBuffer[512] = {};
	int m_SelectedTemplateIndex = 0;
	bool m_CreateCppModule = false;
	std::string m_ErrorMessage;
};

END
