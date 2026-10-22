#include "AudioClip.h"
#include "AudioManager.h"
#include "Functions.h"
#include "Logger.h"
#include <fmod.hpp>
#include <fmod_errors.h>

AudioClip* AudioClip::Create(void* arg)
{
	auto* instance = new AudioClip();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

EResult AudioClip::Initialize(void* arg)
{
	if (Resource::Initialize(arg) != EResult::Success) return EResult::Fail;

	FMOD::System* fmodSystem = AudioManager::Get().GetFMODSystem();
	if (!fmodSystem)
	{
		BAM_LOG(Error, "Engine","AudioClip", "AudioManager's FMOD System is null");
		return EResult::Fail;
	}

	const auto* desc = static_cast<const AudioClipCreateDesc*>(arg);
	if (desc)
	{
		m_Is3D = desc->is3D;
		m_IsStream = desc->isStream;
		m_IsLoop = desc->loop;
	}

	FMOD_MODE mode = FMOD_DEFAULT;
	mode |= (m_Is3D ? (FMOD_3D | FMOD_3D_LINEARROLLOFF) : FMOD_2D);
	mode |= (m_IsLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);
	mode |= (m_IsStream ? FMOD_CREATESTREAM : FMOD_CREATESAMPLE);

	string narrowPath = WStrToStr(m_Path);
	FMOD_RESULT result = FMOD_OK;

	if (m_IsStream)
	{
		result = fmodSystem->createStream(narrowPath.c_str(), mode, nullptr, &m_Sound);
	}
	else
	{
		result = fmodSystem->createSound(narrowPath.c_str(), mode, nullptr, &m_Sound);
	}

	if (result != FMOD_OK || !m_Sound)
	{
		BAM_LOG(Error, "Engine","AudioClip", "Failed to load sound: {} ({})", narrowPath, FMOD_ErrorString(result));
		return EResult::Fail;
	}

	m_Sound->getLength(&m_LengthMs, FMOD_TIMEUNIT_MS);
	m_DurationSeconds = static_cast<f32>(m_LengthMs) / 1000.f;

	return EResult::Success;
}

void AudioClip::Free()
{
	if (m_Sound)
	{
		m_Sound->release();
		m_Sound = nullptr;
	}
	m_LengthMs = 0;
	m_DurationSeconds = 0.f;
}

archive::ArchiveResult AudioClip::WriteArchive(archive::IArchive& ar) const
{
	return Resource::WriteArchive(ar);
}

archive::ArchiveResult AudioClip::ReadArchive(const archive::IArchive& ar)
{
	return Resource::ReadArchive(ar);
}