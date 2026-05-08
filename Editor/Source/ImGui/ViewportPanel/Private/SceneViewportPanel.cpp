#pragma once

#include "SceneViewportPanel.h"
#include "RenderTargetManager.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "ImViewGuizmo.h"

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
void SceneViewportPanel::Initialize(void* arg)
{
	__super::Initialize(arg);
	CAST_DESC

#pragma region Prepare RenderPass And Pipelines
	PreparePipelines();
	PrepareRenderTargetsAndPasses(desc->RenderTargetWidth, desc->RenderTargetHeight);
#pragma endregion
	m_Grid.Initialize(m_Name);
#ifdef _DEBUG
	m_DebugRenderer.Initialize(m_Name);
#endif // _DEBUG

	m_PostProcessChain.Initialize(m_Name);

	m_SelectedRTName = m_LightingColorName;
}
void SceneViewportPanel::PrepareRenderTargetsAndPasses(uint32 width, uint32 height)
{
	wstring prefix = m_Name + L"_";

#pragma region RenderTargets
	auto& rtMgr = RenderTargetManager::Get();

	tagRenderTargetDesc gbufferRTDescs[] =
	{
		//GBuffers
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_Diffuse"},
		{ETextureFormat::R16G16B16A16_FLOAT,	ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_Normal"},
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_PBR"},
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_Emission"},
		{ETextureFormat::R32G32B32A32_FLOAT,	ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_Position"},
		//ShadowDepth
		{ETextureFormat::D32_FLOAT,				ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler, ERenderTargetBindFlag::RTBF_DepthStencil | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::DepthStencil, ETextureDimension::Texture2D, width, height, vec4(0.f), L"ShadowDepth"},
		//Depth
		{ETextureFormat::D24_UNORM_S8_UINT,		ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler, ERenderTargetBindFlag::RTBF_DepthStencil | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::DepthStencil, ETextureDimension::Texture2D, width, height, vec4(0.f), L"Depth"},
		//LightingColor
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::Color,		ETextureDimension::Texture2D, width, height, vec4(0.f, 0.f, 0.f, 1.f), L"LightingColor"},
		//Ping-pong RTs
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::Color,		ETextureDimension::Texture2D, width, height, vec4(0.f, 0.f, 0.f, 1.f), L"PP_A"},
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::Color,		ETextureDimension::Texture2D, width, height, vec4(0.f, 0.f, 0.f, 1.f), L"PP_B"},
	};

	for (auto& rtDesc : gbufferRTDescs)
	{
		rtDesc.Name = prefix + rtDesc.Name;
		rtMgr.CreateRenderTarget(&rtDesc);
		m_OwnedRTNames.push_back(rtDesc.Name);
	}
#pragma endregion

