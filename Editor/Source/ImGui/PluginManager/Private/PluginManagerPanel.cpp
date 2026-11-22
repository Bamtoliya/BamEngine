#include "PluginManagerPanel.h"
#include "EditorPanelPluginApi.h"
#include "PanelPluginGenerator.h"
#include "EditorUI.h"
#include "EditorSettings.h"
#include "ImGuiSplitter.h"
#include "ImGuiManager.h"
#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "ModuleBuildManager.h"
#include "ShaderHotReloadManager.h"
#include "AssetHotReloadManager.h"
#include "LocalizationManager.h"
#include "NotificationManager.h"
#include "IconsFontAwesome7.h"
#include "Logger.h"
#include "PlatformPath.h"
#include <imgui.h>
#include <algorithm>
#include <cctype>
#include <iterator>
#include <optional>
#include <sstream>
#include "PluginResourceEditor.h"
#if defined(_WIN32)
#include <Windows.h>
#include <shellapi.h>
#endif

namespace
{
	ModuleBuildManager* FindBuilderForPluginItem(const PluginModuleItem& item)
	{
		if (!HotReloadManager::IsCreated())
			return nullptr;

		auto& reloadManager = HotReloadManager::Get();
		if (item.IsGameModule)
			return &reloadManager.GetModuleBuilder();

		if (item.HostIndex)
		{
			const auto& entry = ImGuiManager::Get().GetPluginHost().GetEntry(*item.HostIndex);
			return entry.Development ? reloadManager.GetPluginBuilder(entry.Development->Target) : nullptr;
		}

		return item.IsResourcePlugin ? reloadManager.GetPluginBuilder(item.Id) : nullptr;
	}
}

BEGIN(Editor)

PluginManagerPanel::PluginManagerPanel()
{
	m_Name = L"PluginManager";
	m_DisplayNameKey = "PluginManager.Title";
	m_Open = false;
}

EResult PluginManagerPanel::OnInitialize(void* arg)
{
	InitModuleList();
	return EResult::Success;
}

void PluginManagerPanel::InitModuleList()
{
	m_Modules.clear();

	// 1. GameModule (C++ 게임플레이 모듈)
	PluginModuleItem gameModule;
	gameModule.Id = "Projects/GameModule";
	gameModule.DisplayName = "GameModule";
	gameModule.Icon = ICON_FA_GAMEPAD;
	gameModule.Category = EPluginCategory::GameLogic;
	gameModule.IsGameModule = true;
	m_Modules.push_back(gameModule);

	for (const auto* panel : ImGuiManager::Get().GetPluginResourceEditors())
	{
		PluginModuleItem item;
		item.Id = panel->GetTarget();
		item.DisplayName = item.Id;
		item.Icon = ICON_FA_PUZZLE_PIECE;
		item.Category = EPluginCategory::EditorTools;
		item.IsResourcePlugin = true;
		m_Modules.push_back(std::move(item));
	}

	const auto& host = ImGuiManager::Get().GetPluginHost();
	for (std::size_t index = 0; index < host.GetCount(); ++index)
	{
		const auto& entry = host.GetEntry(index);

		PluginModuleItem item;
		item.Id = entry.Id;
		item.DisplayName = entry.DisplayName;
		item.Version = entry.Version;
		item.Icon = ICON_FA_PUZZLE_PIECE;
		item.Category = EPluginCategory::EditorTools;
		item.HostIndex = index;
		m_Modules.push_back(std::move(item));
	}

	// 2. ShaderHotReload (셰이더 리로드 모듈)
	PluginModuleItem shaderModule;
	shaderModule.Id = "ShaderHotReload";
	shaderModule.DisplayName = "Shader Hot Reload";
	shaderModule.Icon = ICON_FA_FIRE;
	shaderModule.Category = EPluginCategory::EditorTools;
	shaderModule.IsShaderModule = true;
	m_Modules.push_back(shaderModule);

	// 3. AssetHotReload (에셋 리로드 모듈)
	PluginModuleItem assetModule;
	assetModule.Id = "AssetHotReload";
	assetModule.DisplayName = "Asset Hot Reload";
	assetModule.Icon = ICON_FA_FOLDER;
	assetModule.Category = EPluginCategory::EditorTools;
	assetModule.IsAssetModule = true;
	m_Modules.push_back(assetModule);
}

void PluginManagerPanel::RefreshPackages()
{
	const auto selected = m_SelectedIndex >= 0 && m_SelectedIndex < static_cast<int>(m_Modules.size())
		? std::optional<PluginModuleItem>(m_Modules[m_SelectedIndex]) : std::nullopt;
	ImGuiManager::Get().GetPluginHost().RefreshInstalledPackages();
	InitModuleList();
	m_SelectedIndex = -1;
	if (!selected)
		return;

	const auto it = std::find_if(m_Modules.begin(), m_Modules.end(), [&selected](const PluginModuleItem& item)
	{
		if (selected->HostIndex)
			return item.HostIndex == selected->HostIndex;
		return !item.HostIndex && item.Id == selected->Id &&
			item.IsGameModule == selected->IsGameModule && item.IsResourcePlugin == selected->IsResourcePlugin &&
			item.IsShaderModule == selected->IsShaderModule && item.IsAssetModule == selected->IsAssetModule;
	});
	if (it != m_Modules.end())
		m_SelectedIndex = static_cast<int>(std::distance(m_Modules.begin(), it));
}

