#pragma once

#include "SceneViewportPanel.h"
#include "EditorUI.h"
#include "ShortcutManager.h"
#include "EditorCameraController.h"
#include "Application.h"
#include "RenderTargetManager.h"
#include "SelectionManager.h"
#include "EditorSettingsService.h"
#include "InputManager.h"
#include "ImViewGuizmoCompat.h"
#include "SceneManager.h"

#include "Entity.h"
#include "Scene.h"
#include "WorldComponents.h"
#include "RenderComponents.h"
#include "Light.h"
#include "TimeManager.h"
#include "SkyAtmosphereComponent.h"
#include "VolumetricCloudComponent.h"
#include "TransformSystem.h"
#include "TransformPropertyEditor.h"
#include "PickingManager.h"
#include "PIEManager.h"

#include <glm/gtc/type_ptr.hpp>

#include "TerrainEditor.h"
#include "TerrainComponent.h"
#include "TerrainUtility.h"
#include "Collision.h"
#include "SceneEditHistory.h"
#include "EditorSettings.h"
#include "PostProcessComponent.h"
#include "NotificationManager.h"
#include <archive/JsonArchive.h>
#include "PrefabModeManager.h"
#include "Model.h"
#include "ResourceManager.h"
#include "TilePalettePanel.h"
#include "SplineCurve.h"
#include "ImGuiManager.h"
#include "ContentBrowserDragPayload.h"

#pragma region Helper
struct LightingPassData
{
	LightShadowData Shadow{};
	VolumetricCloudData Cloud{};
	vec3 SunDirection{ 0.0f, 1.0f, 0.0f };
	f32 PlanetRadius = 6360000.0f;
	uint32 HasIBLMaps = 0u;
	f32 IBLIntensity = 1.0f;
	vec2 _iblPad{ 0.0f };
	vec4 _reserved{ 0.0f };
};
static_assert(sizeof(LightingPassData) == 512);
static_assert(offsetof(LightingPassData, Shadow) == 0);
static_assert(offsetof(LightingPassData, Cloud) == 336);
static_assert(offsetof(LightingPassData, SunDirection) == 464);
static_assert(offsetof(LightingPassData, PlanetRadius) == 476);

static bool WorldToViewportScreen(const vec3& worldPos, const mat4& viewProj,
	const ImVec2& imageScreenPos, const ImVec2& imageSize, ImVec2& outScreenPos)
{
	if (!std::isfinite(imageSize.x) || !std::isfinite(imageSize.y) ||
		imageSize.x <= 0.0f || imageSize.y <= 0.0f)
		return false;

	const vec4 clip = viewProj * vec4(worldPos, 1.0f);
	if (!std::isfinite(clip.x) || !std::isfinite(clip.y) ||
		!std::isfinite(clip.z) || !std::isfinite(clip.w) || clip.w <= 0.000001f)
		return false;

	const vec3 ndc = vec3(clip) / clip.w;
	if (ndc.z < 0.0f || ndc.z > 1.0f ||
		ndc.x < -1.0f || ndc.x > 1.0f || ndc.y < -1.0f || ndc.y > 1.0f)
		return false;

	const ImVec2 candidate(
		imageScreenPos.x + (ndc.x * 0.5f + 0.5f) * imageSize.x,
		imageScreenPos.y + (0.5f - ndc.y * 0.5f) * imageSize.y);

	if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y))
		return false;

	outScreenPos = candidate;
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

static bool IsSceneOverlayVisible(const Engine::FlagComponent& flag)
{
	return flag.IsActiveInHierarchy() && flag.IsVisibleInHierarchy() &&
		!HasFlag(flag.effectiveFlags, Engine::EEntityFlag::Dead);
}

static bool TryTransformSplinePoint(const glm::dmat4& matrix, const vec3& position, vec3& result)
{
	const glm::dvec4 transformed = matrix * glm::dvec4(position, 1.0);
	const double limit = static_cast<double>((std::numeric_limits<float>::max)());

	if (!std::isfinite(transformed.x) || !std::isfinite(transformed.y) || !std::isfinite(transformed.z) ||
		std::abs(transformed.x) > limit || std::abs(transformed.y) > limit || std::abs(transformed.z) > limit)
		return false;

	result = vec3(transformed);
	return true;
}

static bool TryBuildSplineRollGizmo(const Engine::SplineComponent& spline, size_t index,
	const mat4& ownerWorld, const vec3& worldPosition, mat4& result)
{
	if (!spline.IsValid() || spline.Points.size() < 2 || index >= spline.Points.size())
		return false;

	std::array<vec3, Engine::SplineComponent::MaxControlPoints> positions{};
	for (size_t i = 0; i < spline.Points.size(); ++i)
		positions[i] = spline.Points[i].Position;

	const size_t segmentCount = spline.Closed ? spline.Points.size() : spline.Points.size() - 1;
	const double parameter = static_cast<double>(index) / static_cast<double>(segmentCount);

	Engine::SplineSample sample;
	if (!Engine::SplineCurve::Evaluate(
		std::span<const vec3>(positions.data(), spline.Points.size()), spline.Closed, parameter, sample))
		return false;

	glm::dvec3 forward = glm::dmat3(ownerWorld) * glm::dvec3(sample.Tangent);
	const double length = glm::length(forward);
	if (!std::isfinite(length) || length <= 0.0)
		return false;

	forward /= length;

	// 링의 평면을 구성합니다. 조작 축은 forward 하나만 사용합니다.
	const glm::dvec3 referenceUp = std::abs(forward.y) < 0.99 ?
		glm::dvec3(0.0, 1.0, 0.0) : glm::dvec3(1.0, 0.0, 0.0);
	const glm::dvec3 right = glm::normalize(glm::cross(referenceUp, forward));
	const glm::dvec3 up = glm::cross(forward, right);

	const double radians = glm::radians(std::remainder(static_cast<double>(spline.Points[index].RollDegrees), 360.0));
	const double cosine = std::cos(radians);
	const double sine = std::sin(radians);

	result = mat4(1.0f);
	result[0] = vec4(vec3(right * cosine + up * sine), 0.0f);
	result[1] = vec4(vec3(up * cosine - right * sine), 0.0f);
	result[2] = vec4(vec3(forward), 0.0f);
	result[3] = vec4(worldPosition, 1.0f);
	return true;
}
#pragma endregion

#pragma region Contructor&Destructor
EResult SceneViewportPanel::OnInitialize(void* arg)
{
	CAST_DESC
	const EResult result = CameraViewportPanel::OnInitialize(static_cast<CameraViewportPanelDesc*>(desc));
	if (result != EResult::Success)
		return result;

	m_RenderWidth = desc->RenderTargetWidth;
	m_RenderHeight = desc->RenderTargetHeight;
#pragma region Prepare RenderPass And Pipelines
	m_Pipeline.Initialize(m_Name, m_RenderWidth, m_RenderHeight, true);
	m_PostProcessChain.Initialize(m_Name);
	PrepareRenderTargetsAndPasses(m_RenderWidth, m_RenderHeight);
#pragma endregion
	m_Grid.Initialize(m_Name);
	m_DebugRenderer.Initialize(m_Name);
	if (!m_PostProcessChain.GetPipeline().SaveToString(m_DefaultPostProcessSettings))
		EDITOR_LOG_ERROR("Default post-process settings could not be captured: {}", WStrToStr(m_Name));
	m_SelectedRTName = m_LightingColorName;
	return EResult::Success;
}
void SceneViewportPanel::PrepareRenderTargetsAndPasses(uint32 width, uint32 height)
{
	wstring prefix = m_Name + L"_";
	auto& rpMgr = RenderPassManager::Get();

	m_OwnedRTNames = m_Pipeline.GetOwnedRTNames();
	m_ShadowDepthName = m_Pipeline.GetShadowDepthRTName();
	m_PointShadowDepthName = m_Pipeline.GetPointShadowDepthRTName();
	m_LightingColorName = m_Pipeline.GetFinalColorRTName();

	m_ShadowPassID = m_Pipeline.GetShadowPassID();
	m_PointShadowPassID = m_Pipeline.GetPointShadowPassID();
	m_GeometryPassID = m_Pipeline.GetGeometryPassID();
	m_LightingPassID = m_Pipeline.GetLightingPassID();
	m_ForwardPassID = m_Pipeline.GetForwardPassID();
	m_ForwardTransparentPassID = m_Pipeline.GetForwardTransparentPassID();

	// UIOverlayPass (UI 전용 풀스크린 합성 패스)
	m_UIOverlayPassID = rpMgr.RegisterRenderPass(
		prefix + L"UIOverlayPass", {}, L"",
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.f, 0.f, 0.f, -1.f), 200, ERenderSortType::FrontToBack, ERenderPassType::UI,
		EBlendMode::Opaque | EBlendMode::Masked | EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::NonPremultiplied);

	m_PostProcessChain.PrepareResources(width, height);

	m_ChannelPreviewPassID = rpMgr.RegisterRenderPass(
		prefix + L"ChannelPreviewPass", { m_LightingColorName }, L"",
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 400, ERenderSortType::None, ERenderPassType::Custom, EBlendMode::None);

	m_PassOptions = {
		{ L"Shadow",             m_ShadowPassID,             true },
		{ L"PointShadow",        m_PointShadowPassID,        true },
		{ L"Geometry",           m_GeometryPassID,           true },
		{ L"Lighting",           m_LightingPassID,           true },
		{ L"Forward",            m_ForwardPassID,            true },
		{ L"ForwardTransparent", m_ForwardTransparentPassID, true },
		{ L"UIOverlay",          m_UIOverlayPassID,          true },
	};
}
void SceneViewportPanel::PreparePipelines()
{
	// 조명 합성 파이프라인은 m_Pipeline 내부에서 생성·관리됨
}

