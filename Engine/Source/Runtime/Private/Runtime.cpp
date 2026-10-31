#pragma once
#include "Runtime.h"
#include "Logger.h"

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

	if (!create(m_PickingManager)) return fail("PickingManager");

	return EResult::Success;
}

void Runtime::Free()
{
	// Frame snapshots/commands retain scenes and UI assets; release them before their managers.
	if (m_Renderer)
	{
		m_Renderer->ClearAllRenderQueues();
	}

	Safe_Destroy(m_ModuleManager);
	
	// ── 1. 게임 로직 (RHI 무관) ──
	Safe_Destroy(m_TimeManager);
	Safe_Destroy(m_InputManager);
	Safe_Destroy(m_LocalizationManager);
	Safe_Destroy(m_PickingManager);
	Safe_Destroy(m_AudioManager);

	// ── 2. 씬 (Component가 Pipeline/Buffer 참조) ──
	Safe_Destroy(m_SceneManager);
	Safe_Destroy(m_SystemManager);

	// ── 3. 리소스 (RHI 리소스: Mesh, Texture, Shader) ──
	Safe_Destroy(m_ResourceManager);

	// ── 4. 렌더링 인프라 (RHI 리소스) ──
	Safe_Destroy(m_LightManager);
	Safe_Destroy(m_SamplerManager);
	Safe_Destroy(m_PipelineManager);
	Safe_Destroy(m_RenderTargetManager);
	Safe_Destroy(m_RenderPassManager);

	// ── 5. RHI (GPU Device) — 가장 마지막 ──
	Safe_Destroy(m_Renderer);

	// ── 6. Reflection ──
	Safe_Destroy(m_ReflectionRegistry);
}
#pragma endregion

#pragma region Loop
void Runtime::FixedUpdate(f32 dt)
{
	SystemManager::Get().FixedUpdate(m_GlobalRegistry, SceneManager::Get().GetActiveScenes(), dt);
	
}
void Runtime::Update(f32 dt, bool updateInput)
{
	if (updateInput) InputManager::Get().Update(dt);
	AudioManager::Get().Update(dt);

	SystemManager::Get().Update(m_GlobalRegistry, SceneManager::Get().GetActiveScenes(), dt);
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
