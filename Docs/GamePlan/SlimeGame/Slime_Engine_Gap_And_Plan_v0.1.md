# 슬라임 게임 엔진 기능 격차(Gap) 분석 및 개발 계획서

| 항목 | 내용 |
| --- | --- |
| 문서 버전 | v0.1 |
| 작성일 | 2026-11-26 |
| 상태 | 정적 코드 조사 기반 초안 (빌드·실행·GPU 성능 미검증) |
| 대상 프로젝트 | Slime Project (`Projects/GameModule`) on BamEngine |
| 기준 기획서 | `Docs/GamePlan/SlimeGame/Slime_Game_Design_Draft_v0.1.md`, `Docs/GamePlan/SlimeGame/Slime_Movement_Design_v0.1.md` |
| 연계 엔진 계획 | `Docs/StylizedRenderingAndSpriteBakingPlan.md`, `Docs/DevelopmentPriorities.md` |

---

## 1. 조사 범위 및 한계

- **조사 방법**: BamEngine 최신 작업 트리의 엔진(`Engine/Source/`), 에디터(`Editor/Source/`), 클라이언트(`Client/Source/`), 셰이더(`Engine/Resources/Shader/hlsl/`), 게임 모듈(`Projects/GameModule/`) 소스 코드에 대한 정적 분석.
- **미검증 및 한계**:
  - 본 문서 작성 시점에 **빌드, 단위 테스트(CTest), 실제 에디터/클라이언트 실행, GPU 프레임타임 및 메모리 계측은 수행하지 않았다.**
  - `DirectX12RHI` 외 백엔드(`SDLGPURHI`, Vulkan, Metal, WebGPU)의 `BindDynamicVertexBuffer` 및 커스텀 패스 지원 여부는 정적으로 확인하지 않았다.
  - `PhysicsSettings::FixedTimeStep`(0.02f)과 `TimeManager::DefaultFixedStep`(1/60f)의 런타임 동기화 여부는 미확인 상태이다.
  - 따라서 아래의 모든 상태 판정과 우선순위는 **코드 존재 여부 기준의 정적 근거**이며, 실제 M0 단계에서 빌드·실행을 통해 검증해야 한다.

---

## 2. 요구사항 vs BamEngine 현황 요약 대시보드

