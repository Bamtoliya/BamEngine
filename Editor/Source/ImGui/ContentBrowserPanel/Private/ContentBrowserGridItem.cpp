#include "ContentBrowserGridItem.h"

void ContentBrowserGridItem::Draw(const std::filesystem::directory_entry& directoryEntry, const std::filesystem::path& rootPath, void* thumbnailTexID, f32 thumbnailSize, f32 padding, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory, char* searchBuffer)
{
	float filledSize = ImGui::GetContentRegionAvail().x;
	const auto& path = directoryEntry.path();
	auto relativePath = std::filesystem::relative(path, rootPath);

	ImGui::PushID(path.string().c_str());

	// 1. 썸네일 및 부가 요소 렌더링
	Thumbnail(directoryEntry, thumbnailTexID, filledSize, padding);
	ToolTip(directoryEntry);
	ContextMenu(path, outRenamingPath, renameBuffer, renameBufferSize);
	DragAndDropTarget(relativePath);

	// 썸네일 위에서 더블클릭 시 폴더 이동
	if (ImGui::IsItemHovered() && MOUSE_BUTTON_DOUBLE_CLICK(EMouseButton::Left))
	{
		if (directoryEntry.is_directory())
		{
			currentDirectory /= path.filename();
			if (searchBuffer) searchBuffer[0] = '\0';
		}
	}

	// 2. 이름 변경 중인지 확인 후 하단 UI 분기
	if (outRenamingPath == path)
	{
		DrawRename(path, filledSize, outRenamingPath, renameBuffer, renameBufferSize, currentDirectory);
	}
	else
	{
		DrawNameBox(directoryEntry, rootPath, filledSize, currentDirectory, searchBuffer);
	}

	ImGui::PopID();
}

void ContentBrowserGridItem::Thumbnail(const std::filesystem::directory_entry& directoryEntry, void* thumbnailTexID, f32 thumbnailSize, f32 padding)
{
	std::string icon = ICON_FA_FILE;
	ImVec4 typeColor = ImVec4(1.0f, 1.0f, 1.0f, 0.1f);

	if (directoryEntry.is_directory())
	{
		icon = ICON_FA_FOLDER;
		typeColor = ImVec4(0.9f, 0.7f, 0.2f, 0.3f);
	}
	else
	{
		std::string ext = directoryEntry.path().extension().string();
		for (auto& c : ext) c = tolower(c);

		if (ext == ".png" || ext == ".jpg" || ext == ".tga")
		{
			icon = ICON_FA_IMAGE;
			typeColor = ImVec4(0.2f, 0.8f, 0.2f, 0.4f);
		}
		else if (ext == ".fbx" || ext == ".obj")
		{
			icon = ICON_FA_CUBE;
			typeColor = ImVec4(0.2f, 0.4f, 0.9f, 0.4f);
		}
		else if (ext == ".mat")
		{
			icon = ICON_FA_CIRCLE;
			typeColor = ImVec4(0.9f, 0.3f, 0.3f, 0.4f);
		}
		else if (ext == ".hlsl" || ext == ".glsl")
		{
			icon = ICON_FA_CODE;
			typeColor = ImVec4(0.7f, 0.3f, 0.9f, 0.4f);
		}
		else if (ext == ".csv" || ext == ".json")
		{
			icon = ICON_FA_FILE_LINES;
			typeColor = ImVec4(0.6f, 0.6f, 0.6f, 0.4f);
		}
	}

	ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	if (!thumbnailTexID)
	{
		// 썸네일이 없을 때 렌더링하는 배경의 경우, 네임박스와 자연스럽게 이어지도록 상단 모서리만 둥글게 처리합니다.
		drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + thumbnailSize, cursorPos.y + thumbnailSize), ImGui::GetColorU32(typeColor), 4.0f, ImDrawFlags_RoundCornersTop);
		ImFont* font = ImGui::GetFont();
		float iconFontSize = thumbnailSize * 0.85f;
		ImVec2 iconSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, icon.c_str());
		ImVec2 iconPos = ImVec2(cursorPos.x + (thumbnailSize - iconSize.x) * 0.5f, cursorPos.y + (thumbnailSize - iconSize.y) * 0.5f);
		drawList->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(ImGuiCol_Text), icon.c_str());
	}

	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(1.0f, 1.0f, 1.0f, 0.1f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(1.0f, 1.0f, 1.0f, 0.2f));

	// ImageButton과 일반 Button의 패딩 차이로 인한 크기 불일치 해결
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

	std::string buttonId = "##Btn_" + directoryEntry.path().filename().string();

	if (thumbnailTexID)
	{
		ImGui::ImageButton(buttonId.c_str(), (ImTextureID)thumbnailTexID, { thumbnailSize, thumbnailSize });
	}
	else
	{
		ImGui::Button(buttonId.c_str(), { thumbnailSize, thumbnailSize });
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
}

