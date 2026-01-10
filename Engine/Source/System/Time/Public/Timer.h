#pragma once

#include "Base.h"

BEGIN(Engine)

class ENGINE_API Timer final : public Base
{
private:
	Timer();
	virtual ~Timer() = default;
public:
	void SetTargetFPS(uint32 fps) { m_TargetDeltaTime = 1.0f / fps; }
	bool Update(f32 dt)
	{
		m_AccTime += dt;
		if (m_AccTime >= m_TargetDeltaTime)
		{
			m_AccTime -= m_TargetDeltaTime;
			return true;
		}
		return false;
	}
private:
	f32 m_AccTime = { 0.0f };
	f32 m_TargetDeltaTime = { 1.0f / 60.0f };
};

END