| 영역 | 세부 요구사항 | 현재 상태 | 코드 근거 (파일·심볼) | 핵심 Gap |
| --- | --- | --- | --- | --- |
| **물리·입력·고정틱** | 고정 물리 틱 루프 | 바로 활용 가능 | `TimeManager` (`DefaultFixedStep`, `IsFixedUpdateReady`), `Runtime::FixedUpdate`, `SystemManager::FixedUpdate` (`ISystem::OnFixedUpdate`, `OnPostFixedUpdate`) | `PhysicsSettings::FixedTimeStep`(0.02f)과 `TimeManager` 동기화 확인 필요 |
| **물리·입력·고정틱** | 틱 단위 입력 버퍼링 | 부분 지원 (확장 필요) | `InputManager::Update`, `ClientGameRuntime::RunFrame`, `Editor/Application.cpp` | 렌더 프레임당 1회 갱신으로 `FixedUpdate` 0회 시 유실, 2회 이상 시 `Pressed/Released` 중복 소비 |
| **물리·입력·고정틱** | KCC 물리 틱 동기화 | 부분 지원 (확장 필요) | `KinematicCharacterSystem::OnFixedUpdate` (no-op), `UpdateCharacter()` (`OnUpdate` 실행) | KCC가 가변 `OnUpdate`에서 실행되어 `PhysicsSystem::OnFixedUpdate`와 틱 분리 |
| **물리·입력·고정틱** | 공간 질의 (Raycast/Sweep/Overlap) | 바로 활용 가능 | `PhysicsSystem::Raycast`, `SweepSphere/Box/Capsule`, `Overlap3D` (`Collision.h`) | `Overlap3D`는 `MeshShape` 미지원(Box/Sphere/Capsule만 지원) |
| **물리·입력·고정틱** | 트리거 및 비트리거 충돌 콜백 | 부분 지원 (확장 필요) | `TriggerCallbackComponent`, `ScriptableEntity::OnTriggerEnter/Stay/Exit`, `RegisterGlobalTriggerCallback` | 비트리거 `OnCollisionEnter/Stay/Exit` 부재 (`ResolvePhysicsContact3D`가 이벤트 미발행) |
| **물리·입력·고정틱** | Contact Manifold & 상대속도 | 미구현 (신규 필요) | `HitResult` (`Engine/Source/Core/Geometry/Public/HitResult.h`) | 단일 접촉점(`point`, `normal`, `distance`)만 보유; 다중 접촉점·상대속도 부재 |
| **물리·입력·고정틱** | 고속 이동 CCD | 미구현 (신규 필요) | `PhysicsSystem::OnFixedUpdate`, `RigidBodyComponent` | `RigidBodyComponent` 3D CCD/TOI 부재 (고속 새총·분열 파편 터널링 위험) |
| **물리·입력·고정틱** | 런타임 Collider 변경 & 엔티티 생성 | 바로 활용 가능 | `ColliderComponent::SetShape/SetOffset/SetSettings`, `CharacterControllerComponent`, `Scene::CreateEntity/DestroyEntity` | `PhysicsMaterial` 독립 에셋 부재 (`ColliderSettings` 인라인 값만 존재) |
| **절차적 메시·셰이더·SDF** | 동적 정점/노멀 in-place 갱신 | 부분 지원 (확장 필요) | `Mesh::SetStreamBuffer`, `DynamicGeometryDrawCommand`, `DirectX12RHI::BindDynamicVertexBuffer` | `SetStreamBuffer`는 매 호출 GPU 버퍼 재할당; `DynamicGeometryDrawCommand`는 스프라이트 VS 전용 |
| **절차적 메시·셰이더·SDF** | 동적 AABB 및 컬링 정합 | 부분 지원 (확장 필요) | `Mesh::m_BoundingBoxMin/Max`, `MeshRenderSystem::OnSubmit` | `activeMesh->IsDynamic()`일 때 `worldBounds` 생략으로 프러스텀 컬링 비활성화 |
| **절차적 메시·셰이더·SDF** | WPO 그림자 패스 동기화 | 미구현 (신규 필요) | `Renderer::PrepareMeshPipelineDesc`, `ShaderGraphCompiler`, `shadowDepthVS.hlsl` | `Shadow` 패스 오버라이드 미지정 시 기본 `shadowDepthVS` 폴백; 그래프 WPO 미동기화 |
| **절차적 메시·셰이더·SDF** | Toon / Fake SSS 셰이딩 | 미구현 (신규 필요) | `shading_flags.hlsli` (`SHADING_MODEL_TOON`, `SUBSURFACE`), `lightingPS.hlsl` | 플래그만 정의되어 있고 `lightingPS.hlsl`은 `UNLIT` 외 모두 `DEFAULT_LIT` PBR 처리 |
| **절차적 메시·셰이더·SDF** | SDF Raymarching / 커스텀 패스 | 부분 지원 (확장 필요) | `RenderPipelineProfile`, `DeferredProfilePasses`, `Renderer::SubmitCustomCommand` | 프로필 허용 패스 7종 고정; `SDFRaymarch` 등 임의 커스텀 패스 선언 불가 |
| **저해상도·카메라** | 저해상도 내부 RT + 정수배 업스케일 | 미구현 (신규 필요) | `ClientRenderPipeline`, `RenderPipelineProfile` (`ValidateRenderPipelineProfile`), `clientPresentPS.hlsl` | 비-Shadow 패스 `Resolution` 지정 불가; UI가 저해상도 씬과 동일 RT에 그려진 뒤 Linear 출력 |
| **저해상도·카메라** | 회전 직교 카메라 Texel Snapping | 부분 지원 (확장 필요) | `PixelPerfectCameraComponent`, `CameraSystem::UpdateCamera` | 월드 XY만 스냅하여 회전된 3D 직교 뷰의 Right/Up 축 스냅 불가; `OnLateUpdate` `(0,0)` 폴백 |
| **VFX·데칼·오디오** | 3D 메시 파티클 (액적) | 부분 지원 (확장 필요) | `ParticleEmitterComponent`, `ParticleSystem.cpp` (`meshHandle`, 충돌/서브이미터 지원) | `Particle`이 `vec2 size`, `f32 rotation`만 보유하고 `SubmitSprite`로 제출 (3D 스케일/회전/속도 정렬 부재) |
| **VFX·데칼·오디오** | 독립 Trail / Stretch Strand | 부분 지원 (확장 필요) | `CompositeEffect.h` (`VFXTrailTrack`), `SplineMeshComponent` (`DeformToPath`) | 독립 `TrailComponent` 부재; 두 지점을 실시간 연결·절단하는 동적 Strand 기능 부재 |
| **VFX·데칼·오디오** | 데칼 수명·풀링·UV 애니메이션 | 부분 지원 (확장 필요) | `DecalComponent`, `DeferredSceneRenderer.cpp` (`MaxDecalsPerView = 512`), `VFXDecalTrack` | 독립 `DecalComponent`에 `lifetime`/자동 풀링/UV 오프셋·애니메이션 부재 |
| **VFX·데칼·오디오** | 3D OneShot 오디오·피치·동시성 | 부분 지원 (확장 필요) | `AudioManager::PlayOneShot`, `AudioSourceComponent`, `AudioSystem::UpdateSources` | `PlayOneShot`이 2D 전용이며 pitch 변조 및 Concurrency/Cooldown 부재 (`UpdateSources` 과다 로그) |
| **에디터·디버그·프리셋** | 런타임 공용 `DebugDraw` | 미구현 (신규 필요) | `Editor::DebugRenderer` (`Editor/Source/ImGui/ViewportPanel/DebugRenderer/`) | `Engine/Source/` 공용 `DebugDraw` 큐 부재로 `GameModule`에서 직접 드로우 불가 |
| **에디터·디버그·프리셋** | PIE 단일 틱 전진 (`Single-Step`) | 미구현 (신규 필요) | `PIEManager` (`StartPlay/Pause/Resume/Stop`) | `Pause`만 존재하고 1프레임/1물리틱 전진 API 및 UI 버튼 부재 |
| **에디터·디버그·프리셋** | 물질 프리셋 & PIE 변경 유지 | 미구현 (신규 필요) | `PIEManager::Stop`, `InspectorPanel.cpp` | PIE 종료 시 씬 스냅샷 복원으로 튜닝값 소실; 전용 프리셋 저장/로드 워크플로우 부재 |

---

## 3. 책임 경계 원칙: 엔진 범용 기능 vs GameModule (`Projects/GameModule`)

