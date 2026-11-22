#include "GameplayCombatSystem.h"
#include "ComponentEnable.h"
#include "HitboxComponent.h"
#include "HealthComponent.h"
#include "InteractableComponent.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "TransformSystem.h"
#include "FlagComponent.h"
#include "HierarchyComponent.h"
#include "TagComponent.h"
#include "PhysicsSystem.h"
#include "ColliderComponent.h"
#include "RigidBodyComponent.h"
#include "CharacterControllerComponent.h"
#include "BoundingSphere.h"
#include "BoundingBox.h"
#include "Capsule.h"
#include "Functions.h"
#include <algorithm>
#include <glm/gtx/norm.hpp>
#include <limits>

BEGIN(Engine)

namespace
{
	vec3 GetEntityWorldPosition(const entt::registry& world, entt::entity entity)
	{
		if (const auto* wt = world.try_get<WorldTransformComponent>(entity))
		{
			return vec3(wt->worldMatrix[3]);
		}
		if (const auto* t = world.try_get<TransformComponent>(entity))
		{
			return t->position;
		}
		return vec3(0.0f);
	}

	entt::entity GetHierarchyRoot(const entt::registry& world, entt::entity entity)
	{
		entt::entity curr = entity;
		while (curr != entt::null && world.valid(curr))
		{
			const auto* hier = world.try_get<HierarchyComponent>(curr);
			if (!hier || hier->parent == entt::null)
				break;
			curr = hier->parent;
		}
		return curr;
	}

	bool IsSameHierarchy(const entt::registry& world, entt::entity a, entt::entity b)
	{
		if (a == b)
			return true;
		if (a == entt::null || b == entt::null)
			return false;
		return GetHierarchyRoot(world, a) == GetHierarchyRoot(world, b);
	}
}

GameplayCombatSystem* GameplayCombatSystem::Create(void* arg)
{
	auto* instance = new GameplayCombatSystem();
	if (instance->Initialize(arg) != EResult::Success)
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void GameplayCombatSystem::Free()
{
}

EResult GameplayCombatSystem::Initialize(void* arg)
{
	return EResult::Success;
}

void GameplayCombatSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	if (activeScenes.empty())
	{
		ProcessRegistryCombat(registry, dt, nullptr);
	}
	else
	{
		for (Scene* scene : activeScenes)
		{
			if (scene)
			{
				ProcessSceneCombat(*scene, dt);
			}
		}
	}
}

void GameplayCombatSystem::ProcessSceneCombat(Scene& scene, f32 dt)
{
	ProcessRegistryCombat(scene.GetRegistry(), dt, &scene);
}

