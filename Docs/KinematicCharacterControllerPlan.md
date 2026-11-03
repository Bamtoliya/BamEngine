# 키네마틱 캐릭터 컨트롤러(Kinematic Character Controller, KCC) 개발 계획

## 1. 개요 및 목적

- **목적**: 동역학 물리(Dynamic RigidBody)의 원치 않는 토크 회전, 튕김, 떨림 없이, 플레이어 또는 AI 캐릭터가 지형 및 장애물과 정확하고 부드럽게 상호작용할 수 있는 전용 키네마틱 이동 시스템을 구축한다.
- **배경**:
  - 기존 BamEngine의 3D 물리는 정적/동적 RigidBody와 단순 Raycast 중심이었다.
  - 캐릭터 이동 시 계단(Stairs) 오르기, 경사로(Slope) 보행 및 미끄러짐, 벽면 미끄러짐(Slide & Collide), 지면 스냅(Ground Snapping) 등의 기능이 없어 액션/어드벤처/시뮬레이션 캐릭터 제어에 한계가 있었다.
  - 병렬 진행 중인 `AnimationTimelineEditorPlan`과 완벽히 독립된 도메인(물리 시스템 및 캐릭터 이동 컴포넌트)으로 병목 없이 진행한다.

## 2. 아키텍처 설계

### 2.1 ECS 컴포넌트 & 시스템 분리
- **`CharacterControllerComponent` (`Engine/Source/Core/Public/Components/Physics/CharacterControllerComponent.h`)**:
  - 캐릭터의 충돌 캡슐 형상, 이동 속도, 점프력, 중력, 최대 등판 각도, 계단 오르기 오프셋 등 물리/이동 파라미터를 보관하는 데이터 컴포넌트.
  - 리플렉션 및 직렬화(`ReflectionArchive`)를 온전히 지원하여 씬 저장/로드 및 Inspector 편집 가능.
- **`KinematicCharacterSystem` (`Engine/Source/System/Physics/Public/KinematicCharacterSystem.h`, `Private/KinematicCharacterSystem.cpp`)**:
  - `ISystem`을 상속하며, 물리 안정성을 위해 `OnFixedUpdate`에서 모든 활성 `CharacterControllerComponent`를 일괄 시뮬레이션.
  - 씬의 `ColliderComponent` 및 `TerrainComponent`와 상호작용하여 충돌 감지 및 위치 보정을 수행.

### 2.2 핵심 알고리즘 흐름
1. **지면 접지 판정 (Ground Detection & Snapping)**:
   - 캐릭터 캡슐 하단에서 지면 방향(`-Y`)으로 하향 검사를 수행.
   - 바닥면의 법선(`GroundNormal`)과 경사각(`GroundAngle`) 산출.
   - 허용 경사각(`SlopeLimitDegrees`) 이내일 경우 `IsGrounded = true` 및 바닥에 스냅.
   - 초과 시 `IsGrounded = false` 처리 및 경사면을 따라 미끄러지는 외력 부여.
2. **수평 이동 및 장애물 슬라이드 (Horizontal Move & Slide)**:
   - 이동 희망 벡터(`MoveInput`)를 바닥면 법선에 투영하여 경사로를 따른 이동 벡터 계산.
   - 최대 3~4회 반복(Iteration)의 Sweep 검사로 장애물과 접촉 시 법선 방향으로 이동 벡터를 미끄러지게(Slide) 투영.
3. **턱/계단 오르기 (Stair Step-Up)**:
   - 전방 이동 중 충돌 발생 시, 장애물 높이가 `StepOffset`(기본 0.3m) 이하인지 판정.
   - 위로 `StepOffset`만큼 상승 -> 전방 이동 -> 아래로 `StepOffset`만큼 하강하여 계단 위에 자연스럽게 안착.
4. **수직 이동 및 중력/점프 (Vertical Motion, Gravity & Jump)**:
   - 공중에 있는 동안 중력(`Gravity * dt`)을 지속 누적.
   - 지면에서 점프 요청(`JumpRequested`) 시 상향 속도(`JumpForce`) 부여.
   - 천장 충돌(Ceiling Collision) 감지 시 상향 속도를 0으로 즉시 리셋.

## 3. 세부 사양

