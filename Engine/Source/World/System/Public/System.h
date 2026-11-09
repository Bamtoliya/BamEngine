#pragma once

#include "Base.h"
#include "Scene.h"

BEGIN(Engine)

// 시스템이 관여하는 실행 페이즈 (비트마스크)
enum class ESystemPhase : uint8
{
	Gameplay = 1 << 0,  // 물리(Physics), 게임스크립트(Graph) 등 게임 규칙 및 상태 변형 로직
	Presentation = 1 << 1,  // 파티클(Particle), 애니메이션(Animation) 등 시각적 표현 및 연출
	All = Gameplay | Presentation
};

class ENGINE_API ISystem : public Base
{
public:
	virtual ~ISystem() = default;

	virtual void OnAwake() {}
	virtual void OnInitialize() {}

	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) {}
	virtual void OnFixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) {}
	virtual void OnPostFixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) {}
	virtual void OnLateUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) {}

	virtual void OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) {}

	virtual void OnDestroy() {}
	virtual void OnFree() {}

	bool IsActive() const { return m_IsActive; }
	void SetActive(bool active) { m_IsActive = active; }
	void ToggleActive() { m_IsActive = !m_IsActive; }

	//이 시스템이 실행되는 페이즈 (기본값: Gameplay)
	virtual uint8 GetExecutionPhases() const { return static_cast<uint8>(ESystemPhase::Gameplay); }

private:
	bool m_IsActive = true;
};
END