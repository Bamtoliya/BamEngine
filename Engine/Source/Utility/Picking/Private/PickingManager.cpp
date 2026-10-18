#pragma once

#include "PickingManager.h"
#include "SceneManager.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "TransformSystem.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Sprite.h"
#include "Collision.h"
#include <cmath>

IMPLEMENT_SINGLETON(PickingManager)

#pragma region Constructor&Destructor
EResult PickingManager::Initialize(void* arg)
{
	(void)arg;
	ClearState();
	return EResult::Success;
}

void PickingManager::Free()
{
	ClearState();
}
#pragma endregion

void PickingManager::BeginFrame()
{
	++m_PickingFrameSerial;
	ClearPickingTargets(m_PickingTargets);
	m_PickingIds.clear();
	m_PickingPasses.clear();
}

void PickingManager::EndFrame(bool frameSucceeded)
{
	ClearPickingTargets(m_CompletedPickingTargets);
	m_CompletedPickingPasses.clear();
	m_CompletedPickingFrameSerial = 0;
	if (!frameSucceeded) return;
	m_CompletedPickingTargets.swap(m_PickingTargets);
	m_CompletedPickingPasses.swap(m_PickingPasses);
	m_CompletedPickingFrameSerial = m_PickingFrameSerial;
}

void PickingManager::ClearPickingTargets(vector<PickingTarget>& targets)
{
	for (auto& target : targets) Safe_Release(target.scene);
	targets.clear();
}

void PickingManager::ClearState()
{
	ClearPickingTargets(m_PickingTargets);
	ClearPickingTargets(m_CompletedPickingTargets);
	m_PickingIds.clear();
	m_PickingPasses.clear();
	m_CompletedPickingPasses.clear();
	m_CompletedPickingFrameSerial = 0;
}

uint32 PickingManager::RegisterPickingEntity(Scene* scene, entt::entity entity)
{
	if (!scene || !scene->GetRegistry().valid(entity)) return 0;
	const auto* id = scene->GetRegistry().try_get<IDComponent>(entity);
	if (!id || id->id == 0) return 0;
	auto& ids = m_PickingIds[scene];
	if (const auto found = ids.find(entity); found != ids.end()) return found->second;
	if (m_PickingTargets.size() >= 0x00ffffffu) return 0;
	m_PickingTargets.push_back({ scene, entity, id->id });
	Safe_AddRef(scene);
	const uint32 pickingId = static_cast<uint32>(m_PickingTargets.size());
	ids.emplace(entity, pickingId);
	return pickingId;
}

Entity* PickingManager::ResolvePickingEntity(uint32 pickingId) const
{
	if (pickingId == 0 || pickingId > m_CompletedPickingTargets.size()) return nullptr;
	const auto& target = m_CompletedPickingTargets[pickingId - 1];
	const auto scenes = SceneManager::Get().GetActiveScenes();
	if (std::find(scenes.begin(), scenes.end(), target.scene) == scenes.end()) return nullptr;
	const auto& world = target.scene->GetRegistry();
	if (!world.valid(target.entity)) return nullptr;
	const auto* id = world.try_get<IDComponent>(target.entity);
	const auto* flag = world.try_get<FlagComponent>(target.entity);
	if (!id || id->id != target.persistentId || !flag || !HasFlag(flag->flags, EEntityFlag::Active) ||
		!HasFlag(flag->flags, EEntityFlag::Visible) || HasFlag(flag->flags, EEntityFlag::Dead)) return nullptr;
	return target.scene->FindEntity(target.entity);
}

bool PickingManager::TryGetViewportUV(const vec2& mouse, const vec2& origin, const vec2& size, vec2& outUV)
{
	if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x <= 0.0f || size.y <= 0.0f) return false;
	outUV = (mouse - origin) / size;
	return std::isfinite(outUV.x) && std::isfinite(outUV.y) &&
		outUV.x >= 0.0f && outUV.y >= 0.0f && outUV.x < 1.0f && outUV.y < 1.0f;
}
bool PickingManager::TryBuildRay(const mat4& inverseViewProjection, const vec2& uv, Ray& outRay, f32* outMaxDistance)
{
	if (!IsFinite(uv) || uv.x < 0.0f || uv.y < 0.0f || uv.x >= 1.0f || uv.y >= 1.0f)
		return false;
	const f32 x = uv.x * 2.0f - 1.0f;
	const f32 y = 1.0f - uv.y * 2.0f;
	const vec4 nearPoint = inverseViewProjection * vec4(x, y, 0.0f, 1.0f);
	const vec4 farPoint = inverseViewProjection * vec4(x, y, 1.0f, 1.0f);
	if (!std::isfinite(nearPoint.w) || !std::isfinite(farPoint.w) || nearPoint.w == 0.0f || farPoint.w == 0.0f)
		return false;
	const vec3 origin = vec3(nearPoint) / nearPoint.w;
	const vec3 delta = vec3(farPoint) / farPoint.w - origin;
	const f32 length = glm::length(delta);
	if (!IsFinite(origin) || !IsFinite(delta) || !std::isfinite(length) || length <= 0.0f)
		return false;
	outRay = Ray(origin, delta / length);
	if (outMaxDistance) *outMaxDistance = length;
	return true;
}

Entity* PickingManager::PickEntityByRay(const Ray& ray, f32 maxDistance, HitResult* outHitResult, bool testTriangles) const
{
	if (outHitResult) outHitResult->Reset();
	if (!std::isfinite(maxDistance) || maxDistance < 0.0f) return nullptr;

	Entity* closest = nullptr;
	f32 closestDistance = maxDistance;

	for (Scene* scene : SceneManager::Get().GetActiveScenes())
	{
		if (!scene) continue;
		auto& world = scene->GetRegistry();
		TransformSystem::UpdateHierarchy(world);
		for (auto [entity, transform, flag] : world.view<WorldTransformComponent, FlagComponent>().each())
		{
			if (!HasFlag(flag.flags, EEntityFlag::Active) || !HasFlag(flag.flags, EEntityFlag::Visible) ||
				HasFlag(flag.flags, EEntityFlag::Dead)) continue;
			Entity* candidate = scene->FindEntity(entity);
			if (!candidate) continue;

			const auto testMesh = [&](Mesh* mesh, const mat4& matrix)
				{
					if (!mesh) return;
					HitResult hit{};
					const bool hasHit = testTriangles
						? Collision::Raycast(ray, *mesh, matrix, hit, closestDistance)
						: Collision::Raycast(ray, AABB(mesh->GetMin(), mesh->GetMax()), matrix, hit, closestDistance);
					if (hasHit && (!closest || hit.distance < closestDistance))
					{
						closest = candidate;
						closestDistance = hit.distance;
						if (outHitResult)
						{
							*outHitResult = hit;
							outHitResult->userData = candidate;
						}
					}
				};

			if (const auto* renderer = world.try_get<StaticMeshRendererComponent>(entity))
			{
				if (renderer->materialHandle.Get()) testMesh(renderer->meshHandle.Get(), transform.worldMatrix);
			}
			if (const auto* renderer = world.try_get<SpriteRendererComponent>(entity))
			{
				mat4 quadMatrix;
				if (renderer->drawMode == ESpriteDrawMode::Simple && renderer->spriteHandle.Get() &&
					renderer->materialHandle.Get() && renderer->spriteHandle.Get()->TryGetQuadMatrix(quadMatrix))
					testMesh(renderer->meshHandle.Get(), transform.worldMatrix * quadMatrix);
			}
		}
	}
	return closest;
}