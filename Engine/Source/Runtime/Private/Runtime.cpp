#pragma once
#include "Runtime.h"

IMPLEMENT_SINGLETON(Runtime)

#pragma region Constructor&Destructor
EResult Runtime::Initialize(void* arg)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	RUNTIMEDESC* pRuntimeDesc = reinterpret_cast<RUNTIMEDESC*>(arg);
	RENDERERDESC RendererDesc = pRuntimeDesc->RendererDesc;

	m_Renderer = Renderer::Create(&RendererDesc);
	if (!m_Renderer) return EResult::Fail;
	m_TimeManager = TimeManager::Create();
	if (!m_TimeManager) return EResult::Fail;
	return EResult::Success;
}

void Runtime::Free()
{
	Renderer::Destroy();
	TimeManager::Destroy();
}
#pragma endregion

#pragma region Loop
void Runtime::RunFrame(f32 dt)
{
	FixedUpdate(dt);
	Update(dt);
	LateUpdate(dt);
	Render();
}
void Runtime::FixedUpdate(f32 dt)
{
	// Fixed Update Logic Here
}
void Runtime::Update(f32 dt)
{

}
void Runtime::LateUpdate(f32 dt)
{
	// Late Update Logic Here
}
EResult Runtime::Render()
{
	if (IsFailure(Renderer::Get().BeginFrame()))
	{
		fmt::print(stderr, "Renderer BeginFrame Failed\n");
	}

	// Rendering Logic Here


	if (IsFailure(Renderer::Get().EndFrame()))
	{
		fmt::print(stderr, "Renderer EndFrame Failed\n");
	}
	return EResult::Success;
}
#pragma endregion
