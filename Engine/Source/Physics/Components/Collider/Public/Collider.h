#pragma once

#include "Component.h"
#include "Structs.h"
#include "CollisionManager.h"

enum class EColliderType : uint8
{
	Box,
	Box2D,
	Sphere,
	Capsule,
	Mesh
};

struct tagColliderDesc : public tagComponentDesc
{
	EColliderType type = EColliderType::Box;
};


BEGIN(Engine)

CLASS()
class ENGINE_API Collider : public Component
{
	REFLECT_CLASS(Collider)
#pragma region Constructor&Destructor
protected:
	Collider(EColliderType type) : m_Type(type) {}
	virtual ~Collider() {}
	virtual EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

#pragma region Collision
public:
	virtual bool Raycast(const struct Ray& ray, struct HitResult& outResult) { return false; }
#pragma endregion

#pragma region Getter
public:
	EColliderType GetColliderType() const { return m_Type; }
#pragma endregion



#pragma region Member Variable
protected:
	EColliderType m_Type = EColliderType::Box;
#ifdef _DEBUG
	PROPERTY()
	bool m_DrawCollider = { true };
#endif
#pragma endregion

};
END