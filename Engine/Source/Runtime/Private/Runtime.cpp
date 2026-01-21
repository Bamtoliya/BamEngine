#pragma once
#include "Runtime.h"

#include "GameObject.h"
#include "MeshRenderer.h"
#include "Mesh.h"

#include "LayerManager.h"
#include "SceneManager.h"

IMPLEMENT_SINGLETON(Runtime)

#pragma region Constructor&Destructor
EResult Runtime::Initialize(void* arg)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	RUNTIMEDESC* pRuntimeDesc = reinterpret_cast<RUNTIMEDESC*>(arg);
	Renderer::RENDERERDESC RendererDesc = pRuntimeDesc->RendererDesc;

	m_Renderer = Renderer::Create(&RendererDesc);
	if (!m_Renderer) return EResult::Fail;
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

	return EResult::Success;
}

void Runtime::Free()
{
	OnUIRender.Clear();
	Renderer::Destroy();
	TimeManager::Destroy();
	ResourceManager::Destroy();
	PrototypeManager::Destroy();
	LayerManager::Destroy();
	SceneManager::Destroy();
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

	// Rendering Logic Here

	SceneManager::Get().Render(dt);

	if (OnUIRender.IsBound())
	{
		OnUIRender.Broadcast(dt);
	}


	if (IsFailure(Renderer::Get().EndFrame()))
	{
		fmt::print(stderr, "Renderer EndFrame Failed\n");
	}
	return EResult::Success;
}
#pragma endregion
