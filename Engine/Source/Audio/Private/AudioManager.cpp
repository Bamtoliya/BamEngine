#include "AudioManager.h"
#include "Logger.h"
#include <fmod.hpp>
#include <fmod_errors.h>
#include <algorithm>
#include <cmath>

IMPLEMENT_SINGLETON(AudioManager)

#define FMOD_CHECK(expr) \
	do { \
		FMOD_RESULT _res = (expr); \
		if (_res != FMOD_OK) { \
			BAM_LOG(Error, "Engine","AudioManager", "FMOD Error ({}) {}: {}", static_cast<int>(_res), #expr, FMOD_ErrorString(_res)); \
		} \
	} while (0)

EResult AudioManager::Initialize(void* arg)
{
	(void)arg;
	FMOD_RESULT result = FMOD::System_Create(&m_System);
	if (result != FMOD_OK || !m_System)
	{
		BAM_LOG(Error, "Engine","AudioManager", "Failed to create FMOD System: {}", FMOD_ErrorString(result));
		return EResult::Fail;
	}

	// 최대 가상 채널 512개, 기본 초기화 플래그
	result = m_System->init(512, FMOD_INIT_NORMAL, nullptr);
	if (result != FMOD_OK)
	{
		BAM_LOG(Error, "Engine","AudioManager", "Failed to initialize FMOD System: {}", FMOD_ErrorString(result));
		m_System->release();
		m_System = nullptr;
		return EResult::Fail;
	}

	// 채널 그룹(Master, BGM, SFX, UI, Voice) 생성
	FMOD_CHECK(m_System->getMasterChannelGroup(&m_MasterGroup));
	FMOD_CHECK(m_System->createChannelGroup("BGM", &m_BGMGroup));
	FMOD_CHECK(m_System->createChannelGroup("SFX", &m_SFXGroup));
	FMOD_CHECK(m_System->createChannelGroup("UI", &m_UIGroup));
	FMOD_CHECK(m_System->createChannelGroup("Voice", &m_VoiceGroup));

	if (m_MasterGroup)
	{
		if (m_BGMGroup) FMOD_CHECK(m_MasterGroup->addGroup(m_BGMGroup));
		if (m_SFXGroup) FMOD_CHECK(m_MasterGroup->addGroup(m_SFXGroup));
		if (m_UIGroup) FMOD_CHECK(m_MasterGroup->addGroup(m_UIGroup));
		if (m_VoiceGroup) FMOD_CHECK(m_MasterGroup->addGroup(m_VoiceGroup));
	}

	SyncBusGroups();

	BAM_LOG(Info, "Engine","AudioManager", "FMOD Audio System successfully initialized");
	return EResult::Success;
}

void AudioManager::Free()
{
	StopBGM();
	m_SoundStates.clear();

	if (m_VoiceGroup) { m_VoiceGroup->release(); m_VoiceGroup = nullptr; }
	if (m_UIGroup) { m_UIGroup->release(); m_UIGroup = nullptr; }
	if (m_SFXGroup) { m_SFXGroup->release(); m_SFXGroup = nullptr; }
	if (m_BGMGroup) { m_BGMGroup->release(); m_BGMGroup = nullptr; }
	m_MasterGroup = nullptr;

	if (m_System)
	{
		m_System->close();
		m_System->release();
		m_System = nullptr;
		BAM_LOG(Info, "Engine","AudioManager", "FMOD Audio System released");
	}
}

static void PruneInactiveChannels(std::vector<FMOD::Channel*>& channels)
{
	std::erase_if(channels, [](FMOD::Channel* ch)
	{
		if (!ch) return true;
		bool isPlaying = false;
		return ch->isPlaying(&isPlaying) != FMOD_OK || !isPlaying;
	});
}

void AudioManager::Update(f32 dt)
{
	if (std::isfinite(dt) && dt > 0.f)
	{
		m_ElapsedTime += dt;
	}

	m_Mixer.Update(dt);
	UpdateCrossfade(dt);
	SyncBusGroups();

	for (auto& [sound, state] : m_SoundStates)
	{
		(void)sound;
		PruneInactiveChannels(state.ActiveChannels);
	}

	if (m_System)
	{
		m_System->update();
	}
}

FMOD::ChannelGroup* AudioManager::GetBusGroup(EAudioBus bus) const
{
	switch (bus)
	{
	case EAudioBus::Master: return m_MasterGroup;
	case EAudioBus::BGM:    return m_BGMGroup;
	case EAudioBus::SFX:    return m_SFXGroup;
	case EAudioBus::UI:     return m_UIGroup;
	case EAudioBus::Voice:  return m_VoiceGroup;
	default:                return m_SFXGroup ? m_SFXGroup : m_MasterGroup;
	}
}

void AudioManager::SyncBusGroups()
{
	if (m_MasterGroup)
	{
		m_MasterGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::Master));
		m_MasterGroup->setMute(m_Mixer.IsBusMuted(EAudioBus::Master));
	}
	if (m_BGMGroup)
	{
		m_BGMGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::BGM));
		m_BGMGroup->setMute(m_Mixer.IsBusMuted(EAudioBus::BGM));
	}
	if (m_SFXGroup)
	{
		m_SFXGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::SFX));
		m_SFXGroup->setMute(m_Mixer.IsBusMuted(EAudioBus::SFX));
	}
	if (m_UIGroup)
	{
		m_UIGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::UI));
		m_UIGroup->setMute(m_Mixer.IsBusMuted(EAudioBus::UI));
	}
	if (m_VoiceGroup)
	{
		m_VoiceGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::Voice));
		m_VoiceGroup->setMute(m_Mixer.IsBusMuted(EAudioBus::Voice));
	}
}

