#include "PluginManagerPanel.h"
#include "EditorUI.h"
#include "ImGuiManager.h"
#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include "ModuleBuildManager.h"
#include "ShaderHotReloadManager.h"
#include "AssetHotReloadManager.h"
#include "LocalizationManager.h"
#include "IconsFontAwesome7.h"
#include "ReflectionRegistry.h"
#include "Logger.h"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include "PluginResourceEditor.h"

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
	gameModule.Version = "1.0.0";
	gameModule.Author = "BamEngine Project";
	gameModule.Description = "Core C++ gameplay dynamic module. Provides hot-reloadable gameplay systems, components, and game logic.";
	gameModule.Icon = ICON_FA_GAMEPAD;
	gameModule.Category = EPluginCategory::GameLogic;
	gameModule.IsGameModule = true;
	m_Modules.push_back(gameModule);

	for (const auto* panel : ImGuiManager::Get().GetPluginResourceEditors())
	{
		PluginModuleItem item;
		item.Id = panel->GetTarget();
		item.DisplayName = item.Id;
		item.Version = "1";
		item.Author = "BamEngine";
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
	shaderModule.Version = "1.0.0";
	shaderModule.Author = "BamEngine Render Team";
	shaderModule.Description = "Runtime HLSL shader file watcher and DXIL recompilation pipeline.";
	shaderModule.Icon = ICON_FA_FIRE;
	shaderModule.Category = EPluginCategory::EditorTools;
	shaderModule.IsShaderModule = true;
	m_Modules.push_back(shaderModule);

	// 3. AssetHotReload (에셋 리로드 모듈)
	PluginModuleItem assetModule;
	assetModule.Id = "AssetHotReload";
	assetModule.DisplayName = "Asset Hot Reload";
	assetModule.Version = "1.0.0";
	assetModule.Author = "BamEngine Core Team";
	assetModule.Description = "Monitors texture, mesh, material, and localization assets for live hot-reloading.";
	assetModule.Icon = ICON_FA_FOLDER;
	assetModule.Category = EPluginCategory::EditorTools;
	assetModule.IsAssetModule = true;
	m_Modules.push_back(assetModule);

	// 4. TilemapEditor (2D 타일맵 & 팔레트 플러그인)
	PluginModuleItem tilemap;
	tilemap.Id = "TilemapEditor";
	tilemap.DisplayName = "2D Tilemap Editor";
	tilemap.Version = "1.0.0";
	tilemap.Author = "BamEngine 2D Tools";
	tilemap.Description = "Interactive 2D tilemap painting, tile palette grid, and multi-chunk runtime rendering.";
	tilemap.Icon = ICON_FA_BORDER_ALL;
	tilemap.Category = EPluginCategory::EditorTools;
	tilemap.AssociatedPanelName = L"TilePalette";
	m_Modules.push_back(tilemap);

	// 5. SpriteEditor (2D 스프라이트 & 아틀라스 플러그인)
	PluginModuleItem spriteEditor;
	spriteEditor.Id = "SpriteEditor";
	spriteEditor.DisplayName = "Sprite & Atlas Editor";
	spriteEditor.Version = "1.0.0";
	spriteEditor.Author = "BamEngine 2D Tools";
	spriteEditor.Description = "2D sprite slicing, 9-slice borders, and runtime sprite atlas generator.";
	spriteEditor.Icon = ICON_FA_IMAGE;
	spriteEditor.Category = EPluginCategory::EditorTools;
	spriteEditor.AssociatedPanelName = L"SpriteEditor";
	m_Modules.push_back(spriteEditor);

	// 6. AnimatorGraph (3D 애니메이터 그래프 플러그인)
	PluginModuleItem animatorGraph;
	animatorGraph.Id = "AnimatorGraph";
	animatorGraph.DisplayName = "Animator Graph Studio";
	animatorGraph.Version = "1.0.0";
	animatorGraph.Author = "BamEngine Animation Team";
	animatorGraph.Description = "State machine canvas, blend tree evaluation, and transition rule graph editor.";
	animatorGraph.Icon = ICON_FA_DIAGRAM_PROJECT;
	animatorGraph.Category = EPluginCategory::EditorTools;
	animatorGraph.AssociatedPanelName = L"AnimatorGraph";
	m_Modules.push_back(animatorGraph);

	// 7. PrefabEditor (프리팹 격리 편집기)
	PluginModuleItem prefabEditor;
	prefabEditor.Id = "PrefabEditor";
	prefabEditor.DisplayName = "Prefab Isolation Editor";
	prefabEditor.Version = "1.0.0";
	prefabEditor.Author = "BamEngine Core Team";
	prefabEditor.Description = "Isolated prefab editing stage with single-source-of-truth component override pipeline.";
	prefabEditor.Icon = ICON_FA_CUBES;
	prefabEditor.Category = EPluginCategory::EditorTools;
	prefabEditor.AssociatedPanelName = L"PrefabEditor";
	m_Modules.push_back(prefabEditor);

	// 8. TerrainEditor (지형 편집기)
	PluginModuleItem terrainEditor;
	terrainEditor.Id = "TerrainEditor";
	terrainEditor.DisplayName = "Terrain Sculptor";
	terrainEditor.Version = "1.0.0";
	terrainEditor.Author = "BamEngine Environment Tools";
	terrainEditor.Description = "Heightmap sculpt, smooth, flatten, and multi-layer splat texture painting.";
	terrainEditor.Icon = ICON_FA_MOUNTAIN;
	terrainEditor.Category = EPluginCategory::Experimental;
	terrainEditor.AssociatedPanelName = L"TerrainEditor";
	m_Modules.push_back(terrainEditor);
}

