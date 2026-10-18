#pragma once

#include "SceneViewportPanel.h"
#include "EditorCameraController.h"
#include "Application.h"
#include "RenderTargetManager.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "ImViewGuizmo.h"
#include "SceneManager.h"

#include "Entity.h"
#include "Scene.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "TransformSystem.h"
#include "TransformPropertyEditor.h"
#include "PickingManager.h"

#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <string>

#pragma region Helper
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

	RenderTargetDesc gbufferRTDescs[] =
	{
		//GBuffers
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R8G8B8A8_UNORM, ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f)}, L"GBuffer_Diffuse"},
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R16G16B16A16_FLOAT, ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f)}, L"GBuffer_Normal"},
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R8G8B8A8_UNORM, ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f)}, L"GBuffer_PBR"},
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R8G8B8A8_UNORM, ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f)}, L"GBuffer_Emission"},
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R32G32B32A32_FLOAT, ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f)}, L"GBuffer_Position"},
		//ShadowDepth
		{{4096, 4096, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::D32_FLOAT, ETextureDimension::Texture2D, ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f)}, L"ShadowDepth"},
		//Depth
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::D24_UNORM_S8_UINT, ETextureDimension::Texture2D, ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f)}, L"Depth"},
		//LightingColor
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R8G8B8A8_UNORM, ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 1.f)}, L"LightingColor"},
		//Ping-pong RTs
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R8G8B8A8_UNORM, ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 1.f)}, L"PP_A"},
		{{width, height, 1, 1, 1, ETextureSampleCount::TextureSampleCount1, ETextureFormat::R8G8B8A8_UNORM, ETextureDimension::Texture2D, ETextureUsage::RenderTarget | ETextureUsage::Sampler, nullptr, 0, false, vec4(0.f, 0.f, 0.f, 1.f)}, L"PP_B"},
	};

	for (auto& rtDesc : gbufferRTDescs)
	{
		rtDesc.name = prefix + rtDesc.name;
		rtMgr.CreateRenderTarget(&rtDesc);
		m_OwnedRTNames.push_back(rtDesc.name);
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
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 110, ERenderSortType::FrontToBack, ERenderPassType::Forward,
		EBlendMode::Forward);

	m_ForwardTransparentPassID = rpMgr.RegisterRenderPass(
		prefix + L"ForwardTransparentPass",
		{ m_LightingColorName }, depthName,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 120, ERenderSortType::BackToFront, ERenderPassType::ForwardTransparent,
		EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::NonPremultiplied | EBlendMode::Decal);

	

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
		vec4(0.f, 0.f, 0.f, -1.f), 200, ERenderSortType::FrontToBack, ERenderPassType::UI,
		EBlendMode::Opaque | EBlendMode::Masked | EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::NonPremultiplied);
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
		RHIPipelineDesc pd = {};
		pd.pipelineType = EPipelineType::Graphics;
		pd.vertexShader = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS")->GetRHIShader();
		pd.pixelShader = rm.GetResourceHandle<Shader>(psName)->GetRHIShader();
		pd.colorAttachmentCount = 1;
		pd.colorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
		pd.depthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
		pd.depthStencilState.depthTestEnable = false;
		pd.depthStencilState.depthWriteEnable = false;
		pd.topology = ETopology::TriangleList;
		pd.cullMode = ECullMode::None;
		pd.blendState = BlendState{};
		return PipelineManager::Get().GetOrCreatePipeline(pd);
		};

	m_LightingPipeline = createPipe(L"LightingPS");
}
void SceneViewportPanel::Free()
{
	__super::Free();
	m_Grid.Free();

#ifdef _DEBUG
	m_DebugRenderer.Free();
#endif
	m_PostProcessChain.Free();
}
#pragma endregion

#pragma region Loop
void SceneViewportPanel::Update(f32 dt)
{
	// Download the clicked frame before resizing/replacing its render targets.
	ProcessPickingRequest();
	CalculateRenderResolution(m_PanelWidth, m_PanelHeight);

	if (m_CameraController && m_Focused && m_Hovered)
		m_CameraController->HandleInput(dt);

	if (m_CameraController)
	{
		if (m_RenderHeight > 0)
			m_CameraController->SetAspectRatio(static_cast<f32>(m_RenderWidth) / static_cast<f32>(m_RenderHeight));
		m_CameraController->RefreshMatrices();
	}

	const std::optional<CameraBuffer> cameraBuffer = m_CameraController ? m_CameraController->GetCameraBuffer() : std::nullopt;

	RenderPassManager& rpMgr = RenderPassManager::Get();
	Renderer& renderer = Renderer::Get();
	renderer.RegisterViewportCamera(rpMgr.GetRenderPassByID(m_ShadowPassID));
	if (cameraBuffer.has_value())
	{
		renderer.RegisterViewportCamera(*cameraBuffer, rpMgr.GetRenderPassByID(m_GeometryPassID));

		// ForwardPass에 최신 대기/태양광 데이터를 패스 상수로 주입
		if (auto* forwardPass = rpMgr.GetRenderPassByID(m_ForwardPassID))
		{
			SkyAtmosphereData skyData = BuildSkyAtmosphereData();
			forwardPass->SetPassConstantStruct(3, skyData); // slot 3 (b1, space3)에 설정!
		}
		renderer.RegisterViewportCamera(*cameraBuffer, rpMgr.GetRenderPassByID(m_ForwardPassID));
		renderer.RegisterViewportCamera(*cameraBuffer, rpMgr.GetRenderPassByID(m_ForwardTransparentPassID));
		SubmitLightingPass(*cameraBuffer);
	}
	renderer.RegisterViewportCamera(rpMgr.GetRenderPassByID(m_UIOverlayPassID));

	uint32 ppSlot = 0;
	wstring currentRT = m_LightingColorName;
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

	if (cameraBuffer.has_value() && *m_Grid.GetVisible())
		m_Grid.SubmitGrid(*cameraBuffer, IsOrthographic(), m_DisplayRTName, m_OwnedRTNames[6]);

#ifdef _DEBUG
	if (cameraBuffer.has_value() && *m_DebugRenderer.GetDrawCollidersPtr())
		m_DebugRenderer.SubmitDebugDraw(*cameraBuffer, m_DisplayRTName);
#endif

	KeyboardInput();
	MouseInput();
}

