#pragma once

#include "ComponentAnnotations.h"
#include "ResourceHandle.h"
#include "CompositeEffect.h"
#include "ParticleEffectComponent.h"
#include <vector>
namespace FMOD { class Channel; }
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

	// 트레일 스트립 포인트 샘플 (쌍: A = 팁, B = 베이스)
	struct VFXTrailSamplePoint
	{
		vec3 posA = vec3(0.0f);
		vec3 posB = vec3(0.0f);
		f32 age = 0.0f;
	};

	// 개별 트레일 트랙의 런타임 상태
	struct VFXTrailTrackRuntime
	{
		std::vector<VFXTrailSamplePoint> points;
		vec3 lastSpawnPosA = vec3(1e9f);
		vec3 lastSpawnPosB = vec3(1e9f);
		bool hasLastSpawn = false;

		void Reset()
		{
			points.clear();
			hasLastSpawn = false;
			lastSpawnPosA = vec3(1e9f);
			lastSpawnPosB = vec3(1e9f);
		}
	};

	struct VFXSoundChannelStop
	{
		ENGINE_API void operator()(FMOD::Channel* channel) const noexcept;
	};

	struct VFXSoundTrackRuntime
	{
		std::unique_ptr<FMOD::Channel, VFXSoundChannelStop> channel;
		bool triggered = false;

		VFXSoundTrackRuntime() = default;
		VFXSoundTrackRuntime(const VFXSoundTrackRuntime& other) noexcept : triggered(other.triggered) {}
		VFXSoundTrackRuntime& operator=(const VFXSoundTrackRuntime& other) noexcept
		{
			if (this != &other)
			{
				channel.reset();
				triggered = other.triggered;
			}
			return *this;
		}

		VFXSoundTrackRuntime(VFXSoundTrackRuntime&&) noexcept = default;
		VFXSoundTrackRuntime& operator=(VFXSoundTrackRuntime&&) noexcept = default;
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
		bool previewSeeking = false;
		std::optional<uint32> previewSeed;

		// 런타임 파티클 트랙 인스턴스 배열 (effectHandle의 m_ParticleTracks와 1:1 대응)
		std::vector<VFXParticleTrackRuntime> particleRuntimes;

		// 런타임 사운드 트랙 1회 재생 추적 플래그 (effectHandle의 m_SoundTracks와 1:1 대응)
		std::vector<VFXSoundTrackRuntime> soundRuntimes;

		// 런타임 트레일 트랙 인스턴스 배열 (effectHandle의 m_TrailTracks와 1:1 대응)
		std::vector<VFXTrailTrackRuntime> trailRuntimes;

		// 런타임 카메라 셰이크 오프셋 누적값
		vec3 currentCameraShakeOffset = vec3(0.0f);

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

		void StopSoundTracks()
		{
			for (auto& runtime : soundRuntimes)
				runtime.channel.reset();
		}

		void Pause()
		{
			isPlaying = false;
			StopSoundTracks();
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
			for (auto& rt : trailRuntimes)
				rt.Reset();
			soundRuntimes.clear();
			currentCameraShakeOffset = vec3(0.0f);
			hasActiveLight = false;
		}
	};
}
