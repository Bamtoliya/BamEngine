#pragma once

#include "Collider.h"

struct tagBoxColliderDesc : public tagColliderDesc
{
	vec3 center = vec3(0.f);
	vec3 extent = vec3(0.5f);
};

BEGIN(Engine)

CLASS()
class ENGINE_API BoxCollider : public Collider
{
	REFLECT_CLASS(BoxCollider)
	using DESC = tagBoxColliderDesc;
#pragma region Constructor&Destructor
private:
	BoxCollider() : Collider(EColliderType::Box) {}
	virtual ~BoxCollider() {}
	EResult Initialize(void* arg = nullptr);
public:
	static BoxCollider* Create(void* arg = nullptr);
	Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Collision
	virtual bool Intersects(const Ray& ray, const mat4& transform, HitResult& outHit) override { return false; }
#pragma endregion


#pragma region Member Variable
private:
	PROPERTY()
	vec3 m_Center = vec3(0.f);
	PROPERTY()
	vec3 m_Extent = vec3(0.5f);
#pragma endregion

};
END