#pragma once

#include "CollisionManager.h"
#include "BoxCollider.h"
#include "Box2DCollider.h"
#include "GameObject.h"
#include "Transform.h"
#include "Collision.h"
#include "RigidBody.h"

IMPLEMENT_SINGLETON(CollisionManager)

#pragma region Constructor&Destructor
EResult CollisionManager::Initialize(void* arg)
{
	return EResult::Success;
}

void CollisionManager::Free()
{
	RELEASE_VECTOR(m_Colliders);
}
EResult CollisionManager::AddCollider(Collider* collider)
{
	if (!collider) return EResult::InvalidArgument;
	m_Colliders.push_back(collider);
	return EResult::Success;
}
EResult CollisionManager::RemoveCollider(Collider* collider)
{
	auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
	if (it != m_Colliders.end())
	{
		m_Colliders.erase(it);
		return EResult::Success;
	}
	return EResult::InvalidArgument;
}
EResult CollisionManager::ClearColliders()
{
    m_Colliders.clear();
    return EResult::Success;
}
EResult CollisionManager::ClearRigidBodies()
{
    return EResult::Success;
}
bool CollisionManager::Raycast(const Ray& ray, HitResult& outResult)
{
	HitResult tempResult;
	bool hasHit = false;
	f32 minDistance = FLT_MAX;

	for (Collider* collider : m_Colliders)
	{
		if (collider->Raycast(ray, tempResult))
		{
			hasHit = true;
			if (tempResult.Distance < minDistance)
			{
				minDistance = tempResult.Distance;
				outResult = tempResult;
				outResult.UserData = collider->GetOwner();
			}
		}
	}

	return hasHit;
}

namespace
{
	static bool TryGetLocalAABB(Collider* collider, AABB& outLocalAABB)
	{
		if (!collider) return false;

		if (auto* box = dynamic_cast<BoxCollider*>(collider))
		{
			outLocalAABB = box->GetBox();
			return true;
		}

		if (auto* box2D = dynamic_cast<Box2DCollider*>(collider))
		{
			outLocalAABB = box2D->GetAABB2D();
			return true;
		}

		return false;
	}
}

void CollisionManager::ResolveCollisions(f32 dt)
{
    (void)dt;

    const size_t count = m_Colliders.size();
    for (size_t i = 0; i < count; ++i)
    {
        Collider* a = m_Colliders[i];
        if (!a || !a->IsActive()) continue;

        GameObject* ownerA = a->GetOwner();
        if (!ownerA || !ownerA->IsActive()) continue;

        Transform* transformA = ownerA->GetComponent<Transform>();
        if (!transformA) continue;

        AABB localAABB_A;
        if (!TryGetLocalAABB(a, localAABB_A)) continue;

        for (size_t j = i + 1; j < count; ++j)
        {
            Collider* b = m_Colliders[j];
            if (!b || !b->IsActive()) continue;

            GameObject* ownerB = b->GetOwner();
            if (!ownerB || !ownerB->IsActive()) continue;
            if (ownerA == ownerB) continue;

            Transform* transformB = ownerB->GetComponent<Transform>();
            if (!transformB) continue;

            AABB localAABB_B;
            if (!TryGetLocalAABB(b, localAABB_B)) continue;

            HitResult hit = {};
            if (!Collision::Intersects(localAABB_A, transformA->GetWorldMatrix(), localAABB_B, transformB->GetWorldMatrix(), hit))
                continue;

            if (hit.PenetrationDepth <= 0.f)
                continue;

            const vec3 correction = hit.Normal * hit.PenetrationDepth;

            const bool aMovable = (transformA->GetMobility() == EMobility::Movable);
            const bool bMovable = (transformB->GetMobility() == EMobility::Movable);

            if (!aMovable && !bMovable)
                continue;

			if (aMovable && bMovable)
			{
				transformA->SetPosition(transformA->GetLocalPosition() + correction * 0.5f);
				transformB->SetPosition(transformB->GetLocalPosition() - correction * 0.5f);

				RigidBody* rbA = ownerA->GetComponent<RigidBody>();
				if (rbA)
				{
					const f32 vnA = glm::dot(rbA->GetVelocity(), hit.Normal);
					if (vnA < 0.f)
						rbA->SetVelocity(rbA->GetVelocity() - hit.Normal * vnA);
				}

				RigidBody* rbB = ownerB->GetComponent<RigidBody>();
				if (rbB)
				{
					const vec3 normalB = -hit.Normal;
					const f32 vnB = glm::dot(rbB->GetVelocity(), normalB);
					if (vnB < 0.f)
						rbB->SetVelocity(rbB->GetVelocity() - normalB * vnB);
				}
			}
			else if (aMovable)
			{
				transformA->SetPosition(transformA->GetLocalPosition() + correction);

				RigidBody* rbA = ownerA->GetComponent<RigidBody>();
				if (rbA)
				{
					const f32 vnA = glm::dot(rbA->GetVelocity(), hit.Normal);
					if (vnA < 0.f)
						rbA->SetVelocity(rbA->GetVelocity() - hit.Normal * vnA);
				}
			}
			else
			{
				transformB->SetPosition(transformB->GetLocalPosition() - correction);

				RigidBody* rbB = ownerB->GetComponent<RigidBody>();
				if (rbB)
				{
					const vec3 normalB = -hit.Normal;
					const f32 vnB = glm::dot(rbB->GetVelocity(), normalB);
					if (vnB < 0.f)
						rbB->SetVelocity(rbB->GetVelocity() - normalB * vnB);
				}
			}
        }
    }
}
#pragma endregion