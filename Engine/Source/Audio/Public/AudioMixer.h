#pragma once

#include "Engine_API.h"
#include "Macro.h"
#include "Types.h"
#include "ComponentAnnotations.h"
#include <array>
#include <filesystem>
#include <string_view>
#include <optional>

namespace Engine
{
	ENUM()
	enum class EAudioBus : uint8
	{
		Master = 0,
		BGM,
		SFX,
		UI,
		Voice,
		Count
	};

	struct AudioBusState
	{
		f32 volume = 1.0f;
		bool muted = false;
		f32 fadeStartVolume = 1.0f;
		f32 fadeTargetVolume = 1.0f;
		f32 fadeDuration = 0.0f;
		f32 fadeElapsed = 0.0f;
		bool isFading = false;
	};

	class ENGINE_API AudioMixer
	{
	public:
		AudioMixer();
		~AudioMixer() = default;

		void SetBusVolume(EAudioBus bus, f32 volume);
		[[nodiscard]] f32 GetBusVolume(EAudioBus bus) const;

		void SetBusMuted(EAudioBus bus, bool muted);
		[[nodiscard]] bool IsBusMuted(EAudioBus bus) const;

		[[nodiscard]] f32 GetEffectiveVolume(EAudioBus bus) const;

		void FadeBusVolume(EAudioBus bus, f32 targetVolume, f32 durationSeconds);
		void Update(f32 dt);

		[[nodiscard]] static f32 Compute3DAttenuation(f32 distance, f32 minDistance, f32 maxDistance, bool linearRolloff = true);

		[[nodiscard]] bool SaveSettings(const std::filesystem::path& path) const;
		[[nodiscard]] bool LoadSettings(const std::filesystem::path& path);

		[[nodiscard]] static constexpr const char* GetBusName(EAudioBus bus) noexcept
		{
			switch (bus)
			{
			case EAudioBus::Master: return "Master";
			case EAudioBus::BGM:    return "BGM";
			case EAudioBus::SFX:    return "SFX";
			case EAudioBus::UI:     return "UI";
			case EAudioBus::Voice:  return "Voice";
			default:                return "Unknown";
			}
		}

		[[nodiscard]] static std::optional<EAudioBus> ParseBusName(std::string_view name) noexcept
		{
			if (name == "Master") return EAudioBus::Master;
			if (name == "BGM")    return EAudioBus::BGM;
			if (name == "SFX")    return EAudioBus::SFX;
			if (name == "UI")     return EAudioBus::UI;
			if (name == "Voice")  return EAudioBus::Voice;
			return std::nullopt;
		}

		[[nodiscard]] const AudioBusState& GetBusState(EAudioBus bus) const;

	private:
		std::array<AudioBusState, static_cast<size_t>(EAudioBus::Count)> m_Buses{};
	};
}
