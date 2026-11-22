#include "InputManager.h"
#include "ComponentEnable.h"
#include "EditorUI.h"
#pragma push_macro("COLOR")
#undef COLOR
#include "ImGuizmo.h"
#pragma pop_macro("COLOR")
#include <cmath>
#include "PreviewViewportWidget.h"
#include "EditorCameraController.h"
#include "PreviewCameraDefaults.h"
#include "Scene.h"
#include "Entity.h"
#include "ResourceStreamer.h"
#include "Mesh.h"
#include "Renderer.h"
#include "RenderTargetManager.h"
#include "RenderTarget.h"
#include "ImGuiManager.h"
#include "TransformSystem.h"
#include "LightSystem.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "NameComponent.h"
#include "StaticMeshRendererComponent.h"
#include "SkinnedMeshRendererComponent.h"
#include "AnimatorComponent.h"
#include "LightComponent.h"
#include "IconsFontAwesome7.h"
#include "AnimationEvaluator.h"
#include "Skeleton.h"
#include "HierarchyComponent.h"
#include "ShortcutManager.h"
#include "EditorSettingsService.h"
#include "Functions.h"
#include "AnimationSceneQuery.h"
#include "AnimatorPoseUpdate.h"
#include "ParticleSystem.h"
#include "CompositeVFXSystem.h"
#include <vector>
#include "ViewportImage.h"
#include "GizmoHelper.h"

namespace
{
	entt::entity FindFirstEnabledAnimator(const entt::registry& world)
	{
		for (const auto entity : world.view<Engine::AnimatorComponent>())
			if (Engine::IsComponentEnabled<Engine::AnimatorComponent>(world, entity)) return entity;
		return entt::null;
	}

	// 오버레이 툴바 버튼은 글리프 폭과 무관하게 모두 같은 정사각형 슬롯을 사용한다.
	ImVec2 GetOverlayButtonSize()
	{
		constexpr const char* icons[] = { ICON_FA_LAYER_GROUP, ICON_FA_LIGHTBULB, ICON_FA_BORDER_ALL, ICON_FA_BORDER_NONE, ICON_FA_CROSSHAIRS, ICON_FA_PAUSE, ICON_FA_PLAY, ICON_FA_PERSON, ICON_FA_BONE };
		float side = 0.0f;
		for (const char* icon : icons)
		{
			const auto size = EditorUI::GetIconButtonSize(icon);
			side = std::max(side, std::max(size.x, size.y));
		}
		return ImVec2(std::ceil(side), std::ceil(side));
	}

	// 줄바꿈하지 않는다. 같은 줄 배치는 호출자가 ImGui::SameLine()으로 명시한다.
	EditorUI::IconButtonOptions MakeOverlayButtonOptions()
	{
		EditorUI::IconButtonOptions options;
		options.Size = GetOverlayButtonSize();
		options.NoWrap = true;
		return options;
	}
}

PreviewViewportWidget::~PreviewViewportWidget()
{
	Free();
}

void PreviewViewportWidget::Initialize(const std::wstring& widgetId, uint32 width, uint32 height, bool enableGrid)
{
	m_Id = widgetId;
	m_Width = std::max(width, 64u);
	m_Height = std::max(height, 64u);
	m_ShowGrid = enableGrid;

	// 1. 격리된 독립 프리뷰 씬 생성 (acceptSceneMeshes=false 로 메인 씬 유입 차단)
	Engine::SceneCreateDesc sceneDesc{};
	sceneDesc.name = m_Id + L"_Scene";
	m_PreviewScene = Engine::Scene::Create(&sceneDesc);

	// 2. 뷰포트 디퍼드 파이프라인 및 포스트프로세스 체인 초기화
	m_Pipeline.Initialize(m_Id, m_Width, m_Height, false);
	m_PostProcessChain.Initialize(m_Id + L"_PP");
	m_PostProcessChain.PrepareResources(m_Width, m_Height);
	m_ChannelFilter.Initialize(m_Id);
	m_DisplayRTName = m_Pipeline.GetFinalColorRTName();

	// 3. 바닥 그리드 초기화
	m_Grid.Initialize(m_Id + L"_Grid");
	if (m_ShowGrid)
	{
		m_Grid.Show();
	}

	s_ActiveWidgets.insert(this);

	// 4. 에디터 카메라 컨트롤러 초기화 (에디터 표준 조작계)
	float initialFov = Editor::PreviewCameraDefaults::FieldOfViewDegrees;
	float initialSpeed = 10.0f;
	if (EditorSettingsService::IsCreated())
	{
		const auto& prefs = EditorSettingsService::Get().GetPreferences();
		initialFov = prefs.CameraFOV;
		initialSpeed = prefs.CameraSpeed;
	}

	Engine::CameraComponent cameraSettings{};
	cameraSettings.CameraType = Engine::ECameraType::Perspective;
	cameraSettings.FOV = initialFov;
	cameraSettings.NearPlane = 0.1f;
	cameraSettings.FarPlane = 1000.0f;
	cameraSettings.AspectRatio = static_cast<f32>(m_Width) / static_cast<f32>(m_Height);

	m_CameraController = EditorCameraController::Create(m_Id + L"_Camera", cameraSettings);
	if (m_CameraController)
	{
		m_CameraController->SetCameraSpeed(initialSpeed);
		FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
	}

	// 5. 기본 스튜디오 조명 자동 구축
	SetupDefaultStudioLighting();
}

