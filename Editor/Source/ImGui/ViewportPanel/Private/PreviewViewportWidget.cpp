#include "InputManager.h"
#include "PreviewViewportWidget.h"
#include "EditorCameraController.h"
#include "Scene.h"
#include "Entity.h"
#include "Renderer.h"
#include "RenderTargetManager.h"
#include "RenderTarget.h"
#include "ImGuiManager.h"
#include "TransformSystem.h"
#include "LightSystem.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/NameComponent.h"
#include "Components/Render/StaticMeshRendererComponent.h"
#include "Components/Render/SkinnedMeshRendererComponent.h"
#include "Components/Render/AnimatorComponent.h"
#include "Components/Render/LightComponent.h"
#include "IconsFontAwesome7.h"
#include "AnimationEvaluator.h"
#include "Skeleton.h"
#include "Components/Core/HierarchyComponent.h"
#include "ShortcutManager.h"
#include "EditorSettingsService.h"
#include "Functions.h"
namespace Editor
{

	namespace
	{
		static const Engine::Skeleton* FindSkeletonInHierarchy(const entt::registry& world, entt::entity root)
		{
			if (!world.valid(root)) return nullptr;
			if (const auto* skinned = world.try_get<Engine::SkinnedMeshRendererComponent>(root))
			{
				if (const auto* skeleton = skinned->skeletonHandle.Get()) return skeleton;
			}

			const auto* hierarchy = world.try_get<Engine::HierarchyComponent>(root);
			if (!hierarchy) return nullptr;

			for (entt::entity child : hierarchy->children)
			{
				if (!world.valid(child)) continue;
				if (const auto* skeleton = FindSkeletonInHierarchy(world, child)) return skeleton;
			}
			return nullptr;
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
		if (m_ShowGrid)
		{
			m_Grid.Initialize(m_Id + L"_Grid");
			m_Grid.Show();
		}

		// 4. 에디터 카메라 컨트롤러 초기화 (에디터 표준 조작계)
		Engine::CameraComponent cameraSettings{};
		cameraSettings.CameraType = Engine::ECameraType::Perspective;
		cameraSettings.FOV = 45.0f;
		cameraSettings.NearPlane = 0.1f;
		cameraSettings.FarPlane = 1000.0f;
		cameraSettings.AspectRatio = static_cast<f32>(m_Width) / static_cast<f32>(m_Height);

		m_CameraController = EditorCameraController::Create(m_Id + L"_Camera", cameraSettings);
		if (m_CameraController)
		{
			m_CameraController->SetCameraSpeed(10.0f);
			FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
		}

		// 5. 기본 스튜디오 조명 자동 구축
		SetupDefaultStudioLighting();
	}

	void PreviewViewportWidget::Free()
	{
		if (s_ShortcutTarget == this)
			s_ShortcutTarget = nullptr;

		m_IsHovered = false;
		m_IsFocused = false;

		m_Grid.Free();
		m_PostProcessChain.Free();
		m_ChannelFilter.Free();
		m_SelectedRTName.clear();
		m_ChannelView = EViewportChannelView::RGBA;
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
		m_StudioLights[0] = createDirLight(Engine::vec3(30.0f, 25.0f, 0.0f), Engine::vec3(1.0f, 0.98f, 0.95f), intensities[0], true);
		m_StudioLights[1] = createDirLight(Engine::vec3(15.0f, -45.0f, 0.0f), Engine::vec3(0.85f, 0.92f, 1.0f), intensities[1], false);
		m_StudioLights[2] = createDirLight(Engine::vec3(45.0f, 160.0f, 0.0f), Engine::vec3(0.95f, 0.98f, 1.0f), intensities[2], true);

		Engine::TransformSystem::UpdateHierarchy(reg);
		Engine::LightSystem::UpdateLights(reg);
	}

