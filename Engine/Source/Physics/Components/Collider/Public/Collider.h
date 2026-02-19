#pragma once


#include "Component.h"
#include "Structs.h"

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
public:
	virtual void Free() override;
#pragma endregion

#pragma region Collision
public:
	virtual bool Intersects(const Ray& ray, const mat4& transform, HitResult& outHit) PURE;
#pragma endregion

#pragma region Getter
public:
	EColliderType GetColliderType() const { return m_Type; }
#pragma endregion



#pragma region Member Variable
protected:
	EColliderType m_Type = EColliderType::Box;
#pragma endregion

};
END