void PreviewViewportWidget::Free()
{
	s_ActiveWidgets.erase(this);
	if (s_ShortcutTarget == this)
		s_ShortcutTarget = nullptr;

	m_IsHovered = false;
	m_IsFocused = false;
	m_FocusModel = entt::null;
	m_ModelFocusPending = false;

	m_Grid.Free();
	m_PostProcessChain.Free();
	m_ChannelFilter.Free();
	m_SelectedRTName.clear();
	m_PreviewLightDraft.reset();
	m_Pipeline.Free();

	Engine::Safe_Release(m_CameraController);
	m_CameraController = nullptr;

	if (m_PreviewScene)
	{
		Engine::Safe_Release(m_PreviewScene);
		m_PreviewScene = nullptr;
	}
}

entt::registry& PreviewViewportWidget::GetRegistry()
{
	return m_PreviewScene->GetRegistry();
}

Engine::Entity& PreviewViewportWidget::CreateEntity(const std::wstring& name)
{
	auto& entity = m_PreviewScene->CreateEntity();
	auto& nameComp = m_PreviewScene->AddComponent<Engine::NameComponent>(entity);
	nameComp.name = name;
	m_PreviewScene->AddComponent<Engine::TransformComponent>(entity);
	m_PreviewScene->AddComponent<Engine::WorldTransformComponent>(entity);
	return entity;
}

void PreviewViewportWidget::SetupDefaultStudioLighting()
{
	if (!m_PreviewScene) return;

	auto& reg = m_PreviewScene->GetRegistry();
	auto createDirLight = [&reg](const Engine::vec3& rotEuler, const Engine::vec3& color, f32 intensity, bool specular)
		{
			auto entity = reg.create();
			auto& trans = reg.emplace<Engine::TransformComponent>(entity);
			reg.emplace<Engine::WorldTransformComponent>(entity);

			auto& light = reg.emplace<Engine::LightComponent>(entity);
			light.LightType = Engine::ELightType::Directional;
			light.Color = color;
			light.Intensity = intensity;
			light.Flags = Engine::ELightFlags::UseInDeferredRendering | Engine::ELightFlags::AffectDiffuse;
			if (specular) light.Flags |= Engine::ELightFlags::AffectSpecular;

			(void)trans.SetEulerRotation(rotEuler);
			return entity;
		};

	// 3점 스튜디오 조명 (Key: 주광, Fill: 보조광, Rim: 역광)
	const auto& intensities = EditorSettingsService::Get().GetPreferences().PreviewLightIntensity;
	m_StudioLights[0] = createDirLight(Engine::vec3(30.0f, 195.0f, 0.0f), Engine::vec3(1.0f, 0.98f, 0.95f), intensities[0], true);
	m_StudioLights[1] = createDirLight(Engine::vec3(15.0f, 120.0f, 0.0f), Engine::vec3(0.85f, 0.92f, 1.0f), intensities[1], false);
	m_StudioLights[2] = createDirLight(Engine::vec3(45.0f, 20.0f, 0.0f), Engine::vec3(0.95f, 0.98f, 1.0f), intensities[2], true);

	Engine::TransformSystem::UpdateHierarchy(reg);
	Engine::LightSystem::UpdateLights(reg);
}

void PreviewViewportWidget::FocusTarget(const Engine::vec3& targetPos, Engine::f32 distance)
{
	m_FocusModel = entt::null;
	m_ModelFocusPending = false;
	m_FocusPosition = targetPos;
	m_FocusDistance = distance;
	ApplyFocus(targetPos, distance);
}

bool PreviewViewportWidget::SetModelFocusTarget(const Engine::Entity& model, bool refocus)
{
	if (!m_PreviewScene || model.GetScene() != m_PreviewScene)
		return false;

	const auto root = model.GetEntityHandle();
	Engine::BoundingBox bounds;
	if (!TryGetModelBounds(root, bounds) || (refocus && !FrameModelBounds(bounds)))
		return false;

	m_FocusModel = root;
	m_ModelFocusPending = refocus;
	return true;
}

bool PreviewViewportWidget::TryGetModelBounds(entt::entity root, Engine::BoundingBox& bounds) const
{
	const auto& registry = m_PreviewScene->GetRegistry();
	if (!registry.valid(root))
		return false;

	std::vector<entt::entity> pending{ root };
	bool hasBounds = false;
	while (!pending.empty())
	{
		const auto entity = pending.back();
		pending.pop_back();
		if (const auto* hierarchy = registry.try_get<Engine::HierarchyComponent>(entity))
			pending.insert(pending.end(), hierarchy->children.begin(), hierarchy->children.end());

		const Engine::Mesh* mesh = nullptr;
		if (const auto* renderer = Engine::TryGetEnabledComponent<Engine::StaticMeshRendererComponent>(registry, entity))
			mesh = renderer->meshHandle.Get();
		else if (const auto* skinned = Engine::TryGetEnabledComponent<Engine::SkinnedMeshRendererComponent>(registry, entity))
			mesh = skinned->meshHandle.Get();

		const auto* transform = registry.try_get<Engine::WorldTransformComponent>(entity);
		if (!mesh || !transform)
			continue;

		Engine::BoundingBox worldBounds;
		if (!Engine::BoundingBox(mesh->GetMin(), mesh->GetMax()).TryTransformToAABB(transform->worldMatrix, worldBounds))
			return false;

		bounds.min = hasBounds ? glm::min(bounds.min, worldBounds.min) : worldBounds.min;
		bounds.max = hasBounds ? glm::max(bounds.max, worldBounds.max) : worldBounds.max;
		hasBounds = true;
	}
	return hasBounds;
}

