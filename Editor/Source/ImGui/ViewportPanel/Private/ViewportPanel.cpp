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

static bool WorldToViewportScreen(
	const vec3& worldPos,
	const mat4& viewProj,
	const ImVec2& imageScreenPos,
	const ImVec2& imageSize,
	ImVec2& outScreenPos)
{
	vec4 clip = viewProj * vec4(worldPos, 1.0f);

	if (glm::abs(clip.w) < 0.000001f || clip.w <= 0.0f)
		return false;

	vec3 ndc = vec3(clip) / clip.w;

	const bool inDepthGL = (ndc.z >= -1.0f && ndc.z <= 1.0f);
	const bool inDepthVK = (ndc.z >= 0.0f && ndc.z <= 1.0f);
	if (!(inDepthGL || inDepthVK))
		return false;

	if (ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f)
		return false;

	const float u = ndc.x * 0.5f + 0.5f;
	const float v = 1.0f - (ndc.y * 0.5f + 0.5f);

	outScreenPos.x = imageScreenPos.x + u * imageSize.x;
	outScreenPos.y = imageScreenPos.y + v * imageSize.y;
	return true;
}

static bool UnprojectFrustumCorner(
	const mat4& invViewProj,
	const vec3& ndcPos,
	vec3& outWorldPos)
{
	vec4 worldPos = invViewProj * vec4(ndcPos, 1.0f);
	if (glm::abs(worldPos.w) < 0.000001f)
		return false;

	outWorldPos = vec3(worldPos) / worldPos.w;
	return true;
}

static void DrawViewportLineIfVisible(
	ImDrawList* drawList,
	const vec3& worldStart,
	const vec3& worldEnd,
	const mat4& viewProj,
	const ImVec2& imageScreenPos,
	const ImVec2& imageSize,
	ImU32 color,
	float thickness)
{
	ImVec2 screenStart;
	ImVec2 screenEnd;

	if (!WorldToViewportScreen(worldStart, viewProj, imageScreenPos, imageSize, screenStart))
		return;
	if (!WorldToViewportScreen(worldEnd, viewProj, imageScreenPos, imageSize, screenEnd))
		return;

	drawList->AddLine(screenStart, screenEnd, color, thickness);
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
					{ L"GBuffer_Position", ETextureFormat::R32G32B32A32_FLOAT },
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
		// ── ShadowDepth ──
		m_ShadowDepthName = prefix + L"ShadowDepth";
		tagRenderTargetDesc shadowDepthDesc = {};
		shadowDepthDesc.Name = m_ShadowDepthName;
		shadowDepthDesc.Width = w;
		shadowDepthDesc.Height = h;
		shadowDepthDesc.Format = ETextureFormat::D32_FLOAT;
		shadowDepthDesc.Type = ERenderTargetType::DepthStencil;
		shadowDepthDesc.Usage = ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler;
		shadowDepthDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource
			| ERenderTargetBindFlag::RTBF_DepthStencil;
		rtMgr.CreateRenderTarget(&shadowDepthDesc);
		m_OwnedRTNames.push_back(m_ShadowDepthName);
		// ── Depth ──
		wstring depthName = prefix + L"Depth";
		tagRenderTargetDesc depthDesc = {};
		depthDesc.Name = depthName;
		depthDesc.Width = w;
		depthDesc.Height = h;
		depthDesc.Format = ETextureFormat::D24_UNORM_S8_UINT;
		depthDesc.Type = ERenderTargetType::DepthStencil;
		depthDesc.Usage = ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler;
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
		finalDesc.Format = ETextureFormat::R8G8B8A8_UNORM;
		finalDesc.ClearColor = vec4(0.5f, 0.f, 0.0f, 1.0f);
		finalDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource
			| ERenderTargetBindFlag::RTBF_RenderTarget;
		rtMgr.CreateRenderTarget(&finalDesc);
		m_OwnedRTNames.push_back(m_FinalColorName);
		// ── Channel Preview ──
		m_ChannelPreviewName = prefix + L"ChannelPreview";
		tagRenderTargetDesc channelPreviewDesc = {};
		channelPreviewDesc.Name = m_ChannelPreviewName;
		channelPreviewDesc.Width = w;
		channelPreviewDesc.Height = h;
		channelPreviewDesc.Format = ETextureFormat::R8G8B8A8_UNORM;
		channelPreviewDesc.ClearColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		channelPreviewDesc.BindFlag = ERenderTargetBindFlag::RTBF_ShaderResource
			| ERenderTargetBindFlag::RTBF_RenderTarget;
		rtMgr.CreateRenderTarget(&channelPreviewDesc);
		m_OwnedRTNames.push_back(m_ChannelPreviewName);


		// ── Pass 등록 ──
		m_GeometryPassID = rpMgr.RegisterRenderPass(
			prefix + L"GeometryPass",
			gBufferNames, depthName,
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.f, 0.f, -1.f), 0, ERenderSortType::FrontToBack);
		m_LightingPassID = rpMgr.RegisterRenderPass(
			prefix + L"LightingPass",
			{ m_FinalColorName }, L"",
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.0f, 0.0f, -1.0f), 100, ERenderSortType::None);
		m_ShadowPassID = rpMgr.RegisterRenderPass(
			prefix + L"ShadowPass",
			{},                          // color 타깃 없음 (depth-only)
			m_ShadowDepthName,
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.0f, 0.0f, -1.0f),
			0,                           // priority — geometry와 동일하지만 ID가 먼저여서 앞에 실행됨
			ERenderSortType::FrontToBack,
			ERenderPassType::Shadow);
		m_ChannelPreviewPassID = rpMgr.RegisterRenderPass(
			prefix + L"ChannelPreviewPass",
			{ m_ChannelPreviewName }, L"",
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Clear,
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.0f, 0.0f, 1.0f),
			150, ERenderSortType::None);
