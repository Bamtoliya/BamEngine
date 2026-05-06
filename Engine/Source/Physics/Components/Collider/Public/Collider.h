#pragma once

#include "Component.h"
#include "Structs.h"
#include "CollisionManager.h"

BEGIN(Engine)

ENUM()
enum class EColliderType : uint8
{
	Box,
	Box2D,
	Sphere,
	Capsule,
	Mesh
};

struct tagColliderDesc
{
	EColliderType type = EColliderType::Box;
	vec3 center = vec3(0.f);
	vec3 extent = vec3(0.f);   // Box: (ex,ey,ez), Sphere: (r,0,0), Capsule: (r,h,0)
};

struct tagColliderCreateDesc : public tagComponentDesc
{
	tagColliderDesc colliderDesc;
};

CLASS()
class ENGINE_API Collider : public Component
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
protected:
	using DESC = tagColliderCreateDesc;
	Collider() = default;
	Collider(EColliderType type) : m_Type(type) {}
	virtual ~Collider() {}
	virtual EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion


#pragma region Getter

#ifdef _DEBUG
	bool IsDrawCollider() const { return m_DrawCollider; }
#endif
#pragma endregion


#pragma region Collision
public:

public:
	virtual bool Raycast(const struct Ray& ray, struct HitResult& outResult) { return false; }
#pragma endregion

#pragma region Management
public:
	EColliderType GetColliderType() const { return m_Type; }
	virtual tagColliderDesc GetColliderDesc() const { return tagColliderDesc{ m_Type }; }
	void SetColliderType(EColliderType type) { m_Type = type; }
#pragma endregion

#pragma region Save & Load
	virtual void Deserialize(Archive& ar) override;
#pragma endregion

#pragma region Member Variable
protected:
	PROPERTY(EDITABLE)
	EColliderType m_Type = EColliderType::Box;
#ifdef _DEBUG
	PROPERTY(EDITABLE)
	bool m_DrawCollider = { true };
#endif
#pragma endregion

};
END