#pragma once

#include "Base.h"
#include <chrono>

BEGIN(Engine)

class ENGINE_API TimeManager final : public Base
{
DECLARE_SINGLETON(TimeManager)
#pragma region Constructor&Destructor
private:
	TimeManager() {}
	virtual ~TimeManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	void Update();
#pragma endregion

#pragma region Getter
public:
	f32		GetDeltaTime()		const { return m_DeltaTime; }
	f64		GetTotalTime()		const { return m_TotalTime; }
	f32 	GetFixedStep()		const { return m_FixedStep; }
	uint32	GetTargetFPS()		const { return m_TargetFPS; }
	uint32	GetFPS()			const { return m_FPS; }
#pragma endregion

#pragma region Setter
public:
	void	SetTargetFPS(uint32 fps)	{ m_TargetFPS = fps; }
	void	SetFixedStep(f32 step)		{ m_FixedStep = step; }
#pragma endregion

#pragma region Helper
	bool	IsFixedUpdateReady();
#pragma endregion


#pragma region Variables
private:
	chrono::steady_clock::time_point m_LastTime = std::chrono::steady_clock::now();

	//Time
	f32		m_DeltaTime = { 0.f };
	f64		m_TotalTime = { 0.0 };

	//FPS
	uint32	m_FrameCount	= { 0 };
	uint32	m_TargetFPS		= { 60 };
	uint32	m_FPS			= { 0 };
	f32		m_FPSTimer = { 0.f };

	//Fixed Step
	f32		m_FixedStep = { 0.f };
	f64 	m_AccTime	= { 0.0 };
#pragma endregion
};

END