#include "Editor_Includes.h"
#include "DataTableEditor.h"
#include "EditorUI.h"
#include "Functions.h"
#include "NotificationManager.h"
#include "PlatformPath.h"
#include <imgui_stdlib.h>
#include <algorithm>
#include <cctype>
#include <vector>

namespace DataTableEditorDetail
{
	bool ContainsIgnoreCase(std::string_view haystack, std::string_view needle)
	{
		if (needle.empty())
			return true;
		if (haystack.size() < needle.size())
			return false;

		const auto it = std::search(
			haystack.begin(), haystack.end(),
			needle.begin(), needle.end(),
			[](char a, char b)
			{
				return std::tolower(static_cast<unsigned char>(a)) ==
					std::tolower(static_cast<unsigned char>(b));
			});
		return it != haystack.end();
	}

	bool RowMatchesFilter(const Engine::DataTable& table, std::size_t row, std::string_view filter)
	{
		if (filter.empty())
			return true;

		const std::size_t colCount = table.GetColumnCount();
		for (std::size_t col = 0; col < colCount; ++col)
		{
			if (ContainsIgnoreCase(table.GetCell(row, col), filter))
				return true;
		}
		return false;
	}
}

bool DataTableEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	return Engine::ToLowerStr(Engine::WStrToStr(assetPath.extension().wstring())) == ".csv";
}

void DataTableEditor::SetTargetResource(const std::filesystem::path& assetPath)
{
	OpenFile(assetPath);
}

string DataTableEditor::BuildResourceWindowTitle() const
{
	auto title = GetDisplayName();
	if (!m_TargetResourcePath.empty())
		title += " - " + WStrToStr(m_TargetResourcePath.filename().wstring());
	if (HasUnsavedResourceChanges())
		title += "*";
	return title + "###" + GetResourceWindowId();
}

bool DataTableEditor::OpenFile(const std::filesystem::path& path)
{
	ResourceEditorInterface::SetTargetResource(path);
	m_Table.Clear();
	m_LastError.clear();
	m_PendingRemoveRow.reset();
	m_Loaded = false;
	m_Dirty = false;

	if (path.empty())
		return false;

	std::string error;
	if (!m_Table.LoadFromCsvFile(path, &error))
	{
		m_LastError = std::move(error);
		NotificationManager::Get().AddNotification(
			GetDisplayName(),
			WStrToStr(path.filename().wstring()) + ": " + m_LastError,
			ELogLevel::Warn);
		return false;
	}

	m_Loaded = true;
	return true;
}

bool DataTableEditor::SaveFile()
{
	if (!m_Loaded || m_TargetResourcePath.empty())
		return false;

	if (Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath))
		return false;

	std::string error;
	if (!m_Table.SaveToCsvFile(m_TargetResourcePath, &error))
	{
		m_LastError = std::move(error);
		NotificationManager::Get().AddNotification(
			GetDisplayName(),
			WStrToStr(m_TargetResourcePath.filename().wstring()) + ": " + m_LastError,
			ELogLevel::Warn);
		return false;
	}

	m_LastError.clear();
	m_Dirty = false;
	return true;
}

void DataTableEditor::SaveResource()
{
	SaveFile();
}

void DataTableEditor::ReleaseResourceDocument()
{
	m_Table.Clear();
	m_SearchFilter[0] = '\0';
	m_LastError.clear();
	m_PendingRemoveRow.reset();
	m_Loaded = false;
	m_Dirty = false;
}

bool DataTableEditor::RestoreSavedResource()
{
	return OpenFile(m_TargetResourcePath);
}

void DataTableEditor::DrawCustomToolbar()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
	{
		const bool canSave = m_Loaded && m_Dirty && !Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath);
		if (ImGui::MenuItem(LOCAL_LABEL("EditorCommon.Save").c_str(), nullptr, false, canSave))
		{
			SaveResource();
		}
		DrawCloseMenuItem();
		ImGui::EndMenu();
	}
}

void DataTableEditor::DrawCustomQuickAccess()
{
	EditorUI::ToolbarLayout toolbar;
	DrawCopyResourcePath();

	const bool readOnly = Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath);

	ImGui::SameLine();
	ImGui::BeginDisabled(!m_Loaded || !m_Dirty || readOnly);
	if (ImGui::Button(LOCAL_CSTR("EditorCommon.Save")))
	{
		SaveResource();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::BeginDisabled(!m_Loaded || m_Table.GetColumnCount() == 0 || readOnly);
	if (ImGui::Button(LOCAL_LABEL("ResourceEditor.DataTableAddRow").c_str()))
	{
		if (m_Table.AddEmptyRow())
		{
			m_Dirty = true;
		}
	}
	ImGui::EndDisabled();

	if (!m_TargetResourcePath.empty())
	{
		std::string fileLabel = WStrToStr(m_TargetResourcePath.filename().wstring());
		if (m_Dirty)
		{
			fileLabel += " *";
		}
		toolbar.Text(fileLabel.c_str(), !m_Dirty);
	}
}

