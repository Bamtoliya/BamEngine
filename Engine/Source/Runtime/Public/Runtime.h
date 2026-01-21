#pragma once

#include "Base.h"
#include "Renderer.h"
#include "TimeManager.h"
#include "ResourceManager.h"
#include "PrototypeManager.h"

#include "LayerManager.h"
#include "SceneManager.h"

#include "RenderPassManager.h"

BEGIN(Engine)

typedef struct tagRuntimeCreateInfo
{
	Renderer::RENDERERDESC RendererDesc;
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
private:
	ResourceManager* m_ResourceManager = { nullptr };
	PrototypeManager* m_PrototypeManager = { nullptr };
private:
	LayerManager* m_LayerManager = { nullptr };
	SceneManager* m_SceneManager = { nullptr };
#pragma endregion
};

END