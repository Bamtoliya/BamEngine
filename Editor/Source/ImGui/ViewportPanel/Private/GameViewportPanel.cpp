#include "GameViewportPanel.h"
#include "SceneManager.h"
#include "Scene.h"
#include "CameraComponent.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "FlagComponent.h"
#include "PostProcessComponent.h"
#include "CameraSystem.h"
#include "PIEManager.h"
#include "LocalizationManager.h"
#include "InputManager.h"
#include "Renderer.h"
#include "RenderPassManager.h"
#include "SceneViewportPanel.h"
#include "PixelPerfectCameraComponent.h"
#include "EditorSettingsService.h"
#include "PlatformPath.h"

EResult GameViewportPanel::OnInitialize(void* arg)
{
	CAST_DESC
	const EResult result = BaseViewportPanel::OnInitialize(static_cast<ViewportPanelDesc*>(desc));
	if (result != EResult::Success)
		return result;

	m_UseScenePostProcess = true;
	const auto& profilePath = EditorSettingsService::Get().GetProjectSettings().RenderPipelineProfilePath;
	if (profilePath.empty())
	{
		if (!m_Pipeline.Initialize(m_Name, m_RenderWidth, m_RenderHeight))
			return EResult::Fail;
	}
	else
	{
		Engine::RenderPipelineProfile profile;
		auto profileResult = Engine::LoadRenderPipelineProfile(Engine::PlatformPath::ResolveUri(profilePath), profile);
		if (profileResult)
			profileResult = m_Pipeline.Initialize(profile, m_Name, m_RenderWidth, m_RenderHeight);
		if (!profileResult)
		{
			EDITOR_LOG_ERROR("Game View render profile '{}', field '{}': {}", profilePath, profileResult.Field, profileResult.Message);
			return EResult::Fail;
		}
		m_UseScenePostProcess = m_Pipeline.UsesScenePostProcess();
	}
	if (m_UseScenePostProcess)
	{
		m_PostProcessChain.Initialize(m_Name);
		m_PostProcessChain.PrepareResources(m_RenderWidth, m_RenderHeight);
		m_PostProcessChain.GetPipeline().SaveToString(m_DefaultPostProcessSettings);
	}

	RenderPassManager& rpMgr = RenderPassManager::Get();
	m_UIOverlayPassID = rpMgr.RegisterRenderPass(
		m_Name + L"_UIOverlayPass", {}, L"",
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.f, 0.f, 0.f, -1.f), 200, ERenderSortType::FrontToBack, ERenderPassType::UI,
		EBlendMode::Opaque | EBlendMode::Masked | EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::NonPremultiplied);

	m_SelectedRTName = m_DisplayRTName = m_Pipeline.GetFinalColorRTName();
	return m_UIOverlayPassID != INVALID_PASS_ID ? EResult::Success : EResult::Fail;
}

void GameViewportPanel::Free()
{
	if (RenderPassManager::IsCreated())
		RenderPassManager::Get().RemoveRenderPass(m_UIOverlayPassID);
	m_UIOverlayPassID = INVALID_PASS_ID;
	m_PostProcessChain.Free();
	m_Pipeline.Free();
	__super::Free();
}

EResult GameViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
	const auto pipelineResult = m_Pipeline.Resize(width, height);
	const auto ppResult = m_UseScenePostProcess ? m_PostProcessChain.Resize(width, height) : EResult::Success;
	const auto channelResult = BaseViewportPanel::ResizeRenderTargets(width, height);

	if (pipelineResult != EResult::Success) return pipelineResult;
	if (ppResult != EResult::Success) return ppResult;
	return channelResult;
}

