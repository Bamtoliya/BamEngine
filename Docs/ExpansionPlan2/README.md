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
| AF-01 | [Radiance Cascades — 2D 실시간 간접광](RadianceCascadesPlan.md) | P3 |
| AF-02 | [2D Dynamic Lighting — 스프라이트 조명](TwoDDynamicLightingPlan.md) | P2 |
| AF-03 | [3D Gaussian Splatting — 재구성 장면 렌더러](GaussianSplattingPlan.md) | P4 |
| AF-04 | [Layered Material / Advanced BSDF](LayeredMaterialPlan.md) | P3 |
| AF-05 | [Adaptive Probe Volume — 적응형 베이크 조명](AdaptiveProbeVolumePlan.md) | P3 |
| AF-06 | [Screen Space Reflection — 비 DXR 반사](ScreenSpaceReflectionPlan.md) | P2 |
| AF-07 | [Volume Decal — 영역 기반 표면 변화](VolumeDecalPlan.md) | P2 |
| AF-08 | [Volumetric Local Fog — 국소 안개](VolumetricLocalFogPlan.md) | P2 |
| AF-09 | [Portal Rendering — 공간 포털](PortalRenderingPlan.md) | P3 |
| AF-10 | [Planar Reflection — 평면 반사](PlanarReflectionPlan.md) | P2 |
| AF-11 | [Hair Rendering — 헤어 카드와 Strand](HairRenderingPlan.md) | P3 |
| AF-12 | [Eye Rendering — 각막·홍채](EyeRenderingPlan.md) | P2 |
| AF-13 | [Parallax Occlusion Mapping](ParallaxOcclusionMappingPlan.md) | P2 |
| AF-14 | [Variable Rate Shading — 셰이딩 밀도 제어](VariableRateShadingPlan.md) | P3 |
| AF-15 | [Neural Texture Compression — 연구](NeuralTextureCompressionPlan.md) | P4 |
| AF-16 | [Neural Materials — 복합 재질 근사 연구](NeuralMaterialsPlan.md) | P4 |

### 애니메이션·물리 (14개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-17 | [Motion Warping — Root Motion 목표 정렬](MotionWarpingPlan.md) | P2 |
| AF-18 | [Pose Warping — 방향·보폭·경사 보정](PoseWarpingPlan.md) | P2 |
| AF-19 | [Distance Matching — 거리 기반 재생](DistanceMatchingPlan.md) | P2 |
| AF-20 | [Spring Bone — 보조 관절 물리](SpringBonePlan.md) | P2 |
| AF-21 | [Inertialization — 포즈 전환 감쇠](InertializationPlan.md) | P2 |
| AF-22 | [Animation Compression — 클립 압축](AnimationCompressionPlan.md) | P2 |
| AF-23 | [Morph Target / Blendshape — 표정·체형](MorphTargetPlan.md) | P2 |
| AF-24 | [Control Rig — 엔진 내 리깅 저작](ControlRigPlan.md) | P3 |
| AF-25 | [Active Ragdoll / Physical Animation](ActiveRagdollPlan.md) | P3 |
| AF-26 | [XPBD Cloth — 의상 및 천](XPBDClothPlan.md) | P3 |
| AF-27 | [Soft Body Simulation — 탄성 물체](SoftBodyPlan.md) | P4 |
| AF-28 | [Rope / Cable Physics](RopeCablePhysicsPlan.md) | P2 |
| AF-29 | [Vehicle Physics — 이동 수단](VehiclePhysicsPlan.md) | P3 |
| AF-30 | [Physics Material — 표면 물성](PhysicsMaterialPlan.md) | P1 |

