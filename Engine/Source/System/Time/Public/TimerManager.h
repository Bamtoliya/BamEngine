#pragma once

#include "Timer.h"
#include "Base.h"

BEGIN(Engine)

class ENGINE_API TimerManager final : public Base
{
	DECLARE_SINGLETON(TimerManager)
private:
	TimerManager() {}
	virtual ~TimerManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
public:
	void Update(f32 dt);
private:
};
END