#pragma once

#include "Base.h"
#include "Renderer.h"
#include "TimeManager.h"

BEGIN(Engine)

typedef struct tagRuntimeCreateInfo
{
	RENDERERDESC RendererDesc;
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
	EResult BeginFrame();
	EResult Render();
	EResult EndFrame();
private:
	void FixedUpdate(f32 dt);
	void Update(f32 dt);
	void LateUpdate(f32 dt);
#pragma endregion

#pragma region Variables
private:
	Renderer* m_Renderer = { nullptr };
	TimeManager* m_TimeManager = { nullptr };
#pragma endregion
};

END