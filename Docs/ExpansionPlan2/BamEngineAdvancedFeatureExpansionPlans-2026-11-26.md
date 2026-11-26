# BamEngine 추가 기능 확장 계획 — 2차 색인

- 작성일: 2026-11-26
- 대상: 앞선 41개 계획 이후 추가 제안한 기술 전부. 중복 언급된 Animation Compression은 하나로 정리하고 Camera Graph 및 8개 하위 시스템은 독립 계획으로 분리했다.
- 범위: **96개 신규 Markdown 계획서** / 총 71개 외부 자료 진입점(중복 제외).
- 기준: [BamEngine main `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 상태: 조사·설계 제안. 구현/실행/성능 검증 미수행. 기존 계획을 대체하거나 우선순위를 강제 변경하지 않는다.
- 이번 ZIP의 권장 위치: `BamEngine/Docs/AdvancedFeaturePlans/`. 이전 계획은 `Docs/FeaturePlans/`와 `Docs/StylizedRenderingAndSpriteBakingPlan.md`에 별도 유지한다.

## 1. 평가 및 문서 읽는 방법

P1은 좁게 완성해 재사용할 수 있는 기반·생산성 작업, P2는 실제 사용 예제 중심, P3은 측정·콘텐츠 요구 기반 확장, P4는 조건부 연구를 뜻한다. 모든 점수나 순서는 제안이다. 기술이 최신 연구라는 이유만으로 실제 프로젝트에 효과적이라고 단정하지 않는다.

각 문서에는 조사 근거, 재사용 후보, 책임 경계·알고리즘, 데이터/실행 계약, 작업 ID·완료 기준, **추가 조사 필요** 항목, 외부 자료 및 검증 기록표가 있다. 일부 분야는 인접 기술 문서만 확인한 상태이므로 상세 원문, 버전, 수치 및 실제 소스 확인이 선행되어야 한다.

## 2. 분류별 계획

### 그래픽스·렌더링 (16개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-01 | [Radiance Cascades — 2D 실시간 간접광](#af-01) | P3 |
| AF-02 | [2D Dynamic Lighting — 스프라이트 조명](#af-02) | P2 |
| AF-03 | [3D Gaussian Splatting — 재구성 장면 렌더러](#af-03) | P4 |
| AF-04 | [Layered Material / Advanced BSDF](#af-04) | P3 |
| AF-05 | [Adaptive Probe Volume — 적응형 베이크 조명](#af-05) | P3 |
| AF-06 | [Screen Space Reflection — 비 DXR 반사](#af-06) | P2 |
| AF-07 | [Volume Decal — 영역 기반 표면 변화](#af-07) | P2 |
| AF-08 | [Volumetric Local Fog — 국소 안개](#af-08) | P2 |
| AF-09 | [Portal Rendering — 공간 포털](#af-09) | P3 |
| AF-10 | [Planar Reflection — 평면 반사](#af-10) | P2 |
| AF-11 | [Hair Rendering — 헤어 카드와 Strand](#af-11) | P3 |
| AF-12 | [Eye Rendering — 각막·홍채](#af-12) | P2 |
| AF-13 | [Parallax Occlusion Mapping](#af-13) | P2 |
| AF-14 | [Variable Rate Shading — 셰이딩 밀도 제어](#af-14) | P3 |
| AF-15 | [Neural Texture Compression — 연구](#af-15) | P4 |
| AF-16 | [Neural Materials — 복합 재질 근사 연구](#af-16) | P4 |

### 애니메이션·물리 (14개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-17 | [Motion Warping — Root Motion 목표 정렬](#af-17) | P2 |
| AF-18 | [Pose Warping — 방향·보폭·경사 보정](#af-18) | P2 |
| AF-19 | [Distance Matching — 거리 기반 재생](#af-19) | P2 |
| AF-20 | [Spring Bone — 보조 관절 물리](#af-20) | P2 |
| AF-21 | [Inertialization — 포즈 전환 감쇠](#af-21) | P2 |
| AF-22 | [Animation Compression — 클립 압축](#af-22) | P2 |
| AF-23 | [Morph Target / Blendshape — 표정·체형](#af-23) | P2 |
| AF-24 | [Control Rig — 엔진 내 리깅 저작](#af-24) | P3 |
| AF-25 | [Active Ragdoll / Physical Animation](#af-25) | P3 |
| AF-26 | [XPBD Cloth — 의상 및 천](#af-26) | P3 |
| AF-27 | [Soft Body Simulation — 탄성 물체](#af-27) | P4 |
| AF-28 | [Rope / Cable Physics](#af-28) | P2 |
| AF-29 | [Vehicle Physics — 이동 수단](#af-29) | P3 |
| AF-30 | [Physics Material — 표면 물성](#af-30) | P1 |

### 카메라 (9개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-31 | [Gameplay Camera Graph — 카메라 리그](#af-31) | P2 |
| AF-32 | [Camera Impulse — 충격 기반 흔들림](#af-32) | P1 |
| AF-33 | [Camera Volume — 지역 카메라 설정](#af-33) | P2 |
| AF-34 | [Target Group Framing — 다중 대상 구도](#af-34) | P2 |
| AF-35 | [Camera Confiner — 이동 범위 제한](#af-35) | P2 |
| AF-36 | [Camera Collision — 가림·벽 관통 방지](#af-36) | P1 |
| AF-37 | [Camera Blend — 리그 전환](#af-37) | P1 |
| AF-38 | [Lens Distortion — 광각 및 렌즈 왜곡](#af-38) | P2 |
| AF-39 | [Photo Mode — 자유 시점 촬영](#af-39) | P2 |

### 모델링·콘텐츠 제작 (10개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-40 | [CSG / Boolean Modeling — 레벨 블록아웃](#af-40) | P2 |
| AF-41 | [Voxel Terrain — 체적 지형](#af-41) | P3 |
| AF-42 | [Vertex Painting — 정점 속성 브러시](#af-42) | P2 |
| AF-43 | [Texture Painting — UV 텍스처 저작](#af-43) | P3 |
| AF-44 | [Mesh Editing — 정점·변·면 편집](#af-44) | P3 |
| AF-45 | [Automatic UV Unwrapping](#af-45) | P3 |
| AF-46 | [Modular Building Tool — 건물·던전 조립](#af-46) | P2 |
| AF-47 | [Socket Editor — 부착점 저작](#af-47) | P1 |
| AF-48 | [Texture Array Builder — 동질 텍스처 집합](#af-48) | P2 |
| AF-49 | [Sprite Extrusion — 2D 이미지의 입체화](#af-49) | P2 |

### 오디오 (10개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-50 | [Audio Event Asset — 데이터 기반 사운드](#af-50) | P1 |
| AF-51 | [Audio Occlusion — 공간 차폐](#af-51) | P1 |
| AF-52 | [Acoustic Simulation — 공간 반사·잔향](#af-52) | P3 |
| AF-53 | [Adaptive Music — 상태 기반 BGM](#af-53) | P2 |
| AF-54 | [Footstep System — 표면별 발소리](#af-54) | P1 |
| AF-55 | [Audio Concurrency — 동시 발음 제어](#af-55) | P1 |
| AF-56 | [Audio Graph — 노드 기반 사운드 제어](#af-56) | P3 |
| AF-57 | [Reverb Volume — 지역별 잔향](#af-57) | P2 |
| AF-58 | [Audio Visualization — 파형·FFT](#af-58) | P2 |
| AF-59 | [Lip Sync — 음성 기반 입모양](#af-59) | P2 |

### 게임플레이·AI (14개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-60 | [Interaction System — 상호작용](#af-60) | P1 |
| AF-61 | [Inventory Framework — 아이템 보관](#af-61) | P2 |
| AF-62 | [Equipment Framework — 장비 장착](#af-62) | P2 |
| AF-63 | [Quest Graph — 퀘스트 상태](#af-63) | P2 |
| AF-64 | [Dialogue Graph — 분기형 대화](#af-64) | P2 |
| AF-65 | [Event Bus — 타입 안전한 이벤트 전달](#af-65) | P1 |
| AF-66 | [Object Pool — 엔티티/VFX 재사용](#af-66) | P1 |
| AF-67 | [Gameplay Cue — 시각·청각 이벤트](#af-67) | P2 |
| AF-68 | [Hitbox / Hurtbox — 공격 및 피격 판정](#af-68) | P1 |
| AF-69 | [Utility AI — 점수 기반 행동 선택](#af-69) | P2 |
| AF-70 | [GOAP — 목표 지향 행동 계획](#af-70) | P3 |
| AF-71 | [Crowd Simulation — 군중 이동 및 회피](#af-71) | P3 |
| AF-72 | [Game Mode Framework — 게임 규칙 및 진행](#af-72) | P2 |
| AF-73 | [Modular Game Features — 선택 기능 플러그인](#af-73) | P2 |

### UI·입력 (10개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-74 | [UI Navigation Stack — 메뉴 흐름](#af-74) | P1 |
| AF-75 | [Input Mapping Context — 상황별 입력](#af-75) | P1 |
| AF-76 | [Input Rebinding — 키·패드 변경](#af-76) | P1 |
| AF-77 | [Virtualized List — 대량 UI 항목](#af-77) | P1 |
| AF-78 | [UI Theme / Style Sheet — 공통 디자인](#af-78) | P2 |
| AF-79 | [Rich Text — 서식 및 인라인 요소](#af-79) | P2 |
| AF-80 | [MSDF Text Rendering — 확대 가능한 글꼴](#af-80) | P2 |
| AF-81 | [Text Shaping — 다국어 글자 조합](#af-81) | P2 |
| AF-82 | [Responsive Layout — 화면 비율 대응](#af-82) | P2 |
| AF-83 | [Accessibility — 접근성 기반](#af-83) | P2 |

### 엔진 인프라·최적화 (13개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-84 | [Task Graph / Job System — 병렬 작업](#af-84) | P2 |
| AF-85 | [PSO Precaching — 사전 파이프라인 생성](#af-85) | P1 |
| AF-86 | [Persistent Pipeline Cache — 디스크 PSO 캐시](#af-86) | P2 |
| AF-87 | [GPU Skin Cache — 스키닝 결과 공유](#af-87) | P2 |
| AF-88 | [Visibility Buffer — 지오메트리 정보 재구성](#af-88) | P4 |
| AF-89 | [Async Physics — 물리 작업 병렬화](#af-89) | P3 |
| AF-90 | [GPU Resource Residency — VRAM 예산 관리](#af-90) | P3 |
| AF-91 | [DirectStorage — 대량 에셋 I/O](#af-91) | P3 |
| AF-92 | [Asset Dependency Graph — 에셋 의존성](#af-92) | P1 |
| AF-93 | [Incremental Cooking — 변경분 빌드](#af-93) | P1 |
| AF-94 | [Remote Profiling — 외부 Client 계측](#af-94) | P2 |
| AF-95 | [Hot Patch / DLC — 콘텐츠 추가 배포](#af-95) | P3 |
| AF-96 | [Rollback Netcode — 입력 예측·상태 복원](#af-96) | P3 |

## 3. 추천 개발 묶음 및 의존 관계

아래 묶음은 강제 선행 조건이 아니다. 기존 계획 중 준비된 기반을 활용하며 실전 게임/콘텐츠의 요구와 측정을 통해 범위를 선택한다.

| 묶음 | 추천 순서 | 검증용 콘텐츠 |
| --- | --- | --- |
| 제작 사용성 | InputMappingContext → InputRebinding → UINavigationStack → UITheme / VirtualizedList | 설정 메뉴, 인벤토리 |
| 액션 게임 | EventBus → PhysicsMaterial / ObjectPool → HitboxHurtbox → GameplayCue → MotionWarping | 3인칭 공격·대시·피격 |
| 픽셀 아트 | 이전 Sprite Baker → TwoDDynamicLighting → RadianceCascades | 2D 동굴의 발광체와 가림 |
| 애니메이션·아바타 | MorphTarget → SpringBone → SocketEditor → ControlRig → LipSync | 표정·장신구·추적 기반 |
| 카메라 | 기존 Spring Arm 회귀 → CameraCollision / CameraBlend → GameplayCameraGraph → Impulse·Framing·PhotoMode | 3인칭·보스·촬영 |
| 제작 도구 | CSGModeling → MeshEditing → UV / Painting / ModularBuildingTool | 실내 레벨·모듈러 건물 |
| 성능 및 배포 | PSOPrecaching → PersistentPipelineCache → AssetDependencyGraph → IncrementalCooking → RemoteProfiling | cold startup·변경 에셋 내보내기 |
| 오디오 | AudioEventAsset / AudioConcurrency → FootstepSystem / AudioOcclusion → AdaptiveMusic / ReverbVolume → AcousticSimulation | 실내/실외 전투와 음악 |
| 대규모 게임 | TaskJobSystem → GPUSkinCache / CrowdSimulation → GPUResourceResidency / DirectStorage | 군중·대규모 씬 |
| 연구 | VisibilityBuffer, GaussianSplatting, NeuralTextureCompression, NeuralMaterials, SoftBody | 독립 prototype 및 기존 경로 비교 |

## 4. 통합 관리 원칙

- 기존 계획 문서와 신규 계획이 중복되면 기존 작업 ID를 먼저 연결한다. 이미 구현된 경우 신규 구현이 아니라 회귀 테스트·저작 UX·실환경 검증으로 전환한다.
- 공통 프레임워크의 도입이 다른 작업의 필수 선행 조건이 되지 않게 한다. Engine/Editor/Client/GameModule의 책임, DLL ABI, 리소스 및 저장 계약을 유지한다.
- 실행 검증은 기능마다 독립적이다. 자동 검사 통과가 모든 GPU·UI·오디오 품질과 Shipping 검증을 의미하지 않는다.
- 장기 연구는 실제 하드웨어 지원, SDK/외부 콘텐츠 라이선스와 성능 기준을 확인한 후 진행한다.
- 원래 개발 목표별 실제 결과물(2D/3D 게임, 강화학습, 2D/3D 아바타 및 제작 도구)을 우선한다. 계획서의 수가 구현 완료율을 의미하지 않는다.

## 5. 자료 조사 범위 및 한계

이번 작업은 공개 제품 문서, 논문 초록/서지, SDK 저장소 소개와 기존 BamEngine 코드·계획 검토를 바탕으로 작성했다. 각 기술의 전용 원문 및 구현체 전수 검토, SDK 빌드, 벤치마크, 라이선스 법률 검토는 하지 않았다. 인접 자료만 찾은 일부 기능은 추가 조사 필요 항목으로 남겼다.


---

<a id="af-01"></a>

## AF-01 — Radiance Cascades — 2D 실시간 간접광 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-01` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [AF-02 2D Dynamic Lighting — 스프라이트 조명](#af-02) · [StylizedRenderingAndSpriteBakingPlan](../StylizedRenderingAndSpriteBakingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 2D GI의 radiance cascade 계열 연구 / 2D 광원·occluder·normal map 및 pixel-art 조명.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** SpriteRenderSystem, PixelPerfectCameraComponent, Compute RHI.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

2D 발광·차폐 필드에서 거리 구간별 ray interval을 계층적으로 샘플링하고 원거리 cascade를 근거리로 병합한다. 화면 해상도와 독립된 probe grid 및 각도 수를 사용하며 emissive radiance와 transmittance를 함께 저장한다. SDF sphere tracing은 선택적 가속이고 3D GI와 별도 경로다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | CPU brute-force 기준 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | 2D occupancy/emission RT | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | compute cascade merge와 bilinear resolve | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | 픽셀 아트 프로필·디버그 뷰 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-01-01** CPU brute-force 기준.
- [ ] **AF-01-02** 2D occupancy/emission RT.
- [ ] **AF-01-03** compute cascade merge와 bilinear resolve.
- [ ] **AF-01-04** 픽셀 아트 프로필·디버그 뷰.
- [ ] **AF-01-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-01-V01** 닫힌 벽 뒤 누광: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-01-V02** 에너지 감소: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-01-V03** 이동 카메라 경계: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-01-V04** 320×180/640×360 GPU ms 및 메모리 비교: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-01-R01** 논문의 interval/merge 정확한 수식: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-01-R02** cascade interpolation artefact: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-01-R03** HRC 변형: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-01-R04** SDF thin wall 처리: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-01-R05** HDR 합성: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Holographic Radiance Cascades for 2D Global Illumination](https://arxiv.org/abs/2505.02041) — 2D GI의 radiance cascade 계열 연구.
2. [Godot — 2D lights and shadows](https://docs.godotengine.org/en/stable/tutorials/2d/2d_lights_and_shadows.html) — 2D 광원·occluder·normal map 및 pixel-art 조명.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-02"></a>

## AF-02 — 2D Dynamic Lighting — 스프라이트 조명 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-02` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [StylizedRenderingAndSpriteBakingPlan](../StylizedRenderingAndSpriteBakingPlan.md) · [AF-01 Radiance Cascades — 2D 실시간 간접광](#af-01)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 2D 광원·occluder·normal map 및 pixel-art 조명.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** SpriteAtlas, SpriteRenderSystem, PixelPerfectCameraComponent, Tilemap.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

2D world XY에서 point/directional light, 높이 성분을 포함한 L 벡터와 sprite tangent normal을 평가한다. occluder polygon 또는 tile contour로 shadow mask를 만들고 알베도·노멀·emission atlas UV를 동일하게 유지한다. UI는 기본 unlit; pixel grid에서 빛도 스냅한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | normal atlas 및 단일 광원 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | polygon shadow | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | tiled light list와 layer mask | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | Sprite Baker normal 연계 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-02-01** normal atlas 및 단일 광원.
- [ ] **AF-02-02** polygon shadow.
- [ ] **AF-02-03** tiled light list와 layer mask.
- [ ] **AF-02-04** Sprite Baker normal 연계.
- [ ] **AF-02-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-02-V01** flip/rotation normal 정합: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-02-V02** alpha cutout: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-02-V03** tile 경계: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-02-V04** 비정수 확대: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-02-V05** 광원 1/32/128개 성능: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-02-R01** normal convention: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-02-R02** straight/premultiplied alpha: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-02-R03** shadow extrusion 방식: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-02-R04** lit/unlit 합성 및 layer sorting: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Godot — 2D lights and shadows](https://docs.godotengine.org/en/stable/tutorials/2d/2d_lights_and_shadows.html) — 2D 광원·occluder·normal map 및 pixel-art 조명.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-03"></a>

## AF-03 — 3D Gaussian Splatting — 재구성 장면 렌더러 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-03` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P4** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [GPUDrivenRenderingPlan](../FeaturePlans/GPUDrivenRenderingPlan.md) · [FrameGraphPlan](../FeaturePlans/FrameGraphPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: anisotropic Gaussian과 가시성 기반 splat rendering.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** DX12 Compute, ResourceStreamer, RenderPipelineProfile.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

학습된 Gaussian의 center, rotation, scale, opacity, SH를 로드하고 covariance를 화면에 투영한다. tile binning·깊이 정렬 후 front-to-back alpha compositing. 일반 opaque depth를 활용하되 정확한 mesh collision 및 relighting은 별도 proxy 없이는 보장하지 않는다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 정적 PLY importer/검증 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | CPU 정렬 기준 renderer | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | compute tile binning·sort | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | LOD/streaming 실험 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: P4 연구용 prototype. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-03-01** 정적 PLY importer/검증.
- [ ] **AF-03-02** CPU 정렬 기준 renderer.
- [ ] **AF-03-03** compute tile binning·sort.
- [ ] **AF-03-04** LOD/streaming 실험.
- [ ] **AF-03-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-03-V01** 기준 이미지 PSNR/SSIM: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-03-V02** near-plane·NaN·alpha: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-03-V03** 메모리·sort ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-03-V04** opaque occlusion 비교: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-03-R01** PLY 변종 및 SH convention: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-03-R02** sorting 정확성/VRAM: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-03-R03** 공개 데이터와 모델 라이선스: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-03-R04** 4D splat 제외: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-03-R05** training 도구 선택: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Kerbl et al. — 3D Gaussian Splatting](https://doi.org/10.1145/3592433) — anisotropic Gaussian과 가시성 기반 splat rendering.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-04"></a>

## AF-04 — Layered Material / Advanced BSDF 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-04` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [StylizedRenderingAndSpriteBakingPlan](../StylizedRenderingAndSpriteBakingPlan.md) · [AF-11 Hair Rendering — 헤어 카드와 Strand](#af-11) · [AF-12 Eye Rendering — 각막·홍채](#af-12)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 모듈형 재질 모델의 비교 사례 / mesh attributes, morph target과 skinning의 데이터 계약.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** MaterialInterface, ShaderGraphCompiler, DeferredSceneRenderer.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

기존 PBR을 기본 lobe로 유지하고 clear coat, sheen, anisotropy, subsurface, transmission을 한 번에 하나씩 추가한다. BSDF mix와 layer는 다른 연산이며 Fresnel·에너지 보존·normal basis를 검증한다. G-buffer에 모두 넣지 말고 재질 ID/parameter buffer 또는 특수 forward path를 비교한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | clear coat reference | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | tangent anisotropy/sheeen | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | graph lobe node 및 pass 계약 | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | SSS/transmission 별도 평가 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-04-01** clear coat reference.
- [ ] **AF-04-02** tangent anisotropy/sheeen.
- [ ] **AF-04-03** graph lobe node 및 pass 계약.
- [ ] **AF-04-04** SSS/transmission 별도 평가.
- [ ] **AF-04-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-04-V01** white furnace: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-04-V02** roughness extreme: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-04-V03** normal map: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-04-V04** shader variant 수·G-buffer bandwidth: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-04-V05** PBR 회귀: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-04-R01** BRDF 원문: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-04-R02** energy compensation: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-04-R03** glTF KHR_materials 확장: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-04-R04** deferred vs forward 및 shader permutation: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Substrate materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-substrate-materials-in-unreal-engine) — 모듈형 재질 모델의 비교 사례.
2. [Khronos — glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) — mesh attributes, morph target과 skinning의 데이터 계약.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-05"></a>

## AF-05 — Adaptive Probe Volume — 적응형 베이크 조명 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-05` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [WorldPartitionPlan](../FeaturePlans/WorldPartitionPlan.md) · [TextureMaterialBakingPlan](../FeaturePlans/TextureMaterialBakingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 기하 밀도에 따른 probe brick 배치.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ProbeGIVolumeComponent, IBLManager, HDR capture.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

geometry density 기반 sparse brick probe 배치 후 정적 씬에서 SH irradiance 및 validity를 베이크한다. world-space probe lookup, brick indirection, trilinear interpolation과 leak rejection을 적용한다. 기존 동적 Probe GI와 별도 베이크 결과 및 blending 정책을 둔다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 균일 SH probe 기준 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | adaptive brick subdivision | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | invalid probe dilation/visibility | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | bake asset·streaming | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-05-01** 균일 SH probe 기준.
- [ ] **AF-05-02** adaptive brick subdivision.
- [ ] **AF-05-03** invalid probe dilation/visibility.
- [ ] **AF-05-04** bake asset·streaming.
- [ ] **AF-05-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-05-V01** 벽 사이 누광: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-05-V02** 동적 객체의 간접광: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-05-V03** bake 재현성: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-05-V04** brick boundary: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-05-V05** VRAM 및 bake time: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-05-R01** probe bake source: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-05-R02** SH order: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-05-R03** visibility/relocation: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-05-R04** day-night scenario blending: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-05-R05** DXR 미지원 bake: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unity — Adaptive Probe Volumes](https://docs.unity.com/en-us/engine/6000.7/manual/lighting-overview/lighting/probevolumes/concept) — 기하 밀도에 따른 probe brick 배치.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-06"></a>

## AF-06 — Screen Space Reflection — 비 DXR 반사 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-06` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [FrameGraphPlan](../FeaturePlans/FrameGraphPlan.md) · [TemporalUpscalingPlan](../FeaturePlans/TemporalUpscalingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: roughness 기반 stochastic SSR 및 필터링.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** DeferredSceneRenderer, TemporalHistory, depth/normal/roughness.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

view-space reflection ray를 depth pyramid에 추적하고 thickness와 screen edge confidence로 hit를 검증한다. roughness에 따라 cone/mip 또는 stochastic ray를 쓰고 temporal reprojection 후 IBL로 miss fallback한다. 화면 밖 반사 한계를 노출한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 단순 ray march | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | Hi-Z traversal | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | roughness·temporal filter | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | DXR/SSR/IBL 선택 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-06-01** 단순 ray march.
- [ ] **AF-06-02** Hi-Z traversal.
- [ ] **AF-06-03** roughness·temporal filter.
- [ ] **AF-06-04** DXR/SSR/IBL 선택.
- [ ] **AF-06-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-06-V01** 거울·roughness ramp·disocclusion·화면 밖 miss·카메라 cut: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-06-V02** pass ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-06-R01** reversed-Z depth 계약: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-06-R02** thickness scale: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-06-R03** history clamp: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-06-R04** SSSR SDK 통합 여부: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [AMD GPUOpen — FidelityFX SSSR](https://gpuopen.com/fidelityfx-sssr/) — roughness 기반 stochastic SSR 및 필터링.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-07"></a>

## AF-07 — Volume Decal — 영역 기반 표면 변화 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-07` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [AF-04 Layered Material / Advanced BSDF](#af-04) · [AF-08 Volumetric Local Fog — 국소 안개](#af-08)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** DecalComponent, DecalCollector, DeferredSceneRenderer.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

기존 box projection decal을 재사용하되 world-space volume mask, 거리 falloff, normal angle, priority, channel mask를 추가한다. 화면 depth에서 surface position을 복원해 albedo/roughness/normal을 변경한다. 진짜 participating media는 fog 경로로 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 기존 decal 회귀 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | sphere/box mask | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | volume material graph | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | overlapping priority와 culling | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-07-01** 기존 decal 회귀.
- [ ] **AF-07-02** sphere/box mask.
- [ ] **AF-07-03** volume material graph.
- [ ] **AF-07-04** overlapping priority와 culling.
- [ ] **AF-07-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-07-V01** 회전/비균일 scale: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-07-V02** 벽 뒤 projection: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-07-V03** 겹침 순서: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-07-V04** skinned/transparent 정책: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-07-R01** 현행 decal pass 위치: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-07-R02** DBuffer 필요성: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-07-R03** RT blending normal 처리: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-07-R04** 물리 volume과 분리: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Rendering Features Reference](https://dev.epicgames.com/documentation/unreal-engine/rendering-features-reference) — 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-08"></a>

## AF-08 — Volumetric Local Fog — 국소 안개 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-08` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [WeatherSystemPlan](../FeaturePlans/WeatherSystemPlan.md) · [GlobalWindFieldPlan](../FeaturePlans/GlobalWindFieldPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 국소 volume과 전역 volumetric fog의 연동 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** fog.hlsli, DeferredSceneRenderer, volumetric cloud 관련 소스.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

sphere/box density와 falloff를 froxel grid에 주입하고 기존 전역 매질과 extinction/scattering을 합성한다. 광원 샘플링 및 Beer-Lambert 적분, history rejection, bounded culling. 낮은 품질은 analytic depth fog로 fallback.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 단일 sphere analytic | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | froxel injection | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | light/shadow 및 temporal | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | editor volume gizmo | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-08-01** 단일 sphere analytic.
- [ ] **AF-08-02** froxel injection.
- [ ] **AF-08-03** light/shadow 및 temporal.
- [ ] **AF-08-04** editor volume gizmo.
- [ ] **AF-08-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-08-V01** 카메라 안팎: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-08-V02** overlap energy: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-08-V03** 역광: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-08-V04** 얇은 fog: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-08-V05** GPU ms/VRAM: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-08-R01** 현재 fog 실제 실행 구조: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-08-R02** froxel depth mapping: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-08-R03** noise/jitter: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-08-R04** temporal lag: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-08-R05** transparent 합성: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Local Fog Volumes](https://dev.epicgames.com/documentation/en-us/unreal-engine/local-fog-volumes-in-unreal-engine) — 국소 volume과 전역 volumetric fog의 연동 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-09"></a>

## AF-09 — Portal Rendering — 공간 포털 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-09` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [FrameGraphPlan](../FeaturePlans/FrameGraphPlan.md) · [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 별도 반사 뷰와 렌더링 비용의 참고 사례(구버전) / 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** CameraComponent, RenderTargetManager, RenderPipelineProfile.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

입구와 출구의 transform으로 virtual camera를 구성하고 exit plane clip 및 stencil 또는 portal surface texture에 렌더한다. recursion limit, visibility screen area, per-view history를 별도 관리한다. teleport gameplay는 분리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 단일 portal RT | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | oblique clip/mesh mask | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | paired portal 및 bounded recursion | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | view budget | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-09-01** 단일 portal RT.
- [ ] **AF-09-02** oblique clip/mesh mask.
- [ ] **AF-09-03** paired portal 및 bounded recursion.
- [ ] **AF-09-04** view budget.
- [ ] **AF-09-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-09-V01** portal edge: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-09-V02** near-plane: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-09-V03** 비대칭 transform: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-09-V04** 가시성: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-09-V05** TAA history isolation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-09-V06** recursive pass 비용: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-09-R01** clip-space/reversed-Z convention: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-09-R02** mirror parity: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-09-R03** stencil 가능 여부: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-09-R04** teleport physics 계약: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Planar Reflections](https://dev.epicgames.com/documentation/en-us/unreal-engine/planar-reflections?application_version=4.27) — 별도 반사 뷰와 렌더링 비용의 참고 사례(구버전).
2. [Unreal — Rendering Features Reference](https://dev.epicgames.com/documentation/unreal-engine/rendering-features-reference) — 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-10"></a>

## AF-10 — Planar Reflection — 평면 반사 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-10` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [WaterInteractionPlan](../FeaturePlans/WaterInteractionPlan.md) · [AF-06 Screen Space Reflection — 비 DXR 반사](#af-06)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 별도 반사 뷰와 렌더링 비용의 참고 사례(구버전).

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** IBLManager, RenderTargetManager, CameraComponent, water shader.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

plane normal/offset으로 camera를 반사하고 winding/culling을 수정하며 plane clip으로 뒤쪽 geometry를 제거한다. 저해상도 reflected view를 roughness에 따라 filter하고 SSR/IBL과 결합한다. 별도 shadow·history budget.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 평면 거울 1개 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | 물 surface | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | screen size cull/update rate | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | reflection fallback | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-10-01** 평면 거울 1개.
- [ ] **AF-10-02** 물 surface.
- [ ] **AF-10-03** screen size cull/update rate.
- [ ] **AF-10-04** reflection fallback.
- [ ] **AF-10-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-10-V01** 비대칭 view: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-10-V02** plane 교차: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-10-V03** winding: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-10-V04** 카메라 이동: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-10-V05** GPU cost 및 recursion 차단: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-10-R01** oblique projection vs clip distance: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-10-R02** planar view pass 지원: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-10-R03** transparency/volumetric composition: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Planar Reflections](https://dev.epicgames.com/documentation/en-us/unreal-engine/planar-reflections?application_version=4.27) — 별도 반사 뷰와 렌더링 비용의 참고 사례(구버전).

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-11"></a>

## AF-11 — Hair Rendering — 헤어 카드와 Strand 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-11` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [AF-20 Spring Bone — 보조 관절 물리](#af-20) · [AF-04 Layered Material / Advanced BSDF](#af-04)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Hair/Eye 등 특수 셰이딩 모델 / 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** MaterialInterface, MaterialGraph, skeletal meshes.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

MVP는 alpha-masked hair cards, tangent 방향 specular lobes, anisotropic roughness, shadow alpha test를 사용한다. strand/groom은 별도 연구로 curve data, expansion, coverage, self-shadow, LOD를 설계한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 카드 재질 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | dual highlight/tangent | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | shadow와 alpha 안정화 | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | groom import prototype | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-11-01** 카드 재질.
- [ ] **AF-11-02** dual highlight/tangent.
- [ ] **AF-11-03** shadow와 alpha 안정화.
- [ ] **AF-11-04** groom import prototype.
- [ ] **AF-11-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-11-V01** 역광: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-11-V02** tangent 방향: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-11-V03** temporal aliasing: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-11-V04** transparency sorting: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-11-V05** 카드/strand 비용: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-11-R01** Marschner/dual scattering 모델: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-11-R02** groom 규격: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-11-R03** alpha coverage mip: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-11-R04** hair physics 분리: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Shading Models](https://dev.epicgames.com/documentation/en-us/unreal-engine/shading-models-in-unreal-engine) — Hair/Eye 등 특수 셰이딩 모델.
2. [Unreal — Rendering Features Reference](https://dev.epicgames.com/documentation/unreal-engine/rendering-features-reference) — 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-12"></a>

## AF-12 — Eye Rendering — 각막·홍채 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-12` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [AF-23 Morph Target / Blendshape — 표정·체형](#af-23) · [AF-04 Layered Material / Advanced BSDF](#af-04) · [StylizedRenderingAndSpriteBakingPlan](../StylizedRenderingAndSpriteBakingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Hair/Eye 등 특수 셰이딩 모델.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** MaterialInterface, ShaderGraph, skinned mesh.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

안구 sclera와 iris/cornea를 분리하거나 단일 mesh에 iris depth/parallax와 cornea Fresnel을 적용한다. eye rotation 및 pupil parameter를 지원하고 굴절은 제한된 모델부터 적용한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 기본 눈 재질 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | iris depth/refraction | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | wetline 및 pupil controls | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | toon variant | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-12-01** 기본 눈 재질.
- [ ] **AF-12-02** iris depth/refraction.
- [ ] **AF-12-03** wetline 및 pupil controls.
- [ ] **AF-12-04** toon variant.
- [ ] **AF-12-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-12-V01** 시선 각도: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-12-V02** 극단 roughness: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-12-V03** 교차 silhouette: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-12-V04** shadow/picking: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-12-V05** PBR/NPR 비교: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-12-R01** IOR/각막 형상: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-12-R02** texture authoring: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-12-R03** refraction RT 비용: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-12-R04** VRM look-at 매핑: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Shading Models](https://dev.epicgames.com/documentation/en-us/unreal-engine/shading-models-in-unreal-engine) — Hair/Eye 등 특수 셰이딩 모델.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-13"></a>

## AF-13 — Parallax Occlusion Mapping 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-13` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [AF-04 Layered Material / Advanced BSDF](#af-04) · [TextureMaterialBakingPlan](../FeaturePlans/TextureMaterialBakingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 모듈형 재질 모델의 비교 사례 / Hair/Eye 등 특수 셰이딩 모델 / 높이맵 기반 per-pixel 추적과 그림자.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ShaderGraphCompiler, MaterialInterface, HLSL.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

tangent view ray와 height field의 교차를 layer stepping 및 선형 refinement로 구해 UV를 offset한다. angle-dependent steps, grazing fade, optional self-shadow. 실제 silhouette/collision 변화는 없으며 depth write는 별도 검토.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | height map node | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | simple parallax vs POM | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | adaptive steps | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | material preview | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-13-01** height map node.
- [ ] **AF-13-02** simple parallax vs POM.
- [ ] **AF-13-03** adaptive steps.
- [ ] **AF-13-04** material preview.
- [ ] **AF-13-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-13-V01** 정면/비스듬한 각도: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-13-V02** UV seam: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-13-V03** tangent sign: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-13-V04** mip derivative: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-13-V05** GPU ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-13-R01** height convention: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-13-R02** explicit gradients: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-13-R03** displacement depth와 shadow mismatch: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-13-R04** POM 원 논문: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Substrate materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-substrate-materials-in-unreal-engine) — 모듈형 재질 모델의 비교 사례.
2. [Unreal — Shading Models](https://dev.epicgames.com/documentation/en-us/unreal-engine/shading-models-in-unreal-engine) — Hair/Eye 등 특수 셰이딩 모델.
3. [Tatarchuk — Dynamic Parallax Occlusion Mapping](https://doi.org/10.1145/1111411.1111423) — 높이맵 기반 per-pixel 추적과 그림자.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-14"></a>

## AF-14 — Variable Rate Shading — 셰이딩 밀도 제어 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-14` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [TemporalUpscalingPlan](../FeaturePlans/TemporalUpscalingPlan.md) · [FrameGraphPlan](../FeaturePlans/FrameGraphPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: D3D12 VRS tier, rate와 API 계약.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** DirectX12RHI, RHIFrameMetrics, RenderPipelineProfile.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

D3D12 OPTIONS6 tier 확인 후 Tier1 draw rate부터 시작한다. Tier2는 tile-sized shading-rate image 및 combiners로 contrast/motion 기반 rate를 만든다. UI, 얇은 geometry, alpha test는 1x1을 유지하며 pixel art에는 강제하지 않는다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | capability probe | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | per-draw rates | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | tile classification CS | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | debug heatmap·quality fallback | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-14-01** capability probe.
- [ ] **AF-14-02** per-draw rates.
- [ ] **AF-14-03** tile classification CS.
- [ ] **AF-14-04** debug heatmap·quality fallback.
- [ ] **AF-14-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-14-V01** tier 미지원 fallback: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-14-V02** MSAA/derivatives: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-14-V03** edge quality: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-14-V04** GPU pass ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-14-V05** image error: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-14-R01** 정확한 tier/combiner 및 image tile size: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-14-R02** VRS와 TAA/DXR 호환: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-14-R03** driver matrix: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Microsoft — Direct3D 12 Variable Rate Shading](https://microsoft.github.io/DirectX-Specs/d3d/VariableRateShading) — D3D12 VRS tier, rate와 API 계약.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-15"></a>

## AF-15 — Neural Texture Compression — 연구 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-15` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P4** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [VirtualTexturePlan](../FeaturePlans/VirtualTexturePlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: neural texture compression SDK 및 샘플 / neural texture/material 기술 소개; 성능 주장은 별도 실측 필요.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** TextureConverter, ResourceStreamer, material bindings.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

RTX NTC SDK의 material texture 집합 압축과 random access decode를 독립 샘플로 평가한다. offline encoder, runtime decoder, inference capability, traditional BCn fallback을 분리한다. decode-to-texture와 sample-time decode 비용을 비교한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 공개 SDK 샘플 빌드 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | BCn 대비 품질/크기 | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | material asset prototype | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | opt-in streaming | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: P4 연구용 prototype. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-15-01** 공개 SDK 샘플 빌드.
- [ ] **AF-15-02** BCn 대비 품질/크기.
- [ ] **AF-15-03** material asset prototype.
- [ ] **AF-15-04** opt-in streaming.
- [ ] **AF-15-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-15-V01** PSNR/normal angular error: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-15-V02** cold load: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-15-V03** VRAM: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-15-V04** inference cost: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-15-V05** 비지원 GPU fallback: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-15-R01** SDK beta/version: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-15-R02** SDK 라이선스: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-15-R03** shader model·cooperative vector 지원: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-15-R04** cross-vendor 가능성: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-15-R05** 배포 의존성: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [NVIDIA — RTXNTC SDK](https://github.com/NVIDIA-RTX/Rtxntc) — neural texture compression SDK 및 샘플.
2. [NVIDIA — RTX Kit](https://developer.nvidia.com/rtx-kit) — neural texture/material 기술 소개; 성능 주장은 별도 실측 필요.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-16"></a>

## AF-16 — Neural Materials — 복합 재질 근사 연구 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-16` · 분야: 그래픽스·렌더링
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P4** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [RenderingDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderingDevelopmentPlan.md) · [MaterialShaderGraphIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialShaderGraphIntegrationPlan.md) · [RenderPipelineProfilePlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/RenderPipelineProfilePlan.md) · [MaterialPrimaryPassTypeDesign](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/MaterialPrimaryPassTypeDesign.md) · [Shader/BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/Shader/BINDING_LAYOUT.md)
- 연계 계획: [AF-04 Layered Material / Advanced BSDF](#af-04) · [AF-15 Neural Texture Compression — 연구](#af-15)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: neural texture/material 기술 소개; 성능 주장은 별도 실측 필요 / 모듈형 재질 모델의 비교 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** MaterialGraph, shader compiler, GPU metrics.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Source/Render, RHI 및 셰이더는 기능 실행을 소유한다. Editor는 에셋/프리뷰/진단만 담당하고 Client는 공용 프로필을 이용한다.

##### 2.2 핵심 알고리즘과 실행 흐름

복합 BSDF를 학습된 작은 네트워크로 근사하는 기술을 별도 연구 모듈에서 평가한다. 입력 view/light/material parameter의 domain을 명시하고 reference shader와 inference를 동일 장면에서 비교한다. 일반 PBR fallback 필수.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 PBR 및 비활성 상태에서 추가 패스·RT 비용이 발생하지 않는지 확인한다. GPU resource transition/UAV barrier, per-view history, fence 기반 해제, PSO/포맷 계약을 각각 검사한다. 픽셀 아트·UI 경로의 해상도 및 색 공간을 구분한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | reference layered shader | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | SDK sample inference | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | offline training pipeline 검토 | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | opt-in material prototype | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: P4 연구용 prototype. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-16-01** reference layered shader.
- [ ] **AF-16-02** SDK sample inference.
- [ ] **AF-16-03** offline training pipeline 검토.
- [ ] **AF-16-04** opt-in material prototype.
- [ ] **AF-16-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-16-V01** white furnace/energy error: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-16-V02** out-of-domain angles: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-16-V03** latency/VRAM: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-16-V04** portability: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-16-R01** RTX SDK 실제 공개 API/라이선스: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-16-R02** 학습 데이터 생성: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-16-R03** network quantization: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-16-R04** unbiasedness 및 GPU 지원: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [NVIDIA — RTX Kit](https://developer.nvidia.com/rtx-kit) — neural texture/material 기술 소개; 성능 주장은 별도 실측 필요.
2. [Unreal — Substrate materials](https://dev.epicgames.com/documentation/en-us/unreal-engine/overview-of-substrate-materials-in-unreal-engine) — 모듈형 재질 모델의 비교 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-17"></a>

## AF-17 — Motion Warping — Root Motion 목표 정렬 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-17` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [MotionMatchingPlan](../FeaturePlans/MotionMatchingPlan.md) · [AF-68 Hitbox / Hurtbox — 공격 및 피격 판정](#af-68)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: warp window 및 목표 위치·회전.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ActionTimelineSystem, AnimatorPoseUpdate, KinematicCharacterSystem.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

clip에 warp window와 target ID를 지정하고 남은 root translation/rotation과 목표 transform의 차이를 window 곡선으로 배분한다. KCC sweep 결과를 실제 이동 권위로 유지하고 target 변경/blocked 상태를 취소 또는 제한한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | root delta 검증 | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | translation window | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | rotation/target tracking | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | vault·attack timeline | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-17-01** root delta 검증.
- [ ] **AF-17-02** translation window.
- [ ] **AF-17-03** rotation/target tracking.
- [ ] **AF-17-04** vault·attack timeline.
- [ ] **AF-17-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-17-V01** window 끝 위치/각도 오차: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-17-V02** collision blocked: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-17-V03** FPS 독립성: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-17-V04** 취소와 root motion replay: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-17-R01** root motion 추출 convention: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-17-R02** skew warp 수식: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-17-R03** moving target: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-17-R04** network authority 및 rollback: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Motion Warping](https://dev.epicgames.com/documentation/unreal-engine/motion-warping-in-unreal-engine) — warp window 및 목표 위치·회전.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-18"></a>

## AF-18 — Pose Warping — 방향·보폭·경사 보정 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-18` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AnimationRetargetingPlan](../FeaturePlans/AnimationRetargetingPlan.md) · [AF-19 Distance Matching — 거리 기반 재생](#af-19)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 방향·보폭 및 leg IK 보정.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AnimationIK.h, AnimatorPoseUpdate, KCC.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

root-local 방향 차이를 pelvis/spine에 분배하고 leg IK target을 실제 이동 속도에 맞춰 stride scale한다. slope ground normal에 foot alignment 및 pelvis height 보정을 적용한다. 원본 clip과 보정 pose를 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | orientation warp | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | stride warp | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | slope/foot contact | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | animator node | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-18-01** orientation warp.
- [ ] **AF-18-02** stride warp.
- [ ] **AF-18-03** slope/foot contact.
- [ ] **AF-18-04** animator node.
- [ ] **AF-18-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-18-V01** foot slip distance: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-18-V02** knee hyperextension: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-18-V03** 계단/경사: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-18-V04** 저속 방향 반전: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-18-V05** pose CPU ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-18-R01** joint constraints: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-18-R02** foot contact curves: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-18-R03** retarget 순서: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-18-R04** KCC actual velocity sampling: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Pose Warping](https://dev.epicgames.com/documentation/en-us/unreal-engine/pose-warping-in-unreal-engine) — 방향·보폭 및 leg IK 보정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-19"></a>

## AF-19 — Distance Matching — 거리 기반 재생 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-19` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AF-17 Motion Warping — Root Motion 목표 정렬](#af-17) · [AF-18 Pose Warping — 방향·보폭·경사 보정](#af-18)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: root motion에서 거리 curve 생성 및 pose 선택 / Distance Matching과 Stride Warping의 적용 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** FloatCurve, AnimatorStateMachine, ActionTimeline.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

root motion 누적 이동 또는 stop distance를 clip curve로 베이크하고 현재 목표 거리에서 monotonic segment inverse lookup으로 sample time을 구한다. 재생 속도 clamp, notify crossing 처리 및 non-monotonic segment 분리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | curve baker | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | stop clip lookup | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | start/landing | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | graph authoring | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-19-01** curve baker.
- [ ] **AF-19-02** stop clip lookup.
- [ ] **AF-19-03** start/landing.
- [ ] **AF-19-04** graph authoring.
- [ ] **AF-19-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-19-V01** stop 위치 오차: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-19-V02** curve plateau: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-19-V03** 역방향 재생: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-19-V04** notify 중복/누락: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-19-V05** speed limits: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-19-R01** root motion unit: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-19-R02** distance sign: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-19-R03** blend와 event evaluation: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-19-R04** noisy ground speed: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Distance Matching](https://dev.epicgames.com/documentation/ko-kr/unreal-engine/distance-matching-in-unreal-engine) — root motion에서 거리 curve 생성 및 pose 선택.
2. [Unreal — Animation in Lyra](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-in-lyra-sample-game-in-unreal-engine) — Distance Matching과 Stride Warping의 적용 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-20"></a>

## AF-20 — Spring Bone — 보조 관절 물리 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-20` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [ModularCharacterSystemPlan](../FeaturePlans/ModularCharacterSystemPlan.md) · [GlobalWindFieldPlan](../FeaturePlans/GlobalWindFieldPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: spring joint·collider 형식 참고.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AnimationIK, AnimatorPoseUpdate, PhysicsSystem, VRM 장기 목표.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

본 chain의 tip을 Verlet 또는 semi-implicit 방식으로 적분하고 rest length, stiffness, drag, gravity, sphere/capsule colliders를 적용한다. 고정 substep과 teleport/reset, parent transform 변환 후 최종 pose에 반영한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 단일 chain | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | collider | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | 여러 chain·LOD | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | VRM springBone importer | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-20-01** 단일 chain.
- [ ] **AF-20-02** collider.
- [ ] **AF-20-03** 여러 chain·LOD.
- [ ] **AF-20-04** VRM springBone importer.
- [ ] **AF-20-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-20-V01** rest length 오차: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-20-V02** FPS 차이: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-20-V03** teleport: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-20-V04** collider tunneling: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-20-V05** 재생 중 disable/enable: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-20-R01** VRM 1.0 update semantics: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-20-R02** center space: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-20-R03** solver 안정성: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-20-R04** self collision 범위: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [VRM — VRMC_springBone 1.0](https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_springBone-1.0/README.md) — spring joint·collider 형식 참고.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-21"></a>

## AF-21 — Inertialization — 포즈 전환 감쇠 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-21` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [MotionMatchingPlan](../FeaturePlans/MotionMatchingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Distance Matching과 Stride Warping의 적용 사례 / 엔진 내부 rig 및 animation 저작 / Inertialization / Dead Blending 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AnimatorStateMachine, AnimatorPoseUpdate.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

transition 시 source-destination pose의 translation 및 quaternion log offset과 velocity 차이를 저장하고 제한된 시간에 0으로 감쇠시킨다. 새 clip은 바로 평가하되 offset을 적용해 연속성을 유지한다. root motion과 이벤트는 따로 처리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | scalar cubic reference | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | pose offset/velocity | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | graph transition opt-in | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | interruption | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-21-01** scalar cubic reference.
- [ ] **AF-21-02** pose offset/velocity.
- [ ] **AF-21-03** graph transition opt-in.
- [ ] **AF-21-04** interruption.
- [ ] **AF-21-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-21-V01** 전환 위치·속도 불연속: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-21-V02** quaternion shortest path: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-21-V03** 중첩 transition: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-21-V04** source 평가 비용 비교: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-21-R01** quintic/cubic 감쇠 원문: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-21-R02** angular velocity: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-21-R03** interruption handling: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-21-R04** blend mask: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Animation in Lyra](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-in-lyra-sample-game-in-unreal-engine) — Distance Matching과 Stride Warping의 적용 사례.
2. [Unreal — Control Rig](https://dev.epicgames.com/documentation/unreal-engine/control-rig-in-unreal-engine) — 엔진 내부 rig 및 animation 저작.
3. [Unreal — Animation Blueprint Blend Nodes](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-blueprint-blend-nodes-in-unreal-engine) — Inertialization / Dead Blending 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-22"></a>

## AF-22 — Animation Compression — 클립 압축 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-22` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AnimationBudgetAllocatorPlan](../FeaturePlans/AnimationBudgetAllocatorPlan.md) · [AF-93 Incremental Cooking — 변경분 빌드](#af-93)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: animation compression 및 정확도 설정 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Animation assets, AnimatorPoseUpdate, ResourceStreamer.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

raw clip을 기준으로 constant track 제거, quaternion smallest-three 및 quantized translation/scale, key reduction을 실험한다. bone-space 수치 외 end-effector world error로 품질을 결정하고 버전 있는 압축 포맷과 decode cache를 둔다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | raw baseline | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | key reduction | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | quantized codec 또는 ACL 비교 | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | cooker/streaming | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-22-01** raw baseline.
- [ ] **AF-22-02** key reduction.
- [ ] **AF-22-03** quantized codec 또는 ACL 비교.
- [ ] **AF-22-04** cooker/streaming.
- [ ] **AF-22-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-22-V01** clip bytes: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-22-V02** decode ns/pose: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-22-V03** hand/foot max error: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-22-V04** loop seam 및 notify preservation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-22-R01** ACL 라이선스/버전: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-22-R02** variable rate tracks: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-22-R03** endian: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-22-R04** retarget 영향: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-22-R05** error threshold per rig: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Animation Compression Library](https://dev.epicgames.com/documentation/unreal-engine/animation-compression-library-in-unreal-engine?lang=en-US) — animation compression 및 정확도 설정 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-23"></a>

## AF-23 — Morph Target / Blendshape — 표정·체형 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-23` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AF-59 Lip Sync — 음성 기반 입모양](#af-59) · [AF-24 Control Rig — 엔진 내 리깅 저작](#af-24) · [AF-87 GPU Skin Cache — 스키닝 결과 공유](#af-87)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: mesh attributes, morph target과 skinning의 데이터 계약 / spring joint·collider 형식 참고 / skeletal asset·morph preview·skin cache 설정 사례 / 표정 및 humanoid 표준 진입점.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Model import, GPU skinning, AnimatorPoseUpdate, MaterialInterface.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

glTF morph delta POSITION/NORMAL/TANGENT를 보존하고 mesh local에서 skinning 전 weighted sum한다. sparse/dense buffer, active weights cap, curve animation 및 bounds 갱신, shadow/picking 동등성을 정의한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | glTF single target | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | multi target GPU path | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | animator curves | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | expression/preset editor | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-23-01** glTF single target.
- [ ] **AF-23-02** multi target GPU path.
- [ ] **AF-23-03** animator curves.
- [ ] **AF-23-04** expression/preset editor.
- [ ] **AF-23-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-23-V01** weight 0/1/negative 정책: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-23-V02** skinned morph: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-23-V03** normals: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-23-V04** bounds/picking: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-23-V05** memory 및 GPU ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-23-R01** Assimp morph import 손실: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-23-R02** VRM expression mapping: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-23-R03** LOD target remap: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-23-R04** tangent recalculation: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Khronos — glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) — mesh attributes, morph target과 skinning의 데이터 계약.
2. [VRM — VRMC_springBone 1.0](https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_springBone-1.0/README.md) — spring joint·collider 형식 참고.
3. [Unreal — Skeletal Mesh Editor](https://dev.epicgames.com/documentation/unreal-engine/skeletal-mesh-editor-in-unreal-engine) — skeletal asset·morph preview·skin cache 설정 사례.
4. [VRM — VRMC_vrm 1.0](https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_vrm-1.0/README.md) — 표정 및 humanoid 표준 진입점.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-24"></a>

## AF-24 — Control Rig — 엔진 내 리깅 저작 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-24` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AnimationRetargetingPlan](../FeaturePlans/AnimationRetargetingPlan.md) · [AF-47 Socket Editor — 부착점 저작](#af-47) · [AF-23 Morph Target / Blendshape — 표정·체형](#af-23)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 엔진 내부 rig 및 animation 저작.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AnimationIK, GraphExecutor, AnimationTimelinePanel, Reflection.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

skeleton과 분리된 control hierarchy/constraints 및 solve graph를 작성한다. forward solve(control→bone), 필요 시 backward solve(bone→control), local/global space 명시, cycle 검증과 순서 결정. editor rig gizmo와 keyframe bake 제공.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | FK controls | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | two-bone IK constraints | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | graph + timeline | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | clip bake 및 undo | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-24-01** FK controls.
- [ ] **AF-24-02** two-bone IK constraints.
- [ ] **AF-24-03** graph + timeline.
- [ ] **AF-24-04** clip bake 및 undo.
- [ ] **AF-24-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-24-V01** IK target 도달 오차: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-24-V02** cycle reject: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-24-V03** mirrored rig: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-24-V04** save/reload: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-24-V05** export pose 비교: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-24-R01** solver 선택: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-24-R02** joint limits: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-24-R03** control/pose spaces: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-24-R04** retarget/VRM 및 rig file schema: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Control Rig](https://dev.epicgames.com/documentation/unreal-engine/control-rig-in-unreal-engine) — 엔진 내부 rig 및 animation 저작.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-25"></a>

## AF-25 — Active Ragdoll / Physical Animation 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-25` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AF-30 Physics Material — 표면 물성](#af-30) · [AF-17 Motion Warping — Root Motion 목표 정렬](#af-17)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, AnimatorPoseUpdate, KCC.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

skeleton bone에 rigidbody/collider와 joint limit를 매핑하고 target pose orientation에 PD drive를 적용한다. 물리 authority, animation blend weight, recovery pose, center of mass와 gameplay capsule의 책임을 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | passive ragdoll | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | joint drive single limb | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | partial hit reaction | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | get-up transition | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-25-01** passive ragdoll.
- [ ] **AF-25-02** joint drive single limb.
- [ ] **AF-25-03** partial hit reaction.
- [ ] **AF-25-04** get-up transition.
- [ ] **AF-25-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-25-V01** joint limit: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-25-V02** torque cap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-25-V03** solver divergence: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-25-V04** fixed timestep: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-25-V05** recovery and contact: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-25-R01** 현재 physics backend joint/drive 지원: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-25-R02** PhysX 도입 여부: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-25-R03** inertia: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-25-R04** contact solver: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-25-R05** replication: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-26"></a>

## AF-26 — XPBD Cloth — 의상 및 천 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-26` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AF-20 Spring Bone — 보조 관절 물리](#af-20) · [GlobalWindFieldPlan](../FeaturePlans/GlobalWindFieldPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: compliance를 통해 timestep/iteration 의존성을 완화하는 constraint solver / rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, mesh skinning, GlobalWindFieldPlan.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

particle positions/inverse mass, stretch/bend constraints와 compliance를 저장한다. XPBD delta lambda=(-C-alphaTilde*lambda)/(sum(w*grad²)+alphaTilde), alphaTilde=compliance/dt². pin vertices를 skinned pose에 연결하고 capsule collision 적용.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | CPU cloth patch | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | stretch/bend/pin | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | capsule collision | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | parallel/GPU 및 LOD | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-26-01** CPU cloth patch.
- [ ] **AF-26-02** stretch/bend/pin.
- [ ] **AF-26-03** capsule collision.
- [ ] **AF-26-04** parallel/GPU 및 LOD.
- [ ] **AF-26-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-26-V01** time step/iteration별 stiffness: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-26-V02** penetration: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-26-V03** strain: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-26-V04** teleport reset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-26-V05** CPU/GPU ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-26-R01** bending constraint: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-26-R02** self-collision broadphase: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-26-R03** solver coloring: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-26-R04** garment import: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-26-R05** normals: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Macklin et al. — XPBD](https://doi.org/10.1145/2994258.2994272) — compliance를 통해 timestep/iteration 의존성을 완화하는 constraint solver.
2. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-27"></a>

## AF-27 — Soft Body Simulation — 탄성 물체 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-27` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P4** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AF-26 XPBD Cloth — 의상 및 천](#af-26) · [DestructionSystemPlan](../FeaturePlans/DestructionSystemPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: compliance를 통해 timestep/iteration 의존성을 완화하는 constraint solver / rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, mesh resource, DX12 compute.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

tetrahedral rest mesh 또는 shape matching cluster를 사용해 volume/shape constraints를 풀고 surface mesh를 barycentric mapping한다. XPBD prototype과 FEM/PhysX SDK 비교; rigid collision과 deformation update를 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | single soft cube | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | volume preservation | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | contact+surface mapping | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | SDK/GPU comparison | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: P4 연구용 prototype. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-27-01** single soft cube.
- [ ] **AF-27-02** volume preservation.
- [ ] **AF-27-03** contact+surface mapping.
- [ ] **AF-27-04** SDK/GPU comparison.
- [ ] **AF-27-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-27-V01** volume error: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-27-V02** energy drift: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-27-V03** contact penetration: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-27-V04** stable dt: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-27-V05** mesh generation cost: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-27-R01** tet meshing: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-27-R02** constitutive model: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-27-R03** inversion prevention: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-27-R04** PhysX API/license: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-27-R05** coupling: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Macklin et al. — XPBD](https://doi.org/10.1145/2994258.2994272) — compliance를 통해 timestep/iteration 의존성을 완화하는 constraint solver.
2. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-28"></a>

## AF-28 — Rope / Cable Physics 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-28` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [Spline 기반 기존 구현](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AF-20 Spring Bone — 보조 관절 물리](#af-20)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: compliance를 통해 timestep/iteration 의존성을 완화하는 constraint solver / rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** SplineComponent, PhysicsSystem, VFX trail.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

particle chain의 distance/bend constraints를 풀고 fixed endpoints 또는 attached body anchors를 둔다. collision sweep 및 radius, tension과 max stretch 제한. spline 또는 tube mesh를 렌더하며 physics chain과 draw sampling을 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 2 endpoints Verlet/XPBD | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | obstacle collision | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | tube mesh | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | grab/release | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-28-01** 2 endpoints Verlet/XPBD.
- [ ] **AF-28-02** obstacle collision.
- [ ] **AF-28-03** tube mesh.
- [ ] **AF-28-04** grab/release.
- [ ] **AF-28-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-28-V01** length error: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-28-V02** sag: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-28-V03** high-speed collision: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-28-V04** anchor delete: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-28-V05** frame-rate independence: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-28-R01** two-way rigid coupling: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-28-R02** self collision: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-28-R03** rope cut topology: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-28-R04** solver compliance: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Macklin et al. — XPBD](https://doi.org/10.1145/2994258.2994272) — compliance를 통해 timestep/iteration 의존성을 완화하는 constraint solver.
2. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-29"></a>

## AF-29 — Vehicle Physics — 이동 수단 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-29` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AF-30 Physics Material — 표면 물성](#af-30) · [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, Input, KCC, CameraComponent.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

wheel ray/sweep suspension, spring-damper force, tire longitudinal/lateral slip과 friction circle, drivetrain/steering/brake를 fixed step에서 계산한다. rigid chassis와 visual wheels를 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 4 wheel arcade prototype | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | suspension/tire | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | gears/steering | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | SDK comparison | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-29-01** 4 wheel arcade prototype.
- [ ] **AF-29-02** suspension/tire.
- [ ] **AF-29-03** gears/steering.
- [ ] **AF-29-04** SDK comparison.
- [ ] **AF-29-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-29-V01** slope: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-29-V02** curb: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-29-V03** braking distance: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-29-V04** roll stability: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-29-V05** fixed Hz: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-29-V06** input latency: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-29-R01** PhysX Vehicle SDK 채택 여부: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-29-R02** tire model: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-29-R03** surface friction: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-29-R04** suspension tuning: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-29-R05** networking: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-30"></a>

## AF-30 — Physics Material — 표면 물성 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-30` · 분야: 애니메이션·물리
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [AnimatorGraphPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimatorGraphPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md) · [KinematicCharacterControllerPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/KinematicCharacterControllerPlan.md) · [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md)
- 연계 계획: [AF-54 Footstep System — 표면별 발소리](#af-54) · [AF-29 Vehicle Physics — 이동 수단](#af-29) · [AF-51 Audio Occlusion — 공간 차폐](#af-51)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, Collider/RigidBody, TerrainLayerAsset.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Animation/Physics가 평가·고정 스텝·결과를 소유한다. Editor는 모델·타임라인 저작/프리뷰를 담당한다. GameModule은 캐릭터의 게임 규칙과 상태를 소유한다.

##### 2.2 핵심 알고리즘과 실행 흐름

asset에 static/dynamic friction, restitution, combine rule, surface tag를 둔다. contact pair material resolve를 backend에 반영하고 gameplay에는 surface ID를 전달한다. render material과 물리 material 연결은 명시적이다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 pose/physics 경로를 유지한다. FixedUpdate 및 render interpolation, root motion, animation notify의 평가 순서를 기록한다. 시간 스케일·pause·teleport·삭제 및 여러 캐릭터의 상태 격리를 검사한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | schema/default migration | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | pair combine | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | terrain layer lookup | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | debug inspector | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-30-01** schema/default migration.
- [ ] **AF-30-02** pair combine.
- [ ] **AF-30-03** terrain layer lookup.
- [ ] **AF-30-04** debug inspector.
- [ ] **AF-30-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-30-V01** ice/rough slide: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-30-V02** bounce energy: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-30-V03** combine symmetry: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-30-V04** terrain seam: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-30-V05** save/load: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-30-R01** 현행 solver material support: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-30-R02** friction approximation: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-30-R03** triangle/subshape ID: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-30-R04** audio surface mapping: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-31"></a>

## AF-31 — Gameplay Camera Graph — 카메라 리그 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-31` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-32 Camera Impulse — 충격 기반 흔들림](#af-32) · [AF-33 Camera Volume — 지역 카메라 설정](#af-33) · [AF-37 Camera Blend — 리그 전환](#af-37)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 카메라 시스템 설계 비교 / 카메라 조합, tracking 및 impulse 레퍼런스.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** CameraComponent, Spring Arm, Action Timeline, GraphExecutor.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

CameraRig asset의 target, desired pose, constraint, lens, blend node를 순서 있는 DAG로 평가한다. gameplay target 결정과 최종 camera state를 분리하고 per-view 상태·priority·cut 이벤트를 유지한다. PIE/Client 동일 evaluator.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 기존 camera adapter | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | rig asset·priority | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | graph authoring | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | camera modifiers 통합 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-31-01** 기존 camera adapter.
- [ ] **AF-31-02** rig asset·priority.
- [ ] **AF-31-03** graph authoring.
- [ ] **AF-31-04** camera modifiers 통합.
- [ ] **AF-31-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-31-V01** 동일 입력의 pose replay: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-31-V02** target delete: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-31-V03** viewport 독립성: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-31-V04** cycle reject: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-31-V05** cut history reset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-31-R01** camera evaluation phase: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-31-R02** existing spring arm API: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-31-R03** graph domain/side effects: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-31-R04** multiple camera ownership: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Camera System](https://dev.epicgames.com/documentation/unreal-engine/gameplay-camera-system?lang=en-US) — 카메라 시스템 설계 비교.
2. [Unity — Cinemachine 3.1 reference](https://docs.unity.cn/Packages/com.unity.cinemachine%403.1/manual/Reference.html) — 카메라 조합, tracking 및 impulse 레퍼런스.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-32"></a>

## AF-32 — Camera Impulse — 충격 기반 흔들림 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-32` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31) · [AF-67 Gameplay Cue — 시각·청각 이벤트](#af-67)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: game event에 따른 camera shake.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** CameraComponent, FloatCurve, ActionTimeline, VFX.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

event{origin, amplitude, duration, channel, seed}를 listener distance attenuation·envelope로 평가해 최종 camera local translation/rotation에 additive 적용한다. bounded amplitude와 accessibility multiplier, noise phase deterministic.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 단일 impulse | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | curve/falloff | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | channel/priority | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | gameplay cue graph | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-32-01** 단일 impulse.
- [ ] **AF-32-02** curve/falloff.
- [ ] **AF-32-03** channel/priority.
- [ ] **AF-32-04** gameplay cue graph.
- [ ] **AF-32-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-32-V01** 거리 감쇠: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-32-V02** 여러 impulse cap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-32-V03** pause/time scale: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-32-V04** seed 재현: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-32-V05** motion sickness disable: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-32-R01** noise spectrum: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-32-R02** fixed/render time choice: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-32-R03** collision 적용 순서: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-32-R04** split-screen listener: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unity — Cinemachine Impulse](https://github.com/Unity-Technologies/com.unity.cinemachine/blob/main/com.unity.cinemachine/Documentation~/CinemachineImpulse.md) — game event에 따른 camera shake.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-33"></a>

## AF-33 — Camera Volume — 지역 카메라 설정 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-33` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31) · [AF-37 Camera Blend — 리그 전환](#af-37)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 카메라 시스템 설계 비교 / 카메라 조합, tracking 및 impulse 레퍼런스.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Collider query, CameraComponent, Scene.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

box/sphere trigger volume에 camera rig override와 priority, blend distance를 저장한다. player position 기반 weight 및 enter/exit hysteresis, 중첩 시 deterministic order. collider trigger와 camera-only volume을 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | box override | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | priority/transition | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | nested volume | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | debug gizmo | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-33-01** box override.
- [ ] **AF-33-02** priority/transition.
- [ ] **AF-33-03** nested volume.
- [ ] **AF-33-04** debug gizmo.
- [ ] **AF-33-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-33-V01** 경계 왕복: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-33-V02** overlap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-33-V03** teleport: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-33-V04** volume disable: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-33-V05** save/load: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-33-R01** weight normalization: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-33-R02** blend conflict: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-33-R03** streaming cell lifetime: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Camera System](https://dev.epicgames.com/documentation/unreal-engine/gameplay-camera-system?lang=en-US) — 카메라 시스템 설계 비교.
2. [Unity — Cinemachine 3.1 reference](https://docs.unity.cn/Packages/com.unity.cinemachine%403.1/manual/Reference.html) — 카메라 조합, tracking 및 impulse 레퍼런스.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-34"></a>

## AF-34 — Target Group Framing — 다중 대상 구도 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-34` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31) · [AF-35 Camera Confiner — 이동 범위 제한](#af-35)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 카메라 시스템 설계 비교 / 카메라 조합, tracking 및 impulse 레퍼런스 / 다중 target과 구도 계산.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** CameraComponent, Transform, AABB.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

target마다 weight/radius를 적용해 world bounds를 계산하고 view projection에서 화면 safe rectangle에 들어오도록 camera distance/FOV/ortho size를 조정한다. damped motion과 최대 zoom, outlier 정책.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 2 targets | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | frustum fit | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | dynamic membership | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | editor safe frame | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-34-01** 2 targets.
- [ ] **AF-34-02** frustum fit.
- [ ] **AF-34-03** dynamic membership.
- [ ] **AF-34-04** editor safe frame.
- [ ] **AF-34-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-34-V01** 화면비 변화: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-34-V02** target 뒤쪽/삭제: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-34-V03** max FOV: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-34-V04** split-screen: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-34-V05** 안정성: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-34-R01** perspective solve: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-34-R02** occlusion vs framing: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-34-R03** aspect/safe area: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-34-R04** damping: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Camera System](https://dev.epicgames.com/documentation/unreal-engine/gameplay-camera-system?lang=en-US) — 카메라 시스템 설계 비교.
2. [Unity — Cinemachine 3.1 reference](https://docs.unity.cn/Packages/com.unity.cinemachine%403.1/manual/Reference.html) — 카메라 조합, tracking 및 impulse 레퍼런스.
3. [Unity — Cinemachine Target Group](https://docs.unity.cn/Packages/com.unity.cinemachine%403.1/manual/CinemachineTargetGroup.html) — 다중 target과 구도 계산.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-35"></a>

## AF-35 — Camera Confiner — 이동 범위 제한 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-35` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31) · [AF-82 Responsive Layout — 화면 비율 대응](#af-82)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: camera confinement 비교; 최신 버전 API는 별도 확인.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** CameraComponent, Scene volumes, PixelPerfectCamera.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

2D polygon 또는 3D box 영역에서 viewport footprint까지 고려한 camera center feasible region을 구한다. nearest point projection과 damping; 영역이 화면보다 작으면 zoom/letterbox 정책. 회전 viewport는 별도 지원.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 2D AABB | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | polygon inset | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | 3D box | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | gizmo | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-35-01** 2D AABB.
- [ ] **AF-35-02** polygon inset.
- [ ] **AF-35-03** 3D box.
- [ ] **AF-35-04** gizmo.
- [ ] **AF-35-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-35-V01** 좁은 영역: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-35-V02** concave corner: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-35-V03** zoom 변화: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-35-V04** pixel snap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-35-V05** boundary jitter: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-35-R01** polygon offset 알고리즘: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-35-R02** nonconvex/holes: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-35-R03** perspective confiner와 collision 순서: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unity — CinemachineConfiner (2.6 API)](https://docs.unity3d.com/ja/Packages/com.unity.cinemachine%402.6/api/Cinemachine.CinemachineConfiner.html) — camera confinement 비교; 최신 버전 API는 별도 확인.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-36"></a>

## AF-36 — Camera Collision — 가림·벽 관통 방지 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-36` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31) · [AF-34 Target Group Framing — 다중 대상 구도](#af-34)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 카메라 시스템 설계 비교 / rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem sweep/raycast, Spring Arm, KCC.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

desired camera 위치까지 sphere sweep하여 near clip/skin margin만큼 이동 제한한다. obstructed 상태에서 fast retract/slow extend, ignore target collider, optional occluder fade. physics query 결과와 render pose를 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | sphere sweep | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | damping | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | multi-target visibility | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | optional fade | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-36-01** sphere sweep.
- [ ] **AF-36-02** damping.
- [ ] **AF-36-03** multi-target visibility.
- [ ] **AF-36-04** optional fade.
- [ ] **AF-36-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-36-V01** 벽 모서리: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-36-V02** 시작 overlap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-36-V03** 얇은 벽: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-36-V04** 빠른 이동: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-36-V05** camera jitter: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-36-R01** 기존 spring arm 중복 여부: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-36-R02** sweep filters: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-36-R03** near-plane geometry: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-36-R04** transparency sorting: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Camera System](https://dev.epicgames.com/documentation/unreal-engine/gameplay-camera-system?lang=en-US) — 카메라 시스템 설계 비교.
2. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-37"></a>

## AF-37 — Camera Blend — 리그 전환 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-37` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31) · [AF-33 Camera Volume — 지역 카메라 설정](#af-33)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 카메라 조합, tracking 및 impulse 레퍼런스 / 카메라 시스템 설계 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ActionTimeline, CameraComponent, FloatCurve.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

source/target camera state를 position lerp, quaternion shortest slerp, lens interpolation으로 결합한다. priority stack, interruption snapshot, cut flag와 TAA history reset. projection type 변화는 별도 cut 정책.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 두 rig blend | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | curve | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | interrupt/priority | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | timeline camera cut 연동 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-37-01** 두 rig blend.
- [ ] **AF-37-02** curve.
- [ ] **AF-37-03** interrupt/priority.
- [ ] **AF-37-04** timeline camera cut 연동.
- [ ] **AF-37-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-37-V01** 중첩 전환: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-37-V02** target 사라짐: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-37-V03** projection 전환: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-37-V04** no discontinuity: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-37-V05** history reset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-37-R01** lens FOV vs focal length: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-37-R02** blend source freeze/live: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-37-R03** gameplay/UI input ownership: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unity — Cinemachine 3.1 reference](https://docs.unity.cn/Packages/com.unity.cinemachine%403.1/manual/Reference.html) — 카메라 조합, tracking 및 impulse 레퍼런스.
2. [Unreal — Gameplay Camera System](https://dev.epicgames.com/documentation/unreal-engine/gameplay-camera-system?lang=en-US) — 카메라 시스템 설계 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-38"></a>

## AF-38 — Lens Distortion — 광각 및 렌즈 왜곡 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-38` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31) · [AF-39 Photo Mode — 자유 시점 촬영](#af-39)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님 / 카메라 시스템 설계 비교 / wide-angle perspective projection 연구.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PostProcessPipeline, camera projection, ShaderGraph.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

radial Brown-Conrady 모델의 k1/k2 및 tangential p1/p2로 출력 UV를 역매핑하고 overscan·border crop을 제어한다. 광각 주변 늘어짐 완화는 Panini projection 같은 별도 mapping을 비교하며 일반 lens distortion과 구분한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | radial post effect | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | inverse mapping/grid preview | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | wide FOV prototype | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | per-camera presets | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-38-01** radial post effect.
- [ ] **AF-38-02** inverse mapping/grid preview.
- [ ] **AF-38-03** wide FOV prototype.
- [ ] **AF-38-04** per-camera presets.
- [ ] **AF-38-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-38-V01** grid straightness: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-38-V02** edge clipping: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-38-V03** pixel aspect: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-38-V04** TAA/picking UI mismatch: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-38-V05** GPU ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-38-R01** Panini 원문: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-38-R02** inverse solver: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-38-R03** FOV convention: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-38-R04** UI와 picking 좌표 보정: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Rendering Features Reference](https://dev.epicgames.com/documentation/unreal-engine/rendering-features-reference) — 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.
2. [Unreal — Gameplay Camera System](https://dev.epicgames.com/documentation/unreal-engine/gameplay-camera-system?lang=en-US) — 카메라 시스템 설계 비교.
3. [Sharpless et al. — Pannini projection](https://doi.org/10.2312/COMPAESTH/COMPAESTH10/009-016) — wide-angle perspective projection 연구.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-39"></a>

## AF-39 — Photo Mode — 자유 시점 촬영 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-39` · 분야: 카메라
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [ThirdPersonCharacterSystemPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ThirdPersonCharacterSystemPlan.md) · [AnimationTimelineEditorPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationTimelineEditorPlan.md) · [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md)
- 연계 계획: [AF-31 Gameplay Camera Graph — 카메라 리그](#af-31) · [AF-75 Input Mapping Context — 상황별 입력](#af-75)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 카메라 시스템 설계 비교 / 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** CameraComponent, DOF, HDR capture, RuntimeSettings, Input.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Camera가 최종 CameraState와 프레임별 평가를 소유한다. GameModule은 대상/모드 선택, Editor는 rig·curve·volume 저작을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

photo session이 gameplay time scale/input/camera를 저장하고 자유 카메라·FOV·DOF·exposure·HUD hide를 임시 override한다. GPU readback 후 SDR PNG 또는 HDR export, async cancellation. multiplayer pause 불가 정책.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 기존 스프링암·타임라인의 카메라 권한과 충돌하지 않게 한다. camera cut/resize 시 temporal history를 초기화하고 Scene/Game View 간 상태 격리 및 UI 입력 우선순위를 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | free camera + restore | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | lens controls | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | capture | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | tiled high-res 연구 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-39-01** free camera + restore.
- [ ] **AF-39-02** lens controls.
- [ ] **AF-39-03** capture.
- [ ] **AF-39-04** tiled high-res 연구.
- [ ] **AF-39-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-39-V01** exit/scene change 원상복구: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-39-V02** screenshot color space: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-39-V03** UI hide: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-39-V04** paused VFX: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-39-V05** capture failure: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-39-R01** HDR/SDR metadata: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-39-R02** offline accumulation: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-39-R03** privacy of UI: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-39-R04** game time pause authority: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Camera System](https://dev.epicgames.com/documentation/unreal-engine/gameplay-camera-system?lang=en-US) — 카메라 시스템 설계 비교.
2. [Unreal — Rendering Features Reference](https://dev.epicgames.com/documentation/unreal-engine/rendering-features-reference) — 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-40"></a>

## AF-40 — CSG / Boolean Modeling — 레벨 블록아웃 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-40` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [AF-44 Mesh Editing — 정점·변·면 편집](#af-44) · [AF-46 Modular Building Tool — 건물·던전 조립](#af-46)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: CSG 레벨 블록아웃 및 복잡한 geometry 비용 / Boolean·mesh editing·vertex painting·UV 도구 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ModelEditor, Mesh resources, SceneEditHistory.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

primitive brush tree의 union/intersection/difference를 manifold solid 대상으로 평가한다. MVP는 convex BSP 또는 외부 robust boolean 비교; material IDs, UV 생성, normals, collision mesh 재생성 및 bake asset. runtime boolean은 제외.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | primitive brushes | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | subtract room | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | undo/preview | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | bake static mesh | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-40-01** primitive brushes.
- [ ] **AF-40-02** subtract room.
- [ ] **AF-40-03** undo/preview.
- [ ] **AF-40-04** bake static mesh.
- [ ] **AF-40-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-40-V01** coplanar/touching/degenerate inputs: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-40-V02** watertightness: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-40-V03** winding: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-40-V04** undo: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-40-V05** bake time: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-40-R01** robust predicates: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-40-R02** manifold library/license: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-40-R03** triangulation: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-40-R04** nonmanifold failure: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-40-R05** CSG vs SDF: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Godot — Prototyping levels with CSG](https://docs.godotengine.org/en/latest/tutorials/3d/csg_tools.html) — CSG 레벨 블록아웃 및 복잡한 geometry 비용.
2. [Unreal — Modeling Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-tools-in-unreal-engine) — Boolean·mesh editing·vertex painting·UV 도구 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-41"></a>

## AF-41 — Voxel Terrain — 체적 지형 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-41` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [WorldPartitionPlan](../FeaturePlans/WorldPartitionPlan.md) · [InteractiveMeshDeformationPlan](../FeaturePlans/InteractiveMeshDeformationPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Boolean·mesh editing·vertex painting·UV 도구 사례 / rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정 / scalar field 등가면 추출의 원 연구(서지 확인).

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** TerrainSystem, ResourceStreamer, PhysicsSystem.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

chunk scalar density field에서 isosurface를 Marching Cubes 또는 Dual Contouring으로 추출한다. border samples 공유·dirty neighboring chunks, mesh LOD seam, collider/nav rebuild queue. 기존 heightmap과 독립 asset type.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | CPU chunk mesher | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | edit brush/dig | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | neighboring border | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | async collision + LOD | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-41-01** CPU chunk mesher.
- [ ] **AF-41-02** edit brush/dig.
- [ ] **AF-41-03** neighboring border.
- [ ] **AF-41-04** async collision + LOD.
- [ ] **AF-41-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-41-V01** chunk seam: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-41-V02** cave topology: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-41-V03** negative coordinates: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-41-V04** edit persistence: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-41-V05** rebuild budget: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-41-R01** lookup table ambiguity: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-41-R02** transvoxel/dual contouring: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-41-R03** memory compression: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-41-R04** deterministic edits: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Modeling Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-tools-in-unreal-engine) — Boolean·mesh editing·vertex painting·UV 도구 사례.
2. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.
3. [Lorensen & Cline — Marching Cubes](https://doi.org/10.1145/37402.37422) — scalar field 등가면 추출의 원 연구(서지 확인).

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-42"></a>

## AF-42 — Vertex Painting — 정점 속성 브러시 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-42` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [AF-43 Texture Painting — UV 텍스처 저작](#af-43) · [AF-04 Layered Material / Advanced BSDF](#af-04)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Boolean·mesh editing·vertex painting·UV 도구 사례.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ModelEditor, MeshRenderSystem, MaterialGraph, SceneEditHistory.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

ray hit triangle에서 radius 내 vertices를 adjacency 또는 spatial index로 수집하고 falloff·strength로 RGBA vertex mask를 변경한다. shared asset vs per-instance override 구분, dirty GPU buffer 업데이트.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | paint channel | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | brush falloff/undo | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | shader graph vertex color | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | save/reimport | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-42-01** paint channel.
- [ ] **AF-42-02** brush falloff/undo.
- [ ] **AF-42-03** shader graph vertex color.
- [ ] **AF-42-04** save/reimport.
- [ ] **AF-42-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-42-V01** 공유 메시 격리: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-42-V02** seam/LOD: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-42-V03** stroke undo: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-42-V04** high density mesh: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-42-V05** render parity: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-42-R01** vertex mapping on reimport: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-42-R02** format precision: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-42-R03** skinned mesh brush: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-42-R04** topology change: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Modeling Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-tools-in-unreal-engine) — Boolean·mesh editing·vertex painting·UV 도구 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-43"></a>

## AF-43 — Texture Painting — UV 텍스처 저작 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-43` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [AF-42 Vertex Painting — 정점 속성 브러시](#af-42) · [TextureMaterialBakingPlan](../FeaturePlans/TextureMaterialBakingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Boolean·mesh editing·vertex painting·UV 도구 사례.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** MaterialEditor, TextureConverter, RenderTargetManager.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

mesh hit UV 또는 world-space brush projection으로 texture RT에 stamp를 누적한다. color space와 channel mask, stroke tile delta undo, mip/edge dilation, non-destructive layer 또는 copy-on-write. original source 보존.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | UV stamp | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | pressure/falloff | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | undo tiles | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | world projection/seam + export | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-43-01** UV stamp.
- [ ] **AF-43-02** pressure/falloff.
- [ ] **AF-43-03** undo tiles.
- [ ] **AF-43-04** world projection/seam + export.
- [ ] **AF-43-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-43-V01** UV seam/overlap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-43-V02** alpha: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-43-V03** linear/sRGB: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-43-V04** mip bleed: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-43-V05** shared material isolation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-43-R01** texture format/RT UAV: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-43-R02** UDIM: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-43-R03** seam handling: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-43-R04** GPU readback: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-43-R05** layer blending: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Modeling Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-tools-in-unreal-engine) — Boolean·mesh editing·vertex painting·UV 도구 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-44"></a>

## AF-44 — Mesh Editing — 정점·변·면 편집 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-44` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [AF-40 CSG / Boolean Modeling — 레벨 블록아웃](#af-40) · [AF-45 Automatic UV Unwrapping](#af-45)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Boolean·mesh editing·vertex painting·UV 도구 사례.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ModelEditor, ImGuizmo, Reflection, Mesh resources.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

half-edge 또는 indexed topology working copy에서 selection, transform, extrude, delete, weld를 수행한다. topology ID와 undo command, normals/tangent/UV/material slots를 갱신하고 mesh/collision/BVH를 다시 생성한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | vertex/face selection | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | transform | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | extrude/weld | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | asset bake/undo | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-44-01** vertex/face selection.
- [ ] **AF-44-02** transform.
- [ ] **AF-44-03** extrude/weld.
- [ ] **AF-44-04** asset bake/undo.
- [ ] **AF-44-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-44-V01** nonmanifold/degenerate validation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-44-V02** normals: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-44-V03** UV/material preservation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-44-V04** undo: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-44-V05** collision: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-44-R01** half-edge library: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-44-R02** robust topology operations: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-44-R03** skin weight remap: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-44-R04** import/export: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Modeling Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-tools-in-unreal-engine) — Boolean·mesh editing·vertex painting·UV 도구 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-45"></a>

## AF-45 — Automatic UV Unwrapping 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-45` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [AF-44 Mesh Editing — 정점·변·면 편집](#af-44) · [TextureMaterialBakingPlan](../FeaturePlans/TextureMaterialBakingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: UV 저작 흐름 / Boolean·mesh editing·vertex painting·UV 도구 사례 / UV chart 생성/packing 라이브러리 후보.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ModelEditor, TextureMaterialBakingPlan, mesh resources.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

mesh chart segmentation 후 conformal parameterization 및 packing으로 atlas UV를 생성한다. seam vertex duplication 시 index/skin/normal/material을 remap하고 UV0/UV1을 명시한다. xatlas 등 후보를 검증한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | existing UV inspection | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | chart/pack prototype | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | checker preview | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | bake workflow | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-45-01** existing UV inspection.
- [ ] **AF-45-02** chart/pack prototype.
- [ ] **AF-45-03** checker preview.
- [ ] **AF-45-04** bake workflow.
- [ ] **AF-45-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-45-V01** UV overlap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-45-V02** stretch: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-45-V03** padding: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-45-V04** chart utilization: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-45-V05** attribute preservation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-45-R01** xatlas 라이선스/API: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-45-R02** lightmap UV vs texture UV: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-45-R03** topology remap and degenerates: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — UV Editor](https://dev.epicgames.com/documentation/en-us/unreal-engine/uv-editor-in-unreal-engine) — UV 저작 흐름.
2. [Unreal — Modeling Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-tools-in-unreal-engine) — Boolean·mesh editing·vertex painting·UV 도구 사례.
3. [xatlas — mesh parameterization](https://github.com/jpcy/xatlas) — UV chart 생성/packing 라이브러리 후보.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-46"></a>

## AF-46 — Modular Building Tool — 건물·던전 조립 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-46` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [ProceduralContentGenerationPlan](../FeaturePlans/ProceduralContentGenerationPlan.md) · [AF-47 Socket Editor — 부착점 저작](#af-47)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Boolean·mesh editing·vertex painting·UV 도구 사례 / CSG 레벨 블록아웃 및 복잡한 geometry 비용.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Prefab, SplinePlacement, TerrainSystem, Editor gizmo.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

module asset에 grid size, sockets, allowed adjacency, orientation, collision footprint를 저장한다. snap placement와 deterministic grammar/PCG를 사용해 wall/floor/roof를 구성하고 결과를 prefab/instances로 bake한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | grid snap kit | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | socket match | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | room layout generator | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | batch edit/bake | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-46-01** grid snap kit.
- [ ] **AF-46-02** socket match.
- [ ] **AF-46-03** room layout generator.
- [ ] **AF-46-04** batch edit/bake.
- [ ] **AF-46-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-46-V01** rotation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-46-V02** mixed size: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-46-V03** closed room connectivity: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-46-V04** overlap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-46-V05** undo: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-46-V06** seed reproducibility: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-46-R01** socket convention: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-46-R02** WFC/grammar 선택: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-46-R03** nav/collision rebuild: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-46-R04** generated ownership: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Modeling Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-tools-in-unreal-engine) — Boolean·mesh editing·vertex painting·UV 도구 사례.
2. [Godot — Prototyping levels with CSG](https://docs.godotengine.org/en/latest/tutorials/3d/csg_tools.html) — CSG 레벨 블록아웃 및 복잡한 geometry 비용.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-47"></a>

## AF-47 — Socket Editor — 부착점 저작 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-47` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [AF-24 Control Rig — 엔진 내 리깅 저작](#af-24) · [ModularCharacterSystemPlan](../FeaturePlans/ModularCharacterSystemPlan.md) · [AF-62 Equipment Framework — 장비 장착](#af-62)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: skeletal asset·morph preview·skin cache 설정 사례 / 엔진 내부 rig 및 animation 저작.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** SocketAttachmentComponent, ActionTimeline, ModelEditor.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

skeleton bone 또는 static mesh node에 stable socket name/ID와 local transform을 저장한다. bone pose * local socket matrix를 계산해 attachment preview, gizmo, weapon/VFX preview를 제공한다. 기존 socket runtime 재사용.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | existing attachment audit | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | socket list/gizmo | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | preview attachment | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | import/export validation | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-47-01** existing attachment audit.
- [ ] **AF-47-02** socket list/gizmo.
- [ ] **AF-47-03** preview attachment.
- [ ] **AF-47-04** import/export validation.
- [ ] **AF-47-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-47-V01** bone rename/delete: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-47-V02** parent scale: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-47-V03** pose update order: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-47-V04** reimport: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-47-V05** save/load: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-47-R01** 현재 소켓 저장 위치와 stable bone IDs: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-47-R02** mirror: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-47-R03** missing socket fallback: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Skeletal Mesh Editor](https://dev.epicgames.com/documentation/unreal-engine/skeletal-mesh-editor-in-unreal-engine) — skeletal asset·morph preview·skin cache 설정 사례.
2. [Unreal — Control Rig](https://dev.epicgames.com/documentation/unreal-engine/control-rig-in-unreal-engine) — 엔진 내부 rig 및 animation 저작.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-48"></a>

## AF-48 — Texture Array Builder — 동질 텍스처 집합 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-48` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [Terrain 기반 기존 구현](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [TextureMaterialBakingPlan](../FeaturePlans/TextureMaterialBakingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: mesh attributes, morph target과 skinning의 데이터 계약 / 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** TextureConverter, TerrainLayerAsset, RHI texture resources.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

동일 dimensions/format/mip count인 slices를 배열로 조립하고 semantic별 color space를 보존한다. resize/normal renormalize·compression·mip generation 및 slice stable ID mapping, graph sample array node.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | validate inputs | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | array resource upload | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | editor builder | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | terrain/material graph integration | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-48-01** validate inputs.
- [ ] **AF-48-02** array resource upload.
- [ ] **AF-48-03** editor builder.
- [ ] **AF-48-04** terrain/material graph integration.
- [ ] **AF-48-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-48-V01** slice index bounds: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-48-V02** mip/BCn: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-48-V03** normal map: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-48-V04** reimport order: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-48-V05** DX12/SDL capability: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-48-R01** RHI array support: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-48-R02** descriptor view type: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-48-R03** BC compression and max slices: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-48-R04** pak serialization: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Khronos — glTF 2.0 specification](https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html) — mesh attributes, morph target과 skinning의 데이터 계약.
2. [Unreal — Rendering Features Reference](https://dev.epicgames.com/documentation/unreal-engine/rendering-features-reference) — 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-49"></a>

## AF-49 — Sprite Extrusion — 2D 이미지의 입체화 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-49` · 분야: 모델링·콘텐츠 제작
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [InteractiveModelImportPreviewPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/InteractiveModelImportPreviewPlan.md) · [AssetReimportRelocationWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetReimportRelocationWorkflowPlan.md) · [ResourceAndAssetSeparationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ResourceAndAssetSeparationPlan.md) · [EditorUndoRedo](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EditorUndoRedo.md) · [TerrainSystemExpansionPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/TerrainSystemExpansionPlan.md)
- 연계 계획: [AF-44 Mesh Editing — 정점·변·면 편집](#af-44) · [StylizedRenderingAndSpriteBakingPlan](../StylizedRenderingAndSpriteBakingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Boolean·mesh editing·vertex painting·UV 도구 사례 / CSG 레벨 블록아웃 및 복잡한 geometry 비용.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Sprite, SpriteAtlas, ModelEditor, mesh resources.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Editor/Tools가 mutable authoring data와 작업 이력을 소유한다. Engine은 bake된 mesh/texture/scene asset만 실행하고 Client에 editor 의존성을 추가하지 않는다.

##### 2.2 핵심 알고리즘과 실행 흐름

alpha mask contour를 marching squares로 추출해 holes와 winding을 보존하고 triangulate한 뒤 앞/뒤 cap과 side walls를 만든다. extrusion depth, bevel optional, UV 및 pivot/PPU를 원본과 맞춘다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 원본 에셋과 인스턴스 수정 범위를 분리한다. 작업 취소/undo 및 저장 실패 시 원본을 보존한다. generated asset의 stable URI와 dependency를 기록하고 reimport, 충돌·피킹·LOD 갱신을 검증한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | rectangle extrude | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | alpha contour+holes | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | side UV | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | asset export | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-49-01** rectangle extrude.
- [ ] **AF-49-02** alpha contour+holes.
- [ ] **AF-49-03** side UV.
- [ ] **AF-49-04** asset export.
- [ ] **AF-49-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-49-V01** transparent holes: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-49-V02** thin islands: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-49-V03** winding: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-49-V04** pivot/PPU: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-49-V05** degenerate polygons: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-49-R01** contour simplification: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-49-R02** constrained triangulation: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-49-R03** alpha threshold: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-49-R04** bevel topology: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Modeling Tools](https://dev.epicgames.com/documentation/en-us/unreal-engine/modeling-tools-in-unreal-engine) — Boolean·mesh editing·vertex painting·UV 도구 사례.
2. [Godot — Prototyping levels with CSG](https://docs.godotengine.org/en/latest/tutorials/3d/csg_tools.html) — CSG 레벨 블록아웃 및 복잡한 geometry 비용.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-50"></a>

## AF-50 — Audio Event Asset — 데이터 기반 사운드 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-50` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-55 Audio Concurrency — 동시 발음 제어](#af-55) · [AF-67 Gameplay Cue — 시각·청각 이벤트](#af-67)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: event, snapshot, parameter 구조; BamEngine의 현 SDK와 차이 확인 필요 / event timeline, instruments, parameter 및 전환.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AudioManager, AudioClip, AudioMixer, FMOD.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

AudioEvent asset에 clip/event reference, 2D/3D, volume/pitch range, variation weights, bus, concurrency, surface tags를 정의한다. FMOD Core wrapper와 Studio event/bank adapter를 구분하고 handle lifecycle 관리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | clip event schema | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | weighted selection | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | graph/timeline trigger | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | Studio adapter 검토 | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-50-01** clip event schema.
- [ ] **AF-50-02** weighted selection.
- [ ] **AF-50-03** graph/timeline trigger.
- [ ] **AF-50-04** Studio adapter 검토.
- [ ] **AF-50-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-50-V01** seed 선택: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-50-V02** stop/release: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-50-V03** missing bank/clip: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-50-V04** scene unload: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-50-V05** export: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-50-R01** 현 FMOD Core/Studio 사용 범위: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-50-R02** bank GUID/path: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-50-R03** SDK 버전 및 라이선스: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [FMOD — Studio Concepts (2.04 docs)](https://www.fmod.com/docs/2.04/studio/fmod-studio-concepts.html) — event, snapshot, parameter 구조; BamEngine의 현 SDK와 차이 확인 필요.
2. [FMOD — Authoring Events (2.04 docs)](https://www.fmod.com/docs/2.04/studio/authoring-events.html) — event timeline, instruments, parameter 및 전환.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-51"></a>

## AF-51 — Audio Occlusion — 공간 차폐 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-51` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-30 Physics Material — 표면 물성](#af-30) · [AF-52 Acoustic Simulation — 공간 반사·잔향](#af-52)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: spatialization, occlusion/transmission, FMOD 연동 / Studio runtime 계약 비교; Core/Studio 실제 사용 범위 재조사.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AudioSourceComponent, AudioListenerComponent, PhysicsSystem.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

listener-source ray 또는 몇 개의 offset rays로 occlusion factor를 계산하고 low-pass/volume smoothing에 전달한다. update budget, distance priority, ignore self, material transmission을 선택적으로 지원한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | single ray | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | smoothing/filter | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | multi-ray partial | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | debug overlay | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-51-01** single ray.
- [ ] **AF-51-02** smoothing/filter.
- [ ] **AF-51-03** multi-ray partial.
- [ ] **AF-51-04** debug overlay.
- [ ] **AF-51-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-51-V01** 문 열고 닫기: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-51-V02** wall thickness: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-51-V03** ray budget: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-51-V04** rapid movement: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-51-V05** audio pops: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-51-R01** FMOD DSP filter API: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-51-R02** material absorption: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-51-R03** query thread: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-51-R04** Steam Audio 선택: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Valve — Steam Audio FMOD integration](https://valvesoftware.github.io/steam-audio/doc/fmod/index.html) — spatialization, occlusion/transmission, FMOD 연동.
2. [FMOD — Studio API Guide (2.04 docs)](https://fmod.com/docs/2.04/api/studio-guide.html) — Studio runtime 계약 비교; Core/Studio 실제 사용 범위 재조사.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-52"></a>

## AF-52 — Acoustic Simulation — 공간 반사·잔향 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-52` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-51 Audio Occlusion — 공간 차폐](#af-51) · [AF-57 Reverb Volume — 지역별 잔향](#af-57)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: spatialization, occlusion/transmission, FMOD 연동.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, Scene mesh, AudioManager.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

Steam Audio 후보로 static acoustic geometry/material을 export하고 source/listener direct path 및 baked/real-time reflection을 계산한다. audio thread에서 geometry 수정하지 않고 simulation worker와 DSP handoff를 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | simple room prototype | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | source/listener HRTF | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | baked reverb | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | dynamic geometry budget | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-52-01** simple room prototype.
- [ ] **AF-52-02** source/listener HRTF.
- [ ] **AF-52-03** baked reverb.
- [ ] **AF-52-04** dynamic geometry budget.
- [ ] **AF-52-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-52-V01** room impulse response: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-52-V02** occluded sound: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-52-V03** CPU cost: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-52-V04** latency: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-52-V05** device/scene reload: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-52-R01** Steam Audio Core vs FMOD Studio integration: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-52-R02** SDK/license: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-52-R03** geometry scale: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-52-R04** plugin redistribution: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Valve — Steam Audio FMOD integration](https://valvesoftware.github.io/steam-audio/doc/fmod/index.html) — spatialization, occlusion/transmission, FMOD 연동.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-53"></a>

## AF-53 — Adaptive Music — 상태 기반 BGM 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-53` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-50 Audio Event Asset — 데이터 기반 사운드](#af-50) · [AF-72 Game Mode Framework — 게임 규칙 및 진행](#af-72)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: event, snapshot, parameter 구조; BamEngine의 현 SDK와 차이 확인 필요 / event timeline, instruments, parameter 및 전환.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AudioMixer, FMOD, ActionTimeline, gameplay states.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

music state asset에 stems/sections, BPM/bar, transition rules, intensity를 정의한다. DSP clock 또는 Studio parameters로 beat/bar quantized transition과 crossfade를 수행하고 pause/resume 정책을 둔다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | two tracks crossfade | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | beat sync | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | intensity layers | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | graph binding | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-53-01** two tracks crossfade.
- [ ] **AF-53-02** beat sync.
- [ ] **AF-53-03** intensity layers.
- [ ] **AF-53-04** graph binding.
- [ ] **AF-53-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-53-V01** transition timing error: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-53-V02** drift: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-53-V03** seek/pause: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-53-V04** rapid state changes: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-53-V05** clipping: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-53-R01** FMOD Core DSP scheduling vs Studio timeline: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-53-R02** sample rate: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-53-R03** loop points: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-53-R04** streaming: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [FMOD — Studio Concepts (2.04 docs)](https://www.fmod.com/docs/2.04/studio/fmod-studio-concepts.html) — event, snapshot, parameter 구조; BamEngine의 현 SDK와 차이 확인 필요.
2. [FMOD — Authoring Events (2.04 docs)](https://www.fmod.com/docs/2.04/studio/authoring-events.html) — event timeline, instruments, parameter 및 전환.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-54"></a>

## AF-54 — Footstep System — 표면별 발소리 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-54` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-30 Physics Material — 표면 물성](#af-30) · [AF-50 Audio Event Asset — 데이터 기반 사운드](#af-50) · [AF-67 Gameplay Cue — 시각·청각 이벤트](#af-67)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: event timeline, instruments, parameter 및 전환 / rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ActionTimeline events, PhysicsSystem, TerrainLayerAsset, AudioManager.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

foot contact notify 또는 locomotion distance trigger에서 foot position raycast로 physical surface tag를 얻고 surface×gait table에서 audio event/VFX를 선택한다. one-step event dedup과 variation seed.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | notify trigger | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | surface lookup | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | weighted event | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | footprint VFX | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-54-01** notify trigger.
- [ ] **AF-54-02** surface lookup.
- [ ] **AF-54-03** weighted event.
- [ ] **AF-54-04** footprint VFX.
- [ ] **AF-54-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-54-V01** walk/run/stop: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-54-V02** stairs: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-54-V03** terrain layer: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-54-V04** left/right: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-54-V05** missed/double notify: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-54-R01** animation contact curves: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-54-R02** surface blend: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-54-R03** KCC vs foot ray: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-54-R04** rollback audiovisual dedup: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [FMOD — Authoring Events (2.04 docs)](https://www.fmod.com/docs/2.04/studio/authoring-events.html) — event timeline, instruments, parameter 및 전환.
2. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-55"></a>

## AF-55 — Audio Concurrency — 동시 발음 제어 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-55` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-50 Audio Event Asset — 데이터 기반 사운드](#af-50)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 동시 음원 제한/그룹 자산 사례 / Studio runtime 계약 비교; Core/Studio 실제 사용 범위 재조사.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AudioManager, AudioSourceComponent, AudioMixer.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

group limit·scope(owner/global)·priority·retrigger interval·steal policy를 데이터로 관리하고 active voices를 handle로 추적한다. stop fade, virtual voice, music reserved budget. middleware 자체 limits와 중복 적용 방지.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | voice registry | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | cap/steal | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | cooldown | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | debug metrics | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-55-01** voice registry.
- [ ] **AF-55-02** cap/steal.
- [ ] **AF-55-03** cooldown.
- [ ] **AF-55-04** debug metrics.
- [ ] **AF-55-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-55-V01** 동시 100 events: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-55-V02** stolen handle: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-55-V03** rapid retrigger: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-55-V04** fade: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-55-V05** voice count/clipping: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-55-R01** FMOD virtual channel 정책과 Studio max instances: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-55-R02** device limits: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-55-R03** concurrency hierarchy: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Sound Concurrency Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-concurrency-reference-guide) — 동시 음원 제한/그룹 자산 사례.
2. [FMOD — Studio API Guide (2.04 docs)](https://fmod.com/docs/2.04/api/studio-guide.html) — Studio runtime 계약 비교; Core/Studio 실제 사용 범위 재조사.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-56"></a>

## AF-56 — Audio Graph — 노드 기반 사운드 제어 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-56` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-50 Audio Event Asset — 데이터 기반 사운드](#af-50) · [AF-53 Adaptive Music — 상태 기반 BGM](#af-53)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: node 기반 음원 제어의 비교 사례 / event, snapshot, parameter 구조; BamEngine의 현 SDK와 차이 확인 필요.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** GraphExecutor, AudioManager, Reflection.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

editor graph를 random/sequence/switch/mix/parameter/play event 노드로 제한해 control graph로 컴파일한다. audio callback에서 일반 GraphExecutor나 allocation을 실행하지 않는다. DSP graph와 control graph 구분.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | random/sequence | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | parameter switch | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | validation/preview | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | Studio event bridge | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-56-01** random/sequence.
- [ ] **AF-56-02** parameter switch.
- [ ] **AF-56-03** validation/preview.
- [ ] **AF-56-04** Studio event bridge.
- [ ] **AF-56-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-56-V01** cycle reject: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-56-V02** deterministic choice: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-56-V03** voice lifecycle: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-56-V04** parameter smoothing: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-56-V05** no callback allocations: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-56-R01** FMOD DSP vs Studio control graph: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-56-R02** graph scheduling: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-56-R03** audio thread RT constraints: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Sound Cue Reference](https://dev.epicgames.com/documentation/en-us/unreal-engine/sound-cue-reference-for-unreal-engine) — node 기반 음원 제어의 비교 사례.
2. [FMOD — Studio Concepts (2.04 docs)](https://www.fmod.com/docs/2.04/studio/fmod-studio-concepts.html) — event, snapshot, parameter 구조; BamEngine의 현 SDK와 차이 확인 필요.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-57"></a>

## AF-57 — Reverb Volume — 지역별 잔향 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-57` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-52 Acoustic Simulation — 공간 반사·잔향](#af-52) · [AF-51 Audio Occlusion — 공간 차폐](#af-51)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: listener 위치 기반 reverb / event, snapshot, parameter 구조; BamEngine의 현 SDK와 차이 확인 필요.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AudioMixer, Scene volumes, PhysicsSystem.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

listener 위치를 volume shape와 비교해 wet/dry, decay, pre-delay 또는 impulse response send를 priority/falloff로 blend한다. overlap smoothing, reverb bus와 source send routing 분리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | box preset | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | overlap blend | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | editor gizmo | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | acoustic SDK adapter | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-57-01** box preset.
- [ ] **AF-57-02** overlap blend.
- [ ] **AF-57-03** editor gizmo.
- [ ] **AF-57-04** acoustic SDK adapter.
- [ ] **AF-57-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-57-V01** 경계 왕복: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-57-V02** multiple volumes: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-57-V03** dry/wet clipping: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-57-V04** teleport: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-57-V05** device change: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-57-R01** FMOD reverb DSP/Studio snapshots: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-57-R02** convolution IR licensing: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-57-R03** CPU budget: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Valve — Steam Audio Reverb](https://valvesoftware.github.io/steam-audio/doc/fmod/reverb.html) — listener 위치 기반 reverb.
2. [FMOD — Studio Concepts (2.04 docs)](https://www.fmod.com/docs/2.04/studio/fmod-studio-concepts.html) — event, snapshot, parameter 구조; BamEngine의 현 SDK와 차이 확인 필요.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-58"></a>

## AF-58 — Audio Visualization — 파형·FFT 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-58` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-56 Audio Graph — 노드 기반 사운드 제어](#af-56) · [GPUParticleVFXGraphPlan](../FeaturePlans/GPUParticleVFXGraphPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Studio runtime 계약 비교; Core/Studio 실제 사용 범위 재조사.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** FMOD AudioManager, VFX/ShaderGraph, MetricsPanel.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

audio DSP에서 제한된 PCM ring 또는 FFT bins를 읽고 analysis frame(timestamp, RMS, peak, bands)를 publish한다. render thread는 snapshot만 소비하고 attack/release smoothing, normalized bands를 제공한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | RMS/peak | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | FFT window | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | visualizer widget | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | VFX/graph parameter | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-58-01** RMS/peak.
- [ ] **AF-58-02** FFT window.
- [ ] **AF-58-03** visualizer widget.
- [ ] **AF-58-04** VFX/graph parameter.
- [ ] **AF-58-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-58-V01** sine frequency bins: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-58-V02** silence: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-58-V03** clipping: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-58-V04** latency: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-58-V05** sample rate changes: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-58-V06** no realtime allocation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-58-R01** FMOD FFT DSP API/version: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-58-R02** window/overlap: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-58-R03** audio callback threading: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-58-R04** capture rights: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [FMOD — Studio API Guide (2.04 docs)](https://fmod.com/docs/2.04/api/studio-guide.html) — Studio runtime 계약 비교; Core/Studio 실제 사용 범위 재조사.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-59"></a>

## AF-59 — Lip Sync — 음성 기반 입모양 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-59` · 분야: 오디오
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [AnimationPipelineIntegrationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AnimationPipelineIntegrationPlan.md)
- 연계 계획: [AF-23 Morph Target / Blendshape — 표정·체형](#af-23) · [AF-58 Audio Visualization — 파형·FFT](#af-58)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: spring joint·collider 형식 참고 / Studio runtime 계약 비교; Core/Studio 실제 사용 범위 재조사 / 표정 및 humanoid 표준 진입점.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AudioManager, MorphTarget, AnimatorPoseUpdate, UI/VRM goal.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine Audio가 재생 handle, DSP 및 믹서 상태를 소유한다. Editor는 event/preset 저작, GameModule은 발생 조건을 소유한다. middleware는 adapter 뒤에 둔다.

##### 2.2 핵심 알고리즘과 실행 흐름

MVP는 mic/clip envelope에서 jaw/open weight를 계산하고 attack/release를 적용한다. 다음 단계에서 timestamped viseme classifier/phoneme mapping과 expression blending, silence/low confidence fallback. microphone privacy·recording opt-in.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 오디오 스레드에서 파일 I/O, ECS 구조 변경, 장시간 lock이나 일반 그래프 실행을 금지한다. handle 수명·voice stop/release, device 변경과 pause를 검사하고 FMOD SDK 파일 및 재배포 정책을 확인한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | offline audio envelope | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | mic input opt-in | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | viseme adapter | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | VRM expression mapping | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-59-01** offline audio envelope.
- [ ] **AF-59-02** mic input opt-in.
- [ ] **AF-59-03** viseme adapter.
- [ ] **AF-59-04** VRM expression mapping.
- [ ] **AF-59-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-59-V01** A/V offset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-59-V02** silence/noise: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-59-V03** missing morph: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-59-V04** audio seek: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-59-V05** device reconnect: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-59-R01** voice SDK/model license: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-59-R02** Korean phoneme/viseme mapping: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-59-R03** sample sync: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-59-R04** permissions and retention: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [VRM — VRMC_springBone 1.0](https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_springBone-1.0/README.md) — spring joint·collider 형식 참고.
2. [FMOD — Studio API Guide (2.04 docs)](https://fmod.com/docs/2.04/api/studio-guide.html) — Studio runtime 계약 비교; Core/Studio 실제 사용 범위 재조사.
3. [VRM — VRMC_vrm 1.0](https://github.com/vrm-c/vrm-specification/blob/master/specification/VRMC_vrm-1.0/README.md) — 표정 및 humanoid 표준 진입점.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-60"></a>

## AF-60 — Interaction System — 상호작용 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-60` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [SmartObjectInteractionPlan](../FeaturePlans/SmartObjectInteractionPlan.md) · [AF-75 Input Mapping Context — 상황별 입력](#af-75)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님 / abilities/effects/cosmetic cue 및 replication 구분.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, Logic Graph, Input, Smart Objects 계획.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

interactor가 ray/overlap으로 후보를 수집하고 distance/angle/priority/tag 조건으로 정렬한다. interaction definition에 prompt, hold duration, canExecute, execute/cancel, reservation을 둔다. 실제 상태 변경은 gameplay authority에서 실행한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 단일 interact target | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | prompt+focus | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | hold/cancel | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | smart object 및 network adapter | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-60-01** 단일 interact target.
- [ ] **AF-60-02** prompt+focus.
- [ ] **AF-60-03** hold/cancel.
- [ ] **AF-60-04** smart object 및 network adapter.
- [ ] **AF-60-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-60-V01** 대상 삭제: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-60-V02** UI focus: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-60-V03** 중복 입력: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-60-V04** 거리 이탈: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-60-V05** 권한 실패: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-60-R01** SmartObject와 중복 경계: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-60-R02** replication 및 접근성: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-60-R03** interaction scope: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Systems](https://dev.epicgames.com/documentation/unreal-engine/gameplay-systems-in-unreal-engine?lang=en-US) — 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.
2. [Unreal — Understanding Gameplay Ability System](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system) — abilities/effects/cosmetic cue 및 replication 구분.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-61"></a>

## AF-61 — Inventory Framework — 아이템 보관 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-61` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [DataTablePlan](../FeaturePlans/DataTablePlan.md) · [AF-62 Equipment Framework — 장비 장착](#af-62) · [AF-77 Virtualized List — 대량 UI 항목](#af-77)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님 / asset audit, dependency 및 cook/chunk 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Reflection/Archive, SaveGameManager, ResourceHandle, Data Table 계획.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

ItemDefinition stable ID와 ItemInstance ID/quantity/attributes를 분리한다. container 슬롯/weight/stack 정책을 적용한 atomic move/split/merge transaction과 delta events. 게임별 플러그인으로 제공한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | item table/slots | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | stack/move | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | drag-drop+save | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | authority adapter | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-61-01** item table/slots.
- [ ] **AF-61-02** stack/move.
- [ ] **AF-61-03** drag-drop+save.
- [ ] **AF-61-04** authority adapter.
- [ ] **AF-61-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-61-V01** overflow: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-61-V02** same slot: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-61-V03** partial move: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-61-V04** failed transaction rollback: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-61-V05** save version: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-61-R01** unique item schema: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-61-R02** inventory UI virtual list: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-61-R03** authority/security: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-61-R04** migration: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Systems](https://dev.epicgames.com/documentation/unreal-engine/gameplay-systems-in-unreal-engine?lang=en-US) — 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.
2. [Unreal — Asset Management](https://dev.epicgames.com/documentation/unreal-engine/asset-management-in-unreal-engine) — asset audit, dependency 및 cook/chunk 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-62"></a>

## AF-62 — Equipment Framework — 장비 장착 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-62` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-61 Inventory Framework — 아이템 보관](#af-61) · [ModularCharacterSystemPlan](../FeaturePlans/ModularCharacterSystemPlan.md) · [AF-47 Socket Editor — 부착점 저작](#af-47)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: abilities/effects/cosmetic cue 및 replication 구분 / 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** SocketAttachmentComponent, Animator, ResourceHandle, SaveGame.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

slot tag, item instance, attachment socket, gameplay modifiers를 관리한다. equip transaction에서 inventory 소유권·조건을 검증하고 이전 장비 복구 가능하게 처리한다. visual mesh와 stats를 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | weapon slot | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | equip/unequip | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | modular mesh/attributes | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | save/network | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-62-01** weapon slot.
- [ ] **AF-62-02** equip/unequip.
- [ ] **AF-62-03** modular mesh/attributes.
- [ ] **AF-62-04** save/network.
- [ ] **AF-62-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-62-V01** incompatible slot: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-62-V02** duplicate ownership: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-62-V03** missing socket: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-62-V04** asset load fail rollback: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-62-R01** GAS effect lifetime: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-62-R02** body masking: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-62-R03** animation override: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-62-R04** inventory transactions: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Understanding Gameplay Ability System](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system) — abilities/effects/cosmetic cue 및 replication 구분.
2. [Unreal — Gameplay Systems](https://dev.epicgames.com/documentation/unreal-engine/gameplay-systems-in-unreal-engine?lang=en-US) — 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-63"></a>

## AF-63 — Quest Graph — 퀘스트 상태 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-63` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-64 Dialogue Graph — 분기형 대화](#af-64) · [AF-65 Event Bus — 타입 안전한 이벤트 전달](#af-65) · [DataTablePlan](../FeaturePlans/DataTablePlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님 / 대화 작성·분기·presentation 참고.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** GraphExecutor, SaveGame, Reflection, localization.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

quest definition의 stable node ID와 runtime progress를 분리한다. objective counters, prerequisites, branches, rewards를 event-driven state machine으로 처리하며 reward claim은 idempotent transaction이다. cycle/terminal reachability 검사.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | linear quest | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | branch+objectives | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | graph editor | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | save migration | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-63-01** linear quest.
- [ ] **AF-63-02** branch+objectives.
- [ ] **AF-63-03** graph editor.
- [ ] **AF-63-04** save migration.
- [ ] **AF-63-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-63-V01** 중복 event/reward: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-63-V02** out-of-order prerequisites: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-63-V03** reload: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-63-V04** abandoned quest: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-63-R01** quest graph vs dialogue graph: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-63-R02** content migration: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-63-R03** replay/authority: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-63-R04** plugin scope: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Systems](https://dev.epicgames.com/documentation/unreal-engine/gameplay-systems-in-unreal-engine?lang=en-US) — 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.
2. [Yarn Spinner — Documentation](https://yarnspinner.dev/docs/) — 대화 작성·분기·presentation 참고.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-64"></a>

## AF-64 — Dialogue Graph — 분기형 대화 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-64` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-63 Quest Graph — 퀘스트 상태](#af-63) · [AF-79 Rich Text — 서식 및 인라인 요소](#af-79) · [AF-50 Audio Event Asset — 데이터 기반 사운드](#af-50)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 대화 작성·분기·presentation 참고.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** GraphExecutor, UI, localization, AudioEventAsset 제안.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

Dialogue asset에 line locale key, speaker, choices, typed conditions, commands, stable IDs를 저장한다. evaluator가 line/choice를 yield하고 UI presenter가 resume한다. side effects는 허용된 command registry로 제한.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | linear dialogue | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | choices/conditions | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | graph preview | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | voice/quest integration | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-64-01** linear dialogue.
- [ ] **AF-64-02** choices/conditions.
- [ ] **AF-64-03** graph preview.
- [ ] **AF-64-04** voice/quest integration.
- [ ] **AF-64-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-64-V01** missing locale: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-64-V02** invalid branch: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-64-V03** save/resume: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-64-V04** skip: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-64-V05** command errors: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-64-V06** cycles: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-64-R01** Yarn Spinner 런타임 채택 vs 자체 graph: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-64-R02** license: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-64-R03** localization/voice IDs: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Yarn Spinner — Documentation](https://yarnspinner.dev/docs/) — 대화 작성·분기·presentation 참고.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-65"></a>

## AF-65 — Event Bus — 타입 안전한 이벤트 전달 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-65` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-67 Gameplay Cue — 시각·청각 이벤트](#af-67) · [AF-60 Interaction System — 상호작용](#af-60) · [AF-84 Task Graph / Job System — 병렬 작업](#af-84)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님 / worker/task scheduler 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** SystemManager, GraphExecutor, Reflection, ActionTimeline events.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

typed event queue와 immediate dispatch를 명시적으로 구분한다. subscription token/generation, scope(scene/global), stable dispatch ordering, owner destroy unsubscribe 및 nested dispatch 규칙. worker thread는 bounded queue로 전달.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | typed local bus | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | RAII subscriptions | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | phase queue | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | trace/debug | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-65-01** typed local bus.
- [ ] **AF-65-02** RAII subscriptions.
- [ ] **AF-65-03** phase queue.
- [ ] **AF-65-04** trace/debug.
- [ ] **AF-65-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-65-V01** subscriber removal during dispatch: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-65-V02** reentrancy: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-65-V03** scene unload: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-65-V04** ordering: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-65-V05** overflow: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-65-R01** EnTT dispatcher 재사용 가능성: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-65-R02** thread safety: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-65-R03** module unload ABI: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-65-R04** replay semantics: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Systems](https://dev.epicgames.com/documentation/unreal-engine/gameplay-systems-in-unreal-engine?lang=en-US) — 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.
2. [Unreal — Tasks Systems](https://dev.epicgames.com/documentation/en-us/unreal-engine/tasks-systems-in-unreal-engine) — worker/task scheduler 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-66"></a>

## AF-66 — Object Pool — 엔티티/VFX 재사용 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-66` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-67 Gameplay Cue — 시각·청각 이벤트](#af-67) · [AF-68 Hitbox / Hurtbox — 공격 및 피격 판정](#af-68) · [GameTemplatePlan](../FeaturePlans/GameTemplatePlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Scene, Entity IDs, ParticleSystem, ResourceHandle.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

prefab별 pool에서 Acquire/Release 계약과 reset hooks를 둔다. inactive entities는 system query에서 제외하고 generation 또는 lease token으로 stale reference를 방지한다. capacity/max growth, prewarm, scene ownership을 명시한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | projectile pool | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | lifecycle reset | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | VFX integration | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | metrics | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-66-01** projectile pool.
- [ ] **AF-66-02** lifecycle reset.
- [ ] **AF-66-03** VFX integration.
- [ ] **AF-66-04** metrics.
- [ ] **AF-66-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-66-V01** double release: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-66-V02** stale handle: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-66-V03** timer/audio/physics reset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-66-V04** 1000 spawn burst p95 vs create/destroy: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-66-R01** EnTT ID lifetime: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-66-R02** prefab reset semantics: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-66-R03** references and network IDs: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-66-R04** GC/VRAM retention: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Systems](https://dev.epicgames.com/documentation/unreal-engine/gameplay-systems-in-unreal-engine?lang=en-US) — 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-67"></a>

## AF-67 — Gameplay Cue — 시각·청각 이벤트 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-67` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [GameplayAbilitySystemPlan](../FeaturePlans/GameplayAbilitySystemPlan.md) · [AF-50 Audio Event Asset — 데이터 기반 사운드](#af-50) · [AF-32 Camera Impulse — 충격 기반 흔들림](#af-32)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: abilities/effects/cosmetic cue 및 replication 구분.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Composite VFX, AudioManager, ActionTimeline, GAS 계획.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

cue tag를 VFX/audio/camera/decals asset으로 해석하고 event ID, source/target, transform, magnitude로 dispatch한다. transient/loop lifecycle와 predicted/confirmed dedup을 정의하며 gameplay damage authority와 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | cue registry | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | spawn transient | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | loop stop | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | network dedup | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-67-01** cue registry.
- [ ] **AF-67-02** spawn transient.
- [ ] **AF-67-03** loop stop.
- [ ] **AF-67-04** network dedup.
- [ ] **AF-67-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-67-V01** duplicate cue: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-67-V02** missing asset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-67-V03** source death: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-67-V04** rollback correction: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-67-V05** pool reset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-67-R01** GAS event contract: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-67-R02** prediction keys: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-67-R03** relevancy: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-67-R04** cosmetic fallback: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Understanding Gameplay Ability System](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system) — abilities/effects/cosmetic cue 및 replication 구분.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-68"></a>

## AF-68 — Hitbox / Hurtbox — 공격 및 피격 판정 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-68` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-17 Motion Warping — Root Motion 목표 정렬](#af-17) · [AF-67 Gameplay Cue — 시각·청각 이벤트](#af-67) · [AF-66 Object Pool — 엔티티/VFX 재사용](#af-66)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정 / abilities/effects/cosmetic cue 및 replication 구분.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, ActionTimeline, SocketAttachment, KCC.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

attack window에 socket-local shape와 layer/filter, hit policy(per target once/cooldown/multihit)를 지정한다. fixed step에서 previous-current shape sweep 또는 overlap, HitResult를 정렬하여 authority에 전달한다. damage calculation은 GAS/game module에서 처리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | sphere/box hitbox | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | timeline window | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | continuous sweep | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | debug hit history | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-68-01** sphere/box hitbox.
- [ ] **AF-68-02** timeline window.
- [ ] **AF-68-03** continuous sweep.
- [ ] **AF-68-04** debug hit history.
- [ ] **AF-68-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-68-V01** 빠른 무기 tunneling: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-68-V02** self hit: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-68-V03** duplicate targets: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-68-V04** cancel/death: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-68-V05** fixed-step reproducibility: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-68-R01** swept capsule/rotating shapes: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-68-R02** hitstop/time scale: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-68-R03** rollback snapshots: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-68-R04** 2D/3D semantics: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.
2. [Unreal — Understanding Gameplay Ability System](https://dev.epicgames.com/documentation/en-us/unreal-engine/understanding-the-unreal-engine-gameplay-ability-system) — abilities/effects/cosmetic cue 및 replication 구분.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-69"></a>

## AF-69 — Utility AI — 점수 기반 행동 선택 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-69` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [StateTreeAIPlan](../FeaturePlans/StateTreeAIPlan.md) · [AF-70 GOAP — 목표 지향 행동 계획](#af-70)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** NavAgent, AI Perception, GraphExecutor, FloatCurve.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

consideration이 typed blackboard input을 0..1 response curve로 매핑하고 weight/combination으로 action score를 산출한다. inertia/cooldown/hysteresis와 invalid action gating, decision tick budget.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 3 actions | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | curve editor | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | scoring/debug | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | StateTree adapter | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-69-01** 3 actions.
- [ ] **AF-69-02** curve editor.
- [ ] **AF-69-03** scoring/debug.
- [ ] **AF-69-04** StateTree adapter.
- [ ] **AF-69-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-69-V01** tie deterministic: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-69-V02** missing data: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-69-V03** oscillation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-69-V04** low score: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-69-V05** decision cost: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-69-R01** multiplicative vs weighted sum: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-69-R02** normalization: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-69-R03** scoring authoring: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-69-R04** AI scale: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Systems](https://dev.epicgames.com/documentation/unreal-engine/gameplay-systems-in-unreal-engine?lang=en-US) — 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-70"></a>

## AF-70 — GOAP — 목표 지향 행동 계획 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-70` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-69 Utility AI — 점수 기반 행동 선택](#af-69) · [StateTreeAIPlan](../FeaturePlans/StateTreeAIPlan.md) · [SmartObjectInteractionPlan](../FeaturePlans/SmartObjectInteractionPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님 / A* 기반 행동 계획의 실제 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** GraphExecutor, NavGrid/NavMesh, AI Perception, SmartObjects.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

world facts를 bitset 또는 typed key로 나타내고 actions에 preconditions/effects/cost를 정의한다. bounded A*로 목표 만족 action sequence를 찾고 실행 중 world changes로 invalidate/replan한다. 계획과 실제 task를 분리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | symbolic small world | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | A* planner | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | executor/replan | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | debug visualization | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-70-01** symbolic small world.
- [ ] **AF-70-02** A* planner.
- [ ] **AF-70-03** executor/replan.
- [ ] **AF-70-04** debug visualization.
- [ ] **AF-70-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-70-V01** unreachable goal: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-70-V02** action fail: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-70-V03** cycles: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-70-V04** budget timeout: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-70-V05** changing target: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-70-R01** GOAP 원전 및 heuristic admissibility: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-70-R02** symbolic state scope: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-70-R03** parallel planners: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Systems](https://dev.epicgames.com/documentation/unreal-engine/gameplay-systems-in-unreal-engine?lang=en-US) — 게임플레이 프레임워크의 범위 비교; 특정 알고리즘의 근거는 아님.
2. [Jeff Orkin — Three States and a Plan: The AI of F.E.A.R.](https://gdcvault.com/play/1013459/Three-States-and-a-Plan) — A* 기반 행동 계획의 실제 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-71"></a>

## AF-71 — Crowd Simulation — 군중 이동 및 회피 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-71` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AnimationBudgetAllocatorPlan](../FeaturePlans/AnimationBudgetAllocatorPlan.md) · [AnimationSharingPlan](../FeaturePlans/AnimationSharingPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: agent 이동과 collision avoidance 구현 후보.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** NavigationSystem, NavGrid/NavMesh, KCC, AnimationBudget 계획.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

global path corridor와 local steering을 분리하고 spatial hash neighbors, velocity obstacle/RVO 또는 DetourCrowd를 비교한다. agent radius/speed/priority, fixed decision ticks, off-mesh links. gameplay critical agents는 독립 제어.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 50 agent steering | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | neighbor avoidance | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | path corridor | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | LOD/budget | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-71-01** 50 agent steering.
- [ ] **AF-71-02** neighbor avoidance.
- [ ] **AF-71-03** path corridor.
- [ ] **AF-71-04** LOD/budget.
- [ ] **AF-71-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-71-V01** 교차 흐름: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-71-V02** 좁은 문: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-71-V03** congestion: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-71-V04** collision rate: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-71-V05** 100/1000 agents CPU ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-71-R01** DetourCrowd integration/license: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-71-R02** ORCA: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-71-R03** deadlocks: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-71-R04** 2D vs 3D constraints: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Recast Navigation — DetourCrowd](https://github.com/recastnavigation/recastnavigation) — agent 이동과 collision avoidance 구현 후보.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-72"></a>

## AF-72 — Game Mode Framework — 게임 규칙 및 진행 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-72` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-65 Event Bus — 타입 안전한 이벤트 전달](#af-65) · [GameTemplatePlan](../FeaturePlans/GameTemplatePlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 게임 상태·플레이어·게임 규칙 책임.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Runtime, SceneManager, SaveGame, GameModule.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

GameMode definition의 player spawn, match rules, win/loss, phase state machine과 runtime GameState를 분리한다. authority가 transitions를 검증하고 UI는 read-only snapshot을 소비한다. scene change 및 persistent session 경계.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | single-player rounds | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | game state events | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | spawn/checkpoint | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | optional replication | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-72-01** single-player rounds.
- [ ] **AF-72-02** game state events.
- [ ] **AF-72-03** spawn/checkpoint.
- [ ] **AF-72-04** optional replication.
- [ ] **AF-72-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-72-V01** pause/restart: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-72-V02** win/loss once: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-72-V03** disconnect: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-72-V04** scene transfer: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-72-V05** save/load: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-72-R01** current GameModule API: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-72-R02** authority split: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-72-R03** scene ownership: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-72-R04** plugin scope: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Gameplay Framework](https://dev.epicgames.com/documentation/unreal-engine/gameplay-framework-in-unreal-engine) — 게임 상태·플레이어·게임 규칙 책임.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-73"></a>

## AF-73 — Modular Game Features — 선택 기능 플러그인 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-73` · 분야: 게임플레이·AI
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [EngineDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineDevelopmentPlan.md) · [VisualScriptingBlueprintPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/VisualScriptingBlueprintPlan.md) · [NetworkFoundationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/NetworkFoundationPlan.md) · [PrefabSceneWorkflowPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/PrefabSceneWorkflowPlan.md)
- 연계 계획: [AF-72 Game Mode Framework — 게임 규칙 및 진행](#af-72) · [AF-65 Event Bus — 타입 안전한 이벤트 전달](#af-65) · [AF-95 Hot Patch / DLC — 콘텐츠 추가 배포](#af-95)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 독립 feature plugin 및 activation 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** GameModule DLL, Editor plugin architecture, Project settings.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine은 재사용 가능한 계약·기초 이벤트만 제공한다. 장르 특화 규칙·보상·게임 경제는 GameModule 또는 선택 plugin, Editor는 데이터 저작 및 디버그를 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

feature manifest에 stable ID/version/dependencies/registration actions를 둔다. load→register→activate→deactivate→unload 상태를 관리하며 component/event/graph registrations를 reversible token으로 추적한다. hot unload는 live references 없을 때만.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- 게임 상태 변경은 권위 있는 runtime 경계에서 검증한다. stable entity/item IDs, transaction 실패 및 undo/rollback, scene unload와 DLL reload, event 순서와 중복 처리를 명시한다. 온라인 기능은 로컬 완성의 필수 조건이 아니다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | static feature registry | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | activate/deactivate | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | asset dependency | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | module reload integration | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-73-01** static feature registry.
- [ ] **AF-73-02** activate/deactivate.
- [ ] **AF-73-03** asset dependency.
- [ ] **AF-73-04** module reload integration.
- [ ] **AF-73-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-73-V01** dependency cycle: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-73-V02** partial activation rollback: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-73-V03** live object unload reject: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-73-V04** Shipping export: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-73-R01** C++ ABI: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-73-R02** reflection registry removal: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-73-R03** DLL lifetime: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-73-R04** editor vs runtime plugins: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Game Features and Modular Gameplay](https://dev.epicgames.com/documentation/unreal-engine/game-features-and-modular-gameplay-in-unreal-engine) — 독립 feature plugin 및 activation 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-74"></a>

## AF-74 — UI Navigation Stack — 메뉴 흐름 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-74` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-75 Input Mapping Context — 상황별 입력](#af-75) · [TweenSequencePlan](../FeaturePlans/TweenSequencePlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 복합 UI 및 공통 위젯·입력 처리.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** UI Canvas, UIInteraction, RuntimeSettingsMenuComponent, Input.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

screen stack에 modal/nonmodal layer, focus scope, Back handler, transition token을 저장한다. push/pop/replace 시 focus save/restore 및 gameplay input capture를 관리하고 duplicate screens를 정책으로 제한한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | stack API | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | focus restore | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | modal routing | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | tween transitions | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-74-01** stack API.
- [ ] **AF-74-02** focus restore.
- [ ] **AF-74-03** modal routing.
- [ ] **AF-74-04** tween transitions.
- [ ] **AF-74-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-74-V01** nested modal: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-74-V02** Back double event: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-74-V03** removed widget: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-74-V04** keyboard/gamepad: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-74-V05** pause: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-74-R01** 현재 UI focus semantics: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-74-R02** pointer capture: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-74-R03** multi-player layers: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-74-R04** accessibility: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Common UI](https://dev.epicgames.com/documentation/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine) — 복합 UI 및 공통 위젯·입력 처리.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-75"></a>

## AF-75 — Input Mapping Context — 상황별 입력 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-75` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-76 Input Rebinding — 키·패드 변경](#af-76) · [AF-74 UI Navigation Stack — 메뉴 흐름](#af-74)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: action/axis 입력 및 Enhanced Input 소개 / 표준 gamepad 입력 매핑 / 복합 UI 및 공통 위젯·입력 처리.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Input module, ShortcutManager, Logic Graph, UIInteraction.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

raw device input→action binding→context priority→trigger state pipeline을 둔다. context stack별 Consume/PassThrough, chord/axis/deadzone, Started/Triggered/Completed/Cancelled events. Editor shortcut과 runtime 분리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | typed actions | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | context priority | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | keyboard+gamepad | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | UI/vehicle context | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-75-01** typed actions.
- [ ] **AF-75-02** context priority.
- [ ] **AF-75-03** keyboard+gamepad.
- [ ] **AF-75-04** UI/vehicle context.
- [ ] **AF-75-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-75-V01** press/release across context switch: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-75-V02** conflicts: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-75-V03** lost focus: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-75-V04** hotplug: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-75-V05** axis deadzone: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-75-R01** 현재 SDL/GameInput abstraction: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-75-R02** action lifecycle: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-75-R03** device pairing: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-75-R04** serialization: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Input Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/input-overview-in-unreal-engine) — action/axis 입력 및 Enhanced Input 소개.
2. [SDL3 — Gamepad category](https://wiki.libsdl.org/SDL3/CategoryGamepad) — 표준 gamepad 입력 매핑.
3. [Unreal — Common UI](https://dev.epicgames.com/documentation/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine) — 복합 UI 및 공통 위젯·입력 처리.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-76"></a>

## AF-76 — Input Rebinding — 키·패드 변경 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-76` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-75 Input Mapping Context — 상황별 입력](#af-75) · [AF-83 Accessibility — 접근성 기반](#af-83)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: action/axis 입력 및 Enhanced Input 소개 / 표준 gamepad 입력 매핑.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Input, RuntimeSettingsManager, UI, localization.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

action stable ID의 user overrides를 default mapping과 분리한다. capture mode에서 next eligible input을 읽고 reserved keys, conflict detection, swap/replace/cancel, timeout을 제공한다. per-device profile 및 reset default.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | keyboard capture | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | conflict dialog | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | gamepad/axis | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | save/migration | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-76-01** keyboard capture.
- [ ] **AF-76-02** conflict dialog.
- [ ] **AF-76-03** gamepad/axis.
- [ ] **AF-76-04** save/migration.
- [ ] **AF-76-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-76-V01** same key conflict: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-76-V02** Escape cancel: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-76-V03** disconnect: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-76-V04** analog noise: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-76-V05** reset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-76-V06** reload: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-76-R01** SDL physical/logical key semantics: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-76-R02** platform reserved keys: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-76-R03** remap persistence: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Input Overview](https://dev.epicgames.com/documentation/en-us/unreal-engine/input-overview-in-unreal-engine) — action/axis 입력 및 Enhanced Input 소개.
2. [SDL3 — Gamepad category](https://wiki.libsdl.org/SDL3/CategoryGamepad) — 표준 gamepad 입력 매핑.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-77"></a>

## AF-77 — Virtualized List — 대량 UI 항목 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-77` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-66 Object Pool — 엔티티/VFX 재사용](#af-66) · [AF-61 Inventory Framework — 아이템 보관](#af-61)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 복합 UI 및 공통 위젯·입력 처리 / rich text, wrapping 및 focus 접근성의 참고 사례.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** UI layout, Canvas, Scroll widgets, Reflection.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

data source count/key + row factory/bind 계약을 정의한다. visible interval과 overscan만 instantiate/recycle하고 variable height는 prefix sum/Fenwick tree로 offset lookup한다. selection/focus는 stable item ID로 관리한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | fixed row virtualization | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | recycle/reset | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | variable height | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | grid+keyboard | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-77-01** fixed row virtualization.
- [ ] **AF-77-02** recycle/reset.
- [ ] **AF-77-03** variable height.
- [ ] **AF-77-04** grid+keyboard.
- [ ] **AF-77-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-77-V01** 10k rows allocations: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-77-V02** scroll jumps: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-77-V03** selection after reorder: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-77-V04** height change: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-77-V05** focus: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-77-R01** 현 UI scroll clipping: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-77-R02** dynamic measurement: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-77-R03** pooled widget cleanup: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-77-R04** accessibility: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Common UI](https://dev.epicgames.com/documentation/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine) — 복합 UI 및 공통 위젯·입력 처리.
2. [Godot — RichTextLabel](https://docs.godotengine.org/en/stable/classes/class_richtextlabel.html) — rich text, wrapping 및 focus 접근성의 참고 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-78"></a>

## AF-78 — UI Theme / Style Sheet — 공통 디자인 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-78` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-83 Accessibility — 접근성 기반](#af-83) · [AF-82 Responsive Layout — 화면 비율 대응](#af-82)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 복합 UI 및 공통 위젯·입력 처리 / rich text, wrapping 및 focus 접근성의 참고 사례.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** UI widgets, Reflection, resource editors, Localization.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

style token asset에 font/color/spacing/radius/texture와 state(normal/hover/pressed/disabled/focus) overrides를 저장한다. precedence를 local > class/style > theme defaults로 명시하고 versioned invalidation을 사용한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | theme asset | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | button states | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | hierarchy override | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | live preview | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-78-01** theme asset.
- [ ] **AF-78-02** button states.
- [ ] **AF-78-03** hierarchy override.
- [ ] **AF-78-04** live preview.
- [ ] **AF-78-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-78-V01** missing token fallback: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-78-V02** theme switch: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-78-V03** text metrics: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-78-V04** focus contrast: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-78-V05** reload: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-78-R01** UI rendering property ownership: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-78-R02** inheritance cycles: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-78-R03** DPI and color space: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Common UI](https://dev.epicgames.com/documentation/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine) — 복합 UI 및 공통 위젯·입력 처리.
2. [Godot — RichTextLabel](https://docs.godotengine.org/en/stable/classes/class_richtextlabel.html) — rich text, wrapping 및 focus 접근성의 참고 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-79"></a>

## AF-79 — Rich Text — 서식 및 인라인 요소 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-79` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-81 Text Shaping — 다국어 글자 조합](#af-81) · [AF-80 MSDF Text Rendering — 확대 가능한 글꼴](#af-80)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: rich text, wrapping 및 focus 접근성의 참고 사례 / 글자 shaping 라이브러리 및 API.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** UI text, sprite atlas, localization.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

restricted markup parser로 span tree를 만들고 text/icon/style runs를 layout한다. unknown/malformed tags는 literal 또는 diagnostic; untrusted strings escaped. shaping, wrap, hyperlink event, inline icon baseline.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | bold/color | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | icon+wrap | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | localization | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | link/focus | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-79-01** bold/color.
- [ ] **AF-79-02** icon+wrap.
- [ ] **AF-79-03** localization.
- [ ] **AF-79-04** link/focus.
- [ ] **AF-79-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-79-V01** malformed nesting: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-79-V02** CJK wrap: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-79-V03** emoji fallback: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-79-V04** line height: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-79-V05** escape injection: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-79-R01** markup grammar: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-79-R02** HarfBuzz runs: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-79-R03** bidi: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-79-R04** text editing selection: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-79-R05** atlas: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Godot — RichTextLabel](https://docs.godotengine.org/en/stable/classes/class_richtextlabel.html) — rich text, wrapping 및 focus 접근성의 참고 사례.
2. [HarfBuzz Manual](https://harfbuzz.github.io/) — 글자 shaping 라이브러리 및 API.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-80"></a>

## AF-80 — MSDF Text Rendering — 확대 가능한 글꼴 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-80` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-81 Text Shaping — 다국어 글자 조합](#af-81) · [AF-79 Rich Text — 서식 및 인라인 요소](#af-79)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: MSDF 생성 라이브러리 / 글자 shaping 라이브러리 및 API.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** UI font rendering, texture atlas, DX12 HLSL.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

font outlines를 msdfgen 후보로 multi-channel distance atlas에 베이크하고 glyph metrics/pxRange를 저장한다. fragment에서 median(rgb)와 screen-space derivatives로 coverage를 계산한다. 작은 글꼴은 bitmap fallback 비교.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | ASCII atlas | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | shader | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | CJK lazy atlas | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | outline/shadow | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-80-01** ASCII atlas.
- [ ] **AF-80-02** shader.
- [ ] **AF-80-03** CJK lazy atlas.
- [ ] **AF-80-04** outline/shadow.
- [ ] **AF-80-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-80-V01** 다양한 scale/DPI: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-80-V02** sharp corners: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-80-V03** small sizes: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-80-V04** CJK memory: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-80-V05** mip sampling: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-80-R01** font licensing: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-80-R02** MSDF atlas generation: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-80-R03** edge coloring: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-80-R04** gamma correct blending: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [msdfgen — Multi-channel signed distance field generator](https://github.com/Chlumsky/msdfgen) — MSDF 생성 라이브러리.
2. [HarfBuzz Manual](https://harfbuzz.github.io/) — 글자 shaping 라이브러리 및 API.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-81"></a>

## AF-81 — Text Shaping — 다국어 글자 조합 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-81` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-80 MSDF Text Rendering — 확대 가능한 글꼴](#af-80) · [AF-83 Accessibility — 접근성 기반](#af-83)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 글자 shaping 라이브러리 및 API / rich text, wrapping 및 focus 접근성의 참고 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** UI text, SDL3 input/IME, Localization.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

UTF-8→Unicode runs(script/direction/font)→HarfBuzz glyph IDs/advances/clusters→atlas rendering. bidi/line breaking은 별도 Unicode algorithm; grapheme-based caret/selection 및 font fallback 지원.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | Latin/Korean | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | shaping/ligatures | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | RTL+bidi | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | fallback/IME caret | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-81-01** Latin/Korean.
- [ ] **AF-81-02** shaping/ligatures.
- [ ] **AF-81-03** RTL+bidi.
- [ ] **AF-81-04** fallback/IME caret.
- [ ] **AF-81-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-81-V01** 한글 조합: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-81-V02** Arabic: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-81-V03** ligature: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-81-V04** emoji clusters: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-81-V05** selection: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-81-V06** line breaks: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-81-R01** FreeType/HarfBuzz/ICU or FriBidi license: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-81-R02** Windows IME: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-81-R03** Unicode version: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [HarfBuzz Manual](https://harfbuzz.github.io/) — 글자 shaping 라이브러리 및 API.
2. [Godot — RichTextLabel](https://docs.godotengine.org/en/stable/classes/class_richtextlabel.html) — rich text, wrapping 및 focus 접근성의 참고 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-82"></a>

## AF-82 — Responsive Layout — 화면 비율 대응 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-82` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-78 UI Theme / Style Sheet — 공통 디자인](#af-78) · [AF-35 Camera Confiner — 이동 범위 제한](#af-35)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 복합 UI 및 공통 위젯·입력 처리 / rich text, wrapping 및 focus 접근성의 참고 사례.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** Canvas, RectTransform, UILayoutCheck, PixelPerfectCamera.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

anchors/pivot와 min/preferred/max constraints, flex-like horizontal/vertical containers, safe area를 정의한다. layout measure/arrange 분리, dirty propagation 및 cycle detection. virtual resolution과 DPI scale 분리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | anchor presets | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | stack/flex | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | aspect constraints | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | safe area preview | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-82-01** anchor presets.
- [ ] **AF-82-02** stack/flex.
- [ ] **AF-82-03** aspect constraints.
- [ ] **AF-82-04** safe area preview.
- [ ] **AF-82-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-82-V01** 16:9/21:9/4:3: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-82-V02** min window: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-82-V03** nesting: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-82-V04** localization overflow: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-82-V05** pixel art integer zoom: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-82-R01** 현재 layout contract: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-82-R02** text measure feedback loops: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-82-R03** DPI/letterbox input mapping: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Common UI](https://dev.epicgames.com/documentation/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine) — 복합 UI 및 공통 위젯·입력 처리.
2. [Godot — RichTextLabel](https://docs.godotengine.org/en/stable/classes/class_richtextlabel.html) — rich text, wrapping 및 focus 접근성의 참고 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-83"></a>

## AF-83 — Accessibility — 접근성 기반 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-83` · 분야: UI·입력
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [UIEditorFoundation](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIEditorFoundation.md) · [UIInteraction](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIInteraction.md) · [UIRenderOrder](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIRenderOrder.md) · [UIWidgetsAndLayoutPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/UIWidgetsAndLayoutPlan.md) · [ImGuiDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImGuiDevelopmentPlan.md)
- 연계 계획: [AF-76 Input Rebinding — 키·패드 변경](#af-76) · [AF-78 UI Theme / Style Sheet — 공통 디자인](#af-78) · [AF-32 Camera Impulse — 충격 기반 흔들림](#af-32)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 복합 UI 및 공통 위젯·입력 처리 / rich text, wrapping 및 focus 접근성의 참고 사례.

일부 참고 자료는 관련 제품의 기능 소개 또는 인접 기술 사례다. 세부 알고리즘 원문, API 및 실제 동등 기능은 이번 조사에서 완전히 확정되지 않았으며 아래 추가 조사 항목을 먼저 해결한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** UIInteraction, Input, RuntimeSettings, Localization.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine UI/Input이 레이아웃·입력 routing·runtime state를 담당한다. Editor는 미리보기 및 데이터 저작, GameModule은 화면의 게임 로직을 담당한다.

##### 2.2 핵심 알고리즘과 실행 흐름

text scale, high contrast/colorblind-safe cues, subtitles, reduced motion, remap, toggle/hold options을 설정에 둔다. semantic role/name/focus order를 위젯에 제공하고 OS accessibility bridge는 별도 검토한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- UI와 월드 입력의 capture/focus, 화면 비율 및 DPI/locale 변경을 검사한다. 위젯 삭제·scene change·키보드/gamepad hotplug에서 stale handle을 제거한다. UI 텍스트는 locale key로 관리한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | settings and captions | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | focus navigation | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | motion/contrast modes | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | semantic metadata | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-83-01** settings and captions.
- [ ] **AF-83-02** focus navigation.
- [ ] **AF-83-03** motion/contrast modes.
- [ ] **AF-83-04** semantic metadata.
- [ ] **AF-83-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-83-V01** 키보드-only 흐름: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-83-V02** 200% text: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-83-V03** contrast: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-83-V04** subtitle duration: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-83-V05** no color-only status: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-83-R01** UIA platform bridge: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-83-R02** WCAG 적용 범위: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-83-R03** disabled input options: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-83-R04** user testing: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Common UI](https://dev.epicgames.com/documentation/unreal-engine/common-ui-plugin-for-advanced-user-interfaces-in-unreal-engine) — 복합 UI 및 공통 위젯·입력 처리.
2. [Godot — RichTextLabel](https://docs.godotengine.org/en/stable/classes/class_richtextlabel.html) — rich text, wrapping 및 focus 접근성의 참고 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-84"></a>

## AF-84 — Task Graph / Job System — 병렬 작업 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-84` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-89 Async Physics — 물리 작업 병렬화](#af-89) · [AnimationBudgetAllocatorPlan](../FeaturePlans/AnimationBudgetAllocatorPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: worker/task scheduler 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** SystemManager, ResourceStreamer workers, Runtime.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

bounded worker pool과 task handle/dependency DAG를 구현하고 read/write resource declarations로 충돌을 제어한다. main/render thread affinity, cancellation, exception propagation, scene lifetime fence. ECS structural mutation은 command buffer로 commit한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | worker pool | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | DAG dependencies | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | isolated animation/resource jobs | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | profiling | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-84-01** worker pool.
- [ ] **AF-84-02** DAG dependencies.
- [ ] **AF-84-03** isolated animation/resource jobs.
- [ ] **AF-84-04** profiling.
- [ ] **AF-84-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-84-V01** cycle reject: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-84-V02** shutdown: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-84-V03** race detection: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-84-V04** deterministic command order: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-84-V05** p95/worker utilization: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-84-R01** EnTT thread safety: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-84-R02** task granularity: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-84-R03** allocator: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-84-R04** Windows thread affinity: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-84-R05** existing worker reuse: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Tasks Systems](https://dev.epicgames.com/documentation/en-us/unreal-engine/tasks-systems-in-unreal-engine) — worker/task scheduler 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-85"></a>

## AF-85 — PSO Precaching — 사전 파이프라인 생성 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-85` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-86 Persistent Pipeline Cache — 디스크 PSO 캐시](#af-86) · [AF-92 Asset Dependency Graph — 에셋 의존성](#af-92)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: PSO 수집·비동기 compile·cold cache 평가.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PipelineManager, DirectX12Pipeline, shader compiler, RHIFrameMetrics.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

normalized PSO key(shader hashes/root signature/RT formats/depth/input layout/raster states)로 scene/material/pass combinations를 수집한다. bounded background compile queue 및 readiness policy(loading wait/fallback)를 둔다. cache miss metrics와 invalidation.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | PSO key audit | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | trace cold misses | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | prewarm startup assets | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | background and editor hot reload | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-85-01** PSO key audit.
- [ ] **AF-85-02** trace cold misses.
- [ ] **AF-85-03** prewarm startup assets.
- [ ] **AF-85-04** background and editor hot reload.
- [ ] **AF-85-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-85-V01** cold vs warm first scene p95/p99: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-85-V02** shader edit invalidation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-85-V03** device reset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-85-V04** compile failure: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-85-R01** D3D12 concurrent create: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-85-R02** render pass permutations: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-85-R03** memory budget: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-85-R04** pipeline library: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — PSO Precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine) — PSO 수집·비동기 compile·cold cache 평가.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-86"></a>

## AF-86 — Persistent Pipeline Cache — 디스크 PSO 캐시 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-86` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-85 PSO Precaching — 사전 파이프라인 생성](#af-85) · [AF-93 Incremental Cooking — 변경분 빌드](#af-93)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: ID3D12PipelineLibrary 등 API 진입점 / PSO 수집·비동기 compile·cold cache 평가.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PipelineManager, DirectX12RHI, disk cache plan.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

D3D12 pipeline library 또는 cached PSO blobs를 versioned disk file에 저장한다. key에 engine/shader/root sig/adapter LUID·driver compatibility를 반영하고 load 실패 시 recreate. atomic write, checksum, bounded eviction.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | serialize single PSO | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | cache header/invalidation | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | library store/load | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | metrics | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-86-01** serialize single PSO.
- [ ] **AF-86-02** cache header/invalidation.
- [ ] **AF-86-03** library store/load.
- [ ] **AF-86-04** metrics.
- [ ] **AF-86-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-86-V01** corrupt cache: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-86-V02** different driver/GPU: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-86-V03** shader update: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-86-V04** cache hit/load ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-86-V05** process crash: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-86-R01** ID3D12PipelineLibrary support/serialization contracts: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-86-R02** disk quota: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-86-R03** multithread writes: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Microsoft — Core Direct3D 12 interfaces](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-interfaces) — ID3D12PipelineLibrary 등 API 진입점.
2. [Unreal — PSO Precaching](https://dev.epicgames.com/documentation/en-us/unreal-engine/pso-precaching-for-unreal-engine) — PSO 수집·비동기 compile·cold cache 평가.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-87"></a>

## AF-87 — GPU Skin Cache — 스키닝 결과 공유 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-87` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-23 Morph Target / Blendshape — 표정·체형](#af-23) · [FrameGraphPlan](../FeaturePlans/FrameGraphPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: Compute Skin Cache와 모드 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** GPU skinning, AnimatorPoseUpdate, DeferredSceneRenderer, DXR.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

compute pass로 skinned positions/normals/tangents를 GPU buffer에 기록하고 geometry/shadow/velocity/DXR이 재사용한다. previous frame buffer 및 dirty pose/version, per-frame fence-safe memory, vertex shader fallback.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | single mesh compute | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | multi-pass consumers | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | previous positions | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | BLAS update integration | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-87-01** single mesh compute.
- [ ] **AF-87-02** multi-pass consumers.
- [ ] **AF-87-03** previous positions.
- [ ] **AF-87-04** BLAS update integration.
- [ ] **AF-87-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-87-V01** VS skinning reference error: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-87-V02** animated shadows: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-87-V03** motion vectors: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-87-V04** morphs: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-87-V05** VRAM vs saved GPU ms: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-87-R01** UAV/vertex state: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-87-R02** BLAS vertex format: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-87-R03** queue scheduling: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-87-R04** skin cache memory eviction: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Skeletal Mesh Rendering Paths](https://dev.epicgames.com/documentation/en-us/unreal-engine/skeletal-mesh-rendering-paths-in-unreal-engine) — Compute Skin Cache와 모드 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-88"></a>

## AF-88 — Visibility Buffer — 지오메트리 정보 재구성 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-88` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P4** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [GPUDrivenRenderingPlan](../FeaturePlans/GPUDrivenRenderingPlan.md) · [AF-04 Layered Material / Advanced BSDF](#af-04)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님 / D3D12 VRS tier, rate와 API 계약 / triangle/instance ID로 G-buffer 대체 제안.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** DeferredSceneRenderer, bindless, GPU driven 계획.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

geometry pass에서 primitive/instance ID 및 barycentric 정보를 저장하고 shading pass에서 vertex attributes/material을 재구성한다. masked material, derivatives, MSAA, decals 및 motion vectors가 복잡하므로 기존 G-buffer와 독립 실험.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | opaque triangle IDs | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | barycentric interpolation | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | material evaluation | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | bandwidth compare | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: P4 연구용 prototype. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-88-01** opaque triangle IDs.
- [ ] **AF-88-02** barycentric interpolation.
- [ ] **AF-88-03** material evaluation.
- [ ] **AF-88-04** bandwidth compare.
- [ ] **AF-88-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-88-V01** pixel-level reference: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-88-V02** primitive ID overflow: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-88-V03** alpha: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-88-V04** gradients: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-88-V05** GPU bandwidth/timing: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-88-R01** visibility buffer 논문: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-88-R02** D3D12 barycentrics capability: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-88-R03** texture gradients: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-88-R04** material bins: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-88-R05** RT layout: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Rendering Features Reference](https://dev.epicgames.com/documentation/unreal-engine/rendering-features-reference) — 렌더링 기능과 플랫폼별 범위 비교; 개별 알고리즘의 완전한 명세는 아님.
2. [Microsoft — Direct3D 12 Variable Rate Shading](https://microsoft.github.io/DirectX-Specs/d3d/VariableRateShading) — D3D12 VRS tier, rate와 API 계약.
3. [Burns & Hunt — Visibility Buffer](https://jcgt.org/published/0002/02/04/) — triangle/instance ID로 G-buffer 대체 제안.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-89"></a>

## AF-89 — Async Physics — 물리 작업 병렬화 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-89` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-84 Task Graph / Job System — 병렬 작업](#af-84) · [AF-96 Rollback Netcode — 입력 예측·상태 복원](#af-96)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정 / worker/task scheduler 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** PhysicsSystem, Runtime fixed step, SystemManager.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

physics worker가 immutable input snapshot과 command queue를 소비하고 result buffer를 publish한다. fixed tick boundaries에서 결과를 commit하고 render interpolation을 수행한다. immediate query는 read snapshot 또는 sync API로 구분한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | thread boundary audit | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | isolated step | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | double buffer | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | interpolation/query policy | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-89-01** thread boundary audit.
- [ ] **AF-89-02** isolated step.
- [ ] **AF-89-03** double buffer.
- [ ] **AF-89-04** interpolation/query policy.
- [ ] **AF-89-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-89-V01** same fixed input results: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-89-V02** race/shutdown: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-89-V03** teleport: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-89-V04** trigger ordering: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-89-V05** latency/p95: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-89-R01** current physics backend thread safety: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-89-R02** substeps: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-89-R03** KCC dependency: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-89-R04** rollback: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-89-R05** determinism: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [NVIDIA — PhysX SDK](https://developer.nvidia.com/physx-sdk) — rigid/soft body 및 차량 기능 비교; 실제 도입 여부 미확정.
2. [Unreal — Tasks Systems](https://dev.epicgames.com/documentation/en-us/unreal-engine/tasks-systems-in-unreal-engine) — worker/task scheduler 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-90"></a>

## AF-90 — GPU Resource Residency — VRAM 예산 관리 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-90` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [VirtualTexturePlan](../FeaturePlans/VirtualTexturePlan.md) · [WorldPartitionPlan](../FeaturePlans/WorldPartitionPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: ID3D12PipelineLibrary 등 API 진입점 / budget notification 및 residency 관리.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** DirectX12RHI heap allocator, ResourceStreamer, RHIFrameMetrics.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

DXGI budget query와 resource/heap usage tracking, per-frame pinned/in-flight fence, priority/LRU 정책을 사용한다. memory pressure에서 streaming mip/LOD부터 낮추고 D3D12 residency API는 capability 및 page-in queue 검증 후 도입한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | budget telemetry | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | eviction priorities | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | stress test | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | residency API prototype | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-90-01** budget telemetry.
- [ ] **AF-90-02** eviction priorities.
- [ ] **AF-90-03** stress test.
- [ ] **AF-90-04** residency API prototype.
- [ ] **AF-90-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-90-V01** budget exceed: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-90-V02** device removed: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-90-V03** in-flight resource protection: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-90-V04** frame hitch: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-90-V05** reload: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-90-R01** DXGI QueryVideoMemoryInfo: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-90-R02** MakeResident/Evict/EnqueueMakeResident contracts: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-90-R03** UMA behavior: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Microsoft — Core Direct3D 12 interfaces](https://learn.microsoft.com/en-us/windows/win32/direct3d12/direct3d-12-interfaces) — ID3D12PipelineLibrary 등 API 진입점.
2. [Microsoft — Residency](https://learn.microsoft.com/windows/win32/direct3d12/residency) — budget notification 및 residency 관리.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-91"></a>

## AF-91 — DirectStorage — 대량 에셋 I/O 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-91` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-93 Incremental Cooking — 변경분 빌드](#af-93) · [AF-90 GPU Resource Residency — VRAM 예산 관리](#af-90)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: I/O batching 및 storage SDK 샘플.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ResourceStreamer, .bampak, compression and AssetWorker.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

pak index의 offset/length/compression block을 storage requests로 변환한다. conventional async file path를 기준으로 request batching 및 DirectStorage queue를 비교한다. GPU decompression은 compatible formats와 alignment를 확보한 뒤 선택한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | I/O benchmark | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | pak block reader | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | DirectStorage adapter | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | GPU decode experiment | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-91-01** I/O benchmark.
- [ ] **AF-91-02** pak block reader.
- [ ] **AF-91-03** DirectStorage adapter.
- [ ] **AF-91-04** GPU decode experiment.
- [ ] **AF-91-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-91-V01** NVMe/SATA fallback: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-91-V02** corrupted request: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-91-V03** cancellation: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-91-V04** throughput/CPU cost/loading p95: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-91-R01** SDK version/OS/storage requirements: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-91-R02** GDeflate vs LZ4: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-91-R03** pack format migration: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-91-R04** license: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Microsoft — DirectStorage](https://github.com/microsoft/DirectStorage) — I/O batching 및 storage SDK 샘플.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-92"></a>

## AF-92 — Asset Dependency Graph — 에셋 의존성 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-92` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-93 Incremental Cooking — 변경분 빌드](#af-93) · [AF-95 Hot Patch / DLC — 콘텐츠 추가 배포](#af-95)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: asset audit, dependency 및 cook/chunk 비교 / asset metadata 및 검색 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** AssetManager, ResourceManager, pak dependency collection, Reflection.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

stable asset URI/ID를 node로, typed hard/soft/build dependencies를 edge로 저장한다. import result에서 graph 갱신, reverse dependencies와 cycle/SCC 검증, hash 기반 dirty propagation. editor rename/relocate는 transaction으로 처리.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | graph extraction | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | reverse lookup | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | dirty invalidation | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | inspector/build integration | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-92-01** graph extraction.
- [ ] **AF-92-02** reverse lookup.
- [ ] **AF-92-03** dirty invalidation.
- [ ] **AF-92-04** inspector/build integration.
- [ ] **AF-92-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-92-V01** rename: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-92-V02** missing reference: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-92-V03** cycles: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-92-V04** delete protection: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-92-V05** incremental consistency: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-92-R01** 현재 dependency collector 확장: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-92-R02** dynamic string refs: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-92-R03** generated assets: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-92-R04** graph persistence: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Asset Management](https://dev.epicgames.com/documentation/unreal-engine/asset-management-in-unreal-engine) — asset audit, dependency 및 cook/chunk 비교.
2. [Unreal — Asset Registry](https://dev.epicgames.com/documentation/unreal-engine/asset-registry-in-unreal-engine) — asset metadata 및 검색 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-93"></a>

## AF-93 — Incremental Cooking — 변경분 빌드 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-93` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P1** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-92 Asset Dependency Graph — 에셋 의존성](#af-92) · [AF-86 Persistent Pipeline Cache — 디스크 PSO 캐시](#af-86) · [AF-95 Hot Patch / DLC — 콘텐츠 추가 배포](#af-95)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 독립 배포 chunk, DLC/patch 사례 / asset audit, dependency 및 cook/chunk 비교.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** ClientPackage, .bampak, AssetWorker, disk cache plan.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

cook key=source content hash+settings+tool version+platform+dependency hashes. clean and incremental cook manifest 비교, atomic output staging, stale outputs 제거. parallel tasks는 memory/IO budgets 사용한다.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | manifest and key | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | incremental reuse | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | dependency propagation | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | clean equivalence test | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-93-01** manifest and key.
- [ ] **AF-93-02** incremental reuse.
- [ ] **AF-93-03** dependency propagation.
- [ ] **AF-93-04** clean equivalence test.
- [ ] **AF-93-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-93-V01** one asset change: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-93-V02** indirect dependency: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-93-V03** removed asset: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-93-V04** corrupted cache: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-93-V05** clean-vs-incremental hashes: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-93-R01** deterministic pack ordering: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-93-R02** external SDK version: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-93-R03** timestamp vs content hashes: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-93-R04** cache schema: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Cooking Content and Creating Chunks](https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-and-creating-chunks-in-unreal-engine) — 독립 배포 chunk, DLC/patch 사례.
2. [Unreal — Asset Management](https://dev.epicgames.com/documentation/unreal-engine/asset-management-in-unreal-engine) — asset audit, dependency 및 cook/chunk 비교.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-94"></a>

## AF-94 — Remote Profiling — 외부 Client 계측 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-94` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P2** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-84 Task Graph / Job System — 병렬 작업](#af-84) · [RenderGraphViewerPlan](../FeaturePlans/RenderGraphViewerPlan.md)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: trace viewer와 live connection 사례 / network trace 분석.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** MetricsPanel, RHIFrameMetrics, Network transport, Logger.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

versioned binary trace events(timestamp/thread/frame/category/counter)와 bounded ring buffer를 설계한다. local file capture 우선, opt-in authenticated remote connection 및 rate limit; GPU delayed timestamps와 CPU clock calibration.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | trace file | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | viewer | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | localhost live | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | optional remote secure session | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-94-01** trace file.
- [ ] **AF-94-02** viewer.
- [ ] **AF-94-03** localhost live.
- [ ] **AF-94-04** optional remote secure session.
- [ ] **AF-94-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-94-V01** event drop reporting: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-94-V02** clock skew: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-94-V03** reconnect: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-94-V04** overhead: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-94-V05** Shipping disabled default: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-94-R01** trace format/Perfetto integration: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-94-R02** security: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-94-R03** privacy: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-94-R04** network backpressure: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Unreal Insights](https://dev.epicgames.com/documentation/unreal-engine/unreal-insights-in-unreal-engine?lang=en-US) — trace viewer와 live connection 사례.
2. [Unreal — Networking Insights](https://dev.epicgames.com/documentation/unreal-engine/networking-insights-in-unreal-engine) — network trace 분석.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-95"></a>

## AF-95 — Hot Patch / DLC — 콘텐츠 추가 배포 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-95` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [AF-93 Incremental Cooking — 변경분 빌드](#af-93) · [AF-73 Modular Game Features — 선택 기능 플러그인](#af-73)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: 독립 배포 chunk, DLC/patch 사례 / 독립 feature plugin 및 activation 사례.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** .bampak mount, ClientPackage, AssetDependencyGraph.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

base build manifest와 versioned patch chunks, content hashes/signature, mount priority 및 compatibility를 정의한다. install staging→verify→atomic activate→rollback. running objects는 safe reload boundary 또는 restart를 요구한다. native code hot patch 제외.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | local additional pak | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | manifest compatibility | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | atomic patch install | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | rollback | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-95-01** local additional pak.
- [ ] **AF-95-02** manifest compatibility.
- [ ] **AF-95-03** atomic patch install.
- [ ] **AF-95-04** rollback.
- [ ] **AF-95-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-95-V01** tampered/truncated pak: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-95-V02** disk full: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-95-V03** version mismatch: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-95-V04** dependency conflict: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-95-V05** restore base: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-95-R01** signature/key management: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-95-R02** platform/store rules: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-95-R03** CDN transport: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-95-R04** licensing: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-95-R05** save compatibility: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [Unreal — Cooking Content and Creating Chunks](https://dev.epicgames.com/documentation/en-us/unreal-engine/cooking-content-and-creating-chunks-in-unreal-engine) — 독립 배포 chunk, DLC/patch 사례.
2. [Unreal — Game Features and Modular Gameplay](https://dev.epicgames.com/documentation/unreal-engine/game-features-and-modular-gameplay-in-unreal-engine) — 독립 feature plugin 및 activation 사례.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |


---

<a id="af-96"></a>

## AF-96 — Rollback Netcode — 입력 예측·상태 복원 개발 계획

- 작성일: 2026-11-26
- 계획 ID: `AF-96` · 분야: 엔진 인프라·최적화
- 상태: **기술 조사 및 구현 제안 / 소스 전수 대조 및 빌드·실행 미수행**
- 추천 우선순위: **P3** (기존 `DevelopmentPriorities.md`에 대한 제안이며 실제 일정 아님)
- 기준: [BamEngine main 스냅샷 `2a9a7e0`](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9)
- 관련 기존 문서: [CodeRefactoringPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/CodeRefactoringPlan.md) · [DiskCacheDevelopmentPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DiskCacheDevelopmentPlan.md) · [BuildAndPackagingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/BuildAndPackagingPlan.md) · [ClientAssetCookingAndPackingPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ClientAssetCookingAndPackingPlan.md) · [AssetPipelineAndRenderingOptimizationPlan](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/AssetPipelineAndRenderingOptimizationPlan.md)
- 연계 계획: [ReplayTimeTravelPlan](../FeaturePlans/ReplayTimeTravelPlan.md) · [AF-67 Gameplay Cue — 시각·청각 이벤트](#af-67) · [AF-89 Async Physics — 물리 작업 병렬화](#af-89)
- [전체 색인](#bamengine-추가-기능-확장-계획--2차-색인)

### 1. 기술 조사와 도입 목적

공개 기술 자료에서 확인한 비교 근거: rollback SDK 후보 / network trace 분석.

참고 자료는 기능의 개념·API 진입점을 확인하기 위한 것이며, 구현 세부와 지원 버전·정확도·라이선스는 아래 추가 조사 단계에서 확정한다.

##### 1.1 BamEngine의 재사용 후보

- **기반 후보:** NetworkSystem/RPC, FixedUpdate, SaveGame, replay 계획.
- 앞선 코드·계획 검토에서 확인한 이름과 시스템을 출발점으로 삼는다. 이 기능의 완전한 구현 여부나 해당 클래스의 세부 호출 계약은 다시 확인해야 한다. 이미 동등 기능이 있으면 기존 모듈의 확장·검증 계획으로 전환하며 중복 Manager를 만들지 않는다.
- 도입 목적은 기존 기능을 대체하지 않고 실제 사용 가능한 최소 기능을 하나의 독립적인 실행/저작 예제로 검증하는 것이다. 비교 엔진 전체 기능의 동등성은 목표가 아니다.

### 2. 기술 설계 제안

##### 2.1 책임 경계

Engine/Core/Render/Resource가 런타임 계약을 소유한다. Tools는 변환 및 offline 산출물, Editor는 진단/저작 UI, Client는 opt-in runtime만 포함한다.

##### 2.2 핵심 알고리즘과 실행 흐름

fixed tick에서 player inputs와 deterministic simulation snapshots/hash를 ring에 저장한다. missing remote input 예측 후 mismatch 시 earliest affected tick restore→resimulate. audiovisual cue dedup/commit, bounded rollback window. authoritative replication과 혼용은 규칙 명시.

##### 2.3 데이터·API·에셋 계약

- 위 설계에서 필요한 입력, 출력, 작성 데이터, 실행 중 임시 상태의 소유자와 수명을 명확히 한다. 구현 첫 단계에서 기존 코드와 대조해 타입/파일명·함수 시그니처를 결정한다. 본 문서는 새 클래스의 존재나 확정 ABI를 주장하지 않는다.
- 지속 저장이 필요한 경우 stable ID·schema version·기본값·의존 리소스·호환 정책을 정의하고 Reflection/Archive 및 `Engine://`, `Project://` 경로를 재사용한다. 캐시/베이크 산출물은 원본을 대체하지 않는다. 기존 데이터에 새 필드가 없어도 명시된 기본 경로에서 읽을 수 있어야 한다.
- baseline을 보존하고 cold/warm, Debug/Release/Shipping을 구분해 측정한다. 실패 시 안전한 기존 경로로 돌아가고 작업 취소·shutdown·version invalidation·파일 손상 등 부정 경로를 테스트한다. 성능 개선이 입증되지 않으면 도입을 보류한다.
- 신규 에셋은 저장/복원/의존성 수집 및 `.bampak` 패키징 검증을 수행한다. 지원하지 않는 플랫폼 또는 장치에서는 진단 메시지와 기존 경로·기능 비활성 중 명시적인 정책을 적용한다.
- 저작 도구가 필요하면 기존 Resource Editor, PropertyTable, Resource Picker, Undo/Redo, 로캘 CSV, ShortcutManager를 우선 사용한다. 저장 실패·작업 취소·프로젝트 전환 시 부분 결과를 공개하지 않는다.

##### 2.4 구현 단위

| 순서 | 구현 범위 | 통과 후 다음 단계 |
| --- | --- | --- |
| 1 | 2-player deterministic toy | 독립 실행 또는 기준 경로와 결과 비교 |
| 2 | simulated latency | 앞 단계의 자동 검사·부정 경로 보존 |
| 3 | rollback/resimulation | 앞 단계의 자동 검사·부정 경로 보존 |
| 4 | desync diagnostics | 에디터/런타임 경계 및 대표 콘텐츠 통합 확인 |

### 3. 단계별 개발 작업

원칙: 작은 대표 예제부터 구현 후 확장. 선택되지 않은 고급 확장은 선행 조건으로 강제하지 않는다.

- [ ] **AF-96-01** 2-player deterministic toy.
- [ ] **AF-96-02** simulated latency.
- [ ] **AF-96-03** rollback/resimulation.
- [ ] **AF-96-04** desync diagnostics.
- [ ] **AF-96-05** 통합 검증 및 사용 예제 작성: 아래 검증 항목, 실패 처리, 기존 경로 회귀 및 문서 갱신.

> 각 항목은 계획이며 완료 표시가 아니다. 실제 구현 과정에서 단계별 commit과 검증 근거를 남긴다. 외부 SDK를 채택하지 않는 독자 구현도 비교한다.

### 4. 검증과 완료 기준

- [ ] **AF-96-V01** same inputs hash: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-96-V02** delay/loss/reorder: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-96-V03** snapshot overflow: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-96-V04** double sound: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.
- [ ] **AF-96-V05** rollback cost: 테스트 입력, 관측값, 기대값 및 재현 조건을 기록한다.

- [ ] 최소 단일 사례와 스트레스 사례를 나누고, 동등한 입력으로 기능 OFF/ON 또는 baseline/후보를 비교한다. 실제 게임 제작에 쓰이는 사례를 하나 이상 준비한다.
- [ ] 순수 함수·데이터 검사에는 기존 CTest 또는 신규 `Check` 타깃을 활용한다. GPU/오디오/GUI 품질 등 단위 테스트로 증명할 수 없는 부분은 대표 씬과 수동 확인 기록을 남긴다.
- [ ] 적용 범위에 따라 Editor 프리뷰, PIE, 독립 Windows Client, Debug 및 Shipping을 구분한다. 에디터 전용 기능은 배포본에 잘못 포함되지 않는지 검사한다.
- [ ] 성능은 기준 빌드/드라이버/해상도/콘텐츠·seed·동시 객체 수·측정 횟수를 명시하고 CPU/GPU 시간, p50/p95/p99, 메모리 또는 I/O 등 해당 지표를 측정한다. 목표값은 기준선 측정 후 설정한다. 결과가 개선되지 않으면 보류하거나 단순 구현으로 복귀한다.

### 5. 추가 조사 필요 — 구현 전에 해결할 기술 과제

**추가 조사가 필요하다.** 아래는 공개 자료와 정적 대조만으로 결정할 수 없는 설계/구현 위험이다. 세부 API, 수식, 구현체 선택을 확정하지 않았다.

- [ ] **AF-96-R01** floating-point determinism: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-96-R02** physics/EnTT ordering: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-96-R03** RNG: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-96-R04** snapshot size: 구현 전 원문/API와 최소 실험으로 확인한다.
- [ ] **AF-96-R05** GGPO SDK/license: 구현 전 원문/API와 최소 실험으로 확인한다.

- [ ] 기존 코드의 실제 사용 경로·데이터 계약·현재 중복 기능과 미검증 항목을 확인한다. 선행 작업이 필요한 경우 기존 계획에 근거와 이유를 추가한다.
- [ ] 외부 라이브러리/SDK/논문 샘플을 사용할 경우 정확한 버전, 플랫폼/드라이버 요구사항, 유지보수, 라이선스/재배포 조건을 검증한다. 실험 코드를 그대로 제품에 포함하지 않는다.
- [ ] 첫 대표 예제 이후에만 성능 예산과 지원 범위를 확정한다. 연구 기술은 기존 경로 대비 비용·품질·제작 효율을 측정한 뒤 계속/보류를 판단한다.

### 6. 참고 자료

이번 조사에서 확인한 공개 자료의 진입점이다. 제품 기능 소개는 구현 계약의 직접 근거가 아니며 논문은 초록·서지 또는 공개 소개 확인을 포함한다. 전문/원문 수식 검증은 5절에 남긴다.

1. [GGPO — Rollback Networking SDK](https://github.com/pond3r/ggpo) — rollback SDK 후보.
2. [Unreal — Networking Insights](https://dev.epicgames.com/documentation/unreal-engine/networking-insights-in-unreal-engine) — network trace 분석.

- 내부 공통 기준: [EngineEditorClientGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/EngineEditorClientGuidelines.md) · [SourceOrganizationGuidelines](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/SourceOrganizationGuidelines.md) · [DevelopmentPriorities](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/DevelopmentPriorities.md) · [ImplementationLog](https://github.com/Bamtoliya/BamEngine/tree/2a9a7e09378de3b3d6aac6f90d989c233811c1a9/Docs/ImplementationLog.md).
- 이전 41개 계획과 스타일화 렌더링 계획은 별도 제공된 원본이다. 이 문서의 `../FeaturePlans/` 및 `../StylizedRenderingAndSpriteBakingPlan.md` 링크는 기존 문서를 권장 위치에 저장했을 때 연결된다.

### 7. 검증 기록 양식

| 구분 | 상태 | 확인 근거 |
| --- | --- | --- |
| 코드 존재 / 정적 검토 | 대기 | commit 및 파일 경로 |
| 빌드 / 자동 검사 | 대기 | preset, CTest 결과 |
| 실행 / 수동 검증 | 대기 | 대표 씬, 기대·실제 결과 |
| 성능 / 품질 | 대기 | 장치, 기준·후보 측정 및 분석 |
| 미지원 / 보류 | 미확정 | 제약과 후속 판단 |
