#pragma once
#include "SpriteEditor.h"
#include "Functions.h"
#include "ResourceManager.h"
#include "NotificationManager.h"
#include "ImGuiManager.h"
#include "InputManager.h"
#include "ImGuiSplitter.h"
#include "FileDialogs.h"
#include <IconsFontAwesome7.h>
#include <sstream>
#include <algorithm>

namespace Editor
{

SpriteEditor::SpriteEditor()
{
	m_Name = L"Sprite Editor";
	m_DisplayNameKey = "ResourceEditor.SpriteTitle";
}

bool SpriteEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	const auto ext = assetPath.extension().wstring();
	const std::string lower = Engine::ToLowerStr(Engine::WStrToStr(assetPath.filename().wstring()));
	return ext == L".bamsprite" || lower.ends_with(".bamsprite.json") ||
		ext == L".bamtex" || lower.ends_with(".bamtex.json") ||
		ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".tga";
}

void SpriteEditor::SetTargetResource(const std::filesystem::path& assetPath)
{
	ResourceEditorInterface::SetTargetResource(assetPath);
	LoadTextureAndSlices(assetPath);
}

void SpriteEditor::LoadTextureAndSlices(const std::filesystem::path& path)
{
	m_Slices.clear();
	m_SelectedSliceIndex = -1;
	m_IsDirty = false;
	m_TextureHandle = {};

	auto& rm = Engine::ResourceManager::Get();
	const std::wstring normPath = Engine::NormalizePath(path.wstring());
	const std::wstring ext = path.extension().wstring();
	const std::string lowerExt = Engine::ToLowerStr(Engine::WStrToStr(ext));
	const std::string filename = Engine::ToLowerStr(path.filename().string());

	if (lowerExt == ".bamsprite" || filename.ends_with(".bamsprite.json"))
	{
		// 1. 스프라이트 에셋을 연 경우
		auto spriteHandle = rm.GetResourceHandle<Engine::Sprite>(normPath);
		if (!spriteHandle.IsValid())
		{
			auto raw = rm.LoadFile(normPath);
			spriteHandle = static_cast<ResourceHandle<Engine::Sprite>>(raw);
		}

		if (spriteHandle.IsValid() && spriteHandle.Get())
		{
			auto* sprite = spriteHandle.Get();
			m_TextureHandle = sprite->GetTextureHandle();
			if (!m_TextureHandle.IsValid() || !m_TextureHandle.Get())
			{
				std::filesystem::path parentDir = path.parent_path();
				for (const auto& texExt : { ".png", ".jpg", ".jpeg", ".tga", ".bmp", ".dds" })
				{
					std::filesystem::path cand = parentDir / (path.stem().string() + texExt);
					if (std::filesystem::exists(cand))
					{
						auto rawTex = rm.LoadFile(cand.wstring());
						m_TextureHandle = static_cast<ResourceHandle<Engine::Texture>>(rawTex);
						if (m_TextureHandle.IsValid()) break;
					}
				}
			}

			SpriteSliceInfo info;
			info.name = path.stem().string();
			if (Engine::ToLowerStr(info.name).ends_with(".bamsprite"))
				info.name = info.name.substr(0, info.name.size() - 10);
			info.region = sprite->GetRegion();
			info.pivot = sprite->GetPivot();
			info.border = sprite->GetBorder();
			m_Slices.push_back(info);
			m_SelectedSliceIndex = 0;
		}
	}
	else
	{
		// 2. 텍스처 에셋(.png, .jpg, .bamtex 등)을 직접 연 경우 - LoadFile로 이미지 디코드 및 RHI 텍스처 생성
		auto raw = rm.LoadFile(normPath);
		m_TextureHandle = static_cast<ResourceHandle<Engine::Texture>>(raw);
		if (!m_TextureHandle.IsValid())
		{
			m_TextureHandle = rm.GetResourceHandle<Engine::Texture>(normPath);
		}
	}

	// 텍스처가 유효한 경우, 기존 슬라이스 스캔 또는 기본 전체 슬라이스 등록
	if (m_TextureHandle.IsValid() && m_TextureHandle.Get() && m_Slices.empty())
	{
		auto* texture = m_TextureHandle.Get();
		auto* rhiTexture = texture->GetRHITexture();
		const float texW = rhiTexture ? static_cast<float>(rhiTexture->GetWidth()) : 0.0f;
		const float texH = rhiTexture ? static_cast<float>(rhiTexture->GetHeight()) : 0.0f;

		// A. 동일 폴더 내에 기존에 분할 저장된 .bamsprite.json 파일들이 있는지 스캔
		std::filesystem::path parentDir = path.parent_path();
		std::string stemName = path.stem().string();
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
					if (sHandle.IsValid() && sHandle.Get())
					{
						auto* sp = sHandle.Get();
						SpriteSliceInfo slice;
						slice.name = entry.path().stem().string();
						if (Engine::ToLowerStr(slice.name).ends_with(".bamsprite"))
							slice.name = slice.name.substr(0, slice.name.size() - 10);
						slice.region = sp->GetRegion();
						slice.pivot = sp->GetPivot();
						slice.border = sp->GetBorder();
						m_Slices.push_back(slice);
					}
				}
			}
		}

		// B. 스캔된 기존 슬라이스가 없으면 기본 1개 전체 영역 슬라이스 생성
		if (m_Slices.empty())
		{
			SpriteSliceInfo defaultSlice;
			defaultSlice.name = path.stem().string() + "_0";
			defaultSlice.region = { 0.0f, 0.0f, texW, texH };
			defaultSlice.pivot = { 0.5f, 0.5f };
			defaultSlice.border = { 0.0f, 0.0f, 0.0f, 0.0f };
			m_Slices.push_back(defaultSlice);
		}
		m_SelectedSliceIndex = 0;
	}
}