void SceneViewportPanel::Free()
{
	ResetSplinePointSelection();
	if (s_ActiveSceneViewport == this)
		s_ActiveSceneViewport = nullptr;
	__super::Free();
	m_Grid.Free();
	m_DebugRenderer.Free();
	m_PostProcessChain.Free();
	m_Pipeline.Free();
}
#pragma endregion

#pragma region Workspace Settings
void SceneViewportPanel::RestoreViewportState(const ViewportWorkspace& state)
{
	CameraViewportPanel::RestoreViewportState(state);

	auto& passManager = RenderPassManager::Get();

	for (auto& option : m_PassOptions)
	{
		auto* renderPass = passManager.GetRenderPassByID(option.PassID);

		if (!renderPass)
			continue;

		const auto found = state.RenderPassStates.find(option.Name);
		const bool enabled = found != state.RenderPassStates.end() ? found->second : true;

		option.Enabled = enabled;
		renderPass->SetEnabled(enabled);
	}
}

ViewportWorkspace SceneViewportPanel::CaptureViewportState() const
{
	auto state = CameraViewportPanel::CaptureViewportState();
	auto& passManager = RenderPassManager::Get();

	for (const auto& option : m_PassOptions)
	{
		const auto* renderPass = passManager.GetRenderPassByID(option.PassID);

		if (renderPass)
			state.RenderPassStates[option.Name] = renderPass->IsEnabled();
	}

	return state;
}
#pragma endregion

#pragma region Loop
void SceneViewportPanel::Update(f32 dt)
{
	// 1. 창이 닫혔거나 다른 탭 뒤에 숨겨진 경우 렌더링 100% 차단 (0 FPS)
	if (!m_Open || !m_IsVisibleOnScreen)
		return;
	// 2. 적응형 프레임 쓰로틀링 판정
	const bool isHighPriority = m_Focused || m_Hovered || PIEManager::Get().IsPlaying();
	const float targetInterval = isHighPriority ? 0.0f : (1.0f / 15.0f); // 비포커스 시 15 FPS
	m_TimeSinceLastRender += dt;
	if (m_TimeSinceLastRender < targetInterval)
	{
		// 이번 프레임은 GPU 렌더링 제출 생략 (이전 프레임의 RT 이미지를 그대로 표시)
		return;
	}
	m_TimeSinceLastRender = 0.0f;
	// Download the clicked frame before resizing/replacing its render targets.
	ProcessPickingRequest();
	if (!CalculateRenderResolution(m_PanelWidth, m_PanelHeight))
		return;

	// 1. 에디터 카메라 입력: PIE 실행 중에는 에디터 카메라 이동을 차단
	if (m_Open && m_CameraController && m_Focused && m_Hovered)
	{
		if (!IsOrthographic())
		{
			m_CameraController->HandleInput(dt);
		}
		else
		{
			const vec2 mouse = InputManager::Get().GetScreenMousePosition();
			vec2 uv;
			const bool imageHovered = PickingManager::TryGetViewportUV(
				mouse, vec2(m_ImageScreenPos.x, m_ImageScreenPos.y), vec2(m_ImageSize.x, m_ImageSize.y), uv);
			const bool popupOpen = ImGui::IsPopupOpen(
				nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel);

			if (imageHovered && !popupOpen && !ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive() &&
				!ShortcutManager::Get().IsCapturing())
			{
				m_CameraController->HandleInput(dt, true, m_ImageSize.y);
			}
		}
	}

	if (m_CameraController)
	{
		if (m_RenderHeight > 0)
			m_CameraController->SetAspectRatio(static_cast<f32>(m_RenderWidth) / static_cast<f32>(m_RenderHeight));
		m_CameraController->RefreshMatrices();
	}

	// 2. 카메라 버퍼 결정: PIE 중이면 씬 카메라 우선, 없으면 에디터 카메라로 폴백
	const std::optional<CameraBuffer> cameraBuffer = GetActiveCameraBuffer();
	Scene* activeScene = SceneManager::Get().GetCurrentScene();
	if (cameraBuffer.has_value())
		m_Pipeline.Submit(*cameraBuffer, activeScene);
	RenderPassManager& rpMgr = RenderPassManager::Get();
	Renderer& renderer = Renderer::Get();
	auto isPassEnabled = [this](RenderPassID passID) -> bool {
		for (const auto& opt : m_PassOptions)
		{
			if (opt.PassID == passID) return opt.Enabled;
		}
		return true;
		};
	if (isPassEnabled(m_UIOverlayPassID))
		renderer.RegisterViewportCamera(rpMgr.GetRenderPassByID(m_UIOverlayPassID));
	uint32 ppSlot = 0;
	wstring currentRT = m_LightingColorName;
	// Lighting이 꺼진 경우 디버깅 화면으로 GBuffer_Diffuse(0번 인덱스)를 우회 표시
	if (!isPassEnabled(m_LightingPassID) && !m_OwnedRTNames.empty())
		currentRT = m_OwnedRTNames[0];
	SubmitPostProcessPass(dt, ppSlot, currentRT);
	if (isPassEnabled(m_UIOverlayPassID))
		SubmitUIOverlayPass(currentRT);
	if (m_ShowFinalComposed)
		m_DisplayRTName = currentRT;
	else
		m_DisplayRTName = m_SelectedRTName;

	// 채널 프리뷰를 제출하기 직전에 추가
	const auto* displayRT = RenderTargetManager::Get().GetRenderTarget(m_DisplayRTName);
	const auto* displayTexture = displayRT ? displayRT->GetTexture() : nullptr;
	const bool canDrawSceneOverlays = displayTexture &&
		HasFlag(displayTexture->GetUsage(), Engine::ETextureUsage::RenderTarget);

	m_ChannelFilter.SubmitChannelPreviewPass(m_DisplayRTName, m_DisplayRTName);

	// 기존 격자·디버그 선 제출 블록을 교체
	if (cameraBuffer.has_value() && canDrawSceneOverlays)
	{
		if (*m_Grid.GetVisible())
			m_Grid.SubmitGrid(*cameraBuffer, IsOrthographic(), m_DisplayRTName, m_OwnedRTNames[6]);

		DrawSelectedSpline(activeScene);
		m_DebugRenderer.SubmitDebugDraw(*cameraBuffer, m_DisplayRTName);
	}

	MouseInput();
}

void SceneViewportPanel::Draw()
{
	__super::Draw();
}
#pragma endregion

#pragma region Custom Draws
float SceneViewportPanel::GetCustomOptionsWidth() const
{
	const auto& prefabMode = PrefabModeManager::Get();
	if (prefabMode.IsInPrefabMode())
	{
		const std::string title = std::string(ICON_FA_CUBES " ") + LOCAL("PrefabMode.Title") + ": " +
			prefabMode.GetPrefabPath().filename().string() + (prefabMode.HasUnsavedChanges() ? " *" : "");
		const std::string backLabel = std::string(ICON_FA_ARROW_LEFT " ") + LOCAL("PrefabMode.BackToScene");
		const std::string cameraLabel = std::string(ICON_FA_CAMERA " ") + LOCAL("Viewport.CameraMenu");
		return MeasureOptionLabels({ backLabel.c_str(), title.c_str(), cameraLabel.c_str(),
			LOCAL_CSTR("Viewport.GizmoMenu") }) + EditorUI::GetIconButtonSize(ICON_FA_FLOPPY_DISK, true).x +
			ImGui::GetStyle().ItemSpacing.x;
	}
	return CameraViewportPanel::GetCustomOptionsWidth() + MeasureOptionLabels({
		LOCAL_CSTR("Viewport.GizmoMenu"), LOCAL_CSTR("Viewport.DebugMenu"), LOCAL_CSTR("PostProcess.MenuTitle"),
		LOCAL_CSTR("Viewport.RenderPassesMenu"), LOCAL_CSTR("Viewport.Picking") }) +
		EditorUI::GetIconButtonSize(ICON_FA_CLOCK, true).x + ImGui::GetStyle().ItemSpacing.x;
}

void SceneViewportPanel::DrawCustomOptions()
{
	auto& prefabMode = PrefabModeManager::Get();
	if (prefabMode.IsInPrefabMode())
	{
		// 1. 좌측: [<- 씬으로 복귀] | 프리팹: 에셋명 * |
		prefabMode.DrawMenuBarUI();

		// 2. 중앙: 프리팹 편집에 필수적인 기즈모 및 카메라 메뉴
		DrawGizmoMenu();
		DrawCameraMenu();

		if (EditorUI::SmallIconButton("SavePrefab", ICON_FA_FLOPPY_DISK, "PrefabEditor.Save"))
			prefabMode.SavePrefab();


		return;
	}

	EditorUI::IconButtonOptions options;
	options.Compact = true;
	options.Active = m_IsRealtime;
	options.ShortcutHint = ShortcutManager::Get().GetShortcutHint("Viewport.ToggleRealtime");
	if (EditorUI::IconButton("Realtime", ICON_FA_CLOCK, "Viewport.RealtimeTooltip", options)) ToggleRealtime();

	// 일반 씬 뷰포트 헤더바
	DrawResolutionMenu();
	DrawSceneRenderTargetMenu();
	DrawCameraMenu();
	DrawGizmoMenu();
	DrawDebugMenu();
	DrawPostProcessMenu();
	DrawRenderPassMenu();
	DrawPickingMenu();
}

