#pragma once

#include "Base.h"
#include "Macro.h"
#include "Result.h"
#include "AudioMixer.h"

BEGIN(FMOD)
class System;
class Channel;
class Sound;
class ChannelGroup;
END

BEGIN(Engine)

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
	// 사운드 재생 편의 인터페이스 (2D 원샷 사운드)
	EResult PlayOneShot(FMOD::Sound* sound, f32 volume = 1.f, EAudioBus bus = EAudioBus::SFX);

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

private:
	FMOD::System* m_System = { nullptr };
	FMOD::ChannelGroup* m_MasterGroup = { nullptr };
	FMOD::ChannelGroup* m_BGMGroup = { nullptr };
	FMOD::ChannelGroup* m_SFXGroup = { nullptr };
	FMOD::ChannelGroup* m_UIGroup = { nullptr };
	FMOD::ChannelGroup* m_VoiceGroup = { nullptr };

	AudioMixer m_Mixer;
};

END