EPluginStatus PluginManagerPanel::DetermineModuleStatus(const PluginModuleItem& item) const
{
	if (item.IsResourcePlugin)
	{
		if (item.HostIndex)
		{
			const auto status = ImGuiManager::Get().GetPluginHost().GetStatus(*item.HostIndex);
			return status.IsLoaded ? EPluginStatus::Loaded : EPluginStatus::Unloaded;
		}

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

		auto& reloadMgr = HotReloadManager::Get().GetModuleReloader();
		if (reloadMgr.GetState() == EHotReloadState::Failed)
			return EPluginStatus::Failed;
		if (reloadMgr.IsLoaded())
			return EPluginStatus::Loaded;

		return EPluginStatus::Unloaded;
	}

	if (item.IsShaderModule || item.IsAssetModule)
		return EPluginStatus::Loaded;

	if (!item.AssociatedPanelName.empty())
	{
		if (auto* panel = ImGuiManager::Get().GetImGuiPanel(item.AssociatedPanelName))
			return panel->IsOpen() ? EPluginStatus::Loaded : EPluginStatus::Unloaded;
	}

	return EPluginStatus::Loaded;
}

void PluginManagerPanel::DrawStatusBadge(EPluginStatus status)
{
	ImVec4 bgCol, textCol;
	const char* text = "";

	switch (status)
	{
	case EPluginStatus::Loaded:
		bgCol = ImVec4(0.15f, 0.55f, 0.25f, 0.85f);
		textCol = ImVec4(0.85f, 1.0f, 0.85f, 1.0f);
		text = LOCAL_CSTR("PluginManager.StatusLoaded");
		break;
	case EPluginStatus::Unloaded:
		bgCol = ImVec4(0.35f, 0.35f, 0.35f, 0.85f);
		textCol = ImVec4(0.80f, 0.80f, 0.80f, 1.0f);
		text = LOCAL_CSTR("PluginManager.StatusUnloaded");
		break;
	case EPluginStatus::Compiling:
		bgCol = ImVec4(0.75f, 0.60f, 0.10f, 0.85f);
		textCol = ImVec4(1.0f, 1.0f, 0.85f, 1.0f);
		text = LOCAL_CSTR("PluginManager.StatusCompiling");
		break;
	case EPluginStatus::Failed:
		bgCol = ImVec4(0.75f, 0.20f, 0.20f, 0.85f);
		textCol = ImVec4(1.0f, 0.85f, 0.85f, 1.0f);
		text = LOCAL_CSTR("PluginManager.StatusFailed");
		break;
	}

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const ImVec2 textSize = ImGui::CalcTextSize(text);
	const ImVec2 padding(6.0f, 2.0f);
	const ImVec2 badgeSize(textSize.x + padding.x * 2.0f, textSize.y + padding.y * 2.0f);

	drawList->AddRectFilled(pos, ImVec2(pos.x + badgeSize.x, pos.y + badgeSize.y),
		ImGui::ColorConvertFloat4ToU32(bgCol), 4.0f);

	ImGui::SetCursorScreenPos(ImVec2(pos.x + padding.x, pos.y + padding.y));
	ImGui::PushStyleColor(ImGuiCol_Text, textCol);
	ImGui::TextUnformatted(text);
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
		DrawMasterList();
		ImGui::SameLine();
		DrawDetailView();
		DrawApprovalDialog();
	}
	ImGui::End();
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