void SpriteEditor::FitView(float canvasW, float canvasH, float texW, float texH)
{
	if (texW <= 0.0f || texH <= 0.0f || canvasW <= 0.0f || canvasH <= 0.0f) return;
	const float margin = 40.0f;
	const float scaleX = (canvasW - margin) / texW;
	const float scaleY = (canvasH - margin) / texH;
	m_Zoom = std::clamp((std::min)(scaleX, scaleY), 0.1f, 16.0f);
	m_PanOffset = { 0.0f, 0.0f };
}

void SpriteEditor::Draw()
{
	if (!m_Open) return;

	std::string title = LOCAL("ResourceEditor.SpriteTitle");
	if (!m_TargetResourcePath.empty())
		title += " - " + m_TargetResourcePath.filename().string();
	if (m_IsDirty) title += " *";
	title += "###" + WStrToStr(m_Name);

	ImGui::SetNextWindowSize(ImVec2(900.0f, 650.0f), ImGuiCond_FirstUseEver);
	if (BeginPanelWindow(title.c_str(), m_Open, ImGuiWindowFlags_MenuBar))
	{
		DrawMenuBar();
		DrawToolbar();

		const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		const float splitterThickness = 4.0f;
		const float minCanvasW = 200.0f;
		const float minSidebarW = 220.0f;
		float canvasWidth = (std::max)(minCanvasW, contentAvail.x - m_SidebarWidth - splitterThickness - ImGui::GetStyle().ItemSpacing.x);

		// [좌측] 인터랙티브 캔버스
		DrawCanvas(canvasWidth, contentAvail.y);
		ImGui::SameLine();

		// [가변 스플리터]
		SplitterV("##SpriteEditorSplitter", &canvasWidth, &m_SidebarWidth, minCanvasW, minSidebarW, contentAvail.y, splitterThickness);
		ImGui::SameLine();

		// [우측] 사이드바 (슬라이스 디테일 및 목록)
		DrawSidebar(m_SidebarWidth, contentAvail.y);

		// 팝업 모달
		DrawSliceModal();
		DrawCreateAnimationModal();
	}
	ImGui::End();
}

void SpriteEditor::DrawMenuBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu(LOCAL_CSTR("EditorMenu.File")))
		{
			if (ImGui::MenuItem((std::string(ICON_FA_FOLDER_OPEN) + " " + LOCAL("SpriteEditor.OpenTexture")).c_str()))
			{
				std::wstring selectedPath;
				if (FileDialogs::OpenFileDialog(selectedPath, { { L"Image / Sprite Files", L"*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.dds;*.bamsprite;*.bamtex" } }))
				{
					SetTargetResource(selectedPath);
				}
			}
			ImGui::Separator();
			if (ImGui::MenuItem(LOCAL_CSTR("SpriteEditor.ApplySlices"), nullptr, false, !m_Slices.empty()))
			{
				SaveSlices();
			}
			ImGui::Separator();
			if (ImGui::MenuItem(LOCAL_CSTR("EditorCommon.Close")))
			{
				m_Open = false;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(LOCAL_CSTR("SpriteEditor.SliceMethod")))
		{
			if (ImGui::MenuItem(LOCAL_CSTR("SpriteEditor.SliceModalTitle")))
			{
				m_ShowSliceModal = true;
			}
			if (ImGui::MenuItem(LOCAL_CSTR("SpriteEditor.ClearSlices"), nullptr, false, !m_Slices.empty()))
			{
				m_Slices.clear();
				m_SelectedSliceIndex = -1;
				m_IsDirty = true;
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu(LOCAL_CSTR("ResourceEditor.SpriteAnimationTitle")))
		{
			if (ImGui::MenuItem(LOCAL_CSTR("SpriteEditor.CreateAnimModalTitle"), nullptr, false, !m_Slices.empty()))
			{
				m_ShowCreateAnimModal = true;
			}
			ImGui::EndMenu();
		}

		ImGui::EndMenuBar();
	}
}

void SpriteEditor::DrawToolbar()
{
	if (ImGui::Button((std::string(ICON_FA_FOLDER_OPEN) + " " + LOCAL("SpriteEditor.OpenTexture")).c_str()))
	{
		std::wstring selectedPath;
		if (FileDialogs::OpenFileDialog(selectedPath, { { L"Image / Sprite Files", L"*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.dds;*.bamsprite;*.bamtex" } }))
		{
			SetTargetResource(selectedPath);
		}
	}
	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	ImGui::BeginDisabled(m_Slices.empty());
	if (ImGui::Button((std::string(ICON_FA_FLOPPY_DISK) + " " + LOCAL("SpriteEditor.ApplySlices")).c_str()))
	{
		SaveSlices();
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button((std::string(ICON_FA_SCISSORS) + " " + LOCAL("SpriteEditor.SliceModalTitle")).c_str()))
	{
		m_ShowSliceModal = true;
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(m_Slices.empty());
	if (ImGui::Button((std::string(ICON_FA_FILM) + " " + LOCAL("SpriteEditor.CreateAnimModalTitle")).c_str()))
	{
		m_ShowCreateAnimModal = true;
	}
	ImGui::EndDisabled();

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	auto* texture = m_TextureHandle.Get();
	auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;
	const float texW = rhiTexture ? static_cast<float>(rhiTexture->GetWidth()) : 0.0f;
	const float texH = rhiTexture ? static_cast<float>(rhiTexture->GetHeight()) : 0.0f;

	if (ImGui::Button((std::string(ICON_FA_EXPAND) + " " + LOCAL("SpriteEditor.FitView")).c_str()))
	{
		const ImVec2 contentAvail = ImGui::GetContentRegionAvail();
		FitView(contentAvail.x - m_SidebarWidth - 10.0f, contentAvail.y, texW, texH);
	}

	ImGui::SameLine();
	if (ImGui::Button((std::string(ICON_FA_MAGNIFYING_GLASS) + " " + LOCAL("SpriteEditor.OneToOne")).c_str()))
	{
		m_Zoom = 1.0f;
		m_PanOffset = { 0.0f, 0.0f };
	}

	ImGui::SameLine();
	ImGui::SetNextItemWidth(100.0f);
	ImGui::SliderFloat("##Zoom", &m_Zoom, 0.1f, 8.0f, "%.1fx");

	ImGui::SameLine();
	ImGui::TextDisabled("|");
	ImGui::SameLine();

	// 상태 배지
	ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), LOCAL_FMT("SpriteEditor.SliceCountFormat", m_Slices.size()).c_str());

	if (rhiTexture)
	{
		ImGui::SameLine();
		ImGui::TextDisabled(LOCAL_CSTR("SpriteEditor.ResolutionFormat"), rhiTexture->GetWidth(), rhiTexture->GetHeight());
	}

	ImGui::Separator();
}

