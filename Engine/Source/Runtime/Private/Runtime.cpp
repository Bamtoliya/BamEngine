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
			BAM_LOG(Error, "Runtime", "{} creation failed", stage);
			Free();
			return EResult::Fail;
		};

	m_ComponentRegistry = ComponentRegistry::Create();
	if (!m_ComponentRegistry) return fail("ComponentRegistry");

	m_TimeManager = TimeManager::Create();
	if (!m_TimeManager) return fail("TimeManager");

	m_InputManager = InputManager::Create();
	if (!m_InputManager) return fail("InputManager");

	m_ResourceManager = ResourceManager::Create();
	if (!m_ResourceManager) return fail("ResourceManager");

	m_PrototypeManager = PrototypeManager::Create();
	if (!m_PrototypeManager) return fail("PrototypeManager");

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

	m_CameraManager = CameraManager::Create();
	if (!m_CameraManager) return fail("CameraManager");

	PipelineManagerDesc pipelineDesc = {};
	pipelineDesc.rhi = m_Renderer->GetRHI();
	m_PipelineManager = PipelineManager::Create(&pipelineDesc);
	if (!m_PipelineManager) return fail("PipelineManager");

	m_SamplerManager = SamplerManager::Create(m_Renderer->GetRHI());
	if (!m_SamplerManager) return fail("SamplerManager");

	tagLightManagerDesc lightDesc = {};
	lightDesc.RHI = m_Renderer->GetRHI();
	m_LightManager = LightManager::Create(&lightDesc);
	if (!m_LightManager) return fail("LightManager");

	m_CollisionManager = CollisionManager::Create();
	if (!m_CollisionManager) return fail("CollisionManager");

	return EResult::Success;
}

void Runtime::Free()
{
	
	// ── 1. 게임 로직 (RHI 무관) ──
	Safe_Destroy(m_TimeManager);
	Safe_Destroy(m_InputManager);
	Safe_Destroy(m_LocalizationManager);
	Safe_Destroy(m_ComponentRegistry);

	// ── 2. 씬 (Component가 Pipeline/Buffer 참조) ──
	Safe_Destroy(m_SceneManager);
	Safe_Destroy(m_SystemManager);
	Safe_Destroy(m_PrototypeManager);
	Safe_Destroy(m_CollisionManager);

	// ── 3. 리소스 (RHI 리소스: Mesh, Texture, Shader) ──
	Safe_Destroy(m_CameraManager);
	Safe_Destroy(m_ResourceManager);

	// ── 4. 렌더링 인프라 (RHI 리소스) ──
	Safe_Destroy(m_LightManager);
	Safe_Destroy(m_SamplerManager);
	Safe_Destroy(m_PipelineManager);
	Safe_Destroy(m_RenderTargetManager);
	Safe_Destroy(m_RenderPassManager);

	// ── 5. RHI (GPU Device) — 가장 마지막 ──
	Safe_Destroy(m_Renderer);
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
void Runtime::Update(f32 dt)
{
	InputManager::Get().Update(dt);
	SystemManager::Get().Update(m_GlobalRegistry, SceneManager::Get().GetActiveScenes(), dt);
}
void Runtime::LateUpdate(f32 dt)
{
	SystemManager::Get().LateUpdate(m_GlobalRegistry, SceneManager::Get().GetActiveScenes(), dt);
}
EResult Runtime::Render(f32 dt)
{
	auto& renderer = Renderer::Get();
	auto* rhi = renderer.GetRHI();

	if (!rhi)
		return EResult::Fail;

	rhi->BeginMetricsFrame();

	// 실패로 일찍 반환할 때도 이번 렌더 시도의 통계를 확정합니다.
	const auto finish = [rhi](EResult result)
		{
			rhi->EndMetricsFrame(result == EResult::Success);
			return result;
		};

	if (IsFailure(renderer.BeginFrame()))
	{
		ENGINE_LOG_ERROR("Renderer BeginFrame failed");
		return finish(EResult::Fail);
	}

	SystemManager::Get().Sumbit(
		m_GlobalRegistry,
		SceneManager::Get().GetActiveScenes(),
		dt);

	if (IsFailure(renderer.Render(dt)))
	{
		ENGINE_LOG_ERROR("Renderer Render failed");
		return finish(EResult::Fail);
	}

	if (IsFailure(renderer.EndFrame()))
	{
		ENGINE_LOG_ERROR("Renderer EndFrame failed");
		return finish(EResult::Fail);
	}

	m_LightManager->Update(dt);

	return finish(EResult::Success);
}
#pragma endregion
