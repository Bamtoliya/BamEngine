# 모듈형 파티클 & 비주얼 이펙트 시스템 및 에디터 개발 계획 (Modular Particle & VFX System and Editor Plan)

- **작성일:** 2026-11-07
- **상태:** Phase 1, Phase 2 구현 완료 / Phase 3 대기
- **공통 기준:** [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md), [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md)
- **연계 계획:** [EngineDevelopmentPlan.md](EngineDevelopmentPlan.md) 4절(나이아가라형 파티클 시스템), [AnimationTimelineEditorPlan.md](AnimationTimelineEditorPlan.md)(비-PIE 프리뷰 루프 및 타임라인 FX), [DevelopmentPriorities.md](DevelopmentPriorities.md)

---

## 1. 목적과 필요성

1. **물리 및 환경과의 단절 해소:** 기존 파티클 시스템은 공간을 인식하지 못하여 지면이나 벽을 뚫고 지나가며, 바람이나 충격파 등 월드 환경과 상호작용하지 못합니다.
2. **단일 하드코딩 구조의 한계:** 모든 속성이 `ParticleEmitterComponent` 단일 구조체에 집중되어 있어, 복합 이펙트(발사 $\rightarrow$ 충돌 $\rightarrow$ 파편 $\rightarrow$ 연기 $\rightarrow$ 그을음)를 연쇄시키거나 새로운 물리 법칙(컬 노이즈 등)을 모듈식으로 추가하기 어렵습니다.
3. **전용 시각화 저작 도구 부재:** 인스펙터의 수치 입력에만 의존하고 있어, 타임라인을 마우스로 앞뒤로 긁어가며(Scrubbing) 충돌 순간이나 분사 타이밍을 프레임 단위로 미세 조정할 수 있는 독립 에디터가 필요합니다.

본 계획의 목표는 상용 엔진(Unreal Niagara, Unity VFX Graph)의 핵심 아키텍처를 벤치마크하여, **물리 충돌 및 환경 기류 상호작용이 가능한 모듈형 파티클 런타임**과 **독립 프리뷰 뷰포트 및 타임라인 스크러빙을 지원하는 전용 이펙트 에디터(`ParticleEditorPanel`)**를 구축하는 것입니다.

---

## 2. 핵심 아키텍처 및 시스템 흐름

```mermaid
flowchart TD
    subgraph Editor ["에디터 계층 (ParticleEditorPanel)"]
        UI["ParticleEditorPanel"]
        Timeline["타임라인 & 재생 제어 (Play, Pause, Step, Scrubbing)"]
        PreviewScene["독립 PreviewScene (그리드, 암실 조명, 궤도 카메라)"]
        StackUI["모듈 스택 & 커브/그래디언트 편집기"]
    end

    subgraph AssetLayer ["데이터 및 에셋 계층"]
        EffectAsset["ParticleEffect 에셋 (.effect / JSON)"]
        EmitterData["다중 Emitter 설정 (스폰/업데이트/충돌/렌더 모듈)"]
        EffectAsset --> EmitterData
    end

    subgraph SimulationCore ["시뮬레이션 엔진 (ParticleSystem)"]
        SpawnStage["Spawn Stage: 수명/초기속도/원뿔/메시소켓"]
        ForceStage["Force Stage: 중력 + 부력 + 공기저항 + Curl Noise"]
        EnvStage["Environment Stage: 전역 바람 + 방사형 충격파 수신"]
        CollisionStage["Collision Stage: 바닥 평면 / Physics Raycast 반사"]
        EventStage["Event Stage: OnCollision / OnDeath 서브 스폰"]
    end

    subgraph RenderPipeline ["렌더링 엔진 (Renderer)"]
        Billboard["빌보드 스프라이트 렌더러"]
        Ribbon["트레일 / 리본 렌더러"]
        MeshInst["메시 파티클 렌더러"]
        SoftDepth["Soft Particle (Depth Fade)"]
    end

    UI --> EffectAsset
    PreviewScene --> SimulationCore
    EmitterData --> SimulationCore
    SimulationCore --> SpawnStage --> ForceStage --> EnvStage --> CollisionStage --> EventStage
    CollisionStage -.->|"충돌 검사"| PhysicsRaycast["PhysicsSystem::Raycast"]
    EventStage -.->|"데칼 생성"| DecalSystem["DecalManager / RenderTarget"]
    SimulationCore --> RenderPipeline
    RenderPipeline --> Billboard
    RenderPipeline --> Ribbon
    RenderPipeline --> MeshInst
    RenderPipeline --> SoftDepth
```