`Slime_Game_Design_Draft_v0.1.md` 9.3절("슬라임 전용 기능을 무조건 범용 엔진 모듈로 확장하지 않는다")에 따라, **재사용 가능한 범용 인프라만 엔진/에디터 코어에 두고, 슬라임 고유의 수식·상태 머신·프리셋 스키마는 `Projects/GameModule`에 격리**한다.

| 계층 | 담당 범위 (소유 책임) | 포함 항목 예시 | 비고 |
| --- | --- | --- | --- |
| **BamEngine 코어**<br>(`Engine/Source/`) | 장르 무관 범용 런타임·물리·렌더링·VFX·오디오·디버그 인프라 | • `FixedUpdate` 입력 소비 래치/버퍼<br>• 비트리거 `OnCollisionEnter/Stay/Exit` + Contact 정보 + 3D CCD<br>• `Mesh` in-place 동적 정점/노멀 버퍼 갱신 + 동적 AABB<br>• `ShaderGraph`/`Renderer` Shadow WPO 동기화, `SHADING_MODEL_TOON`/`SUBSURFACE`<br>• `RenderPipelineProfile` 저해상도 내부 RT + Point 정수배 업스케일 + UI 분리 합성<br>• `CameraSystem` Right/Up 축 Texel Snapping<br>• 3D 메시 파티클(`vec3` 스케일/3D 회전/속도 정렬), 독립 `TrailComponent`, 데칼 UV/풀링<br>• `AudioManager::PlayOneShot3D` + Concurrency/Cooldown<br>• 엔진 공용 `DebugDraw` 큐 | `StylizedRenderingAndSpriteBakingPlan.md`의 ST-1/ST-2/PX-1/PX-2와 통합 설계 |
| **BamEditor / Client**<br>(`Editor/Source/`, `Client/Source/`) | 범용 제작·디버깅·실행 파이프라인 | • `PIEManager` `Single-Step`(1물리틱/1프레임 전진) 및 PIE 변경사항 유지 옵션<br>• `ViewportPanel` 내 엔진 `DebugDraw` 큐 렌더링 연결<br>• `ClientRenderPipeline` 저해상도-UI 분리 합성 패스 순서 적용 | 기존 `ShortcutManager` 및 로캘 CSV 규칙 준수 |
| **Slime GameModule**<br>(`Projects/GameModule/`) | 슬라임 고유의 물리 제어·절차적 변형·연출 규칙·데이터 스키마 | • 슬라임 물질 파라미터(0~100 정규화 값) 및 물리량 변환 곡선/프리셋 에셋<br>• `SlimeMovementSolver` (Hopping, Sliding, 벽/모서리 Surface Locomotion, Elastic Jump, Slingshot, Bounce/Splat)<br>• 정점 변형 수식(Squash & Stretch, 감쇠 스프링, 볼륨 보존 근사, 국소 압축, 노멀 재계산)<br>• 분열/합체 상태 머신 (`Scene::CreateEntity/DestroyEntity`, 질량 분배, `ColliderComponent::SetShape` 갱신)<br>• SDF/Metaball 필드 평가 또는 Procedural Bridge/Stretch Strand 형상 계산<br>• 충돌 강도→사운드 피치/데칼 크기/액적 파티클 매핑 및 슬라임 전용 `DebugDraw` 시각화 | `CMakeLists.txt`의 `GAME_REFLECTION_HEADERS`에 신규 컴포넌트 헤더 등록 필수 |

---

## 4. 영역별 상세 Gap 분석 및 기술 설계 방향

### (A) 입력 · Fixed Update · 물리

1. **입력 버퍼 유실 및 중복 소비 (`InputManager`, `ClientGameRuntime`, `Application`)**
   - **현황**: `InputManager::ProcessEvent()`는 `m_PendingKeyDown/Up` 등을 큐잉하고 `InputManager::Update(dt)`에서 `m_KeyDownEvents`, `Action.Pressed/Released` 스냅샷으로 전환한다. 그러나 `ClientGameRuntime::RunFrame`과 `Editor/Application.cpp` 모두 `InputManager::Update(dt)` 및 `EvaluateActionMap("Gameplay", ...)`를 렌더 프레임당 1회 `while (timeManager.IsFixedUpdateReady()) FixedUpdate(...)` 루프 **직전**에 호출한다.
   - **문제**: 고주사율(144 FPS 등)에서 `FixedUpdate`가 0회 실행되는 렌더 프레임이 발생하면 해당 프레임의 `Pressed/Released`가 다음 렌더 프레임에 덮어써져 **입력 유실**이 발생한다. 반대로 저주사율(30 FPS 등)에서 `FixedUpdate`가 2회 이상 실행되면 동일한 `Pressed/Released`가 **중복 소비**된다(Space 짧은 점프 vs 홀드 충전, 새총 발사 판정 오류 유발).
   - **설계 방향**: 렌더 프레임에서 수집된 Edge 이벤트(`Pressed/Released`)를 `FixedInputBuffer`에 래치(latch)해 두고, 첫 번째 `FixedUpdate` 틱에서만 소비 후 클리어하는 구조를 `InputManager` 또는 `Runtime` 레벨에 추가한다.

