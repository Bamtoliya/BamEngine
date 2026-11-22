#pragma once
#include "System.h"

BEGIN(Engine)
class Scene;
class ENGINE_API SystemManager : public Base
{
	DECLARE_SINGLETON(SystemManager)
protected:
	SystemManager() {}
	virtual ~SystemManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free();
public:
	void FixedUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt);
	void Update(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt, uint8 phaseMask = static_cast<uint8>(ESystemPhase::All));
	void LateUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt);
	void Sumbit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt);

	// 마지막 Update의 페이즈 마스크. 에디터 실시간 프리뷰(Presentation만)와 게임 실행(Gameplay 포함)을 구분할 때 쓴다.
	uint8 GetUpdatePhaseMask() const { return m_UpdatePhaseMask; }
#if BAM_EDITOR
	void RequestUpdateTiming() { m_UpdateTimingRequested = true; }
	bool IsUpdateTimingRequested() const { return m_UpdateTimingRequested; }
#else
	void RequestUpdateTiming() {}
	bool IsUpdateTimingRequested() const { return false; }
#endif
public:
	template<typename T, typename... Args>
	T* AddSystem(Args&&... args);

	template<typename T>
	void RemoveSystem();

	template<typename T>
	T* GetSystem();
public:
	void InitializeSystems();
	void ClearSystems();
private:
	vector<ISystem*> m_Systems;
	unordered_map<entt::id_type, ISystem*> m_SystemMap;
	uint8 m_UpdatePhaseMask = static_cast<uint8>(ESystemPhase::All);
	bool m_UpdateTimingRequested = false;
};

#include "SystemManager.inl"
END
