#pragma once
#include "ImGuiManager.h"
#include "ResourceEditorInterface.h"
#include "ContentBrowserGridItem.h"
#include "SelectionManager.h"
#include "ShaderHotReloadManager.h"
#include "ModelImporter.h"
#include "Mesh.h"
#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include "SceneManager.h"
#include "Application.h"

namespace
{
	Engine::EResourceType PeekResourceType(const std::filesystem::path& path)
	{
		std::string ext = path.extension().string();
		for (char& c : ext) c = static_cast<char>(tolower(c));

		Engine::ResourceBinaryHeader header;
		archive::ArchiveResult res;

		if (ext == ".json")
		{
			archive::JsonArchive ar;
			if (!ar.LoadFromFile(path)) return Engine::EResourceType::Unknown;
			res = ar.ReadObject("AssetHeader", header, [](archive::IArchive& fields, Engine::ResourceBinaryHeader& h) {
				return fields.Read("ResourceType", h.ResourceType);
				});
		}
		else
		{
			archive::BeveArchive ar;
			if (!ar.LoadFromFile(path)) return Engine::EResourceType::Unknown;
			res = ar.ReadObject("AssetHeader", header, [](archive::IArchive& fields, Engine::ResourceBinaryHeader& h) {
				return fields.Read("ResourceType", h.ResourceType);
				});
		}

		return res ? header.ResourceType : Engine::EResourceType::Unknown;
	}

	bool IsSceneFile(const std::filesystem::path& path)
	{
		std::string ext = path.extension().string();
		for (char& c : ext) c = static_cast<char>(tolower(c));
		if (ext != ".json") return false;

		std::string stem = path.stem().string();
		for (char& c : stem) c = static_cast<char>(tolower(c));
		if (stem.ends_with(".scene") || stem.ends_with(".bamscene")) return true;

		// 일반 .json 파일인 경우 루트의 "Scene" 키 존재 여부 확인
		archive::JsonArchive ar;
		if (!ar.LoadFromFile(path)) return false;

		bool hasScene = false;
		return ar.HasField("Scene", hasScene) && hasScene;
	}

	bool OpenSceneFromFile(const std::filesystem::path& path)
	{
		archive::JsonArchive ar;
		const Engine::EResult result = Engine::SceneManager::Get().LoadScene(ar, path.wstring());
		if (result != Engine::EResult::Success)
		{
			ENGINE_LOG_ERROR("Failed to load scene: {} ({})", path.string(), Engine::ResultToString(result));
			return false;
		}

		Editor::SelectionManager::Get().ClearSelection(true);
		Editor::Application::Get().GetEditHistory().Clear();
		ENGINE_LOG_INFO("Successfully loaded scene from ContentBrowser: {}", path.string());
		return true;
	}

	const char* GetBinaryExtension(Engine::EResourceType type)
	{
		using namespace Engine;
		switch (type)
		{
		case EResourceType::Material:         return ".bammat";
		case EResourceType::MaterialInstance: return ".bammatinst";
		case EResourceType::Mesh:             return ".bammesh";
		case EResourceType::Sprite:           return ".bamsprite";
		case EResourceType::Skeleton:         return ".bamskel";
		case EResourceType::Animation:        return ".bamanim";
		case EResourceType::Model:            return ".bammodel";
		case EResourceType::Texture:          return ".bamtex";
		case EResourceType::Shader:           return ".bamshader";
		default:                              return ".beve";
		}
	}

	void HandleItemDoubleClick(const std::filesystem::directory_entry& directoryEntry,
		std::filesystem::path& currentDirectory, char* searchBuffer)
	{
		const auto& path = directoryEntry.path();

		if (directoryEntry.is_directory())
		{
			currentDirectory /= path.filename();
			if (searchBuffer) searchBuffer[0] = '\0';
			return;
		}

		// 1. Scene JSON 파일 더블클릭 시 씬 로드
		if (IsSceneFile(path))
		{
			OpenSceneFromFile(path);
			return;
		}

		// 2. 일반 리소스 에디터 오픈
		Editor::SelectionManager::Get().SetSelectedAsset(path);
		for (auto* panel : Editor::ImGuiManager::Get().GetImGuiPanels())
		{
			if (auto* resEditor = dynamic_cast<Editor::ResourceEditorInterface*>(panel))
			{
				if (resEditor->IsSupported(path))
				{
					resEditor->SetTargetResource(path);
					resEditor->Open();
					break;
				}
			}
		}
	}
}

