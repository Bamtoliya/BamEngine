#pragma once

#include "BaseViewportPanel.h"
#include "RenderTargetManager.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "DirectX12Texture.h"
#include "ImGuiManager.h"
#include "EditorSettings.h"

#pragma region Contructor&Destructor
void BaseViewportPanel::Initialize(void* arg)
{
	if (!arg) return;
	CAST_DESC
	m_Name = desc->Name;
	m_DisplayNameKey = desc->DisplayNameKey;
	m_WindowID = WStrToStr(m_Name);
	m_ChannelFilter.Initialize(m_Name);
	m_SelectedRTName = L"";
}
void BaseViewportPanel::Free()
{
	m_ChannelFilter.Free();
}
#pragma endregion

#pragma region Loop
void BaseViewportPanel::Update(f32 dt)
{
	if (!CalculateRenderResolution(m_PanelWidth, m_PanelHeight))
		return;
	m_IsRenderTargetViewer = true;
	m_DisplayRTName = m_SelectedRTName;
	if (m_ChannelView != EViewportChannelView::RGBA)
	{
		m_ChannelFilter.SetChannelView(m_ChannelView);
		m_ChannelFilter.SubmitChannelPreviewPass(m_SelectedRTName, m_DisplayRTName);
	}
}
void BaseViewportPanel::Draw()
{
	m_Focused = false;
	m_Hovered = false;
	if (!m_Open) return;
	ImGui::PushID(this);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
	const string windowID = GetDisplayName() + "###" + m_WindowID;
	if (ImGui::Begin(windowID.c_str(), &m_Open, window_flags))
	{
		DrawOptionsBar();
		DrawCustomViewport();
	}
	ImGui::End();
	ImGui::PopStyleVar();


	ImGui::PopID();
}
#pragma endregion

