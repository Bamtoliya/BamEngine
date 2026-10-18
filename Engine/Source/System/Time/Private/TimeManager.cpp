#pragma once
#include "TimeManager.h"

#define MAX_ACCUMULATED_TIME 0.5f

IMPLEMENT_SINGLETON(TimeManager)

#pragma region Constructor&Destructor
EResult TimeManager::Initialize(void* arg)
{
    m_LastTime = std::chrono::steady_clock::now();
    m_DeltaTime = 0.0f;
    m_TotalTime = 0.0;

    m_TargetFPS = 0;
    m_FixedStep = 1.0f / 60.0f;

    m_AccTime = 0.0;

    m_FPS = 0;
    m_FrameCount = 0;
    m_FPSTimer = 0.0;

    return EResult::Success;
}

void TimeManager::Free()
{
}
#pragma endregion

#pragma region Loop
void TimeManager::Update()
{
    auto currentTime = std::chrono::steady_clock::now();
    f32 elapsedTime = std::chrono::duration<f32>(currentTime - m_LastTime).count();
    if (m_TargetFPS > 0)
    {
        const f32 targetFrameTime = 1.0f / static_cast<f32>(m_TargetFPS);
        if (elapsedTime < targetFrameTime)
        {
            const auto sleepDuration = std::chrono::duration<f32>(targetFrameTime - elapsedTime);
            std::this_thread::sleep_for(sleepDuration);
            currentTime = std::chrono::steady_clock::now();
            elapsedTime = std::chrono::duration<f32>(currentTime - m_LastTime).count();
        }
    }

    m_DeltaTime = elapsedTime;
    m_LastTime = currentTime;

    m_TotalTime += static_cast<f64>(m_DeltaTime);
    m_AccTime += static_cast<f64>(m_DeltaTime);

    if (m_AccTime > MAX_ACCUMULATED_TIME)
    {
        m_AccTime = MAX_ACCUMULATED_TIME;
    }

    m_FrameCount++;
    m_FPSTimer += m_DeltaTime;
    if (m_FPSTimer >= 1.0f)
    {
        m_FPS = m_FrameCount;
        m_FrameCount = 0;
        m_FPSTimer -= 1.0f;
    }
}

#pragma endregion


#pragma region Helper
bool TimeManager::IsFixedUpdateReady()
{
	if (m_AccTime >= m_FixedStep)
	{
		m_AccTime -= m_FixedStep;
		return true;
	}
	return false;
}
#pragma endregion