void ContentBrowserGridItem::Draw(const std::filesystem::directory_entry& directoryEntry, const std::filesystem::path& rootPath, ImTextureID thumbnailTexID, f32 thumbnailSize, f32 padding, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory, char* searchBuffer)
{
	float filledSize = ImGui::GetContentRegionAvail().x;
	const auto& path = directoryEntry.path();
	auto relativePath = path.lexically_relative(rootPath);
	std::filesystem::path payloadPath = rootPath.filename() / relativePath;

	ImGui::PushID(path.string().c_str());

	// 1. 썸네일 및 부가 요소 렌더링
	Thumbnail(directoryEntry, thumbnailTexID, filledSize, padding);
	ToolTip(directoryEntry);
	ContextMenu(path, outRenamingPath, renameBuffer, renameBufferSize);
	DragAndDropTarget(payloadPath);

	// 썸네일 위에서 더블클릭 시 처리
	if (ImGui::IsItemHovered() && MOUSE_BUTTON_DOUBLE_CLICK(EMouseButton::Left))
	{
		HandleItemDoubleClick(directoryEntry, currentDirectory, searchBuffer);
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

void ContentBrowserGridItem::Thumbnail(const std::filesystem::directory_entry& directoryEntry, ImTextureID thumbnailTexID, f32 thumbnailSize, f32 padding)
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
			typeColor = ImVec4(0.7f, 0.3f, 0.9f, 0.4f); // 쉐이더 본체: 퍼플
		}
		else if (ext == ".hlsli")
		{
			icon = ICON_FA_FILE_CODE;
			typeColor = ImVec4(0.25f, 0.6f, 0.85f, 0.4f); // 인클루드 헤더: 스카이블루
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
		ImGui::ImageButton(buttonId.c_str(), thumbnailTexID, { thumbnailSize, thumbnailSize });
	}
	else
	{
		ImGui::Button(buttonId.c_str(), { thumbnailSize, thumbnailSize });
	}

	ImGui::PopStyleVar();
	ImGui::PopStyleColor(3);
}

