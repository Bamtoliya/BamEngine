#pragma once

#include "Base.h"
#include "Macro.h"
#include "Result.h"

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
	EResult PlayOneShot(FMOD::Sound* sound, f32 volume = 1.f);

	// 볼륨 제어
	void SetMasterVolume(f32 volume);
	void SetBGMVolume(f32 volume);
	void SetSFXVolume(f32 volume);

	f32 GetMasterVolume() const { return m_MasterVolume; }
	f32 GetBGMVolume() const { return m_BGMVolume; }
	f32 GetSFXVolume() const { return m_SFXVolume; }

	// FMOD System 포인터 (내부 및 AudioSystem/AudioClip 연동용)
	FMOD::System* GetFMODSystem() const { return m_System; }
	FMOD::ChannelGroup* GetBGMGroup() const { return m_BGMGroup; }
	FMOD::ChannelGroup* GetSFXGroup() const { return m_SFXGroup; }

	void StopAllSounds();
private:
	FMOD::System* m_System = { nullptr };
	FMOD::ChannelGroup* m_MasterGroup = { nullptr };
	FMOD::ChannelGroup* m_BGMGroup = { nullptr };
	FMOD::ChannelGroup* m_SFXGroup = { nullptr };

	f32 m_MasterVolume = { 1.f };
	f32 m_BGMVolume = { 1.f };
	f32 m_SFXVolume = { 1.f };
};

END