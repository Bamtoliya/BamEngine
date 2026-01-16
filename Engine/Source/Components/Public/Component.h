#pragma once

#include "Base.h"

BEGIN(Engine)
class GameObject;
class ENGINE_API Component abstract : public Base
{
#pragma region Constructor&Destructor
protected:
	Component() {}
	virtual ~Component() {}
	virtual EResult Initialize(void* arg = nullptr) { return EResult::Success; }
public:
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) PURE;
	virtual void Free() override { m_Owner = nullptr; }
#pragma endregion

#pragma region Loop
public:
	virtual void FixedUpdate(f32 dt) {}
	virtual void Update(f32 dt) {}
	virtual void LateUpdate(f32 dt) {}
	virtual EResult Render(f32 dt) { return EResult::Success; }
#pragma endregion

#pragma region Getter
	GameObject* GetOwner() const { return m_Owner; }
#pragma endregion

#pragma region Setter
	void SetOwner(GameObject* owner) { m_Owner = owner; }
#pragma endregion


protected:
	GameObject* m_Owner = { nullptr };
};

END