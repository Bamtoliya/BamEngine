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
	return nullptr;
}

void TransformSystem::OnUpdate(entt::registry& registry, f32 dt)
{
	auto view = registry.view<TransformComponent>();
	for (auto entity : view)
	{
		auto& transform = view.get<TransformComponent>(entity);
	}
}

void TransformSystem::UpdateHierarchy(entt::registry& registry)
{
	auto view = registry.view<TransformComponent, WorldTransformComponent>();

	for (auto entity : view)
	{
		// 부모가 있는 엔티티는 루트에서 재귀적으로 처리하므로 건너뜀
		if (registry.any_of<HierarchyComponent>(entity))
		{
			auto& hierarchy = registry.get<HierarchyComponent>(entity);
			if (hierarchy.Parent != entt::null)
				continue;
		}
		auto& transform = registry.get<TransformComponent>(entity);
		auto& worldTransform = registry.get<WorldTransformComponent>(entity);
		// 로컬 매트릭스 계산 (Translation * Rotation * Scale)
		mat4 translation = glm::translate(mat4(1.0f), transform.Position);
		mat4 rotation = glm::mat4_cast(transform.Rotation);
		mat4 scale = glm::scale(mat4(1.0f), transform.Scale);

		mat4 localMatrix = translation * rotation * scale;
		worldTransform.WorldMatrix = localMatrix;
		// 자식들이 있다면 재귀적으로 갱신
		UpdateChildWorldMatrix(registry, entity, localMatrix);
	}
}

void TransformSystem::UpdateChildWorldMatrix(entt::registry& registry, entt::entity entity, const mat4& parentWorldMatrix)
{
	if (!registry.any_of<HierarchyComponent>(entity))
		return;
	auto& hierarchy = registry.get<HierarchyComponent>(entity);
	for (auto child : hierarchy.Children)
	{
		if (registry.all_of<TransformComponent, WorldTransformComponent>(child))
		{
			auto& childTransform = registry.get<TransformComponent>(child);
			auto& childWorldTransform = registry.get<WorldTransformComponent>(child);
			// 자식의 로컬 매트릭스 계산
			mat4 translation = glm::translate(mat4(1.0f), childTransform.Position);
			mat4 rotation = glm::mat4_cast(childTransform.Rotation);
			mat4 scale = glm::scale(mat4(1.0f), childTransform.Scale);
			mat4 localMatrix = translation * rotation * scale;
			// 부모의 월드 매트릭스를 곱해 자신의 월드 매트릭스를 완성
			childWorldTransform.WorldMatrix = parentWorldMatrix * localMatrix;
			// 이 자식의 자식들(손주)도 재귀적으로 갱신
			UpdateChildWorldMatrix(registry, child, childWorldTransform.WorldMatrix);
		}
	}
}
