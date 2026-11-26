#include "SceneTransitionManager.h"
#include "SceneManager.h"
#include <archive/JsonArchive.h>
#include <algorithm>

#pragma region Constructor&Destructor
IMPLEMENT_SINGLETON(SceneTransitionManager)

EResult SceneTransitionManager::Initialize(void* /*arg*/)
{
	ResetTransitionState();
	return EResult::Success;
}

void SceneTransitionManager::Free()
{
	ResetTransitionState();
}
#pragma endregion

#pragma region Transition API
EResult SceneTransitionManager::TransitionToScene(
	const std::filesystem::path& scenePath,
	f32 fadeOutDuration,
	f32 fadeInDuration,
	TransitionCallback onBeforeLoad,
	TransitionCallback onAfterLoad)
{
	if (scenePath.empty())
	{
		ENGINE_LOG_WARN("SceneTransitionManager::TransitionToScene failed: scenePath is empty.");
		return EResult::InvalidArgument;
	}

	if (IsTransitioning())
	{
		ENGINE_LOG_WARN("SceneTransitionManager::TransitionToScene ignored: transition already in progress (target={}).",
			m_TargetScenePath.string());
		return EResult::Fail;
	}

	m_TargetScenePath = scenePath;
	m_FadeOutDuration = std::max(0.0f, fadeOutDuration);
	m_FadeInDuration = std::max(0.0f, fadeInDuration);
	m_StateElapsed = 0.0f;
	m_OnBeforeLoad = std::move(onBeforeLoad);
	m_OnAfterLoad = std::move(onAfterLoad);

	if (m_FadeOutDuration <= 0.0f)
	{
		m_FadeAlpha = 1.0f;
		m_State = ESceneTransitionState::Loading;
	}
	else
	{
		m_FadeAlpha = 0.0f;
		m_State = ESceneTransitionState::FadingOut;
	}

	return EResult::Success;
}

void SceneTransitionManager::Update(f32 deltaTime)
{
	if (m_State == ESceneTransitionState::Idle)
		return;

	const f32 dt = std::max(0.0f, deltaTime);

	if (m_State == ESceneTransitionState::FadingOut)
	{
		if (m_FadeOutDuration <= 0.0f)
		{
			m_FadeAlpha = 1.0f;
			m_State = ESceneTransitionState::Loading;
		}
		else
		{
			m_StateElapsed += dt;
			m_FadeAlpha = std::clamp(m_StateElapsed / m_FadeOutDuration, 0.0f, 1.0f);
			if (m_StateElapsed >= m_FadeOutDuration)
			{
				m_FadeAlpha = 1.0f;
				m_State = ESceneTransitionState::Loading;
			}
			else
			{
				return;
			}
		}
	}

	if (m_State == ESceneTransitionState::Loading)
	{
		ExecuteLoad();
		return;
	}

	if (m_State == ESceneTransitionState::FadingIn)
	{
		if (m_FadeInDuration <= 0.0f)
		{
			ResetTransitionState();
			return;
		}

		m_StateElapsed += dt;
		m_FadeAlpha = std::clamp(1.0f - (m_StateElapsed / m_FadeInDuration), 0.0f, 1.0f);
		if (m_StateElapsed >= m_FadeInDuration)
		{
			ResetTransitionState();
		}
	}
}
#pragma endregion

#pragma region Internal
void SceneTransitionManager::ExecuteLoad()
{
	if (!SceneManager::IsCreated())
	{
		ENGINE_LOG_ERROR("SceneTransitionManager::ExecuteLoad failed: SceneManager is not created (target={}).",
			m_TargetScenePath.string());
		ResetTransitionState();
		return;
	}

	const std::filesystem::path targetPath = m_TargetScenePath;
	TransitionCallback beforeCallback = std::move(m_OnBeforeLoad);
	TransitionCallback afterCallback = std::move(m_OnAfterLoad);
	m_OnBeforeLoad = nullptr;
	m_OnAfterLoad = nullptr;

	if (beforeCallback)
	{
		beforeCallback();
	}

	archive::JsonArchive ar;
	const EResult loadResult = SceneManager::Get().LoadScene(ar, targetPath.wstring());
	if (loadResult != EResult::Success)
	{
		ENGINE_LOG_ERROR("SceneTransitionManager::ExecuteLoad failed: SceneManager::LoadScene returned {} (target={}).",
			static_cast<int32>(loadResult), targetPath.string());
		ResetTransitionState();
		return;
	}

	if (afterCallback)
	{
		afterCallback();
	}

	if (m_FadeInDuration <= 0.0f)
	{
		ResetTransitionState();
		return;
	}

	m_StateElapsed = 0.0f;
	m_FadeAlpha = 1.0f;
	m_State = ESceneTransitionState::FadingIn;
}

void SceneTransitionManager::ResetTransitionState()
{
	m_State = ESceneTransitionState::Idle;
	m_TargetScenePath.clear();
	m_FadeOutDuration = 0.0f;
	m_FadeInDuration = 0.0f;
	m_StateElapsed = 0.0f;
	m_FadeAlpha = 0.0f;
	m_OnBeforeLoad = nullptr;
	m_OnAfterLoad = nullptr;
}
#pragma endregion