bool PreviewViewportWidget::FrameModelBounds(const Engine::BoundingBox& bounds)
{
	auto* camera = m_CameraController ? m_CameraController->GetCameraComponent() : nullptr;
	if (!camera)
		return false;

	const auto framing = Editor::PreviewCameraDefaults::CalculateFraming(bounds, camera->AspectRatio, camera->FOV);
	if (!framing)
		return false;

	camera->NearPlane = framing->NearPlane;
	camera->FarPlane = framing->FarPlane;
	ApplyFocus(framing->Center, framing->Distance);

	const float recommendedSpeed = std::clamp(framing->Distance * 0.25f, 1.0f, 5000.0f);
	m_CameraController->SetCameraSpeed(recommendedSpeed);
	m_CameraController->SetZoomSpeed3D(std::clamp(framing->Distance * 0.05f, 0.5f, 500.0f));
	return true;
}

void PreviewViewportWidget::ApplyFocus(const Engine::vec3& targetPos, Engine::f32 distance)
{
	auto* transform = m_CameraController ? m_CameraController->GetTransformComponent() : nullptr;
	if (!transform) return;

	const Engine::vec3 cameraPos = targetPos - Editor::PreviewCameraDefaults::GetForward() * distance;

	(void)transform->SetLocalPosition(cameraPos);
	m_CameraController->SetPivotPoint(targetPos);
	m_CameraController->SetControlRotation(Engine::vec3(Editor::PreviewCameraDefaults::PitchDegrees,
		Editor::PreviewCameraDefaults::YawDegrees, 0.0f));
	m_CameraController->RefreshMatrices();
}

void PreviewViewportWidget::SetCameraSpeed(Engine::f32 speed)
{
	if (m_CameraController) m_CameraController->SetCameraSpeed(speed);
}

void PreviewViewportWidget::CheckResize(uint32 targetWidth, uint32 targetHeight)
{
	if (targetWidth == 0 || targetHeight == 0) return;
	if (targetWidth == m_Width && targetHeight == m_Height) return;

	m_Width = std::max(targetWidth, 64u);
	m_Height = std::max(targetHeight, 64u);

	m_Pipeline.Resize(m_Width, m_Height);
	m_PostProcessChain.Resize(m_Width, m_Height);
	if (m_ChannelFilter.GetChannelPreviewRT())
		(void)m_ChannelFilter.Resize(m_Pipeline.GetFinalColorRTName());

	if (m_CameraController)
	{
		m_CameraController->SetAspectRatio(static_cast<f32>(m_Width) / static_cast<f32>(m_Height));
		m_CameraController->RefreshMatrices();
	}
}

void PreviewViewportWidget::BeginFrame(Engine::f32 dt)
{
	if (!m_PreviewScene) return;

	auto& reg = m_PreviewScene->GetRegistry();
	if (!m_HostPoseControl)
	{
		auto animView = reg.view<Engine::AnimatorComponent>();
		for (auto animEntity : animView)
		{
			if (!Engine::IsComponentEnabled<Engine::AnimatorComponent>(reg, animEntity)) continue;
			auto& animator = animView.get<Engine::AnimatorComponent>(animEntity);
			const auto* skeleton = Engine::FindEnabledSkeletonInHierarchy(reg, animEntity);
			if (!skeleton) continue;
			const auto source = Engine::UpdateAnimatorBasePose(animator, *skeleton, dt);
			if (source == Engine::EAnimatorBasePoseSource::None)
			{
				Engine::AnimationEvaluator::EvaluateBindPoseSkinningMatrices(*skeleton, animator.finalBoneMatrices);
				continue;
			}
			if (source == Engine::EAnimatorBasePoseSource::StateMachine)
				(void)animator.runtimeState.PopTriggeredNotifies();
			Engine::UpdateAnimatorSkinning(animator, *skeleton, dt);
		}
	}

	// 5. 씬 변환 계층과 조명 GPU 버퍼 갱신
	Engine::TransformSystem::UpdateHierarchy(reg);

	const auto& saved = EditorSettingsService::Get().GetPreferences().PreviewLightIntensity;
	const auto& intensities = m_PreviewLightDraft ? *m_PreviewLightDraft : saved;
	for (std::size_t i = 0; i < m_StudioLights.size(); ++i)
	{
		if (auto* light = Engine::TryGetEnabledComponent<Engine::LightComponent>(reg, m_StudioLights[i]))
			light->Intensity = intensities[i] * m_StudioLightMultiplier;
	}

	Engine::LightSystem::UpdateLights(reg);
}

