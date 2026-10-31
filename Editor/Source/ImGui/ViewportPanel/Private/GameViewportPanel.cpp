#include "GameViewportPanel.h"
#include "SceneManager.h"
#include "Scene.h"
#include "Components/Render/CameraComponent.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/FlagComponent.h"
#include "Components/Render/PostProcessComponent.h"
#include "CameraSystem.h"
#include "PIEManager.h"
#include "LocalizationManager.h"

void GameViewportPanel::Initialize(void* arg)
{
	__super::Initialize(arg);
	CAST_DESC

		// 뷰포트 디퍼드 파이프라인 초기화 (접두사: L"GameView")
		m_Pipeline.Initialize(m_Name, m_RenderWidth, m_RenderHeight);
	m_PostProcessChain.Initialize(m_Name);
	m_PostProcessChain.GetPipeline().SaveToString(m_DefaultPostProcessSettings);
	m_SelectedRTName = m_DisplayRTName = m_Pipeline.GetFinalColorRTName();
}

void GameViewportPanel::Free()
{
	m_PostProcessChain.Free();
	m_Pipeline.Free();
	__super::Free();
}

EResult GameViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
	const auto pipelineResult = m_Pipeline.Resize(width, height);
	const auto ppResult = m_PostProcessChain.Resize(width, height);
	const auto channelResult = BaseViewportPanel::ResizeRenderTargets(width, height);

	if (pipelineResult != EResult::Success) return pipelineResult;
	if (ppResult != EResult::Success) return ppResult;
	return channelResult;
}

void GameViewportPanel::SubmitPostProcessPass(f32 dt, std::wstring& currentRT)
{
	Engine::Scene* activeScene = Engine::SceneManager::Get().GetCurrentScene();
	if (!activeScene) return;

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

	m_LastPostProcessEntity = entt::null;
	m_LastPostProcessPreset.clear();
	m_LastPostProcessSettings.clear();
}

void GameViewportPanel::Update(f32 dt)
{
	if (!CalculateRenderResolution(m_PanelWidth, m_PanelHeight))
		return;

	const auto cameraBuffer = FindActiveGameCameraBuffer();
	m_HasActiveCamera = cameraBuffer.has_value();
	if (!m_HasActiveCamera) return;

	Engine::Scene* activeScene = Engine::SceneManager::Get().GetCurrentScene();
	m_Pipeline.Submit(*cameraBuffer, activeScene);
	std::wstring currentRT = m_Pipeline.GetFinalColorRTName();

	SubmitPostProcessPass(dt, currentRT);
	m_DisplayRTName = currentRT;

	if (m_ChannelView != EViewportChannelView::RGBA)
	{
		m_ChannelFilter.SetChannelView(m_ChannelView);
		m_ChannelFilter.SubmitChannelPreviewPass(m_DisplayRTName, m_DisplayRTName);
	}
}

void GameViewportPanel::Draw()
{
	__super::Draw();
}

void GameViewportPanel::DrawCustomViewport()
{
	if (m_HasActiveCamera)
	{
		DrawRenderTargetImage(m_DisplayRTName);
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
	DrawChannelViewButton();
}

std::optional<CameraBuffer> GameViewportPanel::FindActiveGameCameraBuffer()
{
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

			if (!Engine::CameraSystem::UpdateCamera(registry, entity))
			{
				camera.AspectRatio = previousAspectRatio;
				continue;
			}

			return camera.GetCameraBuffer();
		}
	}

	return std::nullopt;
}