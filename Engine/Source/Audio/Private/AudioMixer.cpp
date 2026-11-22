#include "AudioMixer.h"

#include <archive/JsonArchive.h>
#include <algorithm>
#include <cmath>

namespace Engine
{
	AudioMixer::AudioMixer()
	{
		for (auto& bus : m_Buses)
		{
			bus.volume = 1.0f;
			bus.muted = false;
			bus.fadeStartVolume = 1.0f;
			bus.fadeTargetVolume = 1.0f;
			bus.fadeDuration = 0.0f;
			bus.fadeElapsed = 0.0f;
			bus.isFading = false;
		}
	}

	void AudioMixer::SetBusVolume(EAudioBus bus, f32 volume)
	{
		const auto idx = static_cast<size_t>(bus);
		if (idx >= static_cast<size_t>(EAudioBus::Count)) return;

		m_Buses[idx].volume = std::clamp(volume, 0.0f, 1.0f);
		m_Buses[idx].isFading = false;
	}

	f32 AudioMixer::GetBusVolume(EAudioBus bus) const
	{
		const auto idx = static_cast<size_t>(bus);
		if (idx >= static_cast<size_t>(EAudioBus::Count)) return 0.0f;

		return m_Buses[idx].volume;
	}

	void AudioMixer::SetBusMuted(EAudioBus bus, bool muted)
	{
		const auto idx = static_cast<size_t>(bus);
		if (idx >= static_cast<size_t>(EAudioBus::Count)) return;

		m_Buses[idx].muted = muted;
	}

	bool AudioMixer::IsBusMuted(EAudioBus bus) const
	{
		const auto idx = static_cast<size_t>(bus);
		if (idx >= static_cast<size_t>(EAudioBus::Count)) return true;

		return m_Buses[idx].muted;
	}

	f32 AudioMixer::GetEffectiveVolume(EAudioBus bus) const
	{
		const auto idx = static_cast<size_t>(bus);
		if (idx >= static_cast<size_t>(EAudioBus::Count)) return 0.0f;

		const auto& master = m_Buses[static_cast<size_t>(EAudioBus::Master)];
		if (master.muted) return 0.0f;

		const auto& target = m_Buses[idx];
		if (target.muted) return 0.0f;

		if (bus == EAudioBus::Master)
		{
			return master.volume;
		}

		return master.volume * target.volume;
	}

	void AudioMixer::FadeBusVolume(EAudioBus bus, f32 targetVolume, f32 durationSeconds)
	{
		const auto idx = static_cast<size_t>(bus);
		if (idx >= static_cast<size_t>(EAudioBus::Count)) return;

		const f32 clamped = std::clamp(targetVolume, 0.0f, 1.0f);
		if (durationSeconds <= 0.0f)
		{
			m_Buses[idx].volume = clamped;
			m_Buses[idx].fadeStartVolume = clamped;
			m_Buses[idx].fadeTargetVolume = clamped;
			m_Buses[idx].fadeDuration = 0.0f;
			m_Buses[idx].fadeElapsed = 0.0f;
			m_Buses[idx].isFading = false;
			return;
		}

		m_Buses[idx].fadeStartVolume = m_Buses[idx].volume;
		m_Buses[idx].fadeTargetVolume = clamped;
		m_Buses[idx].fadeDuration = durationSeconds;
		m_Buses[idx].fadeElapsed = 0.0f;
		m_Buses[idx].isFading = true;
	}

	void AudioMixer::Update(f32 dt)
	{
		if (dt <= 0.0f) return;

		for (auto& bus : m_Buses)
		{
			if (!bus.isFading) continue;

			bus.fadeElapsed += dt;
			const f32 progress = (bus.fadeDuration > 0.0f) ? (bus.fadeElapsed / bus.fadeDuration) : 1.0f;
			if (progress >= 1.0f)
			{
				bus.volume = bus.fadeTargetVolume;
				bus.isFading = false;
			}
			else
			{
				bus.volume = std::lerp(bus.fadeStartVolume, bus.fadeTargetVolume, progress);
			}
		}
	}

	f32 AudioMixer::Compute3DAttenuation(f32 distance, f32 minDistance, f32 maxDistance, bool linearRolloff)
	{
		if (distance <= minDistance) return 1.0f;
		if (distance >= maxDistance || maxDistance <= minDistance) return 0.0f;

		const f32 range = maxDistance - minDistance;
		const f32 t = (distance - minDistance) / range;
		if (linearRolloff)
		{
			return std::clamp(1.0f - t, 0.0f, 1.0f);
		}

		const f32 factor = 1.0f - t;
		return std::clamp(factor * factor, 0.0f, 1.0f);
	}

	bool AudioMixer::SaveSettings(const std::filesystem::path& path) const
	{
		archive::JsonArchive writer;
		for (size_t i = 0; i < static_cast<size_t>(EAudioBus::Count); ++i)
		{
			const auto bus = static_cast<EAudioBus>(i);
			const char* busName = GetBusName(bus);
			const auto& state = m_Buses[i];
			const auto res = writer.WriteObject(busName, state, [](archive::IArchive& ar, const AudioBusState& s) {
				ar.Write("Volume", s.volume);
				ar.Write("Muted", s.muted);
				return archive::ArchiveResult{};
			});
			if (!res) return false;
		}

		const auto saveRes = writer.SaveToFile(path);
		return static_cast<bool>(saveRes);
	}

	bool AudioMixer::LoadSettings(const std::filesystem::path& path)
	{
		archive::JsonArchive reader;
		const auto loadRes = reader.LoadFromFile(path);
		if (!loadRes) return false;

		for (size_t i = 0; i < static_cast<size_t>(EAudioBus::Count); ++i)
		{
			const auto bus = static_cast<EAudioBus>(i);
			const char* busName = GetBusName(bus);
			auto& state = m_Buses[i];
			reader.ReadObject(busName, state, [](archive::IArchive& ar, AudioBusState& s) {
				ar.Read("Volume", s.volume);
				ar.Read("Muted", s.muted);
				s.volume = std::clamp(s.volume, 0.0f, 1.0f);
				s.isFading = false;
				return archive::ArchiveResult{};
			});
		}

		return true;
	}

	const AudioBusState& AudioMixer::GetBusState(EAudioBus bus) const
	{
		static const AudioBusState s_DefaultState{};
		const auto idx = static_cast<size_t>(bus);
		if (idx >= static_cast<size_t>(EAudioBus::Count)) return s_DefaultState;

		return m_Buses[idx];
	}
}
