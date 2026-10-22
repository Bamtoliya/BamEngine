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

	m_ReflectionRegistry = ReflectionRegistry::Create();
	if (!m_ReflectionRegistry) return fail("ReflectionRegistry");

	m_ModuleManager = ModuleManager::Create();
	if (!m_ModuleManager) return fail("ModuleManager");

	m_TimeManager = TimeManager::Create();
	if (!m_TimeManager) return fail("TimeManager");

	m_InputManager = InputManager::Create();
	if (!m_InputManager) return fail("InputManager");

	m_AudioManager = AudioManager::Create();
	if (!m_AudioManager) return fail("AudioManager");

	m_ResourceManager = ResourceManager::Create();
	if (!m_ResourceManager) return fail("ResourceManager");

	m_SceneManager = SceneManager::Create();
	if (!m_SceneManager) return fail("SceneManager");

	m_SystemManager = SystemManager::Create();
	if (!m_SystemManager) return fail("SystemManager");

	m_LocalizationManager = LocalizationManager::Create();
	if (!m_LocalizationManager) return fail("LocalizationManager");

	m_RenderTargetManager = RenderTargetManager::Create();
	if (!m_RenderTargetManager) return fail("RenderTargetManager");

	m_RenderPassManager = RenderPassManager::Create();
	if (!m_RenderPassManager) return fail("RenderPassManager");

	m_Renderer = Renderer::Create(&rendererDesc);
	if (!m_Renderer) return fail("Renderer");

	PipelineManagerDesc pipelineDesc = {};
	pipelineDesc.rhi = m_Renderer->GetRHI();
	m_PipelineManager = PipelineManager::Create(&pipelineDesc);
	if (!m_PipelineManager) return fail("PipelineManager");

	m_SamplerManager = SamplerManager::Create(m_Renderer->GetRHI());
	if (!m_SamplerManager) return fail("SamplerManager");

	LightManagerDesc lightDesc = {};
	lightDesc.RHI = m_Renderer->GetRHI();
	m_LightManager = LightManager::Create(&lightDesc);
	if (!m_LightManager) return fail("LightManager");

	m_PickingManager = PickingManager::Create();
	if (!m_PickingManager) return fail("PickingManager");

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
void Runtime::RunFrame(f32 dt)
{	
	FixedUpdate(dt);
	Update(dt);
	LateUpdate(dt);
	Render(dt);
	
}
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
	SystemManager::Get().LateUpdate(m_GlobalRegistry, SceneManager::Get().GetActiveScenes(), dt);
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
