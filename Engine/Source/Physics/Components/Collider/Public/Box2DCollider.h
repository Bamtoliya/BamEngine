#pragma once

#include "Collider.h"

BEGIN(Engine)
CLASS()
class ENGINE_API Box2DCollider final : public Collider
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
private:
	Box2DCollider() : Collider(EColliderType::Box2D) {}
	virtual ~Box2DCollider() = default;
	virtual EResult Initialize(void* arg = nullptr) override;
	virtual EResult LateInitialize(void* arg = nullptr) override;
public:
	static Box2DCollider* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
public:
	virtual void LateUpdate(f32 dt) override;
#pragma endregion

#pragma region Auto Fit
public:
	virtual void AutoFit();
#pragma endregion


#pragma region Collision
public:
	virtual bool Raycast(const struct Ray& ray, struct HitResult& outResult) override;
#pragma endregion


#pragma region Getter
public:
	const Rect GetRect() const { return Rect(m_Center.x - m_Extent.x, m_Center.y - m_Extent.y, m_Extent.x * 2, m_Extent.y * 2); }
	const AABB GetAABB2D() const { return AABB(vec3(m_Center - m_Extent, -1.f), vec3(m_Center + m_Extent, 1.0f)); }
#pragma endregion

#pragma region Member Variable
private:
	PROPERTY(EDITABLE)
	vec2 m_Center = vec2(0.f);
	PROPERTY(EDITABLE)
	vec2 m_Extent = vec2(10.f);
#pragma endregion
};
END