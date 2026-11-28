#include "NavigationSystem.h"
#include "NavAgentComponent.h"
#include "AIPerceptionComponent.h"
#include "NavGrid.h"
#include "ComponentEnable.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "TransformSystem.h"
#include "FlagComponent.h"
#include "HierarchyComponent.h"
#include "TagComponent.h"
#include "PhysicsSystem.h"
#include "SystemManager.h"
#include "Functions.h"

#include <glm/gtx/norm.hpp>
#include <algorithm>
#include <cmath>
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
}

EResult NavigationSystem::Initialize(void* arg)
{
	return EResult::Success;
}

NavigationSystem* NavigationSystem::Create(void* arg)
{
	NavigationSystem* instance = new NavigationSystem();
	if (FAILED(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

void NavigationSystem::Free()
{
	ClearNavGrids();
	ClearNavMeshes();
	ISystem::Free();
}

void NavigationSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	if (SystemManager::IsCreated() &&
		(SystemManager::Get().GetUpdatePhaseMask() & static_cast<uint8>(ESystemPhase::Gameplay)) == 0)
	{
		return;
	}

	if (activeScenes.empty())
	{
		ProcessRegistry(registry, dt, nullptr);
	}
	else
	{
		for (Scene* scene : activeScenes)
		{
			if (scene && scene->IsActive() && !scene->IsPaused())
			{
				ProcessScene(*scene, dt);
			}
		}
	}
}

void NavigationSystem::SetSceneNavGrid(const Scene* scene, std::shared_ptr<NavGrid> grid)
{
	if (scene)
	{
		s_SceneNavGrids[scene] = std::move(grid);
	}
}

std::shared_ptr<NavGrid> NavigationSystem::GetSceneNavGrid(const Scene* scene)
{
	if (!scene)
	{
		return nullptr;
	}
	const auto it = s_SceneNavGrids.find(scene);
	if (it != s_SceneNavGrids.end())
	{
		return it->second;
	}
	return nullptr;
}

void NavigationSystem::RemoveSceneNavGrid(const Scene* scene)
{
	if (scene)
	{
		s_SceneNavGrids.erase(scene);
	}
}

void NavigationSystem::ClearNavGrids()
{
	s_SceneNavGrids.clear();
}

void NavigationSystem::SetSceneNavMesh(const Scene* scene, std::shared_ptr<NavMesh> navMesh)
{
	if (scene)
	{
		s_SceneNavMeshes[scene] = std::move(navMesh);
	}
}

std::shared_ptr<NavMesh> NavigationSystem::GetSceneNavMesh(const Scene* scene)
{
	if (!scene)
	{
		return nullptr;
	}
	const auto it = s_SceneNavMeshes.find(scene);
	if (it != s_SceneNavMeshes.end())
	{
		return it->second;
	}
	return nullptr;
}

void NavigationSystem::RemoveSceneNavMesh(const Scene* scene)
{
	if (scene)
	{
		s_SceneNavMeshes.erase(scene);
	}
}

void NavigationSystem::ClearNavMeshes()
{
	s_SceneNavMeshes.clear();
}

void NavigationSystem::ProcessScene(Scene& scene, f32 dt)
{
	ProcessRegistry(scene.GetRegistry(), dt, &scene);
}

void NavigationSystem::ProcessRegistry(entt::registry& world, f32 dt, const Scene* scene)
{
	// ─────────────────────────────────────────────────────────────
	// 1. 에이전트 경로 탐색 및 이동 추종 (NavAgentComponent)
	// ─────────────────────────────────────────────────────────────
	std::shared_ptr<NavMesh> defaultNavMesh;
	if (scene)
	{
		defaultNavMesh = GetSceneNavMesh(scene);
	}
	if (!defaultNavMesh)
	{
		if (const auto* ctxMesh = world.ctx().find<std::shared_ptr<NavMesh>>())
		{
			defaultNavMesh = *ctxMesh;
		}
	}
	const bool useNavMesh = (defaultNavMesh && defaultNavMesh->IsValid());

	std::shared_ptr<NavGrid> defaultNavGrid;
	if (scene)
	{
		defaultNavGrid = GetSceneNavGrid(scene);
	}
	if (!defaultNavGrid)
	{
		if (const auto* ctxGrid = world.ctx().find<std::shared_ptr<NavGrid>>())
		{
			defaultNavGrid = *ctxGrid;
		}
	}

	auto agentView = world.view<NavAgentComponent, TransformComponent>();
	for (auto entity : agentView)
	{
		if (!IsComponentEnabled<NavAgentComponent>(world, entity))
		{
			continue;
		}

		if (const auto* flag = world.try_get<FlagComponent>(entity))
		{
			if (!flag->IsActiveInHierarchy() ||
				HasFlag(flag->effectiveFlags, EEntityFlag::Dead) ||
				HasFlag(flag->effectiveFlags, EEntityFlag::Paused))
			{
				continue;
			}
		}

		auto& agent = agentView.get<NavAgentComponent>(entity);
		auto& transform = agentView.get<TransformComponent>(entity);

		// 경로 재계산 요청 처리 (NavMesh 우선, 미등록 시 NavGrid 폴백)
		if (agent.hasDestination && (agent.pathPending || (agent.autoRepath && agent.path.empty())))
		{
			if (useNavMesh || defaultNavGrid)
			{
				const vec3 currentPos = GetEntityWorldPosition(world, entity);
				const bool pathFound = useNavMesh
					? defaultNavMesh->FindPath(currentPos, agent.targetPosition, agent.path)
					: defaultNavGrid->FindPath(currentPos, agent.targetPosition, agent.path);
				if (pathFound)
				{
					agent.pathPending = false;
					agent.currentWaypointIndex = 0;
					agent.reachedDestination = false;
				}
				else
				{
					agent.pathPending = false;
					agent.reachedDestination = false;
					agent.velocity = vec3(0.0f);
				}
			}
		}

		// 경로 추종 이동
		if (agent.HasActivePath())
		{
			const vec3 currentPos = GetEntityWorldPosition(world, entity);
			const vec3 targetWaypoint = agent.path[agent.currentWaypointIndex];

			const vec2 currentXZ(currentPos.x, currentPos.z);
			const vec2 targetXZ(targetWaypoint.x, targetWaypoint.z);
			const vec2 diff = targetXZ - currentXZ;
			const f32 dist = glm::length(diff);

			const bool isLast = (agent.currentWaypointIndex + 1 >= agent.path.size());
			const f32 arrivalThreshold = isLast ? agent.stoppingDistance : glm::max(0.15f, agent.agentRadius * 0.5f);

			if (dist <= arrivalThreshold)
			{
				if (isLast)
				{
					agent.reachedDestination = true;
					agent.hasDestination = false;
					agent.velocity = vec3(0.0f);
				}
				else
				{
					agent.currentWaypointIndex++;
				}
			}
			else
			{
				const vec2 moveDir = dist > 1e-4f ? (diff / dist) : vec2(0.0f);
				f32 moveStep = agent.moveSpeed * dt;
				if (moveStep > dist)
				{
					moveStep = dist;
				}

				const vec2 newXZ = currentXZ + moveDir * moveStep;
				const f32 t = dist > 1e-4f ? (moveStep / dist) : 1.0f;
				const f32 newY = useNavMesh ? glm::mix(currentPos.y, targetWaypoint.y, t) : transform.position.y;
				const f32 velY = (useNavMesh && dt > 1e-5f) ? ((newY - currentPos.y) / dt) : 0.0f;
				agent.velocity = vec3(moveDir.x * agent.moveSpeed, velY, moveDir.y * agent.moveSpeed);

				(void)transform.SetLocalPosition(vec3(newXZ.x, newY, newXZ.y));

				// 진행 방향을 향하도록 Yaw 회전 갱신 (+Z 전방 기준)
				if (glm::length2(moveDir) > 1e-4f)
				{
					const f32 yawRad = std::atan2(moveDir.x, moveDir.y);
					(void)transform.SetEulerRotation(vec3(0.0f, glm::degrees(yawRad), 0.0f));
				}

				// 최상위 엔티티인 경우 월드 변환 행렬 동기화
				const auto* hier = world.try_get<HierarchyComponent>(entity);
				if (!hier || hier->parent == entt::null)
				{
					if (auto* wt = world.try_get<WorldTransformComponent>(entity))
					{
						TransformSystem::UpdateRootWorld(transform, *wt);
					}
				}
			}
		}
	}

	// ─────────────────────────────────────────────────────────────
	// 2. AI 인지 시스템 (AIPerceptionComponent)
	// ─────────────────────────────────────────────────────────────
	auto perceptionView = world.view<AIPerceptionComponent, TransformComponent>();
	for (auto aiEntity : perceptionView)
	{
		if (!IsComponentEnabled<AIPerceptionComponent>(world, aiEntity))
		{
			continue;
		}

		if (const auto* flag = world.try_get<FlagComponent>(aiEntity))
		{
			if (!flag->IsActiveInHierarchy() ||
				HasFlag(flag->effectiveFlags, EEntityFlag::Dead) ||
				HasFlag(flag->effectiveFlags, EEntityFlag::Paused))
			{
				continue;
			}
		}

		auto& perception = perceptionView.get<AIPerceptionComponent>(aiEntity);
		const auto& aiTransform = perceptionView.get<TransformComponent>(aiEntity);

		vec3 aiPos = aiTransform.position;
		vec3 forward = aiTransform.GetLocalForward();
		if (const auto* wt = world.try_get<WorldTransformComponent>(aiEntity))
		{
			aiPos = vec3(wt->worldMatrix[3]);
			const vec3 fwd = vec3(wt->worldMatrix[2]);
			if (glm::length2(fwd) > 1e-6f)
			{
				forward = glm::normalize(fwd);
			}
		}

		entt::entity bestTarget = entt::null;
		vec3 bestTargetPos(0.0f);
		bool bestHasLOS = false;
		f32 closestDistSq = std::numeric_limits<f32>::max();

		const std::wstring targetTagW = Engine::Unicode::Utf8ToWide(perception.targetTag);

		auto candidateView = world.view<TransformComponent>();
		for (auto candEntity : candidateView)
		{
			if (candEntity == aiEntity || !world.valid(candEntity))
			{
				continue;
			}

			if (const auto* cFlag = world.try_get<FlagComponent>(candEntity))
			{
				if (!cFlag->IsActiveInHierarchy() || HasFlag(cFlag->effectiveFlags, EEntityFlag::Dead))
				{
					continue;
				}
			}

			// 태그 매칭 검사
			if (!perception.targetTag.empty())
			{
				const auto* tagComp = world.try_get<TagComponent>(candEntity);
				if (!tagComp || !tagComp->tags.contains(targetTagW))
				{
					continue;
				}
			}

			const vec3 candPos = GetEntityWorldPosition(world, candEntity);
			const vec3 toTarget = candPos - aiPos;
			const f32 distSq = glm::dot(toTarget, toTarget);
			const f32 maxPerceptionRange = glm::max(perception.sightRange, perception.hearingRange);

			if (distSq > maxPerceptionRange * maxPerceptionRange)
			{
				continue;
			}

			const f32 dist = glm::sqrt(distSq);
			bool canSense = false;
			bool hasLOS = false;

			// 시야각 및 시야 범위 검사
			if (dist <= perception.sightRange)
			{
				const vec3 toTargetDir = dist > 1e-4f ? (toTarget / dist) : forward;
				const f32 dotVal = glm::clamp(glm::dot(forward, toTargetDir), -1.0f, 1.0f);
				const f32 angleDeg = glm::degrees(std::acos(dotVal));

				if (angleDeg <= perception.sightHalfAngleDegrees)
				{
					bool blocked = false;
					if (perception.requireLineOfSight && scene != nullptr)
					{
						const Ray ray(aiPos + vec3(0.0f, 0.5f, 0.0f), toTargetDir);
						SceneRaycastHit hit;
						RaycastQuery query;
						query.maxDistance = dist;
						query.ignoreEntity = aiEntity;
						query.includeTrigger = false;

						if (PhysicsSystem::Raycast(*scene, ray, hit, query))
						{
							if (hit.Entity != candEntity && hit.Hit.distance < dist - 0.05f)
							{
								blocked = true;
							}
						}
					}

					if (!blocked)
					{
						canSense = true;
						hasLOS = true;
					}
				}
			}

			// 청각 범위 검사 (시각 미감지 시 보완)
			if (!canSense && dist <= perception.hearingRange)
			{
				canSense = true;
				hasLOS = false;
			}

			if (canSense)
			{
				if (bestTarget == entt::null || (hasLOS && !bestHasLOS) || (hasLOS == bestHasLOS && distSq < closestDistSq))
				{
					bestTarget = candEntity;
					bestTargetPos = candPos;
					bestHasLOS = hasLOS;
					closestDistSq = distSq;
				}
			}
		}

		if (bestTarget != entt::null)
		{
			perception.detectedTarget = bestTarget;
			perception.lastKnownTargetPos = bestTargetPos;
			perception.hasLineOfSight = bestHasLOS;
			perception.timeSinceLastSeen = 0.0f;
		}
		else
		{
			perception.hasLineOfSight = false;
			if (perception.detectedTarget != entt::null)
			{
				perception.timeSinceLastSeen += dt;
				if (perception.timeSinceLastSeen > 10.0f || !world.valid(perception.detectedTarget))
				{
					perception.ClearTarget();
				}
			}
		}
	}
}

END
