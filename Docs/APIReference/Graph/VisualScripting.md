# Graph / Visual Scripting

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 씬에서 실행
Logic 그래프 저장 → 엔티티 `GraphBindingComponent.graphFile` 지정 → `enabled=true` → Runtime의 GraphSystem 실행. Start 및 Update 이벤트 등은 GraphExecutor 문서와 엔진 노드 등록 상태를 참조한다. Shader domain과 Logic domain을 혼용하지 않는다.

## 직접 실행기
`GraphRegistry::Register(definition)` / `RegisterOrUpdate`, `RegisterBuiltins`, `Find`, `FindPin`, `GetTypes(domain)`을 사용한다. `GraphExecutor::Load(graph,registry,...)` 결과 GraphDiagnostic 확인 → `Begin(entry, delta=0, budget=10000)` → `Resume()` → `Tick(maxTasks=64)` 반복 → `GetState`, `GetDiagnostic`, `GetTrace`, `GetResult` 확인. `Pause`, `Step`, `Stop`, `Clear`로 제어한다. 예제는 Load의 전체 선언 및 실제 GraphDiagnostic 계약을 확인해 구성한다.

```cpp
#include "GraphExecutor.h"
void TickBudgeted(Engine::GraphExecutor& executor) {
    if (executor.GetState() == Engine::EGraphExecutionState::Running)
        executor.Tick(64);
}
```
`Step`은 단일 UI 노드가 아니라 실행 작업 하나일 수 있다. 데이터 순환·실행 예산·지원하지 않는 타입 및 네이티브 예외를 진단으로 다룬다. 사용자 함수에 `GRAPH_PURE(id)` 또는 `GRAPH_CALLABLE(id)` 메타데이터를 적용할 수 있지만 자동 노출을 보장하지 않는다. 인자/반환 타입 및 codegen 등록 확인.

## 수명
GraphSystem::GetCurrentScene/GetCurrentEntity/GetCurrentDeltaTime은 그래프 실행 컨텍스트용이다. 호출 밖에서 global context를 저장/변조하지 않는다. `RequestCurrentEntityDestroy()`는 실행 중 삭제 요청 경계다. 모듈 unload 전에 graph executor와 reflected callable을 폐기한다. [GraphExecution](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/GraphExecution.md), [GraphReflection](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/GraphReflection.md) 참조.

## 관련 소스
- [GraphExecutor.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Public/GraphExecutor.h) — `Engine/Source/Graph/Public/GraphExecutor.h`
- [GraphRegistry.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Public/GraphRegistry.h) — `Engine/Source/Graph/Public/GraphRegistry.h`
- [GraphSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Public/GraphSystem.h) — `Engine/Source/Graph/Public/GraphSystem.h`
- [GraphBindingComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Public/GraphBindingComponent.h) — `Engine/Source/Graph/Public/GraphBindingComponent.h`
- [GraphAnnotations.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Public/GraphAnnotations.h) — `Engine/Source/Graph/Public/GraphAnnotations.h`

## 검증 및 참고 예제
- [GraphExecutionCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/GraphExecutionCheck.cpp)
- [GraphFoundationCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/GraphFoundationCheck.cpp)
- [GraphReflectionCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/GraphReflectionCheck.cpp)
- [ShaderGraphCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/ShaderGraphCheck.cpp)
