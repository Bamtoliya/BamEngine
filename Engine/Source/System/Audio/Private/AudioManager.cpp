#include "AudioManager.h"
#include "Logger.h"
#include <fmod.hpp>
#include <fmod_errors.h>

IMPLEMENT_SINGLETON(AudioManager)

#define FMOD_CHECK(expr) \
	do { \
		FMOD_RESULT _res = (expr); \
		if (_res != FMOD_OK) { \
			BAM_LOG(Error, "AudioManager", "FMOD Error ({}) {}: {}", static_cast<int>(_res), #expr, FMOD_ErrorString(_res)); \
		} \
	} while (0)

EResult AudioManager::Initialize(void* arg)
{
	FMOD_RESULT result = FMOD::System_Create(&m_System);
	if (result != FMOD_OK || !m_System)
	{
		BAM_LOG(Error, "AudioManager", "Failed to create FMOD System: {}", FMOD_ErrorString(result));
		return EResult::Fail;
	}

	// 최대 가상 채널 512개, 기본 초기화 플래그
	result = m_System->init(512, FMOD_INIT_NORMAL, nullptr);
	if (result != FMOD_OK)
	{
		BAM_LOG(Error, "AudioManager", "Failed to initialize FMOD System: {}", FMOD_ErrorString(result));
		m_System->release();
		m_System = nullptr;
		return EResult::Fail;
	}

	// 채널 그룹(Master, BGM, SFX) 생성
	FMOD_CHECK(m_System->getMasterChannelGroup(&m_MasterGroup));
	FMOD_CHECK(m_System->createChannelGroup("BGM", &m_BGMGroup));
	FMOD_CHECK(m_System->createChannelGroup("SFX", &m_SFXGroup));

	if (m_MasterGroup && m_BGMGroup) FMOD_CHECK(m_MasterGroup->addGroup(m_BGMGroup));
	if (m_MasterGroup && m_SFXGroup) FMOD_CHECK(m_MasterGroup->addGroup(m_SFXGroup));

	BAM_LOG(Info, "AudioManager", "FMOD Audio System successfully initialized");
	return EResult::Success;
}

void AudioManager::Free()
{
	if (m_BGMGroup) { m_BGMGroup->release(); m_BGMGroup = nullptr; }
	if (m_SFXGroup) { m_SFXGroup->release(); m_SFXGroup = nullptr; }
	m_MasterGroup = nullptr;

	if (m_System)
	{
		m_System->close();
		m_System->release();
		m_System = nullptr;
		BAM_LOG(Info, "AudioManager", "FMOD Audio System released");
	}
}

void AudioManager::Update(f32 dt)
{
	if (m_System)
	{
		m_System->update();
	}
}

EResult AudioManager::PlayOneShot(FMOD::Sound* sound, f32 volume)
{
	if (!m_System || !sound) return EResult::InvalidArgument;

	FMOD::Channel* channel = nullptr;
	FMOD_RESULT res = m_System->playSound(sound, m_SFXGroup, false, &channel);
	if (res != FMOD_OK) return EResult::Fail;

	if (channel) channel->setVolume(std::clamp(volume, 0.f, 1.f));
	return EResult::Success;
}

void AudioManager::SetMasterVolume(f32 volume)
{
	m_MasterVolume = std::clamp(volume, 0.f, 1.f);
	if (m_MasterGroup) m_MasterGroup->setVolume(m_MasterVolume);
}

void AudioManager::SetBGMVolume(f32 volume)
{
	m_BGMVolume = std::clamp(volume, 0.f, 1.f);
	if (m_BGMGroup) m_BGMGroup->setVolume(m_BGMVolume);
}

void AudioManager::SetSFXVolume(f32 volume)
{
	m_SFXVolume = std::clamp(volume, 0.f, 1.f);
	if (m_SFXGroup) m_SFXGroup->setVolume(m_SFXVolume);
}

void AudioManager::StopAllSounds()
{
	if (m_MasterGroup)
	{
		m_MasterGroup->stop();
	}
}