void ContentBrowserGridItem::ContextMenu(const std::filesystem::path& path,
	std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize)
{
	std::string filenameString = path.filename().string();
	if (ImGui::BeginPopupContextItem())
	{
		if (ImGui::MenuItem((ICON_FA_PEN " " + LOCAL_LABEL("EditorCommon.Rename")).c_str(), "F2"))
		{
			outRenamingPath = path;
			strncpy(renameBuffer, filenameString.c_str(), renameBufferSize);
		}

		if (ImGui::MenuItem((ICON_FA_COPY " " + LOCAL_LABEL("EditorCommon.CopyPath")).c_str(), "Ctrl+C"))
		{
			ImGui::SetClipboardText(path.string().c_str());
		}

		if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.ShowInExplorer")).c_str()))
		{
			if (std::filesystem::is_directory(path))
				ShellExecuteA(NULL, "open", path.string().c_str(), NULL, NULL, SW_SHOWNORMAL);
			else
			{
				std::string param = "/select,\"" + path.string() + "\"";
				ShellExecuteA(NULL, "open", "explorer.exe", param.c_str(), NULL, SW_SHOWNORMAL);
			}
		}

		if (!std::filesystem::is_directory(path))
		{
			std::string filename = path.filename().string();
			for (auto& c : filename) c = static_cast<char>(tolower(c));

			Engine::EResourceType peekedType = Engine::EResourceType::Unknown;
			if (filename.ends_with(".json") || filename.ends_with(".beve"))
				peekedType = PeekResourceType(path);
			if (filename.ends_with(".bammat") || filename.ends_with(".bammatinst") ||
				peekedType == Engine::EResourceType::Material ||
				peekedType == Engine::EResourceType::MaterialInstance)
			{
				ContextMenu_Material(path);
			}
			else if (filename.ends_with(".fbx") || filename.ends_with(".obj") ||
				filename.ends_with(".gltf") || filename.ends_with(".bammesh") ||
				peekedType == Engine::EResourceType::Mesh)
			{
				ContextMenu_Model(path);
			}
			else if (filename.ends_with(".png") || filename.ends_with(".jpg") ||
				filename.ends_with(".tga") || filename.ends_with(".jpeg") || filename.ends_with(".bmp"))
			{
				ContextMenu_Texture(path);
			}
			else if (filename.ends_with(".hlsl") || filename.ends_with(".vert") ||
				filename.ends_with(".frag") || filename.ends_with(".comp") || filename.ends_with(".hlsli"))
			{
				ContextMenu_Shader(path);
			}
		}

		if (!std::filesystem::is_directory(path) && IsSceneFile(path))
		{
			ImGui::Separator();
			if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("UI_LOAD_SCENE")).c_str()))
			{
				OpenSceneFromFile(path);
			}
		}

		// JSON <-> Binary 변환 메뉴
		if (!std::filesystem::is_directory(path))
		{
			std::string filename = path.filename().string();
			for (auto& c : filename) c = static_cast<char>(tolower(c));

			if (filename.ends_with(".json"))
			{
				ImGui::Separator();
				if (ImGui::MenuItem((ICON_FA_FILE_EXPORT " " + LOCAL_LABEL("ContentBrowser.BakeToBinary")).c_str()))
				{
					ConvertJsonToBinary(path);
				}
			}
			else if (filename.find(".bam") != std::string::npos || filename.ends_with(".beve"))
			{
				ImGui::Separator();
				if (ImGui::MenuItem((ICON_FA_FILE_CODE " " + LOCAL_LABEL("ContentBrowser.ExportToJson")).c_str()))
				{
					ConvertBinaryToJson(path);
				}
			}
		}

		ImGui::Separator();

		if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str()))
		{
			std::error_code ec;
			if (std::filesystem::is_directory(path))
				std::filesystem::remove_all(path, ec);
			else
				std::filesystem::remove(path, ec);

			if (ec) ENGINE_LOG_ERROR("Failed to delete asset {}: {}", path.string(), ec.message());
			else ENGINE_LOG_INFO("Deleted asset: {}", path.string());
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
			ImGui::Text("%s", LOCAL_CSTR("ContentBrowser.FolderType"));
		}
		else
		{
			std::string ext = path.extension().string();
			for (auto& c : ext) c = tolower(c);
			ImGui::Text("%s", LOCAL_FMT("ContentBrowser.FileType", ext.c_str()).c_str());

			try
			{
				uintmax_t fileSize = std::filesystem::file_size(path);
				if (fileSize < 1024)
					ImGui::Text("%s", LOCAL_FMT("ContentBrowser.SizeBytes", fileSize).c_str());
				else if (fileSize < 1024 * 1024)
					ImGui::Text("%s", LOCAL_FMT("ContentBrowser.SizeKB", fileSize / 1024).c_str());
				else
					ImGui::Text("%s", LOCAL_FMT("ContentBrowser.SizeMB", (float)fileSize / (1024.0f * 1024.0f)).c_str());
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
		std::wstring wPath = Engine::NormalizePath(relativePath.wstring());

		// 기존 범용 페이로드
		ImGui::SetDragDropPayload(
			"CONTENT_BROWSER_ITEM",
			wPath.c_str(),
			(wPath.length() + 1) * sizeof(wchar_t));

		// One payload per drag; the receiver validates the resolved resource type.

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
		typeStr = LOCAL("ContentBrowser.Folder");
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

	// 네임 박스 위에서 더블클릭 시 처리
	if (ImGui::IsItemHovered() && MOUSE_BUTTON_DOUBLE_CLICK(EMouseButton::Left))
	{
		HandleItemDoubleClick(directoryEntry, currentDirectory, searchBuffer);
	}
}

void ContentBrowserGridItem::ContextMenu_Material(const std::filesystem::path& path)
{
	std::string filename = path.filename().string();
	for (auto& c : filename) c = static_cast<char>(tolower(c));

	const bool isInstance = filename.ends_with(".bammatinst") || filename.ends_with(".bammatinst.json");
	const bool isBaseMat = !isInstance;

	ImGui::Separator();

	// 1. 수동 Reload Material 메뉴 (공통)
	if (ImGui::MenuItem((ICON_FA_ROTATE " " + LOCAL_LABEL("ContentBrowser.ReloadMaterial")).c_str()))
	{
		Engine::ResourceManager& rm = Engine::ResourceManager::Get();
		std::string targetStem = path.stem().string();
		for (char& c : targetStem) c = static_cast<char>(tolower(c));
		if (targetStem.ends_with(".bammat")) targetStem = targetStem.substr(0, targetStem.length() - 7);
		if (targetStem.ends_with(".bammatinst")) targetStem = targetStem.substr(0, targetStem.length() - 11);

		bool reloaded = false;
		auto tryReload = [&](const auto& handles) {
			for (const auto& handle : handles)
			{
				auto* mat = static_cast<Engine::MaterialInterface*>(rm.GetResource(handle));
				if (!mat) continue;

				std::string keyStem = std::filesystem::path(mat->GetKey()).stem().string();
				for (char& c : keyStem) c = static_cast<char>(tolower(c));
				if (keyStem.ends_with(".bammat")) keyStem = keyStem.substr(0, keyStem.length() - 7);
				if (keyStem.ends_with(".bammatinst")) keyStem = keyStem.substr(0, keyStem.length() - 11);

				std::string fileStem = std::filesystem::path(mat->GetPath()).stem().string();
				for (char& c : fileStem) c = static_cast<char>(tolower(c));
				if (fileStem.ends_with(".bammat")) fileStem = fileStem.substr(0, fileStem.length() - 7);
				if (fileStem.ends_with(".bammatinst")) fileStem = fileStem.substr(0, fileStem.length() - 11);

				if (keyStem == targetStem || fileStem == targetStem)
				{
					if (IsSuccess(mat->Reload(path)))
					{
						reloaded = true;
						ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.MaterialReloaded"),
							LOCAL_FMT("Notification.ReloadedFile", path.filename().string()), false, 3.0f);
					}
				}
			}
			};

		tryReload(rm.GetResourceHandles<Engine::Material>());
		tryReload(rm.GetResourceHandles<Engine::MaterialInstance>());

		if (!reloaded)
		{
			ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.MaterialNotLoaded"),
				LOCAL_FMT("Notification.InactiveFile", path.filename().string()), true, 3.0f);
		}
	}

	// 2. Base Material 전용: Material Instance (.bammatinst.json) 생성
	if (isBaseMat)
	{
		if (ImGui::MenuItem((ICON_FA_CLONE " " + LOCAL_LABEL("ContentBrowser.CreateMaterialInstance")).c_str()))
		{
			std::string baseStem = path.stem().string();
			if (baseStem.ends_with(".bammat")) baseStem = baseStem.substr(0, baseStem.length() - 7);

			std::filesystem::path instPath = path.parent_path() / (baseStem + ".bammatinst.json");
			int suffix = 1;
			while (std::filesystem::exists(instPath))
			{
				instPath = path.parent_path() /
					(baseStem + "_" + std::to_string(suffix++) + ".bammatinst.json");
			}

			auto& rm = Engine::ResourceManager::Get();
			std::wstring relBasePath = std::filesystem::relative(path).wstring();
			auto baseHandle = rm.GetResourceHandle<Engine::Material>(relBasePath);
			if (!baseHandle.IsValid())
			{
				baseHandle = static_cast<Engine::ResourceHandle<Engine::Material>>(rm.LoadFile(relBasePath));
			}

			if (baseHandle.IsValid())
			{
				MaterialInstanceDesc desc = {};
				desc.BaseMaterialHandle = baseHandle;
				desc.Key = std::filesystem::relative(instPath).wstring();
				desc.Path = instPath.wstring();
				auto instHandle = rm.LoadResource<Engine::MaterialInstance>(&desc);
				if (instHandle)
				{
					rm.SaveToJsonFile(instHandle.Get(), instPath.wstring());
					ENGINE_LOG_INFO("Created Material Instance (JSON): {}", instPath.string());
					ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.InstanceCreated"),
						instPath.filename().string(), false, 3.0f);
				}
			}
		}
	}

	// 3. Material Instance 전용: Base Material 열기
	if (isInstance)
	{
		if (ImGui::MenuItem((ICON_FA_LINK " " + LOCAL_LABEL("ContentBrowser.OpenBaseMaterial")).c_str()))
		{
			auto& rm = Engine::ResourceManager::Get();
			std::wstring relPath = std::filesystem::relative(path).wstring();
			auto instHandle = rm.GetResourceHandle<Engine::MaterialInstance>(relPath);
			if (!instHandle.IsValid())
			{
				instHandle = static_cast<Engine::ResourceHandle<Engine::MaterialInstance>>(rm.LoadFile(relPath));
			}

			if (instHandle.IsValid() && instHandle.Get() && instHandle.Get()->GetBaseMaterial())
			{
				std::filesystem::path baseMatPath = instHandle.Get()->GetBaseMaterial()->GetPath();
				if (baseMatPath.empty())
					baseMatPath = instHandle.Get()->GetBaseMaterial()->GetKey();

				// 디스크에 .bammat.json 파일이 존재하는지 우선 탐색
				if (!std::filesystem::exists(baseMatPath))
				{
					if (std::filesystem::exists(baseMatPath.string() + ".bammat.json"))
						baseMatPath = baseMatPath.string() + ".bammat.json";
					else if (std::filesystem::exists(baseMatPath.string() + ".json"))
						baseMatPath = baseMatPath.string() + ".json";
				}

				if (std::filesystem::exists(baseMatPath))
				{
					SelectionManager::Get().SetSelectedAsset(baseMatPath);
					for (auto* panel : ImGuiManager::Get().GetImGuiPanels())
					{
						if (auto* resEditor = dynamic_cast<ResourceEditorInterface*>(panel))
						{
							if (resEditor->IsSupported(baseMatPath))
							{
								resEditor->SetTargetResource(baseMatPath);
								resEditor->Open();
								break;
							}
						}
					}
					ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.OpenBaseMaterial"),
						baseMatPath.filename().string(), false, 2.0f);
				}
				else
				{
					ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.BaseMaterialNotFound"),
						baseMatPath.string(), true, 3.0f);
				}
			}
		}
	}
}

