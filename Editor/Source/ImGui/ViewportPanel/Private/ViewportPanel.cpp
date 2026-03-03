#pragma once

#include "ViewportPanel.h"

#include "imgui.h"
#include "ImViewGuizmo.h"

#include "CameraManager.h"
#include "SelectionManager.h"


#pragma region Contructor&Destructor
void ViewportPanel::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		m_Name = desc->Name;

#pragma region Prepare Camera
		tagCameraDesc cameraDesc;
		if (desc->CameraType == EViewportCameraType::Orthographic)
		{
			m_IsOrthographic = true;
			cameraDesc.IsPerspective = false;
			cameraDesc.Width = desc->RenderTargetWidth;
			cameraDesc.Height = desc->RenderTargetHeight;
		}
		m_EditorCamera = EditorCamera::Create(&cameraDesc);
		m_EditorCamera->SetName(m_Name + L"_Camera");

#pragma endregion

#pragma region Prepare Color RenderTarget
		tagRenderTargetDesc rtDesc;
		rtDesc.Width = desc->RenderTargetWidth;
		rtDesc.Height = desc->RenderTargetHeight;
		rtDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource | ERenderTargetBindFlag::RTBF_RenderTarget;
		rtDesc.ClearColor = vec4(0.4f, 0.1f, 0.1f, 1.0f);
		rtDesc.Name = m_Name + L"_RenderTarget";
		m_RenderTarget = RenderTargetManager::Get().CreateRenderTarget(&rtDesc);
#pragma endregion


#pragma region Prepare Depth Stencil RenderTarget
		tagRenderTargetDesc depthStencilDesc;
		depthStencilDesc.Width = desc->RenderTargetWidth;
		depthStencilDesc.Height = desc->RenderTargetHeight;
		depthStencilDesc.Type = ERenderTargetType::DepthStencil;
		depthStencilDesc.Usage = ERenderTargetUsage::RTU_DepthStencil;
		depthStencilDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource | ERenderTargetBindFlag::RTBF_DepthStencil | ERenderTargetBindFlag::RTBF_RenderTarget;
		depthStencilDesc.Format = ERenderTargetFormat::RTF_DEPTH24STENCIL8;
		depthStencilDesc.Name = m_Name + L"_DepthStencil";
		m_DepthStencil = RenderTargetManager::Get().CreateRenderTarget(&depthStencilDesc);
#pragma endregion
		wstring passName = m_Name + L"_Pass";
		m_PassID = RenderPassManager::Get().RegisterRenderPass(passName, { m_RenderTarget->GetName()}, m_DepthStencil->GetName(), ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store, vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, ERenderSortType::FrontToBack);

		wstring debugPassName = L"Debug" + passName;
		m_DebugPassID = RenderPassManager::Get().RegisterRenderPass(
			debugPassName,
			{ m_RenderTarget->GetName() },
			m_DepthStencil->GetName(),     // Depth Buffer
			ERenderPassLoadOperation::RPLO_Load,  // 매우 중요: 기존 화면(MainPass 결과) 유지
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.0f, 0.0f, 0.0f), 0,
			ERenderSortType::None  // 선분은 정렬이 크게 필요 없음
		);
	}

	m_InspectorPanel = new InspectorPanel();
	m_InspectorPanel->Close();
	m_InspectorPanel->SetSelectedGameObject(m_EditorCamera);
	m_Grid.Initialize();
}
void ViewportPanel::Free()
{
	Safe_Release(m_EditorCamera);
	Safe_Release(m_RenderTarget);
	Safe_Release(m_DepthStencil);
	Safe_Release(m_InspectorPanel);
	m_Grid.Free();
}
#pragma endregion

void ViewportPanel::Update(f32 dt)
{
	m_EditorCamera->FixedUpdate(dt);
	m_EditorCamera->Update(dt);
	m_EditorCamera->LateUpdate(dt);
	m_IsOrthographic = !m_EditorCamera->GetCamera()->GetIsPerspective();
	Renderer::Get().RegisterViewportCamera(m_EditorCamera->GetCamera(), m_PassID);
	Renderer::Get().RegisterViewportCamera(m_EditorCamera->GetCamera(), m_DebugPassID);
	m_Grid.SubmitGrid(m_PassID, m_IsOrthographic);
}