void GameViewportPanel::SubmitPostProcessPass(f32 dt, std::wstring& currentRT)
{
	Engine::Scene* activeScene = Engine::SceneManager::Get().GetCurrentScene();
	if (activeScene)
	{
		auto view = activeScene->GetRegistry().view<Engine::PostProcessComponent>();
		for (auto [entity, source] : view.each())
		{
			if (!source.IsEnabled) continue;

			const bool unchanged = m_LastPostProcessEntity == entity &&
				m_LastPostProcessPreset == source.PresetName && m_LastPostProcessSettings == source.PipelineSettings;

			if (!unchanged)
			{
				m_LastPostProcessEntity = entity;
				m_LastPostProcessPreset = source.PresetName;
				m_LastPostProcessSettings = source.PipelineSettings;

				auto& pipeline = m_PostProcessChain.GetPipeline();
				bool applied = pipeline.LoadFromString(m_DefaultPostProcessSettings);
				if (applied && !source.PipelineSettings.empty())
					applied = pipeline.LoadFromString(source.PipelineSettings);
				else if (applied && !source.PresetName.empty())
					applied = pipeline.LoadFromFile(StrToWStr("Resources/Config/PostProcess/Presets/" + source.PresetName + ".json"));

				m_PostProcessStateUsable = applied;
			}

			if (m_PostProcessStateUsable)
				currentRT = m_PostProcessChain.ExecuteChain(dt, currentRT);
			return;
		}
	}

	const bool wasDefaultState = (m_LastPostProcessEntity == entt::null);
	if (!wasDefaultState)
	{
		m_LastPostProcessEntity = entt::null;
		m_LastPostProcessPreset.clear();
		m_LastPostProcessSettings.clear();
		if (!m_DefaultPostProcessSettings.empty())
			m_PostProcessStateUsable = m_PostProcessChain.GetPipeline().LoadFromString(m_DefaultPostProcessSettings);
	}

	// 씬에 활성화된 PostProcessComponent가 없더라도 기본 톤매핑 체인을 실행
	if (m_PostProcessStateUsable)
		currentRT = m_PostProcessChain.ExecuteChain(dt, currentRT);
}

void GameViewportPanel::Update(f32 dt)
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
	if (!CalculateRenderResolution(m_PanelWidth, m_PanelHeight))
		return;

	const auto cameraBuffer = FindActiveGameCameraBuffer();
	if (auto* pass = RenderPassManager::Get().GetRenderPassByID(m_Pipeline.GetGeometryPassID()))
		pass->SetPointSpriteSampling(m_UsePixelPerfectView);
	m_HasActiveCamera = cameraBuffer.has_value();
	if (!m_HasActiveCamera) return;

	Engine::Scene* activeScene = Engine::SceneManager::Get().GetCurrentScene();
	m_Pipeline.Submit(*cameraBuffer, activeScene);
	Renderer::Get().RegisterViewportCamera(RenderPassManager::Get().GetRenderPassByID(m_UIOverlayPassID));
	std::wstring currentRT = m_Pipeline.GetFinalColorRTName();

	if (m_UseScenePostProcess)
		SubmitPostProcessPass(dt, currentRT);
	RenderPassManager::Get().GetRenderPassByID(m_UIOverlayPassID)->SetColorAttachments({ currentRT });
	m_DisplayRTName = currentRT;

	m_ChannelFilter.SubmitChannelPreviewPass(m_DisplayRTName, m_DisplayRTName);
}

void GameViewportPanel::Draw()
{
	m_GameplayImageWindowId = 0;
	m_ImageSize = ImVec2(0.0f, 0.0f);
	__super::Draw();
}

bool GameViewportPanel::HitTestGameplayImage(const vec2& screenPosition) const
{
	if (!m_Open || !m_IsVisibleOnScreen || m_GameplayImageWindowId == 0 ||
		m_ImageSize.x <= 0.0f || m_ImageSize.y <= 0.0f)
	{
		return false;
	}

	const ImVec2 point(screenPosition.x, screenPosition.y);
	const ImRect imageRect(m_ImageScreenPos,
		ImVec2(m_ImageScreenPos.x + m_ImageSize.x, m_ImageScreenPos.y + m_ImageSize.y));
	if (!imageRect.Contains(point))
		return false;

	ImGuiWindow* hoveredWindow = nullptr;
	ImGuiWindow* hoveredWindowUnderMovingWindow = nullptr;
	ImGui::FindHoveredWindowEx(point, true, &hoveredWindow, &hoveredWindowUnderMovingWindow);

	return hoveredWindow && hoveredWindow->ID == m_GameplayImageWindowId &&
		hoveredWindow->InnerClipRect.Contains(point);
}