static Engine::Entity* InstantiateDroppedModel(Engine::Scene& scene, const std::wstring& path)
{
	auto& resources = Engine::ResourceManager::Get();
	const Engine::ResourceHandle<Engine::Model> model(resources.LoadFile(path));
	if (!model.IsValid())
		return nullptr;

	const wchar_t* materialPath = model->GetSkeleton().IsValid()
		? L"Resources/Material/SkinningMaterial.bammat.json"
		: L"Resources/Material/GBufferMaterial.bammat.json";
	const Engine::ResourceHandle<Engine::MaterialInterface> material(resources.LoadFile(materialPath));
	Engine::Entity* root = nullptr;
	if (material.IsValid() && scene.InstantiateLinkedModel(model, material, root) == Engine::EResult::Success)
		return root;
	return nullptr;
}

static void PlaceDroppedAssets(const ImGuiPayload& payload, bool multiple)
{
	const auto paths = ContentBrowser::DecodeDragPaths(payload.Data, payload.DataSize);
	if (multiple ? paths.size() < 2 : paths.size() != 1)
		return;

	auto& sceneManager = SceneManager::Get();
	auto* scene = sceneManager.GetCurrentScene();
	if (!scene)
		return;

	auto& history = Application::Get().GetEditHistory();
	SceneEditSnapshot before;
	const char* historyLabel = "History.Command.PlaceAssets";
	std::vector<Engine::Entity*> spawnedRoots;
	bool attemptedPlacement = false;
	for (const auto& path : paths)
	{
		const std::filesystem::path file(path);
		const auto filename = Engine::ToLowerStr(Engine::WStrToStr(file.filename().wstring()));
		const bool isModel = filename.ends_with(".bammodel") || filename.ends_with(".bammodel.json");
		const bool isPrefab = !isModel && (filename.ends_with(".bamprefab") || filename.ends_with(".prefab") ||
			(multiple ? filename.ends_with(".bamprefab.json") || filename.ends_with(".prefab.json") : filename.ends_with(".json")));
		if (!isModel && !isPrefab)
			continue;

		if (!attemptedPlacement && history.Enabled())
			before = SceneEditSnapshot::Capture(*scene);
		attemptedPlacement = true;

		Engine::Entity* root = nullptr;
		if (isModel)
			root = InstantiateDroppedModel(*scene, path);
		else
		{
			archive::JsonArchive ar;
			if (sceneManager.InstantiatePrefab(ar, path, root, nullptr) != Engine::EResult::Success)
				root = nullptr;
		}
		if (!root)
		{
			if (multiple)
				EDITOR_LOG_WARN("Failed to place dropped asset: {}", Engine::WStrToStr(path));
			continue;
		}

		spawnedRoots.push_back(root);
		if (!multiple)
		{
			historyLabel = isModel ? "History.Command.CreateEntity" : "History.Command.InstantiatePrefab";
			if (isPrefab)
				NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"),
					LOCAL_FMT("Notification.PrefabInstantiated", Engine::WStrToStr(file.stem().wstring())), ELogLevel::Info, 3.0f);
		}
	}

	if (!spawnedRoots.empty())
	{
		if (before)
			history.Record(std::move(before), SceneEditSnapshot::Capture(*scene), historyLabel);

		auto& selection = SelectionManager::Get();
		if (!selection.IsPrimarySelectionLocked())
		{
			selection.ClearSelection();
			for (auto* root : spawnedRoots)
				selection.AddToSelection(root);
		}
	}

	if (multiple && spawnedRoots.size() != paths.size())
		NotificationManager::Get().AddNotification(LOCAL("ContentBrowser.Title"),
			LOCAL_FMT("ContentBrowser.PlaceAssetsResult", spawnedRoots.size(), paths.size()), ELogLevel::Warn);
}

void SceneViewportPanel::DrawCustomViewport()
{

	m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
	m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
	if (m_Focused || !s_ActiveSceneViewport)
		s_ActiveSceneViewport = this;
	Application::Get().GetEditHistory().AllowShortcuts(m_Focused);
	PrefabModeManager::Get().DrawViewportBanner();
	BaseViewportPanel::DrawCustomViewport();
	if (m_ImageSize.x <= 0.f || m_ImageSize.y <= 0.f)
		return;
	const bool imageHovered = ImGui::IsItemHovered();

	if (ImGui::BeginDragDropTarget())
	{
		if (const auto* payload = ImGui::AcceptDragDropPayload(ContentBrowser::MultipleItemsPayload))
			PlaceDroppedAssets(*payload, true);
		else if (const auto* single = ImGui::AcceptDragDropPayload(ContentBrowser::SingleItemPayload))
			PlaceDroppedAssets(*single, false);
		ImGui::EndDragDropTarget();
	}

	if (Scene* scene = SceneManager::Get().GetCurrentScene())
		scene->UpdateHierarchyFlags();
	DrawImViewGuizmo();
	const bool splineOwnsInput = DrawSplinePointEditor(imageHovered);
	const bool tilemapOwnsInput = HandleTilemapPainting(imageHovered);
	if (!splineOwnsInput && !tilemapOwnsInput)
		DrawImGuizmo();
	// UIOverlay 패스가 켜져 있을 때만 에디터 뷰포트 아이콘 오버레이를 표시!
	if (auto* uiPass = RenderPassManager::Get().GetRenderPassByID(m_UIOverlayPassID); !uiPass || uiPass->IsEnabled())
	{
		DrawLightOverlay();
		DrawCameraOverlay();
	}
	DrawSelectionLocked();
	if (!splineOwnsInput && !tilemapOwnsInput)
	{
		CapturePickingRequest(imageHovered);
		HandleTerrainSculpting(imageHovered);
	}
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
	if (ImGui::BeginMenu(LOCAL_CSTR("Viewport.RenderTargetsMenu")))
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
	if (ImGui::BeginMenu(LOCAL_CSTR("Viewport.GizmoMenu")))
	{
		auto& sm = ShortcutManager::Get();
		if (ImGui::MenuItem(LOCAL_LABEL("Action.Viewport.GizmoTranslate").c_str(), sm.GetShortcutHint("Viewport.GizmoTranslate"), m_GizmoOperation == ImGuizmo::TRANSLATE))
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::MenuItem(LOCAL_LABEL("Action.Viewport.GizmoRotate").c_str(), sm.GetShortcutHint("Viewport.GizmoRotate"), m_GizmoOperation == ImGuizmo::ROTATE))
			m_GizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::MenuItem(LOCAL_LABEL("Action.Viewport.GizmoScale").c_str(), sm.GetShortcutHint("Viewport.GizmoScale"), m_GizmoOperation == ImGuizmo::SCALE))
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
			auto& settings = EditorSettingsService::Get();
			const auto& prefs = settings.GetPreferences();
			float snapT = prefs.GizmoSnapTranslation;
			float snapR = prefs.GizmoSnapRotation;
			float snapS = prefs.GizmoSnapScale;

			bool changed = false;
			changed |= ImGui::DragFloat(LOCAL_LABEL("Viewport.SnapTranslation").c_str(), &snapT,
				0.1f, 0.01f, 100.0f, "%.2f");
			changed |= ImGui::DragFloat(LOCAL_LABEL("Viewport.SnapRotation").c_str(), &snapR,
				1.0f, 1.0f, 180.0f, "%.1f deg");
			changed |= ImGui::DragFloat(LOCAL_LABEL("Viewport.SnapScale").c_str(), &snapS,
				0.05f, 0.01f, 10.0f, "%.2f");

			if (changed)
			{
				const auto result = settings.SetGizmoSnapping(snapT, snapR, snapS);
				if (!result.Applied || result.SaveStatus != EEditorSettingsSaveStatus::Saved)
					ImGuiManager::Get().NotifySettingsChange(result);
			}
		}
		ImGui::EndMenu(); // 1번만 호출되도록 정리
	}
}
void SceneViewportPanel::DrawDebugMenu()
{
	if (ImGui::BeginMenu(LOCAL_CSTR("Viewport.DebugMenu")))
	{
		ImGui::MenuItem(LOCAL_LABEL("Viewport.ShowGrid").c_str(), nullptr, m_Grid.GetVisible());
		ImGui::MenuItem(LOCAL_LABEL("Viewport.ShowCollider").c_str(), nullptr, m_DebugRenderer.GetDrawCollidersPtr());
		ImGui::EndMenu();
	}
}
void SceneViewportPanel::DrawPostProcessMenu()
{
	m_PostProcessChain.DrawImGuiMenu();
}
void SceneViewportPanel::DrawRenderPassMenu()
{
	if (ImGui::BeginMenu(LOCAL_CSTR("Viewport.RenderPassesMenu")))
	{
		auto& rpMgr = RenderPassManager::Get();
		for (auto& pass : m_PassOptions)
		{
			string label = WStrToStr(pass.Name);
			if (ImGui::MenuItem(label.c_str(), nullptr, &pass.Enabled))
			{
				if (auto* renderPass = rpMgr.GetRenderPassByID(pass.PassID))
					renderPass->SetEnabled(pass.Enabled);
			}
		}
		ImGui::EndMenu();
	}
}
EResult SceneViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
	if (width == 0 || height == 0)
		return EResult::InvalidArgument;

	bool succeeded = true;
	if (m_Pipeline.Resize(width, height) != EResult::Success)
		succeeded = false;

	m_OwnedRTNames = m_Pipeline.GetOwnedRTNames();

	if (m_PostProcessChain.Resize(width, height) != EResult::Success)
		succeeded = false;

	if (BaseViewportPanel::ResizeRenderTargets(width, height) != EResult::Success)
		succeeded = false;

	if (!succeeded)
		return EResult::Fail;

	if (m_CameraController)
	{
		m_CameraController->SetAspectRatio(static_cast<f32>(width) / static_cast<f32>(height));
		m_CameraController->RefreshMatrices();
	}

	return EResult::Success;
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

	const std::optional<CameraBuffer> cameraBuffer = GetActiveCameraBuffer();

	if (!cameraBuffer)
	{
		return;
	}

	const glm::mat4 viewMatrix = cameraBuffer->viewMatrix;
	const glm::mat4 projectionMatrix = cameraBuffer->projMatrix;

	const auto& prefs = EditorSettingsService::Get().GetPreferences();
	bool useSnap = m_GizmoUseSnap || InputManager::Get().IsCtrlPressed();
	glm::vec3 snapValues = glm::vec3(prefs.GizmoSnapTranslation);
	if (editMode == TransformEditMode::Rotation)
	{
		snapValues = glm::vec3(prefs.GizmoSnapRotation);
	}
	else if (editMode == TransformEditMode::Scale)
	{
		snapValues = glm::vec3(prefs.GizmoSnapScale);
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
		{
			history.Record(std::move(before), SceneEditSnapshot::Capture(*scene, targets), "History.Command.Transform", true);
			if (PrefabModeManager::Get().IsInPrefabMode())
				PrefabModeManager::Get().MarkDirty();
		}
		else ImGui::SetTooltip("%s", LOCAL_CSTR("Viewport.TransformRejected"));
	}

	ImGui::PopID();
	ImGui::PopID();
	ImGui::PopID();
}