---

## 3. 단계별 상세 개발 로드맵 (Phase 1 ~ Phase 5)

### Phase 1: 물리 충돌 & 반발(Bounce) 및 바닥 상호작용 (구현 완료)
- **목표:** 입자가 지면 및 물리 콜라이더에 부딪혀 튀거나(바운스), 미끄러지거나(슬라이딩), 접촉 시 소멸하는 충돌 역학 완성.
- **주요 작업:**
  1. `EParticleCollisionMode` 열거형 정의: `None`, `FloorPlane`, `PhysicsRaycast`.
  2. `EParticleCollisionResponse` 열거형 정의: `Bounce`, `Slide`, `DieOnContact`, `Stop`.
  3. `ParticleEmitterComponent` 프로퍼티 확장: 반발 계수(`restitution`), 마찰 계수(`friction`), 바닥 높이(`floorHeight`), 프레임당 최대 검사 수(`maxCollisionChecksPerFrame`), 충돌 반경(`collisionRadius`), 최소 바운스 속도 임계치(`minBounceSpeed`).
  4. `ParticleSystem::UpdateEmitter` 적분 루프에 충돌 감지 및 반사 벡터 계산 적용:
     $$\vec{v}' = \vec{v} - (1 + e)(\vec{v} \cdot \vec{N})\vec{N}$$
  5. `FloorPlane` 초경량 고속 충돌 및 `PhysicsSystem::Raycast` 정밀 충돌 연동.
- **완료 기준:** 바닥에 튄 불꽃/파편 입자가 탄성 계수에 따라 서서히 감쇠하며 바닥에 안착하거나 슬라이딩하는 동작 검증 완료.

### Phase 2: 공간 연기 확산 역학 (부력 & 컬 노이즈 & 환경 외력) (구현 완료)
- **목표:** 연기가 위로 피어오르고(부력), 공기 저항으로 뭉게뭉게 퍼지며(드래그), 3D 노이즈를 따라 소용돌이치고(컬 노이즈), 월드 바람에 휘날리는 자연스러운 확산 구현.
- **주요 작업:**
  1. 부력(Buoyancy, $\vec{a}_{\text{buoyancy}} = (0, b, 0)$) 및 공기 저항 감쇠.
  2. 3D 무발산 컬 노이즈(Curl Noise / Turbulence, $\nabla \times \vec{\Psi}$) 벡터장 계산 및 파티클 속도 합성.
  3. 전역 바람(Global Wind, $\vec{w}$)에 의한 상대 공기 저항 가속도 연동.
  4. 시간에 따른 노이즈 장 스크롤 속도(`curlNoiseScrollSpeed`) 지원.
- **완료 기준:** 바닥에서 발생한 연기가 장애물과 기류를 타고 퍼져나가며, 위로 떠오르다 자연스럽게 흩어지는 비주얼 완성.

### Phase 3: 이벤트 체이닝 & 표면 상호작용 (Events & Decals)
- **목표:** 한 입자의 충돌/사망 사건이 다른 서브 이미터나 지면 데칼로 이어지는 연쇄 효과 구현.
- **주요 작업:**
  1. 파티클 생명주기 이벤트(`OnSpawn`, `OnCollision`, `OnDeath`) 페이로드 정의.
  2. 서브 이미터 스폰(Event Spawner): 충돌 시 파편 버스트, 미사일 궤적 연기 생성.
  3. 표면 데칼(그을음/피/젖음) 투영 및 물 파문(Water Splash) 연동.
