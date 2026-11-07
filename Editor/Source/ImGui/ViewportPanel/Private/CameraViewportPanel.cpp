#pragma once

#include "CameraViewportPanel.h"
#include "EditorCameraController.h"
#include "IconsFontAwesome7.h"
#include "Components/Core/TransformComponent.h"
#include "EditorSettings.h"

#pragma region Constructor&Destructor
EResult CameraViewportPanel::OnInitialize(void* arg)
{
	CAST_DESC
	const EResult result = BaseViewportPanel::OnInitialize(static_cast<ViewportPanelDesc*>(desc));
	if (result != EResult::Success)
		return result;

	Engine::CameraComponent cameraSettings{};
	cameraSettings.CameraType = desc->IsPerspective ? Engine::ECameraType::Perspective : Engine::ECameraType::Orthographic;
	cameraSettings.AspectRatio = static_cast<f32>(desc->RenderTargetWidth) / static_cast<f32>(desc->RenderTargetHeight);
	cameraSettings.OrthographicSize = static_cast<f32>(desc->RenderTargetHeight);

	m_CameraController = EditorCameraController::Create(m_Name + L"_Camera", cameraSettings);
	m_OwnCamera = (m_CameraController != nullptr);
	return EResult::Success;
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
	if (!CalculateRenderResolution(m_PanelWidth, m_PanelHeight))
		return;

	if (m_Open && m_CameraController && m_Focused)
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
	DrawCameraMenu();
	const float dimGroupWidth = (ImGui::GetFrameHeight() * 1.1f) * 2.0f;
	const float groupSpacing = ImGui::GetStyle().ItemSpacing.x;
	DrawChannelViewButton(dimGroupWidth + groupSpacing);
	DrawDimensionToggleButton();
}
void CameraViewportPanel::DrawCameraMenu()
{
	if (!m_CameraController) return;
	auto* transform = m_CameraController->GetTransformComponent();
	auto* camera = m_CameraController->GetCameraComponent();
	if (!transform || !camera) return;

	const string menuTitle = string(ICON_FA_CAMERA " ") + LOCAL("Viewport.CameraMenu") + "###CameraMenu";
	if (ImGui::BeginMenu(menuTitle.c_str()))
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("Viewport.CameraTransform"));
		ImGui::Separator();

		// 1. Position 직접 수치 입력 및 드래그 (X, Y, Z)
		vec3 pos = transform->position;
		if (ImGui::DragFloat3(LOCAL_LABEL("Viewport.CameraPosition").c_str(), &pos.x, 0.5f))
		{
			if (transform->SetLocalPosition(pos))
				m_CameraController->RefreshMatrices();
		}

		// 2. Rotation 직접 수치 입력 (Pitch, Yaw, Roll)
		vec3 rot = transform->eulerRotation;
		if (ImGui::DragFloat3(LOCAL_LABEL("Viewport.CameraRotation").c_str(), &rot.x, 0.5f))
		{
			m_CameraController->SetControlRotation(rot);
		}

		ImGui::Spacing();
		ImGui::TextDisabled("%s", LOCAL_CSTR("Viewport.CameraOptics"));
		ImGui::Separator();

		// 3. FOV 또는 Orthographic Size 조절
		if (camera->CameraType == Engine::ECameraType::Perspective)
		{
			float fov = camera->FOV;
			if (ImGui::SliderFloat(LOCAL_LABEL("Viewport.CameraFOV").c_str(), &fov, 10.0f, 140.0f))
			{
				camera->FOV = fov;
				m_CameraController->RefreshMatrices();
			}
		}
		else
		{
			float size = camera->OrthographicSize;
			if (ImGui::DragFloat(LOCAL_LABEL("Viewport.CameraOrthographicSize").c_str(), &size, 1.0f, 1.0f, 10000.0f))
			{
				camera->OrthographicSize = size;
				m_CameraController->RefreshMatrices();
			}
		}

		// 4. 카메라 이동 속도 조절
		float speed = m_CameraController->GetCameraSpeed();
		if (ImGui::SliderFloat(LOCAL_LABEL("Viewport.CameraSpeed").c_str(), &speed, 1.0f, 500.0f))
		{
			m_CameraController->SetCameraSpeed(speed);
		}

		ImGui::Spacing();
		ImGui::Separator();

		// 5. 빠른 이동 프리셋 버튼
		if (ImGui::Button(LOCAL_LABEL("Viewport.CameraResetOrigin").c_str()))
		{
			if (transform->SetLocalPosition(vec3(0.0f, 5.0f, -10.0f)))
				m_CameraController->SetControlRotation(vec3(15.0f, 0.0f, 0.0f));
		}
		ImGui::SameLine();
		if (ImGui::Button(LOCAL_LABEL("Viewport.CameraTopView").c_str()))
		{
			if (transform->SetLocalPosition(vec3(pos.x, 50.0f, pos.z)))
				m_CameraController->SetControlRotation(vec3(90.0f, 0.0f, 0.0f));
		}
		ImGui::EndMenu();
	}
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
	const float btnHeight = ImGui::GetFrameHeight();
	const ImVec2 buttonSize(btnHeight * 1.1f, btnHeight);
	const float groupWidth = buttonSize.x * 2.0f;
	const float rightPadding = ImGui::GetStyle().WindowPadding.x;

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
void CameraViewportPanel::RestoreViewportState(const ViewportWorkspace& state)
{
	BaseViewportPanel::RestoreViewportState(state);

	if (!state.HasCamera || !m_CameraController)
		return;

	auto* transform = m_CameraController->GetTransformComponent();
	auto* camera = m_CameraController->GetCameraComponent();

	if (!transform || !camera)
		return;

	const auto finite = [](const std::array<float, 3>& values)
		{
			return std::all_of(values.begin(), values.end(), [](float value) { return std::isfinite(value); });
		};

	if (!finite(state.CameraPosition) || !finite(state.CameraRotation) ||
		!std::isfinite(state.FOV) || state.FOV <= 0.0f || state.FOV >= 180.0f ||
		!std::isfinite(state.OrthographicSize) || state.OrthographicSize <= 0.0f ||
		!std::isfinite(state.CameraSpeed) || state.CameraSpeed < 1.0f)
	{
		EDITOR_LOG_WARN("Invalid editor camera state: {}", WStrToStr(m_Name));
		return;
	}

	const auto previousTransform = *transform;
	const auto previousCamera = *camera;
	const float previousSpeed = m_CameraController->GetCameraSpeed();

	auto candidate = previousTransform;
	const vec3 position(state.CameraPosition[0], state.CameraPosition[1], state.CameraPosition[2]);
	const vec3 rotation(state.CameraRotation[0], state.CameraRotation[1], state.CameraRotation[2]);

	if (!candidate.SetLocalPosition(position) || !candidate.SetEulerRotation(rotation))
	{
		EDITOR_LOG_WARN("Editor camera transform restore failed: {}", WStrToStr(m_Name));
		return;
	}

	*transform = candidate;
	camera->CameraType = state.Perspective ? Engine::ECameraType::Perspective : Engine::ECameraType::Orthographic;
	camera->FOV = state.FOV;
	camera->OrthographicSize = state.OrthographicSize;

	m_CameraController->SetCameraSpeed(state.CameraSpeed);
	m_CameraController->SyncControlRotationFromTransform();
	m_CameraController->RefreshMatrices();

	if (!m_CameraController->GetCameraBuffer())
	{
		*transform = previousTransform;
		*camera = previousCamera;

		m_CameraController->SetCameraSpeed(previousSpeed);
		m_CameraController->SyncControlRotationFromTransform();
		m_CameraController->RefreshMatrices();

		EDITOR_LOG_WARN("Editor camera matrices could not be restored: {}", WStrToStr(m_Name));
	}
}

ViewportWorkspace CameraViewportPanel::CaptureViewportState() const
{
	auto state = BaseViewportPanel::CaptureViewportState();

	if (!m_CameraController)
		return state;

	const auto* transform = m_CameraController->GetTransformComponent();
	const auto* camera = m_CameraController->GetCameraComponent();

	if (!transform || !camera)
		return state;

	state.HasCamera = true;
	state.CameraPosition = { transform->position.x, transform->position.y, transform->position.z };
	state.CameraRotation = { transform->eulerRotation.x, transform->eulerRotation.y, transform->eulerRotation.z };
	state.Perspective = camera->CameraType == Engine::ECameraType::Perspective;
	state.FOV = camera->FOV;
	state.OrthographicSize = camera->OrthographicSize;
	state.CameraSpeed = m_CameraController->GetCameraSpeed();

	return state;
}
#pragma endregion