void ContentBrowserGridItem::ContextMenu(const std::filesystem::path& path, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize)
{
	std::string filenameString = path.filename().string();
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem(ICON_FA_PEN " Rename", "F2"))
		{
			outRenamingPath = path;
			strncpy(renameBuffer, filenameString.c_str(), renameBufferSize);
		}

		if (ImGui::MenuItem(ICON_FA_COPY " Copy Path", "Ctrl+C"))
		{
			ImGui::SetClipboardText(path.string().c_str());
		}

		ImGui::Separator();

		if (ImGui::MenuItem(ICON_FA_TRASH " Delete"))
		{
			// TODO: 삭제 로직
			TODO("GridItem Context Menu 삭제 로직 만들어야함")
		}

		ImGui::EndPopup();
	}

	if (ImGui::IsItemHovered() && KEY_DOWN(EKeyCode::F2))
	{
		outRenamingPath = path;
		strncpy(renameBuffer, filenameString.c_str(), renameBufferSize);
	}
}

void ContentBrowserGridItem::ToolTip(const std::filesystem::directory_entry& directoryEntry)
{
	if (ImGui::IsItemHovered())
	{
		std::filesystem::path path = directoryEntry.path();
		std::string filenameString = path.filename().string();
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(filenameString.c_str());
		ImGui::Separator();

		if (directoryEntry.is_directory())
		{
			ImGui::Text("Type: Folder");
		}
		else
		{
			std::string ext = path.extension().string();
			for (auto& c : ext) c = tolower(c);
			ImGui::Text("Type: %s File", ext.c_str());

			try
			{
				uintmax_t fileSize = std::filesystem::file_size(path);
				if (fileSize < 1024)
					ImGui::Text("Size: %ju Bytes", fileSize);
				else if (fileSize < 1024 * 1024)
					ImGui::Text("Size: %ju KB", fileSize / 1024);
				else
					ImGui::Text("Size: %.2f MB", (float)fileSize / (1024.0f * 1024.0f));
			}
			catch (...) {}
		}
		ImGui::EndTooltip();
	}
}

void ContentBrowserGridItem::DragAndDropTarget(const std::filesystem::path& relativePath)
{
	if (ImGui::BeginDragDropSource())
	{
		std::wstring wPath = relativePath.wstring();
		ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", wPath.c_str(), (wPath.length() + 1) * sizeof(wchar_t));

		ImGui::Text("%s", relativePath.filename().string().c_str());
		ImGui::EndDragDropSource();
	}
}