2. **KCC와 물리 틱 분리 (`KinematicCharacterSystem`)**
   - **현황**: `KinematicCharacterSystem::OnFixedUpdate`는 빈 함수(no-op)이며, 실제 이동 `UpdateCharacter()`는 가변 렌더 틱 `OnUpdate()`에서 실행된다. 반면 `PhysicsSystem`은 `OnFixedUpdate()`에서 실행된다. 또한 `PhysicsSettings::FixedTimeStep`(0.02f)과 `TimeManager::DefaultFixedStep`(1/60f)의 일치 여부가 확인되지 않았다.
   - **설계 방향**: 슬라임은 동적 `RigidBodyComponent` 또는 사용자 정의 Swept Controller(`Collision::Sweep`) 중 단일 권위 솔버를 선택하되, 어떤 방식을 쓰든 이동·충돌 솔버가 **`OnFixedUpdate`에서 고정 스텝으로만 위치/속도를 갱신**하고 `OnUpdate`에서는 시각 보간만 수행하도록 틱 계약을 통일한다. (엔진 `KinematicCharacterSystem`에 고정 틱 실행 옵션 또는 `OnFixedUpdate` 이관 검토)

3. **비트리거 충돌 콜백 및 Contact Manifold 부재 (`PhysicsSystem`, `HitResult`)**
   - **현황**: 트리거(`IsTrigger = true`)는 `TriggerCallbackComponent`, `ScriptableEntity::OnTriggerEnter/Stay/Exit`, `PhysicsSystem::RegisterGlobalTriggerCallback`을 통해 `TriggerEvent`가 발행되지만, 비트리거 충돌은 `ResolvePhysicsContact3D`에서 위치/속도 해소만 수행하고 이벤트를 발행하지 않는다. 또한 `HitResult`는 단일 접촉점(`point`, `normal`, `distance`/`penetrationDepth`)만 보유한다.
   - **설계 방향**:
     - 엔진 `PhysicsSystem`에 비트리거 `CollisionEvent`(`entityA`, `entityB`, `contactPoint`, `contactNormal`, `relativeVelocity`, `impulse`) 및 `OnCollisionEnter/Stay/Exit` 콜백 디스패치 경로를 추가한다.
     - 바닥↔벽 내부 모서리 전환이나 Splat 국소 압축을 위해 다중 접촉점(최소 2~4점 Contact Manifold 또는 다중 `Sweep` 결과 수집) 지원 여부를 검토한다.

4. **고속 이동 CCD 및 PhysicsMaterial (`RigidBodyComponent`, `ColliderSettings`)**
   - **현황**: `PhysicsSystem::OnFixedUpdate`와 `RigidBodyComponent`에는 CCD/TOI가 없으며 이산 적분 후 `PositionIterations`로 해소한다. `ColliderSettings`에는 `Friction`, `Restitution`, `CollisionLayer`, `CollisionMask`, `IsTrigger`가 인라인으로만 존재한다.
   - **설계 방향**:
     - 새총 발사(`Slingshot`) 및 고속 반발(`Strong Bounce`) 시 얇은 벽 관통을 막기 위해 `RigidBodyComponent`에 3D Swept CCD(또는 `SweepCapsule`/`SweepSphere` 기반 TOI 서브스텝)를 추가한다.
     - 0~100 정규화 슬라임 물질 프리셋과 지형 표면별 점착 허용/마찰 계수 보정은 초기 단계에서 `GameModule` 데이터 에셋/구조체로 관리하고, 필요 시 엔진 `PhysicsMaterial` 에셋으로 승격한다.

---

### (B) 절차적 메시 · 셰이더 · SDF

1. **In-place 동적 정점/노멀 버퍼 API 및 동적 AABB (`Mesh`, `MeshRenderSystem`, `Renderer`)**
   - **현황**: `Mesh::SetStreamBuffer(EMeshStream, data, count, stride)`는 호출 시마다 기존 `RHIBuffer`를 `Safe_Release`하고 `rhi->CreateBuffer`로 GPU 버퍼를 새로 할당한다. 반면 링 버퍼 기반 스트리밍 경로인 `DynamicGeometryDrawCommand`(`DirectX12RHI::BindDynamicVertexBuffer`)는 텍스처 1장과 스프라이트 VS를 가정하므로 `Mesh` 기반 Shadow/TAA/Picking/Raytracing과 통합되지 않는다. 또한 `activeMesh->IsDynamic()`일 때 `MeshRenderSystem::OnSubmit`이 `worldBounds`를 비워 프러스텀 컬링이 꺼진다.
   - **설계 방향**:
     - `Mesh`(또는 RHI 버퍼)에 업로드 힙/동적 스테이징 기반 **in-place 업데이트 API(`UpdateStreamBufferInPlace`)**를 추가하여 GPU 메모리 재할당 없이 매 프레임 정점 위치·노멀을 갱신한다.
     - `SetStreamBuffer` 또는 `UpdateStreamBufferInPlace` 호출 시 갱신된 `m_BoundingBoxMin/Max`를 `MeshRenderSystem::OnSubmit`의 `StaticDrawCommand::worldBounds`에 반영하여 동적 변형 메시의 프러스텀 컬링 정확도를 확보한다.

