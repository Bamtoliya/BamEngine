#include "TilePalettePanel.h"
#include "ImGuiManager.h"
#include "ResourceManager.h"
#include "NotificationManager.h"
#include "Functions.h"
#include <IconsFontAwesome7.h>
#include <algorithm>

namespace Editor
{

TilePalettePanel::TilePalettePanel()
{
	m_Name = L"Tile Palette";
	m_DisplayNameKey = "TilePalette.Title";
}

void TilePalettePanel::AddAssetToPalette(const std::filesystem::path& assetPath)
{
	auto& rm = Engine::ResourceManager::Get();
	const std::wstring normPath = Engine::NormalizePath(assetPath.wstring());
	const std::wstring ext = assetPath.extension().wstring();
	const std::string lowerExt = Engine::ToLowerStr(Engine::WStrToStr(ext));
	const std::string filename = Engine::ToLowerStr(assetPath.filename().string());

	int addedCount = 0;

	// 1. 스프라이트 에셋 (.bamsprite, .bamsprite.json)
	if (lowerExt == ".bamsprite" || filename.ends_with(".bamsprite.json"))
	{
		auto handle = rm.GetResourceHandle<Engine::Sprite>(normPath);
		if (!handle.IsValid())
		{
			auto raw = rm.LoadFile(normPath);
			handle = static_cast<ResourceHandle<Engine::Sprite>>(raw);
		}

		if (handle.IsValid())
		{
			if (std::find(m_PaletteSprites.begin(), m_PaletteSprites.end(), handle) == m_PaletteSprites.end())
			{
				m_PaletteSprites.push_back(handle);
				m_SelectedSprite = handle;
				addedCount++;
			}
		}
	}
	// 2. 텍스처 파일 (.png, .jpg, .bamtex 등)
	else if (lowerExt == ".png" || lowerExt == ".jpg" || lowerExt == ".jpeg" ||
		lowerExt == ".tga" || lowerExt == ".bmp" || lowerExt == ".dds" ||
		lowerExt == ".bamtex" || filename.ends_with(".bamtex.json"))
	{
		auto rawTex = rm.LoadFile(normPath);
		auto texHandle = static_cast<ResourceHandle<Engine::Texture>>(rawTex);
		if (texHandle.IsValid() && texHandle.Get())
		{
			m_PaletteTexture = texHandle;

			// A. 동일 폴더 내에 이 텍스처를 기반으로 슬라이스된 기존 .bamsprite.json 파일들 탐색
			std::filesystem::path parentDir = assetPath.parent_path();
			std::string stemName = assetPath.stem().string();
			// .bamtex.json의 경우 한 번 더 stem
			if (filename.ends_with(".bamtex.json"))
			{
				stemName = std::filesystem::path(stemName).stem().string();
			}

			std::error_code ec;
			if (std::filesystem::exists(parentDir, ec))
			{
				for (const auto& entry : std::filesystem::directory_iterator(parentDir, ec))
				{
					if (!entry.is_regular_file()) continue;
					const auto entryName = entry.path().filename().string();
					if (entryName.starts_with(stemName) &&
						(entryName.ends_with(".bamsprite.json") || entry.path().extension() == ".bamsprite"))
					{
						const std::wstring spriteNorm = Engine::NormalizePath(entry.path().wstring());
						auto sHandle = rm.GetResourceHandle<Engine::Sprite>(spriteNorm);
						if (!sHandle.IsValid())
						{
							sHandle = static_cast<ResourceHandle<Engine::Sprite>>(rm.LoadFile(spriteNorm));
						}
						if (sHandle.IsValid())
						{
							if (std::find(m_PaletteSprites.begin(), m_PaletteSprites.end(), sHandle) == m_PaletteSprites.end())
							{
								m_PaletteSprites.push_back(sHandle);
								m_SelectedSprite = sHandle;
								addedCount++;
							}
						}
					}
				}
			}

			// B. 슬라이스된 스프라이트 파일이 없으면 전체 텍스처를 사용하는 기본 스프라이트 1개 생성 및 등록
			if (addedCount == 0)
			{
				auto* tex = texHandle.Get();
				auto* rhi = tex ? tex->GetRHITexture() : nullptr;
				const float w = rhi ? static_cast<float>(rhi->GetWidth()) : 32.0f;
				const float h = rhi ? static_cast<float>(rhi->GetHeight()) : 32.0f;

				SpriteCreateDesc desc;
				desc.texture = texHandle;
				desc.Key = normPath + L"_DefaultSprite";
				desc.Path = normPath;
				desc.region = Engine::Rect(0.0f, 0.0f, w, h);
				desc.pivot = vec2(0.5f, 0.5f);

				Engine::Sprite* newSprite = Engine::Sprite::Create(&desc);
				if (newSprite)
				{
					auto sHandle = rm.AddResource<Engine::Sprite>(desc.Key, newSprite);
					if (sHandle.IsValid())
					{
						if (std::find(m_PaletteSprites.begin(), m_PaletteSprites.end(), sHandle) == m_PaletteSprites.end())
						{
							m_PaletteSprites.push_back(sHandle);
							m_SelectedSprite = sHandle;
							addedCount++;
						}
					}
				}
			}
		}
	}

	if (addedCount > 0)
	{
		NotificationManager::Get().AddNotification(LOCAL("TilePalette.Title"),
			LOCAL_FMT("TilePalette.AddedTiles", addedCount), ELogLevel::Info, 3.0f);
	}
}

void TilePalettePanel::Draw()
{
	if (!m_Open) return;

	std::string title = LOCAL("TilePalette.Title") + "###" + WStrToStr(m_Name);
	ImGui::SetNextWindowSize(ImVec2(340.0f, 420.0f), ImGuiCond_FirstUseEver);

	if (BeginPanelWindow(title.c_str(), m_Open))
	{
		DrawToolbar();
		ImGui::Separator();
		DrawPaletteGrid();
	}
	ImGui::End();
}

void TilePalettePanel::DrawToolbar()
{
	// 1. 브러시 모드 토글 버튼들
	const bool isPaint = (m_BrushMode == ETileBrushMode::Paint);
	const bool isErase = (m_BrushMode == ETileBrushMode::Erase);
	const bool isBox = (m_BrushMode == ETileBrushMode::BoxFill);
	const bool isPicker = (m_BrushMode == ETileBrushMode::Picker);

	if (isPaint) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
	if (ImGui::Button(ICON_FA_PAINTBRUSH "##Paint")) m_BrushMode = ETileBrushMode::Paint;
	if (isPaint) ImGui::PopStyleColor();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("TilePalette.PaintTool"));

