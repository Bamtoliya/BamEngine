#pragma once

#include "Base.h"
#include "Component.h"
BEGIN(Engine)
class ENGINE_API Entity : public Base
{
#pragma region Constructor&Destructor
protected:
	Entity() {}
	virtual ~Entity() {}
	virtual EResult Initalize(void* arg = nullptr) { return EResult::Success; }
public:
	static Entity* Create(void* arg = nullptr);
	virtual Entity* Clone(void* arg = nullptr) { return nullptr; }
	virtual void Free() override {};
#pragma endregion
#pragma region Loop
	virtual void	FixedUpdate(f32 dt) {};
	virtual void	Update(f32 dt) {};
	virtual void	LateUpdate(f32 dt) {};
	virtual EResult	Render(f32 dt) { return EResult::Success; }
#pragma endregion
};
END