void SceneViewportPanel::Draw()
{
	__super::Draw();
}
#pragma endregion

#pragma region Custom Draws
void SceneViewportPanel::DrawCustomOptions()
{
	DrawResolutionMenu();
	DrawSceneRenderTargetMenu();
	DrawGizmoMenu();
	DrawDebugMenu();
	DrawPostProcessMenu();
	DrawRenderPassMenu();
	DrawPickingMenu();
	DrawChannelViewButton(30.0f * 2.0f + 8.0f);
	DrawDimensionToggleButton();
}

void SceneViewportPanel::DrawCustomViewport()
{
    Application::Get().GetEditHistory().AllowShortcuts(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows));
	BaseViewportPanel::DrawCustomViewport();
	const bool imageHovered = ImGui::IsItemHovered();
	DrawImGuizmo();
	DrawImViewGuizmo();
	DrawLightOverlay();
	DrawCameraOverlay();
	DrawSelectionLocked();
	CapturePickingRequest(imageHovered);
}

void SceneViewportPanel::DrawPickingMenu()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("Viewport.Picking").c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.GPUId").c_str(), nullptr, m_PickingMode == EScenePickingMode::GPUId))
			m_PickingMode = EScenePickingMode::GPUId;
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.RayBounds").c_str(), nullptr,
			m_PickingMode == EScenePickingMode::RayBounds))
			m_PickingMode = EScenePickingMode::RayBounds;
		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", LOCAL_CSTR("Viewport.RayBoundsHint"));
		ImGui::EndMenu();
	}
}
#pragma endregion

#pragma region Options Bar
void SceneViewportPanel::DrawSceneRenderTargetMenu()
{
	if (ImGui::BeginMenu(LOCAL_CSTR("UI_RENDER_TARGET")))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.FinalOutput").c_str(), nullptr, m_ShowFinalComposed))
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
		const string preview = currentIdx < shortLabels.size()
			? shortLabels[currentIdx] : LOCAL("EditorCommon.Other");
		if (ImGui::BeginCombo("##RTSelect", preview.c_str()))
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
		if (ImGui::BeginMenu(LOCAL_LABEL("Viewport.AllRenderTargets").c_str()))
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
	if (ImGui::BeginMenu(LOCAL_CSTR("UI_GIZMO")))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.Translate").c_str(), "Shift + 1", m_GizmoOperation == ImGuizmo::TRANSLATE))
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.Rotate").c_str(), "Shift + 2", m_GizmoOperation == ImGuizmo::ROTATE))
			m_GizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.ScaleOperation").c_str(), "Shift + 3", m_GizmoOperation == ImGuizmo::SCALE))
			m_GizmoOperation = ImGuizmo::SCALE;
		ImGui::Separator();
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.Local").c_str(), nullptr, m_GizmoMode == ImGuizmo::LOCAL))
			m_GizmoMode = ImGuizmo::LOCAL;
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.World").c_str(), nullptr, m_GizmoMode == ImGuizmo::WORLD))
			m_GizmoMode = ImGuizmo::WORLD;

		ImGui::Separator();
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.SnapToGrid").c_str(), nullptr, m_GizmoUseSnap))
			m_GizmoUseSnap = !m_GizmoUseSnap;
		if (ImGui::MenuItem(LOCAL_LABEL("Viewport.ShowBounds").c_str(), nullptr, m_GizmoShowBounds))
			m_GizmoShowBounds = !m_GizmoShowBounds;
		if (m_GizmoUseSnap)
		{
			ImGui::InputFloat3(LOCAL_LABEL("Viewport.SnapTranslation").c_str(), glm::value_ptr(m_GizmoSnapTranslation));
			ImGui::InputFloat3(LOCAL_LABEL("Viewport.SnapRotation").c_str(), glm::value_ptr(m_GizmoSnapRotation));
			ImGui::InputFloat3(LOCAL_LABEL("Viewport.SnapScale").c_str(), glm::value_ptr(m_GizmoSnapScale));
		}
		ImGui::EndMenu();
	}
}

void SceneViewportPanel::DrawDebugMenu()
{
	if (ImGui::BeginMenu(LOCAL_CSTR("UI_DEBUG")))
	{
		ImGui::MenuItem(LOCAL_LABEL("Viewport.ShowGrid").c_str(), nullptr, m_Grid.GetVisible());
#ifdef _DEBUG
		ImGui::MenuItem(LOCAL_LABEL("Viewport.ShowCollider").c_str(), nullptr, m_DebugRenderer.GetDrawCollidersPtr());
#endif
		ImGui::EndMenu();
	}
}

void SceneViewportPanel::DrawPostProcessMenu()
{
	m_PostProcessChain.DrawImGuiMenu();
}