#pragma region RenderPasses
	vector<wstring> gBufferNames = { m_OwnedRTNames.begin(), m_OwnedRTNames.begin() + 5 };
	m_ShadowDepthName = m_OwnedRTNames[5];
	wstring depthName = m_OwnedRTNames[6];
	m_LightingColorName = m_OwnedRTNames[7];
	m_PPRTNames[0] = m_OwnedRTNames[8];
	m_PPRTNames[1] = m_OwnedRTNames[9];

	auto& rpMgr = RenderPassManager::Get();
	m_ShadowPassID = rpMgr.RegisterRenderPass(
		prefix + L"ShadowPass", {}, m_ShadowDepthName,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, ERenderSortType::FrontToBack, ERenderPassType::Shadow, EBlendMode::Opaque | EBlendMode::Masked);

	m_GeometryPassID = rpMgr.RegisterRenderPass(
		prefix + L"GeometryPass", gBufferNames, depthName,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.f, 0.f, -1.f), 0, ERenderSortType::FrontToBack, ERenderPassType::Geometry, EBlendMode::Opaque | EBlendMode::Masked);

	m_LightingPassID = rpMgr.RegisterRenderPass(
		prefix + L"LightingPass", { m_LightingColorName }, L"",
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 100, ERenderSortType::None, ERenderPassType::Lighting, EBlendMode::None);

	m_ForwardPassID = rpMgr.RegisterRenderPass(
		prefix + L"ForwardPass",
		{ m_LightingColorName }, depthName,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 110, ERenderSortType::BackToFront, ERenderPassType::Forward,
		EBlendMode::Forward);

	m_ForwardTransparentPassID = rpMgr.RegisterRenderPass(
		prefix + L"ForwardTransparentPass",
		{ m_LightingColorName }, depthName,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 120, ERenderSortType::BackToFront, ERenderPassType::ForwardTransparent,
		EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::NonPremultiplied | EBlendMode::Forward | EBlendMode::Decal);

	

	// ── PostProcess ping-pong passes (priorities 120~135) ──
	for (uint32 pi = 0; pi < MAX_PP_PASSES; ++pi)
	{
		RenderPassID rpID = rpMgr.RegisterRenderPass(
			prefix + L"PP_" + std::to_wstring(pi), { m_PPRTNames[pi % 2] }, L"",
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.f, 0.f, -1.f), 130 + pi, ERenderSortType::None, ERenderPassType::PostProcess, EBlendMode::None);
		m_PPPassIDs.push_back(rpID);
	}

	m_UIOverlayPassID = rpMgr.RegisterRenderPass(
		prefix + L"UIOverlayPass", {}, L"",
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.f, 0.f, 0.f, -1.f), 200, ERenderSortType::FrontToBack, ERenderPassType::UI, EBlendMode::None);
#pragma endregion

	m_ChannelPreviewPassID = rpMgr.RegisterRenderPass(
		prefix + L"ChannelPreviewPass", { m_PPRTNames[0] }, L"",
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 400, ERenderSortType::None, ERenderPassType::Custom, EBlendMode::None);

	m_PassOptions = {
		{ L"Shadow",            true  },
		{ L"Lighting",          true  },
		{ L"ForwardTransparent",true  },
		{ L"UIOverlay",         true  },
	};
}
void SceneViewportPanel::PreparePipelines()
{
	ResourceManager& rm = ResourceManager::Get();
	auto createPipe = [&](const wstring& psName) -> Engine::RHIPipeline* {
		tagRHIPipelineDesc pd = {};
		pd.PipelineType = EPipelineType::Graphics;
		pd.VertexShader = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS")->GetRHIShader();
		pd.PixelShader = rm.GetResourceHandle<Shader>(psName)->GetRHIShader();
		pd.ColorAttachmentCount = 1;
		pd.ColorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
		pd.DepthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
		pd.DepthStencilState.DepthTestEnable = false;
		pd.DepthStencilState.DepthWriteEnable = false;
		pd.Topology = ETopology::TriangleList;
		pd.CullMode = ECullMode::None;
		pd.BlendState = tagBlendState{};
		return PipelineManager::Get().GetOrCreatePipeline(pd);
		};

	m_LightingPipeline = createPipe(L"LightingPS");
}
void SceneViewportPanel::Free()
{
	__super::Free();
	m_Grid.Free();
	m_DebugRenderer.Free();
	m_PostProcessChain.Free();
}
#pragma endregion

void SceneViewportPanel::Update(f32 dt)
{
	CalculateRenderResolution(m_PanelWidth, m_PanelHeight);

	if (m_OwnedCamera && m_Focused && m_Hovered)
	{
		m_OwnedCamera->HandleInput(dt);
	}
		

	if (m_OwnedCamera)
	{
		m_OwnedCamera->FixedUpdate(dt);
		m_OwnedCamera->Update(dt);
		m_OwnedCamera->LateUpdate(dt);
	}

	RenderPassManager& rpMgr = RenderPassManager::Get();
	Renderer& renderer = Renderer::Get();
	renderer.RegisterViewportCamera(nullptr, rpMgr.GetRenderPassByID(m_ShadowPassID));
	renderer.RegisterViewportCamera(m_Camera, rpMgr.GetRenderPassByID(m_GeometryPassID));
	renderer.RegisterViewportCamera(m_Camera, rpMgr.GetRenderPassByID(m_ForwardPassID));
	renderer.RegisterViewportCamera(m_Camera, rpMgr.GetRenderPassByID(m_ForwardTransparentPassID));
	renderer.RegisterViewportCamera(nullptr, rpMgr.GetRenderPassByID(m_UIOverlayPassID));

	SubmitLightingPass(m_Camera);

	uint32 ppSlot = 0;
	wstring currentRT = m_LightingColorName;
	// 체인 순차 실행 (currentRT와 ppSlot이 함수들을 거치며 갱신됨)
	SubmitPostProcessPass(dt, ppSlot, currentRT);
	SubmitUIOverlayPass(currentRT);

	if (m_ShowFinalComposed)
		m_DisplayRTName = currentRT;
	else
		m_DisplayRTName = m_SelectedRTName;

	if (m_ChannelView != EViewportChannelView::RGBA)
	{
		m_ChannelFilter.SetChannelView(m_ChannelView);
		m_ChannelFilter.SubmitChannelPreviewPass(m_DisplayRTName, m_DisplayRTName);
	}

	if (*m_Grid.GetVisible())
	{
		m_Grid.SubmitGrid(m_Camera, IsOrthographic(), m_DisplayRTName, m_OwnedRTNames[6]);
	}

#ifdef _DEBUG
	if (*m_DebugRenderer.GetDrawCollidersPtr())
	{
		m_DebugRenderer.SubmitDebugDraw(m_Camera, m_DisplayRTName);
	}
#endif

	KeyboardInput();
	MouseInput();
}

