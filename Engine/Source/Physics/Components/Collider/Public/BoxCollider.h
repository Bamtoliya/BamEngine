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
	REFLECT_CLASS()
	using DESC = tagBoxColliderDesc;
#pragma region Constructor&Destructor
private:
	BoxCollider() : Collider(EColliderType::Box) {}
	virtual ~BoxCollider() {}
	EResult Initialize(void* arg = nullptr);
public:
	EResult LateInitialize(void* arg = nullptr) override;
	static BoxCollider* Create(void* arg = nullptr);
	Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual void LateUpdate(f32 dt);
#pragma endregion


#pragma region Collision
public:
	virtual bool Raycast(const struct Ray& ray, struct HitResult& outResult) override;
#pragma endregion

#pragma region Getter
public:
	const AABB GetBox() const { return AABB(m_Center - m_Extents, m_Center + m_Extents); }
	const vec3 GetCenter() const { return m_Center; }
	const vec3 GetExtent() const { return m_Extents; }
	const vec3 GetMin() const { return m_Center - m_Extents; }
	const vec3 GetMax() const { return m_Center + m_Extents; }
#pragma endregion

#pragma region Bounds
private:
	void AutoFit();
#pragma endregion


#pragma region Member Variable
private:
	PROPERTY()
	vec3 m_Center;

	PROPERTY()
	vec3 m_Extents;
#pragma endregion

};
END