#pragma region Viewport
void BaseViewportPanel::DrawCustomViewport()
{
	DrawRenderTargetImage(m_DisplayRTName);
}
void BaseViewportPanel::DrawRenderTargetImage(const wstring& rtName)
{
	if (!m_RenderTargetsValid)
	{
		m_Focused = false;
		m_Hovered = false;
		m_ImageSize = ImVec2(0.f, 0.f);
		ImGui::TextWrapped("%s", LOCAL_CSTR("Viewport.RenderTargetsUnavailable"));
		return;
	}

	ImVec2 panelSize = ImGui::GetContentRegionAvail();
	panelSize.x = glm::max(panelSize.x, 1.0f);
	panelSize.y = glm::max(panelSize.y, 1.0f);

	m_PanelWidth = static_cast<uint32>(panelSize.x);
	m_PanelHeight = static_cast<uint32>(panelSize.y);

	uint32 width = m_PanelWidth;
	uint32 height = m_PanelHeight;
	f32 panelAspectRatio = static_cast<f32>(width) / static_cast<f32>(height);

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
			ImTextureID textureID = NULL;
			textureID = ImGuiManager::Get().GetImGuiTextureID(texture);
			//if (auto* dxTex = dynamic_cast<DirectX12Texture*>(texture))
			//{
			//	// DX12 + imgui_impl_dx12 규약: ImTextureID = GPU SRV handle.ptr
			//	const D3D12_GPU_DESCRIPTOR_HANDLE gpu = dxTex->GetSRVGPUHandle();
			//	IM_ASSERT(gpu.ptr != 0);
			//	textureID = static_cast<ImTextureID>(gpu.ptr);
			//}
			//else
			//{
			//	// 다른 백엔드 fallback
			//	textureID = (ImTextureID)texture->GetNativeHandle();
			//}

			ImGui::Image(textureID, finalSize);
		}
	}
}
#pragma endregion

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
	
	if (ImGui::BeginMenu(LOCAL_CSTR("UI_RENDER_TARGET")))
	{
		if (ImGui::BeginMenu(LOCAL_LABEL("Viewport.AllRenderTargets").c_str()))
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
				DrawRTItemContextMenu(name);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
}
void BaseViewportPanel::DrawChannelViewButton(float rightReservedWidth)
{
	const float btnHeight = ImGui::GetFrameHeight();
	const ImVec2 buttonSize(btnHeight, btnHeight);
	const float buttonSpacing = 0.0f;
	const float rightPadding = ImGui::GetStyle().WindowPadding.x;

	const float channelGroupWidth = buttonSize.x * 4.0f + buttonSpacing * 3.0f;
	ImGui::SameLine(ImGui::GetWindowWidth() - rightReservedWidth - channelGroupWidth - rightPadding);

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
void BaseViewportPanel::DrawRTItemContextMenu(const wstring& rtName)
{
	string popupID = "##RTCtx_" + WStrToStr(rtName);
	if (ImGui::BeginPopupContextItem(popupID.c_str()))
	{
		const string label = LOCAL_FMT("Viewport.OpenInViewport", WStrToStr(rtName));
		if (ImGui::MenuItem(label.c_str()))
		{
			// 중복 이름 방지: 이미 같은 이름 패널이 있으면 번호 붙임
			wstring panelName = rtName + L" Viewer";
			while (ImGuiManager::Get().GetImGuiPanel(panelName))
				panelName += L"_";

			ViewportPanelDesc desc;
			desc.Name = panelName;
			desc.RenderTargetWidth = m_RenderWidth;
			desc.RenderTargetHeight = m_RenderHeight;

			BaseViewportPanel* panel = new BaseViewportPanel();
			panel->Initialize(&desc);
			panel->SetSelectedRenderTarget(rtName);  // ← 아래에서 추가할 setter
			ImGuiManager::Get().AddImGuiPanel(panel);
		}
		ImGui::EndPopup();
	}
}
#pragma endregion

#pragma region Resolution&Ratio
EResult BaseViewportPanel::CalculateRenderResolution(uint32 panelWidth, uint32 panelHeight)
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
	newWidth = glm::max(newWidth, static_cast<uint32>(MinResolutionDimension));
	newHeight = glm::max(newHeight, static_cast<uint32>(MinResolutionDimension));

	if (newWidth == m_RenderWidth && newHeight == m_RenderHeight && m_RenderTargetsValid)
		return m_LastResolutionUpdateResult = EResult::Success;

	const auto result = ResizeRenderTargets(newWidth, newHeight);

	if (result != EResult::Success)
	{
		const auto restored = ResizeRenderTargets(m_RenderWidth, m_RenderHeight);
		m_RenderTargetsValid = restored == EResult::Success;

		EDITOR_LOG_ERROR("Viewport resize failed: name={}, requested={}x{}, restored={}",
			WStrToStr(m_Name), newWidth, newHeight, m_RenderTargetsValid);

		return m_LastResolutionUpdateResult = result;
	}

	m_RenderWidth = newWidth;
	m_RenderHeight = newHeight;
	m_RenderTargetsValid = true;
	return m_LastResolutionUpdateResult = EResult::Success;
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
EResult BaseViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
	return m_ChannelFilter.Resize(m_SelectedRTName);
}
void BaseViewportPanel::DrawResolutionMenu()
{
	if (!ImGui::BeginMenu(LOCAL_LABEL("UI_RESOLUTION").c_str()))
		return;

	auto state = CaptureResolutionState();
	bool changed = false;

	if (ImGui::MenuItem(LOCAL_LABEL("Viewport.FreePanelSize").c_str(), nullptr,
		state.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Free)))
	{
		state.ResolutionMode = static_cast<int32>(EViewportResolutionMode::Free);
		changed = true;
	}

	ImGui::Separator();

	for (uint32 index = 0; index < g_PresetCount; ++index)
	{
		const auto& preset = g_ResolutionPresets[index];
		const bool selected = state.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Preset) &&
			state.PresetWidth == static_cast<int32>(preset.Width) &&
			state.PresetHeight == static_cast<int32>(preset.Height);

		if (ImGui::MenuItem(LOCAL_LABEL(preset.NameKey).c_str(), nullptr, selected))
		{
			state.ResolutionMode = static_cast<int32>(EViewportResolutionMode::Preset);
			state.PresetWidth = static_cast<int32>(preset.Width);
			state.PresetHeight = static_cast<int32>(preset.Height);
			changed = true;
		}
	}

	ImGui::Separator();

	const bool isCustom = state.ResolutionMode == static_cast<int32>(EViewportResolutionMode::Custom);

	if (ImGui::MenuItem(LOCAL_LABEL("Viewport.Custom").c_str(), nullptr, isCustom))
	{
		state.ResolutionMode = static_cast<int32>(EViewportResolutionMode::Custom);
		changed = true;
	}

	if (isCustom)
	{
		changed |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Width").c_str(), &state.CustomWidth, 1, 100);
		changed |= ImGui::InputInt(LOCAL_LABEL("EditorCommon.Height").c_str(), &state.CustomHeight, 1, 100);
		state.CustomWidth = glm::clamp(state.CustomWidth, MinResolutionDimension, MaxResolutionWidth);
		state.CustomHeight = glm::clamp(state.CustomHeight, MinResolutionDimension, MaxResolutionHeight);
	}

	ImGui::Separator();

	if (ImGui::BeginMenu(LOCAL_LABEL("Viewport.AspectRatio").c_str()))
	{
		auto ratioItem = [&](const char* label, EAspectRatioMode mode)
			{
				const auto value = static_cast<int32>(mode);

				if (ImGui::MenuItem(label, nullptr, state.AspectRatioMode == value))
				{
					state.AspectRatioMode = value;
					changed = true;
				}
			};

		ratioItem(LOCAL_LABEL("Viewport.FreeAspect").c_str(), EAspectRatioMode::Free);
		ratioItem("16:9", EAspectRatioMode::Ratio_16_9);
		ratioItem("16:10", EAspectRatioMode::Ratio_16_10);
		ratioItem("4:3", EAspectRatioMode::Ratio_4_3);
		ratioItem("21:9", EAspectRatioMode::Ratio_21_9);
		ratioItem("1:1", EAspectRatioMode::Ratio_1_1);
		ratioItem("9:16", EAspectRatioMode::Ratio_9_16);
		ImGui::EndMenu();
	}

	ImGui::Separator();
	ImGui::Text("%s", LOCAL_FMT("Viewport.RenderSize", m_RenderWidth, m_RenderHeight).c_str());

	const float scale = static_cast<float>(m_RenderWidth) / static_cast<float>(m_PanelWidth) * 100.f;
	ImGui::Text("%s", LOCAL_FMT("Viewport.Scale", scale).c_str());
	ImGui::EndMenu();

	if (changed && !TrySetResolutionState(state))
		EDITOR_LOG_WARN("Viewport resolution change was rejected: {}", WStrToStr(m_Name));
}
#pragma endregion

