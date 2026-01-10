#pragma once
#include "Runtime.h"
#include "TimeManager.h"

IMPLEMENT_SINGLETON(Runtime)

#pragma region Constructor&Destructor
EResult Runtime::Initialize(void* arg)
{
#if defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	TimeManager::Create();
	return EResult::Success;
}

void Runtime::Free()
{
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
	return EResult::Success;
}
#pragma endregion
