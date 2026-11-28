# AudioClip / AudioManager / Mixer

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 기본 흐름
AudioClip 리소스 → FMOD sound → AudioManager 재생. 일반 지속 음원/리스너는 AudioSourceComponent/AudioListenerComponent를 우선 사용한다. `AudioSystem::OnLateUpdate`는 엔진이 호출한다. FMOD SDK 런타임과 구성별 라이브러리가 준비되어야 한다.

```cpp
#include "AudioManager.h"
#include "AudioClip.h"
#include "ResourceHandle.h"
void PlaySfx(const Engine::ResourceHandle<Engine::AudioClip>& clip) {
    if (!clip || !clip.Get() || !clip->GetFMODSound()) return;
    (void)Engine::AudioManager::Get().PlayOneShot(clip->GetFMODSound());
}
```
핸들은 실제 재생 중에도 적절한 소유자에 보관해야 한다. 위 함수는 호출자의 리소스 수명 관리를 전제로 한다. EResult를 프로젝트에서 검사/로그 처리한다.

| API | 기본 파라미터 / 주의 |
|---|---|
| `PlayOneShot(sound, volume=1, bus=SFX, pitch=1)` | FMOD raw sound 필요 |
| `PlayOneShot3D(sound,position,volume=1,pitch=1,bus=SFX,minDistance=1,maxDistance=20)` | 공간 위치 및 감쇠 |
| `PlayBGM(sound,volume=1,loop=true)` | BGM |
| `CrossfadeBGM(sound,fadeDuration,targetVolume=1,loop=true)` / `StopBGM()` | 전환 |
| `SetBusVolume(bus,volume)` / `SetBusMuted(bus,muted)` / `FadeBusVolume(bus,target,duration)` | 믹서 제어 |
| `SetSoundConcurrency(sound,maxConcurrentVoices,minRetriggerInterval=0)` | 동시 발음 제어 |

## 사용성 문제
공개 재생 API가 `FMOD::Sound*`를 요구해 게임 코드가 백엔드 수명에 결합된다. AudioClip handle 기반 overload와 playback handle 필요성을 검토한다(A10). 씬 교체, PIE 반복, pause, 동일 사운드 연속 재생과 release 중 재생을 테스트한다. FMOD Studio event API 지원으로 확대 해석하지 않는다.

## 관련 소스
- [AudioClip.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Audio/Public/AudioClip.h) — `Engine/Source/Audio/Public/AudioClip.h`
- [AudioManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Audio/Public/AudioManager.h) — `Engine/Source/Audio/Public/AudioManager.h`
- [AudioSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Audio/Public/AudioSystem.h) — `Engine/Source/Audio/Public/AudioSystem.h`

## 검증 및 참고 예제
- [AudioMixerCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/AudioMixerCheck.cpp)