void ContentBrowserGridItem::ContextMenu_Model(const std::filesystem::path& path)
{
	std::string ext = path.extension().string();
	for (auto& c : ext) c = static_cast<char>(tolower(c));

	ImGui::Separator();

	if (ext == ".bammesh")
	{
		if (ImGui::MenuItem((ICON_FA_ROTATE " " + LOCAL_LABEL("ContentBrowser.ReloadMesh")).c_str()))
		{
			Engine::ResourceManager& rm = Engine::ResourceManager::Get();
			auto meshHandles = rm.GetResourceHandles<Engine::Mesh>();

			std::string targetStem = path.stem().string();
			for (char& c : targetStem) c = static_cast<char>(tolower(c));
			if (targetStem.ends_with(".bammesh")) targetStem = targetStem.substr(0, targetStem.length() - 8);

			bool reloaded = false;
			for (const auto& handle : meshHandles)
			{
				auto* mesh = static_cast<Engine::Mesh*>(rm.GetResource(handle));
				if (!mesh) continue;

				std::string keyStem = std::filesystem::path(mesh->GetKey()).stem().string();
				for (char& c : keyStem) c = static_cast<char>(tolower(c));
				if (keyStem.ends_with(".bammesh")) keyStem = keyStem.substr(0, keyStem.length() - 8);

				std::string fileStem = std::filesystem::path(mesh->GetPath()).stem().string();
				for (char& c : fileStem) c = static_cast<char>(tolower(c));
				if (fileStem.ends_with(".bammesh")) fileStem = fileStem.substr(0, fileStem.length() - 8);

				if (keyStem == targetStem || fileStem == targetStem)
				{
					if (IsSuccess(mesh->Reload(path)))
					{
						reloaded = true;
						ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.MeshReloaded"),
							LOCAL_FMT("Notification.ReloadedFile", path.filename().string()), false, 3.0f);
					}
				}
			}
			if (!reloaded)
			{
				ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.MeshNotLoaded"),
					LOCAL_FMT("Notification.InactiveFile", path.filename().string()), true, 3.0f);
			}
		}
	}
	else if (ext == ".fbx" || ext == ".obj" || ext == ".gltf")
	{
		if (ImGui::MenuItem((ICON_FA_FILE_IMPORT " " + LOCAL_LABEL("ContentBrowser.Reimport")).c_str()))
		{
			ModelImporter* importer = ModelImporter::Create();
			if (importer)
			{
				tagModelImportDesc desc = {};
				const EResult res = importer->Import(path, path.parent_path(), &desc);
				Safe_Release(importer);

				if (IsSuccess(res))
				{
					ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.ModelReimported"),
						LOCAL_FMT("Notification.ReimportedFile", path.filename().string()), false, 3.0f);
				}
				else
				{
					ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.ReimportFailed"),
						LOCAL_FMT("Notification.ReimportFailedFile", path.filename().string()), true, 3.0f);
				}
			}
		}
	}
}

