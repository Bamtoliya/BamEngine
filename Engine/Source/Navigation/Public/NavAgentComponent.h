#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"
#include <glm/glm.hpp>
#include <vector>
#include <cstddef>

namespace Engine
{
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_NAV_AGENT"), CATEGORY("CATEGORY_NAVIGATION"))
	struct NavAgentComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_ENABLED"))
		bool enabled = true;

		static constexpr auto ComponentEnableMember = &NavAgentComponent::enabled;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_MOVE_SPEED"), RANGE(0.0f, 100.0f))
		f32 moveSpeed = 3.5f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_STOPPING_DISTANCE"), RANGE(0.01f, 10.0f))
		f32 stoppingDistance = 0.25f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_AGENT_RADIUS"), RANGE(0.01f, 10.0f))
		f32 agentRadius = 0.4f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TARGET_POSITION"))
		vec3 targetPosition = vec3(0.0f);

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_HAS_DESTINATION"))
		bool hasDestination = false;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_AUTO_REPATH"))
		bool autoRepath = false;

		// 비직렬화 런타임 상태
		std::vector<vec3> path;
		size_t currentWaypointIndex = 0;
		bool pathPending = false;
		bool reachedDestination = false;
		vec3 velocity = vec3(0.0f);

		void SetDestination(const vec3& dest)
		{
			targetPosition = dest;
			hasDestination = true;
			pathPending = true;
			reachedDestination = false;
			currentWaypointIndex = 0;
		}

		void Stop()
		{
			hasDestination = false;
			pathPending = false;
			reachedDestination = false;
			path.clear();
			currentWaypointIndex = 0;
			velocity = vec3(0.0f);
		}

		[[nodiscard]] bool HasActivePath() const
		{
			return hasDestination && !reachedDestination && currentWaypointIndex < path.size();
		}
	};
}