uint32 GameViewportPanel::UpdateGameplayMouseButtons(Engine::InputManager& input, bool enabled)
{
	if (!enabled)
	{
		m_GameplayMouseButtons = 0;
		return 0;
	}

	uint32 frameButtons = 0;
	for (uint32 index = 0; index < static_cast<uint32>(Engine::EMouseButton::Count); ++index)
	{
		const auto button = static_cast<Engine::EMouseButton>(index);
		const auto bit = uint32{ 1 } << index;

		if (input.IsMouseButtonDown(button))
		{
			m_GameplayMouseButtons &= ~bit;
			const auto position = input.GetMouseButtonDownScreenPosition(button);
			if (position && HitTestGameplayImage(*position))
				m_GameplayMouseButtons |= bit;
		}

		frameButtons |= m_GameplayMouseButtons & bit;
		if (!input.IsMouseButtonPressed(button))
			m_GameplayMouseButtons &= ~bit;
	}

	return frameButtons;
}

void GameViewportPanel::DrawCustomViewport()
{
	if (m_HasActiveCamera)
	{
		DrawRenderTargetImage(m_DisplayRTName, m_UsePixelPerfectView);
		m_GameplayImageWindowId = ImGui::GetCurrentWindowRead()->ID;
		return;
	}

	const auto noCamText = LOCAL("Viewport.NoUsableCamera");
	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const ImVec2 textSize = ImGui::CalcTextSize(noCamText.c_str());
	ImGui::SetCursorPos(ImVec2((avail.x - textSize.x) * 0.5f, (avail.y - textSize.y) * 0.5f));
	ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", noCamText.c_str());
}

void GameViewportPanel::DrawCustomOptions()
{
	DrawResolutionMenu();
	DrawRenderTargetMenu();

}

std::optional<CameraBuffer> GameViewportPanel::FindActiveGameCameraBuffer()
{
	m_UsePixelPerfectView = false;
	if (m_RenderWidth == 0 || m_RenderHeight == 0)
		return std::nullopt;

	Engine::Scene* activeScene = Engine::SceneManager::Get().GetCurrentScene();
	if (!activeScene) return std::nullopt;

	activeScene->UpdateHierarchyFlags();

	auto& registry = activeScene->GetRegistry();
	auto view = registry.view<Engine::CameraComponent, Engine::WorldTransformComponent, Engine::FlagComponent>();
	const f32 aspectRatio = static_cast<f32>(m_RenderWidth) / static_cast<f32>(m_RenderHeight);

	// 메인 카메라를 먼저 검사하고, 실패하면 일반 카메라를 검사합니다.
	for (int pass = 0; pass < 2; ++pass)
	{
		const bool requireMainCamera = pass == 0;

		for (const auto entity : view)
		{
			auto& camera = view.get<Engine::CameraComponent>(entity);
			if (camera.IsMainCamera != requireMainCamera)
				continue;

			const auto& flag = view.get<Engine::FlagComponent>(entity);
			if (!flag.IsActiveInHierarchy() ||
				Engine::HasFlag(flag.effectiveFlags, Engine::EEntityFlag::Dead))
				continue;

			const f32 previousAspectRatio = camera.AspectRatio;
			camera.AspectRatio = aspectRatio;

			if (!Engine::CameraSystem::UpdateCamera(registry, entity, m_RenderWidth, m_RenderHeight))
			{
				camera.AspectRatio = previousAspectRatio;
				continue;
			}

			const auto* pixelPerfect = registry.try_get<Engine::PixelPerfectCameraComponent>(entity);
			m_UsePixelPerfectView = camera.CameraType == Engine::ECameraType::Orthographic &&
				pixelPerfect && pixelPerfect->assetsPPU > 0.0f;

			return camera.GetCameraBuffer();
		}
	}

	return std::nullopt;
}