2. **`ShadowDepthPass` WPO 동기화 (`Renderer`, `ShaderGraphCompiler`)**
   - **현황**: `Renderer::PrepareMeshPipelineDesc`에서 `ERenderPassType::Shadow` 시 머티리얼에 `Shadow` 패스 셰이더 오버라이드(`SetPassShader`)가 없으면 `m_DefaultShadowDepthVS`(`shadowDepthVS.hlsl`)로 폴백한다. `ShaderGraphCompiler`는 `Geometry` 패스 VS/PS만 생성하므로 GPU 셰이더에서 `worldPositionOffset`(WPO)으로 변형해도 그림자 실루엣이 따라가지 않는다.
   - **설계 방향**:
     - CPU 측에서 `Mesh` 정점을 직접 변형(`UpdateStreamBufferInPlace`)하는 경우 `ShadowDepthPass`가 동일 `Mesh` 버퍼를 사용하므로 그림자가 자동 일치한다(초기 P0 안전 경로).
     - GPU 셰이더(`worldPositionOffset`) 기반 변형을 병행하려면 `ShaderGraphCompiler`가 `Shadow` 패스 전용 VS도 함께 컴파일하여 `SetPassShader(ERenderPassType::Shadow, ...)`로 자동 등록하도록 확장한다.

3. **Toon / Fake SSS 셰이딩 모델 (`shading_flags.hlsli`, `lightingPS.hlsl`)**
   - **현황**: `shading_flags.hlsli`에 `SHADING_MODEL_TOON(2u)`, `SHADING_MODEL_SUBSURFACE(3u)` 상수가 정의되어 있으나 `lightingPS.hlsl` 실제 분기는 `SHADING_MODEL_UNLIT(1u)` 외 모두 `DEFAULT_LIT` PBR로 처리된다. `ForwardTransparent` 패스에는 `SceneColorCopy`(`t0, space3`), `GBuffer_Position`(`t4, space3`) 바인딩과 `glassPS.hlsl`/`waterPS.hlsl`이 구현되어 있다.
   - **설계 방향**:
     - **`StylizedRenderingAndSpriteBakingPlan.md` ST-1과 연계**하여 `lightingPS.hlsl`에 `SHADING_MODEL_TOON` 및 `SHADING_MODEL_SUBSURFACE`(Fake SSS / Thickness Approximation / Stylized Highlight / Fresnel) 분기를 구현한다.
     - 초기 P0에서는 불투명 G-Buffer 기반 Fake SSS로 캐릭터성을 검증하고, P1에서 굴절/반투명이 필요할 경우 기존 `ForwardTransparent` 패스(`SceneColorCopy`)를 활용한다.

4. **SDF Raymarching / Metaball / 커스텀 렌더 패스 제약 (`RenderPipelineProfile`, `DeferredSceneRenderer`)**
   - **현황**: `RenderPipelineProfile`은 `DirectionalShadow`, `PointShadow`, `GBuffer`, `DeferredLighting`, `ForwardOpaque`, `ForwardTransparent`, `ScenePostProcess` 7종만 허용하며 `DeferredSceneRenderer`는 고정 바인딩(`DeferredScenePassBindings`)을 사용한다.
   - **설계 방향**:
     - **단기(P0)**: 엔진 파이프라인 구조를 크게 흔들지 않기 위해, 분열·합체 연결 표면은 **CPU Marching Cubes / Procedural Bridge Mesh 추출 → `Mesh::UpdateStreamBufferInPlace` 제출** 방식 또는 `ForwardOpaque`/`ForwardTransparent` 내 `Renderer::SubmitCustomCommand` 우회 경로를 우선 검증한다.
     - **중기(P1/P2)**: SDF Raymarching을 정식 도입할 경우 `RenderPipelineProfile`에 커스텀 패스 타입(`CustomPass` / `SDFRaymarch`) 및 G-Buffer 깊이/그림자/조명 합성 계약을 확장한다.

---

### (C) 저해상도 3D 렌더링 및 카메라

1. **내부 저해상도 RT + Nearest 정수배 업스케일 + 풀 해상도 UI 분리 합성**
   - **현황**: `ClientRenderPipeline`은 풀 해상도(`width`, `height`) 기준으로 RT를 생성하고, `RenderPipelineProfile`의 `ValidateRenderPipelineProfile`은 Shadow 외 패스의 `Resolution` 지정을 금지한다. 또한 `m_UIPass`가 씬 컬러 RT에 UI를 먼저 그린 뒤 `m_PresentPass`가 `ESamplerFilter::Linear` 기본 샘플러로 백버퍼에 출력한다.
   - **설계 방향 (`StylizedRenderingAndSpriteBakingPlan.md` PX-1 연계)**:
     - `RenderPipelineProfile`에 씬 기준 해상도(`ReferenceWidth`, `ReferenceHeight`, `ScaleMode: IntegerNearest`) 정책을 추가한다.
     - `ClientRenderPipeline` 및 Editor Game View 파이프라인에서 **저해상도(480×270 / 640×360) 씬 렌더 및 후처리 → Point(Nearest) 정수배 업스케일(+ Letterbox/Pillarbox) → 풀 해상도 `m_UIPass` 별도 합성** 순서로 패스 순서를 재구성한다.
     - 마우스 피킹/UI 입력 좌표 변환 시 정수배 스케일 및 레터박스 오프셋을 반영한다.

2. **회전 직교 카메라 Right/Up 축 Texel Snapping (`PixelPerfectCameraComponent`, `CameraSystem`)**
   - **현황**: `PixelPerfectCameraComponent`와 `CameraSystem::UpdateCamera`는 존재하나, `camPos.x`, `camPos.y`(월드 X/Y 축)만 반올림 스냅한다. 또한 `CameraSystem::OnLateUpdate`가 `UpdateCameras(registry)`를 인자 `(0, 0)`으로 호출해 `vpW/vpH`가 1920×1080으로 폴백되는 경로가 존재한다.
   - **설계 방향 (`StylizedRenderingAndSpriteBakingPlan.md` PX-2 연계)**:
     - 3D 직교 카메라(탑다운/아이소메트릭 등 회전 포함)를 위해 카메라 로컬 `Right`(`finalWorld[0]`), `Up`(`finalWorld[1]`) 축 투영 좌표 기준으로 텍셀 크기 `u = OrthoHeight / RenderHeight` 격자에 스냅하되, 논리적 `Transform`은 유지하고 렌더 뷰 행렬만 보정한다.
     - 에디터/클라이언트 모두 실제 활성 뷰포트 해상도를 `CameraSystem`에 전달하도록 호출 경로를 일원화한다.