void SpriteEditor::DrawCanvas(float availWidth, float availHeight)
{
	ImGui::BeginChild("##SpriteCanvasChild", ImVec2(availWidth, availHeight), true,
		ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

	const ImVec2 canvasPos = ImGui::GetCursorScreenPos();
	const ImVec2 canvasSize = ImGui::GetContentRegionAvail();
	ImDrawList* drawList = ImGui::GetWindowDrawList();

	// 1. 체커보드 배경 렌더링
	const ImU32 colBg1 = IM_COL32(32, 32, 32, 255);
	const ImU32 colBg2 = IM_COL32(44, 44, 44, 255);
	const float checkSize = 16.0f;
	drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), colBg1);
	for (float y = 0.0f; y < canvasSize.y; y += checkSize)
	{
		for (float x = 0.0f; x < canvasSize.x; x += checkSize)
		{
			if (static_cast<int>((x / checkSize) + (y / checkSize)) % 2 == 1)
			{
				drawList->AddRectFilled(
					ImVec2(canvasPos.x + x, canvasPos.y + y),
					ImVec2((std::min)(canvasPos.x + x + checkSize, canvasPos.x + canvasSize.x),
						(std::min)(canvasPos.y + y + checkSize, canvasPos.y + canvasSize.y)),
					colBg2);
			}
		}
	}

	// 2. 마우스 줌 & 팬 (InputManager 일원화)
	auto& input = Engine::InputManager::Get();
	if (ImGui::IsWindowHovered())
	{
		if (input.IsMouseButtonDragging(Engine::EMouseButton::Right) || input.IsMouseButtonDragging(Engine::EMouseButton::Middle))
		{
			const auto delta = input.GetMouseDelta();
			m_PanOffset.x += delta.x;
			m_PanOffset.y += delta.y;
		}
		const float wheel = input.GetMouseScrollDeltaY();
		if (wheel != 0.0f)
		{
			m_Zoom = std::clamp(m_Zoom + wheel * 0.2f, 0.1f, 16.0f);
		}
	}

	auto* texture = m_TextureHandle.Get();
	auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;

	if (rhiTexture)
	{
		ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(rhiTexture);
		const float texW = static_cast<float>(rhiTexture->GetWidth());
		const float texH = static_cast<float>(rhiTexture->GetHeight());

		const ImVec2 center(canvasPos.x + canvasSize.x * 0.5f + m_PanOffset.x, canvasPos.y + canvasSize.y * 0.5f + m_PanOffset.y);
		const ImVec2 texMin(center.x - texW * 0.5f * m_Zoom, center.y - texH * 0.5f * m_Zoom);
		const ImVec2 texMax(texMin.x + texW * m_Zoom, texMin.y + texH * m_Zoom);

		// 텍스처 본체 드로우
		drawList->AddImage(texID, texMin, texMax, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f));
		drawList->AddRect(texMin, texMax, IM_COL32(100, 100, 100, 180), 0.0f, 0, 1.0f);

		// 마우스 클릭으로 슬라이스 선택 판정 (InputManager 기반)
		const vec2 mousePos = input.GetMousePosition();
		const bool isMouseClicked = input.IsMouseButtonDown(Engine::EMouseButton::Left) && ImGui::IsWindowHovered();

		// 3. 슬라이스 박스 렌더링
		for (int i = 0; i < static_cast<int>(m_Slices.size()); ++i)
		{
			const auto& slice = m_Slices[i];
			const bool isSelected = (i == m_SelectedSliceIndex);

			const ImVec2 sMin(texMin.x + slice.region.left * m_Zoom, texMin.y + slice.region.top * m_Zoom);
			const ImVec2 sMax(sMin.x + slice.region.width * m_Zoom, sMin.y + slice.region.height * m_Zoom);

			// 클릭 히트 테스트
			if (isMouseClicked && mousePos.x >= sMin.x && mousePos.x <= sMax.x &&
				mousePos.y >= sMin.y && mousePos.y <= sMax.y)
			{
				m_SelectedSliceIndex = i;
			}

			if (isSelected)
			{
				// 선택된 슬라이스: 노란색 테두리 + 반투명 채우기
				drawList->AddRectFilled(sMin, sMax, IM_COL32(255, 230, 40, 40));
				drawList->AddRect(sMin, sMax, IM_COL32(255, 230, 40, 255), 0.0f, 0, 2.0f);

				// 피벗 십자선
				const ImVec2 pivotPos(sMin.x + slice.region.width * m_Zoom * slice.pivot.x,
					sMax.y - slice.region.height * m_Zoom * slice.pivot.y);
				drawList->AddLine(ImVec2(pivotPos.x - 8.0f, pivotPos.y), ImVec2(pivotPos.x + 8.0f, pivotPos.y), IM_COL32(255, 60, 60, 255), 2.0f);
				drawList->AddLine(ImVec2(pivotPos.x, pivotPos.y - 8.0f), ImVec2(pivotPos.x, pivotPos.y + 8.0f), IM_COL32(60, 255, 60, 255), 2.0f);

				// 9-Slice 보더 가이드라인 (Border L, R, T, B)
				if (slice.border.x > 0.0f || slice.border.y > 0.0f || slice.border.z > 0.0f || slice.border.w > 0.0f)
				{
					const float bL = sMin.x + slice.border.x * m_Zoom;
					const float bR = sMax.x - slice.border.y * m_Zoom;
					const float bT = sMin.y + slice.border.z * m_Zoom;
					const float bB = sMax.y - slice.border.w * m_Zoom;

					drawList->AddLine(ImVec2(bL, sMin.y), ImVec2(bL, sMax.y), IM_COL32(80, 200, 255, 200), 1.0f);
					drawList->AddLine(ImVec2(bR, sMin.y), ImVec2(bR, sMax.y), IM_COL32(80, 200, 255, 200), 1.0f);
					drawList->AddLine(ImVec2(sMin.x, bT), ImVec2(sMax.x, bT), IM_COL32(80, 200, 255, 200), 1.0f);
					drawList->AddLine(ImVec2(sMin.x, bB), ImVec2(sMax.x, bB), IM_COL32(80, 200, 255, 200), 1.0f);
				}
			}
			else
			{
				// 비선택 슬라이스: 얇은 녹색 테두리
				drawList->AddRect(sMin, sMax, IM_COL32(60, 220, 60, 180), 0.0f, 0, 1.0f);
			}
		}
	}
	else
	{
		ImGui::SetCursorPos(ImVec2(20.0f, 20.0f));
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_CSTR("SpriteEditor.NoTextureLoaded"));

		// 화면 중앙에 점선 드롭 가이드 및 텍스처 열기 버튼 제공
		const ImVec2 centerBoxMin(canvasPos.x + 40.0f, canvasPos.y + 40.0f);
		const ImVec2 centerBoxMax(canvasPos.x + canvasSize.x - 40.0f, canvasPos.y + canvasSize.y - 40.0f);
		drawList->AddRect(centerBoxMin, centerBoxMax, IM_COL32(120, 120, 120, 150), 8.0f, ImDrawFlags_None, 1.5f);

		const float btnW = 180.0f;
		const float btnH = 36.0f;
		ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + (canvasSize.x - btnW) * 0.5f, canvasPos.y + canvasSize.y * 0.5f - btnH));
		if (ImGui::Button((std::string(ICON_FA_FOLDER_OPEN) + " " + LOCAL("SpriteEditor.OpenTexture")).c_str(), ImVec2(btnW, btnH)))
		{
			std::wstring selectedPath;
			if (FileDialogs::OpenFileDialog(selectedPath, { { L"Image / Sprite Files", L"*.png;*.jpg;*.jpeg;*.tga;*.bmp;*.dds;*.bamsprite;*.bamtex" } }))
			{
				SetTargetResource(selectedPath);
			}
		}

		ImGui::SetCursorScreenPos(ImVec2(canvasPos.x + (canvasSize.x - 320.0f) * 0.5f, canvasPos.y + canvasSize.y * 0.5f + 12.0f));
		ImGui::TextDisabled("%s %s", ICON_FA_ARROW_DOWN, LOCAL_CSTR("TilePalette.NoSprites"));
	}

	// 캔버스 영역에서 에셋 드래그 앤 드롭 수신
	if (ImGui::BeginDragDropTarget())
	{
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
		{
			const wchar_t* pathStr = static_cast<const wchar_t*>(payload->Data);
			std::filesystem::path droppedPath(pathStr);
			if (IsSupported(droppedPath))
			{
				SetTargetResource(droppedPath);
			}
		}
		else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
		{
			const wchar_t* pathStr = static_cast<const wchar_t*>(payload->Data);
			std::filesystem::path droppedPath(pathStr);
			if (IsSupported(droppedPath))
			{
				SetTargetResource(droppedPath);
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImGui::EndChild();
}

void SpriteEditor::DrawSidebar(float availWidth, float availHeight)
{
	ImGui::BeginChild("##SpriteSidebarChild", ImVec2(availWidth, availHeight), true);

	// 1. 선택된 슬라이스 속성 인스펙터
	if (ImGui::CollapsingHeader(LOCAL_CSTR("SpriteEditor.SelectedSlice"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (m_SelectedSliceIndex >= 0 && m_SelectedSliceIndex < static_cast<int>(m_Slices.size()))
		{
			auto& slice = m_Slices[m_SelectedSliceIndex];

			// 잘린 슬라이스 영역 실시간 썸네일 프리뷰 (원본 종횡비 유지)
			if (auto* tex = m_TextureHandle.Get())
			{
				if (auto* rhiTex = tex->GetRHITexture())
				{
					const float texW = static_cast<float>(rhiTex->GetWidth());
					const float texH = static_cast<float>(rhiTex->GetHeight());
					if (texW > 0.0f && texH > 0.0f)
					{
						const float cropW = (slice.region.width > 0.0f) ? slice.region.width : texW;
						const float cropH = (slice.region.height > 0.0f) ? slice.region.height : texH;
						const ImVec2 uv0(slice.region.left / texW, slice.region.top / texH);
						const ImVec2 uv1((slice.region.left + cropW) / texW, (slice.region.top + cropH) / texH);

						const float boxSize = 96.0f;
						const float scale = boxSize / (std::max)(cropW, cropH);
						const ImVec2 fitSize(cropW * scale, cropH * scale);

						const float availX = ImGui::GetContentRegionAvail().x;
						const ImVec2 boxMin(ImGui::GetCursorScreenPos().x + (availX - boxSize) * 0.5f, ImGui::GetCursorScreenPos().y);
						const ImVec2 boxMax(boxMin.x + boxSize, boxMin.y + boxSize);
						const ImVec2 imgMin(boxMin.x + (boxSize - fitSize.x) * 0.5f, boxMin.y + (boxSize - fitSize.y) * 0.5f);
						const ImVec2 imgMax(imgMin.x + fitSize.x, imgMin.y + fitSize.y);

						ImDrawList* dl = ImGui::GetWindowDrawList();
						dl->AddRectFilled(boxMin, boxMax, IM_COL32(30, 30, 30, 255), 4.0f);
						ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(rhiTex);
						dl->AddImage(texID, imgMin, imgMax, uv0, uv1);
						dl->AddRect(boxMin, boxMax, IM_COL32(100, 100, 100, 180), 4.0f);
						ImGui::Dummy(ImVec2(availX, boxSize + 6.0f));
					}
				}
			}

			char nameBuf[128];
			strncpy_s(nameBuf, slice.name.c_str(), sizeof(nameBuf));
			if (ImGui::InputText(LOCAL_CSTR("SpriteEditor.ClipName"), nameBuf, sizeof(nameBuf)))
			{
				slice.name = nameBuf;
				m_IsDirty = true;
			}

			// 영역 편집 (X, Y, W, H)
			float rectVal[4] = { slice.region.left, slice.region.top, slice.region.width, slice.region.height };
			if (ImGui::DragFloat2(LOCAL_CSTR("SpriteEditor.XYLabel"), rectVal, 1.0f, 0.0f, 8192.0f, "%.0f px"))
			{
				slice.region.left = rectVal[0];
				slice.region.top = rectVal[1];
				m_IsDirty = true;
			}
			if (ImGui::DragFloat2(LOCAL_CSTR("SpriteEditor.WHLabel"), &rectVal[2], 1.0f, 1.0f, 8192.0f, "%.0f px"))
			{
				slice.region.width = rectVal[2];
				slice.region.height = rectVal[3];
				m_IsDirty = true;
			}

			// 피벗 편집 (X, Y)
			if (ImGui::SliderFloat2(LOCAL_CSTR("SpriteEditor.PivotLabel"), &slice.pivot.x, 0.0f, 1.0f, "%.2f"))
			{
				m_IsDirty = true;
			}

			// 피벗 퀵 프리셋 버튼
			if (ImGui::SmallButton(LOCAL_CSTR("SpriteEditor.PivotCenter"))) { slice.pivot = { 0.5f, 0.5f }; m_IsDirty = true; }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("SpriteEditor.PivotBottom"))) { slice.pivot = { 0.5f, 0.0f }; m_IsDirty = true; }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("SpriteEditor.PivotTop"))) { slice.pivot = { 0.5f, 1.0f }; m_IsDirty = true; }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("SpriteEditor.PivotLeft"))) { slice.pivot = { 0.0f, 0.5f }; m_IsDirty = true; }
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_CSTR("SpriteEditor.PivotRight"))) { slice.pivot = { 1.0f, 0.5f }; m_IsDirty = true; }

			// 9-Slice 보더 (Left, Right, Top, Bottom)
			float borderVal[4] = { slice.border.x, slice.border.y, slice.border.z, slice.border.w };
			if (ImGui::DragFloat4(LOCAL_CSTR("SpriteEditor.Border"), borderVal, 1.0f, 0.0f, 512.0f, "%.0f px"))
			{
				slice.border = { borderVal[0], borderVal[1], borderVal[2], borderVal[3] };
				m_IsDirty = true;
			}

			ImGui::Spacing();
			if (ImGui::Button((std::string(ICON_FA_TRASH) + " " + LOCAL("SpriteEditor.DeleteSlice")).c_str()))
			{
				m_Slices.erase(m_Slices.begin() + m_SelectedSliceIndex);
				if (m_SelectedSliceIndex >= static_cast<int>(m_Slices.size()))
					m_SelectedSliceIndex = static_cast<int>(m_Slices.size()) - 1;
				m_IsDirty = true;
			}
		}
		else
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("SpriteEditor.NoSliceSelected"));
		}
	}

	ImGui::Spacing();

	// 2. 슬라이스 전체 목록 테이블
	if (ImGui::CollapsingHeader(LOCAL_CSTR("SpriteEditor.SliceList"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (m_Slices.empty())
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR("SpriteEditor.NoSlices"));
		}
		else
		{
			if (ImGui::BeginTable("##SlicesTable", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersOuter | ImGuiTableFlags_ScrollY, ImVec2(0.0f, 260.0f)))
			{
				ImGui::TableSetupColumn(LOCAL_CSTR("SpriteEditor.IndexHeader"), ImGuiTableColumnFlags_WidthFixed, 28.0f);
				ImGui::TableSetupColumn(LOCAL_CSTR("SpriteEditor.NameHeader"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableSetupColumn(LOCAL_CSTR("SpriteEditor.SizeHeader"), ImGuiTableColumnFlags_WidthFixed, 75.0f);
				ImGui::TableHeadersRow();

				for (int i = 0; i < static_cast<int>(m_Slices.size()); ++i)
				{
					const auto& slice = m_Slices[i];
					ImGui::TableNextRow();

					ImGui::TableSetColumnIndex(0);
					ImGui::Text("%d", i);

					ImGui::TableSetColumnIndex(1);
					const bool isSelected = (i == m_SelectedSliceIndex);
					if (ImGui::Selectable(slice.name.c_str(), isSelected, ImGuiSelectableFlags_SpanAllColumns))
					{
						m_SelectedSliceIndex = i;
					}

					ImGui::TableSetColumnIndex(2);
					ImGui::Text(LOCAL_CSTR("SpriteEditor.SizeFormat"), slice.region.width, slice.region.height);
				}
				ImGui::EndTable();
			}
		}
	}

	ImGui::EndChild();
}

void SpriteEditor::DrawSliceModal()
{
	if (!m_ShowSliceModal) return;

	ImGui::OpenPopup(LOCAL_CSTR("SpriteEditor.SliceModalTitle"));
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(380.0f, 320.0f), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal(LOCAL_CSTR("SpriteEditor.SliceModalTitle"), &m_ShowSliceModal, ImGuiWindowFlags_AlwaysAutoResize))
	{
		const char* methods[] = {
			LOCAL_CSTR("SpriteEditor.GridByCellSize"),
			LOCAL_CSTR("SpriteEditor.GridByCellCount"),
			LOCAL_CSTR("SpriteEditor.Automatic")
		};
		int currentMethod = static_cast<int>(m_SliceParams.method);
		if (ImGui::Combo(LOCAL_CSTR("SpriteEditor.SliceMethod"), &currentMethod, methods, IM_ARRAYSIZE(methods)))
		{
			m_SliceParams.method = static_cast<ESpriteSliceMethod>(currentMethod);
		}

		ImGui::Separator();

		if (m_SliceParams.method == ESpriteSliceMethod::GridByCellSize)
		{
			ImGui::DragInt2(LOCAL_CSTR("SpriteEditor.CellSize"), &m_SliceParams.cellSizeX, 1, 4, 2048);
		}
		else if (m_SliceParams.method == ESpriteSliceMethod::GridByCellCount)
		{
			ImGui::DragInt2(LOCAL_CSTR("SpriteEditor.CellCount"), &m_SliceParams.cellCountCols, 1, 1, 128);
		}
		else
		{
			ImGui::TextDisabled("%s", LOCAL_CSTR("SpriteEditor.AutoDesc"));
		}

		ImGui::DragInt2(LOCAL_CSTR("SpriteEditor.Offset"), &m_SliceParams.offsetX, 1, 0, 1024);
		ImGui::DragInt2(LOCAL_CSTR("SpriteEditor.Padding"), &m_SliceParams.paddingX, 1, 0, 256);

		ImGui::SliderFloat2(LOCAL_CSTR("SpriteEditor.DefaultPivot"), &m_SliceParams.defaultPivot.x, 0.0f, 1.0f, "%.2f");

		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button(LOCAL_CSTR("SpriteEditor.ExecuteSlice"), ImVec2(120.0f, 0.0f)))
		{
			ExecuteSlice(m_SliceParams);
			m_ShowSliceModal = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button(LOCAL_CSTR("EditorCommon.Cancel"), ImVec2(100.0f, 0.0f)))
		{
			m_ShowSliceModal = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void SpriteEditor::DrawCreateAnimationModal()
{
	if (!m_ShowCreateAnimModal) return;

	ImGui::OpenPopup(LOCAL_CSTR("SpriteEditor.CreateAnimModalTitle"));
	ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(420.0f, 320.0f), ImGuiCond_Appearing);

	if (ImGui::BeginPopupModal(LOCAL_CSTR("SpriteEditor.CreateAnimModalTitle"), &m_ShowCreateAnimModal, ImGuiWindowFlags_AlwaysAutoResize))
	{
		int mode = static_cast<int>(m_AnimParams.mode);
		ImGui::RadioButton(LOCAL_CSTR("SpriteEditor.AnimModeSingle"), &mode, 0);
		ImGui::SameLine();
		ImGui::RadioButton(LOCAL_CSTR("SpriteEditor.AnimModeRow"), &mode, 1);
		m_AnimParams.mode = static_cast<EAutoAnimGenerationMode>(mode);

		ImGui::Separator();

		if (m_AnimParams.mode == EAutoAnimGenerationMode::SingleClip)
		{
			char nameBuf[128];
			strncpy_s(nameBuf, m_AnimParams.singleClipName.c_str(), sizeof(nameBuf));
			if (ImGui::InputText(LOCAL_CSTR("SpriteEditor.ClipName"), nameBuf, sizeof(nameBuf)))
			{
				m_AnimParams.singleClipName = nameBuf;
			}
		}
		else
		{
			char rowBuf[256];
			strncpy_s(rowBuf, m_AnimParams.rowNames.c_str(), sizeof(rowBuf));
			if (ImGui::InputText(LOCAL_CSTR("SpriteEditor.RowNames"), rowBuf, sizeof(rowBuf)))
			{
				m_AnimParams.rowNames = rowBuf;
			}
			ImGui::TextDisabled("%s", LOCAL_CSTR("SpriteEditor.RowHelp"));
		}

		ImGui::SliderFloat(LOCAL_CSTR("SpriteEditor.FpsLabel"), &m_AnimParams.fps, 1.0f, 60.0f, "%.0f fps");

		const char* playModes[] = { "Loop", "Once", "PingPong" };
		int currentPlayMode = static_cast<int>(m_AnimParams.playMode);
		if (ImGui::Combo(LOCAL_CSTR("SpriteEditor.PlayModeLabel"), &currentPlayMode, playModes, IM_ARRAYSIZE(playModes)))
		{
			m_AnimParams.playMode = static_cast<Engine::ESpriteAnimationPlayMode>(currentPlayMode);
		}

		ImGui::Separator();
		ImGui::Spacing();

		if (ImGui::Button(LOCAL_CSTR("SpriteEditor.GenerateAnim"), ImVec2(150.0f, 0.0f)))
		{
			GenerateAnimation(m_AnimParams);
			m_ShowCreateAnimModal = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::SameLine();
		if (ImGui::Button(LOCAL_CSTR("EditorCommon.Cancel"), ImVec2(100.0f, 0.0f)))
		{
			m_ShowCreateAnimModal = false;
			ImGui::CloseCurrentPopup();
		}

		ImGui::EndPopup();
	}
}

void SpriteEditor::ExecuteSlice(const SpriteSliceParameters& params)
{
	auto* texture = m_TextureHandle.Get();
	auto* rhiTexture = texture ? texture->GetRHITexture() : nullptr;
	if (!rhiTexture) return;

	const int texW = static_cast<int>(rhiTexture->GetWidth());
	const int texH = static_cast<int>(rhiTexture->GetHeight());
	if (texW <= 0 || texH <= 0) return;

	m_Slices.clear();
	const std::string baseName = m_TargetResourcePath.stem().string();
	int index = 0;

	if (params.method == ESpriteSliceMethod::GridByCellSize)
	{
		const int cellW = (std::max)(1, params.cellSizeX);
		const int cellH = (std::max)(1, params.cellSizeY);
		const int padX = (std::max)(0, params.paddingX);
		const int padY = (std::max)(0, params.paddingY);

		for (int y = params.offsetY; y + cellH <= texH; y += cellH + padY)
		{
			for (int x = params.offsetX; x + cellW <= texW; x += cellW + padX)
			{
				SpriteSliceInfo slice;
				slice.name = baseName + "_" + std::to_string(index++);
				slice.region = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(cellW), static_cast<float>(cellH) };
				slice.pivot = params.defaultPivot;
				m_Slices.push_back(slice);
			}
		}
	}
	else if (params.method == ESpriteSliceMethod::GridByCellCount)
	{
		const int cols = (std::max)(1, params.cellCountCols);
		const int rows = (std::max)(1, params.cellCountRows);
		const int padX = (std::max)(0, params.paddingX);
		const int padY = (std::max)(0, params.paddingY);

		const float availW = static_cast<float>(texW - params.offsetX - (cols - 1) * padX);
		const float availH = static_cast<float>(texH - params.offsetY - (rows - 1) * padY);
		const float cellW = (std::max)(1.0f, availW / cols);
		const float cellH = (std::max)(1.0f, availH / rows);

		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < cols; ++c)
			{
				const float x = params.offsetX + c * (cellW + padX);
				const float y = params.offsetY + r * (cellH + padY);

				SpriteSliceInfo slice;
				slice.name = baseName + "_" + std::to_string(index++);
				slice.region = { x, y, cellW, cellH };
				slice.pivot = params.defaultPivot;
				m_Slices.push_back(slice);
			}
		}
	}
	else
	{
		// Automatic 폴백 (기본 전체 텍스처)
		SpriteSliceInfo slice;
		slice.name = baseName + "_0";
		slice.region = { 0.0f, 0.0f, static_cast<float>(texW), static_cast<float>(texH) };
		slice.pivot = params.defaultPivot;
		m_Slices.push_back(slice);
	}

	m_SelectedSliceIndex = m_Slices.empty() ? -1 : 0;
	m_IsDirty = true;
}