void SceneViewportPanel::DrawImViewGuizmo()
{
	if (IsOrthographic() || !m_CameraController || m_ImageSize.x <= 0.0f || m_ImageSize.y <= 0.0f)
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

	const std::optional<CameraBuffer> cameraBuffer = GetActiveCameraBuffer();
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
		if (!light.IsEnabled || !IsSceneOverlayVisible(flag))
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

	const std::optional<CameraBuffer> cameraBuffer = GetActiveCameraBuffer();
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
		const auto& camera = view.get<CameraComponent>(entity);
		const auto& tr = view.get<WorldTransformComponent>(entity);
		const auto& flag = view.get<FlagComponent>(entity);
		if (!IsSceneOverlayVisible(flag))
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

		const float aspect = (std::isfinite(camera.AspectRatio) && camera.AspectRatio > 0.0f) ? camera.AspectRatio : (16.0f / 9.0f);
		float nearHalfH = 0.0f, nearHalfW = 0.0f, farHalfH = 0.0f, farHalfW = 0.0f;

		if (camera.CameraType == ECameraType::Perspective)
		{
			const float fovRad = glm::radians(camera.FOV);
			nearHalfH = glm::tan(fovRad * 0.5f) * displayNear;
			nearHalfW = nearHalfH * aspect;
			farHalfH = glm::tan(fovRad * 0.5f) * displayFar;
			farHalfW = farHalfH * aspect;
		}
		else
		{
			const float orthoH = camera.OrthographicSize * 0.5f;
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
bool SceneViewportPanel::SyncPostProcess(Scene& scene, entt::entity entity, const PostProcessComponent& source)
{
	const auto restoreRequest = m_PostProcessChain.TakeSceneRestoreRequest();
	const bool restoreRequested = restoreRequest &&
		restoreRequest->TargetScene == &scene && restoreRequest->TargetEntity == entity;

	if (restoreRequest && !restoreRequested)
	{
		m_PostProcessChain.CompleteSceneRestore(false);

		NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
			LOCAL("PostProcess.RestoreUnavailable"), ELogLevel::Warn);
	}

	const bool unchanged = !restoreRequested &&
		m_LastBoundScene == &scene && m_LastPostProcessEntity == entity &&
		m_LastPostProcessPreset == source.PresetName && m_LastPostProcessSettings == source.PipelineSettings;

	if (unchanged)
		return m_PostProcessStateUsable;

	m_LastBoundScene = &scene;
	m_LastPostProcessEntity = entity;
	m_LastPostProcessPreset = source.PresetName;
	m_LastPostProcessSettings = source.PipelineSettings;

	EDITOR_LOG_INFO("Post-process source: scene={}, preset={}, inlineBytes={}",
		WStrToStr(scene.GetName()), source.PresetName, source.PipelineSettings.size());

	const auto notifyFailure = [this, restoreRequested]
		{
			if (restoreRequested)
				m_PostProcessChain.CompleteSceneRestore(false);

			const char* key = m_PostProcessStateUsable
				? "PostProcess.ApplyFailed" : "PostProcess.RestoreFailed";

			NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"), LOCAL(key), ELogLevel::Error);
		};

	auto& pipeline = m_PostProcessChain.GetPipeline();
	const bool hadUsableState = m_PostProcessStateUsable;
	std::string previousSettings;

	if (m_DefaultPostProcessSettings.empty() ||
		(hadUsableState && !pipeline.SaveToString(previousSettings)))
	{
		EDITOR_LOG_ERROR("Post-process settings backup is unavailable: {}", WStrToStr(m_Name));
		notifyFailure();
		return m_PostProcessStateUsable;
	}

	bool applied = pipeline.LoadFromString(m_DefaultPostProcessSettings);

	if (applied && !source.PipelineSettings.empty())
	{
		applied = pipeline.LoadFromString(source.PipelineSettings);
	}
	else if (applied && !source.PresetName.empty())
	{
		const auto path = StrToWStr("Resources/Config/PostProcess/Presets/" + source.PresetName + ".json");
		applied = pipeline.LoadFromFile(path);
	}

	if (applied)
	{
		std::string actualSettings;

		if (pipeline.SaveToString(actualSettings))
			EDITOR_LOG_INFO("Post-process applied settings: {}", actualSettings);

		m_PostProcessStateUsable = true;
		if (restoreRequested)
		{
			m_PostProcessChain.CompleteSceneRestore(true);

			NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"),
				LOCAL("PostProcess.SceneRestored"), ELogLevel::Info);
		}

		return true;
	}

	m_PostProcessStateUsable = hadUsableState && pipeline.LoadFromString(previousSettings);

	EDITOR_LOG_ERROR("Post-process apply failed: viewport={}, preset={}, restored={}",
		WStrToStr(m_Name), source.PresetName, m_PostProcessStateUsable);

	notifyFailure();
	return m_PostProcessStateUsable;
}

void SceneViewportPanel::SubmitPostProcessPass(f32 dt, uint32& slot, std::wstring& currentRT)
{
	if (Scene* activeScene = SceneManager::Get().GetCurrentScene())
	{
		auto view = activeScene->GetRegistry().view<PostProcessComponent>();

		for (auto [entity, source] : view.each())
		{
			if (!source.IsEnabled)
				continue;

			if (SyncPostProcess(*activeScene, entity, source))
				currentRT = m_PostProcessChain.ExecuteChain(dt, currentRT);

			return;
		}
	}

	const bool wasDefaultState = (m_LastBoundScene == nullptr && m_LastPostProcessEntity == entt::null);
	if (!wasDefaultState)
	{
		m_LastBoundScene = nullptr;
		m_LastPostProcessEntity = entt::null;
		m_LastPostProcessPreset.clear();
		m_LastPostProcessSettings.clear();
		if (!m_DefaultPostProcessSettings.empty())
			m_PostProcessStateUsable = m_PostProcessChain.GetPipeline().LoadFromString(m_DefaultPostProcessSettings);
	}

	if (m_PostProcessChain.TakeSceneRestoreRequest())
	{
		m_PostProcessChain.CompleteSceneRestore(false);
		NotificationManager::Get().AddNotification(LOCAL("PostProcess.MenuTitle"), LOCAL("PostProcess.RestoreUnavailable"), ELogLevel::Warn);
	}

	// 씬에 활성화된 PostProcessComponent가 없더라도, 선형 HDR 버퍼의 안전한 변환을 위해 기본 톤매핑 체인을 실행
	if (m_PostProcessStateUsable)
		currentRT = m_PostProcessChain.ExecuteChain(dt, currentRT);
}
void SceneViewportPanel::SubmitUIOverlayPass(const wstring& currentRT)
{
	RenderPassManager::Get().GetRenderPassByID(m_UIOverlayPassID)->SetColorAttachments({ currentRT });
}
#pragma endregion