void SceneViewportPanel::DrawRenderPassMenu()
{
	if (ImGui::BeginMenu(LOCAL_CSTR("UI_PASS")))
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
		if (name == m_ShadowDepthName) continue;
		if (auto* rt = rtMgr.GetRenderTarget(name))
			rt->Resize(width, height);
	}

	if (m_CameraController && height > 0)
	{
		m_CameraController->SetAspectRatio(static_cast<f32>(width) / static_cast<f32>(height));
		m_CameraController->RefreshMatrices();
	}

	BaseViewportPanel::ResizeRenderTargets(width, height);
}
#pragma endregion

#pragma region Overlay
void SceneViewportPanel::DrawImGuizmo()
{
	Engine::Entity* selectedEntity = SelectionManager::Get().GetPrimarySelectedEntity();

	if (selectedEntity == nullptr || m_CameraController == nullptr ||
		m_ImageSize.x <= 0.0f || m_ImageSize.y <= 0.0f)
	{
		return;
	}

	Engine::Scene* scene = selectedEntity->GetScene();

	if (scene == nullptr)
	{
		return;
	}

	auto& world = scene->GetRegistry();
	const entt::entity entity = selectedEntity->GetEntityHandle();

	if (!world.valid(entity) ||
		!world.all_of<Engine::TransformComponent, Engine::WorldTransformComponent>(entity))
	{
		return;
	}

	TransformEditMode editMode;

	switch (m_GizmoOperation)
	{
	case ImGuizmo::TRANSLATE: editMode = TransformEditMode::Translation; break;
	case ImGuizmo::ROTATE: editMode = TransformEditMode::Rotation; break;
	case ImGuizmo::SCALE: editMode = TransformEditMode::Scale; break;
	default: return;
	}

	// 일시정지 중에도 에디터에서 사용할 최신 계층 행렬을 계산합니다.
	Engine::TransformSystem::UpdateHierarchy(world);

	glm::mat4 worldMatrix = world.get<Engine::WorldTransformComponent>(entity).worldMatrix;
	const double determinant = glm::determinant(glm::dmat4(worldMatrix));

	if (!std::isfinite(determinant) || determinant == 0.0)
	{
		return;
	}

	optional<CameraBuffer> cameraBuffer = m_CameraController->GetCameraBuffer();

	if (!cameraBuffer)
	{
		return;
	}

	const glm::mat4 viewMatrix = cameraBuffer->viewMatrix;
	const glm::mat4 projectionMatrix = cameraBuffer->projMatrix;

	bool useSnap = m_GizmoUseSnap || ImGui::GetIO().KeyCtrl;
	glm::vec3 snapValues = m_GizmoSnapTranslation;

	if (editMode == TransformEditMode::Rotation)
	{
		snapValues = m_GizmoSnapRotation;
	}
	else if (editMode == TransformEditMode::Scale)
	{
		snapValues = m_GizmoSnapScale;
	}

	const auto* settings = TransformPropertyEditor::GetSettings(world, entity, editMode);

	if (settings != nullptr && settings->SnapEnabled)
	{
		useSnap = true;
		snapValues = glm::vec3(settings->SnapStep);
	}

	for (int axis = 0; axis < 3; ++axis)
	{
		if (!std::isfinite(snapValues[axis]) || snapValues[axis] <= 0.0f)
		{
			useSnap = false;
		}
	}

	float bounds[6]{ -0.5f, -0.5f, -0.5f, 0.5f, 0.5f, 0.5f };

	if (const auto* renderer = world.try_get<StaticMeshRendererComponent>(entity))
	{
		if (const auto* mesh = renderer->meshHandle.Get())
		{
			const glm::vec3 minimum = mesh->GetMin();
			const glm::vec3 maximum = mesh->GetMax();

			bounds[0] = minimum.x;
			bounds[1] = minimum.y;
			bounds[2] = minimum.z;
			bounds[3] = maximum.x;
			bounds[4] = maximum.y;
			bounds[5] = maximum.z;
		}
	}

	const bool showBounds = m_GizmoShowBounds && editMode == TransformEditMode::Scale;
	const std::string entityId = std::to_string(entt::to_integral(entity));

	ImGui::PushID(this);
	ImGui::PushID(static_cast<const void*>(scene));
	ImGui::PushID(entityId.c_str());

	ImGuizmo::SetID(static_cast<int>(ImGui::GetID("EntityGizmo")));
	ImGuizmo::SetOrthographic(IsOrthographic());
	ImGuizmo::SetDrawlist();
	ImGuizmo::SetRect(m_ImageScreenPos.x, m_ImageScreenPos.y, m_ImageSize.x, m_ImageSize.y);

	auto& history = Application::Get().GetEditHistory();
	const std::array targets{ entity };
	auto before = history.Enabled() ? SceneEditSnapshot::Capture(*scene, targets) : SceneEditSnapshot{};
	const bool operationLocked = TransformPropertyEditor::IsLocked(world, entity, editMode);
	const bool changed = !operationLocked && ImGuizmo::Manipulate(glm::value_ptr(viewMatrix), glm::value_ptr(projectionMatrix),
		m_GizmoOperation, m_GizmoMode, glm::value_ptr(worldMatrix), nullptr,
		useSnap ? glm::value_ptr(snapValues) : nullptr, showBounds ? bounds : nullptr,
		showBounds && useSnap ? glm::value_ptr(snapValues) : nullptr);

	if (changed && history.Enabled() && before)
	{
		if (TransformPropertyEditor::ApplyWorldMatrix(world, entity, worldMatrix, editMode))
			history.Record(std::move(before), SceneEditSnapshot::Capture(*scene, targets), "UI_HISTORY_TRANSFORM", true);
		else ImGui::SetTooltip("%s", LOCAL_CSTR("Viewport.TransformRejected"));
	}

	ImGui::PopID();
	ImGui::PopID();
	ImGui::PopID();
}