void PreviewViewportWidget::Submit(Engine::f32 dt)
{
	if (!m_PreviewScene || !m_CameraController) return;

	const auto optCamera = m_CameraController->GetCameraBuffer();
	if (!optCamera.has_value()) return;
	const auto& cameraBuffer = optCamera.value();

	// 1. 디퍼드 파이프라인 패스 카메라 등록 및 조명 연산 준비
	m_Pipeline.Submit(cameraBuffer, m_PreviewScene);

	// 2. 머티리얼에 맞는 프리뷰 패스로 메시를 제출한다.
	auto& reg = m_PreviewScene->GetRegistry();
	const uint32 geomPassId = m_Pipeline.GetGeometryPassID();
	const uint32 transparentPassId = m_Pipeline.GetForwardTransparentPassID();
	const auto meshPass = [&](const Engine::MaterialInterface& material)
	{
		return material.GetPrimaryPassType() == Engine::ERenderPassType::ForwardTransparent ? transparentPassId : geomPassId;
	};

	// 정적 메쉬 제출
	auto staticView = reg.view<Engine::StaticMeshRendererComponent, Engine::WorldTransformComponent>();
	for (auto entity : staticView)
	{
		if (!Engine::IsComponentEnabled<Engine::StaticMeshRendererComponent>(reg, entity)) continue;
		const auto& meshComp = staticView.get<Engine::StaticMeshRendererComponent>(entity);
		const auto& transComp = staticView.get<Engine::WorldTransformComponent>(entity);
		auto* mesh = meshComp.meshHandle.Get();
		auto* mat = meshComp.materialHandle.Get();
		if (mesh && mat)
		{
			Engine::StaticDrawCommand cmd{};
			cmd.mesh = mesh;
			cmd.material = mat;
			// 씬이 요청한 밉과 상관없이 미리보기에는 가장 자세한 밉을 쓴다.
			if (Engine::ResourceStreamer::IsCreated()) Engine::ResourceStreamer::Get().RequestMaterial(*mat, 0.0f, 1.0f);
			cmd.worldMatrix = transComp.worldMatrix;
			cmd.pickingId = 0;
			Engine::Renderer::Get().SubmitStatic(cmd, meshPass(*mat));
		}
	}

	// 스켈레탈(스키닝) 메쉬 제출 (루트 엔티티의 AnimatorComponent 행렬 자동 연계)
	auto skinnedView = reg.view<Engine::SkinnedMeshRendererComponent, Engine::WorldTransformComponent>();
	for (auto entity : skinnedView)
	{
		if (!Engine::IsComponentEnabled<Engine::SkinnedMeshRendererComponent>(reg, entity)) continue;
		const auto& skinnedComp = skinnedView.get<Engine::SkinnedMeshRendererComponent>(entity);
		const auto& transComp = skinnedView.get<Engine::WorldTransformComponent>(entity);
		auto* mesh = skinnedComp.meshHandle.Get();
		auto* mat = skinnedComp.materialHandle.Get();
		if (mesh && mat)
		{
			entt::entity animEntity = Engine::FindAnimatorEntityInAncestors(reg, entity);
			if (animEntity == entt::null) animEntity = FindFirstEnabledAnimator(reg);
			const auto* animator = animEntity != entt::null
				? Engine::TryGetEnabledComponent<Engine::AnimatorComponent>(reg, animEntity) : nullptr;
			const auto* boneMatrices = (animator && !animator->finalBoneMatrices.empty()) ? &animator->finalBoneMatrices : nullptr;

			Engine::mat4 renderWorldMatrix = transComp.worldMatrix;
			if (animEntity != entt::null)
			{
				if (const auto* animWorld = reg.try_get<Engine::WorldTransformComponent>(animEntity))
					renderWorldMatrix = animWorld->worldMatrix;
			}

			Engine::SkinnedDrawCommand cmd{};
			cmd.mesh = mesh;
			cmd.material = mat;
			// 씬이 요청한 밉과 상관없이 미리보기에는 가장 자세한 밉을 쓴다.
			if (Engine::ResourceStreamer::IsCreated()) Engine::ResourceStreamer::Get().RequestMaterial(*mat, 0.0f, 1.0f);
			if (mat) mat->BuildMaterialConstants(cmd.materialConstants);
			cmd.worldMatrix = renderWorldMatrix;
			cmd.boneMatrices = boneMatrices;
			cmd.pickingId = 0;
			Engine::Renderer::Get().SubmitSkinned(cmd, meshPass(*mat));
		}
	}

	// 파티클 시스템 제출 (프리뷰 씬 내 파티클 에미터/이펙트 렌더링)
	Engine::ParticleSystem::SubmitSceneParticles(m_PreviewScene, cameraBuffer, m_Pipeline.GetGeometryPassID());
	Engine::ParticleSystem::SubmitSceneParticles(m_PreviewScene, cameraBuffer, m_Pipeline.GetForwardTransparentPassID());
	Engine::CompositeVFXSystem::SubmitSceneVFX(m_PreviewScene, cameraBuffer, m_Pipeline.GetGeometryPassID());
	Engine::CompositeVFXSystem::SubmitSceneVFX(m_PreviewScene, cameraBuffer, m_Pipeline.GetForwardTransparentPassID());

	const bool showFinalOutput = m_SelectedRTName.empty();
	m_DisplayRTName = showFinalOutput
		? m_PostProcessChain.ExecuteChain(dt, m_Pipeline.GetFinalColorRTName())
		: m_SelectedRTName;

	m_ChannelFilter.SubmitChannelPreviewPass(m_DisplayRTName, m_DisplayRTName, !showFinalOutput);
	if (showFinalOutput && m_ShowGrid)
	{
		m_Grid.SubmitGrid(cameraBuffer, m_CameraController->IsOrthographic(),
			m_DisplayRTName, m_Pipeline.GetDepthRTName());
	}
}

