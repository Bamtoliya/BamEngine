#include "AudioManager.h"
#include "Logger.h"
#include <fmod.hpp>
#include <fmod_errors.h>
#include <algorithm>

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

void AudioManager::Update(f32 dt)
{
	m_Mixer.Update(dt);
	SyncBusGroups();

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

EResult AudioManager::PlayOneShot(FMOD::Sound* sound, f32 volume, EAudioBus bus)
{
	if (!m_System || !sound) return EResult::InvalidArgument;

	FMOD::ChannelGroup* group = GetBusGroup(bus);
	FMOD::Channel* channel = nullptr;
	FMOD_RESULT res = m_System->playSound(sound, group, false, &channel);
	if (res != FMOD_OK) return EResult::Fail;

	if (channel) channel->setVolume(std::clamp(volume, 0.f, 1.f));
	return EResult::Success;
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
	if (m_MasterGroup)
	{
		m_MasterGroup->stop();
	}
}