void SceneViewportPanel::DrawImViewGuizmo()
{
	if (!m_CameraController || m_ImageSize.x <= 0.0f || m_ImageSize.y <= 0.0f)
		return;

	TransformComponent* cameraTransform = m_CameraController->GetTransformComponent();
	if (!cameraTransform)
		return;

	ImViewGuizmo::BeginFrame();

	auto& style = ImViewGuizmo::GetStyle();
	style.scale = glm::clamp(glm::min(m_ImageSize.x, m_ImageSize.y) / 320.0f, 0.35f, 0.75f);

	const float gizmoDiameter = 256.0f * style.scale;
	const float buttonDiameter = style.toolButtonRadius * 2.0f * style.scale;
	const float padding = 6.0f;
	const float buttonSpacing = 4.0f;

	const ImVec2 gizmoPos(m_ImageScreenPos.x + m_ImageSize.x - gizmoDiameter * 0.5f, m_ImageScreenPos.y + gizmoDiameter * 0.5f);
	const ImVec2 dollyPos(gizmoPos.x - gizmoDiameter * 0.5f - buttonDiameter * 0.5f - buttonSpacing, m_ImageScreenPos.y + buttonDiameter * 0.5f - padding);
	const ImVec2 panPos(dollyPos.x - buttonDiameter - buttonSpacing, dollyPos.y);

	vec3 cameraPos = cameraTransform->position;
	quat cameraRot = cameraTransform->rotation;
	vec3 pivot = cameraPos + (cameraRot * vec3(0.0f, 0.0f, 1.0f)) * 5.0f;

	if (Entity* selectedEntity = SelectionManager::Get().GetPrimarySelectedEntity())
	{
		if (Scene* scene = selectedEntity->GetScene())
		{
			auto& registry = scene->GetRegistry();
			const entt::entity handle = selectedEntity->GetEntityHandle();
			if (registry.valid(handle))
			{
				if (const auto* worldTransform = registry.try_get<WorldTransformComponent>(handle))
					pivot = vec3(worldTransform->worldMatrix[3]);
			}
		}
	}

	bool cameraChanged = false;
	cameraChanged |= ImViewGuizmo::Rotate(cameraPos, cameraRot, pivot, gizmoPos, 0.0125f);
	cameraChanged |= ImViewGuizmo::Dolly(cameraPos, cameraRot, dollyPos, 0.05f);
	cameraChanged |= ImViewGuizmo::Pan(cameraPos, cameraRot, panPos, 0.01f);

	if (cameraChanged)
	{
		static_cast<void>(cameraTransform->SetLocalPosition(cameraPos));
		static_cast<void>(cameraTransform->SetLocalRotation(cameraRot));
		m_CameraController->SyncControlRotationFromTransform();
		m_CameraController->RefreshMatrices();
	}
}

void SceneViewportPanel::DrawLightOverlay()
{
	if (!m_ShowLightOverlay || !m_CameraController)
		return;

	const std::optional<CameraBuffer> cameraBuffer = m_CameraController->GetCameraBuffer();
	if (!cameraBuffer.has_value())
		return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList)
		return;

	const mat4 viewProj = cameraBuffer->viewProjMatrix;
	const vec3 cameraPos = cameraBuffer->cameraPosition;

	Scene* activeScene = SceneManager::Get().GetCurrentScene();
	if (!activeScene)
		return;

	auto view = activeScene->GetRegistry().view<LightComponent, WorldTransformComponent, FlagComponent>();
	for (auto entity : view)
	{
		const auto& light = view.get<LightComponent>(entity);
		const auto& tr = view.get<WorldTransformComponent>(entity);
		const auto& flag = view.get<FlagComponent>(entity);
		if (!light.IsEnabled || !HasFlag(flag.flags, EEntityFlag::Active))
			continue;

		vec3 worldPos = vec3(tr.worldMatrix[3]);
		vec3 forward = vec3(tr.worldMatrix[2]);
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

		switch (light.LightType)
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
			forward *= -1.0f;
			break;
		case ELightType::Point:
		default:
			color = IM_COL32(255, 220, 80, 255);
			lightIcon = ICON_FA_LIGHTBULB;
			drawDir = false;
			dirLen = 1.5f;
			break;
		}

		const float dist = glm::length(worldPos - cameraPos);
		const float nearDist = 2.0f;
		const float farDist = 40.0f;
		const float nearScale = 1.9f;
		const float farScale = 0.75f;

		const float t = glm::clamp((dist - nearDist) / (farDist - nearDist), 0.0f, 1.0f);
		const float distanceScale = glm::mix(nearScale, farScale, t);

		ImFont* font = ImGui::GetFont();
		const float iconFontSize = ImGui::GetFontSize() * 1.8f * distanceScale;
		const ImVec2 iconSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, lightIcon);

		const float fillRadius = (glm::max(iconSize.x, iconSize.y) * 0.5f + 4.0f);
		const float strokeRadius = fillRadius + 2.0f;
		const float ringThickness = glm::max(1.5f, 2.0f * distanceScale);

		drawList->AddCircleFilled(screenPos, fillRadius, IM_COL32(0, 0, 0, 140));
		drawList->AddCircle(screenPos, strokeRadius, color, 0, ringThickness);
		drawList->AddText(font, iconFontSize, ImVec2(screenPos.x - iconSize.x * 0.5f, screenPos.y - iconSize.y * 0.5f), color, lightIcon);

		if (!drawDir)
			continue;

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
			ImVec2 b(tipScreen.x - d.x * headLen + n.x * headW, tipScreen.y - d.y * headLen + n.y * headW);
			ImVec2 c(tipScreen.x - d.x * headLen - n.x * headW, tipScreen.y - d.y * headLen - n.y * headW);

			drawList->AddTriangleFilled(a, b, c, color);
		}
	}
}