---

### (D) VFX · 데칼 · 오디오

1. **3D 메시 파티클 (`ParticleEmitterComponent`, `ParticleSystem`)**
   - **현황**: `meshHandle`, `EParticleCollisionMode`(FloorPlane/PhysicsRaycast), `EParticleCollisionResponse`(Bounce/Slide/DieOnContact/Stop), 서브이미터(`OnParentCollision`/`OnParentDeath`)는 구현되어 있으나, `Particle` 구조체가 `vec2 size`, `f32 rotation`만 보유하고 `ParticleSystem.cpp`가 2D 행렬로 `SubmitSprite`에 제출한다.
   - **설계 방향**: `Particle`에 `vec3 scale3D`, `quat rotation3D`(또는 Euler), `velocityAlignment` 옵션을 확장하고, `meshHandle`이 유효할 때 `StaticDrawCommand`(3D 메시 패스) 또는 3D 인스턴싱 배치로 제출하여 분열 액적이 3D 조명/그림자를 받으며 속도 방향으로 늘어나도록 한다.

2. **독립 `TrailComponent` 및 동적 Stretch Strand (`CompositeEffect`, `SplineMeshComponent`)**
   - **현황**: `VFXTrailTrack`은 `CompositeVFXComponent`에 종속되어 이동 중 동적 시작/중단·폭 제어가 경직되어 있으며, `SplineMeshComponent`는 정적/경로 캐시 중심이다.
   - **설계 방향**:
     - 범용 **독립 `TrailComponent`**(거리/시간 기반 샘플링, 폭·색상·알파 커브, 수명 페이드)을 `Engine/Source/VFX/`에 추가한다.
     - 새총 Anchor↔Body, 분열 절단 시 목이 가늘어지는(necking) **동적 Stretch Strand**는 엔진의 `Mesh::UpdateStreamBufferInPlace` API를 호출하는 `GameModule` 전용 절차적 메시 생성기로 구현한다.

3. **Deferred Decal 수명·풀링·UV 애니메이션 (`DecalComponent`, `DeferredSceneRenderer`)**
   - **현황**: G-Buffer 기반 `DecalComponent`와 `MaxDecalsPerView = 512`, `SortOrder`, `MaxAngle`, `DepthFade`, `Opacity`는 완비되어 있으나, 독립 `DecalComponent`에 `lifetime`/자동 페이드/풀링/UV 오프셋 애니메이션이 없다. 512개 초과 시 `SortOrder`가 큰 것부터 단순 드롭된다.
   - **설계 방향**: 엔진 `DecalComponent` 및 `DecalDrawData`에 `uvOffset`, `uvScale`(벽면 흘러내림 UV 애니메이션용)을 추가하고, `GameModule`에 링 버퍼 기반 `SlimeTrailDecalPool`(최대 개수 제한, 거리 기반 스폰 간격, 수명/페이드아웃 후 재사용)을 구축한다.

4. **3D OneShot 오디오·피치 변조·Concurrency (`AudioManager`, `AudioSystem`)**
   - **현황**: `AudioManager::PlayOneShot`은 2D 전용이며 `pitch` 인자가 없다. 동시 발음 제한(Concurrency/Cooldown)이 없고, `AudioSystem::UpdateSources`에서 매 3D 재생마다 `Info` 로그를 출력한다.
   - **설계 방향 (`DevelopmentPriorities.md` 11.8절 연계)**:
     - `AudioManager::PlayOneShot3D(sound, pos, volume, pitch, bus)` 추가 및 믹서/그룹 단위 `MaxConcurrentVoices`, `MinRetriggerInterval(Cooldown)` 지원.
     - `AudioSystem::UpdateSources`의 매 재생 `Info` 로그를 `Trace` 레벨 또는 제거로 정리한다.

---

### (E) 에디터 · 디버그 · 프리셋 도구

1. **엔진 공용 `DebugDraw` 큐 (`Editor::DebugRenderer` 분리)**
   - **현황**: `Editor::DebugRenderer`가 `Editor/Source/`에 국한되어 `Projects/GameModule`에서 표면 법선, 접선 프레임, 접촉점, 새총 Anchor, 충돌 프록시를 시각화할 수 없다.
   - **설계 방향**: `Engine/Source/Debug/`(또는 `Render/`)에 가벼운 스레드 안전 `DebugDraw` 정적 큐(`DrawLine`, `DrawSphere`, `DrawBox`, `DrawArrow`, `duration`)를 두고, 에디터 `ViewportPanel`이 프레임마다 해당 큐를 소비해 `Editor::DebugRenderer`로 출력하게 연결한다(`BAM_SHIPPING` 시 컴파일 타임 스트립).