void ViewportPanel::Draw()
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
		ImVec2 panelSize = ImGui::GetContentRegionAvail();
		uint32 width = (uint32)panelSize.x;
		uint32 height = (uint32)panelSize.y;
		f32 panelAspectRatio = (f32)width / (f32)height;

		if (m_RenderTarget)
		{
			RHITexture* texture = m_RenderTarget->GetTexture();
			if (texture)
			{
				f32 imageWidth = (f32)m_RenderTarget->GetWidth();
				f32 imageHeight = (f32)m_RenderTarget->GetHeight();

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
				bool isFocused = ImGui::IsWindowFocused();
				bool isHovered = ImGui::IsWindowHovered();
				f32 dt = TimeManager::Get().GetDeltaTime();
				if(isFocused || isHovered)
				{
					m_EditorCamera->HandleInput(dt);
				}

				ImVec2 cursorStart = ImGui::GetCursorPos();
				float offsetX = (panelSize.x - finalSize.x) * 0.5f;
				float offsetY = (panelSize.y - finalSize.y) * 0.5f;
				ImGui::SetCursorPos(ImVec2((float)(int)(cursorStart.x + offsetX), (float)(int)(cursorStart.y + offsetY)));
				ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();

				ImTextureID textureID = (ImTextureID)(size_t)texture->GetNativeHandle();
				ImGui::Image(textureID, finalSize);
				DrawGuizmo(imageScreenPos, finalSize);
				MouseInput(ImGui::GetMousePos(), imageScreenPos, finalSize);
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	m_InspectorPanel->Draw();

	ImGui::PopID();
}

#pragma region Guizmo

#pragma endregion


void ViewportPanel::DrawGuizmo(ImVec2 pos, ImVec2 size)
{
	GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection();
	if (selectedObject == nullptr)
		return;
	ImGuizmo::SetOrthographic(m_IsOrthographic);
	ImGuizmo::SetDrawlist();

	ImVec2 windowPos = ImGui::GetWindowPos();
	ImVec2 windowSize = ImGui::GetWindowSize();
	ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);
	ImGuizmo::SetID(ImGui::GetID(this));

	f32 halfW = size.x;
	f32 halfH = size.y;

	Camera* camera = m_EditorCamera->GetCamera();
	mat4 projMatrix = camera->GetProjMatrix();
	mat4 viewMatrix = camera->GetViewMatrix();

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

	bool snap = m_GizmoUseSnap || ImGui::GetIO().KeyCtrl;
	vec3 snapValues = m_GizmoSnapTranslation;
	if (m_GizmoOperation == ImGuizmo::ROTATE)
		snapValues = m_GizmoSnapRotation;
	else if (m_GizmoOperation == ImGuizmo::SCALE)
		snapValues = m_GizmoSnapScale;
	ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projMatrix), m_GizmoOperation, m_GizmoMode, glm::value_ptr(worldMatrix), nullptr, snap ? glm::value_ptr(snapValues) : nullptr);

	if (ImGuizmo::IsUsing())
	{

		GameObject* parent = selectedObject->GetParent();
		mat4 localMatrix;
		if (parent)
		{
			Transform* parentTransform = parent->GetTransform();
			if(parentTransform)
			{
				mat4 parentWorldMatrix = parentTransform->GetWorldMatrix();
				mat4 inverseParentWorldMatrix = glm::inverse(parentWorldMatrix);
				localMatrix = inverseParentWorldMatrix * worldMatrix;
			}
			else
			{
				localMatrix = worldMatrix;
			}
		}
		else
		{
			localMatrix = worldMatrix;
		}
		float matrixTranslation[3], matrixRotation[3], matrixScale[3];
		ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMatrix), matrixTranslation, matrixRotation, matrixScale);



		// 2D이므로 Z값 변경을 막고 싶다면 여기서 matrixTranslation[2] = 0.0f; 처리를 할 수 있습니다.

		transform->SetPosition(glm::make_vec3(matrixTranslation));
		transform->SetRotation(glm::make_vec3(matrixRotation));
		transform->SetScale(glm::make_vec3(matrixScale));
	}
}

void ViewportPanel::SubmitGridRenderCommand(f32 dt)
{

}