void ContentBrowserGridItem::ContextMenu_Texture(const std::filesystem::path& path)
{
	ImGui::Separator();
	if (ImGui::MenuItem((ICON_FA_ROTATE " " + LOCAL_LABEL("ContentBrowser.ReloadTexture")).c_str()))
	{
		Engine::ResourceManager& rm = Engine::ResourceManager::Get();
		auto texHandles = rm.GetResourceHandles<Engine::Texture>();
		std::string targetStem = path.stem().string();
		for (char& c : targetStem) c = static_cast<char>(tolower(c));
		bool reloaded = false;
		for (const auto& handle : texHandles)
		{
			auto* tex = static_cast<Engine::Texture*>(rm.GetResource(handle));
			if (!tex) continue;
			std::string keyStem = std::filesystem::path(tex->GetKey()).stem().string();
			for (char& c : keyStem) c = static_cast<char>(tolower(c));
			std::string fileStem = std::filesystem::path(tex->GetPath()).stem().string();
			for (char& c : fileStem) c = static_cast<char>(tolower(c));
			if (keyStem == targetStem || fileStem == targetStem)
			{
				if (IsSuccess(tex->Reload(path)))
				{
					reloaded = true;
					ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.TextureReloaded"),
						LOCAL_FMT("Notification.ReloadedFile", path.filename().string()), false, 3.0f);
				}
			}
		}
		if (!reloaded)
		{
			ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.TextureNotLoaded"),
				LOCAL_FMT("Notification.InactiveFile", path.filename().string()), true, 3.0f);
		}
	}
	if (ImGui::MenuItem((ICON_FA_IMAGE " " + LOCAL_LABEL("ContentBrowser.CreateSprite")).c_str()))
	{
		// 1. 중복 없는 .bamsprite.json 경로 결정
		std::filesystem::path spritePath = path.parent_path() / (path.stem().string() + ".bamsprite.json");
		int suffix = 1;
		while (std::filesystem::exists(spritePath))
		{
			spritePath = path.parent_path() / (path.stem().string() + "_" + std::to_string(suffix) + ".bamsprite.json");
			++suffix;
		}

		std::wstring relTexPath = std::filesystem::relative(path).wstring();
		auto& rm = Engine::ResourceManager::Get();
		auto texHandle = rm.LoadFile(relTexPath);

		if (texHandle.IsValid())
		{
			// 2. 텍스처를 래핑하는 임시 Sprite 객체 생성
			SpriteCreateDesc desc = {};
			desc.texture = static_cast<Engine::ResourceHandle<Engine::Texture>>(texHandle);
			desc.Key = std::filesystem::relative(spritePath).wstring();
			desc.Path = spritePath.wstring();
			desc.pivot = vec2(0.5f, 0.5f);
			desc.region = Rect(0.f, 0.f, 0.f, 0.f);

			Engine::Sprite* newSprite = Engine::Sprite::Create(&desc);
			if (newSprite)
			{
				// 3. 디스크에 .bamsprite.json 파일로 먼저 저장
				if (rm.SaveToJsonFile(newSprite, spritePath.wstring()) == EResult::Success)
				{
					// 4. 저장된 파일을 ResourceManager에 정식 로드 등록
					rm.LoadFile(std::filesystem::relative(spritePath).wstring());
					ENGINE_LOG_INFO("Successfully created sprite: {}", spritePath.string());
				}
				rm.DestroyResource(newSprite); // 임시 객체 해제
			}
		}
	}
}