void PreviewViewportWidget::Draw(const ImVec2& size)
{
	const ImVec2 avail = (size.x > 0.0f && size.y > 0.0f) ? size : ImGui::GetContentRegionAvail();
	const uint32 targetWidth = static_cast<uint32>(std::max(avail.x, 64.0f));
	const uint32 targetHeight = static_cast<uint32>(std::max(avail.y, 64.0f));
	CheckResize(targetWidth, targetHeight);
	if (m_ModelFocusPending)
	{
		m_ModelFocusPending = false;
		RefocusTarget();
	}

	const auto image = ResolveViewportImage(m_DisplayRTName);
	if (image.Status != EViewportImageStatus::Ready)
		m_PreviewLightDraft.reset();

	if (image.Status == EViewportImageStatus::Unsupported)
	{
		m_SelectedRTName.clear();
		m_IsHovered = false;
		m_IsFocused = false;
		m_CameraDragButton.reset();
		ImGui::TextWrapped("%s", LOCAL_CSTR("Viewport.RenderTargetPreviewUnsupported"));
		return;
	}
	if (image.Status != EViewportImageStatus::Ready)
	{
		ImGui::Dummy(avail);
		return;
	}

	const ImVec2 startCursor = ImGui::GetCursorPos();
	const ImVec2 widgetSize(static_cast<float>(m_Width), static_cast<float>(m_Height));

	// 오버레이 툴바는 같은 크기의 버튼을 한 줄로만 배치한다. 폭이 부족하면 줄바꿈하지 않고 툴바 영역에서 잘린다.
	const auto& style = ImGui::GetStyle();
	const float margin = std::min(ImGui::GetFontSize() * 0.4f, std::min(widgetSize.x, widgetSize.y) * 0.1f);
	const ImVec2 slot = GetOverlayButtonSize();
	const bool hasAnimator = m_PreviewScene && FindFirstEnabledAnimator(m_PreviewScene->GetRegistry()) != entt::null;
	const int buttonCount = hasAnimator ? 6 : 4;
	const float overlayWidth = std::max(1.0f, widgetSize.x - margin * 2.0f);
	const float overlayHeight = std::max(1.0f, widgetSize.y - margin * 2.0f);
	const float channelWidth = std::min(overlayWidth, ChannelFilter::GetButtonsWidth());
	const ImVec2 channelSize(channelWidth, std::min(overlayHeight, ChannelFilter::GetButtonSize().y));
	const float channelX = margin + overlayWidth - channelWidth;
	const float leftWidth = overlayWidth - channelWidth - style.ItemSpacing.x;
	const float rowWidth = buttonCount * slot.x + (buttonCount - 1) * style.ItemSpacing.x + style.WindowPadding.x * 2.0f;
	const bool shareRow = rowWidth <= leftWidth;
	const float toolbarOffset = shareRow ? 0.0f : channelSize.y + style.ItemSpacing.y;
	const bool showToolbar = toolbarOffset < overlayHeight;
	const ImVec2 toolbarSize(std::min(rowWidth, overlayWidth), std::min(overlayHeight - toolbarOffset, slot.y + style.WindowPadding.y * 2.0f));

	// 1. 뷰포트 렌더타깃 이미지 먼저 렌더링
	ImGui::SetCursorPos(startCursor);
	DrawViewportImage(image, widgetSize);
	m_ViewportScreenPos = ImGui::GetItemRectMin();
	DrawBoneOverlay();

	using Engine::EMouseButton;
	auto& input = Engine::InputManager::Get();
	const auto& io = ImGui::GetIO();
	const auto mousePos = input.GetScreenMousePosition();

	const auto isOver = [&](float x, float y, const ImVec2& size)
		{
			const float minX = m_ViewportScreenPos.x + x;
			const float minY = m_ViewportScreenPos.y + y;
			const float maxX = minX + size.x;
			const float maxY = minY + size.y;
			return (mousePos.x >= minX && mousePos.x <= maxX && mousePos.y >= minY && mousePos.y <= maxY);
		};
	const bool overToolbar = isOver(channelX, margin, channelSize) ||
		(showToolbar && isOver(margin, margin + toolbarOffset, toolbarSize));

	const bool mouseInViewport = (mousePos.x >= m_ViewportScreenPos.x && mousePos.x <= m_ViewportScreenPos.x + widgetSize.x &&
		mousePos.y >= m_ViewportScreenPos.y && mousePos.y <= m_ViewportScreenPos.y + widgetSize.y);
	m_IsHovered = mouseInViewport && !overToolbar;
	m_IsFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

	const bool popupAbove = ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId);
	const bool canNavigate = !io.WantTextInput && !io.AppFocusLost && !popupAbove;

	// 마우스 드래그 유지 및 해제 (드래그 중에는 마우스가 창 밖으로 살짝 나가도 버튼을 뗄 때까지 유지)
	if (!canNavigate)
	{
		m_CameraDragButton.reset();
	}
	else if (m_CameraDragButton)
	{
		if (!input.IsMouseButtonPressed(*m_CameraDragButton))
			m_CameraDragButton.reset();
	}
	else if (m_IsHovered)
	{
		// 카메라 조작 판정: Alt+LMB(Orbit), MMB(Pan), RMB(Look/Fly)
		const bool overGizmo = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
		const bool isAlt = input.IsAltPressed();

		if (isAlt && input.IsMouseButtonDown(EMouseButton::Left))
		{
			m_CameraDragButton = EMouseButton::Left;
			ImGui::SetWindowFocus();
			m_IsFocused = true;
		}
		else if (input.IsMouseButtonDown(EMouseButton::Right))
		{
			m_CameraDragButton = EMouseButton::Right;
			ImGui::SetWindowFocus();
			m_IsFocused = true;
		}
		else if (input.IsMouseButtonDown(EMouseButton::Middle))
		{
			m_CameraDragButton = EMouseButton::Middle;
			ImGui::SetWindowFocus();
			m_IsFocused = true;
		}
		else if (!overGizmo && input.IsMouseButtonDown(EMouseButton::Left))
		{
			ImGui::SetWindowFocus();
			m_IsFocused = true;
		}
	}

	if (canNavigate && (m_IsHovered || m_IsFocused))
		s_ShortcutTarget = this;
	if (m_CameraController && canNavigate && (m_IsHovered || m_CameraDragButton.has_value()))
		m_CameraController->HandleInput(io.DeltaTime, m_IsHovered, m_CameraDragButton);

	const ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav;
	if (showToolbar)
	{
		ImGui::SetCursorPos(ImVec2(startCursor.x + margin, startCursor.y + margin + toolbarOffset));
		if (ImGui::BeginChild("##OverlayToolbarChild", toolbarSize, ImGuiChildFlags_AlwaysUseWindowPadding, toolbarFlags))
			DrawOverlayToolbar();
		else
			m_PreviewLightDraft.reset();
		ImGui::EndChild();
	}
	else
	{
		m_PreviewLightDraft.reset();
	}

	ImGui::SetCursorPos(ImVec2(startCursor.x + channelX, startCursor.y + margin));
	if (ImGui::BeginChild("##OverlayChannelsChild", channelSize, ImGuiChildFlags_None, toolbarFlags))
		m_ChannelFilter.DrawButtons();
	ImGui::EndChild();

	ImGui::SetCursorPos(startCursor);
	ImGui::Dummy(widgetSize);
}

