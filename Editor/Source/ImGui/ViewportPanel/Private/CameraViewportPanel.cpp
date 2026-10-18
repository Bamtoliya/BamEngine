#pragma once

#include "CameraViewportPanel.h"
#include "EditorCameraController.h"

#pragma region Constructor&Destructor
void CameraViewportPanel::Initialize(void* arg)
{
	if (!arg) return;
	__super::Initialize(arg);
	CAST_DESC

	Engine::CameraComponent cameraSettings{};
	cameraSettings.CameraType = desc->IsPerspective ? Engine::ECameraType::Perspective : Engine::ECameraType::Orthographic;
	cameraSettings.AspectRatio = static_cast<f32>(desc->RenderTargetWidth) / static_cast<f32>(desc->RenderTargetHeight);
	cameraSettings.OrthographicSize = static_cast<f32>(desc->RenderTargetHeight);

	m_CameraController = EditorCameraController::Create(m_Name + L"_Camera", cameraSettings);
	m_OwnCamera = (m_CameraController != nullptr);
}

void CameraViewportPanel::Free()
{
	Safe_Release(m_CameraController);
	m_CameraController = nullptr;
	BaseViewportPanel::Free();
}
#pragma endregion

#pragma region Loop
void CameraViewportPanel::Update(f32 dt)
{
	CalculateRenderResolution(m_PanelWidth, m_PanelHeight);

	if (m_CameraController && m_Focused)
		m_CameraController->HandleInput(dt);

	if (m_CameraController && m_RenderHeight > 0)
	{
		m_CameraController->SetAspectRatio(static_cast<f32>(m_RenderWidth) / static_cast<f32>(m_RenderHeight));
		m_CameraController->RefreshMatrices();
	}

	if (m_ChannelView != EViewportChannelView::RGBA)
	{
		m_ChannelFilter.SetChannelView(m_ChannelView);
		m_ChannelFilter.SubmitChannelPreviewPass(m_SelectedRTName, m_DisplayRTName);
	}
}

void CameraViewportPanel::Draw()
{
	BaseViewportPanel::Draw();
}
#pragma endregion

#pragma region Custom Draws
void CameraViewportPanel::DrawCustomViewport()
{
	DrawRenderTargetImage(m_DisplayRTName);
}

void CameraViewportPanel::DrawCustomOptions()
{
	DrawResolutionMenu();
	DrawRenderTargetMenu();
	DrawChannelViewButton(30.0f * 2.0f + 8.0f);
	DrawDimensionToggleButton();
}
#pragma endregion

#pragma region Camera Management
bool CameraViewportPanel::IsPerspective() const
{
	return m_CameraController != nullptr && m_CameraController->IsPerspective();
}

bool CameraViewportPanel::IsOrthographic() const
{
	return m_CameraController != nullptr && m_CameraController->IsOrthographic();
}

void CameraViewportPanel::SetPerspective(bool perspective)
{
	if (m_CameraController != nullptr)
		m_CameraController->SetPerspective(perspective);
}

void CameraViewportPanel::SetOrthographic(bool orthographic)
{
	if (m_CameraController != nullptr)
		m_CameraController->SetOrthographic(orthographic);
}
#pragma endregion

#pragma region Dimension 
void CameraViewportPanel::DrawDimensionToggleButton()
{
	ImVec2 buttonSize(30.0f, 0.0f);
	float groupWidth = buttonSize.x * 2.0f;
	float rightPadding = 10.0f;

	ImGui::SameLine(ImGui::GetWindowWidth() - groupWidth - rightPadding);

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

	if (IsOrthographic())
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

	if (ImGui::Button("2D", buttonSize))
		SetOrthographic();
	ImGui::PopStyleColor();

	ImGui::SameLine();

	if (IsPerspective())
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

	if (ImGui::Button("3D", buttonSize))
		SetPerspective();
	ImGui::PopStyleColor();

	ImGui::PopStyleVar(2);
}
#pragma endregion