EPluginStatus PluginManagerPanel::DetermineModuleStatus(const PluginModuleItem& item) const
{
	if (item.HostIndex)
	{
		const auto& host = ImGuiManager::Get().GetPluginHost();
		const auto& entry = host.GetEntry(*item.HostIndex);
		if (host.IsPluginSourceBuilding() && host.GetBuildingPluginId() == entry.Id)
			return EPluginStatus::Compiling;

		const auto* builder = FindBuilderForPluginItem(item);
		if (builder && builder->IsCompiling())
			return EPluginStatus::Compiling;
		if (builder && builder->GetStatus() == EModuleBuildStatus::Failed)
			return EPluginStatus::Failed;

		const auto status = host.GetStatus(*item.HostIndex);
		if (status.LastActionFailed)
			return EPluginStatus::Failed;
		return status.IsLoaded ? EPluginStatus::Loaded : EPluginStatus::Unloaded;
	}

	if (item.IsResourcePlugin)
	{
		const auto* builder = HotReloadManager::Get().GetPluginBuilder(item.Id);
		if (builder && builder->IsCompiling())
			return EPluginStatus::Compiling;
		if (builder && builder->GetStatus() == EModuleBuildStatus::Failed)
			return EPluginStatus::Failed;

		const auto* panel = ImGuiManager::Get().GetPluginResourceEditor(item.Id);
		if (panel && panel->ReloadFailed())
			return EPluginStatus::Failed;
		return panel && panel->IsLoaded() ? EPluginStatus::Loaded : EPluginStatus::Unloaded;
	}

	if (item.IsGameModule)
	{
		auto& buildMgr = HotReloadManager::Get().GetModuleBuilder();
		if (buildMgr.IsCompiling())
			return EPluginStatus::Compiling;
		if (buildMgr.GetStatus() == EModuleBuildStatus::Failed)
			return EPluginStatus::Failed;

		auto& reloadMgr = HotReloadManager::Get().GetModuleReloader();
		if (reloadMgr.GetState() == EHotReloadState::Failed)
			return EPluginStatus::Failed;
		if (reloadMgr.IsLoaded())
			return EPluginStatus::Loaded;

		return EPluginStatus::Unloaded;
	}

	return EPluginStatus::Unloaded;
}

void PluginManagerPanel::DrawStatusBadge(EPluginStatus status)
{
	ImVec4 bgCol, textCol;
	std::string text;

	switch (status)
	{
	case EPluginStatus::Loaded:
		bgCol = ImVec4(0.15f, 0.55f, 0.25f, 0.85f);
		textCol = ImVec4(0.85f, 1.0f, 0.85f, 1.0f);
		text = LOCAL("PluginManager.StatusLoaded");
		break;
	case EPluginStatus::Unloaded:
		bgCol = ImVec4(0.35f, 0.35f, 0.35f, 0.85f);
		textCol = ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
		text = LOCAL("PluginManager.StatusUnloaded");
		break;
	case EPluginStatus::Compiling:
		bgCol = ImVec4(0.75f, 0.60f, 0.10f, 0.85f);
		textCol = ImVec4(1.0f, 1.0f, 0.85f, 1.0f);
		text = LOCAL("PluginManager.StatusCompiling");
		break;
	case EPluginStatus::Failed:
		bgCol = ImVec4(0.75f, 0.20f, 0.20f, 0.85f);
		textCol = ImVec4(1.0f, 0.85f, 0.85f, 1.0f);
		text = LOCAL("PluginManager.StatusFailed");
		break;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
	const ImVec2 padding(6.0f, 2.0f);
	const ImVec2 badgeSize(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);

	drawList->AddRectFilled(pos, ImVec2(pos.x + badgeSize.x, pos.y + badgeSize.y),
		ImGui::ColorConvertFloat4ToU32(bgCol), 4.0f);

	ImGui::SetCursorScreenPos(ImVec2(pos.x + padding.x, pos.y + padding.y));
	ImGui::PushStyleColor(ImGuiCol_Text, textCol);
	ImGui::TextUnformatted(text.c_str());
	ImGui::PopStyleColor();

	ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + badgeSize.y + 2.0f));
}

void PluginManagerPanel::Draw()
{
	if (!m_Open)
		return;

	ImGui::SetNextWindowSize(ImVec2(880.0f, 580.0f), ImGuiCond_FirstUseEver);

	const std::string title = LOCAL("PluginManager.Title") + "###PluginManager";
	if (BeginPanelWindow(title.c_str(), m_Open, ImGuiWindowFlags_None))
	{
		auto& host = ImGuiManager::Get().GetPluginHost();
		EditorUI::ToolbarLayout toolbar;
		ImGui::BeginDisabled(host.GetApprovalPrompt() != nullptr);
		if (toolbar.Icon("RefreshPackages", ICON_FA_ARROWS_ROTATE, "PluginManager.RefreshPackages"))
			RefreshPackages();
		if (toolbar.Icon("CreatePanelPlugin", ICON_FA_PLUS, "PluginManager.NewPanelPlugin"))
		{
			m_CreateErrorKey = nullptr;
			m_PartialPluginDirectory.clear();
			ImGui::OpenPopup("###CreatePanelPlugin");
		}
		ImGui::EndDisabled();

		if (toolbar.Icon("PluginInfo", ICON_FA_CIRCLE_INFO, "PluginManager.PackageInformation"))
			ImGui::OpenPopup("##PluginInformation");
		if (ImGui::BeginPopup("##PluginInformation"))
		{
			ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * 30.0f);
			const auto pluginDirectory = host.GetProjectPluginDirectory();
			if (pluginDirectory.empty())
				ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.ProjectDirectoryUnavailable"));
			else
				ImGui::TextUnformatted(LOCAL_FMT("PluginManager.AddPackageHelp",
					WStrToStr(pluginDirectory.wstring())).c_str());
			if (*PluginSdkCompatibility)
				ImGui::TextUnformatted(LOCAL_FMT("PluginManager.SdkCompatibility", PluginSdkCompatibility).c_str());
			else
				ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.SdkUnavailable"));
			ImGui::PopTextWrapPos();
			ImGui::EndPopup();
		}
		ImGui::Separator();

		const float spacing = ImGui::GetStyle().ItemSpacing.x;
		const float paneWidth = ImGui::GetContentRegionAvail().x - Editor::SplitterThickness - spacing * 2.0f;
		if (paneWidth > 2.0f)
		{
			const float minWidth = std::min(ImGui::GetFontSize() * 9.0f, paneWidth * 0.5f);
			const float sidebarWidth = std::clamp(m_PreferredSidebarWidth, minWidth, paneWidth - minWidth);
			DrawMasterList(sidebarWidth);
			ImGui::SameLine();
			const float delta = Editor::SplitterBar("##PluginManagerSplitter", ImGuiAxis_X);
			if (delta != 0.0f)
				m_PreferredSidebarWidth = std::clamp(sidebarWidth + delta, minWidth, paneWidth - minWidth);
			ImGui::SameLine();
			DrawDetailView();
		}
		else
		{
			DrawDetailView();
		}
		DrawApprovalDialog();
		DrawCreatePluginDialog();
	}
	ImGui::End();
}

