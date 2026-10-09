#include "TransformSystem.h"
#include "CoreComponents.h"
#include <entt/entt.hpp>

EResult TransformSystem::Initialize(void* arg)
{
	return EResult();
}

void TransformSystem::Free()
{
}

TransformSystem* TransformSystem::Create(void* arg)
{
	TransformSystem* instance = new TransformSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		instance = nullptr;
		return nullptr;
	}
	return instance;
}

void TransformSystem::OnLateUpdate(entt::registry& globalRegistry, const vector<Scene*> activeScenes, f32 dt)
{
	UpdateHierarchy(globalRegistry);

	for (Scene* scene : activeScenes)
	{
		if (scene && !scene->IsPaused())
		{
			UpdateHierarchy(scene->GetRegistry());
		}
	}
}

void TransformSystem::UpdateHierarchy(entt::registry& registry)
{
	auto transforms = registry.view<TransformComponent>();

	for (auto entity : transforms)
	{
		auto& transform = transforms.get<TransformComponent>(entity);

		// 실패한 회전 입력은 컴포넌트 내부에서 이전 상태로 복구합니다.
		static_cast<void>(transform.SynchronizeRotation());
	}

	auto view = registry.view<TransformComponent, WorldTransformComponent>();

	for (auto entity : view)
	{
		// 부모가 있는 엔티티는 루트에서 재귀적으로 처리하므로 건너뜀
		if (registry.any_of<HierarchyComponent>(entity))
		{
			auto& hierarchy = registry.get<HierarchyComponent>(entity);
			if (hierarchy.parent != entt::null)
				continue;
		}
		auto& transform = registry.get<TransformComponent>(entity);
		auto& worldTransform = registry.get<WorldTransformComponent>(entity);
		const mat4 localMatrix = transform.GetLocalMatrix();
		worldTransform.worldMatrix = localMatrix;
		// 자식들이 있다면 재귀적으로 갱신
		UpdateChildWorldMatrix(registry, entity, localMatrix);
	}
}

void TransformSystem::UpdateChildWorldMatrix(entt::registry& registry, entt::entity entity, const mat4& parentWorldMatrix)
{
	if (!registry.any_of<HierarchyComponent>(entity))
		return;
	auto& hierarchy = registry.get<HierarchyComponent>(entity);
	for (auto child : hierarchy.children)
	{
		if (registry.all_of<TransformComponent, WorldTransformComponent>(child))
		{
			auto& childTransform = registry.get<TransformComponent>(child);
			auto& childWorldTransform = registry.get<WorldTransformComponent>(child);
			const mat4 localMatrix = childTransform.GetLocalMatrix();
			childWorldTransform.worldMatrix = parentWorldMatrix * localMatrix;
			// 이 자식의 자식들(손주)도 재귀적으로 갱신
			UpdateChildWorldMatrix(registry, child, childWorldTransform.worldMatrix);
		}
	}
}
