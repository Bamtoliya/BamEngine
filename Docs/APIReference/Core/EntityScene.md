# Entity / Scene / Hierarchy

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 사용 모델
`Scene`이 EnTT registry와 Entity를 관리한다. `Entity`는 복사 불가 래퍼이며 `entt::entity`는 해당 registry 안에서만 의미가 있다. 씬 저장용 영속 ID와 실행 핸들을 혼용하지 않는다. 씬 전환·삭제·PIE 종료 후 포인터/참조를 재사용하지 않는다.

## 주요 API
| API | 계약 및 사용 조건 |
|---|---|
| `Scene::CreateEntity() -> Entity&` | 활성 프로젝트의 유효한 씬에서 생성 |
| `Entity::AddComponent<T>(Args&&...)` | 타입이 없는 상태에서 추가; 중복 방지 |
| `Scene::HasComponent<T>(Entity&)`, `GetComponent<T>(Entity&)` | 조회 전 존재 및 소속 확인 |
| `Scene::DestroyEntity(Entity&) -> bool` | 삭제 실패 가능; 결과 확인, 삭제 후 참조 폐기 |
| `Scene::ReparentEntity(Entity&, Entity* = nullptr)` | null은 루트; Cycle/InvalidTransform/DepthLimit 등 결과 분기 |
| `Scene::FindEntityById(uint64)` | 현재 씬에서 다시 해결 |
| `Entity::GetWeakRef()`, `Entity::Resolve(ref)` | 비소유 참조; null 처리 필수 |
| `IsComponentEnabled<T>(world, entity)` | 활성 여부 확인; 엔티티/계층 활성도 별도 고려 |

```cpp
#include "Scene.h"
#include "Entity.h"
#include "TransformComponent.h"
void Spawn(Engine::Scene& scene) {
    auto& entity = scene.CreateEntity();
    if (scene.HasComponent<Engine::TransformComponent>(entity)) {
        auto& transform = scene.GetComponent<Engine::TransformComponent>(entity);
        (void)transform.SetLocalPosition({0.f, 2.f, 0.f});
    }
}
```

## 씬 전환 및 프리팹
`SceneManager::GetCurrentScene()`의 null을 처리한다. `ChangeScene(archive::IArchive&, const wstring&)`, `LoadSceneAdditive(path, name)`, `UnloadAdditiveScene(name)`, `GetAdditiveScene(name)`은 결과를 확인한다. `MarkDontDestroyOnLoad(scene, entity, group)` 및 `TransferPersistentEntities(from, to)`가 있지만 모든 외부 참조가 보존된다는 보장은 없다.
`SavePrefab(ar, path, root)` / `InstantiatePrefab(ar, path, outRoot, parent)`의 EResult와 outRoot를 함께 확인한다. 저장 데이터는 원본 씬/프리팹과 구별한다. 인스턴스의 직접 갱신 계약은 [기존 프리팹 계획](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/PrefabSceneWorkflowPlan.md)를 참고한다.

## 주의
registry 직접 조작은 reflection 의존성, hierarchy 및 archive 불변식을 건너뛸 수 있다. 엔티티 삭제 중 순회 변경은 지연 처리하는 것을 권장한다. 씬 참조를 보관하는 이벤트 구독도 씬 종료에 맞춰 정리한다. 씬 객체 생성/Free를 게임 코드에서 직접 수행하기보다 SceneManager 수명주기를 따른다.

## 관련 소스
- [Entity.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/Entity.h) — `Engine/Source/World/Entity/Public/Entity.h`
- [Scene.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Scene/Public/Scene.h) — `Engine/Source/World/Scene/Public/Scene.h`
- [SceneManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Scene/Public/SceneManager.h) — `Engine/Source/World/Scene/Public/SceneManager.h`
- [ComponentEnable.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/ComponentEnable.h) — `Engine/Source/World/Entity/Public/ComponentEnable.h`

## 검증 및 참고 예제
- [SceneReflectionCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/SceneReflectionCheck.cpp)
- [SceneArchiveCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/SceneArchiveCheck.cpp)
- [SceneStreamingCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/SceneStreamingCheck.cpp)