EResult AudioManager::PlayOneShotInternal(FMOD::Sound* sound, f32 volume, f32 pitch, EAudioBus bus, bool is3D, const vec3& position, f32 minDistance, f32 maxDistance)
{
	if (!m_System || !sound) return EResult::InvalidArgument;

	auto stateIt = m_SoundStates.find(sound);
	if (stateIt != m_SoundStates.end())
	{
		auto& state = stateIt->second;
		PruneInactiveChannels(state.ActiveChannels);

		if (state.Config.MinRetriggerInterval > 0.f && (m_ElapsedTime - state.LastPlayTime) < state.Config.MinRetriggerInterval)
		{
			return EResult::Success;
		}
		if (state.Config.MaxConcurrentVoices > 0 && state.ActiveChannels.size() >= state.Config.MaxConcurrentVoices)
		{
			return EResult::Success;
		}
	}

	FMOD::ChannelGroup* group = GetBusGroup(bus);
	FMOD::Channel* channel = nullptr;
	FMOD_RESULT res = m_System->playSound(sound, group, true, &channel);
	if (res != FMOD_OK) return EResult::Fail;

	if (channel)
	{
		const f32 clampedVol = std::isfinite(volume) ? std::clamp(volume, 0.f, 1.f) : 1.f;
		const f32 clampedPitch = (std::isfinite(pitch) && pitch > 0.f) ? std::clamp(pitch, 0.01f, 100.f) : 1.f;
		channel->setVolume(clampedVol);
		channel->setPitch(clampedPitch);
		channel->setMode(FMOD_LOOP_OFF);

		if (is3D)
		{
			const f32 safeMinDist = (std::isfinite(minDistance) && minDistance >= 0.f) ? minDistance : 1.f;
			const f32 safeMaxDist = (std::isfinite(maxDistance) && maxDistance > safeMinDist) ? maxDistance : (safeMinDist + 19.f);
			const FMOD_VECTOR fPos{ position.x, position.y, position.z };
			const FMOD_VECTOR fVel{ 0.f, 0.f, 0.f };
			channel->setMode(FMOD_3D | FMOD_3D_LINEARROLLOFF);
			channel->set3DAttributes(&fPos, &fVel);
			channel->set3DMinMaxDistance(safeMinDist, safeMaxDist);
			channel->set3DSpread(0.f);
		}
		else
		{
			channel->setMode(FMOD_2D);
		}

		channel->setPaused(false);
	}

	if (stateIt != m_SoundStates.end())
	{
		stateIt->second.LastPlayTime = m_ElapsedTime;
		if (channel) stateIt->second.ActiveChannels.push_back(channel);
	}

	return EResult::Success;
}

EResult AudioManager::PlayOneShot(FMOD::Sound* sound, f32 volume, EAudioBus bus, f32 pitch)
{
	return PlayOneShotInternal(sound, volume, pitch, bus, false, vec3(0.f), 1.f, 20.f);
}

EResult AudioManager::PlayOneShot3D(FMOD::Sound* sound, const vec3& position, f32 volume, f32 pitch, EAudioBus bus, f32 minDistance, f32 maxDistance)
{
	return PlayOneShotInternal(sound, volume, pitch, bus, true, position, minDistance, maxDistance);
}

