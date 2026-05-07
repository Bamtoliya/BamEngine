#pragma once

#include "BaseViewportPanel.h"
#include "RenderTargetManager.h"
#include "CameraManager.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "ImViewGuizmo.h"

#pragma region Contructor&Destructor
void BaseViewportPanel::Initialize(void* arg)
{
	if (!arg) return;
	CAST_DESC
	m_Name = desc->Name;
	m_ChannelFilter.Initialize(m_Name);
	m_SelectedRTName = L"";
}
void BaseViewportPanel::Free()
{
	m_ChannelFilter.Free();
}
#pragma endregion

void BaseViewportPanel::Update(f32 dt)
{
	CalculateRenderResolution(m_PanelWidth, m_PanelHeight);
	if (m_ChannelView != EViewportChannelView::RGBA)
	{
		m_ChannelFilter.SetChannelView(m_ChannelView);
		m_ChannelFilter.SubmitChannelPreviewPass(m_SelectedRTName, m_DisplayRTName);
	}
}

void BaseViewportPanel::Draw()
{
	if (!m_Open) return;
	ImGui::PushID(this);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bool opened = true;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
	string windowID = WStrToStr(m_Name);
	if (ImGui::Begin(windowID.c_str(), &opened, window_flags))
	{
		DrawOptionsBar();
		DrawCustomViewport();
	}
	ImGui::End();
	ImGui::PopStyleVar();


	ImGui::PopID();
}

void BaseViewportPanel::DrawCustomViewport()
{
	DrawRenderTargetImage(m_DisplayRTName);
}

void BaseViewportPanel::DrawRenderTargetImage(const wstring& rtName)
{
	ImVec2 panelSize = ImGui::GetContentRegionAvail();
	m_PanelWidth = glm::max((uint32)panelSize.x, (uint32)1);
	m_PanelHeight = glm::max((uint32)panelSize.y, (uint32)1);
	uint32 width = (uint32)panelSize.x;
	uint32 height = (uint32)panelSize.y;
	f32 panelAspectRatio = (f32)width / (f32)height;

	RenderTarget* currentRT = RenderTargetManager::Get().GetRenderTarget(rtName);
	if (currentRT)
	{
		RHITexture* texture = currentRT->GetTexture();
		if (texture)
		{
			f32 imageWidth = (f32)currentRT->GetWidth();
			f32 imageHeight = (f32)currentRT->GetHeight();

			f32 imageRatio = imageWidth / imageHeight;

			ImVec2 finalSize = panelSize;

			if (panelAspectRatio > imageRatio)
			{
				finalSize.y = panelSize.y;
				finalSize.x = finalSize.y * imageRatio;
			}
			else
			{
				finalSize.x = panelSize.x;
				finalSize.y = finalSize.x / imageRatio;
			}

			m_Focused = ImGui::IsWindowFocused();
			m_Hovered = ImGui::IsWindowHovered();

			ImVec2 cursorStart = ImGui::GetCursorPos();
			float offsetX = (panelSize.x - finalSize.x) * 0.5f;
			float offsetY = (panelSize.y - finalSize.y) * 0.5f;
			ImGui::SetCursorPos(ImVec2((float)(int)(cursorStart.x + offsetX), (float)(int)(cursorStart.y + offsetY)));
			m_ImageScreenPos = ImGui::GetCursorScreenPos();
			m_ImageSize = finalSize;
			ImGui::GetWindowDrawList()->AddRectFilled(
				m_ImageScreenPos,
				ImVec2(m_ImageScreenPos.x + finalSize.x, m_ImageScreenPos.y + finalSize.y),
				IM_COL32(50, 50, 50, 255));
			ImTextureID textureID = (ImTextureID)texture->GetNativeHandle();
			ImGui::Image(textureID, finalSize);
		}
	}
}