#pragma region Workspace State
void BaseViewportPanel::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.ViewportStates[GetName()] = CaptureViewportState();
}
void BaseViewportPanel::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	const auto found = workspace.ViewportStates.find(GetName());

	if (found != workspace.ViewportStates.end())
		RestoreViewportState(found->second);
}
bool BaseViewportPanel::TrySetResolutionState(const ViewportWorkspace& state)
{
	const auto validSize = [](std::int32_t width, std::int32_t height)
		{
			return width >= MinResolutionDimension && width <= MaxResolutionWidth &&
				height >= MinResolutionDimension && height <= MaxResolutionHeight;
		};

	const bool validMode = state.ResolutionMode >= 0 &&
		state.ResolutionMode <= static_cast<std::int32_t>(EViewportResolutionMode::Custom);
	const bool validAspect = state.AspectRatioMode >= 0 &&
		state.AspectRatioMode <= static_cast<std::int32_t>(EAspectRatioMode::Ratio_9_16);

	if (!validMode || !validAspect || !validSize(state.CustomWidth, state.CustomHeight) ||
		!validSize(state.PresetWidth, state.PresetHeight))
	{
		return false;
	}

	auto mode = static_cast<EViewportResolutionMode>(state.ResolutionMode);
	int32 presetIndex = m_PresetIndex;
	int32 customWidth = state.CustomWidth;
	int32 customHeight = state.CustomHeight;
	bool presetFound = false;

	for (uint32 index = 0; index < g_PresetCount; ++index)
	{
		const auto& preset = g_ResolutionPresets[index];

		if (preset.Width == static_cast<uint32>(state.PresetWidth) &&
			preset.Height == static_cast<uint32>(state.PresetHeight))
		{
			presetIndex = static_cast<int32>(index);
			presetFound = true;
			break;
		}
	}

	if (mode == EViewportResolutionMode::Preset && !presetFound)
	{
		mode = EViewportResolutionMode::Custom;
		customWidth = state.PresetWidth;
		customHeight = state.PresetHeight;

		EDITOR_LOG_WARN("Viewport preset is unavailable; using custom resolution: {}", WStrToStr(m_Name));
	}

	m_ResolutionMode = mode;
	m_AspectRatioMode = static_cast<EAspectRatioMode>(state.AspectRatioMode);
	m_PresetIndex = presetIndex;
	m_CustomWidth = customWidth;
	m_CustomHeight = customHeight;
	return true;
}

ViewportWorkspace BaseViewportPanel::CaptureResolutionState() const
{
	ViewportWorkspace state;
	state.ResolutionMode = static_cast<std::int32_t>(m_ResolutionMode);
	state.AspectRatioMode = static_cast<std::int32_t>(m_AspectRatioMode);
	state.CustomWidth = m_CustomWidth;
	state.CustomHeight = m_CustomHeight;

	if (m_PresetIndex >= 0 && m_PresetIndex < static_cast<int32>(g_PresetCount))
	{
		const auto& preset = g_ResolutionPresets[m_PresetIndex];
		state.PresetWidth = static_cast<std::int32_t>(preset.Width);
		state.PresetHeight = static_cast<std::int32_t>(preset.Height);
	}

	return state;
}

void BaseViewportPanel::RestoreViewportState(const ViewportWorkspace& state)
{
	if (!TrySetResolutionState(state))
		EDITOR_LOG_WARN("Invalid viewport resolution state: {}", WStrToStr(m_Name));
}

ViewportWorkspace BaseViewportPanel::CaptureViewportState() const
{
	return CaptureResolutionState();
}
#pragma endregion