void SpriteEditor::SaveSlices()
{
	if (m_Slices.empty() || !m_TextureHandle.IsValid()) return;

	auto& rm = Engine::ResourceManager::Get();
	const std::filesystem::path destDir = m_TargetResourcePath.parent_path();
	const std::string targetLower = Engine::ToLowerStr(m_TargetResourcePath.filename().string());
	const bool preferJson = targetLower.ends_with(".bamsprite.json") || !targetLower.ends_with(".bamsprite");

	for (const auto& slice : m_Slices)
	{
		std::string cleanName = slice.name;
		if (Engine::ToLowerStr(cleanName).ends_with(".bamsprite"))
			cleanName = cleanName.substr(0, cleanName.size() - 10);

		const std::filesystem::path jsonPath = destDir / (cleanName + ".bamsprite.json");
		const std::filesystem::path binPath = destDir / (cleanName + ".bamsprite");
		const bool useJson = std::filesystem::exists(jsonPath) || (!std::filesystem::exists(binPath) && preferJson);
		const std::filesystem::path outPath = useJson ? jsonPath : binPath;

		SpriteCreateDesc desc;
		desc.texture = m_TextureHandle;
		desc.region = slice.region;
		desc.pivot = slice.pivot;
		desc.border = slice.border;
		desc.Path = outPath.wstring();
		desc.Key = Engine::PlatformPath::ToProjectRelative(outPath).wstring();

		auto* sprite = Engine::Sprite::Create(&desc);
		if (sprite)
		{
			if (useJson)
				rm.SaveToJsonFile(sprite, outPath.wstring());
			else
				rm.SaveToBinaryFile(sprite, outPath.wstring());
			rm.DestroyResource(sprite);
		}

		// 이미 메모리에 로드된 Sprite 인스턴스가 있으면 잘린 영역(Region)/피벗/보더를 즉시 실시간 갱신
		Engine::Handle existingHandle = rm.FindHandleByFilePath(outPath);
		if (!existingHandle.IsValid())
		{
			const std::wstring relKey = Engine::PlatformPath::ToProjectRelative(outPath).wstring();
			existingHandle = rm.FindHandleByKey(relKey);
			if (!existingHandle.IsValid() && useJson && relKey.size() > 5)
				existingHandle = rm.FindHandleByKey(relKey.substr(0, relKey.size() - 5));
		}

		if (auto* liveSprite = dynamic_cast<Engine::Sprite*>(rm.GetResource(existingHandle)))
		{
			liveSprite->SetTexture(m_TextureHandle);
			liveSprite->SetRegion(slice.region);
			liveSprite->SetPivot(slice.pivot);
			liveSprite->SetBorder(slice.border);
		}
		else
		{
			rm.LoadFile(outPath.wstring());
		}
	}

	m_IsDirty = false;
	NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.SpriteTitle"),
		LOCAL_FMT("Notification.Saved", m_TargetResourcePath.filename().string()), ELogLevel::Info);
}

