#include "TerrainEditor.h"
#include "EditorUI.h"
#include "Application.h"
#include "SceneManager.h"
#include "SelectionManager.h"
#include "TerrainComponent.h"
#include "WorldComponents.h"
#include "Scene.h"
#include "imgui.h"
#include "FileDialogs.h"
#include "TerrainUtility.h"
#include "SceneEditHistory.h"
#include "NotificationManager.h"
#include "ShortcutManager.h"
#include <algorithm>
#include <array>
#include <string>
#include <filesystem>

TerrainEditor* TerrainEditor::s_Instance = nullptr;

TerrainEditor::TerrainEditor()
{
	s_Instance = this;
	m_Name = L"Terrain Editor";
	m_DisplayNameKey = "TerrainEditor.Title";
	m_Open = true;
}

TerrainEditor::~TerrainEditor()
{
	if (s_Instance == this) s_Instance = nullptr;
}

void TerrainEditor::SelectPaintLayer(uint32 layer)
{
	if (layer < Engine::TerrainComponent::MaxLayers) m_PaintLayer = layer;
}

void TerrainEditor::AdjustBrushRadius(float factor)
{
	m_BrushParams.Radius = std::clamp(m_BrushParams.Radius * factor, 1.0f, 100.0f);
}

void TerrainEditor::DrawPalette(const Engine::TerrainComponent& terrain)
{
	if (m_Tool == ETerrainTool::Paint)
	{
		// 레이어 팔레트: 8칸(4×2). 색 견본은 구분용 고정 색이며 레이어 텍스처는 TerrainLayer 에셋 단계에서 연결한다.
		static constexpr ImVec4 swatches[Engine::TerrainComponent::MaxLayers] = {
			{ 0.35f, 0.60f, 0.25f, 1.0f }, { 0.55f, 0.55f, 0.58f, 1.0f }, { 0.55f, 0.40f, 0.25f, 1.0f }, { 0.70f, 0.68f, 0.60f, 1.0f },
			{ 0.30f, 0.45f, 0.70f, 1.0f }, { 0.80f, 0.75f, 0.45f, 1.0f }, { 0.60f, 0.35f, 0.55f, 1.0f }, { 0.85f, 0.85f, 0.90f, 1.0f } };

		ImGui::TextUnformatted(LOCAL_CSTR("TerrainEditor.LayerPalette"));
		const float cellWidth = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * 3.0f) / 4.0f;
		for (uint32 layer = 0; layer < Engine::TerrainComponent::MaxLayers; ++layer)
		{
			if (layer % 4 != 0) ImGui::SameLine();
			ImGui::PushID(static_cast<int>(layer));
			const bool selected = layer == m_PaintLayer;
			ImGui::PushStyleColor(ImGuiCol_Button, swatches[layer]);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, selected ? 3.0f : 0.0f);
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
			// 레이어 칸이 참조하는 에셋이 있으면 그 이름(파일 이름)을, 없으면 "레이어 N"을 보인다.
			const auto* asset = layer < terrain.Layers.size() ? terrain.Layers[layer].Layer.Get() : nullptr;
			const std::string label = asset ? asset->GetDisplayName() : LOCAL_FMT("TerrainEditor.LayerSlot", layer + 1);
			if (ImGui::Button(label.c_str(), ImVec2(cellWidth, 36.0f)))
				SelectPaintLayer(layer);
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar();
			if (ImGui::IsItemHovered())
			{
				const std::string actionId = "Terrain.Layer." + std::to_string(layer + 1);
				ImGui::SetTooltip("%s", LOCAL_FMT("TerrainEditor.LayerTooltip", layer + 1, ShortcutManager::Get().GetShortcutHint(actionId)).c_str());
			}
			ImGui::PopID();
		}
		// 선택한 레이어의 게임 속성 요약. 칸에 넣을 에셋은 인스펙터의 지형 컴포넌트 "레이어" 목록에서, 값은 지형 레이어 편집기에서 고친다.
		const auto* selectedAsset = m_PaintLayer < terrain.Layers.size() ? terrain.Layers[m_PaintLayer].Layer.Get() : nullptr;
		if (selectedAsset)
		{
			const auto& settings = selectedAsset->GetSettings();
			ImGui::TextDisabled("%s", LOCAL_FMT("TerrainEditor.LayerSummary", LOCAL(settings.Walkable ? "TerrainEditor.Walkable" : "TerrainEditor.NotWalkable"),
				settings.NavCost, settings.SurfaceType).c_str());
		}
		else
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("TerrainEditor.LayerUndefined"));
		}
		return;
	}

	// 속성 페인트: 게임 의미(이동 차단·물)를 보이지 않는 맵에 칠한다. 지우기는 None을 칠한다.
	ImGui::TextUnformatted(LOCAL_CSTR("TerrainEditor.AttributePalette"));
	int attribute = m_PaintAttribute;
	ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.AttributeErase").c_str(), &attribute, static_cast<int>(Engine::ETerrainAttribute::None)); ImGui::SameLine();
	ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.AttributeBlocked").c_str(), &attribute, static_cast<int>(Engine::ETerrainAttribute::Blocked)); ImGui::SameLine();
	ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.AttributeWater").c_str(), &attribute, static_cast<int>(Engine::ETerrainAttribute::Water));
	m_PaintAttribute = static_cast<uint8>(attribute);
	ImGui::TextDisabled("%s", LOCAL_CSTR("TerrainEditor.AttributeHint"));
}