#pragma region Inputs
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
	if (TerrainEditor::Get() && TerrainEditor::Get()->IsSculptingActive()) return;
	if (!imageHovered || !InputManager::Get().IsMouseButtonDown(EMouseButton::Left) || !ImGui::IsWindowHovered() ||
		ImGui::IsAnyItemActive() || ImGuizmo::IsOver() || ImGuizmo::IsUsing() ||
		ImViewGuizmo::IsOver() || ImViewGuizmo::IsUsing() ||
		InputManager::Get().IsMouseButtonPressed(EMouseButton::Right) || SelectionManager::Get().IsPrimarySelectionLocked()) return;
	const auto screenMouse = InputManager::Get().GetScreenMousePosition();
	const ImVec2 mouse(screenMouse.x, screenMouse.y);
	vec2 uv;
	if (!PickingManager::TryGetViewportUV(vec2(mouse.x, mouse.y), vec2(m_ImageScreenPos.x, m_ImageScreenPos.y),
		vec2(m_ImageSize.x, m_ImageSize.y), uv)) return;
	if (Entity* overlayEntity = PickOverlayEntity(mouse))
	{
		m_PickingRequest.pending = false;
		auto& selection = SelectionManager::Get();
		if (InputManager::Get().IsCtrlPressed())
			selection.ToggleEntitySelection(overlayEntity);
		else
			selection.SetSelectedEntity(overlayEntity);
		return;
	}
	auto* target = RenderTargetManager::Get().GetRenderTarget(m_Name + L"_GBuffer_Position");
	if (!target || !target->GetTexture()) return;
	PickingRequest request;
	request.toggle = InputManager::Get().IsCtrlPressed();
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
			EDITOR_LOG_ERROR("Scene viewport ID picking: pixel download failed.");
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
	const std::optional<CameraBuffer> cameraBuffer = GetActiveCameraBuffer();
	if (!cameraBuffer.has_value()) return false;
	vec2 uv;
	if (!PickingManager::TryGetViewportUV(vec2(mousePos.x, mousePos.y), vec2(m_ImageScreenPos.x, m_ImageScreenPos.y),
		vec2(m_ImageSize.x, m_ImageSize.y), uv)) return false;
	return PickingManager::TryBuildRay(cameraBuffer->invViewProjMatrix, uv, ray, &maxDistance);
}
Entity* SceneViewportPanel::PickOverlayEntity(const ImVec2& mousePos) const
{
	if ((!m_ShowLightOverlay && !m_ShowCameraOverlay) || !m_CameraController) return nullptr;
	const std::optional<CameraBuffer> cameraBuffer = const_cast<SceneViewportPanel*>(this)->GetActiveCameraBuffer();
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
			if (!light.IsEnabled || !IsSceneOverlayVisible(flag))
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
			if (!IsSceneOverlayVisible(flag))
				continue;
			testOverlayIcon(entity, tr.worldMatrix, ICON_FA_CAMERA, 1.7f);
		}
	}
	return closestEntity;
}
#pragma endregion

std::optional<CameraBuffer> SceneViewportPanel::GetActiveCameraBuffer()
{
	return m_CameraController ? m_CameraController->GetCameraBuffer() : std::nullopt;
}

void SceneViewportPanel::HandleTerrainSculpting(bool imageHovered)
{
	auto* terrainEditor = TerrainEditor::Get();
	const bool canSculpt = terrainEditor && terrainEditor->IsSculptingActive() && imageHovered;

	Scene* activeScene = SceneManager::Get().GetCurrentScene();
	Entity* targetEntity = terrainEditor ? terrainEditor->GetTargetTerrainEntity() : nullptr;

	// 1. 마우스 좌클릭을 떼었을 때 (Stroke 종료): Undo/Redo 히스토리에 단일 작업으로 등록
	if (m_IsSculptStrokeActive && !InputManager::Get().IsMouseButtonPressed(EMouseButton::Left))
	{
		if (m_SculptModified && activeScene && targetEntity && targetEntity->GetScene() == activeScene)
		{
			const std::array targets{ targetEntity->GetEntityHandle() };
			SceneEditSnapshot after = SceneEditSnapshot::Capture(*activeScene, targets);
			Application::Get().GetEditHistory().Record(m_SculptBeforeSnapshot, after, LOCAL("TerrainEditor.Sculpt"));
		}
		m_IsSculptStrokeActive = false;
		m_SculptModified = false;
		m_SculptBeforeSnapshot = {};
	}

	if (!canSculpt || !activeScene || !targetEntity || targetEntity->GetScene() != activeScene)
	{
		return;
	}

	auto& registry = activeScene->GetRegistry();
	auto* terrain = registry.try_get<Engine::TerrainComponent>(targetEntity->GetEntityHandle());
	auto* transform = registry.try_get<Engine::WorldTransformComponent>(targetEntity->GetEntityHandle());
	if (!terrain || !transform) return;

	Engine::Ray ray;
	float maxDist = 1000.0f;
	const auto screenMouse = InputManager::Get().GetScreenMousePosition();
	const ImVec2 mousePos(screenMouse.x, screenMouse.y);
	if (!ScreenPosToRay(mousePos, ray, maxDist)) return;

	Engine::HitResult hitResult;
	if (!Engine::Collision::Raycast(ray, *terrain, transform->worldMatrix, hitResult, maxDist))
		return;

	// 2. 브러시 영향 범위 원형 기즈모 그리기 (32각 와이어프레임 링)
	const std::optional<CameraBuffer> cameraBuffer = GetActiveCameraBuffer();
	if (cameraBuffer.has_value())
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		const float radius = terrainEditor->GetBrushParams().Radius;
		const int segments = 32;
		ImVec2 prevScreenPos{};
		bool hasPrev = false;

		for (int i = 0; i <= segments; ++i)
		{
			const float angle = (static_cast<float>(i) / static_cast<float>(segments)) * 6.2831853f;
			const glm::vec3 circleWorldPos = hitResult.point + glm::vec3(std::cos(angle) * radius, 0.0f, std::sin(angle) * radius);
			ImVec2 screenPos{};
			if (WorldToViewportScreen(circleWorldPos, cameraBuffer->viewProjMatrix, m_ImageScreenPos, m_ImageSize, screenPos))
			{
				if (hasPrev) drawList->AddLine(prevScreenPos, screenPos, IM_COL32(50, 220, 100, 255), 2.0f);
				prevScreenPos = screenPos;
				hasPrev = true;
			}
			else hasPrev = false;
		}
	}

	// 3. 마우스 좌클릭 시작: 변경 전 스냅샷 캡처
	if (InputManager::Get().IsMouseButtonDown(EMouseButton::Left) && !ImGuizmo::IsUsing() && !InputManager::Get().IsMouseButtonPressed(EMouseButton::Right))
	{
		const std::array targets{ targetEntity->GetEntityHandle() };
		m_SculptBeforeSnapshot = SceneEditSnapshot::Capture(*activeScene, targets);
		m_IsSculptStrokeActive = true;
		m_SculptModified = false;
	}

	// 4. 마우스 좌클릭 드래그 중: 실시간 지형 변형
	if (m_IsSculptStrokeActive && InputManager::Get().IsMouseButtonPressed(EMouseButton::Left) && !ImGuizmo::IsUsing())
	{
		const float dt = TimeManager::Get().GetDeltaTime();
		if (Engine::TerrainUtility::ApplyBrush(*terrain, hitResult.point, transform->worldMatrix, terrainEditor->GetBrushParams(), dt))
		{
			m_SculptModified = true;
		}
	}
}


