#pragma once

#include "ViewportPanel.h"

#include "imgui.h"
#include "ImViewGuizmo.h"

#include "CameraManager.h"
#include "SelectionManager.h"
#include "InputManager.h"

#pragma region Helper
static quat ExtractRotationQuat(const mat4& matrix)
{
	vec3 scale;
	vec3 translation;
	vec3 skew;
	vec4 perspective;
	quat rotation;

	glm::decompose(matrix, scale, rotation, translation, skew, perspective);
	return glm::normalize(rotation);
}

static vec3 ExtractDeltaEulerDegrees(const mat4& deltaMatrix)
{
	const quat deltaRotation = ExtractRotationQuat(deltaMatrix);
	return glm::degrees(glm::eulerAngles(deltaRotation));
}
#pragma endregion



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
			cameraDesc.OrthoSize = desc->RenderTargetHeight;
		}
		m_EditorCamera = EditorCamera::Create(&cameraDesc);
		m_EditorCamera->SetName(m_Name + L"_Camera");

#pragma endregion

		auto& rtMgr = RenderTargetManager::Get();
		auto& rpMgr = RenderPassManager::Get();
		uint32 w = desc->RenderTargetWidth;
		uint32 h = desc->RenderTargetHeight;
		wstring prefix = m_Name + L"_";

		struct { const wchar_t* suffix; ETextureFormat fmt; } gBufferDefs[] = {
					{ L"GBuffer_Diffuse",  ETextureFormat::R8G8B8A8_UNORM },
					{ L"GBuffer_Normal",   ETextureFormat::R16G16B16A16_FLOAT },
					{ L"GBuffer_PBR",      ETextureFormat::R8G8B8A8_UNORM },
					{ L"GBuffer_Emission", ETextureFormat::R8G8B8A8_UNORM },
					{ L"GBuffer_Position", ETextureFormat::R16G16B16A16_FLOAT },
		};
		vector<wstring> gBufferNames;
		for (auto& def : gBufferDefs)
		{
			wstring name = prefix + def.suffix;
			tagRenderTargetDesc rtDesc = {};
			rtDesc.Name = name;
			rtDesc.Width = w;
			rtDesc.Height = h;
			rtDesc.Format = def.fmt;
			rtDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource
				| ERenderTargetBindFlag::RTBF_RenderTarget;
			rtMgr.CreateRenderTarget(&rtDesc);
			gBufferNames.push_back(name);
			m_OwnedRTNames.push_back(name);
		}
		// ── Depth ──
		wstring depthName = prefix + L"Depth";
		tagRenderTargetDesc depthDesc = {};
		depthDesc.Name = depthName;
		depthDesc.Width = w;
		depthDesc.Height = h;
		depthDesc.Format = ETextureFormat::D24_UNORM_S8_UINT;
		depthDesc.Type = ERenderTargetType::DepthStencil;
		depthDesc.Usage = ETextureUsage::DepthStencilTarget;
		depthDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource
			| ERenderTargetBindFlag::RTBF_DepthStencil;
		rtMgr.CreateRenderTarget(&depthDesc);
		m_OwnedRTNames.push_back(depthName);
		// ── FinalColor ──
		m_FinalColorName = prefix + L"FinalColor";
		tagRenderTargetDesc finalDesc = {};
		finalDesc.Name = m_FinalColorName;
		finalDesc.Width = w;
		finalDesc.Height = h;
		finalDesc.ClearColor = vec4(0.5f, 0.f, 0.0f, 1.0f);
		finalDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource
			| ERenderTargetBindFlag::RTBF_RenderTarget;
		rtMgr.CreateRenderTarget(&finalDesc);
		m_OwnedRTNames.push_back(m_FinalColorName);
		// ── Pass 등록 ──
		m_GeometryPassID = rpMgr.RegisterRenderPass(
			prefix + L"GeometryPass",
			gBufferNames, depthName,
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.f, 0.f, -1.f), 0, ERenderSortType::FrontToBack);
		m_LightingPassID = rpMgr.RegisterRenderPass(
			prefix + L"LightingPass",
			{ m_FinalColorName }, L"",
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.0f, 0.0f, -1.0f), 100, ERenderSortType::None);
#ifdef _DEBUG
		m_DebugPassID = rpMgr.RegisterRenderPass(
			L"Debug" + prefix + L"Pass",
			{ m_FinalColorName }, depthName,
			ERenderPassLoadOperation::RPLO_Load,
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f), 500, ERenderSortType::None);
#endif
		m_DisplayRenderTargetName = gBufferNames[0]; // 초기: GBuffer_Diffuse
	}

	m_InspectorPanel = new InspectorPanel();
	m_InspectorPanel->Close();
	m_InspectorPanel->SetSelectedGameObject(m_EditorCamera);
	m_Grid.Initialize();
	

	ResourceManager& rm = ResourceManager::Get();
	tagRHIPipelineDesc pipelineDesc = {};
	pipelineDesc.PipelineType = EPipelineType::Graphics;
	pipelineDesc.VertexShader = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS")->GetRHIShader();
	pipelineDesc.PixelShader = rm.GetResourceHandle<Shader>(L"LightingPS")->GetRHIShader();
	pipelineDesc.ColorAttachmentCount = 1;
	pipelineDesc.ColorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
	pipelineDesc.DepthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
	pipelineDesc.DepthStencilState.DepthTestEnable = false;
	pipelineDesc.DepthStencilState.DepthWriteEnable = false;
	pipelineDesc.Topology = ETopology::TriangleList;
	pipelineDesc.CullMode = ECullMode::None;
	pipelineDesc.BlendMode = EBlendMode::Opaque;
	m_LightingPipeline = PipelineManager::Get().GetOrCreatePipeline(pipelineDesc);
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
	Renderer::Get().RegisterViewportCamera(m_EditorCamera->GetCamera(), m_GeometryPassID);
	Renderer::Get().RegisterViewportCamera(m_EditorCamera->GetCamera(), m_DebugPassID);
	m_Grid.SubmitGrid(m_GeometryPassID, m_IsOrthographic);
	SubmitLightingPass();
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

		RenderTarget* currentRT = RenderTargetManager::Get().GetRenderTarget(m_DisplayRenderTargetName);
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
void ViewportPanel::DrawGuizmo(ImVec2 pos, ImVec2 size)
{
	GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection();
	if (!selectedObject) return;

	Transform* transform = selectedObject->GetComponent<Transform>();
	if (!transform) return;

	// 1. ImGuizmo 초기 설정
	ImGuizmo::SetOrthographic(m_IsOrthographic);
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(pos.x, pos.y, size.x, size.y);
	ImGuizmo::SetID(ImGui::GetID(this));

	// 2. 카메라 및 매트릭스 준비
	Camera* camera = m_EditorCamera->GetCamera();
	mat4 projMatrix = camera->GetProjMatrix();
	mat4 viewMatrix = camera->GetViewMatrix();
	mat4 worldMatrix = transform->GetWorldMatrix();
	mat4 deltaMatrix = glm::identity<mat4>();

	// [Helper] 월드 매트릭스를 로컬 매트릭스로 변환하는 람다 함수 (중복 제거)
	auto getLocalMatrix = [](GameObject* obj, const mat4& wMatrix) -> mat4 {
		GameObject* parent = obj->GetParent();
		if (parent && parent->GetTransform())
			return glm::inverse(parent->GetTransform()->GetWorldMatrix()) * wMatrix;
		return wMatrix;
		};

	// 3. 조작 전(Old) 로컬 회전값 백업 (Euler Flip 방지용)
	mat4 oldLocalMatrix = getLocalMatrix(selectedObject, worldMatrix);
	quat oldLocalQuat = ExtractRotationQuat(oldLocalMatrix);

	// 4. 기즈모 단축키 및 스냅 설정
	if (ImGui::IsWindowFocused())
	{
		if (KEY_PRESSED(Engine::EKeyCode::W)) m_GizmoOperation = ImGuizmo::TRANSLATE;
		if (KEY_PRESSED(Engine::EKeyCode::E)) m_GizmoOperation = ImGuizmo::ROTATE;
		if (KEY_PRESSED(Engine::EKeyCode::R)) m_GizmoOperation = ImGuizmo::SCALE;
	}

	bool snap = m_GizmoUseSnap || ImGui::GetIO().KeyCtrl;
	vec3 snapValues = m_GizmoSnapTranslation;
	if (m_GizmoOperation == ImGuizmo::ROTATE) snapValues = m_GizmoSnapRotation;
	else if (m_GizmoOperation == ImGuizmo::SCALE) snapValues = m_GizmoSnapScale;

	// 5. 기즈모 조작 렌더링 및 연산
	ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projMatrix),
		m_GizmoOperation, m_GizmoMode,
		glm::value_ptr(worldMatrix), glm::value_ptr(deltaMatrix),
		snap ? glm::value_ptr(snapValues) : nullptr);

	if (ImGuizmo::IsUsing())
	{
		// 조작 후(New) 로컬 매트릭스
		mat4 localMatrix = getLocalMatrix(selectedObject, worldMatrix);

		if (m_GizmoOperation == ImGuizmo::ROTATE)
		{
			// 쿼터니언을 이용해 순수 회전 변화량(Delta) 추출 후 기존 오일러에 누적 (연속성 보장)
			quat newLocalQuat = ExtractRotationQuat(localMatrix);
			quat deltaLocalQuat = glm::inverse(oldLocalQuat) * newLocalQuat;
			vec3 deltaEuler = glm::degrees(glm::eulerAngles(deltaLocalQuat));

			transform->SetRotation(transform->GetLocalRotationEuler() + deltaEuler);
		}
		else
		{
			// 이동 및 크기 조절은 로컬 매트릭스 분해값을 그대로 적용
			float matrixTranslation[3], matrixRotation[3], matrixScale[3];
			ImGuizmo::DecomposeMatrixToComponents(glm::value_ptr(localMatrix), matrixTranslation, matrixRotation, matrixScale);

			transform->SetPosition(glm::make_vec3(matrixTranslation));
			transform->SetScale(glm::make_vec3(matrixScale));
		}
	}
}
#pragma endregion

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
			// ── 1. 콤보박스: 이 뷰포트의 RT (짧은 이름) ──
			wstring prefix = m_Name + L"_";
			int currentIdx = 0;
			vector<string> shortLabels;
			for (int i = 0; i < m_OwnedRTNames.size(); ++i)
			{
				wstring shortW = m_OwnedRTNames[i];
				if (shortW.find(prefix) == 0)
					shortW = shortW.substr(prefix.size());
				shortLabels.push_back(WStrToStr(shortW));
				if (m_OwnedRTNames[i] == m_DisplayRenderTargetName)
					currentIdx = i;
			}
			// 현재 선택이 OwnedRT가 아니면 "Other" 표시
			const char* preview = currentIdx < shortLabels.size()
				? shortLabels[currentIdx].c_str() : "Other";
			if (ImGui::BeginCombo("##RTSelect", preview))
			{
				for (int i = 0; i < shortLabels.size(); ++i)
				{
					bool selected = (m_OwnedRTNames[i] == m_DisplayRenderTargetName);
					if (ImGui::Selectable(shortLabels[i].c_str(), selected))
						m_DisplayRenderTargetName = m_OwnedRTNames[i];
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			// ── 2. 접힌 메뉴: 전체 RT (풀 이름) ──
			if (ImGui::BeginMenu("All RenderTargets"))
			{
				auto allNames = RenderTargetManager::Get().GetAllRenderTargetNames();
				for (const auto& name : allNames)
				{
					string label = WStrToStr(name);
					bool selected = (name == m_DisplayRenderTargetName);
					if (ImGui::MenuItem(label.c_str(), nullptr, selected))
						m_DisplayRenderTargetName = name;
				}
				ImGui::EndMenu();
			}
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
	if (MOUSE_BUTTON_DOWN(Engine::EMouseButton::Left) &&
		ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered() && !ImGuizmo::IsOver())
	{
		Ray mouseRay = ScreenPosToRay(mousePos, imageMin, imageSize);
		SelectionManager& selectionManager = SelectionManager::Get();
		GameObject* pickedObject = selectionManager.PickObjectByRay(mouseRay);
		if (pickedObject)
		{
			if (KEY_PRESSED(EKeyCode::LCtrl))
			{
				selectionManager.ToggleSelection(pickedObject);
			}
			else
			{
				selectionManager.SetSelectedObject(pickedObject);
			}
			ImGui::SetWindowFocus();
		}
		else
		{
			selectionManager.ClearSelection();
		}
	}

	SDL_Window* window = static_cast<SDL_Window*>(Renderer::Get().GetRHI()->GetWindowHandle());
	
	if (MOUSE_BUTTON_DOWN("right"))
	{
		m_InitialMousePos = InputManager::Get().GetMousePosition();
	}
	else if (MOUSE_BUTTON_PRESSED("right"))
	{
		SDL_SetWindowRelativeMouseMode(window, true);
		ImGui::GetIO().MousePos = ImVec2(-FLT_MAX, -FLT_MAX);
	}
	else if (MOUSE_BUTTON_UP("right")) 
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

	vec4 nearPointNDC = vec4(ndcX, ndcY, -1.0f, 1.0f);
	vec4 farPointNDC = vec4(ndcX, ndcY, 1.0f, 1.0f);

	vec4 nearPointWorld = invVP * nearPointNDC;
	vec4 farPointWorld = invVP * farPointNDC;

	Ray ray;
	ray.Origin = vec3(nearPointWorld) / nearPointWorld.w;
	//ray.Origin.x += m_RenderTarget->GetWidth() / 2.f;
	//ray.Origin.y -= m_RenderTarget->GetHeight() / 2.f;
	ray.Direction = glm::normalize(vec3(farPointWorld / farPointWorld.w) - ray.Origin);

	cout << "Ray Origin: " << ray.Origin.x << ", " << ray.Origin.y << ", " << ray.Origin.z << endl;
	cout << "Ray Direction: " << ray.Direction.x << ", " << ray.Direction.y << ", " << ray.Direction.z << endl;

	return ray;
}
#pragma endregion

#pragma region Rendering
void ViewportPanel::SubmitLightingPass()
{
	Renderer::Get().SubmitCustomCommand(
		[this](f32 dt, RenderPass* pass) -> EResult
		{
			auto* rhi = Renderer::Get().GetRHI();
			auto& rtMgr = RenderTargetManager::Get();
			auto& smMgr = SamplerManager::Get();
			RHISampler* sampler = smMgr.GetDefaultSampler();
			wstring prefix = m_Name + L"_";
			tagCameraBuffer camBuf = m_EditorCamera->GetCamera()->GetCameraBuffer();
			camBuf.time = dt;
			rhi->BindConstantBuffer(&camBuf, sizeof(tagCameraBuffer), 0);
			rhi->BindConstantBuffer(&camBuf, sizeof(tagCameraBuffer), 3);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_Diffuse")->GetTexture(),
				sampler, 0);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_Normal")->GetTexture(),
				sampler, 1);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_PBR")->GetTexture(),
				sampler, 2);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_Emission")->GetTexture(),
				sampler, 3);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_Position")->GetTexture(),
				sampler, 4);
			// Lighting 파이프라인 바인딩
			rhi->BindPipeline(m_LightingPipeline);
			// 풀스크린 삼각형 (버텍스 버퍼 없이 3개 그리기)
			rhi->Draw(3);
			return EResult::Success;
		},
		m_LightingPassID);
}
#pragma endregion