void PreviewViewportWidget::DrawOverlayToolbar()
{
	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		s_ShortcutTarget = this;

	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 0.75f));

	DrawRenderTargetPopup();
	ImGui::SameLine();
	DrawPreviewLightingPopup();

	auto options = MakeOverlayButtonOptions();
	options.Active = m_ShowGrid;
	options.ShortcutHint = ShortcutManager::Get().GetShortcutHint("Viewport.ToggleGrid");
	ImGui::SameLine();
	if (EditorUI::IconButton("Grid", m_ShowGrid ? ICON_FA_BORDER_ALL : ICON_FA_BORDER_NONE, "PreviewViewport.GridToggle", options))
		ToggleGrid();

	options.Active = false;
	options.Enabled = CanFocusTarget();
	options.ShortcutHint = ShortcutManager::Get().GetShortcutHint("Viewport.Focus");
	ImGui::SameLine();
	if (EditorUI::IconButton("Focus", ICON_FA_CROSSHAIRS, "PreviewViewport.Focus", options))
	{
		s_ShortcutTarget = this;
		ShortcutManager::Get().ExecuteAction("Viewport.Focus");
	}

	if (m_PreviewScene)
	{
		auto& world = m_PreviewScene->GetRegistry();
		const auto animEntity = FindFirstEnabledAnimator(world);
		if (animEntity != entt::null)
		{
			options.Enabled = true;
			options.ShortcutHint = nullptr;
			ImGui::SameLine();
			if (m_HostPoseControl)
			{
				options.Active = m_ShowBindPose;
				if (EditorUI::IconButton("BindPose", ICON_FA_PERSON, "PreviewViewport.ShowBindPose", options))
					m_ShowBindPose = !m_ShowBindPose;
			}
			else
			{
				auto& anim = world.get<Engine::AnimatorComponent>(animEntity);
				options.Active = anim.isPlaying;
				if (EditorUI::IconButton("PlayMotion", anim.isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY, anim.isPlaying ? "PreviewViewport.ShowBindPose" : "PreviewViewport.PlayMotion", options))
					anim.isPlaying = !anim.isPlaying;
			}
			ImGui::SameLine();
			DrawBoneOverlayPopup();
		}
	}

	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
}