void TerrainEditor::Draw()
{
	if (!m_Open) return;

	if (BeginPanelWindow(LOCAL_LABEL("TerrainEditor.Title").c_str(), m_Open))
	{
		Engine::Scene* scene = SceneManager::Get().GetCurrentScene();
		if (!scene)
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("TerrainEditor.NoScene"));
			ImGui::End();
			return;
		}

		auto& registry = scene->GetRegistry();

		// 현재 선택된 엔티티가 TerrainComponent를 가지고 있으면 자동 추적
		Engine::Entity* selected = SelectionManager::Get().GetPrimarySelectedEntity();
		if (selected && selected->GetScene() == scene)
		{
			if (registry.try_get<Engine::TerrainComponent>(selected->GetEntityHandle()))
			{
				m_TargetEntity = selected;
			}
		}

		auto* terrainComp = (m_TargetEntity && m_TargetEntity->GetScene() == scene) ?
			registry.try_get<Engine::TerrainComponent>(m_TargetEntity->GetEntityHandle()) : nullptr;

		if (terrainComp)
		{
			const auto* nameComp = registry.try_get<Engine::NameComponent>(m_TargetEntity->GetEntityHandle());
			std::string nameStr = nameComp ? WStrToStr(nameComp->name) : LOCAL("TerrainEditor.DefaultName");
			ImGui::TextColored(ImVec4(0.3f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_FMT("TerrainEditor.Target", nameStr).c_str());
		}
		else
		{
			EditorUI::EmptyState("TerrainEditor.NoSelection");
			m_IsSculpting = false;
			ImGui::End();
			return;
		}

		ImGui::Separator();

		// 스컬프팅 On/Off 큰 버튼
		EditorUI::TextButtonOptions sculptBtnOpt;
		sculptBtnOpt.FullWidth = true;
		sculptBtnOpt.Height = ImGui::GetFrameHeight() * 1.25f;
		if (m_IsSculpting)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
			if (EditorUI::TextButton("ToggleSculpt", "TerrainEditor.SculptOn", sculptBtnOpt)) m_IsSculpting = false;
			ImGui::PopStyleColor();
		}
		else
		{
			if (EditorUI::TextButton("ToggleSculpt", "TerrainEditor.SculptOff", sculptBtnOpt)) m_IsSculpting = true;
		}

		// 도구: 조형 / 레이어 페인트 / 속성 페인트 (선택된 도구는 강조 색)
		ImGui::Spacing();
		ImGui::TextUnformatted(LOCAL_CSTR("TerrainEditor.Tool"));
		const auto toolButton = [this](const char* id, ETerrainTool tool, const char* labelKey, bool sameLine)
			{
				EditorUI::TextButtonOptions opt;
				opt.Active = (m_Tool == tool);
				opt.SameLine = sameLine;
				if (EditorUI::TextButton(id, labelKey, opt)) m_Tool = tool;
			};
		toolButton("ToolSculpt", ETerrainTool::Sculpt, "TerrainEditor.Sculpt", false);
		toolButton("ToolPaint", ETerrainTool::Paint, "TerrainEditor.PaintTool", true);
		toolButton("ToolAttribute", ETerrainTool::Attribute, "TerrainEditor.AttributeTool", true);

		ImGui::Spacing();
		if (m_Tool == ETerrainTool::Sculpt)
		{
			ImGui::TextUnformatted(LOCAL_CSTR("TerrainEditor.BrushMode"));
			int modeInt = static_cast<int>(m_BrushParams.Mode);
			ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.Raise").c_str(), &modeInt, 0); ImGui::SameLine();
			ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.Lower").c_str(), &modeInt, 1); ImGui::SameLine();
			ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.Smooth").c_str(), &modeInt, 2); ImGui::SameLine();
			ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.Flatten").c_str(), &modeInt, 3);
			m_BrushParams.Mode = static_cast<Engine::ETerrainBrushMode>(modeInt);
		}
		else
		{
			DrawPalette(*terrainComp);
		}

		ImGui::Spacing();
		ImGui::Separator();

		ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.Radius").c_str(), &m_BrushParams.Radius, 1.0f, 100.0f, "%.1f m");
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", LOCAL_FMT("TerrainEditor.RadiusTooltip", ShortcutManager::Get().GetShortcutHint("Terrain.Brush.Smaller"),
				ShortcutManager::Get().GetShortcutHint("Terrain.Brush.Larger")).c_str());
		if (m_Tool == ETerrainTool::Sculpt)
			ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.Strength").c_str(), &m_BrushParams.Strength, 0.1f, 50.0f, "%.1f");
		else if (m_Tool == ETerrainTool::Paint)
			ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.Strength").c_str(), &m_PaintStrength, 0.05f, 5.0f, "%.2f");
		if (m_Tool != ETerrainTool::Attribute)
			ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.Falloff").c_str(), &m_BrushParams.Falloff, 0.0f, 1.0f, "%.2f");

		if (m_BrushParams.Mode == Engine::ETerrainBrushMode::Flatten)
		{
			const float maxHeight = terrainComp->MaxHeight;
			ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.TargetHeight").c_str(), &m_BrushParams.TargetHeight, 0.0f, maxHeight, "%.1f m");
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextUnformatted(LOCAL_CSTR("TerrainEditor.HeightmapFile"));

		// 가져오기·초기화는 높이 전체를 바꾸므로 스컬프트와 같은 장면 편집 이력으로 기록해 Ctrl+Z로 되돌린다.
		const auto recordTerrainEdit = [&](const char* labelKey, const auto& edit)
			{
				const std::array targets{ m_TargetEntity->GetEntityHandle() };
				SceneEditSnapshot before = SceneEditSnapshot::Capture(*scene, targets);
				if (!edit()) return false;
				SceneEditSnapshot after = SceneEditSnapshot::Capture(*scene, targets);
				Application::Get().GetEditHistory().Record(std::move(before), std::move(after), LOCAL(labelKey));
				return true;
			};

		EditorUI::TextButtonOptions fileActionOpt;
		fileActionOpt.FullWidth = true;

		// 높이맵 가져오기 (Import): 16비트 PNG·8비트 이미지·RAW(.raw/.r16, 리틀 엔디언 uint16)
		if (EditorUI::TextButton("ImportHeightmap", "TerrainEditor.ImportHeightmap", fileActionOpt))
		{
			std::wstring openPath;
			const std::vector<FileDialogFilter> filters = {
				{ StrToWStr(LOCAL("TerrainEditor.HeightmapFiles")), L"*.png;*.raw;*.r16;*.jpg;*.jpeg;*.bmp;*.tga" },
				{ StrToWStr(LOCAL("FileDialog.AllFiles")), L"*.*" }
			};
			if (FileDialogs::OpenFileDialog(openPath, filters, L"", EFileDialogContext::HeightmapOpen))
			{
				const bool imported = recordTerrainEdit("TerrainEditor.HistoryImport",
					[&] { return Engine::TerrainUtility::LoadHeightmapFromFile(*terrainComp, openPath); });
				if (!imported)
					NotificationManager::Get().AddNotification(LOCAL("TerrainEditor.HeightmapFile"),
						LOCAL_FMT("TerrainEditor.ImportFailed", WStrToStr(std::filesystem::path(openPath).filename().wstring())), ELogLevel::Warn);
			}
		}

		// 높이맵 내보내기 (Export): 16비트 PNG 또는 RAW. 확장자로 형식을 정한다.
		if (EditorUI::TextButton("ExportHeightmap", "TerrainEditor.ExportHeightmap", fileActionOpt))
		{
			std::wstring savePath;
			const std::vector<FileDialogFilter> filters = {
				{ StrToWStr(LOCAL("TerrainEditor.Png16Files")), L"*.png" },
				{ StrToWStr(LOCAL("TerrainEditor.RawFiles")), L"*.raw;*.r16" }
			};
			if (FileDialogs::SaveFileDialog(savePath, filters, L"Terrain_Heightmap", L"png", EFileDialogContext::HeightmapSave))
			{
				const auto fileName = WStrToStr(std::filesystem::path(savePath).filename().wstring());
				if (Engine::TerrainUtility::SaveHeightmapToFile(*terrainComp, savePath))
					NotificationManager::Get().AddNotification(LOCAL("TerrainEditor.HeightmapFile"),
						LOCAL_FMT("TerrainEditor.ExportDone", fileName), ELogLevel::Info, 3.0f);
				else
					NotificationManager::Get().AddNotification(LOCAL("TerrainEditor.HeightmapFile"),
						LOCAL_FMT("TerrainEditor.ExportFailed", fileName), ELogLevel::Warn);
			}
		}

		// 지형 평탄화 초기화 (Reset)
		if (EditorUI::TextButton("ResetTerrain", "TerrainEditor.ResetTerrain", fileActionOpt))
		{
			recordTerrainEdit("TerrainEditor.HistoryReset", [&]
				{
					std::fill(terrainComp->HeightData.begin(), terrainComp->HeightData.end(), 0.0f);
					terrainComp->MarkHeightDirty();
					terrainComp->IsDataDirty = true;
					return true;
				});
		}
	}
	ImGui::End();
}