void DataTableEditor::DrawCustomContent()
{
	if (!m_LastError.empty())
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_LastError.c_str());
		ImGui::Spacing();
	}

	if (!m_Loaded)
	{
		EditorUI::EmptyState("ResourceEditor.NoResource");
		return;
	}

	EditorUI::DrawSearchBar("##DataTableSearch", m_SearchFilter, sizeof(m_SearchFilter));

	const std::size_t colCount = m_Table.GetColumnCount();
	if (colCount == 0)
	{
		EditorUI::EmptyState("ResourceEditor.NoResource");
		return;
	}

	const std::string_view filterView(m_SearchFilter);
	std::vector<std::size_t> visibleRows;
	visibleRows.reserve(m_Table.GetRowCount());
	for (std::size_t row = 0; row < m_Table.GetRowCount(); ++row)
	{
		if (DataTableEditorDetail::RowMatchesFilter(m_Table, row, filterView))
		{
			visibleRows.push_back(row);
		}
	}

	ImGui::TextDisabled("%zu / %zu rows", visibleRows.size(), m_Table.GetRowCount());
	ImGui::Spacing();

	const bool readOnly = Engine::PlatformPath::IsReadOnlyTarget(m_TargetResourcePath);
	const int totalTableCols = static_cast<int>(colCount) + 2;
	const ImGuiTableFlags tableFlags =
		ImGuiTableFlags_Borders |
		ImGuiTableFlags_RowBg |
		ImGuiTableFlags_Resizable |
		ImGuiTableFlags_ScrollX |
		ImGuiTableFlags_ScrollY;

	if (ImGui::BeginTable("DataTableGrid", totalTableCols, tableFlags, ImGui::GetContentRegionAvail()))
	{
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 48.0f);

		const auto& columns = m_Table.GetColumns();
		for (std::size_t col = 0; col < colCount; ++col)
		{
			ImGui::TableSetupColumn(columns[col].c_str(), ImGuiTableColumnFlags_WidthStretch);
		}

		ImGui::TableSetupColumn("##Actions", ImGuiTableColumnFlags_WidthFixed | ImGuiTableColumnFlags_NoResize, 36.0f);
		ImGui::TableHeadersRow();

		ImGuiListClipper clipper;
		clipper.Begin(static_cast<int>(visibleRows.size()));
		while (clipper.Step())
		{
			for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i)
			{
				const std::size_t rowIdx = visibleRows[static_cast<std::size_t>(i)];
				ImGui::TableNextRow();
				ImGui::PushID(static_cast<int>(rowIdx));

				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("%zu", rowIdx);

				for (std::size_t colIdx = 0; colIdx < colCount; ++colIdx)
				{
					ImGui::TableSetColumnIndex(static_cast<int>(colIdx) + 1);
					ImGui::PushID(static_cast<int>(colIdx));

					std::string cellValue = m_Table.GetCell(rowIdx, colIdx);
					ImGui::SetNextItemWidth(-FLT_MIN);
					const ImGuiInputTextFlags inputFlags =
						readOnly ? ImGuiInputTextFlags_ReadOnly : ImGuiInputTextFlags_None;
					if (ImGui::InputText("##Cell", &cellValue, inputFlags))
					{
						if (!readOnly && m_Table.SetCell(rowIdx, colIdx, std::move(cellValue)))
						{
							m_Dirty = true;
						}
					}

					ImGui::PopID();
				}

				ImGui::TableSetColumnIndex(static_cast<int>(colCount) + 1);
				ImGui::BeginDisabled(readOnly);
				if (ImGui::SmallButton("X"))
				{
					m_PendingRemoveRow = rowIdx;
				}
				ImGui::EndDisabled();

				ImGui::PopID();
			}
		}

		ImGui::EndTable();
	}

	if (m_PendingRemoveRow.has_value())
	{
		if (!readOnly && m_Table.RemoveRow(*m_PendingRemoveRow))
		{
			m_Dirty = true;
		}
		m_PendingRemoveRow.reset();
	}
}