void AudioManager::SetSoundConcurrency(FMOD::Sound* sound, const SoundConcurrencyConfig& config)
{
	if (!sound) return;
	auto& state = m_SoundStates[sound];
	state.Config = config;
}

void AudioManager::SetSoundConcurrency(FMOD::Sound* sound, uint32 maxConcurrentVoices, f32 minRetriggerInterval)
{
	SetSoundConcurrency(sound, SoundConcurrencyConfig{ maxConcurrentVoices, minRetriggerInterval });
}

void AudioManager::ClearSoundConcurrency(FMOD::Sound* sound)
{
	if (!sound) return;
	m_SoundStates.erase(sound);
}

SoundConcurrencyConfig AudioManager::GetSoundConcurrency(FMOD::Sound* sound) const
{
	if (!sound) return {};
	auto it = m_SoundStates.find(sound);
	return it != m_SoundStates.end() ? it->second.Config : SoundConcurrencyConfig{};
}

void AudioManager::SetMasterVolume(f32 volume)
{
	m_Mixer.SetBusVolume(EAudioBus::Master, volume);
	if (m_MasterGroup) m_MasterGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::Master));
}

void AudioManager::SetBGMVolume(f32 volume)
{
	m_Mixer.SetBusVolume(EAudioBus::BGM, volume);
	if (m_BGMGroup) m_BGMGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::BGM));
}

void AudioManager::SetSFXVolume(f32 volume)
{
	m_Mixer.SetBusVolume(EAudioBus::SFX, volume);
	if (m_SFXGroup) m_SFXGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::SFX));
}

void AudioManager::SetUIVolume(f32 volume)
{
	m_Mixer.SetBusVolume(EAudioBus::UI, volume);
	if (m_UIGroup) m_UIGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::UI));
}

void AudioManager::SetVoiceVolume(f32 volume)
{
	m_Mixer.SetBusVolume(EAudioBus::Voice, volume);
	if (m_VoiceGroup) m_VoiceGroup->setVolume(m_Mixer.GetBusVolume(EAudioBus::Voice));
}

void AudioManager::SetBusVolume(EAudioBus bus, f32 volume)
{
	m_Mixer.SetBusVolume(bus, volume);
	FMOD::ChannelGroup* group = GetBusGroup(bus);
	if (group) group->setVolume(m_Mixer.GetBusVolume(bus));
}

void AudioManager::SetBusMuted(EAudioBus bus, bool muted)
{
	m_Mixer.SetBusMuted(bus, muted);
	FMOD::ChannelGroup* group = GetBusGroup(bus);
	if (group) group->setMute(muted);
}

void AudioManager::FadeBusVolume(EAudioBus bus, f32 targetVolume, f32 durationSeconds)
{
	m_Mixer.FadeBusVolume(bus, targetVolume, durationSeconds);
}

void AudioManager::StopAllSounds()
{
	StopBGM();
	if (m_MasterGroup)
	{
		m_MasterGroup->stop();
	}
	for (auto& [sound, state] : m_SoundStates)
	{
		(void)sound;
		state.ActiveChannels.clear();
	}
}

static bool IsChannelPlaying(FMOD::Channel* channel)
{
	if (!channel) return false;
	bool isPlaying = false;
	return channel->isPlaying(&isPlaying) == FMOD_OK && isPlaying;
}

EResult AudioManager::PlayBGM(FMOD::Sound* sound, f32 volume, bool loop)
{
	if (!m_System || !sound) return EResult::InvalidArgument;

	StopBGM();

	const f32 clampedVol = std::isfinite(volume) ? std::clamp(volume, 0.f, 1.f) : 1.f;
	FMOD::Channel* channel = nullptr;
	FMOD_RESULT res = m_System->playSound(sound, m_BGMGroup, true, &channel);
	if (res != FMOD_OK || !channel) return EResult::Fail;

	channel->setMode(FMOD_2D | (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));
	channel->setVolume(clampedVol);
	channel->setPaused(false);

	m_CurrentBGMChannel = channel;
	m_CurrentBGMSound = sound;
	m_CurrentBGMVolume = clampedVol;
	return EResult::Success;
}

