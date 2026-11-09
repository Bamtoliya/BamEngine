#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "ParticleEffect.h"
#include <random>
#include <optional>
#include <vector>

namespace Engine
{
	// 개별 이미터의 런타임 시뮬레이션 상태 인스턴스
	struct ParticleEmitterInstance
	{
		std::vector<Particle> particles;
		std::vector<ParticleCollisionEvent> collisionEvents;
		f32 emissionAccumulator = 0.0f;
		f32 burstTimer = 0.0f;
		f64 elapsedTime = 0.0;
		std::mt19937 rng;
		bool rngInitialized = false;

		void Reset()
		{
			particles.clear();
			collisionEvents.clear();
			emissionAccumulator = 0.0f;
			burstTimer = 0.0f;
			elapsedTime = 0.0;
		}

		void ClearEvents()
		{
			collisionEvents.clear();
		}
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_PARTICLE_EFFECT"), CATEGORY("CATEGORY_RENDER"))
	struct ParticleEffectComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_EFFECT_HANDLE"))
		ResourceHandle<ParticleEffect> effectHandle;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_PLAY_STATE"))
		EParticlePlayState playState = EParticlePlayState::Playing;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_PLAY_RATE"))
		f32 playRate = 1.0f;

		// 런타임 이미터 인스턴스 배열 (effectHandle의 m_Emitters 크기와 1:1 동기화)
		std::vector<ParticleEmitterInstance> instances;
		bool isInitialized = false;
		std::optional<uint32> previewSeed;

		// 런타임 제어 API
		void Play() { playState = EParticlePlayState::Playing; }
		void Pause() { playState = EParticlePlayState::Paused; }
		void Stop() { playState = EParticlePlayState::Stopped; }
		void Reset()
		{
			for (auto& inst : instances)
				inst.Reset();
		}
	};
}