### 3.1 `CharacterControllerComponent` 사양
- **형상(Capsule) 파라미터**:
  - `f32 Height = 2.0f`: 전체 캡슐 높이 (머리 꼭대기부터 발바닥까지).
  - `f32 Radius = 0.5f`: 캡슐 반지름.
  - `vec3 Center = vec3(0.0f, 1.0f, 0.0f)`: 캐릭터 기준점(발바닥) 대비 캡슐 중심 오프셋.
  - `f32 SkinWidth = 0.05f`: 수치 오차 및 터널링 방지를 위한 접촉 여유 마진.
- **이동 파라미터**:
  - `f32 WalkSpeed = 5.0f`: 기본 걷기 속도 (m/s).
  - `f32 RunSpeed = 8.0f`: 달리기 속도 (m/s).
  - `f32 JumpForce = 7.0f`: 점프 초기 상승 속도 (m/s).
  - `f32 Gravity = -19.6f`: 중력 가속도 (m/s^2).
  - `f32 SlopeLimitDegrees = 45.0f`: 걸어 오를 수 있는 최대 경사면 각도.
  - `f32 StepOffset = 0.3f`: 걸어 오를 수 있는 최대 턱/계단 높이 (m).
  - `f32 AirControl = 0.2f`: 공중에서의 수평 방향 조작 반영 비율 (0~1).
  - `uint32 CollisionMask = 0xFFFFFFFFu`: 충돌 검사 대상 레이어 마스크.
- **런타임 상태**:
  - `vec3 Velocity = vec3(0.0f)`: 현재 실제 이동 속도 벡터.
  - `vec3 MoveInput = vec3(0.0f)`: 외부에서 전달받은 희망 이동 방향/벡터.
  - `bool IsGrounded = false`: 지면 접지 여부.
  - `bool IsStepping = false`: 계단/턱을 등판 중인지 여부.
  - `vec3 GroundNormal = vec3(0.0f, 1.0f, 0.0f)`: 지면 법선 벡터.
  - `f32 GroundAngle = 0.0f`: 지면 경사각.

### 3.2 제어 인터페이스 (Public API)
- `void Move(const vec3& direction)`: 수평 이동 입력 전달 (정규화된 방향 벡터).
- `void Jump()`: 점프 실행 요청.
- `void SetVelocity(const vec3& vel)`: 속도 직접 설정 (넉백, 포털 이동 등).
- `void Teleport(const vec3& position)`: 강제 위치 이동 및 잔류 속도 초기화.

## 4. 개발 로드맵

- [x] **Phase 1: 컴포넌트 및 기본 시스템 골격 구축**
  - `CharacterControllerComponent.h` 정의 (리플렉션 및 로캘 키 연결).
  - `KinematicCharacterSystem.h/.cpp` 클래스 생성 및 `ISystem` 등록.
  - `ReflectionRegistry.cpp`에 `CharacterControllerComponent` 등록.
- [x] **Phase 2: 지면 감지 및 기본 중력/점프 구현**
  - 하향 충돌 감지(하단 스윕 및 레이캐스트)와 `IsGrounded` 상태 갱신.
  - 중력 적용, 바닥 안착(Ground Snapping), 상향 점프 로직 검증.
- [x] **Phase 3: 수평 이동 및 장애물 슬라이딩 (Slide & Collide)**
  - 이동 희망 벡터를 향한 충돌 스윕(Sweep) 처리.
  - 벽면 충돌 시 법선 평면 투영(Slide) 및 다중 반복(Bounce loop) 처리.
- [x] **Phase 4: 경사로(Slope) 및 계단 오르기(Stair Step-Up)**
  - 등판 한계 각도 초과 시 경사면 미끄러짐.
  - 전방 장애물 발생 시 `StepOffset` 수직 상승 -> 전방 이동 -> 하향 안착 알고리즘 구현.
- [x] **Phase 5: 런타임 시스템 등록, 스크립팅 노드 및 에디터 뷰포트 시각화**
  - `Application.cpp` / `Client/Source/Main.cpp`에 `KinematicCharacterSystem` 시스템 등록 (2026-11-03 완료).
  - `GraphGameplayLibrary.h`에 `CharacterController` 비주얼 스크립팅 노드 7종 연결 (2026-11-03 완료).
  - `DebugRenderer.cpp`에 캐릭터 컨트롤러 캡슐 및 지면 법선 디버그 와이어프레임 렌더링 (2026-11-03 완료).
  - 런타임 및 에디터 뷰포트 통합 검증 완료.