2. **PIE `Single-Step` (`PIEManager`, `ToolBar`)**
   - **현황**: `PIEManager`에 `StartPlay`/`Pause`/`Resume`/`Stop`만 있고 1프레임 또는 1고정틱 전진이 없다.
   - **설계 방향**: `PIEManager::StepFrame()` / `StepFixedTick()`을 추가하여 `Paused` 상태에서 정확히 `TimeManager::GetFixedStep()`만큼 1틱 진행하고 다시 멈추도록 구현한다(슬라임 압축·반발·벽 전환 프레임 단위 검증 필수 도구).

3. **물질 프리셋 에셋 및 PIE 변경 유지 (`PIEManager`, `InspectorPanel`)**
   - **현황**: `PIEManager::Stop` 시 씬 JSON 스냅샷 전체 복원으로 PIE 중 인스펙터 튜닝값이 사라진다.
   - **설계 방향**: `GameModule`에 슬라임 물질 프리셋 JSON/데이터 에셋(`.slimepreset.json` 등) 로드·저장 유틸리티를 두어 PIE 중에도 파일로 즉시 저장/불러오기할 수 있게 하고, 중기적으로 에디터 `PIEManager`에 선택적 변경 유지(또는 프리셋 직렬화) 워크플로우를 연결한다.

---

## 5. 엔진 보강 우선순위 (P0 / P1 / P2) 및 마일스톤 매핑

`Slime_Movement_Design_v0.1.md` 16절(M0~M10) 및 `Slime_Game_Design_Draft_v0.1.md` 8.3절/9.2절과 연계한 실행 로드맵이다.

| 작업 ID | 우선순위 | 작업명 | 선행 조건 | 대상 모듈 | 완료 기준 (DoD) | 기획 마일스톤 매핑 |
| --- | --- | --- | --- | --- | --- | --- |
| **SE-01** | **P0** | `FixedUpdate` 입력 래치/버퍼링 & KCC/물리 틱 정합성 확보 | 없음 | Engine (`Input`, `Runtime`, `Physics`), Client | 30/60/144 FPS 환경에서 `Pressed/Released` 유실·중복 소비 0건, 이동 솔버 `OnFixedUpdate` 단일 권위 실행 | `M0`, `M1`, `M3` |
| **SE-02** | **P0** | 비트리거 충돌 콜백(`OnCollisionEnter/Stay/Exit`) 및 접촉 정보 확장 | 없음 | Engine (`Physics`, `Core/Geometry`) | `ResolvePhysicsContact3D` 충돌 시 법선·접촉점·상대속도 이벤트 발행 및 CTest 단위 검증 통과 | `M1`, `M4` (단계 2) |
| **SE-03** | **P0** | `Mesh` in-place 동적 정점/노멀 업데이트 API & 동적 AABB 컬링 정합 | 없음 | Engine (`Render/Mesh`, `RHI`) | 매 프레임 `RHIBuffer` 재할당 없이 변형 메시 및 `ShadowDepthPass` 그림자 동기화, 동적 `worldBounds` 반영 | `M2` (단계 1~2) |
| **SE-04** | **P0** | 엔진 공용 `DebugDraw` 큐 및 PIE `Single-Step`(1틱 전진) | 없음 | Engine (`Debug`), Editor (`PIE`, `Viewport`) | `GameModule`에서 법선·접촉점·속도 벡터 시각화 가능, `Pause` 중 `Single-Step`으로 1고정틱 진행 확인 | `M0`~`M4` (Playgound 15.1) |
| **SE-05** | **P0** | Toon / Fake SSS 셰이딩 모델 (`SHADING_MODEL_TOON`/`SUBSURFACE`) | `ST-0` (`StylizedRendering`) | Engine (`Render`, `Shader/hlsl`) | `lightingPS.hlsl`에서 PBR과 Toon/Fake SSS 동시 렌더링, Fresnel/Stylized Highlight 제어 가능 | 단계 1 (9.2절 P0) |
| **SE-06** | **P0** | 3D 메시 파티클 확장 (`vec3` 스케일, 3D 회전, 속도 정렬) | 없음 | Engine (`VFX`) | `meshHandle` 파티클이 3D 조명·충돌 반발과 함께 속도 방향으로 신장·회전하며 액적 연출 가능 | `M4`, `M10` (단계 3) |
| **SE-07** | **P1** | `RigidBody` 3D CCD / Swept TOI 및 `AudioManager::PlayOneShot3D` + Concurrency | `SE-02` | Engine (`Physics`, `Audio`) | 고속 새총 발사 속도에서 얇은 벽 터널링 방지, 3D 피치 변조 및 연속 충돌 시 오디오 클리핑 방지 | `M4`, `M7`, `M8` |
| **SE-08** | **P1** | 독립 `TrailComponent` & `DecalComponent` UV 오프셋/애니메이션 확장 | `SE-03` | Engine (`VFX`, `Render/Decal`) | 이동 흔적 리본과 벽면 흘러내림 데칼 UV 애니메이션 동작, `GameModule` 풀링과 결합해 수명 관리 | `M5`, `M6`, `M7` |
| **SE-09** | **P1** | 저해상도 내부 RT + Point 정수배 업스케일 + 풀 해상도 UI 분리 합성 | `ST-0` (`PX-1`) | Engine (`Render/Pipeline`), Client, Editor | 480×270 / 640×360 내부 렌더 후 정수배 Nearest 업스케일, UI 선명도 및 피킹 좌표 일치 | 단계 4 (9.2절 P1) |
| **SE-10** | **P1** | 회전 직교 카메라 Right/Up 축 Texel Snapping & 호출 경로 통일 | `SE-09` (`PX-2`) | Engine (`Camera`) | 회전된 직교 3D 뷰에서 카메라 이동 시 배경 텍셀 격자 떨림 억제, 에디터/클라이언트 해상도 일치 | 단계 4 (9.2절 P1) |
| **SE-11** | **P1** | `ShaderGraph` Shadow WPO 자동 동기화 & 물질 프리셋 실시간 튜닝 워크플로우 | `SE-03`, `SE-04` | Engine (`ShaderGraph`), Editor (`PIE`) | 그래프 WPO 변경 시 그림자 일치, PIE 중 튜닝한 슬라임 물질 프리셋 파일 저장·복원 지원 | 단계 5 (9.2절 P1) |
| **SE-12** | **P2** | `RenderPipelineProfile` 커스텀 패스 확장(SDF Raymarching/Outline) 및 다중 접촉 Manifold | `SE-02`, `SE-05`, `SE-09` | Engine (`Render/Pipeline`, `Physics`) | SDF 연결 표면의 깊이·그림자 통합 또는 100개체 인스턴싱 최적화 (병목 실측 후 착수) | `M9`, `M10` (9.2절 P2) |

