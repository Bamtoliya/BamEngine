#pragma once

#include "ComponentAnnotations.h"
#include "Types.h"
#include <algorithm>
#include <cmath>

namespace Engine
{
	STRUCT(COMPONENT, CATEGORY("CATEGORY_GAMEPLAY"))
	struct HealthComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, RANGE(1.0f, 100000.0f))
		f32 maxHealth = 100.0f;

		PROPERTY(EDITABLE, RANGE(0.0f, 100000.0f))
		f32 currentHealth = 100.0f;

		PROPERTY(EDITABLE)
		bool invulnerable = false;

		PROPERTY(READONLY)
		bool isDead = false;

		f32 ApplyDamage(f32 amount)
		{
			if (isDead || invulnerable || !std::isfinite(amount) || amount <= 0.0f)
				return 0.0f;

			const f32 prev = std::max(0.0f, currentHealth);
			currentHealth = std::max(0.0f, prev - amount);
			if (currentHealth <= 0.0f)
			{
				currentHealth = 0.0f;
				isDead = true;
			}
			return prev - currentHealth;
		}

		f32 Heal(f32 amount)
		{
			if (isDead || !std::isfinite(amount) || amount <= 0.0f)
				return 0.0f;

			const f32 cap = std::max(0.0f, maxHealth);
			const f32 prev = std::clamp(currentHealth, 0.0f, cap);
			currentHealth = std::min(cap, prev + amount);
			return currentHealth - prev;
		}

		void ResetHealth()
		{
			maxHealth = std::max(0.0f, maxHealth);
			currentHealth = maxHealth;
			isDead = (currentHealth <= 0.0f);
		}

		f32 GetHealthRatio() const
		{
			return maxHealth > 0.0f ? std::clamp(currentHealth / maxHealth, 0.0f, 1.0f) : 0.0f;
		}
	};
}
