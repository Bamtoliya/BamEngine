# Navigation / AI

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 흐름
씬 단위 nav data를 준비한 뒤 agent를 구성한다. `NavigationSystem::SetSceneNavGrid(scene,shared_ptr<NavGrid>)` 또는 `SetSceneNavMesh(scene,shared_ptr<NavMesh>)`. `GetSceneNavGrid/Mesh`, `RemoveSceneNavGrid/Mesh`로 수명 관리한다. 두 데이터가 함께 존재할 때의 우선순위는 구현을 확인한다.

```cpp
#include "NavAgentComponent.h"
void Navigate(Engine::NavAgentComponent& agent) {
    agent.SetDestination({7.f, 0.f, 1.f});
}
```
`moveSpeed`, `stoppingDistance`, `agentRadius`, `autoRepath`, `hasDestination`, `pathPending`, `reachedDestination`, `velocity`, `HasActivePath()`, `Stop()`을 통해 제어 및 상태 확인. agent target만 설정한다고 nav data가 자동으로 만들어진다고 가정하지 않는다. `NavigationSystem::ProcessScene/Registry`는 시스템/테스트 경계로 일반 게임에서 중복 호출하지 않는다.

## 검증 및 위험
NavigationAICheck에는 10x10 NavGrid, 목적지 지정, 우회, NavMesh 및 파일 로딩 사례가 있다. 동적 장애물, 군중, 경사로, 다층, 경로 실패 및 씬 전환 후 nav cache 정리를 검증한다. Scene 포인터를 key로 삼는 인터페이스이므로 scene 파괴 시 제거 경로의 존재 및 자동 호출을 추가 확인한다(A11). AI perception은 타입 존재만 확인했으며 감지 계약은 후속 조사한다.

## 관련 소스
- [NavigationSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Navigation/Public/NavigationSystem.h) — `Engine/Source/Navigation/Public/NavigationSystem.h`
- [NavAgentComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Navigation/Public/NavAgentComponent.h) — `Engine/Source/Navigation/Public/NavAgentComponent.h`
- [NavGrid.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Navigation/Public/NavGrid.h) — `Engine/Source/Navigation/Public/NavGrid.h`
- [NavMesh.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Navigation/Public/NavMesh.h) — `Engine/Source/Navigation/Public/NavMesh.h`
- [AIPerceptionComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Navigation/Public/AIPerceptionComponent.h) — `Engine/Source/Navigation/Public/AIPerceptionComponent.h`

## 검증 및 참고 예제
- [NavigationAICheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/NavigationAICheck.cpp)