void GameplayCombatSystem::ProcessRegistryCombat(entt::registry& world, f32 dt, const Scene* scene)
{
	auto hitboxView = world.view<HitboxComponent>();
	for (auto hitboxEntity : hitboxView)
	{
		auto& hitbox = hitboxView.get<HitboxComponent>(hitboxEntity);
		if (!hitbox.active || !IsComponentEnabled<HitboxComponent>(world, hitboxEntity))
		{
			hitbox.ResetSwingHits();
			continue;
		}
		if (const auto* flag = world.try_get<FlagComponent>(hitboxEntity))
		{
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))
			{
				hitbox.ResetSwingHits();
				continue;
			}
		}

		const vec3 hitboxPos = GetEntityWorldPosition(world, hitboxEntity);
		std::vector<entt::entity> candidateTargets;
		candidateTargets.reserve(16);

		const auto* collider = world.try_get<ColliderComponent>(hitboxEntity);

		if (scene != nullptr)
		{
			OverlapQuery query{};
			query.includeTrigger = true;
			query.ignoreEntity = hitboxEntity;

			if (collider)
			{
				const vec3 colCenter = hitboxPos + collider->GetOffset();
				switch (collider->GetShapeType())
				{
				case EColliderShapeType::Sphere:
				{
					const auto& sp = std::get<SphereShape>(collider->GetShape());
					auto hits = PhysicsSystem::OverlapAll(*scene, BoundingSphere(colCenter, sp.Radius), query);
					for (const auto& hit : hits) candidateTargets.push_back(hit.Entity);
					break;
				}
				case EColliderShapeType::Box:
				{
					const auto& bx = std::get<BoxShape>(collider->GetShape());
					const vec3 half = bx.Size * 0.5f;
					auto hits = PhysicsSystem::OverlapAll(*scene, BoundingBox(colCenter - half, colCenter + half), query);
					for (const auto& hit : hits) candidateTargets.push_back(hit.Entity);
					break;
				}
				case EColliderShapeType::Capsule:
				{
					const auto& cp = std::get<CapsuleShape>(collider->GetShape());
					const f32 halfHeight = std::max(0.0f, cp.Height * 0.5f - cp.Radius);
					const vec3 pA = colCenter - vec3(0.0f, halfHeight, 0.0f);
					const vec3 pB = colCenter + vec3(0.0f, halfHeight, 0.0f);
					auto hits = PhysicsSystem::OverlapAll(*scene, Capsule(pA, pB, cp.Radius), query);
					for (const auto& hit : hits) candidateTargets.push_back(hit.Entity);
					break;
				}
				default:
				{
					auto hits = PhysicsSystem::OverlapAll(*scene, BoundingSphere(hitboxPos, hitbox.fallbackRadius), query);
					for (const auto& hit : hits) candidateTargets.push_back(hit.Entity);
					break;
				}
				}
			}
			else
			{
				auto hits = PhysicsSystem::OverlapAll(*scene, BoundingSphere(hitboxPos, hitbox.fallbackRadius), query);
				for (const auto& hit : hits) candidateTargets.push_back(hit.Entity);
			}
		}

		auto healthView = world.view<HealthComponent>();
		for (auto targetEntity : healthView)
		{
			if (std::find(candidateTargets.begin(), candidateTargets.end(), targetEntity) != candidateTargets.end())
				continue;
			if (IsSameHierarchy(world, hitboxEntity, targetEntity))
				continue;

			const vec3 targetPos = GetEntityWorldPosition(world, targetEntity);
			bool inVolume = false;

			if (collider)
			{
				const vec3 colCenter = hitboxPos + collider->GetOffset();
				switch (collider->GetShapeType())
				{
				case EColliderShapeType::Sphere:
				{
					const auto& sp = std::get<SphereShape>(collider->GetShape());
					f32 targetRadius = 0.0f;
					if (const auto* targetCol = world.try_get<ColliderComponent>(targetEntity))
					{
						if (targetCol->GetShapeType() == EColliderShapeType::Sphere)
							targetRadius = std::get<SphereShape>(targetCol->GetShape()).Radius;
					}
					inVolume = (glm::distance(targetPos, colCenter) <= (sp.Radius + targetRadius));
					break;
				}
				case EColliderShapeType::Box:
				{
					const auto& bx = std::get<BoxShape>(collider->GetShape());
					const vec3 half = bx.Size * 0.5f;
					inVolume = BoundingBox(colCenter - half, colCenter + half).Contains(targetPos);
					break;
				}
				case EColliderShapeType::Capsule:
				{
					const auto& cp = std::get<CapsuleShape>(collider->GetShape());
					const f32 halfHeight = std::max(0.0f, cp.Height * 0.5f - cp.Radius);
					const vec3 pA = colCenter - vec3(0.0f, halfHeight, 0.0f);
					const vec3 pB = colCenter + vec3(0.0f, halfHeight, 0.0f);
					inVolume = Capsule(pA, pB, cp.Radius).Contains(targetPos);
					break;
				}
				default:
					inVolume = (glm::distance(targetPos, hitboxPos) <= hitbox.fallbackRadius);
					break;
				}
			}
			else
			{
				f32 targetRadius = 0.0f;
				if (const auto* targetCol = world.try_get<ColliderComponent>(targetEntity))
				{
					if (targetCol->GetShapeType() == EColliderShapeType::Sphere)
						targetRadius = std::get<SphereShape>(targetCol->GetShape()).Radius;
				}
				inVolume = (glm::distance(targetPos, hitboxPos) <= (hitbox.fallbackRadius + targetRadius));
			}

			if (inVolume)
			{
				candidateTargets.push_back(targetEntity);
			}
		}

		for (entt::entity targetEntity : candidateTargets)
		{
			if (!world.valid(targetEntity))
				continue;

			if (IsSameHierarchy(world, hitboxEntity, targetEntity))
				continue;

			auto* health = TryGetEnabledComponent<HealthComponent>(world, targetEntity);
			if (!health || health->isDead || health->invulnerable)
				continue;

			if (const auto* flag = world.try_get<FlagComponent>(targetEntity))
			{
				if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))
					continue;
			}

			if (!hitbox.teamTag.empty())
			{
				bool friendly = false;
				if (const auto* targetHitbox = world.try_get<HitboxComponent>(targetEntity))
				{
					if (targetHitbox->teamTag == hitbox.teamTag)
						friendly = true;
				}
				if (!friendly)
				{
					if (const auto* targetTag = world.try_get<TagComponent>(targetEntity))
					{
						if (targetTag->tags.contains(StrToWStr(hitbox.teamTag)))
							friendly = true;
					}
				}
				if (friendly)
					continue;
			}

			if (hitbox.HasHit(targetEntity))
				continue;

			hitbox.RecordHit(targetEntity);
			health->ApplyDamage(hitbox.damage);

			if (hitbox.knockbackForce > 0.0f)
			{
				const vec3 targetPos = GetEntityWorldPosition(world, targetEntity);
				vec3 dir = targetPos - hitboxPos;
				if (glm::length2(dir) < 1.0e-6f)
				{
					dir = vec3(0.0f, 0.0f, 1.0f);
				}
				else
				{
					dir = glm::normalize(dir);
				}

				const vec3 impulse = dir * hitbox.knockbackForce;
				if (auto* rb = world.try_get<RigidBodyComponent>(targetEntity))
				{
					rb->LinearVelocity += impulse;
				}
				if (auto* kcc = world.try_get<CharacterControllerComponent>(targetEntity))
				{
					kcc->Velocity += impulse;
				}
			}

			if (hitbox.destroyOnHit)
			{
				hitbox.SetActive(false);
				if (auto* flag = world.try_get<FlagComponent>(hitboxEntity))
				{
					flag->SetActive(false);
				}
				break;
			}
		}
	}
}