void PluginManagerPanel::DrawCreatePluginDialog()
{
	if (!ImGui::IsPopupOpen("###CreatePanelPlugin"))
		return;
	const auto title = LOCAL("PluginManager.CreatePanelTitle") + "###CreatePanelPlugin";
	ImGui::SetNextWindowSize(ImVec2(ImGui::GetFontSize() * 36.0f, 0.0f), ImGuiCond_FirstUseEver);
	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::TextWrapped("%s", LOCAL_CSTR("PluginManager.CreatePanelDescription"));
	if (EditorUI::BeginPropertyTable("CreatePanelFields"))
	{
		EditorUI::PropertyRow("PluginManager.NewIdentifier");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##Identifier", m_NewPluginId, sizeof(m_NewPluginId));
		EditorUI::PropertyRow("PluginManager.NewDisplayName");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##Name", m_NewPluginName, sizeof(m_NewPluginName));
		EditorUI::PropertyRow("PluginManager.NewVersion");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputText("##Version", m_NewPluginVersion, sizeof(m_NewPluginVersion));
		EditorUI::PropertyRow("PluginManager.NewWidth");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputInt("##Width", &m_NewPanelWidth);
		EditorUI::PropertyRow("PluginManager.NewHeight");
		ImGui::SetNextItemWidth(-1.0f);
		ImGui::InputInt("##Height", &m_NewPanelHeight);
		EditorUI::EndPropertyTable();
	}

	auto& host = ImGuiManager::Get().GetPluginHost();
	const auto pluginDirectory = host.GetProjectPluginDirectory();
	if (!pluginDirectory.empty())
	{
		const auto destination = WStrToStr(pluginDirectory.wstring()) + "\\UserPanel_" + m_NewPluginId;
		ImGui::TextWrapped("%s", LOCAL_FMT("PluginManager.CreatePanelDirectory", destination).c_str());
	}
	else
		ImGui::TextWrapped("%s", LOCAL_CSTR("PluginManager.ProjectDirectoryUnavailable"));
	if (m_CreateErrorKey)
		ImGui::TextWrapped("%s", LOCAL_CSTR(m_CreateErrorKey));
	if (!m_PartialPluginDirectory.empty())
		ImGui::TextWrapped("%s", LOCAL_FMT("PluginManager.CreatePanelPartial",
			WStrToStr(m_PartialPluginDirectory.wstring())).c_str());

	const EditorUI::DialogAction actions[] = {{"EditorCommon.Cancel"}, {"PluginManager.CreatePanelAction"}};
	const int action = EditorUI::DialogButtonBar("CreatePanelActions", actions, IM_ARRAYSIZE(actions));
	if (action == 0)
		ImGui::CloseCurrentPopup();
	else if (action == 1)
	{
		m_CreateErrorKey = nullptr;
		m_PartialPluginDirectory.clear();
		RefreshPackages();
		const std::string packageId = std::string("user.panel.") + m_NewPluginId;
		bool duplicate = false;
		for (std::size_t i = 0; i < host.GetCount(); ++i)
		{
			const auto& entry = host.GetEntry(i);
			if (entry.Id == packageId && entry.Status != EPluginCatalogStatus::MissingPackage)
				duplicate = true;
		}
		if (duplicate)
			m_CreateErrorKey = "PluginManager.CreateDuplicateId";
		else
		{
			const PanelPluginOptions options{m_NewPluginId, m_NewPluginName, m_NewPluginVersion,
				m_NewPanelWidth, m_NewPanelHeight};
			const auto root = pluginDirectory.empty() ? std::filesystem::path{} : pluginDirectory.parent_path();
			const auto result = CreatePanelPlugin(root, options, m_PartialPluginDirectory);
			switch (result)
			{
			case EPanelPluginCreateResult::Success:
			{
				const auto created = m_PartialPluginDirectory;
				m_PartialPluginDirectory.clear();
				RefreshPackages();
				for (std::size_t i = 0; i < m_Modules.size(); ++i)
				{
					if (m_Modules[i].HostIndex && host.GetEntry(*m_Modules[i].HostIndex).PackageDirectory == created)
					{
						m_SelectedIndex = static_cast<int>(i);
						break;
					}
				}
				ImGui::CloseCurrentPopup();
				break;
			}
			case EPanelPluginCreateResult::InvalidIdentifier: m_CreateErrorKey = "PluginManager.CreateInvalidId"; break;
			case EPanelPluginCreateResult::InvalidName: m_CreateErrorKey = "PluginManager.CreateInvalidName"; break;
			case EPanelPluginCreateResult::InvalidVersion: m_CreateErrorKey = "PluginManager.CreateInvalidVersion"; break;
			case EPanelPluginCreateResult::InvalidSize: m_CreateErrorKey = "PluginManager.CreateInvalidSize"; break;
			case EPanelPluginCreateResult::UnsupportedEnvironment: m_CreateErrorKey = "PluginManager.CreateUnsupported"; break;
			case EPanelPluginCreateResult::DirectoryExists: m_CreateErrorKey = "PluginManager.CreateDirectoryExists"; break;
			case EPanelPluginCreateResult::FileSystemError: m_CreateErrorKey = "PluginManager.CreateFileError"; break;
			}
		}
	}
	ImGui::EndPopup();
}

