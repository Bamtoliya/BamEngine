#pragma once

#include "Base.h"
#include "Collider.h"

BEGIN(Engine)

class ENGINE_API CollisionManager : public Base
{
	DECLARE_SINGLETON(CollisionManager)
private:
	CollisionManager() {}
	virtual ~CollisionManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
public:
	EResult AddCollider(class Collider* collider);
	EResult RemoveCollider(class Collider* collider);

public:
	bool Raycast(const Ray& ray, HitResult& outResult);
private:
	vector<class Collider*> m_Colliders;
};
END