# 3D 게임 제작

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 첫 번째 씬: 물리 실험장

다음은 확인한 컴포넌트를 조합한 제작 예시이며 자동 생성되는 템플릿이 아니다.

| 엔티티 | 구성 | 예시 설정 |
|---|---|---|
| Ground | Cube 또는 Plane의 렌더러 + Collider + RigidBody | Cube를 얇고 넓게, Static, Box Collider |
| FallingBox | Cube + Collider + RigidBody | Ground 위에 배치, Dynamic, Mass 1, GravityScale 1 |
| Sun | Directional Light | 실험장을 비추도록 회전 |
| Camera | CameraComponent | 두 물체를 볼 수 있는 위치, 주 카메라 지정 확인 |

1. Hierarchy에서 위 엔티티를 생성한다.
2. Box Collider의 크기와 메시의 실제 크기를 확인한다. Ground는 충돌 확인이 쉬운 Cube를 추천한다.
3. FallingBox의 BodyType을 Dynamic으로 변경한다(기본 Static).
4. PIE Play에서 박스가 낙하해 바닥에 접촉하는지 확인한다.
5. Pause 후 Step으로 관찰하고 Stop 후 편집 씬 복원 여부를 확인한다.

Collider 형상과 RigidBody 설정은 [물리와 충돌](Physics.md)을 참고한다.

## 3인칭 캐릭터

**코드 확인:** `ThirdPersonControllerComponent`는 `TransformComponent`, `WorldTransformComponent`, `CharacterControllerComponent`를 요구하며 SpringArmEntity를 지정할 수 있다.

1. Player 루트 엔티티 생성. 모델 또는 기본 도형은 시각용으로 배치한다.
2. CharacterController 및 ThirdPersonController 추가.
3. SpringArm과 카메라로 추적 구조를 구성한다. ThirdPersonController의 SpringArmEntity를 명시적으로 연결하는 것을 권장한다.
4. 프로젝트 Input Actions에서 Gameplay 맵과 이동/점프/달리기/카메라 액션을 확인한다.
5. PIE에서 입력, 충돌, 지면 판정, 카메라 회전을 검증한다.
6. MouseSensitivity, SprintMultiplier, OrientRotationToMovement, RequireRightClickToRotate를 조절한다.
7. 그래프에서 이동을 공급할 경우 `UseGraphMoveInput`의 동작 조건을 확인한다.

CharacterController와 ThirdPersonController가 존재한다고 완성형 전투 캐릭터가 제공되는 것은 아니다. 카메라·애니메이션·피격·공격 판정은 별도로 구성한다.

애니메이션 연결은 [Animation](Animation.md), 물리 설정은 [Physics](Physics.md)를 참고한다.

## 관련 문서 및 소스

- [`EntityFactory.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Application/Private/EntityFactory.cpp) — `Editor/Source/Application/Private/EntityFactory.cpp`
- [`ThirdPersonControllerComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/ThirdPersonControllerComponent.h) — `Engine/Source/Physics/Public/ThirdPersonControllerComponent.h`
- [`RigidBodyComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/RigidBodyComponent.h) — `Engine/Source/Physics/Public/RigidBodyComponent.h`
- [`ColliderComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/ColliderComponent.h) — `Engine/Source/Physics/Public/ColliderComponent.h`
- [전체 근거 및 검토 범위](Sources.md)