void ContentBrowserGridItem::ContextMenu_Shader(const std::filesystem::path& path)
{
	std::string ext = path.extension().string();
	for (auto& c : ext) c = static_cast<char>(tolower(c));

	ImGui::Separator();

	if (ext == ".hlsli")
	{
		if (ImGui::MenuItem((ICON_FA_ROTATE " " + LOCAL_LABEL("ContentBrowser.RecompileAllShaders")).c_str()))
		{
			ShaderHotReloadManager::Get().RequestRecompileAll();
		}
	}
	else
	{
		if (ImGui::MenuItem((ICON_FA_CODE " " + LOCAL_LABEL("ContentBrowser.RecompileShader")).c_str()))
		{
			ShaderHotReloadManager::Get().RecompileFile(path);
		}
	}
}

bool ContentBrowserGridItem::ConvertJsonToBinary(const std::filesystem::path& path)
{
	// 1. 파일 헤더로부터 실제 리소스 타입 감지 (파일명 무관)
	Engine::EResourceType assetType = PeekResourceType(path);

	// 2. 해당 타입에 맞는 표준 바이너리 확장자 결정
	std::string binExt = GetBinaryExtension(assetType);

	// 3. 출력 경로 결정 (Wood.json -> Wood.bammat, foo.bammat.json -> foo.bammat)
	std::string baseStem = path.stem().string();
	if (baseStem.ends_with(binExt))
		baseStem = baseStem.substr(0, baseStem.length() - binExt.length());

	std::filesystem::path outPath = path.parent_path() / (baseStem + binExt);

	auto& rm = Engine::ResourceManager::Get();
	std::wstring relPath = std::filesystem::relative(path).wstring();
	Engine::Handle handle = rm.LoadFile(relPath);
	Engine::Resource* res = rm.GetResource(handle);
	if (!res)
	{
		ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.BakeFailed"),
			LOCAL_FMT("Notification.JsonLoadFailed", path.filename().string()), true, 3.0f);
		return false;
	}

	const EResult result = rm.SaveToBeveFile(res, outPath.wstring());
	if (IsSuccess(result))
	{
		ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.BakeSucceeded"),
			LOCAL_FMT("Notification.CreatedFile", outPath.filename().string()), false, 3.0f);
		ENGINE_LOG_INFO("[Asset Baking] Baked {} (Type: {}) -> {}",
			path.string(), static_cast<uint32>(assetType), outPath.string());
		return true;
	}

	ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.BakeFailed"),
		LOCAL_FMT("Notification.BinarySaveFailed", outPath.filename().string()), true, 3.0f);
	return false;
}

bool ContentBrowserGridItem::ConvertBinaryToJson(const std::filesystem::path& path)
{
	std::filesystem::path outPath = path.parent_path() / (path.filename().string() + ".json");

	auto& rm = Engine::ResourceManager::Get();
	std::wstring relPath = std::filesystem::relative(path).wstring();
	Engine::Handle handle = rm.LoadFile(relPath);
	Engine::Resource* res = rm.GetResource(handle);
	if (!res)
	{
		ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.ExportFailed"),
			LOCAL_FMT("Notification.BinaryLoadFailed", path.filename().string()), true, 3.0f);
		return false;
	}

	const EResult result = rm.SaveToJsonFile(res, outPath.wstring());
	if (IsSuccess(result))
	{
		ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.ExportSucceeded"),
			LOCAL_FMT("Notification.CreatedFile", outPath.filename().string()), false, 3.0f);
		ENGINE_LOG_INFO("[Asset Export] Successfully exported binary to JSON: {} -> {}",
			path.string(), outPath.string());
		return true;
	}

	ShaderHotReloadManager::Get().AddNotification(LOCAL("Notification.ExportFailed"),
		LOCAL_FMT("Notification.JsonSaveFailed", outPath.filename().string()), true, 3.0f);
	return false;
}