# Runtime / System / Time

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Internal / Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 호출 책임
게임 프로젝트는 이미 초기화된 Runtime의 시스템을 사용한다. Editor/Client 실행 루프가 `FixedUpdate(dt)`, `Update(dt, updateInput, phaseMask)`, `LateUpdate(dt)`, `Render(dt)`를 호출한다. 게임 코드에서 중복으로 실행하지 않는다.

구현부에서 `FixedUpdate`는 시스템 실행 뒤 `ConsumeFixedStepActions()`를 호출한다. `Update`는 선택적으로 입력을 갱신하고 오디오 및 CombatEffectService를 처리한다. hitstop은 이 경로의 시스템 dt에 적용되며 fixed/late dt까지 동일하게 스케일된다고 가정하면 안 된다. `LateUpdate`는 hierarchy flags를 갱신한다. `Render`는 BeginFrame → streamer → `Sumbit`(실제 철자) → Render → EndFrame 순서로 진행한다.

## 공개 함수
`TimeManager::GetDeltaTime()`, `GetTotalTime()`, `GetFixedStep()`, `GetFixedAccumulator()`, `GetFPS()`, `SetTargetFPS(uint32)`, `SetFixedStep(f32) -> bool`, `IsFixedUpdateReady()`.
`SystemManager::AddSystem<T>(...)`, `GetSystem<T>()`, `RemoveSystem<T>()`는 모듈/시스템 등록 지점에서 사용한다. 등록 순서, phaseMask, 시뮬레이션·렌더 제출 분리는 실제 등록부를 추가 검증해야 한다.

```cpp
#include "TimeManager.h"
float FrameSeconds() { return Engine::TimeManager::Get().GetDeltaTime(); }
```

## 문제점
`Timer`는 public 생성 팩토리가 보이지 않고 생성자가 private이다. 또한 `SetTargetFPS(0)`에서 0으로 나누고, `Update`는 한 호출당 최대 한 스텝만 소비한다. 범용 타이머로 문서화하기보다 계약을 먼저 개선할 필요가 있다(A03). TimeManager의 fixed step 소비는 실행 루프 담당이며 게임 코드에서 readiness를 임의 소비하지 않는다.

## 관련 소스
- [Runtime.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Runtime/Public/Runtime.h) — `Engine/Source/Runtime/Public/Runtime.h`
- [SystemManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/System/Public/SystemManager.h) — `Engine/Source/World/System/Public/SystemManager.h`
- [TimeManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Time/Public/TimeManager.h) — `Engine/Source/Time/Public/TimeManager.h`
- [Timer.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Time/Public/Timer.h) — `Engine/Source/Time/Public/Timer.h`
- [Runtime.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Runtime/Private/Runtime.cpp) — `Engine/Source/Runtime/Private/Runtime.cpp`

## 검증 및 참고 예제
- 이번 조사에서 이 API에 직접 대응하는 테스트를 확인하지 못함.