void SceneViewportPanel::Draw()
{
	__super::Draw();
}

#pragma region Custom Draws

void SceneViewportPanel::DrawCustomViewport()
{
	BaseViewportPanel::DrawCustomViewport();
	DrawImGuizmo();
	DrawImViewGuizmo();
	DrawLightOverlay();
	DrawCameraOverlay();
}

void SceneViewportPanel::DrawImGuizmo()
{
	GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection();
	if (!selectedObject) return;

	Transform* transform = selectedObject->GetComponent<Transform>();
	if (!transform) return;
	Camera* camera = m_Camera;

	// 1. ImGuizmo 초기 설정
	ImGuizmo::SetOrthographic(IsOrthographic());
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(m_ImageScreenPos.x, m_ImageScreenPos.y, m_ImageSize.x, m_ImageSize.y);
	ImGuizmo::SetID(ImGui::GetID(this));

	// 2. 카메라 및 매트릭스 준비
	
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

void SceneViewportPanel::DrawImViewGuizmo()
{
	if (!m_OwnedCamera || !m_Camera)
		return;

	if (m_ImageSize.x <= 0.0f || m_ImageSize.y <= 0.0f)
		return;

	Transform* cameraTransform = m_OwnedCamera->GetTransform();
	if (!cameraTransform)
		return;

	ImViewGuizmo::BeginFrame();

	auto& style = ImViewGuizmo::GetStyle();
	style.scale = glm::clamp(glm::min(m_ImageSize.x, m_ImageSize.y) / 320.0f, 0.35f, 0.75f);

	const float gizmoDiameter = 256.0f * style.scale;
	const float buttonDiameter = style.toolButtonRadius * 2.0f * style.scale;
	const float padding = 6.0f;
	const float buttonSpacing = 4.0f;

	const ImVec2 gizmoPos(
		m_ImageScreenPos.x + m_ImageSize.x - gizmoDiameter * 0.5f,
		m_ImageScreenPos.y + gizmoDiameter * 0.5f);

	const ImVec2 dollyPos(
		gizmoPos.x - gizmoDiameter * 0.5f - buttonDiameter * 0.5f - buttonSpacing,
		m_ImageScreenPos.y + buttonDiameter * 0.5f - padding);

	const ImVec2 panPos(
		dollyPos.x - buttonDiameter - buttonSpacing,
		dollyPos.y);

	vec3 cameraPos = cameraTransform->GetWorldPosition();
	quat cameraRot = cameraTransform->GetWorldRotationQuat();

	vec3 pivot = cameraPos + (cameraRot * vec3(0.0f, 0.0f, 1.0f)) * 5.0f;

	if (GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection())
	{
		if (Transform* selectedTransform = selectedObject->GetTransform())
		{
			pivot = selectedTransform->GetWorldPosition();
		}
	}

	bool cameraChanged = false;

	cameraChanged |= ImViewGuizmo::Rotate(
		cameraPos,
		cameraRot,
		pivot,
		gizmoPos,
		0.0125f);

	cameraChanged |= ImViewGuizmo::Dolly(
		cameraPos,
		cameraRot,
		dollyPos,
		0.05f);

	cameraChanged |= ImViewGuizmo::Pan(
		cameraPos,
		cameraRot,
		panPos,
		0.01f);

	if (cameraChanged)
	{
		cameraTransform->SetPosition(cameraPos);
		cameraTransform->SetRotation(cameraRot);
	}
}

void SceneViewportPanel::DrawLightOverlay()
{
	if (!m_ShowLightOverlay || !m_OwnedCamera || !m_OwnedCamera->GetCamera())
		return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList)
		return;

	Camera* camera = m_Camera;
	if (!camera)
		return;

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
		if (!WorldToViewportScreen(worldPos, viewProj, m_ImageScreenPos, m_ImageSize, screenPos))
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
		case ELightType::Sky:
			color = IM_COL32(255, 180, 0, 255);
			lightIcon = ICON_FA_CLOUD_SUN;
			drawDir = true;
			dirLen = 3.0f;
			forward *= -1;
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
		if (!WorldToViewportScreen(tipWorld, viewProj, m_ImageScreenPos, m_ImageSize, tipScreen))
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
	}
}