	ImGui::SameLine();
	if (isErase) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
	if (ImGui::Button(ICON_FA_ERASER "##Erase")) m_BrushMode = ETileBrushMode::Erase;
	if (isErase) ImGui::PopStyleColor();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("TilePalette.EraseTool"));

	ImGui::SameLine();
	if (isBox) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
	if (ImGui::Button(ICON_FA_SQUARE "##BoxFill")) m_BrushMode = ETileBrushMode::BoxFill;
	if (isBox) ImGui::PopStyleColor();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("TilePalette.BoxTool"));

	ImGui::SameLine();
	if (isPicker) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
	if (ImGui::Button(ICON_FA_EYE_DROPPER "##Picker")) m_BrushMode = ETileBrushMode::Picker;
	if (isPicker) ImGui::PopStyleColor();
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("TilePalette.PickerTool"));

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	// 썸네일 크기 슬라이더
	ImGui::SetNextItemWidth(70.0f);
	ImGui::SliderFloat("##TileSize", &m_TileThumbnailSize, 24.0f, 96.0f, "%.0fpx");
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("TilePalette.ThumbnailSize"));

	ImGui::SameLine();
	if (!m_PaletteSprites.empty())
	{
		if (ImGui::Button(ICON_FA_TRASH_CAN "##ClearPalette"))
		{
			m_PaletteSprites.clear();
			m_SelectedSprite = {};
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Clear"));
	}
}

