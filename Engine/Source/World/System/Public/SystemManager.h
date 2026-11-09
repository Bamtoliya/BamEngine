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
};

#include "SystemManager.inl"
END