#pragma region Options Bar
void BaseViewportPanel::DrawOptionsBar()
{
	if (ImGui::BeginMenuBar())
	{
		DrawCustomOptions();
	}
	ImGui::EndMenuBar();
}
void BaseViewportPanel::DrawCustomOptions()
{
	DrawResolutionMenu();
	DrawRenderTargetMenu();
	DrawChannelViewButton();
}
void BaseViewportPanel::DrawRenderTargetMenu()
{
	if (ImGui::BeginMenu("RenderTarget"))
	{
		if (ImGui::BeginMenu("All RenderTargets"))
		{
			auto allNames = RenderTargetManager::Get().GetAllRenderTargetNames();
			for (const auto& name : allNames)
			{
				string label = WStrToStr(name);
				bool selected = (name == m_SelectedRTName);
				if (ImGui::MenuItem(label.c_str(), nullptr, selected))
				{
					m_SelectedRTName = name;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
}
void BaseViewportPanel::DrawChannelViewButton()
{
	const ImVec2 buttonSize(28.0f, 0.0f);
	const float buttonSpacing = 0.0f;
	const float groupGapToDimension = 8.0f;
	const float rightPadding = 10.0f;

	const float dimensionGroupWidth = 30.0f * 2.0f;
	const float channelGroupWidth = buttonSize.x * 4.0f + buttonSpacing * 3.0f;

	ImGui::SameLine(
		ImGui::GetWindowWidth() - dimensionGroupWidth - groupGapToDimension - channelGroupWidth - rightPadding);

	auto toggleFlag = [this](EViewportChannelView flag)
		{
			const uint8 current = static_cast<uint8>(m_ChannelView);
			const uint8 bit = static_cast<uint8>(flag);

			if ((current & bit) != 0)
				m_ChannelView = static_cast<EViewportChannelView>(current & ~bit);
			else
				m_ChannelView = static_cast<EViewportChannelView>(current | bit);
		};

	auto drawToggleButton = [&](const char* label, EViewportChannelView flag, const ImVec4& activeColor)
		{
			const bool isActive = HasFlag(m_ChannelView, flag);

			if (isActive)
				ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
			else
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

			if (ImGui::Button(label, buttonSize))
				toggleFlag(flag);

			ImGui::PopStyleColor();
		};

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(buttonSpacing, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

	drawToggleButton("R", EViewportChannelView::R, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
	ImGui::SameLine();
	drawToggleButton("G", EViewportChannelView::G, ImVec4(0.20f, 0.65f, 0.20f, 1.0f));
	ImGui::SameLine();
	drawToggleButton("B", EViewportChannelView::B, ImVec4(0.20f, 0.35f, 0.80f, 1.0f));
	ImGui::SameLine();
	drawToggleButton("A", EViewportChannelView::A, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));

	ImGui::PopStyleVar(2);
}
#pragma endregion

#pragma region Resolution&Ratio
void BaseViewportPanel::CalculateRenderResolution(uint32 panelWidth, uint32 panelHeight)
{
	uint32 newWidth = panelWidth;
	uint32 newHeight = panelHeight;
	switch (m_ResolutionMode)
	{
	case EViewportResolutionMode::Free:
		newWidth = panelWidth;
		newHeight = panelHeight;
		break;
	case EViewportResolutionMode::Preset:
		newWidth = g_ResolutionPresets[m_PresetIndex].Width;
		newHeight = g_ResolutionPresets[m_PresetIndex].Height;
		break;
	case EViewportResolutionMode::Custom:
		newWidth = (uint32)m_CustomWidth;
		newHeight = (uint32)m_CustomHeight;
		break;
	}
	// Aspect Ratio 강제 적용 (Free 모드 + 비율 고정 시)
	if (m_AspectRatioMode != EAspectRatioMode::Free)
	{
		float targetRatio = GetAspectRatio(m_AspectRatioMode);
		float currentRatio = (float)newWidth / (float)newHeight;
		if (currentRatio > targetRatio)
			newWidth = (uint32)(newHeight * targetRatio);
		else
			newHeight = (uint32)(newWidth / targetRatio);
	}
	// 최소 크기 보정
	newWidth = glm::max(newWidth, (uint32)64);
	newHeight = glm::max(newHeight, (uint32)64);
	// 변경 감지 → RT 재생성
	if (newWidth != m_RenderWidth || newHeight != m_RenderHeight)
	{
		m_RenderWidth = newWidth;
		m_RenderHeight = newHeight;
		ResizeRenderTargets(m_RenderWidth, m_RenderHeight);
	}
}

f32 BaseViewportPanel::GetAspectRatio(EAspectRatioMode mode) const
{
	switch (mode)
	{
	case EAspectRatioMode::Ratio_16_9:  return 16.f / 9.f;
	case EAspectRatioMode::Ratio_16_10: return 16.f / 10.f;
	case EAspectRatioMode::Ratio_4_3:   return 4.f / 3.f;
	case EAspectRatioMode::Ratio_21_9:  return 21.f / 9.f;
	case EAspectRatioMode::Ratio_1_1:   return 1.f;
	case EAspectRatioMode::Ratio_9_16:  return 9.f / 16.f;
	default: return 1.f;
	}
}

void BaseViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
	m_ChannelFilter.Resize(m_SelectedRTName);
}

void BaseViewportPanel::DrawResolutionMenu()
{
	if (ImGui::BeginMenu("Resolution"))
	{
		// --- Resolution Mode ---
		if (ImGui::MenuItem("Free (Panel Size)", nullptr,
			m_ResolutionMode == EViewportResolutionMode::Free))
			m_ResolutionMode = EViewportResolutionMode::Free;
		ImGui::Separator();
		// --- Presets ---
		for (uint32 i = 0; i < g_PresetCount; ++i)
		{
			bool selected = (m_ResolutionMode == EViewportResolutionMode::Preset
				&& m_PresetIndex == (int32)i);
			if (ImGui::MenuItem(g_ResolutionPresets[i].Name, nullptr, selected))
			{
				m_ResolutionMode = EViewportResolutionMode::Preset;
				m_PresetIndex = i;
			}
		}
		ImGui::Separator();
		// --- Custom ---
		bool isCustom = (m_ResolutionMode == EViewportResolutionMode::Custom);
		if (ImGui::MenuItem("Custom...", nullptr, isCustom))
			m_ResolutionMode = EViewportResolutionMode::Custom;
		if (isCustom)
		{
			ImGui::InputInt("Width", &m_CustomWidth, 1, 100);
			ImGui::InputInt("Height", &m_CustomHeight, 1, 100);
			m_CustomWidth = glm::clamp(m_CustomWidth, 64, 7680);
			m_CustomHeight = glm::clamp(m_CustomHeight, 64, 4320);
		}
		ImGui::Separator();
		// --- Aspect Ratio ---
		if (ImGui::BeginMenu("Aspect Ratio"))
		{
			auto ratioItem = [&](const char* label, EAspectRatioMode mode)
				{
					if (ImGui::MenuItem(label, nullptr, m_AspectRatioMode == mode))
						m_AspectRatioMode = mode;
				};
			ratioItem("Free", EAspectRatioMode::Free);
			ratioItem("16:9", EAspectRatioMode::Ratio_16_9);
			ratioItem("16:10", EAspectRatioMode::Ratio_16_10);
			ratioItem("4:3", EAspectRatioMode::Ratio_4_3);
			ratioItem("21:9", EAspectRatioMode::Ratio_21_9);
			ratioItem("1:1", EAspectRatioMode::Ratio_1_1);
			ratioItem("9:16", EAspectRatioMode::Ratio_9_16);
			ImGui::EndMenu();
		}
		// --- 현재 해상도 표시 ---
		ImGui::Separator();
		ImGui::Text("Render: %u x %u", m_RenderWidth, m_RenderHeight);
		float scale = (float)m_RenderWidth / (float)m_PanelWidth * 100.f;
		ImGui::Text("Scale: %.0f%%", scale);
		ImGui::EndMenu();
	}
}
#pragma endregion