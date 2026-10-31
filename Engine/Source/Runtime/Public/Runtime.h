#pragma once

#include "Base.h"
#include "Renderer.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "ResourceManager.h"
#include "SystemManager.h"
#include "SceneManager.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "PipelineManager.h"
#include "SamplerManager.h"
#include "LightManager.h"
#include "LocalizationManager.h"
#include "ReflectionRegistry.h"
#include "PickingManager.h"
#include "AudioManager.h"
#include "ModuleManager.h"

BEGIN(Engine)
typedef struct RuntimeCreateInfo
{
	RendererDesc RendererDesc;
} RUNTIMEDESC;

class ENGINE_API Runtime : public Base
{
	DECLARE_SINGLETON(Runtime)
#pragma region Constructor&Destructor
private:
	Runtime() {}
	virtual ~Runtime() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	// 호스트가 고정 스텝의 크기와 실행 횟수를 결정합니다.
	void FixedUpdate(f32 dt);
	// 호스트에서 입력을 먼저 갱신했다면 같은 입력 샘플을 재사용합니다.
	void Update(f32 dt, bool updateInput = true);
	void LateUpdate(f32 dt);
	EResult Render(f32 dt);
#pragma endregion

#pragma region Variables
private:
	entt::registry m_GlobalRegistry;
	ReflectionRegistry* m_ReflectionRegistry = { nullptr };
	TimeManager* m_TimeManager = { nullptr };
	InputManager* m_InputManager = { nullptr };
	Renderer* m_Renderer = { nullptr };
	RenderPassManager* m_RenderPassManager = { nullptr };
	RenderTargetManager* m_RenderTargetManager = { nullptr };
	PipelineManager* m_PipelineManager = { nullptr };
	SamplerManager* m_SamplerManager = { nullptr };
	LightManager* m_LightManager = { nullptr };
	ResourceManager* m_ResourceManager = { nullptr };
	SceneManager* m_SceneManager = { nullptr };
	SystemManager* m_SystemManager = { nullptr };
	LocalizationManager* m_LocalizationManager = { nullptr };
	PickingManager* m_PickingManager = { nullptr };
	AudioManager* m_AudioManager = { nullptr };
	ModuleManager* m_ModuleManager = { nullptr };
#pragma endregion
};

END