### 카메라 (9개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-31 | [Gameplay Camera Graph — 카메라 리그](GameplayCameraGraphPlan.md) | P2 |
| AF-32 | [Camera Impulse — 충격 기반 흔들림](CameraImpulsePlan.md) | P1 |
| AF-33 | [Camera Volume — 지역 카메라 설정](CameraVolumePlan.md) | P2 |
| AF-34 | [Target Group Framing — 다중 대상 구도](TargetGroupFramingPlan.md) | P2 |
| AF-35 | [Camera Confiner — 이동 범위 제한](CameraConfinerPlan.md) | P2 |
| AF-36 | [Camera Collision — 가림·벽 관통 방지](CameraCollisionPlan.md) | P1 |
| AF-37 | [Camera Blend — 리그 전환](CameraBlendPlan.md) | P1 |
| AF-38 | [Lens Distortion — 광각 및 렌즈 왜곡](LensDistortionPlan.md) | P2 |
| AF-39 | [Photo Mode — 자유 시점 촬영](PhotoModePlan.md) | P2 |

### 모델링·콘텐츠 제작 (10개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-40 | [CSG / Boolean Modeling — 레벨 블록아웃](CSGModelingPlan.md) | P2 |
| AF-41 | [Voxel Terrain — 체적 지형](VoxelTerrainPlan.md) | P3 |
| AF-42 | [Vertex Painting — 정점 속성 브러시](VertexPaintingPlan.md) | P2 |
| AF-43 | [Texture Painting — UV 텍스처 저작](TexturePaintingPlan.md) | P3 |
| AF-44 | [Mesh Editing — 정점·변·면 편집](MeshEditingPlan.md) | P3 |
| AF-45 | [Automatic UV Unwrapping](AutomaticUVUnwrappingPlan.md) | P3 |
| AF-46 | [Modular Building Tool — 건물·던전 조립](ModularBuildingToolPlan.md) | P2 |
| AF-47 | [Socket Editor — 부착점 저작](SocketEditorPlan.md) | P1 |
| AF-48 | [Texture Array Builder — 동질 텍스처 집합](TextureArrayBuilderPlan.md) | P2 |
| AF-49 | [Sprite Extrusion — 2D 이미지의 입체화](SpriteExtrusionPlan.md) | P2 |

### 오디오 (10개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-50 | [Audio Event Asset — 데이터 기반 사운드](AudioEventAssetPlan.md) | P1 |
| AF-51 | [Audio Occlusion — 공간 차폐](AudioOcclusionPlan.md) | P1 |
| AF-52 | [Acoustic Simulation — 공간 반사·잔향](AcousticSimulationPlan.md) | P3 |
| AF-53 | [Adaptive Music — 상태 기반 BGM](AdaptiveMusicPlan.md) | P2 |
| AF-54 | [Footstep System — 표면별 발소리](FootstepSystemPlan.md) | P1 |
| AF-55 | [Audio Concurrency — 동시 발음 제어](AudioConcurrencyPlan.md) | P1 |
| AF-56 | [Audio Graph — 노드 기반 사운드 제어](AudioGraphPlan.md) | P3 |
| AF-57 | [Reverb Volume — 지역별 잔향](ReverbVolumePlan.md) | P2 |
| AF-58 | [Audio Visualization — 파형·FFT](AudioVisualizationPlan.md) | P2 |
| AF-59 | [Lip Sync — 음성 기반 입모양](LipSyncPlan.md) | P2 |

