#pragma once

#include "ViewportPanel.h"

#include "imgui.h"
#include "ImViewGuizmo.h"

#include "CameraManager.h"


#pragma region Contructor&Destructor
void ViewportPanel::Initialize(void* arg)
{
}
#pragma endregion

void ViewportPanel::Draw()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bool opened = true;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
	if (ImGui::Begin("Viewport", &opened, window_flags))
	{
		DrawOptionsBar();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();
		uint32 width = (uint32)panelSize.x;
		uint32 height = (uint32)panelSize.y;
		f32 panelAspectRatio = (f32)width / (f32)height;

		RenderTarget* target = RenderTargetManager::Get().GetRenderTarget(L"RenderTarget_1");
		if (target)
		{
			RHITexture* texture = target->GetTexture();
			if (texture)
			{
				f32 imageWidth = (f32)target->GetWidth();
				f32 imageHeight = (f32)target->GetHeight();

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

				//if (panelSize.x > 0 && panelSize.y > 0)
				//{
				//	Renderer::Get().GetRHI()->Resize((uint32)finalSize.x, (uint32)finalSize.y);
				//}

				ImVec2 cursorStart = ImGui::GetCursorPos();
				float offsetX = (panelSize.x - finalSize.x) * 0.5f;
				float offsetY = (panelSize.y - finalSize.y) * 0.5f;
				ImGui::SetCursorPos(ImVec2((float)(int)(cursorStart.x + offsetX), (float)(int)(cursorStart.y + offsetY)));

				ImTextureID textureID = (ImTextureID)(size_t)texture->GetNativeHandle();
				ImGui::Image(textureID, finalSize); \

				DrawGuizmo(ImVec2(imageWidth, imageHeight));
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();
}

void ViewportPanel::DrawGuizmo(ImVec2 size)
{
	GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection();
	if (selectedObject == nullptr)
		return;
	ImGuizmo::SetOrthographic(m_IsOrthographic);
	ImGuizmo::SetDrawlist();

	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 imageMin = ImGui::GetItemRectMin();
	ImVec2 imageMax = ImGui::GetItemRectMax();

	ImGuizmo::SetRect(imageMin.x, imageMin.y, imageMax.x - imageMin.x, imageMax.y - imageMin.y);

	f32 halfW = size.x;
	f32 halfH = size.y;

	Camera* mainCamera = CameraManager::Get().GetMainCamera();
	mat4 projMatrix = mainCamera->GetProjMatrix();
	mat4 viewMatrix = mainCamera->GetViewMatrix();

	Transform* transform = selectedObject->GetComponent<Transform>();
	if (!transform)
		return;
	mat4 worldMatrix = transform->GetWorldMatrix();

	if (ImGui::IsWindowFocused())
	{
		if (ImGui::IsKeyPressed(ImGuiKey_W)) m_GizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::IsKeyPressed(ImGuiKey_E)) m_GizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::IsKeyPressed(ImGuiKey_R)) m_GizmoOperation = ImGuizmo::SCALE;
	}
	
	ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projMatrix), m_GizmoOperation, m_GizmoMode, glm::value_ptr(worldMatrix));

	if (ImGuizmo::IsUsing())
	{
		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(worldMatrix), matrixTranslation, matrixRotation, matrixScale);

		// 2D이므로 Z값 변경을 막고 싶다면 여기서 matrixTranslation[2] = 0.0f; 처리를 할 수 있습니다.

		transform->SetPosition(glm::make_vec3(matrixTranslation));
		transform->SetRotation(glm::make_vec3(matrixRotation));
		transform->SetScale(glm::make_vec3(matrixScale));
	}
}

void ViewportPanel::DrawOptionsBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("Gizmo"))
		{
			if (ImGui::MenuItem("Translate (W)", "W", m_GizmoOperation == ImGuizmo::TRANSLATE))
				m_GizmoOperation = ImGuizmo::TRANSLATE;
			if (ImGui::MenuItem("Rotate (E)", "E", m_GizmoOperation == ImGuizmo::ROTATE))
				m_GizmoOperation = ImGuizmo::ROTATE;
			if (ImGui::MenuItem("Scale (R)", "R", m_GizmoOperation == ImGuizmo::SCALE))
				m_GizmoOperation = ImGuizmo::SCALE;
			ImGui::Separator();
			if (ImGui::MenuItem("Local", nullptr, m_GizmoMode == ImGuizmo::LOCAL))
				m_GizmoMode = ImGuizmo::LOCAL;
			if (ImGui::MenuItem("World", nullptr, m_GizmoMode == ImGuizmo::WORLD))
				m_GizmoMode = ImGuizmo::WORLD;
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("RenderTarget"))
		{

		}

		if (ImGui::BeginMenu("TempMenu"))
		{

		}

		if (ImGui::Button("TempButton"))
		{

		}
	}
	ImGui::EndMenuBar();
}