---

## 6. 위험 요소, 미확정 기술 결정 및 검증 계획

### 6.1 주요 기술 위험 및 미확정 결정

| 항목 | 위험 내용 | 결정 / 대응 방향 |
| --- | --- | --- |
| **권위 이동 솔버 선택** | `RigidBodyComponent` 기반 적분과 `Collision::Sweep` 기반 커스텀 솔버가 동일 엔티티 위치를 동시에 제어할 위험 | **M0에서 단일 권위 솔버 확정** (자유 반발·충돌이 핵심이면 `RigidBody`+CCD, 정밀 벽면 부착/모서리 전환이 핵심이면 커스텀 Swept Controller 채택). |
| **분열·합체 표면 기술** | SDF Raymarching은 `RenderPipelineProfile` 확장 및 깊이·그림자 통합 비용이 큼 | P0에서는 **Procedural Bridge Mesh / CPU Marching Cubes + `SE-03`(in-place 정점 갱신)**으로 먼저 검증하고, 품질 한계 확인 시 `SE-12`로 SDF 전환. |
| **저해상도에서 미세 효과 소실** | 480×270에서 얇은 점액질 가닥, 눈, 작은 액적이 1픽셀 미만으로 사라질 수 있음 | Native 해상도와 480×270/640×360을 동일 카메라 구도로 비교하고 최소 가시 두께/액적 크기 보정 정책 수립. |
| **비-DX12 RHI 백엔드 호환** | `DirectX12RHI` 외 `SDLGPURHI` 등에서 동적 정점 버퍼/인스턴싱/커스텀 패스 동작이 다를 수 있음 | 1차 검증은 Windows DX12 기준으로 수행하고 미지원 백엔드에는 capability 폴백 명시. |

### 6.2 계층별 검증 계획

1. **정적 확인 (빌드 전)**
   - `Projects/GameModule/CMakeLists.txt`의 `GAME_REFLECTION_HEADERS`에 신규 슬라임 컴포넌트 헤더 등록 및 리플렉션 코드 생성 확인.
   - 신규 로캘 키 추가 시 `Engine/Resources/Data/Localization/*.csv` 원본 수정 원칙 준수 확인.
   - `Shader/BINDING_LAYOUT.md`와 `lightingPS.hlsl`/`DecalDrawData` GPU 상수 버퍼 정렬(16바이트 배수) 일치 확인.

2. **헤드리스 단위 테스트 (CTest)**
   - `InputFixedStepCheck` (신규): 가변 렌더 프레임(`0회`, `1회`, `3회` FixedUpdate)에서 `Pressed/Released` 1회 소비 및 무유실 검증.
   - `PhysicsCollisionCallbackCheck` (신규): 비트리거 충돌 `OnEnter/Stay/Exit` 발생, 법선·상대속도 정확성, 고속 발사 CCD 터널링 방지 검증.
   - `MeshDynamicBufferCheck` (신규): `UpdateStreamBufferInPlace` 후 `m_GeometryVersion` 증가, 동적 `m_BoundingBoxMin/Max` 갱신, 버퍼 재할당 없음 검증.
   - `PixelPerfectCameraCheck` (기존 확장): 회전된 3D 직교 카메라의 Right/Up 축 스냅 및 서브픽셀 오차 범위 검증.
   - `RenderPipelineProfileCheck` (기존 확장): 저해상도 기준 해상도·정수배 업스케일 정책 유효성 검증.

3. **Playground 실환경 검증 (`Slime Movement Playground`)**
   - **30 / 60 / 144 FPS 고정 틱 재현성**: 동일 입력 시퀀스로 Hopping, Elastic Jump, Slingshot 궤적 및 착지 위치 비교.
   - **물리·시각 정합**: PIE `Single-Step`과 `DebugDraw`를 켜고 충돌 프록시, 접촉 법선, 그림자 실루엣(`ShadowDepthPass`), AABB 컬링 경계 일치 확인.
   - **저해상도 & 스타일 비교**: Native vs 640×360 vs 480×270 전환 시 UI 선명도, 카메라 이동 중 픽셀 떨림, 눈/가닥 가독성 비교.
   - **성능 계측**: `MetricsPanel`로 1 / 10 / 50 / 100개체 기준 CPU(물리·메시 변형) 및 GPU(G-Buffer·Toon/SSS·VFX·Decal) 패스 시간 측정.