#pragma region Options Bar
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

			ImGui::Separator();
			if (ImGui::MenuItem("Snap to Grid", nullptr, m_GizmoUseSnap))
				m_GizmoUseSnap = !m_GizmoUseSnap;
			if (m_GizmoUseSnap)
			{
				ImGui::InputFloat3("Snap Translation", glm::value_ptr(m_GizmoSnapTranslation));
				ImGui::InputFloat3("Snap Rotation", glm::value_ptr(m_GizmoSnapRotation));
				ImGui::InputFloat3("Snap Scale", glm::value_ptr(m_GizmoSnapScale));
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("RenderTarget"))
		{
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Grid"))
		{
			if (ImGui::MenuItem("Show Grid"))
			{
				m_Grid.Toggle();
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("TempMenu"))
		{
			ImGui::EndMenu();
		}

		if (ImGui::Button("Camera"))
		{
			m_InspectorPanel->ToggleOpen();
		}

		DrawDimensionToggleButton();
	}
	ImGui::EndMenuBar();
}
void ViewportPanel::DrawDimensionToggleButton()
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
	if (m_IsOrthographic)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

	if (ImGui::Button("2D", buttonSize))
	{
		m_IsOrthographic = true;

		// 카메라 설정을 즉시 변경하고 싶다면 여기서 호출
		if (m_EditorCamera) {
			m_EditorCamera->GetCamera()->SetPerspective(false);
		}
	}
	ImGui::PopStyleColor(); // 색상 복구

	ImGui::SameLine(); // 옆에 바로 붙이기

	// 5. [3D 버튼] 그리기
	// 비활성화 상태(!2D)면 'Active' 색상
	if (!m_IsOrthographic)
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

	if (ImGui::Button("3D", buttonSize))
	{
		m_IsOrthographic = false;

		// 카메라 설정을 즉시 변경하고 싶다면 여기서 호출
		if (m_EditorCamera) {
			m_EditorCamera->GetCamera()->SetPerspective();
		}
	}
	ImGui::PopStyleColor(); // 색상 복구

	// 6. 스타일 복구
	ImGui::PopStyleVar(2); // ItemSpacing, FrameRounding
}
#pragma endregion

#pragma region Input
void ViewportPanel::MouseInput(const ImVec2& mousePos, const ImVec2& imageMin, const ImVec2& imageSize)
{
	if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGuizmo::IsOver())
	{
		Ray mouseRay = ScreenPosToRay(mousePos, imageMin, imageSize);
		GameObject* pickedObject = SelectionManager::Get().PickObjectByRay(mouseRay);
		if (pickedObject)
		{
			// 선택된 객체가 있다면, 해당 객체를 선택 상태로 전환
			SelectionManager::Get().ToggleSelection(pickedObject);
			ImGui::SetWindowFocus();
		}
		else
		{
			SelectionManager::Get().ClearSelection();
		}
	}

	SDL_Window* window = static_cast<SDL_Window*>(Renderer::Get().GetRHI()->GetWindowHandle());
	if (ImGui::IsMouseClicked(1))
	{
		m_InitialMousePos = InputManager::Get().GetMousePosition();
	}
	else if (ImGui::IsMouseDown(1))
	{
		SDL_SetWindowRelativeMouseMode(window, true);
		ImGui::GetIO().MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	}
	else if (ImGui::IsMouseReleased(1))
	{
		SDL_SetWindowRelativeMouseMode(window, false);
		SDL_WarpMouseInWindow(window, (int)m_InitialMousePos.x, (int)m_InitialMousePos.y);
	}
}

Ray ViewportPanel::ScreenPosToRay(const ImVec2& mousePos, const ImVec2& imageMin, const ImVec2& imageSize)
{
	f32 localX = mousePos.x - imageMin.x;
	f32 localY = mousePos.y - imageMin.y;

	f32 ndcX = (localX / imageSize.x) * 2.0f - 1.0f;
	f32 ndcY = 1.0f - (localY / imageSize.y) * 2.0f;

	mat4 projInvMatrix = m_EditorCamera->GetCamera()->GetProjMatrixInv();
	mat4 viewInvMatrix = m_EditorCamera->GetCamera()->GetViewMatrixInv();

	mat4 invVP = viewInvMatrix * projInvMatrix;

	vec4 nearPointNDC = vec4(ndcX, ndcY, 0.0f, 1.0f);
	vec4 farPointNDC = vec4(ndcX, ndcY, 1.0f, 1.0f);

	vec4 nearPointWorld = invVP * nearPointNDC;
	vec4 farPointWorld = invVP * farPointNDC;

	Ray ray;
	ray.Origin = vec3(nearPointWorld) / nearPointWorld.w;
	//ray.Origin.x += m_RenderTarget->GetWidth() / 2.f;
	//ray.Origin.y -= m_RenderTarget->GetHeight() / 2.f;
	ray.Direction = glm::normalize(vec3(farPointWorld / farPointWorld.w - vec4(ray.Origin, 0.f)));

	cout << "Ray Origin: " << ray.Origin.x << ", " << ray.Origin.y << ", " << ray.Origin.z << endl;
	cout << "Ray Direction: " << ray.Direction.x << ", " << ray.Direction.y << ", " << ray.Direction.z << endl;

	return ray;
}
#pragma endregion