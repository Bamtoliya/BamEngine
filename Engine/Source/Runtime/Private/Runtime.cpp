#pragma once
#include "Runtime.h"
#include "IBLManager.h"
#include "Logger.h"
#include "RuntimeSettingsManager.h"
#include "ActionCombatHelpers.h"
#if BAM_EDITOR
#include <chrono>
#endif

IMPLEMENT_SINGLETON(Runtime)

#pragma region Constructor&Destructor
EResult Runtime::Initialize(void* arg)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	if (!arg)
		return EResult::InvalidArgument;

	const auto* runtimeDesc = static_cast<const RUNTIMEDESC*>(arg);
	if (!runtimeDesc->RendererDesc.rhiDesc)
		return EResult::InvalidArgument;

	m_ProjectRoot = runtimeDesc->ProjectRoot.empty() ? PlatformPath::GetExecutableDirectory() : PlatformPath::Normalize(runtimeDesc->ProjectRoot);
	m_AssetRoot = runtimeDesc->AssetRoot.empty() ? "Resources" : PlatformPath::Normalize(runtimeDesc->AssetRoot);
	m_EngineResourceRoot = runtimeDesc->EngineResourceRoot.empty() ? "Engine/Resources" : PlatformPath::Normalize(runtimeDesc->EngineResourceRoot);

	PlatformPath::SetProjectRoot(m_ProjectRoot);
	PlatformPath::SetProjectAssetRoot(m_AssetRoot.is_absolute() ? m_AssetRoot : m_ProjectRoot / m_AssetRoot);
	PlatformPath::SetEngineResourceRoot(m_EngineResourceRoot.is_absolute() ? m_EngineResourceRoot : m_ProjectRoot / m_EngineResourceRoot);

	RendererDesc rendererDesc = runtimeDesc->RendererDesc;

	const auto fail = [this](const char* stage)
		{
			BAM_LOG(Error, "Engine","Runtime", "{} creation failed", stage);
			Free();
			return EResult::Fail;
		};

	const auto create = []<typename T, typename... Args>(T * &target, Args&&... args)
	{
		target = T::Create(std::forward<Args>(args)...);
		return target != nullptr;
	};

	if (!create(m_ReflectionRegistry)) return fail("ReflectionRegistry");
	if (!create(m_ModuleManager)) return fail("ModuleManager");
	if (!create(m_TimeManager)) return fail("TimeManager");
	if (!create(m_InputManager)) return fail("InputManager");
	if (!create(m_AudioManager)) return fail("AudioManager");
	if (!create(m_ResourceManager)) return fail("ResourceManager");
	if (!create(m_ResourceStreamer)) return fail("ResourceStreamer");
	if (!create(m_SceneManager)) return fail("SceneManager");
	if (!create(m_SystemManager)) return fail("SystemManager");
	if (!create(m_LocalizationManager)) return fail("LocalizationManager");
	if (!create(m_RenderTargetManager)) return fail("RenderTargetManager");
	if (!create(m_RenderPassManager)) return fail("RenderPassManager");

	if (!create(m_Renderer, &rendererDesc)) return fail("Renderer");

	PipelineManagerDesc pipelineDesc = {};
	pipelineDesc.rhi = m_Renderer->GetRHI();
	if (!create(m_PipelineManager, &pipelineDesc)) return fail("PipelineManager");

	if (!create(m_SamplerManager, m_Renderer->GetRHI())) return fail("SamplerManager");

	LightManagerDesc lightDesc = {};
	lightDesc.RHI = m_Renderer->GetRHI();
	if (!create(m_LightManager, &lightDesc)) return fail("LightManager");

	RaytracingSceneManagerDesc raytracingDesc = {};
	raytracingDesc.RHI = m_Renderer->GetRHI();
	if (!create(m_RaytracingSceneManager, &raytracingDesc)) return fail("RaytracingSceneManager");

	if (!create(m_PickingManager)) return fail("PickingManager");

	if (!RuntimeSettingsManager::IsCreated())
		RuntimeSettingsManager::Create();
	if (RuntimeSettingsManager::IsCreated())
		RuntimeSettingsManager::Get().BindRuntime(m_TimeManager, m_LocalizationManager, m_AudioManager, m_Renderer->GetRHI());

	return EResult::Success;
}

