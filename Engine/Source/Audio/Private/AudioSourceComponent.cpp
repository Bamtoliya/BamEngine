#include "AudioSourceComponent.h"
#include <fmod.hpp>

namespace Engine
{
	AudioSourceComponent::~AudioSourceComponent()
	{
		StopImmediate();
	}

	AudioSourceComponent::AudioSourceComponent(const AudioSourceComponent& other)
		: clip(other.clip)
		, volume(other.volume)
		, pitch(other.pitch)
		, loop(other.loop)
		, playOnAwake(other.playOnAwake)
		, is3D(other.is3D)
		, minDistance(other.minDistance)
		, maxDistance(other.maxDistance)
		, m_Channel(nullptr)
		, m_IsPlaying(false)
		, m_PlayRequested(other.m_PlayRequested)
		, m_StopRequested(other.m_StopRequested)
		, m_PauseRequested(other.m_PauseRequested)
		, m_ResumeRequested(other.m_ResumeRequested)
		, m_AwakeTriggered(other.m_AwakeTriggered)
	{
	}

	AudioSourceComponent& AudioSourceComponent::operator=(const AudioSourceComponent& other)
	{
		if (this != &other)
		{
			StopImmediate();
			clip = other.clip;
			volume = other.volume;
			pitch = other.pitch;
			loop = other.loop;
			playOnAwake = other.playOnAwake;
			is3D = other.is3D;
			minDistance = other.minDistance;
			maxDistance = other.maxDistance;
			m_Channel = nullptr;
			m_IsPlaying = false;
			m_PlayRequested = other.m_PlayRequested;
			m_StopRequested = other.m_StopRequested;
			m_PauseRequested = other.m_PauseRequested;
			m_ResumeRequested = other.m_ResumeRequested;
			m_AwakeTriggered = other.m_AwakeTriggered;
		}
		return *this;
	}

	AudioSourceComponent::AudioSourceComponent(AudioSourceComponent&& other) noexcept
		: clip(std::move(other.clip))
		, volume(other.volume)
		, pitch(other.pitch)
		, loop(other.loop)
		, playOnAwake(other.playOnAwake)
		, is3D(other.is3D)
		, minDistance(other.minDistance)
		, maxDistance(other.maxDistance)
		, m_Channel(other.m_Channel)
		, m_IsPlaying(other.m_IsPlaying)
		, m_PlayRequested(other.m_PlayRequested)
		, m_StopRequested(other.m_StopRequested)
		, m_PauseRequested(other.m_PauseRequested)
		, m_ResumeRequested(other.m_ResumeRequested)
		, m_AwakeTriggered(other.m_AwakeTriggered)
	{
		other.m_Channel = nullptr;
		other.m_IsPlaying = false;
	}

	AudioSourceComponent& AudioSourceComponent::operator=(AudioSourceComponent&& other) noexcept
	{
		if (this != &other)
		{
			StopImmediate();
			clip = std::move(other.clip);
			volume = other.volume;
			pitch = other.pitch;
			loop = other.loop;
			playOnAwake = other.playOnAwake;
			is3D = other.is3D;
			minDistance = other.minDistance;
			maxDistance = other.maxDistance;
			m_Channel = other.m_Channel;
			m_IsPlaying = other.m_IsPlaying;
			m_PlayRequested = other.m_PlayRequested;
			m_StopRequested = other.m_StopRequested;
			m_PauseRequested = other.m_PauseRequested;
			m_ResumeRequested = other.m_ResumeRequested;
			m_AwakeTriggered = other.m_AwakeTriggered;

			other.m_Channel = nullptr;
			other.m_IsPlaying = false;
		}
		return *this;
	}

	void AudioSourceComponent::StopImmediate()
	{
		if (m_Channel)
		{
			m_Channel->stop();
			m_Channel = nullptr;
		}
		m_IsPlaying = false;
		m_PlayRequested = false;
		m_StopRequested = false;
	}
}
