#pragma once
#include "Runtime.h"


#ifdef _DEBUG
#include "GameObject.h"
#include "Transform.h"
#include "MeshRenderer.h"
#include "Mesh.h"
#include "SDLShader.h"
#endif

IMPLEMENT_SINGLETON(Runtime)

#pragma region Constructor&Destructor
EResult Runtime::Initialize(void* arg)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	RUNTIMEDESC* pRuntimeDesc = reinterpret_cast<RUNTIMEDESC*>(arg);
	tagRendererDesc RendererDesc = pRuntimeDesc->RendererDesc;

	if (!ReflectionRegistry::Create()) return EResult::Fail;
	InitReflectionSystem();

	m_ComponentRegistry = ComponentRegistry::Create();
	if (!m_ComponentRegistry) return EResult::Fail;

	m_TimeManager = TimeManager::Create();
	if (!m_TimeManager) return EResult::Fail;

	m_ResourceManager = ResourceManager::Create();
	if (!m_ResourceManager) return EResult::Fail;
	m_PrototypeManager = PrototypeManager::Create();
	if (!m_PrototypeManager) return EResult::Fail;

	m_LayerManager = LayerManager::Create();
	if (!m_LayerManager) return EResult::Fail;
	m_SceneManager = SceneManager::Create();
	if (!m_SceneManager) return EResult::Fail;

	m_LocalizationManager = LocalizationManager::Create();
	if (!m_LocalizationManager) return EResult::Fail;

	m_RenderTargetManager = RenderTargetManager::Create();
	if (!m_RenderTargetManager) return EResult::Fail;
	m_RenderPassManager = RenderPassManager::Create();
	if (!m_RenderPassManager) return EResult::Fail;
	m_Renderer = Renderer::Create(&RendererDesc);
	if (!m_Renderer) return EResult::Fail;
	m_CameraManager = CameraManager::Create();
	if (!m_CameraManager) return EResult::Fail;

	tagPipelineManagerDesc pipelineDesc = {};
	pipelineDesc.rhi = m_Renderer->GetRHI();
	m_PipelineManager = PipelineManager::Create(&pipelineDesc);
	if (!m_PipelineManager) return EResult::Fail;
	m_SamplerManager = SamplerManager::Create(m_Renderer->GetRHI());
	if (!m_SamplerManager) return EResult::Fail;

	m_LocalizationManager->LoadData();
	return EResult::Success;
}

void Runtime::Free()
{
	TimeManager::Destroy();

	ResourceManager::Destroy();
	PrototypeManager::Destroy();

	LayerManager::Destroy();
	SceneManager::Destroy();

	ComponentRegistry::Destroy();
	ReflectionRegistry::Destroy();
	LocalizationManager::Destroy();

	CameraManager::Destroy();
	SamplerManager::Destroy();
	PipelineManager::Destroy();
	RenderTargetManager::Destroy();
	RenderPassManager::Destroy();
	Renderer::Destroy();
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
	SceneManager::Get().FixedUpdate(dt);
}
void Runtime::Update(f32 dt)
{
	SceneManager::Get().Update(dt);
}
void Runtime::LateUpdate(f32 dt)
{
	SceneManager::Get().LateUpdate(dt);
}
EResult Runtime::Render(f32 dt)
{
	if (IsFailure(Renderer::Get().BeginFrame()))
	{
		fmt::print(stderr, "Renderer BeginFrame Failed\n");
	}

	if (IsFailure(Renderer::Get().Render(dt)))
	{
		fmt::print(stderr, "Renderer Render Failed\n");
	}

	if (IsFailure(Renderer::Get().EndFrame()))
	{
		fmt::print(stderr, "Renderer EndFrame Failed\n");
	}
	return EResult::Success;
}
#pragma endregion