entt::entity GameplayCombatSystem::FindNearestInteractable(const Scene& scene, const vec3& interactorPos, std::string_view requiredTag)
{
	const auto& world = scene.GetRegistry();
	auto view = world.view<InteractableComponent>();

	entt::entity bestEntity = entt::null;
	f32 bestDistSq = std::numeric_limits<f32>::max();

	for (auto entity : view)
	{
		if (!IsComponentEnabled<InteractableComponent>(world, entity))
			continue;

		const auto& interactable = view.get<InteractableComponent>(entity);
		if (!interactable.CanInteract())
			continue;

		if (const auto* flag = world.try_get<FlagComponent>(entity))
		{
			if (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead))
				continue;
		}

		if (!requiredTag.empty())
		{
			bool tagMatched = (interactable.interactTag == requiredTag);
			if (!tagMatched)
			{
				if (const auto* tagComp = world.try_get<TagComponent>(entity))
				{
					tagMatched = tagComp->tags.contains(StrToWStr(std::string(requiredTag)));
				}
			}
			if (!tagMatched)
				continue;
		}

		const vec3 entityPos = GetEntityWorldPosition(world, entity);
		const f32 dist = glm::distance(interactorPos, entityPos);
		if (dist <= interactable.interactRange)
		{
			const f32 distSq = dist * dist;
			if (distSq < bestDistSq)
			{
				bestDistSq = distSq;
				bestEntity = entity;
			}
		}
	}

	return bestEntity;
}

bool GameplayCombatSystem::TryInteractNearest(Scene& scene, const vec3& interactorPos, entt::entity* outInteracted, std::string_view requiredTag)
{
	const entt::entity target = FindNearestInteractable(scene, interactorPos, requiredTag);
	if (target == entt::null)
	{
		if (outInteracted)
			*outInteracted = entt::null;
		return false;
	}

	auto& world = scene.GetRegistry();
	auto* interactable = world.try_get<InteractableComponent>(target);
	if (!interactable || !interactable->TryConsume())
	{
		if (outInteracted)
			*outInteracted = entt::null;
		return false;
	}

	if (outInteracted)
		*outInteracted = target;
	return true;
}

END