#ifdef _DEBUG
		m_DebugPassID = rpMgr.RegisterRenderPass(
			L"Debug" + prefix + L"Pass",
			{ m_FinalColorName }, depthName,
			ERenderPassLoadOperation::RPLO_Load,
			ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Load,
			ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f), 500, ERenderSortType::None);
#endif
		
		m_DisplayRenderTargetName = m_FinalColorName; // 초기: GBuffer_Diffuse
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

	tagRHIPipelineDesc channelPipelineDesc = {};
	channelPipelineDesc.PipelineType = EPipelineType::Graphics;
	channelPipelineDesc.VertexShader = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS")->GetRHIShader();
	channelPipelineDesc.PixelShader = rm.GetResourceHandle<Shader>(L"ViewportChannelPS")->GetRHIShader();
	channelPipelineDesc.ColorAttachmentCount = 1;
	channelPipelineDesc.ColorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
	channelPipelineDesc.DepthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
	channelPipelineDesc.DepthStencilState.DepthTestEnable = false;
	channelPipelineDesc.DepthStencilState.DepthWriteEnable = false;
	channelPipelineDesc.Topology = ETopology::TriangleList;
	channelPipelineDesc.CullMode = ECullMode::None;
	channelPipelineDesc.BlendMode = EBlendMode::Opaque;
	m_ChannelPreviewPipeline = PipelineManager::Get().GetOrCreatePipeline(channelPipelineDesc);
}
void ViewportPanel::Free()
{
	m_LockedSceneCamera = nullptr;
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

	RenderPassManager& rpMgr = RenderPassManager::Get();

	// 씬 카메라 잠금 상태면 해당 카메라로 렌더, 아니면 에디터 카메라
	Camera* activeCamera = (m_LockedSceneCamera && m_LockedSceneCamera->IsActive())
		? m_LockedSceneCamera
		: m_EditorCamera->GetCamera();

	Renderer::Get().RegisterViewportCamera(nullptr, rpMgr.GetRenderPassByID(m_ShadowPassID));
	Renderer::Get().RegisterViewportCamera(activeCamera, rpMgr.GetRenderPassByID(m_GeometryPassID));
	Renderer::Get().RegisterViewportCamera(activeCamera, rpMgr.GetRenderPassByID(m_DebugPassID));

	m_Grid.SubmitGrid(m_GeometryPassID, m_IsOrthographic);
	SubmitLightingPass();
	SubmitChannelPreviewPass();
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

		const bool useChannelPreview = (m_ChannelView != EViewportChannelView::RGBA);
		const wstring& displayName = useChannelPreview ? m_ChannelPreviewName : m_DisplayRenderTargetName;
		RenderTarget* currentRT = RenderTargetManager::Get().GetRenderTarget(displayName);
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
				const bool useSceneCamera = (m_LockedSceneCamera && m_LockedSceneCamera->IsActive());
				if (!useSceneCamera && (isFocused || isHovered))
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
				DrawLightOverlay(imageScreenPos, finalSize);
				DrawCameraOverlay(imageScreenPos, finalSize);

				{
					if (m_LockedSceneCamera && m_LockedSceneCamera->IsActive())
					{
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						if (drawList)
						{
							const string camName = [&]() -> string {
								GameObject* owner = m_LockedSceneCamera->GetOwner();
								return owner ? WStrToStr(owner->GetName()) : "Scene Camera";
								}();

							const string label = ICON_FA_CAMERA " " + camName;
							const ImVec2 labelPos = ImVec2(imageScreenPos.x + 8.0f, imageScreenPos.y + 8.0f);

							drawList->AddRectFilled(
								ImVec2(labelPos.x - 4.0f, labelPos.y - 2.0f),
								ImVec2(labelPos.x + ImGui::CalcTextSize(label.c_str()).x + 4.0f,
									labelPos.y + ImGui::GetTextLineHeight() + 2.0f),
								IM_COL32(0, 0, 0, 160), 3.0f);

							drawList->AddText(labelPos, IM_COL32(90, 210, 255, 255), label.c_str());
						}
					}
				}
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

#pragma region Overlay
void ViewportPanel::DrawLightOverlay(const ImVec2& imageScreenPos, const ImVec2& imageSize)
{
	if (!m_ShowLightOverlay || !m_EditorCamera || !m_EditorCamera->GetCamera())
		return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList)
		return;

	const bool useSceneCamera = (m_LockedSceneCamera && m_LockedSceneCamera->IsActive());
	Camera* camera = useSceneCamera ? m_LockedSceneCamera : m_EditorCamera->GetCamera();

	const mat4 viewProj = camera->GetProjMatrix() * camera->GetViewMatrix();
	const vec3 cameraPos = camera->GetCameraBuffer().cameraPosition;

	const auto& lights = LightManager::Get().GetLightSources();
	for (LightSource* light : lights)
	{
		if (!light || !light->IsActive())
			continue;

		GameObject* owner = light->GetOwner();
		if (!owner || !owner->IsActive())
			continue;

		Transform* tr = owner->GetTransform();
		if (!tr)
			continue;

		vec3 worldPos = tr->GetWorldPosition();

		vec3 forward = tr->GetForward();
		if (glm::dot(forward, forward) < 0.000001f)
			forward = vec3(0.0f, 0.0f, 1.0f);
		else
			forward = glm::normalize(forward);

		ImVec2 screenPos;
		if (!WorldToViewportScreen(worldPos, viewProj, imageScreenPos, imageSize, screenPos))
			continue;

		ImU32 color = IM_COL32(255, 220, 80, 255);
		const char* lightIcon = ICON_FA_LIGHTBULB;
		bool drawDir = false;
		float dirLen = 1.5f;

		switch (light->GetType())
		{
		case ELightType::Directional:
			color = IM_COL32(255, 180, 0, 255);
			lightIcon = ICON_FA_SUN;
			drawDir = true;
			dirLen = 3.0f;
			break;
		case ELightType::Spot:
			color = IM_COL32(80, 220, 255, 255);
			lightIcon = ICON_FA_LOCATION_DOT;
			drawDir = true;
			dirLen = 1.8f;
			break;
		case ELightType::Point:
		default:
			color = IM_COL32(255, 220, 80, 255);
			lightIcon = ICON_FA_LIGHTBULB;
			drawDir = false;
			dirLen = 1.5f;
			break;
		}

		// distance-based scale
		const float dist = glm::length(worldPos - cameraPos);

		// 원하는 감도에 맞게 조절
		const float nearDist = 2.0f;
		const float farDist = 40.0f;
		const float nearScale = 1.9f; // 가까울 때 크게
		const float farScale = 0.75f; // 멀 때 작게

		float t = (dist - nearDist) / (farDist - nearDist);
		t = glm::clamp(t, 0.0f, 1.0f);
		const float distanceScale = glm::mix(nearScale, farScale, t);

		// 배경 링 + 아이콘
		ImFont* font = ImGui::GetFont();
		const float iconFontSize = ImGui::GetFontSize() * 1.8f * distanceScale;
		const ImVec2 iconSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, lightIcon);

		const float fillRadius = (glm::max(iconSize.x, iconSize.y) * 0.5f + 4.0f);
		const float strokeRadius = fillRadius + 2.0f;
		const float ringThickness = glm::max(1.5f, 2.0f * distanceScale);

		drawList->AddCircleFilled(screenPos, fillRadius, IM_COL32(0, 0, 0, 140));
		drawList->AddCircle(screenPos, strokeRadius, color, 0, ringThickness);
		drawList->AddText(
			font,
			iconFontSize,
			ImVec2(screenPos.x - iconSize.x * 0.5f, screenPos.y - iconSize.y * 0.5f),
			color,
			lightIcon);

		if (!drawDir)
			continue;

		// 방향 길이도 거리 기반으로 살짝 반영
		const float dirLenScaled = dirLen * glm::mix(0.9f, 1.35f, distanceScale);
		ImVec2 tipScreen;
		vec3 tipWorld = worldPos + forward * dirLenScaled;
		if (!WorldToViewportScreen(tipWorld, viewProj, imageScreenPos, imageSize, tipScreen))
			continue;

		const float lineThickness = glm::max(1.5f, 2.0f * distanceScale);
		drawList->AddLine(screenPos, tipScreen, color, lineThickness);

		ImVec2 d(tipScreen.x - screenPos.x, tipScreen.y - screenPos.y);
		float len = sqrtf(d.x * d.x + d.y * d.y);
		if (len > 0.0001f)
		{
			d.x /= len;
			d.y /= len;
			ImVec2 n(-d.y, d.x);

			const float headLen = 10.0f * distanceScale;
			const float headW = 4.0f * distanceScale;

			ImVec2 a = tipScreen;
			ImVec2 b(tipScreen.x - d.x * headLen + n.x * headW,
				tipScreen.y - d.y * headLen + n.y * headW);
			ImVec2 c(tipScreen.x - d.x * headLen - n.x * headW,
				tipScreen.y - d.y * headLen - n.y * headW);

			drawList->AddTriangleFilled(a, b, c, color);
		}

		// 방향 아이콘도 같이 찍고 싶으면 사용
		//drawList->AddText(ImVec2(tipScreen.x + 3.0f, tipScreen.y - 8.0f), color, ICON_FA_ARROW_RIGHT_LONG);
	}
}
void ViewportPanel::DrawCameraOverlay(const ImVec2& imageScreenPos, const ImVec2& imageSize)
{
	if (!m_ShowCameraOverlay || !m_EditorCamera || !m_EditorCamera->GetCamera())
		return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList)
		return;

	const bool useSceneCamera = (m_LockedSceneCamera && m_LockedSceneCamera->IsActive());
	Camera* activeCamera = useSceneCamera ? m_LockedSceneCamera : m_EditorCamera->GetCamera();

	const mat4 editorViewProj = activeCamera->GetProjMatrix() * activeCamera->GetViewMatrix();
	const vec3 editorCameraPos = activeCamera->GetCameraBuffer().cameraPosition;

	const ImU32 cameraColor = IM_COL32(90, 210, 255, 255);
	const ImU32 frustumColor = IM_COL32(90, 210, 255, 200);
	const char* cameraIcon = ICON_FA_CAMERA;

	const auto& cameras = CameraManager::Get().GetCameras();
	for (Camera* targetCamera : cameras)
	{
		if (!targetCamera || targetCamera == activeCamera || !targetCamera->IsActive())
			continue;

		if (targetCamera == m_LockedSceneCamera)
			continue;

		GameObject* owner = targetCamera->GetOwner();
		if (!owner || !owner->IsActive())
			continue;

		Transform* transform = owner->GetTransform();
		if (!transform)
			continue;

		const vec3 worldPos = transform->GetWorldPosition();
		ImVec2 screenPos;
		if (!WorldToViewportScreen(worldPos, editorViewProj, imageScreenPos, imageSize, screenPos))
			continue;

		// ── 거리 기반 스케일 (아이콘 크기) ──────────────────────
		const float dist = glm::length(worldPos - editorCameraPos);
		float t = glm::clamp((dist - 2.0f) / (40.0f - 2.0f), 0.0f, 1.0f);
		const float distanceScale = glm::mix(1.9f, 0.75f, t);
		const float lineThickness = glm::max(1.5f, 1.8f * distanceScale);

		// ── 카메라 아이콘 ────────────────────────────────────────
		ImFont* font = ImGui::GetFont();
		const float iconFontSize = ImGui::GetFontSize() * 1.7f * distanceScale;
		const ImVec2 iconSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, cameraIcon);
		const float fillRadius = glm::max(iconSize.x, iconSize.y) * 0.5f + 4.0f;
		const float strokeRadius = fillRadius + 2.0f;

		drawList->AddCircleFilled(screenPos, fillRadius, IM_COL32(0, 0, 0, 140));
		drawList->AddCircle(screenPos, strokeRadius, cameraColor, 0, lineThickness);
		drawList->AddText(
			font, iconFontSize,
			ImVec2(screenPos.x - iconSize.x * 0.5f, screenPos.y - iconSize.y * 0.5f),
			cameraColor, cameraIcon);

		// ── Frustum 직접 계산 ─────────────────────────────────────
		// 에디터 카메라와의 거리에 비례해 displayFar를 결정.
		// 너무 멀면 작아지지 않도록 min(실제Far, 에디터거리 * 0.5)로 클램프.
		const float actualFar = targetCamera->GetCameraBuffer().projMatrix[3][2]; // 사용 안 함
		const float displayNear = 0.5f;
		const float displayFar = glm::clamp(dist * 0.5f, 2.0f, 20.0f);

		const vec3 forward = glm::normalize(-transform->GetForward());
		const vec3 right = glm::normalize(transform->GetRight());
		const vec3 up = glm::normalize(transform->GetUp());

		const bool isPerspective = targetCamera->GetIsPerspective();

		// near/far 평면의 하프 크기 계산
		float nearHalfH, nearHalfW, farHalfH, farHalfW;
		if (isPerspective)
		{
			const float fovRad = glm::radians(targetCamera->GetFOV());
			const float aspect = 16.0f / 9.0f; // 고정 aspect — Camera에 GetAspect()가 있으면 교체
			nearHalfH = glm::tan(fovRad * 0.5f) * displayNear;
			nearHalfW = nearHalfH * aspect;
			farHalfH = glm::tan(fovRad * 0.5f) * displayFar;
			farHalfW = farHalfH * aspect;
		}
		else
		{
			// Ortho: OrthoSize가 절반 높이
			const float orthoH = targetCamera->GetOrthoSize();
			const float orthoW = orthoH * (16.0f / 9.0f);
			nearHalfH = farHalfH = orthoH;
			nearHalfW = farHalfW = orthoW;
		}

		const vec3 nearCenter = worldPos + forward * displayNear;
		const vec3 farCenter = worldPos + forward * displayFar;

		// 8 코너
		const vec3 corners[8] =
		{
			nearCenter - right * nearHalfW - up * nearHalfH, // near BL
			nearCenter + right * nearHalfW - up * nearHalfH, // near BR
			nearCenter + right * nearHalfW + up * nearHalfH, // near TR
			nearCenter - right * nearHalfW + up * nearHalfH, // near TL
			farCenter - right * farHalfW - up * farHalfH,  // far  BL
			farCenter + right * farHalfW - up * farHalfH,  // far  BR
			farCenter + right * farHalfW + up * farHalfH,  // far  TR
			farCenter - right * farHalfW + up * farHalfH,  // far  TL
		};

		const int edges[12][2] =
		{
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 }, // near rect
			{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 }, // far  rect
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }, // connecting
		};

		for (int i = 0; i < 12; ++i)
		{
			DrawViewportLineIfVisible(
				drawList,
				corners[edges[i][0]],
				corners[edges[i][1]],
				editorViewProj,
				imageScreenPos,
				imageSize,
				frustumColor,
				lineThickness);
		}

		// 카메라 아이콘 → far 중심 방향선
		ImVec2 farCenterScreen;
		if (WorldToViewportScreen(farCenter, editorViewProj, imageScreenPos, imageSize, farCenterScreen))
			drawList->AddLine(screenPos, farCenterScreen, cameraColor, lineThickness);
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

		if (ImGui::BeginMenu("Overlay"))
		{
			ImGui::MenuItem("Show Grid", nullptr, m_Grid.GetVisible());
			ImGui::MenuItem("Show Light Overlay", nullptr, &m_ShowLightOverlay);
			ImGui::MenuItem("Show Camera Overlay", nullptr, &m_ShowCameraOverlay);
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("TempMenu"))
		{
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("SceneCamera"))
		{
			// [에디터 카메라로 복귀]
			bool editorSelected = (m_LockedSceneCamera == nullptr);
			if (ImGui::MenuItem("Editor Camera", nullptr, editorSelected))
				m_LockedSceneCamera = nullptr;

			ImGui::Separator();

			// [씬 내 카메라 목록]
			Camera* editorCam = m_EditorCamera->GetCamera();
			const auto& cameras = CameraManager::Get().GetCameras();

			if (cameras.empty() || (cameras.size() == 1 && cameras[0] == editorCam))
			{
				ImGui::TextDisabled("(No scene cameras)");
			}
			else
			{
				for (Camera* cam : cameras)
				{
					if (!cam || cam == editorCam)
						continue;

					GameObject* owner = cam->GetOwner();
					if (!owner)
						continue;

					string label = WStrToStr(owner->GetName());
					if (!cam->IsActive() || !owner->IsActive())
						label += " (inactive)";

					bool isSelected = (m_LockedSceneCamera == cam);
					if (ImGui::MenuItem(label.c_str(), nullptr, isSelected))
						m_LockedSceneCamera = cam;
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::Button("Camera"))
		{
			m_InspectorPanel->ToggleOpen();
		}

		DrawChannelViewButton();
		DrawDimensionToggleButton();
	}
	ImGui::EndMenuBar();
}
void ViewportPanel::DrawChannelViewButton()
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
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(m_ShadowDepthName)->GetTexture(),
				sampler, 5);
			// Lighting 파이프라인 바인딩
			{
				
				tagCameraBuffer shadowCam = LightManager::Get().GetShadowCameraBuffer(0u);
				tagLightShadowData shadowCamData = LightManager::Get().GetShadowData(0u);
				rhi->BindConstantBuffer(&shadowCamData, sizeof(shadowCamData), 1);
			}
			rhi->BindPipeline(m_LightingPipeline);

			if (IsFailure(LightManager::Get().Bind(0)))
			{
				fmt::print(stderr, "LightManager::Bind failed\n");
				return EResult::Fail;
			}

			if (IsFailure(rhi->Draw(3)))
			{
				fmt::print(stderr, "Lighting draw failed\n");
				return EResult::Fail;
			}
			return EResult::Success;
		},
		m_LightingPassID);
}