void ContentBrowserGridItem::DrawRename(const std::filesystem::path& path, f32 thumbnailSize, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory)
{
	// 이름 변경 텍스트 박스도 썸네일과 딱 붙도록 Y축 여백 제거
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemSpacing.y);

	float width = (currentDirectory == path.parent_path()) ? thumbnailSize : ImGui::GetContentRegionAvail().x * 0.8f;
	ImGui::SetNextItemWidth(width);

	if (ImGui::InputText("##RenameInput", renameBuffer, renameBufferSize, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
	{
		std::filesystem::path newPath = path.parent_path() / renameBuffer;
		if (renameBuffer[0] != '\0' && !std::filesystem::exists(newPath))
		{
			try {
				std::filesystem::rename(path, newPath);
				if (currentDirectory == path) currentDirectory = newPath;
			}
			catch (...) {}
		}
		outRenamingPath.clear();
	}

	if (ImGui::IsItemVisible() && !ImGui::IsAnyItemActive())
		ImGui::SetKeyboardFocusHere(-1);

	if (ImGui::IsItemDeactivated() && !ImGui::IsItemDeactivatedAfterEdit())
	{
		outRenamingPath.clear();
	}
	else if (KEY_DOWN(EKeyCode::Escape))
	{
		outRenamingPath.clear();
	}
}

void ContentBrowserGridItem::DrawNameBox(const std::filesystem::directory_entry& directoryEntry, const std::filesystem::path& rootPath, f32 thumbnailSize, std::filesystem::path& currentDirectory, char* searchBuffer)
{
	const auto& path = directoryEntry.path();
	auto relativePath = std::filesystem::relative(path, rootPath);

	// 폴더면 그대로(filename), 파일이면 확장자 제거(stem)
	std::string filenameString = directoryEntry.is_directory() ? relativePath.filename().string() : relativePath.stem().string();

	float boxWidth = thumbnailSize;

	// 고정 높이 지정: 텍스트 2줄(이름, 타입) + 여백(12.0f)으로 항상 균일한 높이 유지
	float boxHeight = ImGui::GetTextLineHeight() * 2.0f + 12.0f;

	std::string typeStr = "";
	if (directoryEntry.is_directory())
	{
		typeStr = "Folder";
	}
	else
	{
		typeStr = path.extension().string();
		if (!typeStr.empty() && typeStr[0] == '.') typeStr = typeStr.substr(1);
		for (auto& c : typeStr) c = toupper(c);
	}

	ImVec2 typeTextSize = ImGui::CalcTextSize(typeStr.c_str());

	ImVec2 cursorPos = ImGui::GetCursorScreenPos();
	// [핵심 변경점] 썸네일과 완전히 밀착시키기 위해 ImGui의 기본 ItemSpacing만큼 위로 끌어올립니다.
	cursorPos.y -= ImGui::GetStyle().ItemSpacing.y;

	ImVec2 boxMin = cursorPos;
	ImVec2 boxMax = ImVec2(boxMin.x + boxWidth, boxMin.y + boxHeight);

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// 1. 네임 박스 배경 (상단은 각지게, 하단만 둥글게 처리하여 이어진 카드 느낌 연출)
	ImU32 boxColor = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 0.8f));
	drawList->AddRectFilled(boxMin, boxMax, boxColor, 4.0f, ImDrawFlags_RoundCornersBottom);

	// 2. 텍스트 축약(Ellipsis) 로직 (UTF-8 한글 깨짐 방지 처리 포함)
	float maxTextWidth = boxWidth - 8.0f; // 양옆 패딩 4.0f 제외한 최대 너비
	std::string displayFileName = filenameString;

	if (ImGui::CalcTextSize(displayFileName.c_str()).x > maxTextWidth)
	{
		while (!displayFileName.empty() && ImGui::CalcTextSize((displayFileName + "...").c_str()).x > maxTextWidth)
		{
			// UTF-8 안전한 pop_back (멀티바이트 문자가 중간에 잘리지 않도록 안전하게 삭제)
			while (!displayFileName.empty())
			{
				char c = displayFileName.back();
				displayFileName.pop_back();
				if ((c & 0xC0) != 0x80) break; // 후속 바이트(10xxxxxx)가 아니면 1글자 지우기 완료
			}
		}
		displayFileName += "...";
	}

	// 3. 파일 이름 렌더링 (1줄 렌더링, 길면 축약됨)
	drawList->AddText(ImVec2(boxMin.x + 4.0f, boxMin.y + 4.0f), ImGui::GetColorU32(ImGuiCol_Text), displayFileName.c_str());

	// 4. 확장자/타입 렌더링 (우측 하단 고정 배치)
	ImVec2 typePos = ImVec2(boxMax.x - typeTextSize.x - 4.0f, boxMax.y - typeTextSize.y - 4.0f);
	drawList->AddText(typePos, ImGui::GetColorU32(ImGuiCol_TextDisabled), typeStr.c_str());

	// 5. 공간 점유 및 더블 클릭 인터랙션
	ImGui::SetCursorScreenPos(boxMin);
	ImGui::Dummy(ImVec2(boxWidth, boxHeight));

	if (ImGui::IsItemHovered() && MOUSE_BUTTON_DOUBLE_CLICK(EMouseButton::Left))
	{
		if (directoryEntry.is_directory())
		{
			currentDirectory /= path.filename();
			if (searchBuffer) searchBuffer[0] = '\0';
		}
	}
}