#include "CompositeEffect.h"
#include <archive/JsonArchive.h>
#include <algorithm>

namespace Engine
{
	EResult CompositeEffect::Initialize(void* arg)
	{
		if (arg)
		{
			auto* desc = static_cast<DESC*>(arg);
			m_TotalDuration = (desc->totalDuration > 0.05f) ? desc->totalDuration : 2.0f;
			m_IsLooping = desc->isLooping;
			if (GetTotalTrackCount() == 0 && !desc->Path.empty())
			{
				LoadFromFile(desc->Path);
			}
		}
		return EResult::Success;
	}

	CompositeEffect* CompositeEffect::Create(void* arg)
	{
		auto* instance = new CompositeEffect();
		if (instance->Initialize(arg) != EResult::Success)
		{
			delete instance;
			return nullptr;
		}
		return instance;
	}

	void CompositeEffect::Free()
	{
		ClearAllTracks();
	}

	uint32 CompositeEffect::GetTotalTrackCount() const
	{
		return static_cast<uint32>(
			m_SpriteTracks.size() +
			m_ParticleTracks.size() +
			m_LightTracks.size() +
			m_MeshTracks.size() +
			m_SoundTracks.size() +
			m_TrailTracks.size() +
			m_DecalTracks.size() +
			m_CameraShakeTracks.size());
	}

	void CompositeEffect::ClearAllTracks()
	{
		m_SpriteTracks.clear();
		m_ParticleTracks.clear();
		m_LightTracks.clear();
		m_MeshTracks.clear();
		m_SoundTracks.clear();
		m_TrailTracks.clear();
		m_DecalTracks.clear();
		m_CameraShakeTracks.clear();
	}

	void CompositeEffect::RecalculateDurationFromTracks()
	{
		f32 maxTime = 0.0f;

		for (const auto& track : m_SpriteTracks)
		{
			maxTime = std::max(maxTime, track.startTime + track.duration);
		}

		for (const auto& track : m_ParticleTracks)
		{
			maxTime = std::max(maxTime, track.startTime + track.duration);
		}

		for (const auto& track : m_LightTracks)
		{
			maxTime = std::max(maxTime, track.startTime + track.duration);
		}

		for (const auto& track : m_MeshTracks)
		{
			maxTime = std::max(maxTime, track.startTime + track.duration);
		}

		for (const auto& track : m_SoundTracks)
		{
			maxTime = std::max(maxTime, track.startTime + 0.5f);
		}

		for (const auto& track : m_TrailTracks)
		{
			maxTime = std::max(maxTime, track.startTime + track.duration);
		}

		for (const auto& track : m_DecalTracks)
		{
			maxTime = std::max(maxTime, track.startTime + track.duration);
		}

		for (const auto& track : m_CameraShakeTracks)
		{
			maxTime = std::max(maxTime, track.startTime + track.duration);
		}

		if (maxTime > 0.05f)
		{
			m_TotalDuration = maxTime;
		}
	}

	bool CompositeEffect::SaveToFile(const std::wstring& filePath) const
	{
		archive::JsonArchive ar;
		if (!WriteArchive(ar)) return false;
		return static_cast<bool>(ar.SaveToFile(filePath));
	}

	bool CompositeEffect::LoadFromFile(const std::wstring& filePath)
	{
		archive::JsonArchive ar;
		if (!ar.LoadFromFile(filePath)) return false;
		return static_cast<bool>(ReadArchive(ar));
	}
}
