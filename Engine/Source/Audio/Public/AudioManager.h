#pragma once

#include "Base.h"
#include "Macro.h"
#include "Result.h"
#include "AudioMixer.h"
#include <unordered_map>
#include <vector>

BEGIN(FMOD)
class System;
class Channel;
class Sound;
class ChannelGroup;
END

BEGIN(Engine)

struct ENGINE_API SoundConcurrencyConfig
{
	uint32 MaxConcurrentVoices = 0; // 0이면 무제한
	f32 MinRetriggerInterval = 0.f; // 초 단위, 0 이하이면 쿨다운 없음
};

class ENGINE_API AudioManager final : public Base
{
	DECLARE_SINGLETON(AudioManager)

private:
	AudioManager() = default;
	virtual ~AudioManager() = default;
	EResult Initialize(void* arg = nullptr);

public:
	virtual void Free() override;
	void Update(f32 dt);

public:
	// 사운드 재생 편의 인터페이스 (2D / 3D 원샷 사운드)
	EResult PlayOneShot(FMOD::Sound* sound, f32 volume = 1.f, EAudioBus bus = EAudioBus::SFX, f32 pitch = 1.f);
	EResult PlayOneShot3D(FMOD::Sound* sound, const vec3& position, f32 volume = 1.f, f32 pitch = 1.f, EAudioBus bus = EAudioBus::SFX, f32 minDistance = 1.f, f32 maxDistance = 20.f);

	// BGM 재생 및 크로스페이드 제어
	EResult PlayBGM(FMOD::Sound* sound, f32 volume = 1.f, bool loop = true);
	EResult CrossfadeBGM(FMOD::Sound* sound, f32 fadeDuration, f32 targetVolume = 1.f, bool loop = true);
	void StopBGM();
	void UpdateCrossfade(f32 deltaTime);
	bool IsBGMPlaying() const;
	bool IsCrossfading() const { return m_IsCrossfading; }

	// 사운드별 동시 보이스 수 및 최소 재트리거 간격 설정 (기본: 무제한)
	void SetSoundConcurrency(FMOD::Sound* sound, const SoundConcurrencyConfig& config);
	void SetSoundConcurrency(FMOD::Sound* sound, uint32 maxConcurrentVoices, f32 minRetriggerInterval = 0.f);
	void ClearSoundConcurrency(FMOD::Sound* sound);
	SoundConcurrencyConfig GetSoundConcurrency(FMOD::Sound* sound) const;

	// 믹서 접근
	AudioMixer& GetMixer() { return m_Mixer; }
	const AudioMixer& GetMixer() const { return m_Mixer; }

	// 버스 볼륨 제어
	void SetMasterVolume(f32 volume);
	void SetBGMVolume(f32 volume);
	void SetSFXVolume(f32 volume);
	void SetUIVolume(f32 volume);
	void SetVoiceVolume(f32 volume);

	void SetBusVolume(EAudioBus bus, f32 volume);
	f32 GetBusVolume(EAudioBus bus) const { return m_Mixer.GetBusVolume(bus); }

	void SetBusMuted(EAudioBus bus, bool muted);
	bool IsBusMuted(EAudioBus bus) const { return m_Mixer.IsBusMuted(bus); }

	void FadeBusVolume(EAudioBus bus, f32 targetVolume, f32 durationSeconds);

	f32 GetMasterVolume() const { return m_Mixer.GetBusVolume(EAudioBus::Master); }
	f32 GetBGMVolume() const { return m_Mixer.GetBusVolume(EAudioBus::BGM); }
	f32 GetSFXVolume() const { return m_Mixer.GetBusVolume(EAudioBus::SFX); }
	f32 GetUIVolume() const { return m_Mixer.GetBusVolume(EAudioBus::UI); }
	f32 GetVoiceVolume() const { return m_Mixer.GetBusVolume(EAudioBus::Voice); }

	// FMOD System 포인터 (내부 및 AudioSystem/AudioClip 연동용)
	FMOD::System* GetFMODSystem() const { return m_System; }
	FMOD::ChannelGroup* GetMasterGroup() const { return m_MasterGroup; }
	FMOD::ChannelGroup* GetBGMGroup() const { return m_BGMGroup; }
	FMOD::ChannelGroup* GetSFXGroup() const { return m_SFXGroup; }
	FMOD::ChannelGroup* GetUIGroup() const { return m_UIGroup; }
	FMOD::ChannelGroup* GetVoiceGroup() const { return m_VoiceGroup; }
	FMOD::ChannelGroup* GetBusGroup(EAudioBus bus) const;

	void StopAllSounds();

private:
	void SyncBusGroups();
	EResult PlayOneShotInternal(FMOD::Sound* sound, f32 volume, f32 pitch, EAudioBus bus, bool is3D, const vec3& position, f32 minDistance, f32 maxDistance);

private:
	struct SoundVoiceState
	{
		SoundConcurrencyConfig Config{};
		f32 LastPlayTime = -1.0e9f;
		std::vector<FMOD::Channel*> ActiveChannels;
	};

	FMOD::System* m_System = { nullptr };
	FMOD::ChannelGroup* m_MasterGroup = { nullptr };
	FMOD::ChannelGroup* m_BGMGroup = { nullptr };
	FMOD::ChannelGroup* m_SFXGroup = { nullptr };
	FMOD::ChannelGroup* m_UIGroup = { nullptr };
	FMOD::ChannelGroup* m_VoiceGroup = { nullptr };

	AudioMixer m_Mixer;
	f32 m_ElapsedTime = 0.f;
	std::unordered_map<FMOD::Sound*, SoundVoiceState> m_SoundStates;

	// BGM 및 크로스페이드 상태
	FMOD::Channel* m_CurrentBGMChannel = { nullptr };
	FMOD::Sound* m_CurrentBGMSound = { nullptr };
	f32 m_CurrentBGMVolume = 1.f;

	FMOD::Channel* m_NextBGMChannel = { nullptr };
	FMOD::Sound* m_NextBGMSound = { nullptr };
	bool m_IsCrossfading = false;
	f32 m_CrossfadeDuration = 0.f;
	f32 m_CrossfadeElapsed = 0.f;
	f32 m_CrossfadeStartVolume = 1.f;
	f32 m_CrossfadeTargetVolume = 1.f;
};

END
