#pragma once
#include "ContentBrowserPanel.h"

EResult ContentBrowserPanel::Initialize(void* arg)
{
	filesystem::path executablePath = filesystem::current_path();
	m_RootPath = executablePath;
	m_CurrentDirectory = m_RootPath;
	return EResult();
}

void ContentBrowserPanel::Draw()
{
	if (!m_Open) return;
	ImGui::Begin("Content Browser", &m_Open);

	BackButton();

	ImGui::SameLine();
	ImGui::Text("Current Directory: %s", m_CurrentDirectory.string().c_str());
	ImGui::Separator();

	f32 cellSize = m_ThumbnailSize + m_Padding;
	f32 panelWidth = ImGui::GetContentRegionAvail().x;
	int32 columnCount = static_cast<int>(panelWidth / cellSize);
	if (columnCount < 1) columnCount = 1;

	if (ImGui::BeginTable("ContentBrowserTable", columnCount))
	{
		for (auto& directoryEntry : filesystem::directory_iterator(m_CurrentDirectory))
		{
			const auto& path = directoryEntry.path();
			auto relativePath = filesystem::relative(path, m_RootPath);
			string filenameString = relativePath.filename().string();

			ImGui::TableNextColumn();

			ImGui::PushID(filenameString.c_str());

			// 아이콘 선택 (폴더 vs 파일)
			const char* icon = directoryEntry.is_directory() ? ICON_FA_FOLDER : ICON_FA_FILE;

			// 썸네일 버튼 스타일
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::Button(icon, { m_ThumbnailSize, m_ThumbnailSize });

			// 드래그 앤 드랍 소스 설정 (파일일 경우)
			if (ImGui::BeginDragDropSource())
			{
				const wchar_t* itemPath = relativePath.c_str();
				// "CONTENT_BROWSER_ITEM" 이라는 이름으로 경로 데이터 전달
				ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));

				ImGui::Text("%s", filenameString.c_str());
				ImGui::EndDragDropSource();
			}

			ImGui::PopStyleColor();

			// 인터랙션: 폴더 진입
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
			{
				if (directoryEntry.is_directory())
					m_CurrentDirectory /= path.filename();
			}

			ImGui::TextWrapped("%s", filenameString.c_str());

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	ImGui::End();
}

void ContentBrowserPanel::BackButton()
{
	if (m_CurrentDirectory != m_RootPath)
	{
		if (ImGui::Button(ICON_FA_ARROW_LEFT_LONG))
		{
			m_CurrentDirectory = m_CurrentDirectory.parent_path();
		}
	}
}