bool SceneViewportPanel::HandleTilemapPainting(bool imageHovered)
{
	auto* palette = dynamic_cast<TilePalettePanel*>(ImGuiManager::Get().GetImGuiPanel(L"Tile Palette"));
	if (!palette || !palette->IsOpen())
	{
		m_IsTilemapPaintingActive = false;
		m_IsTileBoxFillDragging = false;
		return false;
	}

	Scene* activeScene = SceneManager::Get().GetCurrentScene();
	if (!activeScene) return false;

	Entity* targetEntity = SelectionManager::Get().GetPrimarySelectedEntity();
	auto& registry = activeScene->GetRegistry();

	Engine::TilemapComponent* tilemap = nullptr;
	Engine::WorldTransformComponent* worldTransform = nullptr;

	if (targetEntity && targetEntity->GetScene() == activeScene)
	{
		tilemap = registry.try_get<Engine::TilemapComponent>(targetEntity->GetEntityHandle());
		worldTransform = registry.try_get<Engine::WorldTransformComponent>(targetEntity->GetEntityHandle());
	}

	if (!tilemap || !worldTransform)
	{
		auto view = registry.view<Engine::TilemapComponent, Engine::WorldTransformComponent, Engine::FlagComponent>();
		for (auto [ent, tm, wt, flag] : view.each())
		{
			if (flag.IsActiveInHierarchy() && !HasFlag(flag.effectiveFlags, Engine::EEntityFlag::Dead))
			{
				tilemap = &tm;
				worldTransform = &wt;
				break;
			}
		}
	}

	if (!tilemap || !worldTransform)
	{
		m_IsTilemapPaintingActive = false;
		m_IsTileBoxFillDragging = false;
		return false;
	}

	if (imageHovered && !ImGui::GetIO().WantTextInput)
	{
		if (ImGui::IsKeyPressed(ImGuiKey_B, false)) palette->SetBrushMode(ETileBrushMode::Paint);
		else if (ImGui::IsKeyPressed(ImGuiKey_E, false)) palette->SetBrushMode(ETileBrushMode::Erase);
		else if (ImGui::IsKeyPressed(ImGuiKey_U, false)) palette->SetBrushMode(ETileBrushMode::BoxFill);
		else if (ImGui::IsKeyPressed(ImGuiKey_I, false)) palette->SetBrushMode(ETileBrushMode::Picker);
	}

	Engine::Ray ray;
	float maxDist = 2000.0f;
	const auto screenMouse = InputManager::Get().GetScreenMousePosition();
	const ImVec2 mousePos(screenMouse.x, screenMouse.y);
	if (!ScreenPosToRay(mousePos, ray, maxDist)) return false;

	const mat4& worldMatrix = worldTransform->worldMatrix;
	const vec3 planeNormal = glm::normalize(vec3(worldMatrix[2]));
	const vec3 planePoint = vec3(worldMatrix[3]);

	const float denom = glm::dot(ray.direction, planeNormal);
	if (std::abs(denom) < 1e-5f) return false;

	const float t = glm::dot(planePoint - ray.origin, planeNormal) / denom;
	if (t < 0.0f || t > maxDist) return false;

	const vec3 hitWorldPos = ray.origin + ray.direction * t;
	const mat4 invWorld = glm::inverse(worldMatrix);
	const vec4 localHit = invWorld * vec4(hitWorldPos, 1.0f);

	const vec2 cellCoord = tilemap->WorldToCell(vec2(localHit.x, localHit.y));
	const int32 cellX = static_cast<int32>(cellCoord.x);
	const int32 cellY = static_cast<int32>(cellCoord.y);

	const std::optional<CameraBuffer> cameraBuffer = GetActiveCameraBuffer();
	if (!cameraBuffer.has_value()) return false;

	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const ETileBrushMode brushMode = palette->GetBrushMode();
	const auto selectedSprite = palette->GetSelectedSprite();

	auto getCellScreenQuad = [&](int32 cx, int32 cy, ImVec2 outScreen[4]) -> bool
	{
		const vec2 cellSize = tilemap->cellSize;
		const vec2 minLocal = tilemap->CellToWorld(cx, cy);
		const vec2 maxLocal = minLocal + cellSize;

		const vec3 corners[4] = {
			vec3(worldMatrix * vec4(minLocal.x, minLocal.y, 0.0f, 1.0f)),
			vec3(worldMatrix * vec4(maxLocal.x, minLocal.y, 0.0f, 1.0f)),
			vec3(worldMatrix * vec4(maxLocal.x, maxLocal.y, 0.0f, 1.0f)),
			vec3(worldMatrix * vec4(minLocal.x, maxLocal.y, 0.0f, 1.0f))
		};

		for (int i = 0; i < 4; ++i)
		{
			if (!WorldToViewportScreen(corners[i], cameraBuffer->viewProjMatrix, m_ImageScreenPos, m_ImageSize, outScreen[i]))
				return false;
		}
		return true;
	};

	if (imageHovered)
	{
		ImU32 highlightColor = IM_COL32(255, 220, 50, 220);
		if (brushMode == ETileBrushMode::Erase) highlightColor = IM_COL32(255, 60, 60, 220);
		else if (brushMode == ETileBrushMode::BoxFill) highlightColor = IM_COL32(50, 220, 255, 220);
		else if (brushMode == ETileBrushMode::Picker) highlightColor = IM_COL32(200, 100, 255, 220);

		if (m_IsTileBoxFillDragging && brushMode == ETileBrushMode::BoxFill)
		{
			const int32 startX = static_cast<int32>(m_TileBoxFillStartCell.x);
			const int32 startY = static_cast<int32>(m_TileBoxFillStartCell.y);
			const int32 x0 = std::min(startX, cellX);
			const int32 x1 = std::max(startX, cellX);
			const int32 y0 = std::min(startY, cellY);
			const int32 y1 = std::max(startY, cellY);

			for (int32 y = y0; y <= y1; ++y)
			{
				for (int32 x = x0; x <= x1; ++x)
				{
					ImVec2 quad[4];
					if (getCellScreenQuad(x, y, quad))
					{
						drawList->AddPolyline(quad, 4, highlightColor, ImDrawFlags_Closed, 1.5f);
					}
				}
			}
		}
		else
		{
			ImVec2 quad[4];
			if (getCellScreenQuad(cellX, cellY, quad))
			{
				if (brushMode == ETileBrushMode::Paint && selectedSprite.IsValid() && selectedSprite.Get())
				{
					auto* sp = selectedSprite.Get();
					if (auto* tex = sp->GetTexture(); tex && tex->GetRHITexture())
					{
						const ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(tex->GetRHITexture());
						const vec4 uv = sp->GetUVTransform();
						drawList->AddImageQuad(texID, quad[0], quad[1], quad[2], quad[3],
							ImVec2(uv.x, uv.y + uv.w), ImVec2(uv.x + uv.z, uv.y + uv.w),
							ImVec2(uv.x + uv.z, uv.y), ImVec2(uv.x, uv.y),
							IM_COL32(255, 255, 255, 140));
					}
				}
				drawList->AddPolyline(quad, 4, highlightColor, ImDrawFlags_Closed, 2.0f);
			}
		}
	}

	const bool leftDown = InputManager::Get().IsMouseButtonDown(EMouseButton::Left);
	const bool leftPressed = InputManager::Get().IsMouseButtonPressed(EMouseButton::Left);
	const bool rightPressed = InputManager::Get().IsMouseButtonPressed(EMouseButton::Right);

	if (rightPressed) return false;

	if (imageHovered && leftDown)
	{
		m_IsTilemapPaintingActive = true;
		if (brushMode == ETileBrushMode::BoxFill)
		{
			m_TileBoxFillStartCell = vec2(static_cast<f32>(cellX), static_cast<f32>(cellY));
			m_IsTileBoxFillDragging = true;
		}
	}

	if (m_IsTilemapPaintingActive && leftPressed && imageHovered)
	{
		if (brushMode == ETileBrushMode::Paint)
		{
			if (selectedSprite.IsValid())
			{
				tilemap->SetTile(cellX, cellY, selectedSprite);
				m_TilemapStrokeModified = true;
			}
		}
		else if (brushMode == ETileBrushMode::Erase)
		{
			tilemap->ClearTile(cellX, cellY);
			m_TilemapStrokeModified = true;
		}
		else if (brushMode == ETileBrushMode::Picker)
		{
			const TileCell* cell = tilemap->GetTile(cellX, cellY);
			if (cell && cell->sprite.IsValid())
			{
				palette->SetSelectedSprite(cell->sprite);
				palette->SetBrushMode(ETileBrushMode::Paint);
			}
		}
	}

	if (m_IsTilemapPaintingActive && !leftPressed)
	{
		if (m_IsTileBoxFillDragging && brushMode == ETileBrushMode::BoxFill && selectedSprite.IsValid())
		{
			const int32 startX = static_cast<int32>(m_TileBoxFillStartCell.x);
			const int32 startY = static_cast<int32>(m_TileBoxFillStartCell.y);
			const int32 x0 = std::min(startX, cellX);
			const int32 x1 = std::max(startX, cellX);
			const int32 y0 = std::min(startY, cellY);
			const int32 y1 = std::max(startY, cellY);

			for (int32 y = y0; y <= y1; ++y)
			{
				for (int32 x = x0; x <= x1; ++x)
				{
					tilemap->SetTile(x, y, selectedSprite);
				}
			}
			m_TilemapStrokeModified = true;
			m_IsTileBoxFillDragging = false;
		}

		if (m_TilemapStrokeModified)
		{
			tilemap->SyncSavedTiles();
			m_TilemapStrokeModified = false;
		}

		m_IsTilemapPaintingActive = false;
	}

	return imageHovered && (m_IsTilemapPaintingActive || (brushMode != ETileBrushMode::Picker && selectedSprite.IsValid()));
}

void SceneViewportPanel::FocusSelectedEntity()
{
	if (m_CameraController)
		m_CameraController->FocusEntity(SelectionManager::Get().GetPrimarySelectedEntity());
}

