#pragma once

#include "CameraViewportPanel.h"
#include "CameraManager.h"
#include "InspectorPanel.h"

#pragma region Constructor&Destructor
void CameraViewportPanel::Initialize(void* arg)
{
	if (!arg) return;
	CAST_DESC
	m_Name = desc->Name;
	

	if (desc->IsSceneCamera)
	{
		SetCamera(Engine::CameraManager::Get().GetMainCamera());
	}
	else
	{
		tagCameraDesc cameraDesc;
		cameraDesc.IsPerspective = desc->IsPerspective;
		cameraDesc.Aspect = static_cast<f32>(desc->RenderTargetWidth) / static_cast<f32>(desc->RenderTargetHeight);
		cameraDesc.OrthoSize = desc->RenderTargetHeight;
		m_OwnedCamera = EditorCamera::Create(&cameraDesc);
		if (m_OwnedCamera)
		{
			m_OwnedCamera->SetName(m_Name + L"_Camera");
			SetCamera(m_OwnedCamera->GetCamera());
		}
		m_InspectorPanel = new InspectorPanel();
		m_InspectorPanel->SetSelectedGameObject(m_OwnedCamera);
		m_InspectorPanel->Close();
	}
}

void CameraViewportPanel::Free()
{
	Safe_Release(m_InspectorPanel);
	Safe_Release(m_OwnedCamera);
	m_Camera = nullptr;
	BaseViewportPanel::Free();
}
#pragma endregion


#pragma region Loop
void CameraViewportPanel::Update(f32 dt)
{
	CalculateRenderResolution(m_PanelWidth, m_PanelHeight);

	if (m_OwnedCamera && m_Focused)
		m_OwnedCamera->HandleInput(dt);

	if (m_OwnedCamera)
	{
		m_OwnedCamera->FixedUpdate(dt);
		m_OwnedCamera->Update(dt);
		m_OwnedCamera->LateUpdate(dt);
	}

	if (m_Camera && m_RenderHeight > 0)
		m_Camera->SetAspect(
			static_cast<f32>(m_RenderWidth) / static_cast<f32>(m_RenderHeight));

	if (m_ChannelView != EViewportChannelView::RGBA)
	{
		m_ChannelFilter.SetChannelView(m_ChannelView);
		m_ChannelFilter.SubmitChannelPreviewPass(m_SelectedRTName, m_DisplayRTName);
	}
}
void CameraViewportPanel::Draw()
{
	BaseViewportPanel::Draw();
	if(m_InspectorPanel && m_InspectorPanel->IsOpen())
		m_InspectorPanel->Draw();
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
	if (m_InspectorPanel) DrawCameraMenu();
	DrawChannelViewButton();
	DrawDimensionToggleButton();
}
#pragma endregion
void CameraViewportPanel::DrawCameraMenu()
{
	if (ImGui::Button("Camera"))
	{
		m_InspectorPanel->ToggleOpen();
	}
}

#pragma region Dimension 
void CameraViewportPanel::DrawDimensionToggleButton()
{
	// 1. 버튼 크기 및 그룹 전체 너비 계산
	ImVec2 buttonSize(30.0f, 0.0f); // 너비 30px 고정 (텍스트에 맞춰 조절 가능)
	float groupWidth = buttonSize.x * 2.0f;
	float rightPadding = 10.0f; // 우측 여백

	// 2. 커서를 우측 끝으로 이동 (윈도우 너비 - 그룹 너비 - 여백)
	ImGui::SameLine(ImGui::GetWindowWidth() - groupWidth - rightPadding);

	// 3. 스타일 적용: 간격 제거 및 각진 모서리 (토글 느낌)
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f); // 네모난 버튼

	// 4. [2D 버튼] 그리기
	// 활성화 상태(2D)면 'Active' 색상, 아니면 기본 배경보다 어두운 색상 사용
	if (IsOrthographic())
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

	if (ImGui::Button("2D", buttonSize))
	{
		SetOrthographic();
	}
	ImGui::PopStyleColor(); // 색상 복구

	ImGui::SameLine(); // 옆에 바로 붙이기

	// 5. [3D 버튼] 그리기
	// 비활성화 상태(!2D)면 'Active' 색상
	if (IsPerspective())
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

	if (ImGui::Button("3D", buttonSize))
	{
		SetPerspective();
	}
	ImGui::PopStyleColor(); // 색상 복구

	// 6. 스타일 복구
	ImGui::PopStyleVar(2); // ItemSpacing, FrameRounding
}
#pragma endregion

