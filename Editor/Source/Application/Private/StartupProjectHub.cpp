#include "StartupProjectHub.h"
#include "EditorSettingsService.h"
#include "ProjectSessionManager.h"
#include "FileDialogs.h"
#include "PlatformPath.h"
#include "ProjectDescriptor.h"
#include "Functions.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace StartupProjectHubDetail
{
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

	void ApplyHubDarkStyle()
	{
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 0.0f;
		style.FrameRounding = 4.0f;
		style.ChildRounding = 4.0f;
		style.PopupRounding = 4.0f;
		style.WindowPadding = ImVec2(16.0f, 14.0f);
		style.FramePadding = ImVec2(10.0f, 6.0f);
		style.ItemSpacing = ImVec2(10.0f, 8.0f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
		style.Colors[ImGuiCol_ChildBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
		style.Colors[ImGuiCol_Button] = ImVec4(0.22f, 0.24f, 0.28f, 1.0f);
		style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.29f, 0.33f, 0.40f, 1.0f);
		style.Colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.20f, 0.24f, 1.0f);
	}
}

StartupProjectHubResult StartupProjectHub::Run()
{
	StartupProjectHubResult result{};

	if (!SDL_WasInit(SDL_INIT_VIDEO))
	{
		if (!SDL_Init(SDL_INIT_VIDEO))
		{
			result.UseDefaultProject = true;
			return result;
		}
	}

	if (!EditorSettingsService::IsCreated())
		EditorSettingsService::Create();
	if (EditorSettingsService::IsCreated())
		EditorSettingsService::Get().LoadRecentProjects();

	SDL_Window* window = SDL_CreateWindow("BamEngine - Project Hub", 960, 620, SDL_WINDOW_RESIZABLE);
	if (!window)
	{
		result.UseDefaultProject = true;
		return result;
	}
	SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);

	SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
	if (!renderer)
	{
		SDL_DestroyWindow(window);
		result.UseDefaultProject = true;
		return result;
	}
	SDL_SetRenderVSync(renderer, 1);

	IMGUI_CHECKVERSION();
	ImGuiContext* prevCtx = ImGui::GetCurrentContext();
	ImGuiContext* hubCtx = ImGui::CreateContext();
	ImGui::SetCurrentContext(hubCtx);

	ImGuiIO& io = ImGui::GetIO();
	io.IniFilename = nullptr;
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

	// 한글 폰트 로드 (Windows 맑은 고딕 우선)
	const std::filesystem::path koreanFontPath = L"C:/Windows/Fonts/malgun.ttf";
	std::error_code fontEc;
	if (std::filesystem::exists(koreanFontPath, fontEc))
	{
		io.Fonts->AddFontFromFileTTF(Engine::Unicode::WideToUtf8(koreanFontPath.wstring()).c_str(), 16.0f, nullptr, io.Fonts->GetGlyphRangesKorean());
	}

	StartupProjectHubDetail::ApplyHubDarkStyle();
	ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
	ImGui_ImplSDLRenderer3_Init(renderer);

	int activeTab = 0;
	char projectNameBuffer[128] = {};
	char targetFolderBuffer[512] = {};
	int selectedTemplateIndex = 0;
	bool createCppModule = false;
	std::string errorMessage;

	const auto defaultParent = Engine::PlatformPath::GetProjectRoot().parent_path();
	if (!defaultParent.empty())
		StartupProjectHubDetail::CopyUtf8ToBuffer(Engine::Unicode::WideToUtf8(defaultParent.wstring()), targetFolderBuffer, sizeof(targetFolderBuffer));

	bool running = true;
	while (running)
	{
		SDL_Event event{};
		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);
			if (event.type == SDL_EVENT_QUIT ||
				(event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)))
			{
				result.Cancelled = true;
				running = false;
			}
		}

		if (!running)
			break;

		ImGui_ImplSDLRenderer3_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		int winW = 960;
		int winH = 620;
		SDL_GetWindowSize(window, &winW, &winH);

		ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(static_cast<float>(winW), static_cast<float>(winH)));
		constexpr ImGuiWindowFlags rootFlags =
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar;

		if (ImGui::Begin("StartupProjectHubRoot", nullptr, rootFlags))
		{
			ImGui::TextUnformatted("BamEngine Project Hub");
			ImGui::SameLine();
			ImGui::TextDisabled("  |  프로젝트를 선택하거나 새로 생성하세요");
			ImGui::Separator();

			const float errorReserve = errorMessage.empty() ? 0.0f : ImGui::GetTextLineHeightWithSpacing();
			const float footerReserve = 52.0f + errorReserve;

			if (ImGui::BeginChild("HubBody", ImVec2(0.0f, -footerReserve), ImGuiChildFlags_None))
			{
				if (ImGui::BeginTabBar("HubTabs"))
				{
					if (ImGui::BeginTabItem("최근 프로젝트"))
					{
						activeTab = 0;

						if (ImGui::Button("프로젝트 열기..."))
						{
							std::wstring selectedFile;
							const std::vector<FileDialogFilter> filters = { { L"BamEngine Project (*.bamproject)", L"*.bamproject" } };
							if (FileDialogs::OpenFileDialog(selectedFile, filters, L"", "Open Project") && !selectedFile.empty())
							{
								result.SelectedProjectFileOrRoot = std::filesystem::path(selectedFile);
								running = false;
							}
						}
						ImGui::SameLine();
						if (ImGui::Button("폴더 열기..."))
						{
							std::wstring selectedFolder;
							if (FileDialogs::OpenFolderDialog(selectedFolder, L"", "Open Project Folder") && !selectedFolder.empty())
							{
								result.SelectedProjectFileOrRoot = std::filesystem::path(selectedFolder);
								running = false;
							}
						}

						const std::vector<RecentProjectEntry> emptyEntries;
						const auto& recentProjects = EditorSettingsService::IsCreated() ? EditorSettingsService::Get().GetRecentProjects() : emptyEntries;

						std::vector<std::filesystem::path> missingTargets;
						for (const auto& entry : recentProjects)
						{
							const std::filesystem::path entryTarget = !entry.ProjectFilePath.empty()
								? std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectFilePath))
								: std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectRoot));
							std::error_code ec;
							if (!std::filesystem::exists(entryTarget, ec))
								missingTargets.push_back(entryTarget);
						}

						if (!missingTargets.empty() && EditorSettingsService::IsCreated())
						{
							ImGui::SameLine();
							if (ImGui::Button("없는 항목 정리"))
							{
								for (const auto& target : missingTargets)
									EditorSettingsService::Get().RemoveRecentProject(target);
							}
						}

						ImGui::Spacing();
						ImGui::BeginChild("StartupRecentList", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders);
						const auto& currentList = EditorSettingsService::IsCreated() ? EditorSettingsService::Get().GetRecentProjects() : emptyEntries;
						std::filesystem::path pendingRemove;

						if (currentList.empty())
						{
							ImGui::TextDisabled("최근 프로젝트가 없습니다.");
						}
						else
						{
							for (size_t i = 0; i < currentList.size(); ++i)
							{
								const auto& entry = currentList[i];
								const std::filesystem::path entryTarget = !entry.ProjectFilePath.empty()
									? std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectFilePath))
									: std::filesystem::path(Engine::Unicode::Utf8ToWide(entry.ProjectRoot));
								std::error_code existsEc;
								const bool exists = std::filesystem::exists(entryTarget, existsEc);

								ImGui::PushID(static_cast<int>(i));
								ImGui::BeginGroup();
								ImGui::TextUnformatted(entry.ProjectName.empty() ? entry.ProjectRoot.c_str() : entry.ProjectName.c_str());
								if (!exists)
								{
									ImGui::SameLine();
									ImGui::TextColored(ImVec4(0.95f, 0.40f, 0.35f, 1.0f), "[경로 없음]");
								}
								ImGui::TextDisabled("%s", !entry.ProjectFilePath.empty() ? entry.ProjectFilePath.c_str() : entry.ProjectRoot.c_str());
								ImGui::EndGroup();

								const ImGuiStyle& style = ImGui::GetStyle();
								const float openW = 76.0f;
								const float removeW = !exists ? 76.0f : 0.0f;
								const float totalW = openW + (!exists ? (removeW + style.ItemSpacing.x) : 0.0f);

								ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - totalW));
								if (!exists)
								{
									ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.65f, 0.22f, 0.22f, 1.0f));
									ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.78f, 0.28f, 0.28f, 1.0f));
									ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.18f, 0.18f, 1.0f));
									if (ImGui::Button("제거", ImVec2(removeW, 0.0f)))
										pendingRemove = entryTarget;
									ImGui::PopStyleColor(3);
									ImGui::SameLine();
								}

								ImGui::BeginDisabled(!exists);
								if (ImGui::Button("열기", ImVec2(openW, 0.0f)))
								{
									result.SelectedProjectFileOrRoot = entryTarget;
									running = false;
								}
								ImGui::EndDisabled();

								ImGui::Separator();
								ImGui::PopID();
							}
						}

						if (!pendingRemove.empty() && EditorSettingsService::IsCreated())
							EditorSettingsService::Get().RemoveRecentProject(pendingRemove);

						ImGui::EndChild();
						ImGui::EndTabItem();
					}

					if (ImGui::BeginTabItem("새 프로젝트 생성"))
					{
						activeTab = 1;
						ImGui::Spacing();
						ImGui::TextUnformatted("프로젝트 이름");
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::InputText("##StartupProjectName", projectNameBuffer, sizeof(projectNameBuffer));

						ImGui::Spacing();
						ImGui::TextUnformatted("대상 폴더");
						const float browseW = 96.0f;
						ImGui::SetNextItemWidth(std::max(120.0f, ImGui::GetContentRegionAvail().x - browseW - ImGui::GetStyle().ItemSpacing.x));
						ImGui::InputText("##StartupTargetFolder", targetFolderBuffer, sizeof(targetFolderBuffer));
						ImGui::SameLine();
						if (ImGui::Button("찾아보기...", ImVec2(browseW, 0.0f)))
						{
							std::wstring selectedFolder;
							if (FileDialogs::OpenFolderDialog(selectedFolder, Engine::Unicode::Utf8ToWide(targetFolderBuffer), "Target Folder") && !selectedFolder.empty())
								StartupProjectHubDetail::CopyUtf8ToBuffer(Engine::Unicode::WideToUtf8(selectedFolder), targetFolderBuffer, sizeof(targetFolderBuffer));
						}

						ImGui::Spacing();
						ImGui::TextUnformatted("템플릿");
						const char* templates[3] = { "빈 프로젝트", "3D 시작 프로젝트", "2D 시작 프로젝트" };
						ImGui::SetNextItemWidth(-FLT_MIN);
						ImGui::Combo("##StartupTemplate", &selectedTemplateIndex, templates, 3);

						ImGui::Spacing();
						ImGui::Checkbox("C++ 게임 모듈 생성", &createCppModule);

						ImGui::EndTabItem();
					}

					ImGui::EndTabBar();
				}
			}
			ImGui::EndChild();

			if (!errorMessage.empty())
				ImGui::TextColored(ImVec4(0.95f, 0.35f, 0.35f, 1.0f), "%s", errorMessage.c_str());

			ImGui::Separator();

			const ImGuiStyle& style = ImGui::GetStyle();
			const float exitW = 90.0f;
			const float primaryW = (activeTab == 1) ? 130.0f : 165.0f;
			const float footerW = exitW + primaryW + style.ItemSpacing.x;
			ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetContentRegionMax().x - footerW));

			if (ImGui::Button("종료", ImVec2(exitW, 0.0f)))
			{
				result.Cancelled = true;
				running = false;
			}
			ImGui::SameLine();

			if (activeTab == 1)
			{
				if (ImGui::Button("생성 및 열기", ImVec2(primaryW, 0.0f)))
				{
					errorMessage.clear();
					const std::string projectName = StartupProjectHubDetail::TrimCopy(projectNameBuffer);
					const std::string targetFolderUtf8 = StartupProjectHubDetail::TrimCopy(targetFolderBuffer);
					if (projectName.empty())
					{
						errorMessage = "유효한 프로젝트 이름을 입력하세요.";
					}
					else if (targetFolderUtf8.empty())
					{
						errorMessage = "유효한 대상 폴더를 선택하세요.";
					}
					else
					{
						const std::filesystem::path targetDir = std::filesystem::path(Engine::Unicode::Utf8ToWide(targetFolderUtf8)) / Engine::Unicode::Utf8ToWide(projectName);
						Engine::EProjectTemplate projectTemplate = Engine::EProjectTemplate::Empty;
						if (selectedTemplateIndex == 1) projectTemplate = Engine::EProjectTemplate::Default3D;
						else if (selectedTemplateIndex == 2) projectTemplate = Engine::EProjectTemplate::Default2D;

						if (!ProjectSessionManager::IsCreated())
							ProjectSessionManager::Create();

						const auto createResult = ProjectSessionManager::Get().CreateProject(
							targetDir, projectName, projectTemplate, false, {}, createCppModule);
						if (!createResult.Succeeded)
						{
							errorMessage = !createResult.ErrorMessage.empty() ? createResult.ErrorMessage : "프로젝트 생성에 실패했습니다.";
						}
						else
						{
							result.SelectedProjectFileOrRoot = createResult.ProjectFilePath;
							running = false;
						}
					}
				}
			}
			else
			{
				if (ImGui::Button("기본 프로젝트로 계속", ImVec2(primaryW, 0.0f)))
				{
					result.UseDefaultProject = true;
					running = false;
				}
			}
		}
		ImGui::End();

		ImGui::Render();
		SDL_SetRenderDrawColor(renderer, 28, 28, 31, 255);
		SDL_RenderClear(renderer);
		ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
		SDL_RenderPresent(renderer);
	}

	ImGui_ImplSDLRenderer3_Shutdown();
	ImGui_ImplSDL3_Shutdown();
	ImGui::DestroyContext(hubCtx);
	ImGui::SetCurrentContext(prevCtx);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	return result;
}
