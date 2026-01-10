#pragma once

#include "Base.h"

BEGIN(Engine)

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
	EResult Render();
private:
	void FixedUpdate(f32 dt);
	void Update(f32 dt);
	void LateUpdate(f32 dt);
#pragma endregion

#pragma region Variables

#pragma endregion
};

END