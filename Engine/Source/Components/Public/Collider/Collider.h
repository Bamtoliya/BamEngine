#pragma once


#include "Component.h"
#include "Structs.h"

enum class EColliderType : uint8
{
	Box,
	Sphere,
	Capsule,
	Mesh
};


BEGIN(Engine)

CLASS()
class ENGINE_API Collider : public Component
{
	REFLECT_CLASS(Collider)
#pragma region Constructor&Destructor
protected:
	Collider() {}
	virtual ~Collider() {}
public:
	virtual void Free() override;
#pragma endregion

#pragma region Collision
public:
	virtual bool Intersects(const Ray& ray, const mat4& transform, HitResult& outHit) PURE;
#pragma endregion
};
END