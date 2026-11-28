# 2D / 3D Physics

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 구성
3D는 `RigidBodyComponent`와 `ColliderComponent`, 2D는 `RigidBody2DComponent`와 `Collider2DComponent`를 구분한다. 3D BodyType 기본값은 Static; Dynamic은 Mass, LinearDamping, AngularDamping, GravityScale, FreezeRotation, UseCCD를 설정할 수 있다. 3D Collider의 shape variant는 Box/Sphere/Capsule/Mesh이며 Box.Size는 전체 크기, Capsule.Height는 반구 포함 전체 높이(Y축)다. shape setter가 유효성을 검사한다.

```cpp
#include "Scene.h"
#include "RigidBodyComponent.h"
#include "ColliderComponent.h"
bool AttachDynamic(Engine::Entity& e) {
    auto& body = e.AddComponent<Engine::RigidBodyComponent>();
    body.BodyType = Engine::ERigidBodyType::Dynamic;
    body.Mass = 2.f;
    auto& collider = e.AddComponent<Engine::ColliderComponent>();
    Engine::BoxShape box;
    box.Size = {1.f, 1.f, 1.f};
    return collider.SetShape(box);
}
```
전제: 각 컴포넌트가 없는 정상 씬 엔티티. 렌더러 메시와 collider 크기 및 scale 일치를 검증한다.

## Query API
| 함수 | 용도 |
|---|---|
| `Raycast(scene, ray, outHit, maxDistance = FLT_MAX)` | 첫 충돌; bool false 처리 |
| `RaycastAll(scene, ray, query)` | 모든 hit 목록 |
| `Sweep(scene, sphere/box/capsule, direction, outHit, query)` | 형상 이동 검사 |
| `SweepAll(...)` / `OverlapAll(scene, shape, query)` | 다중 충돌 / 겹침 검색 |
| `Overlap2D(entityA, entityB, outHitResult)` | 두 2D 엔티티 겹침 |
| `Overlap3D(scene, entityA, entityB, outHitResult)` | 두 3D 엔티티 겹침 |
| `GetTriggerEvents2D/3D(scene)` / `GetCollisionEvents3D(scene)` | 시스템이 제공하는 이벤트 span |

`RaycastQuery`, `SweepQuery`, `OverlapQuery` 설정은 헤더/구현을 추가 확인하여 필터링한다. 방향 정규화 및 distance 계약은 테스트 확인 없이 가정하지 않는다. 이벤트 span은 다음 시뮬레이션 및 씬 변경을 넘어 보관하지 않는다. `RegisterGlobalTriggerCallback`, `RegisterGlobalCollisionCallback`는 전역 등록/전체 clear만 공개 헤더에서 확인되어 개별 구독 해제 문제가 있다(A07).

## 캐릭터
`ThirdPersonControllerComponent`는 Transform, WorldTransform, CharacterController 의존성을 선언한다. `SpringArmEntity`, MouseSensitivity, RequireRightClickToRotate, SprintMultiplier, UseGraphMoveInput 등으로 입력을 연결한다. 일반 Dynamic rigid body를 캐릭터 컨트롤러로 대체할 때 중복 이동 경로를 제거한다. `PhysicsSystem::OnFixedUpdate`는 엔진이 호출한다. 힘/충격량의 고수준 API는 이번 조사에서 확정하지 않았으므로 존재하지 않는 AddForce 예제를 제공하지 않는다.

## 관련 소스
- [PhysicsSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/PhysicsSystem.h) — `Engine/Source/Physics/Public/PhysicsSystem.h`
- [ColliderComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/ColliderComponent.h) — `Engine/Source/Physics/Public/ColliderComponent.h`
- [RigidBodyComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/RigidBodyComponent.h) — `Engine/Source/Physics/Public/RigidBodyComponent.h`
- [ThirdPersonControllerComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/ThirdPersonControllerComponent.h) — `Engine/Source/Physics/Public/ThirdPersonControllerComponent.h`
- [Collider2DComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/Collider2DComponent.h) — `Engine/Source/Physics/Public/Collider2DComponent.h`
- [RigidBody2DComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/RigidBody2DComponent.h) — `Engine/Source/Physics/Public/RigidBody2DComponent.h`

## 검증 및 참고 예제
- [Physics2DCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/Physics2DCheck.cpp)
- [CollisionGeometryCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/CollisionGeometryCheck.cpp)
- [KinematicCharacterCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/KinematicCharacterCheck.cpp)
