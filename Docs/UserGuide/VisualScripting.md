# 비주얼 스크립팅

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 그래프 만들기

1. `Window → Panels → Graph` 또는 Content Browser의 Graph 생성 경로를 사용한다.
2. Logic 도메인을 선택하고 노드를 배치한다.
3. 데이터 핀은 호환되는 자료형끼리, 실행 핀은 실행 흐름끼리 연결한다.
4. Graph를 저장하고 테스트 실행의 결과/진단을 확인한다.
5. 실제 씬에 연결하려면 엔티티에 `GraphBindingComponent`를 추가하고 `graphFile`을 지정하며 `enabled`를 확인한다.
6. PIE에서 Start/Update 등의 이벤트 실행을 검증한다.

기존 GraphPanel/GraphExecution 문서의 계산 실습:

```text
Core.Float(value=2).result → Math.AddFloat.a
Math.AddFloat.b = 3
Math.AddFloat.result → Logic.FloatResult.value
Start.exec → Logic.FloatResult.exec
```

예상 결과는 5다. 노드 이름과 핀 연결은 실제 패널의 레지스트리 표시를 우선한다. Branch, Sequence, Loop, 변수, 리플렉션 기반 노드 등은 기존 문서에 설명되어 있다. 무한 루프나 잘못된 자료형 연결에 대한 진단을 확인한다. Shader Graph와 Logic Graph는 같은 실행 의미를 갖지 않는다.

## 그래프와 C++의 역할 분리

| 그래프 우선 | C++ 우선 |
|---|---|
| 문 열기, 트리거, 간단한 입력과 상호작용 | 복잡한 물리 제어, 절차적 생성, 대량 계산 |
| UI 흐름과 소규모 이벤트 조합 | 게임 규칙, 데이터 관리, 재사용 기능 |
| 간단한 수치 조합 및 콘텐츠 실험 | 커스텀 ECS 시스템과 새로운 그래프 노드 |

게임별 상태를 엔진의 전역 싱글턴에 무조건 저장하지 않는다. PIE 종료, 씬 전환, 프로젝트 전환에서 정리되어야 하는 데이터를 명확히 나눈다.

## 관련 문서 및 소스

- [GraphPanel.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/GraphPanel.md), [GraphExecution.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/GraphExecution.md)
- [`GraphBindingComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Public/GraphBindingComponent.h) — `Engine/Source/Graph/Public/GraphBindingComponent.h`
- [전체 근거 및 검토 범위](Sources.md)

