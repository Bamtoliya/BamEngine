#pragma once

#include "ComponentAnnotations.h"
#include "SceneEntityReference.h"
#include "Types.h"

namespace Engine
{
	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_ATTACK_CHALLENGE"), CATEGORY("CATEGORY_GAMEPLAY"))
	struct AttackChallengeComponent
	{
		REFLECT_BODY()

		static constexpr int32 MaxRequiredHits = 12;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_TARGET_ENTITY"))
		SceneEntityReference TargetEntity;

		PROPERTY(EDITABLE, RANGE(1, 12), DISPLAY_NAME("PROP_CHALLENGE_REQUIRED_HITS"))
		int32 RequiredHits = 3;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CHALLENGE_HIT_RADIUS"))
		f32 HitRadius = 2.5f;

		PROPERTY(EDITABLE, DISPLAY_NAME("PROP_CHALLENGE_HIT_HEIGHT"))
		f32 HitHeight = 2.0f;

		PROPERTY(EDITABLE, ALLOW_MISSING, DISPLAY_NAME("PROP_USE_GRAPH_ATTACK_INPUT"))
		bool UseGraphAttackInput = false;
	};
}
