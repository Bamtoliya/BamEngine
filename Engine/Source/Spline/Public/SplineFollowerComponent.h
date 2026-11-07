#pragma once

#include "Engine_Includes.h"
#include "ComponentAnnotations.h"
#include <entt/entity/entity.hpp>

namespace Engine
{
	class SplineFollowSystem;

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_SPLINE_FOLLOWER"), CATEGORY("CATEGORY_SPLINE"),
		REQUIRE_COMPONENTS(TransformComponent, WorldTransformComponent))
		struct ENGINE_API SplineFollowerComponent
	{
		REFLECT_BODY()

			// 음수이면 역방향으로 이동합니다.
			PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPLINE_SPEED"))
			f32 Speed = 2.0f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_SPLINE_START_DISTANCE"))
			f64 StartDistance = 0.0;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_LOOPING"))
			bool Loop = false;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_IS_PLAYING"))
			bool IsPlaying = true;

		PROPERTY(READONLY, NOSERIALIZE, DISPLAY_NAME("PROP_SPLINE_CURRENT_DISTANCE"))
			f64 CurrentDistance = 0.0;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_SPLINE_ALIGN_TO_MOVEMENT"))
			bool AlignToMovement = false;

		void Play() { IsPlaying = true; }
		void Pause() { IsPlaying = false; }
		void Restart() { m_Initialized = false; IsPlaying = true; }
		void Stop() { m_Initialized = false; IsPlaying = false; }

	private:
		friend class SplineFollowSystem;

		bool m_Initialized = false;
		entt::entity m_BoundPath = entt::null;
		std::string m_LastFailure;
	};
}