void SceneViewportPanel::DrawCameraOverlay()
{
	if (!m_ShowCameraOverlay || !m_OwnedCamera || !m_OwnedCamera->GetCamera())
		return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList)
		return;

	if (!m_Camera)
		return;

	const mat4 editorViewProj = m_Camera->GetProjMatrix() * m_Camera->GetViewMatrix();
	const vec3 editorCameraPos = m_Camera->GetCameraBuffer().cameraPosition;

	const ImU32 cameraColor = IM_COL32(90, 210, 255, 255);
	const ImU32 frustumColor = IM_COL32(90, 210, 255, 200);
	const char* cameraIcon = ICON_FA_CAMERA;

	const auto& cameras = CameraManager::Get().GetCameras();
	for (Camera* targetCamera : cameras)
	{
		if (!targetCamera || targetCamera == m_Camera || !targetCamera->IsActive())
			continue;

		if (targetCamera == m_Camera)
			continue;

		GameObject* owner = targetCamera->GetOwner();
		if (!owner || !owner->IsActive())
			continue;

		Transform* transform = owner->GetTransform();
		if (!transform)
			continue;

		const vec3 worldPos = transform->GetWorldPosition();
		ImVec2 screenPos;
		if (!WorldToViewportScreen(worldPos, editorViewProj, m_ImageScreenPos, m_ImageSize, screenPos))
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
				m_ImageScreenPos,
				m_ImageSize,
				frustumColor,
				lineThickness);
		}

		// 카메라 아이콘 → far 중심 방향선
		ImVec2 farCenterScreen;
		if (WorldToViewportScreen(farCenter, editorViewProj, m_ImageScreenPos, m_ImageSize, farCenterScreen))
			drawList->AddLine(screenPos, farCenterScreen, cameraColor, lineThickness);
	}
}

void SceneViewportPanel::DrawCustomOptions()
{
	DrawResolutionMenu();
	DrawSceneRenderTargetMenu();
	DrawGizmoMenu();
	DrawDebugMenu();
	DrawPostProcessMenu();
	DrawRenderPassMenu();
	if (m_InspectorPanel) DrawCameraMenu();
	DrawChannelViewButton();
	DrawDimensionToggleButton();

}

void SceneViewportPanel::DrawSceneRenderTargetMenu()
{
	if (ImGui::BeginMenu("RenderTarget"))
	{
		if (ImGui::MenuItem("FinalRT", nullptr, m_ShowFinalComposed))
		{
			m_ShowFinalComposed = true;
			m_Grid.Show();
		}


		ImGui::Separator();

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
			if (m_OwnedRTNames[i] == m_SelectedRTName)
				currentIdx = i;
		}
		// 현재 선택이 OwnedRT가 아니면 "Other" 표시
		const char* preview = currentIdx < shortLabels.size()
			? shortLabels[currentIdx].c_str() : "Other";
		if (ImGui::BeginCombo("##RTSelect", preview))
		{
			for (int i = 0; i < shortLabels.size(); ++i)
			{
				bool selected = !m_ShowFinalComposed && (m_OwnedRTNames[i] == m_SelectedRTName);
				if (ImGui::Selectable(shortLabels[i].c_str(), selected))
				{
					m_ShowFinalComposed = false;
					m_SelectedRTName = m_OwnedRTNames[i];
					m_Grid.Hide();
				}
				DrawRTItemContextMenu(m_OwnedRTNames[i]);
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
				bool selected = !m_ShowFinalComposed && (name == m_SelectedRTName);
				if (ImGui::MenuItem(label.c_str(), nullptr, selected))
				{
					m_ShowFinalComposed = false;
					m_SelectedRTName = name;
					m_Grid.Hide();
				}
				DrawRTItemContextMenu(name);
			}
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
}

void SceneViewportPanel::DrawGizmoMenu()
{
	if (ImGui::BeginMenu("Gizmo"))
	{
		if (ImGui::MenuItem("Translate (W)", "Shift + 1", m_GizmoOperation == ImGuizmo::TRANSLATE))
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::MenuItem("Rotate (E)", "Shift + 2", m_GizmoOperation == ImGuizmo::ROTATE))
			m_GizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::MenuItem("Scale (R)", "Shift + 3", m_GizmoOperation == ImGuizmo::SCALE))
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
}

