#pragma once

#include "Base.h"
#include <filesystem>
#include <functional>

BEGIN(Engine)

enum class ESceneTransitionState : uint8
{
	Idle = 0,
	FadingOut,
	Loading,
	FadingIn
};

class ENGINE_API SceneTransitionManager : public Base
{
#pragma region Constructor&Destructor
	DECLARE_SINGLETON(SceneTransitionManager)
protected:
	SceneTransitionManager() = default;
	virtual ~SceneTransitionManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Transition API
public:
	using TransitionCallback = std::function<void()>;

	EResult TransitionToScene(
		const std::filesystem::path& scenePath,
		f32 fadeOutDuration = 0.5f,
		f32 fadeInDuration = 0.5f,
		TransitionCallback onBeforeLoad = nullptr,
		TransitionCallback onAfterLoad = nullptr);

	void Update(f32 deltaTime);

	void SetFadeColor(const vec4& color) { m_FadeColor = color; }

	[[nodiscard]] f32 GetFadeAlpha() const { return m_FadeAlpha; }
	[[nodiscard]] const vec4& GetFadeColor() const { return m_FadeColor; }
	[[nodiscard]] bool IsTransitioning() const { return m_State != ESceneTransitionState::Idle; }
	[[nodiscard]] ESceneTransitionState GetState() const { return m_State; }
#pragma endregion

#pragma region Internal
private:
	void ExecuteLoad();
	void ResetTransitionState();
#pragma endregion

#pragma region Variable
private:
	ESceneTransitionState m_State = ESceneTransitionState::Idle;
	std::filesystem::path m_TargetScenePath;
	f32 m_FadeOutDuration = 0.0f;
	f32 m_FadeInDuration = 0.0f;
	f32 m_StateElapsed = 0.0f;
	f32 m_FadeAlpha = 0.0f;
	vec4 m_FadeColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	TransitionCallback m_OnBeforeLoad;
	TransitionCallback m_OnAfterLoad;
#pragma endregion
};

END
