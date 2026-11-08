#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "CompositeEffect.h"
#include "Components/Render/ParticleEffectComponent.h"
#include <vector>

namespace Engine
{
	// 개별 파티클 트랙의 런타임 시뮬레이션 상태
	struct VFXParticleTrackRuntime
	{
		std::vector<ParticleEmitterInstance> emitterInstances;
		bool isInitialized = false;

		void Reset()
		{
			for (auto& inst : emitterInstances)
				inst.Reset();
			isInitialized = false;
		}
	};

	STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_COMPOSITE_VFX"), CATEGORY("CATEGORY_RENDER"))
	struct CompositeVFXComponent
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_EFFECT_HANDLE"))
		ResourceHandle<CompositeEffect> effectHandle;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_IS_PLAYING"))
		bool isPlaying = true;

		PROPERTY(EDITABLE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_IS_LOOPING"))
		bool isLooping = false;

		PROPERTY(EDITABLE, ALLOW_MISSING, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_PLAY_RATE"))
		f32 playRate = 1.0f;

		PROPERTY(READONLY, NOSERIALIZE, CATEGORY("CATEGORY_RENDER"), DISPLAY_NAME("PROP_CURRENT_TIME"))
		f32 currentTime = 0.0f;

		f32 prevTime = 0.0f;
		bool isFinished = false;

		// 런타임 파티클 트랙 인스턴스 배열 (effectHandle의 m_ParticleTracks와 1:1 대응)
		std::vector<VFXParticleTrackRuntime> particleRuntimes;

		// 런타임 동적 조명 합성 상태
		vec3 activeLightColor = vec3(0.0f);
		f32 activeLightIntensity = 0.0f;
		f32 activeLightRadius = 0.0f;
		vec3 activeLightOffset = vec3(0.0f);
		bool hasActiveLight = false;

		void Play()
		{
			isPlaying = true;
			isFinished = false;
		}

		void Pause()
		{
			isPlaying = false;
		}

		void Stop()
		{
			isPlaying = false;
			currentTime = 0.0f;
			prevTime = 0.0f;
			isFinished = false;
			ResetRuntimes();
		}

		void Restart()
		{
			currentTime = 0.0f;
			prevTime = 0.0f;
			isPlaying = true;
			isFinished = false;
			ResetRuntimes();
		}

		void ResetRuntimes()
		{
			for (auto& rt : particleRuntimes)
				rt.Reset();
			hasActiveLight = false;
		}
	};
}