void Runtime::Free()
{
	if (RuntimeSettingsManager::IsCreated())
		RuntimeSettingsManager::Get().UnbindRuntime();
	// Runtime::Initialize에서 생성한 싱글톤이므로 여기서 해제하지 않으면 종료 시 누수로 보고된다.
	RuntimeSettingsManager::Destroy();

	// ── 0. 씬(Scene)을 참조하는 프레임 큐 및 피킹 타겟 정리 ──
	if (m_Renderer)
	{
		m_Renderer->ClearAllRenderQueues();
	}
	// 파일을 읽는 작업 스레드를 씬과 모듈보다 먼저 멈춘다.
	Safe_Destroy(m_ResourceStreamer);
	Safe_Destroy(m_PickingManager);
	// ── 1. 씬 (모든 외부 참조가 해제되어 SceneManager에서 즉시 소멸됨) ──
	Safe_Destroy(m_SceneManager);
	// ── 2. 동적 모듈 (씬 엔티티 소멸 후 언로드 - SystemManager가 살아있으므로 모듈 시스템 정상 해제) ──
	Safe_Destroy(m_ModuleManager);
	// ── 3. 시스템 (모듈 시스템 해제 완료 후 남아있는 엔진 내장 시스템 해제) ──
	Safe_Destroy(m_SystemManager);
	// ── 4. 게임 로직 (RHI 무관) ──
	Safe_Destroy(m_TimeManager);
	Safe_Destroy(m_InputManager);
	Safe_Destroy(m_LocalizationManager);
	// ── 5. 리소스 (RHI 리소스: Mesh, Texture, Shader) ──
	if (m_ResourceManager)
		IBLManager::Get().Free();
	Safe_Destroy(m_ResourceManager);
	Safe_Destroy(m_AudioManager);
	// ── 6. 렌더링 인프라 (RHI 리소스) ──
	// 가속 구조 관리자는 메시 정점·인덱스 버퍼 참조를 보유하므로 RHI보다 먼저 해제한다.
	Safe_Destroy(m_RaytracingSceneManager);
	Safe_Destroy(m_LightManager);
	Safe_Destroy(m_SamplerManager);
	Safe_Destroy(m_PipelineManager);
	Safe_Destroy(m_RenderTargetManager);
	Safe_Destroy(m_RenderPassManager);
	// ── 7. RHI (GPU Device) — 가장 마지막 ──
	Safe_Destroy(m_Renderer);
	// ── 8. Reflection ──
	Safe_Destroy(m_ReflectionRegistry);
}
#pragma endregion

#pragma region Loop
void Runtime::FixedUpdate(f32 dt)
{
	SystemManager::Get().FixedUpdate(m_GlobalRegistry, SceneManager::Get().GetActiveScenes(), dt);
	if (m_InputManager) m_InputManager->ConsumeFixedStepActions();
}
void Runtime::Update(f32 dt, bool updateInput, uint8 phaseMask)
{
	if (updateInput) InputManager::Get().Update(dt);
	auto& systems = SystemManager::Get();
#if BAM_EDITOR
	const bool measure = systems.IsUpdateTimingRequested();
	using Clock = std::chrono::steady_clock;
	const auto audioStart = measure ? Clock::now() : Clock::time_point{};
#endif
	AudioManager::Get().Update(dt);
#if BAM_EDITOR
	const auto scenesStart = measure ? Clock::now() : Clock::time_point{};
#endif
	const auto& activeScenes = SceneManager::Get().GetActiveScenes();
#if BAM_EDITOR
	if (measure)
	{
		using Milliseconds = std::chrono::duration<double, std::milli>;
		BAM_LOG(Info, "Engine", "UpdateTiming", "First PIE update: FMOD update={:.2f} ms, active scenes={:.2f} ms",
			Milliseconds(scenesStart - audioStart).count(), Milliseconds(Clock::now() - scenesStart).count());
	}
#endif
	const f32 hitstopScale = CombatEffectService::Update(dt);
	const f32 scaledDt = dt * hitstopScale;
	systems.Update(m_GlobalRegistry, activeScenes, scaledDt, phaseMask);
}
void Runtime::LateUpdate(f32 dt)
{
	const auto activeScenes = SceneManager::Get().GetActiveScenes();

	for (Scene* scene : activeScenes)
	{
		if (scene)
			scene->UpdateHierarchyFlags();
	}

	SystemManager::Get().LateUpdate(m_GlobalRegistry, activeScenes, dt);
}
EResult Runtime::Render(f32 dt)
{
	if (m_ResourceManager)
		m_ResourceManager->FlushDeferredDeletes();

	auto& renderer = Renderer::Get();
	auto& picking = PickingManager::Get();
	auto* rhi = renderer.GetRHI();

	if (!rhi)
		return EResult::Fail;

	rhi->BeginMetricsFrame();
	picking.BeginFrame();

	// 실패로 일찍 반환할 때도 이번 렌더 시도의 통계를 확정합니다.
	const auto finish = [&rhi, &picking](EResult result)
		{
			picking.EndFrame(result == EResult::Success);
			rhi->EndMetricsFrame(result == EResult::Success);
			
			return result;
		};

	const EResult beginResult = renderer.BeginFrame();
	if (IsFailure(beginResult))
	{
		ENGINE_LOG_ERROR("Renderer BeginFrame failed");
		renderer.ClearFrameQueues();
		return finish(beginResult);
	}

	// 업로드를 이번 프레임 명령에 기록하고, 이어지는 제출에서 다음 요청을 모은다.
	if (m_ResourceStreamer) m_ResourceStreamer->Update();

	SystemManager::Get().Sumbit(m_GlobalRegistry, SceneManager::Get().GetActiveScenes(), dt);

	const EResult renderResult = renderer.Render(dt);
	const EResult endResult = renderer.EndFrame();

	if (IsFailure(renderResult))
		ENGINE_LOG_ERROR("Renderer Render failed");

	if (IsFailure(endResult))
	{
		ENGINE_LOG_ERROR("Renderer EndFrame failed");
		return finish(endResult);
	}

	if (renderResult != EResult::Success)
		return finish(renderResult);

	m_LightManager->Update(dt);

	return finish(EResult::Success);
}
#pragma endregion
