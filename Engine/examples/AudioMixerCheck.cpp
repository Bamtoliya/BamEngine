#include "AudioMixer.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <string_view>

namespace
{
	bool Expect(bool condition, std::string_view description)
	{
		if (!condition)
		{
			std::cerr << "[FAIL] " << description << '\n';
			return false;
		}
		std::cout << "[PASS] " << description << '\n';
		return true;
	}

	bool ApproxEqual(Engine::f32 a, Engine::f32 b, Engine::f32 epsilon = 0.001f)
	{
		return std::abs(a - b) <= epsilon;
	}
}

int main()
{
	using namespace Engine;
	std::cout << "=== AudioMixer Check Starting ===" << '\n';

	bool allPassed = true;

	// 1. 기본 상태 검증
	{
		AudioMixer mixer;
		bool defaultOk = true;
		for (uint8 i = 0; i < static_cast<uint8>(EAudioBus::Count); ++i)
		{
			const auto bus = static_cast<EAudioBus>(i);
			if (!ApproxEqual(mixer.GetBusVolume(bus), 1.0f) || mixer.IsBusMuted(bus) ||
				!ApproxEqual(mixer.GetEffectiveVolume(bus), 1.0f))
			{
				defaultOk = false;
				break;
			}
		}
		allPassed &= Expect(defaultOk, "Default state: all buses volume=1.0, unmuted, effective=1.0");
	}

	// 2. Master x SubBus 유효 볼륨 계산 & 볼륨 클램프 검증
	{
		AudioMixer mixer;
		mixer.SetBusVolume(EAudioBus::Master, 0.5f);
		mixer.SetBusVolume(EAudioBus::BGM, 0.8f);
		mixer.SetBusVolume(EAudioBus::SFX, 0.6f);
		mixer.SetBusVolume(EAudioBus::UI, 0.4f);
		mixer.SetBusVolume(EAudioBus::Voice, 0.2f);

		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::Master), 0.5f), "Master effective volume == 0.5");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::BGM), 0.4f), "BGM effective volume == 0.5 * 0.8 = 0.4");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::SFX), 0.3f), "SFX effective volume == 0.5 * 0.6 = 0.3");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::UI), 0.2f), "UI effective volume == 0.5 * 0.4 = 0.2");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::Voice), 0.1f), "Voice effective volume == 0.5 * 0.2 = 0.1");

		// 볼륨 클램프 [0, 1]
		mixer.SetBusVolume(EAudioBus::SFX, 2.5f);
		allPassed &= Expect(ApproxEqual(mixer.GetBusVolume(EAudioBus::SFX), 1.0f), "SetBusVolume upper clamp to 1.0");
		mixer.SetBusVolume(EAudioBus::SFX, -1.0f);
		allPassed &= Expect(ApproxEqual(mixer.GetBusVolume(EAudioBus::SFX), 0.0f), "SetBusVolume lower clamp to 0.0");
	}

	// 3. Mute 전파 검증
	{
		AudioMixer mixer;
		mixer.SetBusVolume(EAudioBus::Master, 0.8f);
		mixer.SetBusVolume(EAudioBus::SFX, 0.5f);
		mixer.SetBusVolume(EAudioBus::BGM, 0.5f);

		// 개별 버스 Mute
		mixer.SetBusMuted(EAudioBus::SFX, true);
		allPassed &= Expect(mixer.IsBusMuted(EAudioBus::SFX), "SFX bus muted is true");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::SFX), 0.0f), "SFX effective volume is 0.0 when SFX is muted");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::BGM), 0.4f), "BGM effective volume remains 0.4 while SFX is muted");

		mixer.SetBusMuted(EAudioBus::SFX, false);
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::SFX), 0.4f), "SFX effective volume restored to 0.4 after unmuting");

		// Master 버스 Mute 전파
		mixer.SetBusMuted(EAudioBus::Master, true);
		allPassed &= Expect(mixer.IsBusMuted(EAudioBus::Master), "Master bus muted is true");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::Master), 0.0f), "Master effective volume is 0.0 when Master muted");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::BGM), 0.0f), "BGM effective volume is 0.0 when Master muted");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::SFX), 0.0f), "SFX effective volume is 0.0 when Master muted");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::UI), 0.0f), "UI effective volume is 0.0 when Master muted");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::Voice), 0.0f), "Voice effective volume is 0.0 when Master muted");

		// Master Unmute 시 모든 버스 유효 볼륨 복구
		mixer.SetBusMuted(EAudioBus::Master, false);
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::Master), 0.8f), "Master effective volume restored after Master unmuted");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::BGM), 0.4f), "BGM effective volume restored after Master unmuted");
		allPassed &= Expect(ApproxEqual(mixer.GetEffectiveVolume(EAudioBus::SFX), 0.4f), "SFX effective volume restored after Master unmuted");
	}

	// 4. FadeBusVolume 다중 프레임 보간 검증
	{
		AudioMixer mixer;
		mixer.SetBusVolume(EAudioBus::Voice, 0.0f);
		mixer.FadeBusVolume(EAudioBus::Voice, 1.0f, 1.0f); // 1초 동안 0 -> 1

		allPassed &= Expect(mixer.GetBusState(EAudioBus::Voice).isFading, "Voice bus isFading == true right after FadeBusVolume");

		mixer.Update(0.25f);
		allPassed &= Expect(ApproxEqual(mixer.GetBusVolume(EAudioBus::Voice), 0.25f), "Voice fade progress at 0.25s is ~0.25");

		mixer.Update(0.25f);
		allPassed &= Expect(ApproxEqual(mixer.GetBusVolume(EAudioBus::Voice), 0.50f), "Voice fade progress at 0.50s is ~0.50");

		mixer.Update(0.25f);
		allPassed &= Expect(ApproxEqual(mixer.GetBusVolume(EAudioBus::Voice), 0.75f), "Voice fade progress at 0.75s is ~0.75");

		mixer.Update(0.25f);
		allPassed &= Expect(ApproxEqual(mixer.GetBusVolume(EAudioBus::Voice), 1.0f), "Voice fade completed at 1.0s to 1.0");
		allPassed &= Expect(!mixer.GetBusState(EAudioBus::Voice).isFading, "Voice isFading == false after completion");

		// 초과 프레임 업데이트 시에도 1.0 유지
		mixer.Update(0.5f);
		allPassed &= Expect(ApproxEqual(mixer.GetBusVolume(EAudioBus::Voice), 1.0f), "Voice volume stays 1.0 after further updates");

		// duration <= 0 즉시 페이드
		mixer.FadeBusVolume(EAudioBus::Voice, 0.35f, 0.0f);
		allPassed &= Expect(ApproxEqual(mixer.GetBusVolume(EAudioBus::Voice), 0.35f), "Instant fade with duration <= 0.0");
		allPassed &= Expect(!mixer.GetBusState(EAudioBus::Voice).isFading, "Instant fade leaves isFading == false");
	}

	// 5. 3D 거리 감쇠 계산 검증
	{
		constexpr f32 minD = 2.0f;
		constexpr f32 maxD = 10.0f;

		// minDistance 이내 -> 1.0
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(0.0f, minD, maxD, true), 1.0f), "3D linear atten at 0.0m <= minD is 1.0");
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(1.5f, minD, maxD, true), 1.0f), "3D linear atten at 1.5m <= minD is 1.0");
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(2.0f, minD, maxD, true), 1.0f), "3D linear atten at 2.0m == minD is 1.0");

		// maxDistance 이상 -> 0.0
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(10.0f, minD, maxD, true), 0.0f), "3D linear atten at 10.0m == maxD is 0.0");
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(15.0f, minD, maxD, true), 0.0f), "3D linear atten at 15.0m >= maxD is 0.0");

		// 중간 거리 선형 보간: distance = 6.0m (중간: (6-2)/(10-2) = 0.5 -> 0.5)
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(6.0f, minD, maxD, true), 0.5f), "3D linear atten at midpoint 6.0m is 0.5");
		// distance = 4.0m: (4-2)/8 = 0.25 -> 0.75
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(4.0f, minD, maxD, true), 0.75f), "3D linear atten at 4.0m is 0.75");

		// 비선형 롤오프 (linearRolloff = false)
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(1.0f, minD, maxD, false), 1.0f), "3D non-linear atten at 1.0m is 1.0");
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(12.0f, minD, maxD, false), 0.0f), "3D non-linear atten at 12.0m is 0.0");
		// distance = 6.0m: t = 0.5 -> (1 - 0.5)^2 = 0.25
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(6.0f, minD, maxD, false), 0.25f), "3D non-linear atten at midpoint is 0.25");

		// 경계 방어 (maxDistance <= minDistance)
		allPassed &= Expect(ApproxEqual(AudioMixer::Compute3DAttenuation(5.0f, 10.0f, 5.0f, true), 0.0f), "3D atten with invalid maxDistance returns 0.0");
	}

	// 6. 설정 JSON 저장/로드 라운드트립 검증
	{
		AudioMixer mixerA;
		mixerA.SetBusVolume(EAudioBus::Master, 0.9f);
		mixerA.SetBusMuted(EAudioBus::Master, false);

		mixerA.SetBusVolume(EAudioBus::BGM, 0.75f);
		mixerA.SetBusMuted(EAudioBus::BGM, false);

		mixerA.SetBusVolume(EAudioBus::SFX, 0.6f);
		mixerA.SetBusMuted(EAudioBus::SFX, true);

		mixerA.SetBusVolume(EAudioBus::UI, 0.45f);
		mixerA.SetBusMuted(EAudioBus::UI, false);

		mixerA.SetBusVolume(EAudioBus::Voice, 0.3f);
		mixerA.SetBusMuted(EAudioBus::Voice, true);

		const std::filesystem::path tempPath = std::filesystem::temp_directory_path() / "AudioMixerRoundTripTest.json";

		const bool saveOk = mixerA.SaveSettings(tempPath);
		allPassed &= Expect(saveOk, "AudioMixer::SaveSettings succeeds");

		AudioMixer mixerB;
		const bool loadOk = mixerB.LoadSettings(tempPath);
		allPassed &= Expect(loadOk, "AudioMixer::LoadSettings succeeds");

		bool roundTripMatch = true;
		for (uint8 i = 0; i < static_cast<uint8>(EAudioBus::Count); ++i)
		{
			const auto bus = static_cast<EAudioBus>(i);
			if (!ApproxEqual(mixerA.GetBusVolume(bus), mixerB.GetBusVolume(bus)))
			{
				roundTripMatch = false;
				std::cerr << "Volume mismatch on bus " << AudioMixer::GetBusName(bus) << '\n';
			}
			if (mixerA.IsBusMuted(bus) != mixerB.IsBusMuted(bus))
			{
				roundTripMatch = false;
				std::cerr << "Muted mismatch on bus " << AudioMixer::GetBusName(bus) << '\n';
			}
			if (!ApproxEqual(mixerA.GetEffectiveVolume(bus), mixerB.GetEffectiveVolume(bus)))
			{
				roundTripMatch = false;
				std::cerr << "Effective volume mismatch on bus " << AudioMixer::GetBusName(bus) << '\n';
			}
		}
		allPassed &= Expect(roundTripMatch, "Round trip preserves all bus volumes, muted states, and effective volumes");

		std::error_code ec;
		std::filesystem::remove(tempPath, ec);
	}

	// 7. 버스 이름 및 파싱 검증
	{
		bool namesOk = true;
		for (uint8 i = 0; i < static_cast<uint8>(EAudioBus::Count); ++i)
		{
			const auto bus = static_cast<EAudioBus>(i);
			const char* name = AudioMixer::GetBusName(bus);
			const auto parsed = AudioMixer::ParseBusName(name);
			if (!parsed.has_value() || *parsed != bus)
			{
				namesOk = false;
				break;
			}
		}
		allPassed &= Expect(namesOk, "GetBusName and ParseBusName bijection holds for all valid buses");
		allPassed &= Expect(!AudioMixer::ParseBusName("InvalidBus").has_value(), "ParseBusName returns nullopt for unknown bus");
	}

	std::cout << "=== AudioMixer Check Finished: " << (allPassed ? "ALL PASSED" : "FAILED") << " ===" << '\n';
	return allPassed ? 0 : 1;
}