void PluginManagerPanel::DrawMasterList()
{
	ImGui::BeginChild("##PluginMasterListChild", ImVec2(270.0f, 0.0f), true);

	// 1. 검색창
	EditorUI::SearchBar("PluginSearch", m_SearchFilter, sizeof(m_SearchFilter), "PluginManager.SearchPlaceholder");

	// 2. 카테고리 필터
	const char* categoryNames[] = {
		LOCAL_CSTR("PluginManager.CategoryAll"),
		LOCAL_CSTR("PluginManager.CategoryGameLogic"),
		LOCAL_CSTR("PluginManager.CategoryEditorTools"),
		LOCAL_CSTR("PluginManager.CategoryExperimental")
	};
	int currentCategory = static_cast<int>(m_FilterCategory);
	ImGui::PushItemWidth(-1.0f);
	if (ImGui::Combo("##PluginCategoryCombo", &currentCategory, categoryNames, IM_ARRAYSIZE(categoryNames)))
		m_FilterCategory = static_cast<EPluginCategory>(currentCategory);
	ImGui::PopItemWidth();

	ImGui::Separator();

	// 3. 모듈 목록 카드 렌더링
	std::string searchLower = m_SearchFilter;
	std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

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
			std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
			std::string idLower = item.Id;
			std::transform(idLower.begin(), idLower.end(), idLower.begin(), ::tolower);

			if (nameLower.find(searchLower) == std::string::npos &&
				idLower.find(searchLower) == std::string::npos)
				continue;
		}

		visibleCount++;

		const bool isSelected = (m_SelectedIndex == static_cast<int>(i));
		ImGui::PushID(static_cast<int>(i));

		// 카드 항목 렌더링
		const float itemHeight = 44.0f;
		const ImVec2 startPos = ImGui::GetCursorScreenPos();
		const float contentWidth = ImGui::GetContentRegionAvail().x;

		if (ImGui::Selectable("##ModuleCard", isSelected, ImGuiSelectableFlags_None, ImVec2(contentWidth, itemHeight)))
			m_SelectedIndex = static_cast<int>(i);

		// 커스텀 카드 내부 요소 오버레이
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const float iconSize = 22.0f;
		const ImVec2 iconPos(startPos.x + 8.0f, startPos.y + 6.0f);

		// 아이콘
		ImGui::SetCursorScreenPos(iconPos);
		ImGui::Text("%s", item.Icon ? item.Icon : ICON_FA_PUZZLE_PIECE);

		// 이름과 버전
		ImGui::SetCursorScreenPos(ImVec2(iconPos.x + iconSize + 6.0f, startPos.y + 4.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
		const auto* panel = item.IsResourcePlugin ? ImGuiManager::Get().GetPluginResourceEditor(item.Id) : nullptr;
		const auto displayName = panel ? panel->GetDisplayName() : item.DisplayName;
		ImGui::TextUnformatted(displayName.c_str());
		ImGui::PopStyleColor();

		// 부제목: 버전
		ImGui::SetCursorScreenPos(ImVec2(iconPos.x + iconSize + 6.0f, startPos.y + 22.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::Text("v%s", item.Version.c_str());
		ImGui::PopStyleColor();

		// 우측 상태 뱃지
		const EPluginStatus status = DetermineModuleStatus(item);
		const char* statusStr = (status == EPluginStatus::Loaded) ? LOCAL_CSTR("PluginManager.StatusLoaded") :
			(status == EPluginStatus::Compiling) ? LOCAL_CSTR("PluginManager.StatusCompiling") :
			(status == EPluginStatus::Failed) ? LOCAL_CSTR("PluginManager.StatusFailed") : LOCAL_CSTR("PluginManager.StatusUnloaded");
		const ImVec2 badgeTextSize = ImGui::CalcTextSize(statusStr);
		const float badgeWidth = badgeTextSize.x + 10.0f;

		ImGui::SetCursorScreenPos(ImVec2(startPos.x + contentWidth - badgeWidth - 4.0f, startPos.y + 12.0f));
		DrawStatusBadge(status);

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

	// 1. 헤더 (아이콘 + 이름 + 버전 + 작성자 + 설명)
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
	const auto* panel = item.IsResourcePlugin ? ImGuiManager::Get().GetPluginResourceEditor(item.Id) : nullptr;
	const auto displayName = panel ? panel->GetDisplayName() : item.DisplayName;
	ImGui::Text("%s  %s", item.Icon ? item.Icon : ICON_FA_PUZZLE_PIECE, displayName.c_str());
	ImGui::PopFont();

	if (!item.Version.empty())
	{
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));
		if (item.Author.empty())
			ImGui::Text("v%s", item.Version.c_str());
		else
			ImGui::Text("v%s  by %s", item.Version.c_str(), item.Author.c_str());
		ImGui::PopStyleColor();
	}

	ImGui::Spacing();
	ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x);
	const auto description = panel ? panel->GetDescription() : item.Description;
	ImGui::TextUnformatted(description.c_str());
	ImGui::PopTextWrapPos();

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
		if (!entry.Development && entry.Status != EPluginCatalogStatus::Ready && entry.Status != EPluginCatalogStatus::NeedsApproval)
		{
			if (status.IsLoaded && toolbar.Button("Unload", ICON_FA_EJECT, "PluginManager.Unload"))
				host.Request(index, EPluginHostAction::Unload);

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
		const bool compiling = builder && builder->IsCompiling();

		ImGui::BeginDisabled(compiling);
		if (status.IsLoaded)
		{
			if (toolbar.Button("Reload", ICON_FA_ARROWS_ROTATE, "PluginManager.Reload"))
				host.Request(index, EPluginHostAction::Reload);
		}
		else if (toolbar.Button("Load", ICON_FA_PLAY, "PluginManager.Load"))
		{
			host.Request(index, EPluginHostAction::Load);
		}

		if (builder)
		{
			const char* label = status.IsLoaded ? "PluginManager.RebuildAndReload" : "PluginManager.Build";
			if (toolbar.Button("Build", ICON_FA_HAMMER, label))
				builder->RequestBuild();
		}
		ImGui::EndDisabled();

		if (status.IsLoaded && toolbar.Button("Unload", ICON_FA_EJECT, "PluginManager.Unload"))
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

		if (status.LastActionFailed)
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.LastPluginOperationFailed"));

		return;
	}
	if (item.IsResourcePlugin)
	{
		auto* builder = HotReloadManager::Get().GetPluginBuilder(item.Id);
		auto* panel = ImGuiManager::Get().GetPluginResourceEditor(item.Id);
		const bool compiling = builder && builder->IsCompiling();

		ImGui::BeginDisabled(compiling || !panel);
		if (toolbar.Button("ResourcePluginReload", ICON_FA_ARROWS_ROTATE, "PluginManager.Reload"))
			panel->RequestReload();
		ImGui::EndDisabled();

		if (builder)
		{
			ImGui::BeginDisabled(compiling);
			if (toolbar.Button("ResourcePluginBuild", ICON_FA_HAMMER, "PluginManager.RebuildAndReload"))
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
		if (toolbar.Button("PluginManager.Reload", ICON_FA_ARROWS_ROTATE, "PluginManager.Reload"))
		{
			reloadMgr.ReloadModule();
		}
		if (compiling) ImGui::EndDisabled();

		// [Rebuild & Reload]
		if (compiling) ImGui::BeginDisabled();
		if (toolbar.Button("PluginManager.RebuildAndReload", ICON_FA_HAMMER, "PluginManager.RebuildAndReload"))
		{
			buildMgr.RequestBuild();
		}
		if (compiling) ImGui::EndDisabled();

		// [Load / Unload]
		const bool isLoaded = reloadMgr.IsLoaded();
		if (toolbar.Button("ModuleLoad", isLoaded ? ICON_FA_EJECT : ICON_FA_PLAY,
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
		if (toolbar.Button("PluginManager.RecompileAllShaders", ICON_FA_FIRE, "PluginManager.RecompileAllShaders"))
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
	else if (!item.AssociatedPanelName.empty())
	{
		if (auto* panel = ImGuiManager::Get().GetImGuiPanel(item.AssociatedPanelName))
		{
			bool isOpen = panel->IsOpen();
			if (toolbar.Button("PanelVisibility", isOpen ? ICON_FA_EYE_SLASH : ICON_FA_EYE,
				isOpen ? "PluginManager.ClosePanel" : "PluginManager.OpenPanel"))
				panel->SetOpen(!isOpen);
		}
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
			if (entry.Development)
			{
				EditorUI::PropertyRow("PluginManager.PropSourceDir");
				const auto source = WStrToStr(entry.Development->SourceDirectory.wstring());
				ImGui::TextUnformatted(source.c_str());
			}

			EditorUI::PropertyRow("PluginManager.PropOriginalDll");
			const auto binary = WStrToStr(entry.BinaryPath.wstring());
			ImGui::TextUnformatted(binary.empty() ? "-" : binary.c_str());
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
		else if (!item.AssociatedPanelName.empty())
		{
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.PropAssociatedPanel"));
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(WStrToStr(item.AssociatedPanelName).c_str());
		}

		EditorUI::EndSettingsTable();
	}
}

void PluginManagerPanel::DrawProvidedFeatures(const PluginModuleItem& item)
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("PluginManager.FeaturesHeader"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	if (item.IsGameModule)
	{
		ImGui::BulletText("%s", LOCAL_CSTR("PluginManager.FeatureSystems"));
		ImGui::Indent();
		ImGui::TextUnformatted("- SkyAtmosphereSystem (Environment Atmosphere Rendering)");
		ImGui::Unindent();

		ImGui::Spacing();
		ImGui::BulletText("%s", LOCAL_CSTR("PluginManager.FeatureComponents"));
		ImGui::Indent();
		if (Engine::ReflectionRegistry::IsCreated())
		{
			const auto entries = Engine::ReflectionRegistry::Get().GetComponentEntries();
			int gameCompCount = 0;
			for (const auto& entry : entries)
			{
				if (entry.Category == "Gameplay" || entry.Category == "GameModule" || entry.Category == "Custom")
				{
					ImGui::Text("- %s (Category: %s)", entry.Name.c_str(), entry.Category.c_str());
					gameCompCount++;
				}
			}
			if (gameCompCount == 0)
			{
				ImGui::TextUnformatted("- SkyAtmosphereComponent (Atmosphere Scattering & Sun Direction)");
			}
		}
		else
		{
			ImGui::TextUnformatted("- SkyAtmosphereComponent");
		}
		ImGui::Unindent();
	}
	else if (item.HostIndex)
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
	else if (!item.AssociatedPanelName.empty())
	{
		if (auto* panel = ImGuiManager::Get().GetImGuiPanel(item.AssociatedPanelName))
		{
			bool isOpen = panel->IsOpen();
			if (ImGui::Checkbox(LOCAL_CSTR("PluginManager.ShowPanelInWorkspace"), &isOpen))
				panel->SetOpen(isOpen);
		}
	}
	else
	{
		ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.CoreEnginePipelineFeature"));
	}
}

void PluginManagerPanel::DrawBuildConsole(const PluginModuleItem& item)
{
	auto& reloadManager = HotReloadManager::Get();
	auto* builder = FindBuilderForPluginItem(item);
	if (!builder)
		return;
	auto& buildMgr = *builder;

	EditorUI::ToolbarLayout toolbar;
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("PluginManager.BuildConsoleHeader"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	// 상단 제어 바: [Copy Log], [Clear], [Retry Compile] (실패 시), [Auto-scroll]
	if (toolbar.Button("EditorCommon.Copy", ICON_FA_COPY, "EditorCommon.Copy"))
	{
		ImGui::SetClipboardText(buildMgr.GetLastBuildOutput().c_str());
	}

	if (buildMgr.GetStatus() == EModuleBuildStatus::Failed && !buildMgr.IsCompiling())
	{
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.75f, 0.20f, 0.20f, 0.9f));
		if (toolbar.Button("PluginManager.RetryCompile", ICON_FA_HAMMER, "PluginManager.RetryCompile"))
		{
			buildMgr.RequestBuild();
		}
		ImGui::PopStyleColor();

	}

	ImGui::Checkbox(LOCAL_CSTR("PluginManager.AutoScroll"), &m_AutoScrollLog);

	ImGui::Spacing();

	// 콘솔 텍스트 차일드 윈도우
	ImGui::BeginChild("##BuildOutputLog", ImVec2(0.0f, 180.0f), true, ImGuiWindowFlags_HorizontalScrollbar);

	const std::string& output = buildMgr.GetLastBuildOutput();
	if (output.empty())
	{
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::TextUnformatted(LOCAL_CSTR("PluginManager.NoBuildOutput"));
		ImGui::PopStyleColor();
	}
	else
	{
		std::istringstream stream(output);
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