void TilePalettePanel::DrawPaletteGrid()
{
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	const int cols = (std::max)(1, static_cast<int>((avail.x + spacing) / (m_TileThumbnailSize + spacing)));

	// 전체 패널 영역에서 드래그 앤 드롭 지원 (CONTENT_BROWSER_ITEM 및 ASSET_PATH)
	auto checkAndHandleDragDrop = [this]()
	{
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
			{
				const wchar_t* pathStr = static_cast<const wchar_t*>(payload->Data);
				AddAssetToPalette(std::filesystem::path(pathStr));
			}
			else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				const wchar_t* pathStr = static_cast<const wchar_t*>(payload->Data);
				AddAssetToPalette(std::filesystem::path(pathStr));
			}
			ImGui::EndDragDropTarget();
		}
	};

	if (m_PaletteSprites.empty())
	{
		const ImVec2 startPos = ImGui::GetCursorScreenPos();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const ImVec2 boxMax(startPos.x + avail.x, startPos.y + (std::max)(120.0f, avail.y));

		// 드롭 안내 점선 박스 시각화
		drawList->AddRect(startPos, boxMax, IM_COL32(100, 100, 100, 150), 6.0f, ImDrawFlags_None, 1.5f);

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30.0f);
		ImGui::TextWrapped("  %s %s", ICON_FA_ARROW_DOWN, LOCAL_CSTR("TilePalette.NoSprites"));

		ImGui::Dummy(avail);
		checkAndHandleDragDrop();
		return;
	}

	ImGui::BeginChild("##TileGridChild", avail, false);
	checkAndHandleDragDrop();

	int spriteToRemove = -1;
	for (int i = 0; i < static_cast<int>(m_PaletteSprites.size()); ++i)
	{
		auto& spriteHandle = m_PaletteSprites[i];
		if (!spriteHandle.IsValid() || !spriteHandle.Get()) continue;

		auto* sprite = spriteHandle.Get();
		auto* texture = sprite->GetTexture();
		auto* rhiTex = texture ? texture->GetRHITexture() : nullptr;
		if (!rhiTex) continue;

		ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(rhiTex);
		const vec4 uv = sprite->GetUVTransform();
		const bool isSelected = (m_SelectedSprite == spriteHandle);

		ImGui::PushID(i);
		if (isSelected)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.9f, 0.8f, 0.2f, 0.4f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.9f, 0.2f, 1.0f));
		}

		if (ImGui::ImageButton("##TileBtn", texID, ImVec2(m_TileThumbnailSize, m_TileThumbnailSize),
			ImVec2(uv.x, uv.y), ImVec2(uv.x + uv.z, uv.y + uv.w)))
		{
			m_SelectedSprite = spriteHandle;
		}

		if (isSelected)
		{
			ImGui::PopStyleColor(2);
		}

		// 타일 썸네일 우클릭 컨텍스트 메뉴 (개별 삭제 지원)
		if (ImGui::BeginPopupContextItem())
		{
			if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.Delete")))
			{
				spriteToRemove = i;
			}
			ImGui::EndPopup();
		}

		ImGui::PopID();

		if ((i + 1) % cols != 0 && (i + 1) < static_cast<int>(m_PaletteSprites.size()))
		{
			ImGui::SameLine();
		}
	}

	if (spriteToRemove >= 0 && spriteToRemove < static_cast<int>(m_PaletteSprites.size()))
	{
		if (m_SelectedSprite == m_PaletteSprites[spriteToRemove])
		{
			m_SelectedSprite = {};
		}
		m_PaletteSprites.erase(m_PaletteSprites.begin() + spriteToRemove);
	}

	checkAndHandleDragDrop();
	ImGui::EndChild();
}

} // namespace Editor
