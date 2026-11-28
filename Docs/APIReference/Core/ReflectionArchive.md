# Reflection / Serialization

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 개념
Reflection은 타입/멤버 정보를, Archive는 영속 데이터 읽기·쓰기를 담당한다. `STRUCT(COMPONENT, ...)`, `REFLECT_BODY()`, `PROPERTY(EDITABLE, ...)`가 엔진 컴포넌트 헤더의 선언 패턴이다. 그래프 노드 노출은 별도 `GRAPH_PURE`/`GRAPH_CALLABLE` 메타데이터다.

## 등록 및 조회
- `ReflectionRegistry::GetComponentEntries() -> std::span<const ComponentReflectionEntry>`: 등록 목록; 등록 변경 이후 span을 저장하지 않는다.
- `FindComponent(std::type_index / entt::id_type / std::string_view)` 및 `GetComponentObject(world, storageId, entity)`로 조회한다.
- `ForEachComponent(world, entity, visitor)` / `Scene::ForEachReflectedComponent(entity, visitor)`는 방문 결과를 처리한다.
- `WriteComponent(ar, key, world, entity, type, codecs)` / `ReadComponent(...)` 반환 `archive::ArchiveResult`를 반드시 확인한다.
- `RegisterComponent<T>(...)` / `UnregisterComponent<T>()`, `REGISTER_MODULE_COMPONENTS(ModuleName)` / `UNREGISTER_MODULE_COMPONENTS(ModuleName)`는 모듈 등록 경계를 확인하여 사용한다.

```cpp
#include "ReflectionRegistry.h"
void InspectTypes(const Engine::ReflectionRegistry& registry) {
    for (const auto& entry : registry.GetComponentEntries()) {
        (void)entry.CanDisable();
    }
}
```

## 작성 시 계약
편집 가능 여부와 저장 여부는 독립적이다. `NOSERIALIZE`, `ALLOW_MISSING`, `VISIBLE(false)` 등은 기존 사용 예에 맞춰 선택한다. 포인터, 콜백, GPU 객체 등 실행 전용 상태는 영속 데이터에서 분리한다. 타입/필드 이름을 변경할 때 기존 씬 호환 정책과 누락 필드 기본값을 정한다. 모듈 DLL을 제거하기 전에 해당 타입의 인스턴스, object view 및 호출 가능 객체를 해제한다.

## 검증
등록 → Inspector 노출 → Archive 왕복 → 씬 재열기 → PIE 반복 → 모듈 언로드 순으로 검증한다. 직접 엔진 registry의 mutable 접근은 내부 성격이며 등록 수명 관리 없이 호출하지 않는다. [ReflectionArchiveGuidelines](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/ReflectionArchiveGuidelines.md) 참조.

## 관련 소스
- [ReflectionRegistry.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Reflection/Public/ReflectionRegistry.h) — `Engine/Source/Core/Reflection/Public/ReflectionRegistry.h`
- [ComponentAnnotations.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Reflection/Public/ComponentAnnotations.h) — `Engine/Source/Core/Reflection/Public/ComponentAnnotations.h`
- [Scene.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Scene/Public/Scene.h) — `Engine/Source/World/Scene/Public/Scene.h`
- [GraphAnnotations.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Public/GraphAnnotations.h) — `Engine/Source/Graph/Public/GraphAnnotations.h`

## 검증 및 참고 예제
- [SceneReflectionCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/SceneReflectionCheck.cpp)
- [SceneArchiveCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/SceneArchiveCheck.cpp)