Engine::RHITexture* PreviewViewportWidget::GetPreviewRHITexture() const
{
	auto& rtm = Engine::RenderTargetManager::Get();
	auto* colorRT = rtm.GetRenderTarget(m_DisplayRTName);
	return colorRT ? colorRT->GetTexture() : nullptr;
}

bool PreviewViewportWidget::CanFocusTarget() const
{
	return m_PreviewScene && m_CameraController && m_CameraController->GetTransformComponent() &&
		(m_FocusModel == entt::null || m_PreviewScene->GetRegistry().valid(m_FocusModel));
}

void PreviewViewportWidget::RefocusTarget()
{
	if (!CanFocusTarget())
		return;

	if (m_FocusModel == entt::null)
	{
		ApplyFocus(m_FocusPosition, m_FocusDistance);
		return;
	}

	Engine::BoundingBox bounds;
	if (TryGetModelBounds(m_FocusModel, bounds))
		FrameModelBounds(bounds);
}

void PreviewViewportWidget::DrawRenderTargetPopup()
{
	if (EditorUI::IconButton("PreviewRenderTargets", ICON_FA_LAYER_GROUP, "Viewport.RenderTargetsMenu", MakeOverlayButtonOptions()))
		ImGui::OpenPopup("##PreviewRenderTargets");

	if (!ImGui::BeginPopup("##PreviewRenderTargets"))
		return;

	if (ImGui::Selectable(LOCAL_LABEL("Viewport.FinalOutput").c_str(), m_SelectedRTName.empty(),
		ImGuiSelectableFlags_NoAutoClosePopups))
	{
		m_SelectedRTName.clear();
	}

	const std::wstring prefix = m_Id.empty() ? L"VP_" : m_Id + L"_";
	for (const auto& name : m_Pipeline.GetOwnedRTNames())
	{
		if (name == m_Pipeline.GetShadowDepthRTName() || name == m_Pipeline.GetPointShadowDepthRTName())
			continue;

		const auto shortName = name.starts_with(prefix) ? name.substr(prefix.size()) : name;
		const auto label = Engine::WStrToStr(shortName);

		ImGui::PushID(label.c_str());
		if (ImGui::Selectable(label.c_str(), m_SelectedRTName == name, ImGuiSelectableFlags_NoAutoClosePopups))
			m_SelectedRTName = name;
		ImGui::PopID();
	}

	ImGui::EndPopup();
}

void PreviewViewportWidget::DrawPreviewLightingPopup()
{
	constexpr const char* popupId = "##PreviewLightingPopup";
	auto& settings = EditorSettingsService::Get();

	if (EditorUI::IconButton("PreviewLighting", ICON_FA_LIGHTBULB, "Settings.PreviewLighting", MakeOverlayButtonOptions()))
		ImGui::OpenPopup(popupId);

	if (!ImGui::BeginPopup(popupId))
	{
		m_PreviewLightDraft.reset();
		return;
	}

	if (!m_PreviewLightDraft)
		m_PreviewLightDraft = settings.GetPreferences().PreviewLightIntensity;
	auto& draft = *m_PreviewLightDraft;
	constexpr const char* keys[] = {
		"Settings.PreviewKeyLight", "Settings.PreviewFillLight", "Settings.PreviewRimLight"
	};

	for (std::size_t i = 0; i < draft.size(); ++i)
	{
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 12.0f);
		ImGui::SliderFloat(LOCAL_LABEL(keys[i]).c_str(), &draft[i],
			0.0f, EditorPreferences::MaxPreviewLightIntensity, "%.2f");
	}

	if (ImGui::Button(LOCAL_LABEL("Settings.Btn.ResetDefaults").c_str()))
		draft = EditorPreferences{}.PreviewLightIntensity;

	ImGui::Separator();
	ImGui::TextUnformatted(LOCAL_CSTR("PreviewViewport.LightingApplyScope"));

	const EditorUI::DialogAction actions[] = {
		{ "EditorCommon.Cancel" }, { "EditorCommon.Apply", draft != settings.GetPreferences().PreviewLightIntensity }
	};
	const int action = EditorUI::DialogButtonBar("LightingActions", actions, IM_ARRAYSIZE(actions));
	if (action == 1)
	{
		const auto result = settings.SetPreviewLightIntensity(draft);
		ImGuiManager::Get().NotifySettingsChange(result);
		if (result.Applied)
			ImGui::CloseCurrentPopup();
	}
	if (action == 0)
		ImGui::CloseCurrentPopup();

	ImGui::EndPopup();

	if (!ImGui::IsPopupOpen(popupId))
		m_PreviewLightDraft.reset();
}

void PreviewViewportWidget::DrawBoneOverlayPopup()
{
	constexpr const char* popupId = "##PreviewBoneOverlayPopup";
	auto options = MakeOverlayButtonOptions();
	options.Active = m_ShowBoneJoints || m_ShowBoneAxes || m_ShowBoneNames;
	if (EditorUI::IconButton("BoneOverlay", ICON_FA_BONE, "PreviewViewport.BoneOverlay", options))
		ImGui::OpenPopup(popupId);

	if (!ImGui::BeginPopup(popupId)) return;
	ImGui::Checkbox(LOCAL_LABEL("PreviewViewport.ShowBoneJoints").c_str(), &m_ShowBoneJoints);
	ImGui::Checkbox(LOCAL_LABEL("PreviewViewport.ShowBoneAxes").c_str(), &m_ShowBoneAxes);
	ImGui::Checkbox(LOCAL_LABEL("PreviewViewport.ShowBoneNames").c_str(), &m_ShowBoneNames);
	ImGui::EndPopup();
}