void SceneViewportPanel::DrawCameraOverlay()
{
	if (!m_ShowCameraOverlay || !m_CameraController)
		return;

	const std::optional<CameraBuffer> cameraBuffer = m_CameraController->GetCameraBuffer();
	if (!cameraBuffer.has_value())
		return;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!drawList)
		return;

	Scene* activeScene = SceneManager::Get().GetCurrentScene();
	if (!activeScene)
		return;

	const mat4 editorViewProj = cameraBuffer->viewProjMatrix;
	const vec3 editorCameraPos = cameraBuffer->cameraPosition;

	const ImU32 cameraColor = IM_COL32(90, 210, 255, 255);
	const ImU32 frustumColor = IM_COL32(90, 210, 255, 200);
	const char* cameraIcon = ICON_FA_CAMERA;

	auto view = activeScene->GetRegistry().view<CameraComponent, WorldTransformComponent, FlagComponent>();
	for (auto entity : view)
	{
		const auto& targetCamera = view.get<CameraComponent>(entity);
		const auto& tr = view.get<WorldTransformComponent>(entity);
		const auto& flag = view.get<FlagComponent>(entity);
		if (!HasFlag(flag.flags, EEntityFlag::Active))
			continue;

		const vec3 worldPos = vec3(tr.worldMatrix[3]);
		ImVec2 screenPos;
		if (!WorldToViewportScreen(worldPos, editorViewProj, m_ImageScreenPos, m_ImageSize, screenPos))
			continue;

		const float dist = glm::length(worldPos - editorCameraPos);
		const float t = glm::clamp((dist - 2.0f) / (40.0f - 2.0f), 0.0f, 1.0f);
		const float distanceScale = glm::mix(1.9f, 0.75f, t);
		const float lineThickness = glm::max(1.5f, 1.8f * distanceScale);

		ImFont* font = ImGui::GetFont();
		const float iconFontSize = ImGui::GetFontSize() * 1.7f * distanceScale;
		const ImVec2 iconSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, cameraIcon);
		const float fillRadius = glm::max(iconSize.x, iconSize.y) * 0.5f + 4.0f;
		const float strokeRadius = fillRadius + 2.0f;

		drawList->AddCircleFilled(screenPos, fillRadius, IM_COL32(0, 0, 0, 140));
		drawList->AddCircle(screenPos, strokeRadius, cameraColor, 0, lineThickness);
		drawList->AddText(font, iconFontSize, ImVec2(screenPos.x - iconSize.x * 0.5f, screenPos.y - iconSize.y * 0.5f), cameraColor, cameraIcon);

		const float displayNear = 0.5f;
		const float displayFar = glm::clamp(dist * 0.5f, 2.0f, 20.0f);

		vec3 right = vec3(tr.worldMatrix[0]);
		vec3 up = vec3(tr.worldMatrix[1]);
		vec3 forward = vec3(tr.worldMatrix[2]);
		right = glm::dot(right, right) > 1e-6f ? glm::normalize(right) : vec3(1.0f, 0.0f, 0.0f);
		up = glm::dot(up, up) > 1e-6f ? glm::normalize(up) : vec3(0.0f, 1.0f, 0.0f);
		forward = glm::dot(forward, forward) > 1e-6f ? glm::normalize(forward) : vec3(0.0f, 0.0f, 1.0f);

		const float aspect = (std::isfinite(targetCamera.AspectRatio) && targetCamera.AspectRatio > 0.0f) ? targetCamera.AspectRatio : (16.0f / 9.0f);
		float nearHalfH = 0.0f, nearHalfW = 0.0f, farHalfH = 0.0f, farHalfW = 0.0f;

		if (targetCamera.CameraType == ECameraType::Perspective)
		{
			const float fovRad = glm::radians(targetCamera.FOV);
			nearHalfH = glm::tan(fovRad * 0.5f) * displayNear;
			nearHalfW = nearHalfH * aspect;
			farHalfH = glm::tan(fovRad * 0.5f) * displayFar;
			farHalfW = farHalfH * aspect;
		}
		else
		{
			const float orthoH = targetCamera.OrthographicSize * 0.5f;
			const float orthoW = orthoH * aspect;
			nearHalfH = farHalfH = orthoH;
			nearHalfW = farHalfW = orthoW;
		}

		const vec3 nearCenter = worldPos + forward * displayNear;
		const vec3 farCenter = worldPos + forward * displayFar;

		const vec3 corners[8] =
		{
			nearCenter - right * nearHalfW - up * nearHalfH,
			nearCenter + right * nearHalfW - up * nearHalfH,
			nearCenter + right * nearHalfW + up * nearHalfH,
			nearCenter - right * nearHalfW + up * nearHalfH,
			farCenter - right * farHalfW - up * farHalfH,
			farCenter + right * farHalfW - up * farHalfH,
			farCenter + right * farHalfW + up * farHalfH,
			farCenter - right * farHalfW + up * farHalfH,
		};

		const int edges[12][2] =
		{
			{ 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 },
			{ 4, 5 }, { 5, 6 }, { 6, 7 }, { 7, 4 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 },
		};

		for (int i = 0; i < 12; ++i)
		{
			DrawViewportLineIfVisible(drawList, corners[edges[i][0]], corners[edges[i][1]], editorViewProj, m_ImageScreenPos, m_ImageSize, frustumColor, lineThickness);
		}

		ImVec2 farCenterScreen;
		if (WorldToViewportScreen(farCenter, editorViewProj, m_ImageScreenPos, m_ImageSize, farCenterScreen))
			drawList->AddLine(screenPos, farCenterScreen, cameraColor, lineThickness);
	}
}