- **완료 기준:** 하나의 이펙트 실행으로 발사 $\rightarrow$ 충돌 $\rightarrow$ 파편 튐 $\rightarrow$ 바닥 자국의 풀 체인이 자동으로 연쇄 동작.

### Phase 4: 모듈형 아키텍처 및 독립 에셋화 (`ParticleEffect` Asset)
- **목표:** 단일 컴포넌트 구조에서 나이아가라 스타일의 모듈식 에셋 구조로 아키텍처 전환.
- **주요 작업:**
  1. 데이터 구조 분리: `ParticleSpawnModule`, `ParticleUpdateModule`, `ParticleCollisionModule`, `ParticleRendererModule`.
  2. 다중 이미터를 포함하는 `.effect` 독립 에셋 정의 및 JSON 직렬화/역직렬화(Archive) 완비.
- **완료 기준:** 여러 프리팹/엔티티에서 동일한 `.effect` 에셋을 참조하여 일관된 효과를 재생.

### Phase 5: 독립 프리뷰 및 전용 이펙트 에디터 (`ParticleEditorPanel`)
- **목표:** 메인 씬의 방해 없이 이펙트를 실시간으로 편집하고 프레임 단위로 미세 조정하는 독립 저작 도구 구축.
- **주요 작업:**
  1. 격리된 프리뷰 뷰포트(`PreviewScene`): 메인 씬 오염 없는 독립 오프스크린 씬 (그리드, 암실 라이팅, 마우스 궤도 카메라, Non-PIE 실시간 루프).
  2. 타임라인 및 재생 제어기: Play, Pause, Step, 배속 조절, 고정 시드 기반 결정론적 스크러빙(Scrubbing).
  3. 모듈 스택 및 ImGui 커브/그래디언트 편집 UI.
  4. 성능 프로파일러: 활성 입자 수, 드로우 콜, CPU 시뮬레이션 소요 시간(ms) 실시간 모니터링.
- **완료 기준:** 에디터 패널 안에서 이펙트 제작, 실시간 타임라인 검증, 저장 및 씬 배치까지 원스톱 워크플로우 완성.

---

## 4. 로캘 및 단축키 관리 기준

- **로캘 키:** 신규 UI 문구 및 파라미터는 번역 JSON을 직접 수정하지 않고, **`Resources/Data/Localization/Editor.csv` 및 `Engine.csv`에 원본 키를 등록**합니다.
- **단축키 중앙 집중 관리:** 에디터 재생 제어 단축키(Space = Play/Pause, `[` / `]` = Step Frame, R = Restart)는 **`ShortcutManager`(`EditorActionRegistry`)에 액션으로 등록**합니다.

---

## 5. 검증 기준 및 확인 절차

1. **물리 충돌 검증:** 경사면 및 바닥 콜라이더에 파티클을 쏘았을 때 법선 반사가 정확하며, 반발 계수($e=0.5$) 시 튀는 높이가 지수적으로 감소하는지 확인.
2. **연기 컬 노이즈 검증:** 파티클들이 직선으로 뻗지 않고 유기적인 소용돌이를 형성하며, 바닥/벽면 충돌 시 뚫고 나가지 않고 모퉁이를 따라 미끄러지는지 확인.
3. **결정론적 스크러빙 검증:** 시드를 고정했을 때 타임라인을 $t=1.0\text{s}$ 위치로 앞뒤로 스크러빙해도 매번 동일한 위치/형태가 나타나는지 확인.
4. **저장 및 복원 검증:** `.effect` 에셋 저장 후 씬을 재열거나 에디터를 재시작했을 때 모든 모듈과 커브 데이터가 무손실 복원되는지 확인.