void PluginManagerPanel::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("PluginManager.SidebarWidth", m_PreferredSidebarWidth);
}

void PluginManagerPanel::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_PreferredSidebarWidth = workspace.GetSplitter("PluginManager.SidebarWidth", 270.0f);
}

void PluginManagerPanel::DrawApprovalDialog()
{
	auto& host = ImGuiManager::Get().GetPluginHost();
	const auto* prompt = host.GetApprovalPrompt();
	if (!prompt)
		return;

	const auto title = LOCAL("PluginManager.ApprovePluginTitle") + "###PluginApproval";
	if (!ImGui::IsPopupOpen(title.c_str()))
		ImGui::OpenPopup(title.c_str());
	if (!ImGui::BeginPopupModal(title.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
		return;

	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetFontSize() * 38.0f);
	ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.ApprovePluginWarning"));
	ImGui::TextUnformatted(prompt->DisplayName.c_str());
	ImGui::TextUnformatted(prompt->Approval.binary.c_str());
	ImGui::TextUnformatted(prompt->Approval.sha256.c_str());
	ImGui::PopTextWrapPos();

	const EditorUI::DialogAction actions[] = {{"EditorCommon.Cancel"}, {"PluginManager.ApproveAndLoad"}};
	const int action = EditorUI::DialogButtonBar("PluginApprovalActions", actions, IM_ARRAYSIZE(actions));
	if (action >= 0)
	{
		host.ResolveApproval(action == 1);
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

void PluginManagerPanel::DrawMasterList(float width)
{
	ImGui::BeginChild("##PluginMasterListChild", ImVec2(width, 0.0f), true);

	// 1. 검색창
	EditorUI::SearchBar("PluginSearch", m_SearchFilter, sizeof(m_SearchFilter), "PluginManager.SearchPlaceholder");

	// 2. 카테고리 필터
	const auto allCategory = LOCAL("PluginManager.CategoryAll");
	const auto gameCategory = LOCAL("PluginManager.CategoryGameLogic");
	const auto editorCategory = LOCAL("PluginManager.CategoryEditorTools");
	const char* categoryNames[] = { allCategory.c_str(), gameCategory.c_str(), editorCategory.c_str() };
	int currentCategory = static_cast<int>(m_FilterCategory);
	ImGui::PushItemWidth(-1.0f);
	if (ImGui::Combo("##PluginCategoryCombo", &currentCategory, categoryNames, IM_ARRAYSIZE(categoryNames)))
		m_FilterCategory = static_cast<EPluginCategory>(currentCategory);
	ImGui::PopItemWidth();

	ImGui::Separator();

	// 3. 모듈 목록 카드 렌더링
	std::string searchLower = m_SearchFilter;
	const auto toLower = [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); };
	std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), toLower);

	int visibleCount = 0;
	for (size_t i = 0; i < m_Modules.size(); ++i)
	{
		const auto& item = m_Modules[i];

		// 카테고리 필터
		if (m_FilterCategory != EPluginCategory::All && item.Category != m_FilterCategory)
			continue;

		// 검색어 필터
		if (!searchLower.empty())
		{
			const auto* panel = item.IsResourcePlugin ? ImGuiManager::Get().GetPluginResourceEditor(item.Id) : nullptr;
			std::string nameLower = panel ? panel->GetDisplayName() : item.DisplayName;
			std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), toLower);
			std::string idLower = item.Id;
			std::transform(idLower.begin(), idLower.end(), idLower.begin(), toLower);

			if (nameLower.find(searchLower) == std::string::npos &&
				idLower.find(searchLower) == std::string::npos)
				continue;
		}

		visibleCount++;

		const bool isSelected = (m_SelectedIndex == static_cast<int>(i));
		ImGui::PushID(static_cast<int>(i));

		const auto* panel = item.IsResourcePlugin ? ImGuiManager::Get().GetPluginResourceEditor(item.Id) : nullptr;
		const auto displayName = panel ? panel->GetDisplayName() : item.DisplayName;
		const EPluginStatus status = DetermineModuleStatus(item);
		const auto statusStr = (status == EPluginStatus::Loaded) ? LOCAL("PluginManager.StatusLoaded") :
			(status == EPluginStatus::Compiling) ? LOCAL("PluginManager.StatusCompiling") :
			(status == EPluginStatus::Failed) ? LOCAL("PluginManager.StatusFailed") : LOCAL("PluginManager.StatusUnloaded");
		const float padding = ImGui::GetStyle().FramePadding.x;
		const float lineHeight = ImGui::GetTextLineHeight();
		const float itemHeight = lineHeight * 2.0f + padding * 2.0f + 4.0f;
		const ImVec2 startPos = ImGui::GetCursorScreenPos();
		const float contentWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x);

		if (ImGui::Selectable("##ModuleCard", isSelected, ImGuiSelectableFlags_None, ImVec2(contentWidth, itemHeight)))
			m_SelectedIndex = static_cast<int>(i);

		const bool hovered = ImGui::IsItemHovered();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const char* icon = item.Icon ? item.Icon : ICON_FA_PUZZLE_PIECE;
		const float titleX = startPos.x + padding * 2.0f + ImGui::CalcTextSize(icon).x;
		const float right = startPos.x + contentWidth - padding;
		const float titleWidth = std::max(0.0f, right - titleX);
		drawList->PushClipRect(startPos, ImVec2(startPos.x + contentWidth, startPos.y + itemHeight), true);
		drawList->AddText(ImVec2(startPos.x + padding, startPos.y + padding), ImGui::GetColorU32(ImGuiCol_Text), icon);
		drawList->PushClipRect(ImVec2(titleX, startPos.y), ImVec2(std::max(titleX, right), startPos.y + itemHeight), true);
		drawList->AddText(ImVec2(titleX, startPos.y + padding), ImGui::GetColorU32(ImGuiCol_Text), displayName.c_str());
		drawList->PopClipRect();

		const bool isService = item.IsShaderModule || item.IsAssetModule;
		const float badgeWidth = ImGui::CalcTextSize(statusStr.c_str()).x + 12.0f;
		const bool showBadge = !isService && contentWidth >= badgeWidth + padding * 2.0f;
		if (showBadge)
		{
			ImGui::SetCursorScreenPos(ImVec2(right - badgeWidth, startPos.y + padding + lineHeight));
			DrawStatusBadge(status);
		}
		if (!item.Version.empty())
		{
			const auto version = LOCAL_FMT("PluginManager.VersionFormat", item.Version);
			const float versionRight = showBadge ? right - badgeWidth - padding : right;
			drawList->PushClipRect(ImVec2(startPos.x + padding, startPos.y),
				ImVec2(std::max(startPos.x + padding, versionRight), startPos.y + itemHeight), true);
			drawList->AddText(ImVec2(startPos.x + padding, startPos.y + padding + lineHeight + 2.0f),
				ImGui::GetColorU32(ImGuiCol_TextDisabled), version.c_str());
			drawList->PopClipRect();
		}
		drawList->PopClipRect();

		if (hovered && (ImGui::CalcTextSize(displayName.c_str()).x > titleWidth || (!isService && !showBadge)))
		{
			if (isService)
				ImGui::SetTooltip("%s", displayName.c_str());
			else
				ImGui::SetTooltip("%s\n%s", displayName.c_str(), statusStr.c_str());
		}

		ImGui::SetCursorScreenPos(ImVec2(startPos.x, startPos.y + itemHeight + 4.0f));
		ImGui::Dummy(ImVec2(0.0f, 0.0f));
		ImGui::PopID();
	}

	if (visibleCount == 0)
	{
		EditorUI::EmptyState("PluginManager.NoMatchingPlugins");
	}

	ImGui::EndChild();
}

