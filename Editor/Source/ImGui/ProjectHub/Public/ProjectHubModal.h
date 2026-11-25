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
	void Close();
	bool IsOpen() const { return m_IsOpen; }
	void Draw();

private:
	void DrawRecentProjectsSection();
	void DrawCreateProjectSection();
	void TrySwitchProject(const std::filesystem::path& projectFileOrRoot);
	void TryCreateProject();

private:
	bool m_OpenRequested = false;
	bool m_IsOpen = false;
	int m_ActiveTab = 0;
	char m_ProjectNameBuffer[128] = {};
	char m_TargetFolderBuffer[512] = {};
	int m_SelectedTemplateIndex = 0;
	bool m_CreateCppModule = false;
	std::string m_ErrorMessage;
};

END
