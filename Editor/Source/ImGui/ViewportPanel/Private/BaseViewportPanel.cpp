#pragma once

#include "BaseViewportPanel.h"
#include "EditorUI.h"
#include "RenderTargetManager.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "ImGuiManager.h"
#include "EditorSettings.h"
#include "ViewportImage.h"

#pragma region Contructor&Destructor
EResult BaseViewportPanel::OnInitialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	m_Name = desc->Name;
	m_DisplayNameKey = desc->DisplayNameKey;
	m_WindowID = WStrToStr(m_Name);
	m_ChannelFilter.Initialize(m_Name);
	m_SelectedRTName = desc->InitialRenderTargetName;
	return EResult::Success;
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
	m_ChannelFilter.SubmitChannelPreviewPass(m_SelectedRTName, m_DisplayRTName);
}
void BaseViewportPanel::Draw()
{
	m_Focused = false;
	m_Hovered = false;
	if (!m_Open)
	{
		m_IsVisibleOnScreen = false;
		return;
	}
	if (m_FocusRequested)
		ImGui::SetNextWindowFocus();
	ImGui::PushID(this);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
	const string windowID = GetDisplayName() + "###" + m_WindowID;
	// ImGui::Begin이 false를 반환하면 다른 탭 뒤에 완전히 숨겨진 상태입니다.
	const auto padding = ImGui::GetStyle().FramePadding;
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padding.x, std::max(padding.y, ImGui::GetFontSize() * 0.15f)));
	const bool visible = BeginPanelWindow(windowID.c_str(), m_Open, window_flags);
	if (visible)
	{
		if (m_FocusRequested)
		{
			ImGui::SetWindowFocus();
			m_FocusRequested = false;
		}
		m_IsVisibleOnScreen = true;
		DrawOptionsBar();
	}
	ImGui::PopStyleVar();
	if (visible) DrawCustomViewport();
	else
	{
		if (m_FocusRequested)
		{
			ImGui::SetWindowFocus(windowID.c_str());
			m_FocusRequested = false;
		}
		m_IsVisibleOnScreen = false;
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
void BaseViewportPanel::DrawRenderTargetImage(const wstring& rtName, bool nativePixels)
{
	m_ImageSize = ImVec2(0.0f, 0.0f);
	if (!m_RenderTargetsValid)
	{
		m_Focused = false;
		m_Hovered = false;
		ImGui::TextWrapped("%s", LOCAL_CSTR("Viewport.RenderTargetsUnavailable"));
		return;
	}

	ImVec2 panelSize = ImGui::GetContentRegionAvail();
	panelSize.x = glm::max(panelSize.x, 1.0f);
	panelSize.y = glm::max(panelSize.y, 1.0f);
	m_PanelWidth = static_cast<uint32>(panelSize.x);
	m_PanelHeight = static_cast<uint32>(panelSize.y);

	const auto image = ResolveViewportImage(rtName);
	if (image.Status == EViewportImageStatus::Unsupported)
	{
		m_Focused = false;
		m_Hovered = false;
		ImGui::TextWrapped("%s", LOCAL_CSTR("Viewport.RenderTargetPreviewUnsupported"));
		return;
	}
	if (image.Status != EViewportImageStatus::Ready)
		return;

	const float imageWidth = image.SourceSize.x;
	const float imageHeight = image.SourceSize.y;
	ImVec2 finalSize = panelSize;

	m_Focused = ImGui::IsWindowFocused();
	m_Hovered = ImGui::IsWindowHovered();

	if (nativePixels)
	{
		const ImGuiViewport* viewport = ImGui::GetWindowViewport();
		const ImVec2 scale = viewport->FramebufferScale;
		const ImVec2 start = ImGui::GetCursorScreenPos();
		if (!(scale.x > 0.0f && scale.y > 0.0f))
			return;

		const float left = std::ceil((start.x - viewport->Pos.x) * scale.x);
		const float top = std::ceil((start.y - viewport->Pos.y) * scale.y);
		const float right = std::floor((start.x + panelSize.x - viewport->Pos.x) * scale.x);
		const float bottom = std::floor((start.y + panelSize.y - viewport->Pos.y) * scale.y);
		if (right - left < imageWidth || bottom - top < imageHeight)
		{
			ImGui::TextWrapped("%s", LOCAL_CSTR("Viewport.PixelPerfectTargetTooLarge"));
			return;
		}

		finalSize = ImVec2(imageWidth / scale.x, imageHeight / scale.y);
		const float x = left + std::floor((right - left - imageWidth) * 0.5f);
		const float y = top + std::floor((bottom - top - imageHeight) * 0.5f);
		ImGui::SetCursorScreenPos(ImVec2(viewport->Pos.x + x / scale.x, viewport->Pos.y + y / scale.y));
	}
	else
	{
		const float panelRatio = static_cast<float>(m_PanelWidth) / static_cast<float>(m_PanelHeight);
		const float imageRatio = imageWidth / imageHeight;
		if (panelRatio > imageRatio)
			finalSize.x = finalSize.y * imageRatio;
		else
			finalSize.y = finalSize.x / imageRatio;

		const ImVec2 start = ImGui::GetCursorPos();
		const float x = start.x + (panelSize.x - finalSize.x) * 0.5f;
		const float y = start.y + (panelSize.y - finalSize.y) * 0.5f;
		ImGui::SetCursorPos(ImVec2(static_cast<float>(static_cast<int>(x)), static_cast<float>(static_cast<int>(y))));
	}

	m_ImageScreenPos = DrawViewportImage(image, finalSize, IM_COL32(50, 50, 50, 255));
	m_ImageSize = finalSize;
}
#pragma endregion

#pragma region Options Bar
float BaseViewportPanel::MeasureOptionLabels(std::initializer_list<const char*> labels)
{
	const auto& style = ImGui::GetStyle();
	float width = 0.0f;
	for (const char* label : labels)
		width += ImGui::CalcTextSize(label).x + style.ItemSpacing.x * 2.0f + style.FramePadding.x * 2.0f;
	return width;
}
float BaseViewportPanel::GetCustomOptionsWidth() const
{
	return MeasureOptionLabels({ LOCAL_CSTR("Viewport.Resolution"), LOCAL_CSTR("Viewport.RenderTargetsMenu") });
}
void BaseViewportPanel::DrawOptionsBar()
{
	if (!ImGui::BeginMenuBar()) return;

	const float channelsWidth = ChannelFilter::GetButtonsWidth();
	const float projectionWidth = GetProjectionButtonsWidth();
	const float gap = ImGui::GetStyle().ItemSpacing.x;
	const float togglesWidth = channelsWidth + (projectionWidth > 0.0f ? projectionWidth + gap : 0.0f);
	const float right = std::min(ImGui::GetWindowDrawList()->GetClipRectMax().x,
		ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x) - gap;
	const auto alignRight = [](float edge, float width)
	{
		const float offset = edge - ImGui::GetCursorScreenPos().x - width;
		if (offset > 0.0f) ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);
	};

	if (GetCustomOptionsWidth() + togglesWidth + gap <= right - ImGui::GetCursorScreenPos().x)
		DrawCustomOptions();
	else if (ImGui::BeginMenu(ICON_FA_SLIDERS "###ViewportOptions"))
	{
		DrawCustomOptions();
		ImGui::EndMenu();
	}

	const bool fitsMenuBar = togglesWidth <= right - ImGui::GetCursorScreenPos().x;
	if (fitsMenuBar)
	{
		alignRight(right, togglesWidth);
		m_ChannelFilter.DrawButtons();
		if (projectionWidth > 0.0f)
		{
			ImGui::SameLine(0.0f, gap);
			DrawProjectionButtons();
		}
	}
	ImGui::EndMenuBar();
	if (fitsMenuBar) return;

	// 좁은 창에서도 직접 조작할 수 있게 토글을 메뉴 아래에 배치한다.
	const float bodyRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x - gap;
	const bool fitsOneRow = togglesWidth <= bodyRight - ImGui::GetCursorScreenPos().x;
	alignRight(bodyRight, fitsOneRow ? togglesWidth : channelsWidth);
	ImGui::BeginGroup();
	m_ChannelFilter.DrawButtons();
	ImGui::EndGroup();
	if (projectionWidth > 0.0f)
	{
		if (fitsOneRow) ImGui::SameLine();
		alignRight(bodyRight, projectionWidth);
		DrawProjectionButtons();
	}
}
void BaseViewportPanel::DrawCustomOptions()
{
	DrawResolutionMenu();
	DrawRenderTargetMenu();
}
void BaseViewportPanel::DrawRenderTargetMenu()
{
	
	if (ImGui::BeginMenu(LOCAL_CSTR("Viewport.RenderTargetsMenu")))
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
			desc.InitialRenderTargetName = rtName;

			auto* panel = new BaseViewportPanel();
			if (const EResult result = panel->Initialize(&desc); result != EResult::Success)
			{
				Safe_Release(panel);
				EDITOR_LOG_ERROR("Render target viewer initialization failed: {}", ResultToString(result));
			}
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
	if (!ImGui::BeginMenu(LOCAL_LABEL("Viewport.Resolution").c_str()))
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