void PluginManagerPanel::DrawDetailView()
{
	ImGui::BeginChild("##PluginDetailChild", ImVec2(0.0f, 0.0f), false);

	if (m_SelectedIndex < 0 || m_SelectedIndex >= static_cast<int>(m_Modules.size()))
	{
		// 빈 상태 표시 (Empty State)
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.SelectModuleHint"));
		ImGui::PopStyleColor();
		ImGui::EndChild();
		return;
	}

	const auto& item = m_Modules[m_SelectedIndex];

	// 1. 헤더 (아이콘 + 이름 + 버전 + 설명)
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
	const auto* panel = item.IsResourcePlugin ? ImGuiManager::Get().GetPluginResourceEditor(item.Id) : nullptr;
	const auto displayName = panel ? panel->GetDisplayName() : item.DisplayName;
	ImGui::TextWrapped("%s  %s", item.Icon ? item.Icon : ICON_FA_PUZZLE_PIECE, displayName.c_str());
	ImGui::PopFont();

	if (!item.Version.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
		ImGui::TextWrapped("%s", LOCAL_FMT("PluginManager.VersionFormat", item.Version).c_str());
		ImGui::PopStyleColor();
	}

	const auto description = panel ? panel->GetDescription() : std::string{};
	if (!description.empty())
	{
		ImGui::Spacing();
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
		ImGui::TextUnformatted(description.c_str());
		ImGui::PopTextWrapPos();
	}

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 2. 툴바
	DrawHeaderToolbar(item);

	ImGui::Spacing();
	ImGui::Separator();
	ImGui::Spacing();

	// 3. 메타데이터 속성표
	DrawMetadataTable(item);

	ImGui::Spacing();

	// 4. 제공 컴포넌트 / 에디터 패널 목록
	DrawProvidedFeatures(item);

	ImGui::Spacing();

	if (item.IsGameModule || item.IsResourcePlugin || item.HostIndex)
	{
		DrawBuildConsole(item);
	}

	ImGui::EndChild();
}