void SpriteEditor::GenerateAnimation(const SpriteAutoAnimParameters& params)
{
	if (m_Slices.empty() || !m_TextureHandle.IsValid()) return;

	// 1. 먼저 슬라이스 에셋들을 디스크에 저장
	SaveSlices();

	auto& rm = Engine::ResourceManager::Get();
	const std::filesystem::path destDir = m_TargetResourcePath.parent_path();
	const std::string baseName = m_TargetResourcePath.stem().string();

	if (params.mode == EAutoAnimGenerationMode::SingleClip)
	{
		// 단일 애니메이션 클립 생성
		auto* anim = Engine::SpriteAnimation::Create();
		if (anim)
		{
			anim->SetFPS(params.fps);
			anim->SetPlayMode(params.playMode);

			for (const auto& slice : m_Slices)
			{
				const std::filesystem::path spritePath = destDir / (slice.name + ".bamsprite");
				const std::wstring normPath = Engine::NormalizePath(spritePath.wstring());
				auto handle = rm.GetResourceHandle<Engine::Sprite>(normPath);
				if (!handle.IsValid())
				{
					SpriteCreateDesc desc;
					desc.Path = normPath;
					handle = rm.LoadResource<Engine::Sprite>(&desc);
				}
				anim->AddFrame(handle, 1.0f);
			}

			const std::filesystem::path animPath = destDir / (params.singleClipName + ".spriteanim");
			anim->SaveToFile(animPath.wstring());
			rm.DestroyResource(anim);
		}
	}
	else
	{
		// 행(Row) 단위 다중 클립 분할 생성
		std::vector<std::string> actionNames;
		std::stringstream ss(params.rowNames);
		std::string item;
		while (std::getline(ss, item, ','))
		{
			size_t start = item.find_first_not_of(" \t\r\n");
			size_t end = item.find_last_not_of(" \t\r\n");
			if (start != std::string::npos && end != std::string::npos)
				actionNames.push_back(item.substr(start, end - start + 1));
		}

		// 슬라이스를 Y 좌표 기준으로 행 분류 (오차 허용치 +-4px)
		std::vector<std::vector<SpriteSliceInfo>> rowGroups;
		for (const auto& slice : m_Slices)
		{
			bool foundGroup = false;
			for (auto& group : rowGroups)
			{
				if (!group.empty() && std::abs(group[0].region.top - slice.region.top) <= 4.0f)
				{
					group.push_back(slice);
					foundGroup = true;
					break;
				}
			}
			if (!foundGroup)
			{
				rowGroups.push_back({ slice });
			}
		}

		// Y 좌표 오름차순 정렬
		std::sort(rowGroups.begin(), rowGroups.end(), [](const auto& a, const auto& b) {
			return a[0].region.top < b[0].region.top;
		});

		// 각 행별 애니메이션 에셋 저장
		for (size_t r = 0; r < rowGroups.size(); ++r)
		{
			auto& group = rowGroups[r];
			// X 좌표 오름차순 정렬 (프레임 순서)
			std::sort(group.begin(), group.end(), [](const auto& a, const auto& b) {
				return a.region.left < b.region.left;
			});

			std::string actionName = (r < actionNames.size()) ? actionNames[r] : ("Action_" + std::to_string(r));
			std::string clipFileName = baseName + "_" + actionName;

			auto* anim = Engine::SpriteAnimation::Create();
			if (anim)
			{
				anim->SetFPS(params.fps);
				anim->SetPlayMode(params.playMode);

				for (const auto& slice : group)
				{
					const std::filesystem::path spritePath = destDir / (slice.name + ".bamsprite");
					const std::wstring normPath = Engine::NormalizePath(spritePath.wstring());
					auto handle = rm.GetResourceHandle<Engine::Sprite>(normPath);
					if (!handle.IsValid())
					{
						SpriteCreateDesc desc;
						desc.Path = normPath;
						handle = rm.LoadResource<Engine::Sprite>(&desc);
					}
					anim->AddFrame(handle, 1.0f);
				}

				const std::filesystem::path animPath = destDir / (clipFileName + ".spriteanim");
				anim->SaveToFile(animPath.wstring());
				rm.DestroyResource(anim);
			}
		}
	}

	NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.SpriteTitle"),
		LOCAL_CSTR("SpriteEditor.GenerateAnimSuccess"), ELogLevel::Info);
}

} // namespace Editor