EResult AudioManager::CrossfadeBGM(FMOD::Sound* sound, f32 fadeDuration, f32 targetVolume, bool loop)
{
	if (!m_System || !sound) return EResult::InvalidArgument;

	if (!std::isfinite(fadeDuration) || fadeDuration <= 0.f)
	{
		return PlayBGM(sound, targetVolume, loop);
	}

	const f32 clampedTargetVol = std::isfinite(targetVolume) ? std::clamp(targetVolume, 0.f, 1.f) : 1.f;

	// 동일 트랙 중복 크로스페이드 방지
	if (m_IsCrossfading)
	{
		if (m_NextBGMSound == sound && IsChannelPlaying(m_NextBGMChannel))
		{
			m_CrossfadeTargetVolume = clampedTargetVol;
			return EResult::Success;
		}
		// 기존 크로스페이드 중이었다면 이전 outgoing 트랙을 정리하고 현재 incoming 트랙을 outgoing으로 승격
		if (m_CurrentBGMChannel)
		{
			m_CurrentBGMChannel->stop();
		}
		const f32 prevProgress = std::clamp(m_CrossfadeElapsed / m_CrossfadeDuration, 0.f, 1.f);
		m_CurrentBGMChannel = m_NextBGMChannel;
		m_CurrentBGMSound = m_NextBGMSound;
		m_CurrentBGMVolume = m_CrossfadeTargetVolume * prevProgress;
		m_NextBGMChannel = nullptr;
		m_NextBGMSound = nullptr;
		m_IsCrossfading = false;
	}
	else if (m_CurrentBGMSound == sound && IsChannelPlaying(m_CurrentBGMChannel))
	{
		m_CurrentBGMVolume = clampedTargetVol;
		m_CurrentBGMChannel->setVolume(clampedTargetVol);
		return EResult::Success;
	}

	FMOD::Channel* nextChannel = nullptr;
	FMOD_RESULT res = m_System->playSound(sound, m_BGMGroup, true, &nextChannel);
	if (res != FMOD_OK || !nextChannel) return EResult::Fail;

	nextChannel->setMode(FMOD_2D | (loop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF));
	nextChannel->setVolume(0.f);
	nextChannel->setPaused(false);

	m_NextBGMChannel = nextChannel;
	m_NextBGMSound = sound;
	m_IsCrossfading = true;
	m_CrossfadeDuration = fadeDuration;
	m_CrossfadeElapsed = 0.f;
	m_CrossfadeStartVolume = IsChannelPlaying(m_CurrentBGMChannel) ? m_CurrentBGMVolume : 0.f;
	m_CrossfadeTargetVolume = clampedTargetVol;

	return EResult::Success;
}

void AudioManager::StopBGM()
{
	if (m_CurrentBGMChannel)
	{
		m_CurrentBGMChannel->stop();
		m_CurrentBGMChannel = nullptr;
	}
	if (m_NextBGMChannel)
	{
		m_NextBGMChannel->stop();
		m_NextBGMChannel = nullptr;
	}
	m_CurrentBGMSound = nullptr;
	m_NextBGMSound = nullptr;
	m_CurrentBGMVolume = 1.f;
	m_IsCrossfading = false;
	m_CrossfadeDuration = 0.f;
	m_CrossfadeElapsed = 0.f;
	m_CrossfadeStartVolume = 1.f;
	m_CrossfadeTargetVolume = 1.f;
}

void AudioManager::UpdateCrossfade(f32 deltaTime)
{
	if (!m_IsCrossfading) return;
	if (!std::isfinite(deltaTime) || deltaTime <= 0.f) return;

	m_CrossfadeElapsed += deltaTime;
	const f32 t = (m_CrossfadeDuration > 0.f) ? std::clamp(m_CrossfadeElapsed / m_CrossfadeDuration, 0.f, 1.f) : 1.f;

	if (IsChannelPlaying(m_CurrentBGMChannel))
	{
		m_CurrentBGMChannel->setVolume(m_CrossfadeStartVolume * (1.f - t));
	}
	if (IsChannelPlaying(m_NextBGMChannel))
	{
		m_NextBGMChannel->setVolume(m_CrossfadeTargetVolume * t);
	}

	if (t >= 1.f)
	{
		if (m_CurrentBGMChannel)
		{
			m_CurrentBGMChannel->stop();
		}
		m_CurrentBGMChannel = m_NextBGMChannel;
		m_CurrentBGMSound = m_NextBGMSound;
		m_CurrentBGMVolume = m_CrossfadeTargetVolume;
		m_NextBGMChannel = nullptr;
		m_NextBGMSound = nullptr;
		m_IsCrossfading = false;
		m_CrossfadeDuration = 0.f;
		m_CrossfadeElapsed = 0.f;
	}
}

bool AudioManager::IsBGMPlaying() const
{
	return IsChannelPlaying(m_CurrentBGMChannel) || (m_IsCrossfading && IsChannelPlaying(m_NextBGMChannel));
}
