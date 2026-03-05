#pragma once

#include "Box2DCollider.h"
#include "ComponentRegistry.h"
#include "GameObject.h"
#include "Transform.h"
#include "Collision.h"
#include "SpriteRenderer.h"
#ifdef _DEBUG
#include "Renderer.h"
#endif

REGISTER_COMPONENT(Box2DCollider)

#pragma region Constructor&Destructor
EResult Box2DCollider::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		m_Center = desc->center;
		m_Extent = desc->extent;
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
#ifdef _DEBUG
	if (m_DrawCollider)
	{
		vec3 worldPos = m_Owner ? m_Owner->GetTransform()->GetWorldPosition() : vec3(0.f);
		Rect worldRect = GetRect();
		worldRect.Left += worldPos.x;
		worldRect.Top += worldPos.y;
		Renderer::Get().DrawDebugRect(worldRect, vec4(0.f, 1.f, 0.f, 1.f));
	}
#endif // _DEBUG

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
	vec3 worldPos = m_Owner ? m_Owner->GetTransform()->GetWorldPosition() : vec3(0.f);
	Rect worldRect = GetRect();
	worldRect.Left += worldPos.x;
	worldRect.Top += worldPos.y;
	if (Collision::Raycast(ray, worldRect, outResult))
	{
		outResult.UserData = m_Owner;
		return true;
	}
	return false;
}
#pragma endregion