void SceneViewportPanel::DrawSelectionLocked()
{
	// 선택 잠금 상태인지 확인
	if (SelectionManager::Get().IsPrimarySelectionLocked())
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		if (!drawList) return;
		// ─── 1. 위치 계산 (좌측 하단) ───
		float padding = 15.0f; // 여백
		float fontSize = ImGui::GetFontSize();

		// 위치: 이미지 왼쪽 끝 + 패딩, 이미지 아래쪽 끝 - 패딩 - 글자 높이
		ImVec2 textPos = ImVec2(
			m_ImageScreenPos.x + padding,
			m_ImageScreenPos.y + m_ImageSize.y - padding - fontSize
		);
		// ─── 2. 가독성을 위한 그림자/배경 (선택 사항) ───
		// 검은색 그림자를 살짝 깔아주면 밝은 배경에서도 잘 보입니다.
		drawList->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200),
			LOCAL_CSTR("Viewport.SelectionLocked"));
		// ─── 3. 초록색 텍스트 출력 ───
		// 밝은 초록색 (Green)
		ImU32 textColor = IM_COL32(50, 255, 50, 255);
		drawList->AddText(textPos, textColor, LOCAL_CSTR("Viewport.SelectionLocked"));
	}
}
#pragma endregion

#pragma region Rendering
void SceneViewportPanel::SubmitLightingPass(const CameraBuffer& cameraBuffer)
{
	Renderer::Get().SubmitCustomCommand(
		[this, cameraBuffer](f32 dt, RenderPass* pass) -> EResult
		{
			auto* rhi = Renderer::Get().GetRHI();
			auto& rtMgr = RenderTargetManager::Get();
			auto& smMgr = SamplerManager::Get();
			RHISampler* sampler = smMgr.GetDefaultSampler();
			wstring prefix = m_Name + L"_";

			CameraBuffer camBuf = cameraBuffer;
			camBuf.time = dt;
			rhi->BindConstantBuffer(&camBuf, sizeof(CameraBuffer), 0);
			const wstring sourceNames[] = {
				prefix + L"GBuffer_Diffuse", prefix + L"GBuffer_Normal", prefix + L"GBuffer_PBR",
				prefix + L"GBuffer_Emission", prefix + L"GBuffer_Position", m_ShadowDepthName
			};

			// Fog 상수 버퍼 바인딩 (slot 2 -> register(b0, space3))
			FogData fogData{};
			fogData.FogEnabled = 0u;

			if (Scene* activeScene = SceneManager::Get().GetCurrentScene())
			{
				auto fogView = activeScene->GetRegistry().view<FogComponent>();
				for (auto [e, fog] : fogView.each())
				{
					if (!fog.IsEnabled) continue;

					fogData.FogEnabled = 1u;
					fogData.FogType = static_cast<uint32>(fog.FogType);
					fogData.FogColor = fog.FogColor;
					fogData.FogDensity = fog.FogDensity;
					fogData.FogHeight = fog.FogHeight;
					fogData.FogHeightFalloff = fog.FogHeightFalloff;
					fogData.FogStartDistance = fog.FogStartDistance;
					fogData.FogEndDistance = fog.FogEndDistance;
					fogData.FogMaxOpacity = fog.FogMaxOpacity;

					if (fog.AutoSyncAtmosphere)
					{
						auto skyView = activeScene->GetRegistry().view<SkyAtmosphereComponent>();
						for (auto [se, sky] : skyView.each())
						{
							if (sky.IsEnabled)
							{
								fogData.FogColor = glm::mix(fog.FogColor, sky.HorizonColor, 0.65f);
								break;
							}
						}
					}
					break;
				}
			}

			if (IsFailure(rhi->BindConstantBuffer(&fogData, sizeof(fogData), 2)))
				return EResult::Fail;

			for (uint32 i = 0; i < 6; ++i)
			{
				auto* source = rtMgr.GetRenderTarget(sourceNames[i]);
				if (!source || !source->GetTexture()) return EResult::Fail;

				if (IsFailure(rhi->BindTextureSampler(source->GetTexture(), sampler, MAX_MATERIAL_TEXTURE_SLOTS + i)))
					return EResult::Fail;
			}
			// Lighting 파이프라인 바인딩
			{
				CameraBuffer shadowCam = LightManager::Get().GetShadowCameraBuffer(0u);
				LightShadowData shadowCamData = LightManager::Get().GetShadowData(0u);
				if (IsFailure(rhi->BindConstantBuffer(&shadowCamData, sizeof(shadowCamData), 3)))
					return EResult::Fail;
			}
			if (IsFailure(rhi->BindPipeline(m_LightingPipeline)))
				return EResult::Fail;

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
		if (KEY_DOWN("1")) m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
		if (KEY_DOWN("2")) m_GizmoOperation = ImGuizmo::OPERATION::ROTATE;
		if (KEY_DOWN("3")) m_GizmoOperation = ImGuizmo::OPERATION::SCALE;
		if (KEY_DOWN(EKeyCode::L)) SelectionManager::Get().TogglePrimarySelectionLock();
	}

}
void SceneViewportPanel::MouseInput()
{
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
void SceneViewportPanel::CapturePickingRequest(bool imageHovered)
{
	if (!imageHovered || !ImGui::IsMouseClicked(ImGuiMouseButton_Left) || !ImGui::IsWindowHovered() ||
		ImGui::IsAnyItemActive() || ImGuizmo::IsOver() || ImGuizmo::IsUsing() ||
		ImViewGuizmo::IsOver() || ImViewGuizmo::IsUsing() ||
		ImGui::IsMouseDown(ImGuiMouseButton_Right) || SelectionManager::Get().IsPrimarySelectionLocked()) return;
	const ImVec2 mouse = ImGui::GetMousePos();
	vec2 uv;
	if (!PickingManager::TryGetViewportUV(vec2(mouse.x, mouse.y), vec2(m_ImageScreenPos.x, m_ImageScreenPos.y),
		vec2(m_ImageSize.x, m_ImageSize.y), uv)) return;
	if (Entity* overlayEntity = PickOverlayEntity(mouse))
	{
		m_PickingRequest.pending = false;
		auto& selection = SelectionManager::Get();
		if (ImGui::GetIO().KeyCtrl)
			selection.ToggleEntitySelection(overlayEntity);
		else
			selection.SetSelectedEntity(overlayEntity);
		return;
	}
	auto* target = RenderTargetManager::Get().GetRenderTarget(m_Name + L"_GBuffer_Position");
	if (!target || !target->GetTexture()) return;
	PickingRequest request;
	request.toggle = ImGui::GetIO().KeyCtrl;
	request.mode = m_PickingMode;
	request.uv = uv;
	request.frameSerial = PickingManager::Get().GetPickingFrameSerial();
	request.width = target->GetWidth();
	request.height = target->GetHeight();
	if (request.mode == EScenePickingMode::RayBounds && !ScreenPosToRay(mouse, request.ray, request.maxDistance))
		return;
	request.pending = true;
	m_PickingRequest = request;
}

void SceneViewportPanel::ProcessPickingRequest()
{
	if (!m_PickingRequest.pending) return;
	const PickingRequest request = m_PickingRequest;
	m_PickingRequest.pending = false;
	auto& selection = SelectionManager::Get();
	if (selection.IsPrimarySelectionLocked()) return;
	auto& picking = PickingManager::Get();
	Entity* picked = nullptr;
	if (request.mode == EScenePickingMode::GPUId)
	{
		if (request.frameSerial != picking.GetCompletedPickingFrameSerial() ||
			!picking.HasPickingPass(m_GeometryPassID)) return;
		auto* target = RenderTargetManager::Get().GetRenderTarget(m_Name + L"_GBuffer_Position");
		if (!target || !target->GetTexture() || target->GetWidth() != request.width ||
			target->GetHeight() != request.height || request.width == 0 || request.height == 0) return;
		const uint32 x = glm::min(static_cast<uint32>(request.uv.x * request.width), request.width - 1);
		const uint32 y = glm::min(static_cast<uint32>(request.uv.y * request.height), request.height - 1);
		vec4 pixel(0.0f);
		if (Renderer::Get().GetRHI()->ReadTexturePixel(target->GetTexture(), x, y, pixel) != EResult::Success)
		{
			ENGINE_LOG_ERROR("Scene viewport ID picking: pixel download failed.");
			return;
		}
		if (!std::isfinite(pixel.a) || pixel.a < 0.0f || pixel.a > 16777215.0f || std::floor(pixel.a) != pixel.a)
			return;
		picked = picking.ResolvePickingEntity(static_cast<uint32>(pixel.a));
		if (pixel.a != 0.0f && !picked) return;
	}
	else
	{
		picked = picking.PickEntityByRay(request.ray, request.maxDistance);
	}
	if (request.toggle)
	{
		if (picked) selection.ToggleEntitySelection(picked);
	}
	else
	{
		selection.SetSelectedEntity(picked);
	}
}

bool SceneViewportPanel::ScreenPosToRay(const ImVec2& mousePos, Engine::Ray& ray, float& maxDistance)
{
	if (!m_CameraController) return false;
	const std::optional<CameraBuffer> cameraBuffer = m_CameraController->GetCameraBuffer();
	if (!cameraBuffer.has_value()) return false;
	vec2 uv;
	if (!PickingManager::TryGetViewportUV(vec2(mousePos.x, mousePos.y), vec2(m_ImageScreenPos.x, m_ImageScreenPos.y),
		vec2(m_ImageSize.x, m_ImageSize.y), uv)) return false;
	return PickingManager::TryBuildRay(cameraBuffer->invViewProjMatrix, uv, ray, &maxDistance);
}
Entity* SceneViewportPanel::PickOverlayEntity(const ImVec2& mousePos) const
{
	if ((!m_ShowLightOverlay && !m_ShowCameraOverlay) || !m_CameraController) return nullptr;
	const std::optional<CameraBuffer> cameraBuffer = m_CameraController->GetCameraBuffer();
	if (!cameraBuffer.has_value()) return nullptr;
	Scene* activeScene = SceneManager::Get().GetCurrentScene();
	if (!activeScene) return nullptr;
	ImFont* font = ImGui::GetFont();
	if (!font) return nullptr;
	const mat4 viewProj = cameraBuffer->viewProjMatrix;
	const vec3 cameraPos = cameraBuffer->cameraPosition;
	const float baseFontSize = ImGui::GetFontSize();
	auto& registry = activeScene->GetRegistry();
	Entity* closestEntity = nullptr;
	float closestCameraDist = FLT_MAX;
	const auto testOverlayIcon = [&](entt::entity handle, const mat4& worldMatrix, const char* icon, float fontScale)
		{
			const vec3 worldPos = vec3(worldMatrix[3]);
			ImVec2 screenPos{};
			if (!WorldToViewportScreen(worldPos, viewProj, m_ImageScreenPos, m_ImageSize, screenPos)) return;
			const float dist = glm::length(worldPos - cameraPos);
			const float t = glm::clamp((dist - 2.0f) / (40.0f - 2.0f), 0.0f, 1.0f);
			const float distanceScale = glm::mix(1.9f, 0.75f, t);
			const float iconFontSize = baseFontSize * fontScale * distanceScale;
			const ImVec2 iconSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, icon);
			const float strokeRadius = glm::max(iconSize.x, iconSize.y) * 0.5f + 6.0f;
			const float dx = mousePos.x - screenPos.x;
			const float dy = mousePos.y - screenPos.y;
			if ((dx * dx + dy * dy) <= (strokeRadius * strokeRadius) && dist < closestCameraDist)
			{
				if (Entity* candidate = activeScene->FindEntity(handle))
				{
					closestEntity = candidate;
					closestCameraDist = dist;
				}
			}
		};
	if (m_ShowLightOverlay)
	{
		auto view = registry.view<LightComponent, WorldTransformComponent, FlagComponent>();
		for (auto entity : view)
		{
			const auto& light = view.get<LightComponent>(entity);
			const auto& tr = view.get<WorldTransformComponent>(entity);
			const auto& flag = view.get<FlagComponent>(entity);
			if (!light.IsEnabled || !HasFlag(flag.flags, EEntityFlag::Active) || HasFlag(flag.flags, EEntityFlag::Dead))
				continue;
			const char* lightIcon = ICON_FA_LIGHTBULB;
			switch (light.LightType)
			{
			case ELightType::Directional: lightIcon = ICON_FA_SUN; break;
			case ELightType::Spot: lightIcon = ICON_FA_LOCATION_DOT; break;
			case ELightType::Sky: lightIcon = ICON_FA_CLOUD_SUN; break;
			case ELightType::Point:
			default: lightIcon = ICON_FA_LIGHTBULB; break;
			}
			testOverlayIcon(entity, tr.worldMatrix, lightIcon, 1.8f);
		}
	}
	if (m_ShowCameraOverlay)
	{
		auto view = registry.view<CameraComponent, WorldTransformComponent, FlagComponent>();
		for (auto entity : view)
		{
			const auto& tr = view.get<WorldTransformComponent>(entity);
			const auto& flag = view.get<FlagComponent>(entity);
			if (!HasFlag(flag.flags, EEntityFlag::Active) || HasFlag(flag.flags, EEntityFlag::Dead))
				continue;
			testOverlayIcon(entity, tr.worldMatrix, ICON_FA_CAMERA, 1.7f);
		}
	}
	return closestEntity;
}
#pragma endregion

SkyAtmosphereData SceneViewportPanel::BuildSkyAtmosphereData()
{
	SkyAtmosphereData skyData{};
	if (!SceneManager::IsCreated()) return skyData;

	for (Scene* scene : SceneManager::Get().GetActiveScenes())
	{
		if (!scene) continue;
		auto& registry = scene->GetRegistry();

		// 1. SkyAtmosphereComponent 검색 및 시간대 계산
		auto skyView = registry.view<SkyAtmosphereComponent>();
		for (auto [e, sky] : skyView.each())
		{
			if (!sky.IsEnabled) continue;

			// 실시간 시간 흐름 처리 (1초당 24 / DayCycleDuration 시간 전진)
			if (sky.EnableTimeProgression && sky.DayCycleDuration > 0.1f)
			{
				f32 dt = ImGui::GetIO().DeltaTime;
				sky.TimeOfDay = std::fmod(sky.TimeOfDay + (24.0f / sky.DayCycleDuration) * dt, 24.0f);
				if (sky.TimeOfDay < 0.0f) sky.TimeOfDay += 24.0f;
			}

			// 시간대(TimeOfDay)와 위도(Latitude) 기반 태양 방향(To-Sun) 계산
			f32 hourAngle = (sky.TimeOfDay - 12.0f) * (glm::pi<float>() / 12.0f);
			f32 latRad = glm::radians(sky.Latitude);
			vec3 toSun = glm::normalize(vec3(std::sin(hourAngle), std::cos(hourAngle) * std::cos(latRad), -std::cos(hourAngle) * std::sin(latRad)));

			skyData.SunDirection = toSun;
			skyData.ZenithColor = sky.ZenithColor;
			skyData.HorizonColor = sky.HorizonColor;
			skyData.GroundColor = sky.GroundColor;
			skyData.HorizonBlend = sky.HorizonBlend;
			skyData.SkyIntensity = sky.SkyIntensity;
			skyData.SunSize = sky.SunSize;
			skyData.SunColor = sky.CustomSunColor;
			skyData.SunIntensity = sky.CustomSunIntensity;

			skyData.CloudCoverage = sky.CloudCoverage;
			skyData.CloudDensity = sky.CloudDensity;
			skyData.CloudSpeed = sky.CloudSpeed;
			skyData.CloudAltitude = sky.CloudAltitude;
			skyData.EnableClouds = sky.EnableClouds ? 1u : 0u;

			// Directional Light가 존재하고 AutoSync가 켜져 있으면 라이트 회전도 동기화
			if (sky.AutoSyncDirectionalLight)
			{
				auto lightView = registry.view<LightComponent, TransformComponent>();
				for (auto [le, light, tr] : lightView.each())
				{
					if (light.IsEnabled && light.LightType == ELightType::Directional)
					{
						// 태양 광선 Forward = -ToSun
						vec3 forward = -toSun;
						f32 pitch = glm::degrees(std::asin(-forward.y));
						f32 yaw = glm::degrees(std::atan2(forward.x, forward.z));
						tr.eulerRotation = vec3(pitch, yaw, 0.0f);
						tr.rotation = glm::quat(glm::radians(tr.eulerRotation));

						skyData.SunColor = light.Color;
						skyData.SunIntensity = light.Intensity;
						break;
					}
				}
			}
			return skyData;
		}
	}
	return skyData;
}