#pragma region Spline
void SceneViewportPanel::DrawSelectedSpline(Engine::Scene* scene)
{
	auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();
	if (!scene || !selected || selected->GetScene() != scene)
		return;

	auto& world = scene->GetRegistry();
	const auto entity = selected->GetEntityHandle();
	if (!world.valid(entity))
		return;

	const auto* spline = world.try_get<Engine::SplineComponent>(entity);
	if (!spline || !spline->IsValid())
		return;

	mat4 worldMatrix;
	if (!Engine::TransformSystem::TryGetWorldMatrix(world, entity, worldMatrix))
		return;

	if (!m_SplinePreviewCached || m_SplinePreviewClosed != spline->Closed ||
		m_SplinePreviewSource != spline->Points)
	{
		m_SplinePreviewSource = spline->Points;
		m_SplinePreviewClosed = spline->Closed;
		m_SplinePreviewCached = true;
		m_SplinePreviewSamples.clear();

		std::vector<vec3> points;
		points.reserve(spline->Points.size());
		for (const auto& point : spline->Points)
			points.push_back(point.Position);

		if (points.size() == 1)
		{
			m_SplinePreviewSamples.push_back(points.front());
		}
		else
		{
			const size_t segmentCount = spline->Closed ? points.size() : points.size() - 1;
			const size_t samplesPerSegment = std::min(size_t{ 16 }, MaxSplinePreviewSegments / segmentCount);
			const size_t sampleCount = segmentCount * samplesPerSegment;

			std::vector<vec3> samples(sampleCount + 1);
			for (size_t i = 0; i <= sampleCount; ++i)
			{
				Engine::SplineSample sample;
				const f64 parameter = static_cast<f64>(i) / static_cast<f64>(sampleCount);
				if (!Engine::SplineCurve::Evaluate(points, spline->Closed, parameter, sample))
					return;

				samples[i] = sample.Position;
			}

			m_SplinePreviewSamples = std::move(samples);
		}
	}

	if (m_SplinePreviewSamples.empty())
		return;

	const glm::dmat4 transform(worldMatrix);
	const auto toWorld = [&transform](const vec3& local, vec3& result) -> bool
		{
			return TryTransformSplinePoint(transform, local, result);
		};

	std::array<vec3, MaxSplinePreviewSegments + 1> curvePositions{};
	std::array<vec3, Engine::SplineComponent::MaxControlPoints> controlPositions{};

	for (size_t i = 0; i < m_SplinePreviewSamples.size(); ++i)
	{
		if (!toWorld(m_SplinePreviewSamples[i], curvePositions[i]))
			return;
	}

	for (size_t i = 0; i < spline->Points.size(); ++i)
	{
		if (!toWorld(spline->Points[i].Position, controlPositions[i]))
			return;
	}

	const vec4 curveColor(0.1f, 0.9f, 1.0f, 1.0f);
	const vec4 pointColor(1.0f, 0.75f, 0.15f, 1.0f);

	for (size_t i = 1; i < m_SplinePreviewSamples.size(); ++i)
		m_DebugRenderer.DrawLine(curvePositions[i - 1], curvePositions[i], curveColor);

	constexpr float markerSize = 0.08f;
	for (size_t i = 0; i < spline->Points.size(); ++i)
	{
		const auto& position = controlPositions[i];
		m_DebugRenderer.DrawLine(position - vec3(markerSize, 0.0f, 0.0f), position + vec3(markerSize, 0.0f, 0.0f), pointColor);
		m_DebugRenderer.DrawLine(position - vec3(0.0f, markerSize, 0.0f), position + vec3(0.0f, markerSize, 0.0f), pointColor);
		m_DebugRenderer.DrawLine(position - vec3(0.0f, 0.0f, markerSize), position + vec3(0.0f, 0.0f, markerSize), pointColor);
	}
}

void SceneViewportPanel::StopSplinePointDrag()
{
	if (!m_SplinePointWasUsing)
		return;

	Application::Get().GetEditHistory().Flush();

	// 외부 편집으로 중단된 ImGuizmo 드래그 상태도 종료합니다.
	ImGuizmo::Enable(false);
	ImGuizmo::Enable(true);

	m_SplinePointWasUsing = false;
	m_SplinePointBlockUntilRelease = InputManager::Get().IsMouseButtonPressed(EMouseButton::Left);
}

void SceneViewportPanel::ResetSplinePointSelection()
{
	StopSplinePointDrag();

	m_SplinePointScene = nullptr;
	m_SplinePointEntity = entt::null;
	m_SplinePointOwnerId = 0;
	m_SplinePointRevision = 0;
	m_SplinePointIndex = -1;
	m_SplinePointExpected.clear();
}

void SceneViewportPanel::SelectSplinePoint(Engine::Scene* scene, entt::entity entity, int index)
{
	auto& history = Application::Get().GetEditHistory();
	auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();

	if (!PIEManager::Get().IsEditing() || !history.Enabled() || !scene || !selected ||
		selected->GetScene() != scene || selected->GetEntityHandle() != entity)
		return;

	auto& world = scene->GetRegistry();
	if (!world.valid(entity))
		return;

	const auto* spline = world.try_get<Engine::SplineComponent>(entity);
	const auto* identity = world.try_get<Engine::IDComponent>(entity);

	if (!spline || !identity || spline->Points.size() > Engine::SplineComponent::MaxControlPoints)
		return;

	StopSplinePointDrag();

	m_SplinePointScene = scene;
	m_SplinePointEntity = entity;
	m_SplinePointOwnerId = identity->id;
	m_SplinePointRevision = history.Revision();
	m_SplinePointExpected = spline->Points;
	m_SplinePointIndex = index >= 0 && static_cast<size_t>(index) < spline->Points.size() ? index : -1;

	if (m_SplinePointIndex >= 0 &&
		m_GizmoOperation != ImGuizmo::TRANSLATE && m_GizmoOperation != ImGuizmo::ROTATE)
		m_GizmoOperation = ImGuizmo::TRANSLATE;
}