### 게임플레이·AI (14개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-60 | [Interaction System — 상호작용](InteractionSystemPlan.md) | P1 |
| AF-61 | [Inventory Framework — 아이템 보관](InventoryFrameworkPlan.md) | P2 |
| AF-62 | [Equipment Framework — 장비 장착](EquipmentFrameworkPlan.md) | P2 |
| AF-63 | [Quest Graph — 퀘스트 상태](QuestGraphPlan.md) | P2 |
| AF-64 | [Dialogue Graph — 분기형 대화](DialogueGraphPlan.md) | P2 |
| AF-65 | [Event Bus — 타입 안전한 이벤트 전달](EventBusPlan.md) | P1 |
| AF-66 | [Object Pool — 엔티티/VFX 재사용](ObjectPoolPlan.md) | P1 |
| AF-67 | [Gameplay Cue — 시각·청각 이벤트](GameplayCuePlan.md) | P2 |
| AF-68 | [Hitbox / Hurtbox — 공격 및 피격 판정](HitboxHurtboxPlan.md) | P1 |
| AF-69 | [Utility AI — 점수 기반 행동 선택](UtilityAIPlan.md) | P2 |
| AF-70 | [GOAP — 목표 지향 행동 계획](GOAPPlan.md) | P3 |
| AF-71 | [Crowd Simulation — 군중 이동 및 회피](CrowdSimulationPlan.md) | P3 |
| AF-72 | [Game Mode Framework — 게임 규칙 및 진행](GameModeFrameworkPlan.md) | P2 |
| AF-73 | [Modular Game Features — 선택 기능 플러그인](ModularGameFeaturesPlan.md) | P2 |

### UI·입력 (10개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-74 | [UI Navigation Stack — 메뉴 흐름](UINavigationStackPlan.md) | P1 |
| AF-75 | [Input Mapping Context — 상황별 입력](InputMappingContextPlan.md) | P1 |
| AF-76 | [Input Rebinding — 키·패드 변경](InputRebindingPlan.md) | P1 |
| AF-77 | [Virtualized List — 대량 UI 항목](VirtualizedListPlan.md) | P1 |
| AF-78 | [UI Theme / Style Sheet — 공통 디자인](UIThemePlan.md) | P2 |
| AF-79 | [Rich Text — 서식 및 인라인 요소](RichTextPlan.md) | P2 |
| AF-80 | [MSDF Text Rendering — 확대 가능한 글꼴](MSDFTextRenderingPlan.md) | P2 |
| AF-81 | [Text Shaping — 다국어 글자 조합](TextShapingPlan.md) | P2 |
| AF-82 | [Responsive Layout — 화면 비율 대응](ResponsiveLayoutPlan.md) | P2 |
| AF-83 | [Accessibility — 접근성 기반](AccessibilityPlan.md) | P2 |

### 엔진 인프라·최적화 (13개)

| ID | 기능 계획 | 우선순위 |
| --- | --- | --- |
| AF-84 | [Task Graph / Job System — 병렬 작업](TaskJobSystemPlan.md) | P2 |
| AF-85 | [PSO Precaching — 사전 파이프라인 생성](PSOPrecachingPlan.md) | P1 |
| AF-86 | [Persistent Pipeline Cache — 디스크 PSO 캐시](PersistentPipelineCachePlan.md) | P2 |
| AF-87 | [GPU Skin Cache — 스키닝 결과 공유](GPUSkinCachePlan.md) | P2 |
| AF-88 | [Visibility Buffer — 지오메트리 정보 재구성](VisibilityBufferPlan.md) | P4 |
| AF-89 | [Async Physics — 물리 작업 병렬화](AsyncPhysicsPlan.md) | P3 |
| AF-90 | [GPU Resource Residency — VRAM 예산 관리](GPUResourceResidencyPlan.md) | P3 |
| AF-91 | [DirectStorage — 대량 에셋 I/O](DirectStoragePlan.md) | P3 |
| AF-92 | [Asset Dependency Graph — 에셋 의존성](AssetDependencyGraphPlan.md) | P1 |
| AF-93 | [Incremental Cooking — 변경분 빌드](IncrementalCookingPlan.md) | P1 |
| AF-94 | [Remote Profiling — 외부 Client 계측](RemoteProfilingPlan.md) | P2 |
| AF-95 | [Hot Patch / DLC — 콘텐츠 추가 배포](HotPatchDLCPlan.md) | P3 |
| AF-96 | [Rollback Netcode — 입력 예측·상태 복원](RollbackNetcodePlan.md) | P3 |

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
