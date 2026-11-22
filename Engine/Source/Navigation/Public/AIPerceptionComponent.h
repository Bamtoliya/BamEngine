#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"
#include <entt/entity/entity.hpp>
#include <glm/glm.hpp>
#include <string>

namespace Engine
{
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_AI_PERCEPTION"), CATEGORY("CATEGORY_AI"))
	struct AIPerceptionComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
		bool enabled = true;

		static constexpr auto ComponentEnableMember = &AIPerceptionComponent::enabled;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SIGHT_RANGE"), RANGE(0.0f, 1000.0f))
		f32 sightRange = 12.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SIGHT_HALF_ANGLE"), RANGE(0.0f, 180.0f))
		f32 sightHalfAngleDegrees = 60.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_HEARING_RANGE"), RANGE(0.0f, 1000.0f))
		f32 hearingRange = 6.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TARGET_TAG"))
		std::string targetTag = "Player";

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_REQUIRE_LOS"))
		bool requireLineOfSight = true;

		// 비직렬화 런타임 상태
		entt::entity detectedTarget = entt::null;
		vec3 lastKnownTargetPos = vec3(0.0f);
		bool hasLineOfSight = false;
		f32 timeSinceLastSeen = 0.0f;

		[[nodiscard]] bool HasTarget() const
		{
			return detectedTarget != entt::null;
		}

		void ClearTarget()
		{
			detectedTarget = entt::null;
			lastKnownTargetPos = vec3(0.0f);
			hasLineOfSight = false;
			timeSinceLastSeen = 0.0f;
		}
	};
}