namespace
{
	static bool IsFullRGBA(EViewportChannelView flags)
	{
		return flags == EViewportChannelView::RGBA;
	}
}
void ViewportPanel::SubmitChannelPreviewPass()
{
	if (IsFullRGBA(m_ChannelView))
		return;

	Renderer::Get().SubmitCustomCommand(
		[this](f32 dt, RenderPass* pass) -> EResult
		{
			if (!m_ChannelPreviewPipeline)
				return EResult::Fail;

			RenderTarget* sourceRT = RenderTargetManager::Get().GetRenderTarget(m_DisplayRenderTargetName);
			if (!sourceRT || !sourceRT->GetTexture())
				return EResult::Fail;

			auto* rhi = Renderer::Get().GetRHI();
			auto& smMgr = SamplerManager::Get();
			RHISampler* sampler = smMgr.GetDefaultSampler();

			ChannelViewData channelData = {};
			channelData.Flags = static_cast<uint32>(m_ChannelView);

			if (IsFailure(rhi->BindConstantBuffer(&channelData, sizeof(ChannelViewData), 0)))
				return EResult::Fail;

			if (IsFailure(rhi->BindTextureSampler(sourceRT->GetTexture(), sampler, 0)))
				return EResult::Fail;

			if (IsFailure(rhi->BindPipeline(m_ChannelPreviewPipeline)))
				return EResult::Fail;

			return rhi->Draw(3);
		},
		m_ChannelPreviewPassID);
}
#pragma endregion
