#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"
#include <entt/entity/entity.hpp>
#include <algorithm>
#include <string>
#include <vector>

namespace Engine
{
	STRUCT(COMPONENT, CATEGORY("CATEGORY_GAMEPLAY"))
	struct HitboxComponent
	{
		REFLECT_BODY()

		static constexpr auto ComponentEnableMember = &HitboxComponent::active;

		PROPERTY(EDITABLE, RANGE(0.0f, 100000.0f))
		f32 damage = 10.0f;

		PROPERTY(EDITABLE, RANGE(0.0f, 10000.0f))
		f32 knockbackForce = 0.0f;

		PROPERTY(EDITABLE)
		std::string teamTag;

		PROPERTY(EDITABLE)
		bool destroyOnHit = false;

		PROPERTY(EDITABLE)
		bool active = true;

		PROPERTY(EDITABLE, ALLOW_MISSING, RANGE(0.05f, 100.0f))
		f32 fallbackRadius = 1.0f;

		// Transient hit tracking for swing deduplication (not serialized)
		std::vector<uint32> hitEntitiesInSwing;

		bool HasHit(entt::entity target) const
		{
			const uint32 id = static_cast<uint32>(target);
			return std::find(hitEntitiesInSwing.begin(), hitEntitiesInSwing.end(), id) != hitEntitiesInSwing.end();
		}

		bool HasHit(uint32 targetId) const
		{
			return std::find(hitEntitiesInSwing.begin(), hitEntitiesInSwing.end(), targetId) != hitEntitiesInSwing.end();
		}

		void RecordHit(entt::entity target)
		{
			const uint32 id = static_cast<uint32>(target);
			if (!HasHit(id))
			{
				hitEntitiesInSwing.push_back(id);
			}
		}

		void RecordHit(uint32 targetId)
		{
			if (!HasHit(targetId))
			{
				hitEntitiesInSwing.push_back(targetId);
			}
		}

		void ResetSwingHits()
		{
			hitEntitiesInSwing.clear();
		}

		void SetActive(bool inActive)
		{
			ResetSwingHits();
			active = inActive;
		}
	};
}
