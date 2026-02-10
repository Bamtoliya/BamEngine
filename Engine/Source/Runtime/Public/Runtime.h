#pragma once

#include "Base.h"

#include "TimeManager.h"
#include "ResourceManager.h"
#include "PrototypeManager.h"

#include "LayerManager.h"
#include "SceneManager.h"

#include "Renderer.h"
#include "RenderPassManager.h"
#include "RenderTargetManager.h"
#include "PipelineManager.h"
#include "SamplerManager.h"

#include "ComponentRegistry.h"

#include "LocalizationManager.h"

BEGIN(Engine)

typedef struct tagRuntimeCreateInfo
{
	tagRendererDesc RendererDesc;
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
	void RunFrame(f32 dt);
private:
	void FixedUpdate(f32 dt);
	void Update(f32 dt);
	void LateUpdate(f32 dt);
	EResult Render(f32 dt);
#pragma endregion

#pragma region Variables
private:
	TimeManager* m_TimeManager = { nullptr };
private:
	Renderer* m_Renderer = { nullptr };
	RenderPassManager* m_RenderPassManager = { nullptr };
	RenderTargetManager* m_RenderTargetManager = { nullptr };
	PipelineManager* m_PipelineManager = { nullptr };
	SamplerManager* m_SamplerManager = { nullptr };
private:
	ResourceManager* m_ResourceManager = { nullptr };
	PrototypeManager* m_PrototypeManager = { nullptr };
private:
	LayerManager* m_LayerManager = { nullptr };
	SceneManager* m_SceneManager = { nullptr };
private:
	ComponentRegistry* m_ComponentRegistry = { nullptr };
private:
	LocalizationManager* m_LocalizationManager = { nullptr };
#pragma endregion
};

END