# Reinforcement Learning / Simulation

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Experimental** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 실행 흐름
`SceneRLEnvironment(SceneRLEnvConfig={})` → `Initialize()` → `Reset(optional seed)` → `Step(RLAction)` → 종료 조건에 따라 reset → `Close()`. `GetObservationSpace`, `GetActionSpace`로 agent의 데이터 계약을 확인한다. RLObservation, RLStepResult의 실제 필드는 RLEnvironment 정의를 확인하여 Python 등과 연결한다.

```cpp
#include "SceneRLEnvironment.h"
void ResetEpisode(Engine::SceneRLEnvironment& env) {
    const auto observation = env.Reset(12345ull);
    (void)observation;
}
```
전제: 초기화 성공 및 소유자가 종료를 담당한다. 예제에서 같은 seed reset, 다른 seed reset, 목표 도달과 낙하를 검사한다. `SetTargetPosition`, `SetCurriculumLevel`, `GetEpisodeReward`, `GetBestEpisodeReward`, waypoint index를 조회/설정할 수 있다. snapshot API는 SaveGameManager를 명시적으로 넘기는 경로가 있다. manager pointer를 보관하는 SetSnapshotManager는 수명 확인 필요.

## 벡터화
예제에서는 `VectorizedRLEnvironment::CreateSceneNavigation(4,cfg,true)`, `CreateCartPole(4,true)`, `ResetAll(100ull)`을 사용한다. true의 세부 의미 및 동시 실행/스레드 안전성은 선언과 구현에서 재확인한다. 동일 seed의 전체 GPU/물리 결정성, 병렬 환경 격리, 외부 학습 라이브러리 연결을 완료 상태로 간주하지 않는다. 학습 throughput 및 reset 누수, reward 분포, 종료/절단 처리의 회귀 검증이 필요하다.

## 관련 소스
- [SceneRLEnvironment.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Simulation/Public/SceneRLEnvironment.h) — `Engine/Source/Simulation/Public/SceneRLEnvironment.h`
- [RLEnvironment.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Simulation/Public/RLEnvironment.h) — `Engine/Source/Simulation/Public/RLEnvironment.h`
- [VectorizedRLEnvironment.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Simulation/Public/VectorizedRLEnvironment.h) — `Engine/Source/Simulation/Public/VectorizedRLEnvironment.h`

## 검증 및 참고 예제
- [RLSimulationCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/RLSimulationCheck.cpp)
