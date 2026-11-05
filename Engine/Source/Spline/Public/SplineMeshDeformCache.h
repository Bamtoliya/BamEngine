#pragma once

#include "Engine_Includes.h"
#include "Structs/BoundingBox.h"
#include "SplineMeshComponent.h"

namespace Engine
{
	class Mesh;

	struct SplineDeformedMeshInstance
	{
		std::shared_ptr<Mesh> Geometry;
		mat4 WorldMatrix{ 1.0f };
		BoundingBox WorldBounds;
		ESplineMeshBuildStatus BuildStatus = ESplineMeshBuildStatus::DeformationFailed;
	};

	class ENGINE_API SplineMeshDeformCache final
	{
	public:
		static SplineDeformedMeshInstance GetOrBuild(entt::registry& world, entt::entity entity,
			const mat4& worldMatrix);

		static SplineDeformedMeshInstance Find(const entt::registry& world, entt::entity entity,
			const mat4& worldMatrix);

		static void Prune(entt::registry& world);
	};
}