void PreviewViewportWidget::DrawBoneOverlay()
{
	if (!m_PreviewScene || !m_CameraController) return;
	if (!m_ShowBoneJoints && !m_ShowBoneAxes && !m_ShowBoneNames) return;
	const auto camera = m_CameraController->GetCameraBuffer();
	if (!camera) return;

	const ImVec2 origin = m_ViewportScreenPos;
	const ImVec2 size = GetViewportSize();
	const Engine::mat4& viewProj = camera->viewProjMatrix;
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	drawList->PushClipRect(origin, ImVec2(origin.x + size.x, origin.y + size.y), true);

	constexpr ImU32 axisColors[] = { IM_COL32(230, 60, 60, 255), IM_COL32(60, 200, 60, 255), IM_COL32(70, 120, 240, 255) };
	auto& reg = m_PreviewScene->GetRegistry();
	auto animView = reg.view<Engine::AnimatorComponent>();
	for (auto entity : animView)
	{
		if (!Engine::IsComponentEnabled<Engine::AnimatorComponent>(reg, entity)) continue;
		const auto& animator = animView.get<Engine::AnimatorComponent>(entity);
		const auto* skeleton = Engine::FindEnabledSkeletonInHierarchy(reg, entity);
		if (!skeleton) continue;
		const auto& bones = skeleton->GetBones();
		if (bones.empty() || animator.finalBoneMatrices.size() < bones.size()) continue;

		Engine::mat4 entityWorld(1.0f);
		if (const auto* wt = reg.try_get<Engine::WorldTransformComponent>(entity)) entityWorld = wt->worldMatrix;

		// 스키닝 행렬(global * offset)에서 오프셋을 되돌려 본의 월드 행렬을 구한다. 타임라인 소켓 계산과 같은 규칙이다.
		std::vector<Engine::mat4> boneWorld(bones.size());
		for (std::size_t i = 0; i < bones.size(); ++i)
			boneWorld[i] = entityWorld * animator.finalBoneMatrices[i] * glm::inverse(bones[i].OffsetMatrix);

		for (std::size_t i = 0; i < bones.size(); ++i)
		{
			const Engine::vec3 pos = Engine::vec3(boneWorld[i][3]);
			ImVec2 screenPos{};
			if (!GizmoHelper::WorldToScreen(pos, viewProj, origin, size, screenPos)) continue;

			if (m_ShowBoneJoints)
			{
				const Engine::int32 parent = bones[i].ParentIndex;
				ImVec2 parentPos{};
				if (parent >= 0 && parent < static_cast<Engine::int32>(bones.size()) && GizmoHelper::WorldToScreen(Engine::vec3(boneWorld[parent][3]), viewProj, origin, size, parentPos))
					drawList->AddLine(parentPos, screenPos, IM_COL32(255, 210, 80, 200), 1.5f);
				drawList->AddCircleFilled(screenPos, 3.5f, IM_COL32(255, 230, 120, 255));
			}

			if (m_ShowBoneAxes)
			{
				// 카메라 거리에 비례한 길이로 원근을 유지하면서 화면상 크기를 일정하게 둔다.
				const float axisLength = glm::distance(camera->cameraPosition, pos) * 0.05f;
				for (int axis = 0; axis < 3; ++axis)
				{
					const Engine::vec3 dir = Engine::vec3(boneWorld[i][axis]);
					if (glm::length(dir) < 1e-6f) continue;
					ImVec2 axisEnd{};
					if (GizmoHelper::WorldToScreen(pos + glm::normalize(dir) * axisLength, viewProj, origin, size, axisEnd))
						drawList->AddLine(screenPos, axisEnd, axisColors[axis], 2.0f);
				}
			}

			if (m_ShowBoneNames)
			{
				const std::string name = Engine::WStrToStr(bones[i].Name);
				const ImVec2 textPos(screenPos.x + 5.0f, screenPos.y - ImGui::GetFontSize() - 2.0f);
				drawList->AddText(ImVec2(textPos.x + 1.0f, textPos.y + 1.0f), IM_COL32(0, 0, 0, 200), name.c_str());
				drawList->AddText(textPos, IM_COL32(255, 255, 255, 230), name.c_str());
			}
		}
	}
	drawList->PopClipRect();
}

void PreviewViewportWidget::ApplyGlobalCameraPreferences(float speed, float fov)
{
	for (auto* widget : s_ActiveWidgets)
	{
		if (!widget || !widget->m_CameraController) continue;
		widget->m_CameraController->SetCameraSpeed(speed);
		if (auto* camera = widget->m_CameraController->GetCameraComponent())
		{
			camera->FOV = fov;
			if (camera->CameraType == Engine::ECameraType::Perspective)
				widget->m_CameraController->RefreshMatrices();
		}
	}
}
