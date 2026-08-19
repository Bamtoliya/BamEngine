#pragma once
#include "Base.h"

BEGIN(Engine)

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
	void FixedUpdate(entt::registry& registry, f32 dt);
	void Update(entt::registry& registry, f32 dt);
	void LateUpdate(entt::registry& registry, f32 dt);
	void Render(entt::registry& registry);
public:
	template<typename T, typename... Args>
	T* RegisterSystem(Args&&... args);

	template<typename T>
	void UnregisterSystem();
private:

};
END