bool SceneViewportPanel::DrawSplinePointEditor(bool imageHovered)
{
	auto& input = InputManager::Get();
	auto& history = Application::Get().GetEditHistory();

	if (!input.IsMouseButtonPressed(EMouseButton::Left))
		m_SplinePointBlockUntilRelease = false;

	auto* scene = SceneManager::Get().GetCurrentScene();
	auto* selected = SelectionManager::Get().GetPrimarySelectedEntity();

	if (!PIEManager::Get().IsEditing() || !history.Enabled() ||
		!scene || !selected || selected->GetScene() != scene ||
		(TerrainEditor::Get() && TerrainEditor::Get()->IsSculptingActive()))
	{
		ResetSplinePointSelection();
		return m_SplinePointBlockUntilRelease;
	}

	auto& world = scene->GetRegistry();
	const auto entity = selected->GetEntityHandle();
	auto* spline = world.valid(entity) ? world.try_get<Engine::SplineComponent>(entity) : nullptr;
	const auto* identity = world.valid(entity) ? world.try_get<Engine::IDComponent>(entity) : nullptr;

	if (!spline || !identity || spline->Points.empty() ||
		spline->Points.size() > Engine::SplineComponent::MaxControlPoints)
	{
		ResetSplinePointSelection();
		return m_SplinePointBlockUntilRelease;
	}

	const bool ownerChanged = m_SplinePointScene != scene || m_SplinePointEntity != entity ||
		m_SplinePointOwnerId != identity->id;

	if (ownerChanged)
	{
		ResetSplinePointSelection();

		m_SplinePointScene = scene;
		m_SplinePointEntity = entity;
		m_SplinePointOwnerId = identity->id;
		m_SplinePointRevision = history.Revision();
		m_SplinePointExpected = spline->Points;
	}
	else if (m_SplinePointRevision != history.Revision() || m_SplinePointExpected != spline->Points)
	{
		StopSplinePointDrag();

		// 값 편집은 같은 인덱스를 유지하고, 추가·삭제는 선택을 해제합니다.
		if (m_SplinePointExpected.size() != spline->Points.size() ||
			(m_SplinePointIndex >= 0 && static_cast<size_t>(m_SplinePointIndex) >= spline->Points.size()))
			m_SplinePointIndex = -1;

		m_SplinePointRevision = history.Revision();
		m_SplinePointExpected = spline->Points;
	}

	if (m_SplinePointBlockUntilRelease)
		return true;

	const auto camera = GetActiveCameraBuffer();
	mat4 ownerWorld;
	if (!camera || !TransformSystem::TryGetWorldMatrix(world, entity, ownerWorld))
	{
		ResetSplinePointSelection();
		return m_SplinePointBlockUntilRelease;
	}

	const glm::dmat4 ownerMatrix(ownerWorld);
	std::array<vec3, Engine::SplineComponent::MaxControlPoints> worldPoints{};
	std::array<ImVec2, Engine::SplineComponent::MaxControlPoints> screenPoints{};
	std::array<bool, Engine::SplineComponent::MaxControlPoints> visible{};

	const auto mouse = input.GetScreenMousePosition();
	const float radius = std::max(5.0f, ImGui::GetFontSize() * 0.35f);
	float nearestDistance = radius * radius * 2.25f;
	double nearestDepth = (std::numeric_limits<double>::max)();
	int hoveredPoint = -1;

	for (size_t i = 0; i < spline->Points.size(); ++i)
	{
		if (!TryTransformSplinePoint(ownerMatrix, spline->Points[i].Position, worldPoints[i]))
		{
			ResetSplinePointSelection();
			return m_SplinePointBlockUntilRelease;
		}

		visible[i] = WorldToViewportScreen(
			worldPoints[i], camera->viewProjMatrix, m_ImageScreenPos, m_ImageSize, screenPoints[i]);

		if (!visible[i])
			continue;

		const float dx = mouse.x - screenPoints[i].x;
		const float dy = mouse.y - screenPoints[i].y;
		const float distance = dx * dx + dy * dy;
		const double depth = glm::length(glm::dvec3(worldPoints[i]) - glm::dvec3(camera->cameraPosition));

		if (distance < nearestDistance ||
			(std::abs(distance - nearestDistance) < 0.01f && depth < nearestDepth))
		{
			nearestDistance = distance;
			nearestDepth = depth;
			hoveredPoint = static_cast<int>(i);
		}
	}

	auto* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(m_ImageScreenPos,
		ImVec2(m_ImageScreenPos.x + m_ImageSize.x, m_ImageScreenPos.y + m_ImageSize.y), true);

	for (size_t i = 0; i < spline->Points.size(); ++i)
	{
		if (!visible[i])
			continue;

		const bool selectedPoint = static_cast<int>(i) == m_SplinePointIndex;
		const ImU32 color = selectedPoint ? IM_COL32(255, 180, 40, 255) : IM_COL32(60, 210, 240, 255);
		drawList->AddCircleFilled(screenPoints[i], radius, color);
		drawList->AddCircle(screenPoints[i], radius, IM_COL32(20, 25, 30, 255), 0, 1.5f);
	}

	drawList->PopClipRect();

	const bool navigating = input.IsAltPressed() || input.IsMouseButtonPressed(EMouseButton::Right) ||
		input.IsMouseButtonPressed(EMouseButton::Middle) || ImViewGuizmo::IsOver() || ImViewGuizmo::IsUsing();

	bool gizmoOver = false;
	bool usingNow = false;
	const double determinant = glm::determinant(ownerMatrix);
	const bool invertible = std::isfinite(determinant) && determinant != 0.0;

	const bool translating = m_GizmoOperation == ImGuizmo::TRANSLATE;
	const bool rotating = m_GizmoOperation == ImGuizmo::ROTATE;

	if (m_SplinePointWasUsing &&
		(m_SplinePointDragOperation != m_GizmoOperation || m_SplinePointDragOwnerWorld != ownerWorld ||
			navigating || ImGui::GetIO().WantTextInput))
		StopSplinePointDrag();

	if (m_SplinePointBlockUntilRelease)
		return true;

	if (m_SplinePointIndex >= 0 && (translating || rotating))
	{
		const size_t index = static_cast<size_t>(m_SplinePointIndex);
		mat4 pointWorld = ownerWorld;
		pointWorld[3] = vec4(worldPoints[index], 1.0f);

		const bool ready = rotating ?
			TryBuildSplineRollGizmo(*spline, index, ownerWorld, worldPoints[index], pointWorld) : invertible;

		if (ready)
		{
			ImGui::PushID(this);
			ImGui::PushID(static_cast<const void*>(scene));
			ImGui::PushID(std::to_string(identity->id).c_str());
			ImGui::PushID(m_SplinePointIndex);

			ImGuizmo::SetID(static_cast<int>(ImGui::GetID("SplinePointGizmo")));
			ImGuizmo::SetOrthographic(IsOrthographic());
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(m_ImageScreenPos.x, m_ImageScreenPos.y, m_ImageSize.x, m_ImageSize.y);

			// 다른 뷰포트나 다른 대상의 진행 중 드래그를 중단하지 않습니다.
			const bool anotherGizmoUsing = ImGuizmo::IsUsingAny() && !ImGuizmo::IsUsing();

			if (!anotherGizmoUsing)
			{
				const auto& preferences = EditorSettingsService::Get().GetPreferences();
				const float snapStep = rotating ? preferences.GizmoSnapRotation : preferences.GizmoSnapTranslation;
				const vec3 snap(snapStep);
				const bool useSnap = (m_GizmoUseSnap || input.IsCtrlPressed()) &&
					std::isfinite(snapStep) && snapStep > 0.0f;

				const bool allowInput = !navigating && !ImGui::GetIO().WantTextInput &&
					(imageHovered || m_SplinePointWasUsing);
				const mat4 beforeMatrix = pointWorld;

				// Inspector 편집 중에도 표시하되, 뷰포트 밖에서는 조작하지 않습니다.
				ImGuizmo::Enable(allowInput);
				const bool changed = ImGuizmo::Manipulate(
					glm::value_ptr(camera->viewMatrix), glm::value_ptr(camera->projMatrix),
					rotating ? ImGuizmo::ROTATE_Z : ImGuizmo::TRANSLATE,
					rotating ? ImGuizmo::LOCAL : m_GizmoMode, glm::value_ptr(pointWorld), nullptr,
					useSnap ? glm::value_ptr(snap) : nullptr);

				gizmoOver = allowInput && ImGuizmo::IsOver();
				usingNow = allowInput && ImGuizmo::IsUsing() &&
					input.IsMouseButtonPressed(EMouseButton::Left);
				ImGuizmo::Enable(true);

				if (usingNow && !m_SplinePointWasUsing)
				{
					history.Flush();
					m_SplinePointDragOperation = m_GizmoOperation;
					m_SplinePointDragOwnerWorld = ownerWorld;
				}

				if (changed)
				{
					const auto previous = spline->Points[index];
					auto candidate = previous;
					bool valid = true;

					if (rotating)
					{
						const glm::dvec3 beforeRight(beforeMatrix[0]);
						const glm::dvec3 afterRight(pointWorld[0]);
						const glm::dvec3 axis(beforeMatrix[2]);

						const double sine = glm::dot(axis, glm::cross(beforeRight, afterRight));
						const double cosine = glm::dot(beforeRight, afterRight);
						const double delta = glm::degrees(std::atan2(sine, cosine));
						const double roll = static_cast<double>(previous.RollDegrees) + delta;
						const double limit = static_cast<double>((std::numeric_limits<float>::max)());

						valid = std::isfinite(roll) && std::abs(roll) <= limit;
						if (valid)
							candidate.RollDegrees = static_cast<f32>(roll);
					}
					else
					{
						valid = TryTransformSplinePoint(
							glm::inverse(ownerMatrix), vec3(pointWorld[3]), candidate.Position);
					}

					bool applied = false;
					if (valid && candidate != previous)
					{
						const std::array targets{ entity };
						auto before = SceneEditSnapshot::Capture(*scene, targets);

						if (before)
						{
							spline->Points[index] = candidate;
							auto after = spline->IsValid() ?
								SceneEditSnapshot::Capture(*scene, targets) : SceneEditSnapshot{};

							if (after)
							{
								const char* label = rotating ?
									"History.Command.RotateSplinePoint" : "History.Command.MoveSplinePoint";
								history.Record(std::move(before), std::move(after), label, true);
								m_SplinePointExpected = spline->Points;
								m_SplinePointRevision = history.Revision();
								applied = true;

								auto& prefab = PrefabModeManager::Get();
								if (prefab.IsInPrefabMode() && prefab.GetPrefabScene() == scene)
									prefab.MarkDirty();
							}
							else
							{
								spline->Points[index] = previous;
							}
						}
					}

					if (!valid || (candidate != previous && !applied))
						ImGui::SetTooltip("%s", LOCAL_CSTR("Spline.PointEditRejected"));
				}
			}

			ImGui::PopID();
			ImGui::PopID();
			ImGui::PopID();
			ImGui::PopID();
		}
		else if (rotating && imageHovered && hoveredPoint == m_SplinePointIndex)
		{
			ImGui::SetTooltip("%s", LOCAL_CSTR("Spline.RollUnavailable"));
		}
	}
	else if (m_SplinePointWasUsing)
	{
		StopSplinePointDrag();
	}

	if (m_SplinePointWasUsing && !usingNow)
		history.Flush();

	m_SplinePointWasUsing = usingNow;

	if (imageHovered && hoveredPoint >= 0 && !usingNow)
		ImGui::SetTooltip("%s", LOCAL_FMT("Spline.PointHint", hoveredPoint).c_str());

	const bool canSelect = imageHovered && ImGui::IsWindowHovered() &&
		!ImGui::IsAnyItemActive() && !ImGui::GetIO().WantTextInput && !navigating &&
		!usingNow && !gizmoOver && !ImGuizmo::IsUsingAny();

	if (canSelect && input.IsMouseButtonDown(EMouseButton::Left))
	{
		m_SplinePointIndex = hoveredPoint;
		if (hoveredPoint >= 0)
		{
			if (m_GizmoOperation != ImGuizmo::TRANSLATE && m_GizmoOperation != ImGuizmo::ROTATE)
				m_GizmoOperation = ImGuizmo::TRANSLATE;
			m_PickingRequest.pending = false;
		}
	}

	return m_SplinePointIndex >= 0;
}

#pragma endregion

#pragma region Camera
void SceneViewportPanel::RequestProjectionMode(bool perspective)
{
	if (!m_CameraController || perspective == IsPerspective())
		return;

	auto* transform = m_CameraController->GetTransformComponent();
	auto* camera = m_CameraController->GetCameraComponent();
	if (!transform || !camera)
		return;

	auto next = *transform;
	const vec3 pivot = m_CameraController->GetPivotPoint();

	if (perspective)
	{
		const float defaultPitch = glm::degrees(std::atan2(4.0f, 8.0f));
		const PerspectiveView view = m_LastPerspectiveView.value_or(PerspectiveView{
			vec3(0.0f, 4.0f, -8.0f), vec3(defaultPitch, 0.0f, 0.0f), vec3(0.0f, 0.5f, 0.0f)
			});

		if (!next.SetLocalPosition(view.Position) || !next.SetEulerRotation(view.Rotation))
			return;

		if (m_LastPerspectiveView)
			m_LastXYPosition = transform->position;

		m_CameraController->SetPivotPoint(view.Pivot);
	}
	else
	{
		const vec3 position = m_LastXYPosition.value_or(vec3(pivot.x, pivot.y, -10.0f));
		if (!next.SetLocalPosition(position) || !next.SetEulerRotation(vec3(0.0f)))
			return;

		m_LastPerspectiveView = PerspectiveView{ transform->position, transform->eulerRotation, pivot };
		if (!m_LastXYPosition)
			camera->OrthographicSize = 10.0f;
	}

	*transform = next;
	m_CameraController->SyncControlRotationFromTransform();
	SetPerspective(perspective);
}
#pragma endregion