void PluginManagerPanel::DrawHeaderToolbar(const PluginModuleItem& item)
{
	EditorUI::ToolbarLayout toolbar;
	if (item.HostIndex)
	{
		auto& host = ImGuiManager::Get().GetPluginHost();
		const auto index = *item.HostIndex;
		const auto& entry = host.GetEntry(index);
		const auto status = host.GetStatus(index);
		const bool hasSource = !entry.PackageDirectory.empty() &&
			std::filesystem::exists(entry.PackageDirectory / "CMakeLists.txt");
		const bool isBuildingThis = host.IsPluginSourceBuilding() && host.GetBuildingPluginId() == entry.Id;
		const bool isAnySourceBuilding = host.IsPluginSourceBuilding();

#if defined(_WIN32)
		if (!entry.PackageDirectory.empty() && toolbar.Icon("OpenPackageDirectory", ICON_FA_FOLDER_OPEN, "EditorCommon.ShowInExplorer"))
		{
			const auto result = ShellExecuteW(nullptr, L"open", entry.PackageDirectory.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			if (reinterpret_cast<INT_PTR>(result) <= 32)
			{
				NotificationManager::Get().AddNotification(LOCAL("EditorCommon.ShowInExplorer"),
					LOCAL_FMT("PluginManager.OpenFolderFailed", WStrToStr(entry.PackageDirectory.wstring())), ELogLevel::Warn);
			}
		}
#endif
		if (!entry.Development && entry.Status != EPluginCatalogStatus::Ready && entry.Status != EPluginCatalogStatus::NeedsApproval)
		{
			if (status.IsLoaded && toolbar.Icon("Unload", ICON_FA_EJECT, "PluginManager.Unload"))
				host.Request(index, EPluginHostAction::Unload);

			if (hasSource)
			{
				ImGui::BeginDisabled(isAnySourceBuilding);
				const char* buildLabel = status.IsLoaded ? "PluginManager.RebuildAndReload" : "PluginManager.Build";
				if (toolbar.Icon("SourceBuild", ICON_FA_HAMMER, buildLabel))
					host.BuildAndReloadPluginSource(entry.Id);
				ImGui::EndDisabled();

				if (isBuildingThis)
					ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.StatusCompiling"));
				else if (status.LastActionFailed)
					ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.BuildFailed"));
				else
					ImGui::TextWrapped("%s", LOCAL_CSTR("PluginManager.PackageMissingBinary"));
				return;
			}

			// Content-only plugin without binary
			const bool hasContent = !entry.PackageDirectory.empty() &&
				(std::filesystem::exists(entry.PackageDirectory / "Content") ||
				 std::filesystem::exists(entry.PackageDirectory / "plugin.json"));
			if (hasContent && entry.BinaryPath.empty())
			{
				if (!status.IsLoaded && toolbar.Icon("Load", ICON_FA_PLAY, "PluginManager.Load"))
					host.Request(index, EPluginHostAction::Load);
				return;
			}

			const char* messageKey = "PluginManager.PackageUnavailable";
			switch (entry.Status)
			{
			case EPluginCatalogStatus::Ready:
			case EPluginCatalogStatus::NeedsApproval:
				break;
			case EPluginCatalogStatus::InvalidManifest:
				messageKey = "PluginManager.PackageInvalidManifest";
				break;
			case EPluginCatalogStatus::IncompatibleSdk:
				messageKey = "PluginManager.PackageIncompatibleSdk";
				break;
			case EPluginCatalogStatus::UnsupportedEditorModules:
				messageKey = "PluginManager.PackageUnsupportedModules";
				break;
			case EPluginCatalogStatus::MissingBinary:
				messageKey = "PluginManager.PackageMissingBinary";
				break;
			case EPluginCatalogStatus::MissingPackage:
				messageKey = "PluginManager.PackageMissing";
				break;
			case EPluginCatalogStatus::UnsafeBinaryPath:
				messageKey = "PluginManager.PackageUnsafePath";
				break;
			case EPluginCatalogStatus::DuplicatePackageId:
				messageKey = "PluginManager.PackageDuplicateId";
				break;
			}

			ImGui::TextWrapped("%s", LOCAL_CSTR(messageKey));
			return;
		}
		auto* builder = FindBuilderForPluginItem(item);
		const bool compiling = (builder && builder->IsCompiling()) || isBuildingThis;

		ImGui::BeginDisabled(compiling || isAnySourceBuilding);
		if (status.IsLoaded)
		{
			if (toolbar.Icon("Reload", ICON_FA_ARROWS_ROTATE, "PluginManager.Reload"))
				host.Request(index, EPluginHostAction::Reload);
		}
		else if (toolbar.Icon("Load", ICON_FA_PLAY, "PluginManager.Load"))
		{
			host.Request(index, EPluginHostAction::Load);
		}

		if (builder)
		{
			const char* label = status.IsLoaded ? "PluginManager.RebuildAndReload" : "PluginManager.Build";
			if (toolbar.Icon("Build", ICON_FA_HAMMER, label))
				builder->RequestBuild();
		}
		else if (hasSource)
		{
			const char* label = status.IsLoaded ? "PluginManager.RebuildAndReload" : "PluginManager.Build";
			if (toolbar.Icon("SourceBuild", ICON_FA_HAMMER, label))
				host.BuildAndReloadPluginSource(entry.Id);
		}
		ImGui::EndDisabled();

		if (status.IsLoaded && toolbar.Icon("Unload", ICON_FA_EJECT, "PluginManager.Unload"))
			host.Request(index, EPluginHostAction::Unload);

		if (builder)
		{
			bool autoCompile = builder->IsAutoCompileEnabled();
			if (ImGui::Checkbox(LOCAL_LABEL("PluginManager.AutoCompileOnSave").c_str(), &autoCompile))
				builder->SetAutoCompileEnabled(autoCompile);

			const auto buildStatus = builder->GetStatus();
			if (buildStatus == EModuleBuildStatus::Compiling)
				ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.StatusCompiling"));
			else if (buildStatus == EModuleBuildStatus::Failed)
				ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.BuildFailed"));
			else if (buildStatus == EModuleBuildStatus::Succeeded)
				ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.BuildSucceeded"));
		}
		else if (hasSource)
		{
			if (isBuildingThis)
				ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.StatusCompiling"));
			else if (status.LastActionFailed)
				ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.BuildFailed"));
		}

		if (status.LastActionFailed && !builder && !hasSource)
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.LastPluginOperationFailed"));

		return;
	}
	if (item.IsResourcePlugin)
	{
		auto* builder = HotReloadManager::Get().GetPluginBuilder(item.Id);
		auto* panel = ImGuiManager::Get().GetPluginResourceEditor(item.Id);
		const bool compiling = builder && builder->IsCompiling();

		ImGui::BeginDisabled(compiling || !panel);
		if (toolbar.Icon("ResourcePluginReload", ICON_FA_ARROWS_ROTATE, "PluginManager.Reload"))
			panel->RequestReload();
		ImGui::EndDisabled();

		if (builder)
		{
			ImGui::BeginDisabled(compiling);
			if (toolbar.Icon("ResourcePluginBuild", ICON_FA_HAMMER, "PluginManager.RebuildAndReload"))
				builder->RequestBuild();
			ImGui::EndDisabled();

			bool autoCompile = builder->IsAutoCompileEnabled();
			if (ImGui::Checkbox(LOCAL_LABEL("PluginManager.AutoCompileOnSave").c_str(), &autoCompile))
				builder->SetAutoCompileEnabled(autoCompile);
		}
		return;
	}

	if (item.IsGameModule)
	{
		auto& reloadMgr = HotReloadManager::Get().GetModuleReloader();
		auto& buildMgr = HotReloadManager::Get().GetModuleBuilder();

		const bool compiling = buildMgr.IsCompiling();

		// [Reload]
		if (compiling) ImGui::BeginDisabled();
		if (toolbar.Icon("PluginManager.Reload", ICON_FA_ARROWS_ROTATE, "PluginManager.Reload"))
		{
			reloadMgr.ReloadModule();
		}
		if (compiling) ImGui::EndDisabled();

		// [Rebuild & Reload]
		if (compiling) ImGui::BeginDisabled();
		if (toolbar.Icon("PluginManager.RebuildAndReload", ICON_FA_HAMMER, "PluginManager.RebuildAndReload"))
		{
			buildMgr.RequestBuild();
		}
		if (compiling) ImGui::EndDisabled();

		// [Load / Unload]
		const bool isLoaded = reloadMgr.IsLoaded();
		if (toolbar.Icon("ModuleLoad", isLoaded ? ICON_FA_EJECT : ICON_FA_PLAY,
			isLoaded ? "PluginManager.Unload" : "PluginManager.Load"))
		{
			if (isLoaded)
			{
				reloadMgr.UnloadModule();
			}
			else
			{
				reloadMgr.LoadModule(reloadMgr.GetOriginalDllPath());
			}
		}

		// [Auto-Reload on Save]
		bool autoReload = reloadMgr.IsAutoReloadEnabled();
		if (ImGui::Checkbox(LOCAL_CSTR("PluginManager.AutoReloadOnSave"), &autoReload))
			reloadMgr.SetAutoReload(autoReload);

		// [Auto-Compile]
		bool autoCompile = buildMgr.IsAutoCompileEnabled();
		if (ImGui::Checkbox(LOCAL_CSTR("PluginManager.AutoCompileOnSave"), &autoCompile))
			buildMgr.SetAutoCompileEnabled(autoCompile);
	}
	else if (item.IsShaderModule)
	{
		auto& shaderReloader = HotReloadManager::Get().GetShaderReloader();
		if (toolbar.Icon("PluginManager.RecompileAllShaders", ICON_FA_FIRE, "PluginManager.RecompileAllShaders"))
		{
			shaderReloader.RequestRecompileAll();
		}

		bool autoReload = shaderReloader.IsAutoReloadEnabled();
		if (ImGui::Checkbox(LOCAL_CSTR("PluginManager.AutoReloadShaders"), &autoReload))
			shaderReloader.SetAutoReload(autoReload);
	}
	else if (item.IsAssetModule)
	{
		auto& assetReloader = HotReloadManager::Get().GetAssetReloader();
		bool autoReload = assetReloader.IsAutoReloadEnabled();
		if (ImGui::Checkbox(LOCAL_CSTR("PluginManager.AutoReloadAssets"), &autoReload))
			assetReloader.SetAutoReload(autoReload);
	}
}

void PluginManagerPanel::DrawMetadataTable(const PluginModuleItem& item)
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("PluginManager.MetadataHeader"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (EditorUI::BeginSettingsTable("##PluginMetadataTable", 2, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_RowBg))
	{
		ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Property"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 10.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("EditorCommon.Value"), ImGuiTableColumnFlags_WidthStretch);

		// 1. 식별자
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropIdentifier"));
		ImGui::TableSetColumnIndex(1);
		ImGui::TextUnformatted(item.Id.c_str());
		if (item.HostIndex)
		{
			const auto& entry = ImGuiManager::Get().GetPluginHost().GetEntry(*item.HostIndex);
			const char* originKey = "PluginManager.OriginDevelopment";
			if (entry.Origin == EPluginOrigin::Builtin)
				originKey = "PluginManager.OriginBuiltin";
			else if (entry.Origin == EPluginOrigin::Custom)
				originKey = "PluginManager.OriginCustom";

			EditorUI::PropertyRow("PluginManager.PropOrigin");
			ImGui::TextUnformatted(LOCAL_CSTR(originKey));
			if (!entry.PackageDirectory.empty())
			{
				EditorUI::PropertyRow("PluginManager.PropPackageDirectory");
				const auto packageDirectory = WStrToStr(entry.PackageDirectory.wstring());
				ImGui::TextWrapped("%s", packageDirectory.c_str());
			}
			if (entry.Development)
			{
				EditorUI::PropertyRow("PluginManager.PropSourceDir");
				const auto source = WStrToStr(entry.Development->SourceDirectory.wstring());
				ImGui::TextUnformatted(source.c_str());
			}

			EditorUI::PropertyRow("PluginManager.PropOriginalDll");
			const auto binary = WStrToStr(entry.BinaryPath.wstring());
			ImGui::TextUnformatted(binary.empty() ? "-" : binary.c_str());

			if (Engine::PlatformPath::HasPluginMount(entry.Id))
			{
				EditorUI::PropertyRow("EditorCommon.CopyUri");
				const std::string uri = "Plugin://" + entry.Id + "/";
				ImGui::TextUnformatted(uri.c_str());
			}
		}
		else if (item.IsGameModule)
		{
			auto& reloadMgr = HotReloadManager::Get().GetModuleReloader();
			auto& buildMgr = HotReloadManager::Get().GetModuleBuilder();

			// 2. 원본 DLL 경로
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropOriginalDll"));
			ImGui::TableSetColumnIndex(1);
			const std::string origDll = reloadMgr.GetOriginalDllPath().string();
			ImGui::TextUnformatted(origDll.empty() ? "-" : origDll.c_str());

			// 3. 섀도우 복사본 경로
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropShadowDll"));
			ImGui::TableSetColumnIndex(1);
			const std::string shadowDll = reloadMgr.GetCurrentShadowDllPath().string();
			ImGui::TextUnformatted(shadowDll.empty() ? "-" : shadowDll.c_str());

			// 4. 소스 디렉터리
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropSourceDir"));
			ImGui::TableSetColumnIndex(1);
			const std::string srcDir = buildMgr.GetSourceDirectory().string();
			ImGui::TextUnformatted(srcDir.empty() ? "-" : srcDir.c_str());

			// 5. 리로드 인덱스
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropReloadCount"));
			ImGui::TableSetColumnIndex(1);
			ImGui::Text("%u", reloadMgr.GetReloadIndex());

			// 6. 빌드 상태
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropBuildStatus"));
			ImGui::TableSetColumnIndex(1);
			const char* statusStr = buildMgr.IsCompiling() ? LOCAL_CSTR("PluginManager.StatusCompiling") :
				(buildMgr.GetStatus() == EModuleBuildStatus::Succeeded ? "Succeeded" :
				(buildMgr.GetStatus() == EModuleBuildStatus::Failed ? "Failed" : "Idle"));
			ImGui::TextUnformatted(statusStr);
		}
		else if (item.IsResourcePlugin)
		{
			if (const auto* builder = HotReloadManager::Get().GetPluginBuilder(item.Id))
			{
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropSourceDir"));
				ImGui::TableSetColumnIndex(1);
				const auto path = WStrToStr(builder->GetSourceDirectory().wstring());
				ImGui::TextUnformatted(path.c_str());
			}
		}
		else if (item.IsShaderModule)
		{
			auto& shaderMgr = HotReloadManager::Get().GetShaderReloader();
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropWatchPaths"));
			ImGui::TableSetColumnIndex(1);
			const auto& paths = shaderMgr.GetWatchPaths();
			if (paths.empty())
				ImGui::TextUnformatted("-");
			else
			{
				for (const auto& p : paths)
					ImGui::TextUnformatted(p.string().c_str());
			}
		}
		EditorUI::EndSettingsTable();
	}
}

void PluginManagerPanel::DrawProvidedFeatures(const PluginModuleItem& item)
{
	if (!item.HostIndex && !item.IsResourcePlugin)
		return;

	if (!ImGui::CollapsingHeader(LOCAL_CSTR("PluginManager.FeaturesHeader"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (item.HostIndex)
	{
		auto& gui = ImGuiManager::Get();
		auto& host = gui.GetPluginHost();
		const auto index = *item.HostIndex;
		const auto panels = host.GetPanels(index);

		if (panels.empty())
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.NoRegisteredPanels"));

		for (const auto& panel : panels)
		{
			const auto label = LOCAL(panel.TitleKey) + "###" + WStrToStr(panel.Id);
			if (ImGui::Selectable(label.c_str()))
				host.FocusPanel(index, panel.Id, gui);
		}
	}
	else if (item.IsResourcePlugin)
	{
		if (const auto* panel = ImGuiManager::Get().GetPluginResourceEditor(item.Id))
			ImGui::TextUnformatted(panel->GetDisplayName().c_str());
	}
}

void PluginManagerPanel::DrawBuildConsole(const PluginModuleItem& item)
{
	auto* builder = FindBuilderForPluginItem(item);
	auto& host = ImGuiManager::Get().GetPluginHost();
	const bool isHostPlugin = item.HostIndex.has_value();
	const bool hasSource = isHostPlugin &&
		!host.GetEntry(*item.HostIndex).PackageDirectory.empty() &&
		std::filesystem::exists(host.GetEntry(*item.HostIndex).PackageDirectory / "CMakeLists.txt");
	const bool hasHostLogs = isHostPlugin && !host.GetPluginBuildLogs().empty();

	if (!builder && !hasSource && !hasHostLogs)
		return;

	if (!ImGui::CollapsingHeader(LOCAL_CSTR("PluginManager.BuildConsoleHeader"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	EditorUI::ToolbarLayout toolbar;
	std::string outputText;
	bool isCompiling = false;
	bool isFailed = false;

	if (builder)
	{
		outputText = builder->GetLastBuildOutput();
		isCompiling = builder->IsCompiling();
		isFailed = (builder->GetStatus() == EModuleBuildStatus::Failed);
	}
	else if (isHostPlugin)
	{
		outputText = host.GetPluginBuildLogs();
		isCompiling = host.IsPluginSourceBuilding() && (host.GetBuildingPluginId() == host.GetEntry(*item.HostIndex).Id);
		isFailed = host.GetStatus(*item.HostIndex).LastActionFailed;
	}

	// 상단 제어 바: [Copy Log], [Retry Compile] (실패 시), [Auto-scroll]
	if (toolbar.Icon("EditorCommon.Copy", ICON_FA_COPY, "EditorCommon.Copy"))
	{
		ImGui::SetClipboardText(outputText.c_str());
	}

	if (isFailed && !isCompiling)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.20f, 0.9f));
		if (toolbar.Icon("PluginManager.RetryCompile", ICON_FA_HAMMER, "PluginManager.RetryCompile"))
		{
			if (builder)
				builder->RequestBuild();
			else if (isHostPlugin)
				host.BuildAndReloadPluginSource(host.GetEntry(*item.HostIndex).Id);
		}
		ImGui::PopStyleColor();
	}

	ImGui::Checkbox(LOCAL_CSTR("PluginManager.AutoScroll"), &m_AutoScrollLog);

	ImGui::Spacing();

	// 콘솔 텍스트 차일드 윈도우
	ImGui::BeginChild("##BuildOutputLog", ImVec2(0.0f, 180.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

	if (outputText.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.NoBuildOutput"));
		ImGui::PopStyleColor();
	}
	else
	{
		std::istringstream stream(outputText);
		std::string line;
		while (std::getline(stream, line))
		{
			// 에러 라인 감지
			if (line.find("error") != std::string::npos || line.find("FAILED") != std::string::npos || line.find("Error") != std::string::npos)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
				ImGui::TextUnformatted(line.c_str());
				ImGui::PopStyleColor();
			}
			// 경고 라인 감지
			else if (line.find("warning") != std::string::npos || line.find("Warning") != std::string::npos)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.3f, 1.0f));
				ImGui::TextUnformatted(line.c_str());
				ImGui::PopStyleColor();
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
				ImGui::TextUnformatted(line.c_str());
				ImGui::PopStyleColor();
			}
		}

		if (m_AutoScrollLog && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
			ImGui::SetScrollHereY(1.0f);
	}

	ImGui::EndChild();
}

END
