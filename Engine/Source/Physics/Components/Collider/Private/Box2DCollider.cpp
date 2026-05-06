#pragma once

#include "Box2DCollider.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "Transform.h"
#include "Collision.h"
#include "SpriteRenderer.h"


REGISTER_COMPONENT(Box2DCollider)

#pragma region Constructor&Destructor
EResult Box2DCollider::Initialize(void* arg)	
{
	if (arg)
	{
		CAST_DESC
		m_Center = vec2(desc->colliderDesc.center.x, desc->colliderDesc.center.y);
		m_Extent = vec2(desc->colliderDesc.extent.x, desc->colliderDesc.extent.y);
	}
	return __super::Initialize();
}

EResult Box2DCollider::LateInitialize(void* arg)
{
	AutoFit();
	return __super::LateInitialize();
}

Box2DCollider* Box2DCollider::Create(void* arg)
{
	Box2DCollider* instance = new Box2DCollider();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* Box2DCollider::Clone(GameObject* owner, void* arg)
{
	Box2DCollider* clone = Create(arg);
	if (clone)
	{
		clone->SetOwner(owner);
		AutoFit();
	}
	return clone;
}
void Box2DCollider::Free()
{
	__super::Free();
}
#pragma endregion

#pragma region Loop
void Box2DCollider::LateUpdate(f32 dt)
{
}

#pragma endregion

#pragma region AutoFit
void Box2DCollider::AutoFit()
{
	if (!m_Owner) return;

	SpriteRenderer* spriteRenderer = m_Owner->GetComponent<SpriteRenderer>();
	if (spriteRenderer && spriteRenderer->GetSprite())
	{
		Texture* texture = spriteRenderer->GetSprite()->GetTexture();
		if (texture)
		{
			f32 worldWidth = texture->GetWorldWidth() * spriteRenderer->GetTiling().x;
			f32 worldHeight = texture->GetWorldHeight() * spriteRenderer->GetTiling().y;
			m_Extent = vec2(worldWidth, worldHeight) * 0.5f;
		}
	}
}
#pragma endregion



#pragma region Collision
bool Box2DCollider::Raycast(const Ray& ray, HitResult& outResult)
{
	mat4 worldMatrix = m_Owner ? m_Owner->GetTransform()->GetWorldMatrix() : glm::identity<mat4>();
	const AABB localAABB = GetAABB2D();
	if (Collision::Raycast(ray, localAABB, worldMatrix, outResult))
	{
		outResult.UserData = m_Owner;
		return true;
	}
	return false;
}
#pragma endregion



