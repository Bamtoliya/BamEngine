#include "GameViewportPanel.h"
#include "SceneManager.h"
#include "Scene.h"
#include "Components/Render/CameraComponent.h"
#include "Components/Core/TransformComponent.h"
#include "Components/Core/WorldTransformComponent.h"
#include "Components/Core/FlagComponent.h"
#include "CameraSystem.h"
#include "PIEManager.h"

void GameViewportPanel::Initialize(void* arg)
{
	__super::Initialize(arg);
	CAST_DESC

		// 뷰포트 디퍼드 파이프라인 초기화 (접두사: L"GameView")
		m_Pipeline.Initialize(m_Name, m_RenderWidth, m_RenderHeight);
	m_SelectedRTName = m_DisplayRTName = m_Pipeline.GetFinalColorRTName();
}

void GameViewportPanel::Free()
{
	m_Pipeline.Free();
	__super::Free();
}

void GameViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
	__super::ResizeRenderTargets(width, height);
	m_Pipeline.Resize(width, height);
}

void GameViewportPanel::Update(f32 dt)
{
	CalculateRenderResolution(m_PanelWidth, m_PanelHeight);

	// 1. 씬 내의 인게임 메인 카메라(IsMainCamera) 탐색
	const auto cameraBuffer = FindActiveGameCameraBuffer();
	m_HasActiveCamera = cameraBuffer.has_value();

	// 2. 인게임 카메라가 존재하면 전용 파이프라인으로 디퍼드 렌더링 제출
	if (m_HasActiveCamera)
	{
		Engine::Scene* activeScene = Engine::SceneManager::Get().GetCurrentScene();
		m_Pipeline.Submit(*cameraBuffer, activeScene);
		m_DisplayRTName = m_Pipeline.GetFinalColorRTName();
	}

	// 3. 채널 뷰 필터 (RGBA 외 채널 확인 시)
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
	}
	else
	{
		// 씬에 활성화된 메인 카메라가 없을 때 안내 표시
		ImVec2 avail = ImGui::GetContentRegionAvail();
		const char* noCamText = "No active [CameraComponent] with [IsMainCamera] in Scene";
		ImVec2 textSize = ImGui::CalcTextSize(noCamText);
		ImGui::SetCursorPos(ImVec2((avail.x - textSize.x) * 0.5f, (avail.y - textSize.y) * 0.5f));
		ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", noCamText);
	}
}

void GameViewportPanel::DrawCustomOptions()
{
	DrawResolutionMenu();
	DrawRenderTargetMenu();
	DrawChannelViewButton();
}

std::optional<CameraBuffer> GameViewportPanel::FindActiveGameCameraBuffer()
{
	Engine::Scene* activeScene = Engine::SceneManager::Get().GetCurrentScene();
	if (!activeScene) return std::nullopt;

	auto& registry = activeScene->GetRegistry();
	auto view = registry.view<Engine::CameraComponent, Engine::WorldTransformComponent, Engine::FlagComponent>();

	Engine::CameraComponent* targetCamera = nullptr;
	for (auto entity : view)
	{
		const auto& flag = view.get<Engine::FlagComponent>(entity);
		if (!Engine::HasFlag(flag.flags, Engine::EEntityFlag::Active)) continue;

		auto& cam = view.get<Engine::CameraComponent>(entity);
		if (cam.IsMainCamera)
		{
			targetCamera = &cam;
			break;
		}
		if (!targetCamera) targetCamera = &cam; // 메인 지정이 없으면 첫 번째 활성 카메라 사용
	}

	if (!targetCamera) return std::nullopt;

	// GameView 해상도 종횡비에 맞게 카메라 버퍼 갱신
	if (m_RenderHeight > 0)
	{
		targetCamera->AspectRatio = static_cast<f32>(m_RenderWidth) / static_cast<f32>(m_RenderHeight);
		Engine::CameraSystem::UpdateCameras(registry);
	}

	return targetCamera->GetCameraBuffer();
}