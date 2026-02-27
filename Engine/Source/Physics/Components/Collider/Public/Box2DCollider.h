#pragma once

#include "Collider.h"


struct tagBox2DColliderDesc : public tagColliderDesc
{
	vec2 center = vec2(0.f);
	vec2 extent = vec2(0.5f);
};

BEGIN(Engine)
class ENGINE_API Box2DCollider final : public Collider
{
	REFLECT_CLASS(Box2DCollider)
	using DESC = tagBox2DColliderDesc;
#pragma region Constructor&Destructor
private:
	Box2DCollider() : Collider(EColliderType::Box2D) {}
	virtual ~Box2DCollider() = default;
	EResult Initialize(void* arg = nullptr) override;
public:
	static Box2DCollider* Create(void* arg = nullptr);
	virtual Component* Clone(GameObject* owner, void* arg = nullptr) override;
	virtual void Free() override;
#pragma endregion

#pragma region Loop
	virtual void LateUpdate(f32 dt) override;
#pragma endregion



#pragma region Collision
public:
	virtual bool Raycast(const struct Ray& ray, struct HitResult& outResult) override;
#pragma endregion


#pragma region Getter
public:
	const Rect GetRect() const { return Rect(m_Center.x - m_Extent.x, m_Center.y - m_Extent.y, m_Extent.x * 2, m_Extent.y * 2); }
#pragma endregion

#pragma region Member Variable
private:
	PROPERTY()
	vec2 m_Center = vec2(0.f);
	PROPERTY()
	vec2 m_Extent = vec2(0.5f);
#pragma endregion
};
END