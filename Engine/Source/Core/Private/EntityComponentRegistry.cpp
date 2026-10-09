#include "EntityComponentRegistry.h"

#include "CoreComponents.h"
#include "Resources.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"
#include "TransformSystem.h"

#include <stdexcept>

namespace
{
	template<typename T>
	bool AddSpatialComponent(entt::registry& world, entt::entity entity)
	{
		world.get_or_emplace<Engine::TransformComponent>(entity);
		world.get_or_emplace<Engine::WorldTransformComponent>(entity);
		world.get_or_emplace<Engine::FlagComponent>(entity);
		world.get_or_emplace<T>(entity);

		Engine::TransformSystem::UpdateHierarchy(world);
		return true;
	}
}

namespace Engine
{
	const EntityComponentRegistry& GetEntityComponentRegistry()
	{
		static const EntityComponentRegistry registry = []
			{
				EntityComponentRegistry result;

				const bool registered =
					result.Register<NameComponent>("Name", "Core") &&
					result.Register<TagComponent>("Tag", "Core") &&
					result.Register<FlagComponent>("Flags", "Core") &&
					result.Register<TransformComponent>("Transform", "Core", &AddSpatialComponent<TransformComponent>) &&
					result.Register<CameraComponent>("Camera", "Rendering", &AddSpatialComponent<CameraComponent>) &&
					result.Register<LightComponent>("Light", "Rendering", &AddSpatialComponent<LightComponent>) &&
					result.Register<StaticMeshRendererComponent>(
						"Static Mesh Renderer", "Rendering", &AddSpatialComponent<StaticMeshRendererComponent>) &&
					result.Register<SpriteRendererComponent>(
						"Sprite Renderer", "Rendering", &AddSpatialComponent<SpriteRendererComponent>) &&
					result.Register<SkinnedMeshRendererComponent>(
						"Skinned Mesh Renderer", "Rendering", &AddSpatialComponent<SkinnedMeshRendererComponent>) &&
					result.Register<RigidBodyComponent>("Rigid Body", "Physics 3D", &AddSpatialComponent<RigidBodyComponent>) &&
					result.Register<ColliderComponent>("Collider", "Physics 3D", &AddSpatialComponent<ColliderComponent>) &&
					result.Register<JointComponent>("Joint", "Physics 3D", &AddSpatialComponent<JointComponent>) &&
					result.Register<RigidBody2DComponent>(
						"Rigid Body 2D", "Physics 2D", &AddSpatialComponent<RigidBody2DComponent>) &&
					result.Register<Collider2DComponent>(
						"Collider 2D", "Physics 2D", &AddSpatialComponent<Collider2DComponent>) &&
					result.Register<Joint2DComponent>("Joint 2D", "Physics 2D", &AddSpatialComponent<Joint2DComponent>);

				if (!registered)
					throw std::logic_error("Entity component registration failed.");

				return result;
			}();

		return registry;
	}
}