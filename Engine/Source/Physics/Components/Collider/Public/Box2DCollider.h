#pragma once

#include "Collider.h"

BEGIN(Engine)
class ENGINE_API Box2DCollider final : public Collider
{
	REFLECT_CLASS(Box2DCollider, Collider)
#pragma region Constructor&Destructor
private:
	Box2DCollider();
	virtual ~Box2DCollider() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static Box2DCollider* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion


#pragma region Member Variable
private:
	Rect m_Rect;
#pragma endregion

}
END