void SceneViewportPanel::DrawDebugMenu()
{
	if (ImGui::BeginMenu("Debug"))
	{
		ImGui::MenuItem("Show Grid", nullptr, m_Grid.GetVisible());
		ImGui::MenuItem("Show Collider", nullptr, m_DebugRenderer.GetDrawCollidersPtr());
		ImGui::EndMenu();
	}
}

void SceneViewportPanel::DrawPostProcessMenu()
{
	m_PostProcessChain.DrawImGuiMenu();
}

void SceneViewportPanel::DrawRenderPassMenu()
{
	if (ImGui::BeginMenu("Passes"))
	{
		for (auto& pass : m_PassOptions)
		{
			string label = WStrToStr(pass.Name);
			ImGui::MenuItem(label.c_str(), nullptr, &pass.Enabled);
		}
		ImGui::EndMenu();
	}
}
void SceneViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
	auto& rtMgr = RenderTargetManager::Get();
	for (const auto& name : m_OwnedRTNames)
	{
		if (auto* rt = rtMgr.GetRenderTarget(name))
			rt->Resize(width, height);
	}

	if (m_Camera)
		m_Camera->SetAspect(static_cast<f32>(width) / static_cast<f32>(height));

	// Base: ChannelFilter RT 리사이즈
	BaseViewportPanel::ResizeRenderTargets(width, height);
}
#pragma endregion

#pragma region Rendering
void SceneViewportPanel::SubmitLightingPass(Camera* camera)
{
	Renderer::Get().SubmitCustomCommand(
		[this, camera](f32 dt, RenderPass* pass) -> EResult
		{
			auto* rhi = Renderer::Get().GetRHI();
			auto& rtMgr = RenderTargetManager::Get();
			auto& smMgr = SamplerManager::Get();
			RHISampler* sampler = smMgr.GetDefaultSampler();
			wstring prefix = m_Name + L"_";

			tagCameraBuffer camBuf = camera->GetCameraBuffer();
			camBuf.time = dt;
			rhi->BindConstantBuffer(&camBuf, sizeof(tagCameraBuffer), 0, EShaderType::Vertex);
			rhi->BindConstantBuffer(&camBuf, sizeof(tagCameraBuffer), 0, EShaderType::Pixel);
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
				rhi->BindConstantBuffer(&shadowCamData, sizeof(shadowCamData), 1, EShaderType::Pixel);
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

void SceneViewportPanel::SubmitPostProcessPass(f32 dt, uint32& slot, std::wstring& currentRT)
{
	m_PostProcessChain.ExecuteChain(dt, m_PPPassIDs, currentRT, m_PPRTNames);
	slot += static_cast<uint32>(m_PPPassIDs.size()); // Or just let slot be handled. Wait, slot is not needed outside?
}
void SceneViewportPanel::SubmitUIOverlayPass(const wstring& currentRT)
{
	RenderPassManager::Get().GetRenderPassByID(m_UIOverlayPassID)->SetColorAttachments({ currentRT });
}
#pragma endregion

#pragma region Inputs
void SceneViewportPanel::KeyboardInput()
{
	if (KEY_PRESSED(EKeyCode::LShift))
	{
		if (KEY_PRESSED("1")) m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
		if (KEY_PRESSED("2")) m_GizmoOperation = ImGuizmo::OPERATION::ROTATE;
		if (KEY_PRESSED("3")) m_GizmoOperation = ImGuizmo::OPERATION::SCALE;
	}
}
void SceneViewportPanel::MouseInput()
{
	ImVec2 mousePos = ImGui::GetMousePos();
	if (MOUSE_BUTTON_DOWN(Engine::EMouseButton::Left) && m_Focused && m_Hovered)
	{
		Ray mouseRay = ScreenPosToRay(mousePos);
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
Engine::Ray SceneViewportPanel::ScreenPosToRay(const ImVec2& mousePos)
{
	f32 localX = mousePos.x - m_ImageScreenPos.x;
	f32 localY = mousePos.y - m_ImageScreenPos.y;

	f32 ndcX = (localX / m_ImageSize.x) * 2.0f - 1.0f;
	f32 ndcY = 1.0f - (localY / m_ImageSize.y) * 2.0f;
	mat4 projInvMatrix = m_Camera->GetProjMatrixInv();
	mat4 viewInvMatrix = m_Camera->GetViewMatrixInv();

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