	void PreviewViewportWidget::FocusTarget(const Engine::vec3& targetPos, Engine::f32 distance)
	{
		if (!m_CameraController) return;
		auto* transform = m_CameraController->GetTransformComponent();
		if (!transform) return;
		m_FocusPosition = targetPos;
		m_FocusDistance = distance;

		// 기존: Yaw 0도 (모델의 등 뒤에서 바라봄)
		// 수정: Yaw 160도 (모델의 정면 대각선 얼짱 각도에서 얼굴을 바라봄!)
		const Engine::f32 pitchDeg = 15.0f;
		const Engine::f32 yawDeg = 160.0f; // 180도 완전 정면 또는 150~160도 쿼터뷰
		const Engine::f32 pitchRad = glm::radians(pitchDeg);
		const Engine::f32 yawRad = glm::radians(yawDeg);

		// 카메라의 시선 방향 벡터
		Engine::vec3 forwardDir;
		forwardDir.x = sinf(yawRad) * cosf(pitchRad);
		forwardDir.y = -sinf(pitchRad);
		forwardDir.z = cosf(yawRad) * cosf(pitchRad);

		// 카메라 위치: 타깃으로부터 시선 반대 방향으로 물러남 (모델 앞쪽 +Z 영역에 카메라 배치)
		const Engine::vec3 cameraPos = targetPos - forwardDir * distance;

		(void)transform->SetLocalPosition(cameraPos);
		m_CameraController->SetPivotPoint(targetPos);
		m_CameraController->SetControlRotation(Engine::vec3(pitchDeg, yawDeg, 0.0f));
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

		// 1. 카메라 입력 처리
		if (m_CameraController && (m_IsFocused || m_IsHovered))
		{
			m_CameraController->HandleInput(dt, m_IsHovered);
		}

		auto& reg = m_PreviewScene->GetRegistry();
		auto animView = reg.view<Engine::AnimatorComponent>();

		for (auto animEntity : animView)
		{
			auto& animator = animView.get<Engine::AnimatorComponent>(animEntity);
			const auto* skeleton = FindSkeletonInHierarchy(reg, animEntity);
			if (!skeleton) continue;

			auto& localPose = animator.localBonePose;
			localPose.clear();

			// 2. 베이스 로컬 포즈 평가
			if (animator.isPlaying && animator.runtimeState.HasStateMachine())
			{
				animator.runtimeState.UpdateLocalPose(dt * animator.playRate, *skeleton, localPose);
				animator.currentTime = animator.runtimeState.GetStateTime();

				// 독립 미리보기에서는 게임플레이 노티파이를 실행하지 않는다.
				(void)animator.runtimeState.PopTriggeredNotifies();
			}
			else if (animator.isPlaying && animator.currentClip.Get())
			{
				const auto* clip = animator.currentClip.Get();
				const f64 tps = clip->GetTicksPerSecond() > 0.0 ? clip->GetTicksPerSecond() : 25.0;
				const f32 durationSec = clip->GetDuration() > 0.0
					? static_cast<f32>(clip->GetDuration() / tps) : 0.0f;

				animator.currentTime += dt * animator.playRate;
				if (!animator.isLooping && durationSec > 0.0f && animator.currentTime >= durationSec)
				{
					animator.currentTime = durationSec;
					animator.isPlaying = false;
				}

				Engine::AnimationEvaluator::EvaluateLocalPose(*clip, *skeleton,
					animator.currentTime, localPose, animator.isLooping);
			}
			else
			{
				// 기존 미리보기 정책: 정지 상태 또는 클립이 없으면 단위 스키닝 행렬 사용
				animator.finalBoneMatrices.assign(skeleton->GetBones().size(), glm::identity<Engine::mat4>());
				continue;
			}

			if (localPose.empty()) continue;

			// 3. 기존 포즈 위에 레이어를 배열 순서대로 누적 합성
			for (auto& layer : animator.layers)
			{
				if (!layer.isPlaying || layer.weight <= 0.0f) continue;

				const auto* layerClip = layer.clip.Get();
				if (!layerClip) continue;

				if (layer.mask.boneWeights.size() != skeleton->GetBones().size())
				{
					layer.mask.BuildWeights(*skeleton);
				}

				const f64 layerTps = layerClip->GetTicksPerSecond() > 0.0
					? layerClip->GetTicksPerSecond() : 25.0;
				const f32 layerDuration = layerClip->GetDuration() > 0.0
					? static_cast<f32>(layerClip->GetDuration() / layerTps) : 0.0f;

				layer.currentTime += dt * layer.playRate;
				if (!layer.isLooping && layerDuration > 0.0f && layer.currentTime >= layerDuration)
				{
					layer.currentTime = layerDuration;
					layer.isPlaying = false;
				}

				Engine::AnimationEvaluator::ApplyMaskedLocalPose(*layerClip, layer.currentTime,
					layer.mask.boneWeights, layer.weight, *skeleton, localPose, layer.isLooping);
			}

			// 4. 모든 합성이 끝난 뒤 최종 스키닝 행렬 생성
			Engine::AnimationEvaluator::BuildSkinningMatrices(*skeleton, localPose, animator.finalBoneMatrices);
		}

		// 5. 씬 변환 계층과 조명 GPU 버퍼 갱신
		Engine::TransformSystem::UpdateHierarchy(reg);

		const auto& saved = EditorSettingsService::Get().GetPreferences().PreviewLightIntensity;
		const auto& intensities = m_PreviewLightDraft ? *m_PreviewLightDraft : saved;
		for (std::size_t i = 0; i < m_StudioLights.size(); ++i)
		{
			if (auto* light = reg.try_get<Engine::LightComponent>(m_StudioLights[i]))
				light->Intensity = intensities[i];
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

		// 2. 프리뷰 씬 내부의 메쉬 엔티티들을 파이프라인의 GeometryPass에 자동 제출
		auto& reg = m_PreviewScene->GetRegistry();
		const uint32 geomPassId = m_Pipeline.GetGeometryPassID();

		// 정적 메쉬 제출
		auto staticView = reg.view<Engine::StaticMeshRendererComponent, Engine::WorldTransformComponent>();
		for (auto entity : staticView)
		{
			const auto& meshComp = staticView.get<Engine::StaticMeshRendererComponent>(entity);
			const auto& transComp = staticView.get<Engine::WorldTransformComponent>(entity);
			auto* mesh = meshComp.meshHandle.Get();
			auto* mat = meshComp.materialHandle.Get();
			if (mesh && mat)
			{
				Engine::StaticDrawCommand cmd{};
				cmd.mesh = mesh;
				cmd.material = mat;
				cmd.worldMatrix = transComp.worldMatrix;
				cmd.pickingId = 0;
				Engine::Renderer::Get().SubmitStatic(cmd, geomPassId);
			}
		}

		// 스켈레탈(스키닝) 메쉬 제출 (루트 엔티티의 AnimatorComponent 행렬 자동 연계)
		auto skinnedView = reg.view<Engine::SkinnedMeshRendererComponent, Engine::WorldTransformComponent>();
		for (auto entity : skinnedView)
		{
			const auto& skinnedComp = skinnedView.get<Engine::SkinnedMeshRendererComponent>(entity);
			const auto& transComp = skinnedView.get<Engine::WorldTransformComponent>(entity);
			auto* mesh = skinnedComp.meshHandle.Get();
			auto* mat = skinnedComp.materialHandle.Get();
			if (mesh && mat)
			{
				auto* animator = reg.try_get<Engine::AnimatorComponent>(entity);
				if (!animator)
				{
					auto animView = reg.view<Engine::AnimatorComponent>();
					if (!animView.empty()) animator = &animView.get<Engine::AnimatorComponent>(animView.front());
				}
				const auto* boneMatrices = (animator && !animator->finalBoneMatrices.empty()) ? &animator->finalBoneMatrices : nullptr;

				Engine::SkinnedDrawCommand cmd{};
				cmd.mesh = mesh;
				cmd.material = mat;
				cmd.worldMatrix = transComp.worldMatrix;
				cmd.boneMatrices = boneMatrices;
				cmd.pickingId = 0;
				Engine::Renderer::Get().SubmitSkinned(cmd, geomPassId);
			}
		}

		const bool showFinalOutput = m_SelectedRTName.empty();
		m_DisplayRTName = showFinalOutput
			? m_PostProcessChain.ExecuteChain(dt, m_Pipeline.GetFinalColorRTName())
			: m_SelectedRTName;

		auto channelView = m_ChannelView;
		if (!showFinalOutput && channelView == EViewportChannelView::RGBA)
			channelView = EViewportChannelView::RGB;

		if (channelView != EViewportChannelView::RGBA)
		{
			m_ChannelFilter.SetChannelView(channelView);
			m_ChannelFilter.SubmitChannelPreviewPass(m_DisplayRTName, m_DisplayRTName);
		}

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

		auto& rtm = Engine::RenderTargetManager::Get();
		auto* colorRT = rtm.GetRenderTarget(m_DisplayRTName);
		if (!colorRT || !colorRT->GetTexture())
		{
			ImGui::Dummy(avail);
			return;
		}

		ImTextureID texID = ImGuiManager::Get().GetImGuiTextureID(colorRT->GetTexture());
		if (!texID)
		{
			ImGui::Dummy(avail);
			return;
		}

		const ImVec2 startCursor = ImGui::GetCursorPos();
		const ImVec2 widgetSize(static_cast<float>(m_Width), static_cast<float>(m_Height));

		// 1. 뷰포트 마우스 입력 캐처 (Alt+LMB 시 ImGui 창 자체가 이동하는 것 방지)
		ImGui::InvisibleButton("##ViewportInputCatcher", widgetSize,
			ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);

		m_IsHovered = ImGui::IsItemHovered();
		if (m_IsHovered && (Engine::InputManager::Get().IsMouseButtonDown(Engine::EMouseButton::Right) || Engine::InputManager::Get().IsMouseButtonDown(Engine::EMouseButton::Middle) || Engine::InputManager::Get().IsMouseButtonDown(Engine::EMouseButton::Left)))
		{
			ImGui::SetWindowFocus();
			m_IsFocused = true;
		}
		if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			m_IsFocused = false;
		}
		if (m_IsHovered && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			s_ShortcutTarget = this;

		// 2. 뷰포트 렌더타깃 이미지 렌더링
		ImGui::SetCursorPos(startCursor);
		ImGui::Image(texID, widgetSize);

		// 3. 우측 상단 미니 오버레이 툴바 (자식 윈도우로 Z-order 상위 분리하여 클릭 보장)
		ImGui::SetCursorPos(ImVec2(startCursor.x + 8.0f, startCursor.y + 8.0f));
		const ImGuiWindowFlags toolbarFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
		const auto& style = ImGui::GetStyle();
		const float frameHeight = ImGui::GetFrameHeight();
		const ImVec2 toolbarSize(
			frameHeight * 5.0f + style.ItemSpacing.x * 4.0f + style.WindowPadding.x * 2.0f,
			frameHeight + style.WindowPadding.y * 2.0f);
		if (ImGui::BeginChild("##OverlayToolbarChild", toolbarSize, false, toolbarFlags))
		{
			DrawOverlayToolbar();
		}
		ImGui::EndChild();

		ImGui::SetCursorPos(startCursor);
		ImGui::Dummy(widgetSize);
	}

	void PreviewViewportWidget::DrawOverlayToolbar()
	{
		if (ImGui::IsWindowHovered() && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			s_ShortcutTarget = this;

		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 0.75f));

		DrawRenderTargetPopup();
		ImGui::SameLine();
		DrawPreviewLightingPopup();
		ImGui::SameLine();

		// 그리드 On/Off 토글 버튼
		const char* gridIcon = m_ShowGrid ? ICON_FA_BORDER_ALL : ICON_FA_BORDER_NONE;
		if (ImGui::Button(gridIcon))
		{
			m_ShowGrid = !m_ShowGrid;
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("PreviewViewport.GridToggle"));

		ImGui::SameLine();

		// 카메라 포커스 버튼
		auto& shortcuts = ShortcutManager::Get();

		ImGui::BeginDisabled(!CanFocusTarget());
		if (ImGui::Button(ICON_FA_CROSSHAIRS))
		{
			s_ShortcutTarget = this;
			shortcuts.ExecuteAction("Viewport.Focus");
		}
		ImGui::EndDisabled();

		if (ImGui::IsItemHovered())
		{
			const char* hint = shortcuts.GetShortcutHint("Viewport.Focus");
			if (hint && hint[0] != '\0')
				ImGui::SetTooltip("%s (%s)", LOCAL_CSTR("PreviewViewport.Focus"), hint);
			else
				ImGui::SetTooltip("%s", LOCAL_CSTR("PreviewViewport.Focus"));
		}

		// 씬에 애니메이터가 있다면 바인드 포즈 / 재생 토글 버튼 제공
		if (m_PreviewScene)
		{
			auto animView = m_PreviewScene->GetRegistry().view<Engine::AnimatorComponent>();
			if (!animView.empty())
			{
				auto& anim = animView.get<Engine::AnimatorComponent>(animView.front());
				ImGui::SameLine();
				const char* playIcon = anim.isPlaying ? ICON_FA_PAUSE : ICON_FA_PLAY;
				if (ImGui::Button(playIcon))
				{
					anim.isPlaying = !anim.isPlaying;
				}
				if (ImGui::IsItemHovered())
				{
					const char* tooltipKey = anim.isPlaying ? "PreviewViewport.ShowBindPose" : "PreviewViewport.PlayMotion";
					ImGui::SetTooltip("%s", LOCAL_CSTR(tooltipKey));
				}
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
		return m_PreviewScene && m_CameraController && m_CameraController->GetTransformComponent();
	}

	void PreviewViewportWidget::RefocusTarget()
	{
		if (!CanFocusTarget())
			return;

		FocusTarget(m_FocusPosition, m_FocusDistance);
	}

	void PreviewViewportWidget::DrawRenderTargetPopup()
	{
		if (ImGui::Button(ICON_FA_LAYER_GROUP "##PreviewRenderTargets"))
			ImGui::OpenPopup("##PreviewRenderTargets");

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", LOCAL_CSTR("Viewport.RenderTargetsMenu"));

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

		ImGui::Separator();
		ChannelFilter::DrawButtons(m_ChannelView);
		ImGui::EndPopup();
	}

	void PreviewViewportWidget::DrawPreviewLightingPopup()
	{
		constexpr const char* popupId = "##PreviewLightingPopup";
		auto& settings = EditorSettingsService::Get();

		if (ImGui::Button(ICON_FA_LIGHTBULB "##PreviewLighting"))
		{
			m_PreviewLightDraft = settings.GetPreferences().PreviewLightIntensity;
			ImGui::OpenPopup(popupId);
		}

		if (ImGui::IsItemHovered())
			ImGui::SetTooltip("%s", LOCAL_CSTR("Settings.PreviewLighting"));

		if (!ImGui::BeginPopup(popupId))
		{
			m_PreviewLightDraft.reset();
			return;
		}

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

		ImGui::BeginDisabled(draft == settings.GetPreferences().PreviewLightIntensity);
		if (ImGui::Button(LOCAL_LABEL("EditorCommon.Apply").c_str()))
		{
			const auto result = settings.SetPreviewLightIntensity(draft);
			ImGuiManager::Get().NotifySettingsChange(result);
			if (result.Applied)
				ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		if (ImGui::Button(LOCAL_LABEL("EditorCommon.Cancel").c_str()))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();

		if (!ImGui::IsPopupOpen(popupId))
			m_PreviewLightDraft.reset();
	}
}