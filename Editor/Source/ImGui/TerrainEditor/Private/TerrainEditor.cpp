#include "TerrainEditor.h"
#include "Application.h"
#include "SceneManager.h"
#include "SelectionManager.h"
#include "TerrainComponent.h"
#include "CoreComponents.h"
#include "Scene.h"
#include "imgui.h"
#include "FileDialogs.h"

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

void TerrainEditor::Draw()
{
	if (!m_Open) return;

	if (ImGui::Begin(LOCAL_LABEL("TerrainEditor.Title").c_str(), &m_Open))
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
			ImGui::TextDisabled("%s", LOCAL_CSTR("TerrainEditor.NoSelection"));
			m_IsSculpting = false;
			ImGui::End();
			return;
		}

		ImGui::Separator();

		// 스컬프팅 On/Off 큰 버튼
		if (m_IsSculpting)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.7f, 0.3f, 1.0f));
			if (ImGui::Button(LOCAL_LABEL("TerrainEditor.SculptOn").c_str(), ImVec2(-1.0f, 32.0f))) m_IsSculpting = false;
			ImGui::PopStyleColor();
		}
		else
		{
			if (ImGui::Button(LOCAL_LABEL("TerrainEditor.SculptOff").c_str(), ImVec2(-1.0f, 32.0f))) m_IsSculpting = true;
		}

		ImGui::Spacing();
		ImGui::TextUnformatted(LOCAL_CSTR("TerrainEditor.BrushMode"));

		int modeInt = static_cast<int>(m_BrushParams.Mode);
		ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.Raise").c_str(), &modeInt, 0); ImGui::SameLine();
		ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.Lower").c_str(), &modeInt, 1); ImGui::SameLine();
		ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.Smooth").c_str(), &modeInt, 2); ImGui::SameLine();
		ImGui::RadioButton(LOCAL_LABEL("TerrainEditor.Flatten").c_str(), &modeInt, 3);
		m_BrushParams.Mode = static_cast<Engine::ETerrainBrushMode>(modeInt);

		ImGui::Spacing();
		ImGui::Separator();

		ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.Radius").c_str(), &m_BrushParams.Radius, 1.0f, 100.0f, "%.1f m");
		ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.Strength").c_str(), &m_BrushParams.Strength, 0.1f, 50.0f, "%.1f");
		ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.Falloff").c_str(), &m_BrushParams.Falloff, 0.0f, 1.0f, "%.2f");

		if (m_BrushParams.Mode == Engine::ETerrainBrushMode::Flatten)
		{
			const float maxHeight = terrainComp->MaxHeight;
			ImGui::SliderFloat(LOCAL_LABEL("TerrainEditor.TargetHeight").c_str(), &m_BrushParams.TargetHeight, 0.0f, maxHeight, "%.1f m");
		}
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::TextUnformatted(LOCAL_CSTR("TerrainEditor.HeightmapFile"));

		// 높이맵 가져오기 (Import)
		if (ImGui::Button(LOCAL_LABEL("TerrainEditor.ImportHeightmap").c_str(), ImVec2(-1.0f, 26.0f)))
		{
			std::wstring openPath;
			const std::vector<FileDialogFilter> filters = {
				{ L"Image Files", L"*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.hdr" },
				{ L"All Files", L"*.*" }
			};
			if (FileDialogs::OpenFileDialog(openPath, filters))
			{
				Engine::TerrainUtility::LoadHeightmapFromFile(*terrainComp, openPath);
			}
		}

		// 높이맵 내보내기 / 저장 (Export / Bake)
		if (ImGui::Button(LOCAL_LABEL("TerrainEditor.ExportHeightmap").c_str(), ImVec2(-1.0f, 26.0f)))
		{
			std::wstring savePath;
			const std::vector<FileDialogFilter> filters = {
				{ L"PNG Image (*.png)", L"*.png" }
			};
			if (FileDialogs::SaveFileDialog(savePath, filters, L"Terrain_Heightmap", L"png"))
			{
				Engine::TerrainUtility::SaveHeightmapToFile(*terrainComp, savePath);
			}
		}

		// 지형 평탄화 초기화 (Reset)
		if (ImGui::Button(LOCAL_LABEL("TerrainEditor.ResetTerrain").c_str(), ImVec2(-1.0f, 26.0f)))
		{
			std::fill(terrainComp->HeightData.begin(), terrainComp->HeightData.end(), 0.0f);
			terrainComp->IsDirty = true;
		}
	}
	ImGui::End();
}