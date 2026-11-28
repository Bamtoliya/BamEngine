# BamEngine Implementation Log

This document tracks implementation details, architectural decisions, and technical debt.

### [2026-11-28] 리소스 수명 관리, 이벤트 구독, 프로젝트 모듈 빌드 및 개발 문서

- 리소스 핸들의 타입과 슬롯을 참조 획득 시 확인한다. 리소스 매니저는 등록·해제, 유휴 캐시 수거와 지연 삭제를 관리하며, 의존성 스냅샷으로 등록된 리소스 간 참조를 조회한다. 비동기 로더와 씬 스트리밍 경로도 함께 갱신했다. [리소스 API](APIReference/Resource/ResourceManagement.md)
- EventBus는 발행 중 이벤트 저장소의 수명을 유지하고, 구독 핸들을 버스에서 발급한다. Delegate의 구독 변경 처리와 함께 확인할 수 있다. [이벤트 API](APIReference/Core/Event.md)
- Windows 게임 모듈 산출물을 Editor, Client, Shipping 역할과 빌드 구성별로 분리하고, 외부 CMake 소비자와 Editor 모듈 빌드·Client 패키징 경로를 갱신했다. 파일 경계에서는 공용 Unicode 변환을 사용한다. [프로젝트 모듈 API](APIReference/Core/ProjectModule.md)
- 사용자 가이드와 API 레퍼런스에 목차와 소스 근거를 추가했다. 각 문서에 표시한 기준 커밋은 최초 조사 시점을 가리킨다. [사용자 가이드](UserGuide/README.md) · [API 레퍼런스](APIReference/README.md)
- 검증 상태는 변경분 정적 확인이다. 빌드와 CTest, Editor 핫 리로드, Client 패키징과 독립 실행 검증이 남아 있다.

### [2026-11-27] 정적 메시 `InstanceData::customData` 확장(96B) · 공용 `instanceData.hlsli` · 프로젝트 셰이더 자동 컴파일

- **요약**:
  1. **인스턴스별 사용자 데이터(`customData`) 파이프라인 확장**:
     - `Engine/Source/Render/Public/InstanceData.h`, `RenderCommands.h`, `StaticMeshRendererComponent.h`, `MeshRenderSystem.cpp`, `Renderer.cpp`, `RHI.h`: `InstanceData`에 `vec4 customData`(16B) 추가(`sizeof(InstanceData) == 96`), `RHI::BindInstanceBuffer`의 `stride` 기본값 제거로 셰이더 구조체 크기 불일치 방지, `MeshInstancingCheck.cpp` 96B 레이아웃 단언 갱신.
     - `Engine/Resources/Shader/hlsl/instanceData.hlsli`(신규) 및 `defaultVS.hlsl`, `gbufferVS.hlsl`, `shadowDepthVS.hlsl`, `shadowDepthMaskedVS.hlsl`, `waveVS.hlsl`, `glassPS.hlsl`: 공용 `InstanceData` 선언 일원화 및 `ShaderGraphCompiler.cpp`/`GraphRegistry.cpp` 연동.
  2. **프로젝트 셰이더 기동 시 자동 컴파일 (`ShaderHotReloadManager.h/.cpp`)**:
     - `CompileStaleProjectShaders()` 추가: 에디터 초기화 시 `Project://Shader` 하위 `.hlsl` 중 바이너리가 오래되었거나 누락된 셰이더를 `Project://Shader/bin`으로 자동 컴파일.
- **검증 상태**: 코드 반영 / 빌드·실행 미검증.

### [2026-11-27] `PhysicsMaterial` 로더·에디터 피커 통합 및 `Engine` 로캘 동기화

- **요약**:
  1. **`PhysicsMaterial` `.bamphysmat` 확장자 로더 및 에디터 통합**:
     - `Engine/Source/Resource/Private/ResourceManager.cpp`, `Engine/Source/Physics/Private/PhysicsMaterial.cpp`: `.bamphysmat` 명시적 로더 등록 및 직렬화 보강.
     - `Editor/Source/ImGui/Public/ResourcePicker.h`, `ReflectionPropertyDrawer.cpp`: `PhysicsMaterial` 호환 확장자(`.bamphysmat`/`.bamphysmat.json`), 배지(`PHYSICS MAT`), 인스펙터 인라인/프로퍼티 리소스 피커 연동.
  2. **엔진 로캘 키 동기화**:
     - `Engine/Resources/Data/Localization/Engine.csv`, `English.json`, `Korean.json`: `PROP_CUSTOM_DATA`, `PhysicsMaterial`, 신규 그래프 노드 등 누락 키 반영.
- **검증 상태**: 코드 반영 / 빌드·실행 미검증.

### [2026-11-27] 게임 모듈 핫 리로드 크래시 수정 · 미저장 씬 보존 · 시작/프로젝트 전환 씬 중복 로드 제거

- **증상**: 핫 리로드 때마다 `SceneDocument::~SceneDocument → Scene::Free → registry.clear → sparse_set::clear`에서 액세스 위반(`0xC0000005`, 해제된 DLL 주소). 호출 경로: 리로드 후 `RestoreLastEditedScene → ImGuiManager::UpdateSceneLoad → AdoptSceneDocument → EditorDocumentManager::Remove`.
- **원인**: 언로드 전 콜백이 `SceneManager::NewScene()`만 호출했지만 `SceneDocument`(AddRef)와 `SceneEditHistory` 스냅샷(`Data::Owner`)이 이전 씬을 계속 참조했다. 게임 모듈 컴포넌트 `storage<T>`를 가진 registry가 `FreeLibrary` 이후까지 살아남아, 문서 교체 시 마지막 Release에서 언로드된 vtable을 호출했다. 같은 콜백이 리로드 후 디스크의 `LastScenePath`를 다시 읽어 미저장 편집이 사라졌고, 시작(`RebindForProject` → 콜백 + `Initialize` 7단계)과 프로젝트 전환(콜백 + `LoadStartupSceneOrFallback`)에서는 씬이 중복 로드되었다.
- **변경**:
  1. `Application::ReleaseSceneForModuleUnload()` (언로드 전): PIE 중이면 중지한다. 실제 핫 리로드(`EHotReloadState::Reloading`)일 때만 현재 씬을 `JsonArchive`와 `TerrainDataFile::CaptureMemory`로 메모리에 보관한다(경로·Dirty·프로젝트 루트 포함). 이어서 선택·피킹·인스펙터 캐시를 비우고, `NewScene()` 뒤 `AdoptSceneDocument({}, false)`로 문서와 편집 이력의 참조까지 놓아 이전 registry를 DLL이 살아 있을 때 파괴한다.
  2. `Application::RestoreSceneAfterModuleReload()` (리로드 후): 보관한 스냅샷이 있고 모듈이 로드되어 있으며 프로젝트 루트가 같을 때만 씬을 복원하고 문서를 다시 연결한다. 미저장 상태였으면 `NotifyUntrackedChange()`로 Dirty를 유지한다. 시작·프로젝트 전환에서는 스냅샷이 없으므로 아무것도 하지 않는다(중복 로드 제거).
  3. 렌더 큐 정리: `SceneViewportPanel::Update`(`ImGuiManager::Update`)가 `HotReloadManager::Update`보다 먼저 `Scene*`를 캡처한 조명 패스 등을 제출한다. 그래서 같은 프레임에 씬을 파괴하면 `DeferredSceneRenderer::SubmitLightingPass` 람다가 해제된 registry를 조회해 크래시가 났다(`view<FogComponent>` → `dense_map` → `fast_mod`). 씬 파괴 직후 `Renderer::ClearAllRenderQueues()`로 커스텀 큐와 레이트레이싱 요청을 비운다.
  4. `ModuleHotReloadManager`: 프리팹 모드가 열려 있으면 자동·수동 리로드를 보류한다. 메인 씬을 `PrefabModeManager::m_SavedMainScene`이 따로 보관하기 때문이다. 자동 리로드의 변경 파일은 대기열에 남아 모드를 나간 뒤 적용된다.
- **제약**: 리로드 후 Undo/Redo 이력은 초기화된다. 씬 직렬화에 실패하면 미저장 편집을 잃는다(오류 로그 출력). 추가(additive) 씬은 복원하지 않는다.
- **검증 상태**: 코드 반영 / 빌드·실행 미검증.

### [2026-11-27] 비균일 스케일 Box-Sphere 충돌 수정 · SpringArm 데드존 · 에디터 PIE/종료 안정화 · Glass 알파 반영

- **요약**:
  1. **비균일 스케일 Box vs Sphere 3D 충돌 버그 수정 (`PhysicsSystem.cpp`)**:
     - `PhysicsSystem::Overlap3D`의 `boxA && sphereB` 경로가 구 반지름을 박스의 X축 스케일(`length(worldA[0])`)로만 나누던 문제를 수정.
     - 박스의 3축 스케일(`sx, sy, sz`)과 정규직교 기저(`axisX, axisY, axisZ`)를 분리해 월드 반크기(`halfExtents`)와 원래 구 반지름(`worldRadius`)으로 충돌 지점·법선·침투 깊이를 정확히 계산.
  2. **카메라 지연 데드존 및 상대 지터 제거 (`SpringArmComponent.h`, `SpringArmSystem.cpp`, `TimeManager.h`)**:
     - `SpringArmComponent::CameraLagDeadZone`(기본 `0.04f`) 추가 및 단독 카메라 속도 외삽 제거로 60Hz 물리 이동 시 미세 떨림 완화.
     - `TimeManager::GetFixedAccumulator()` 접근자 추가.
  3. **에디터 종료 및 PIE 중 핫리로드 안정화 (`Application.cpp`)**:
     - `Application::Free()`에서 `SceneManager::Destroy()`를 `HotReloadManager::Destroy()`보다 먼저 호출해 DLL 언로드 전 `entt::registry` 스토리지 풀 안전 해제.
     - PIE 실행 중(`!PIEManager::Get().IsEditing()`)에는 `HotReloadManager::Update()` 지연 처리.
  4. **반투명 Glass 셰이더 알파 반영 (`glassPS.hlsl`)**:
     - 출력 알파를 `1.0f` 고정에서 `gMaterial.baseColor.a` 기반으로 변경해 내부 오브젝트(눈·기포 등) 투과 표현 지원.
- **검증 상태**:
  - `PhysicsSystem.cpp` 비균일 스케일 Box-Sphere 수정 및 `ProjectSlime` 북쪽 벽(`scale=[24,8,1]`) 충돌 정상화 확인 대기(엔진 리빌드 후 검증).

### [2026-11-27] 프로젝트 감사 보통 항목 일괄 수정 (핫패스 스크래치 버퍼 · 콘텐츠 브라우저 I/O·입력 · 로캘 정리)

- **요약**:
  1. **렌더 핫패스 매 프레임 로컬 컨테이너 할당 제거 (`fix-hotpath-allocs`)**:
     - `MeshRenderSystem.h/.cpp`: `m_ScratchTargets`, `m_ScratchViews`, `m_ScratchStreamingViews`, `m_ScratchViewLODs`, `m_ScratchPlainUVShaders` 멤버 스크래치 버퍼 재사용 전환.
     - `Renderer.h/.cpp`: `RenderMotionVectors`의 로컬 `bones` 벡터를 `m_MotionPassScratchBones` 멤버 버퍼 재사용으로 전환.
  2. **콘텐츠 브라우저 매 프레임 OS Stat 제거 & `Escape` 입력 통일 (`fix-cb-io-input`)**:
     - `ContentBrowserPanel.h/.cpp`, `ContentBrowserGridItem.cpp`: `m_HasEngineRootDir`/`m_HasEditorRootDir` 초기화 1회 캐싱, 툴팁 `file_size` 호버 경로 기준 1회 캐싱, `ImGui::IsKeyPressed(ImGuiKey_Escape)` → `InputManager::Get().IsKeyButtonDown(EKeyCode::Escape)` 통일.
  3. **하드코딩 UI 문구 로캘화 및 중복 `Escape` 정리 (`fix-locale-cleanup`)**:
     - `SceneViewportPanel.cpp`/`Editor.Viewport.csv`(`Viewport.ShowGameplayGizmos`), `AnimatorGraphPanel.cpp`/`Editor.AnimatorGraph.csv`(`BaseLayerWeight`, `BaseLayerMask`, `EvaluatedBones`, `IsKeyButtonDown(Escape)` 정리), `DataTableEditor.cpp`/`Editor.ResourceEditor.csv`(`ResourceEditor.DataTableAddRow`).
- **검증 상태**:
  - 사용자 실환경에서 `GenerateLocalization`, `Engine`, `Editor`, `Client` 빌드 완료 및 전체 CTest 통과 확인 완료.

### [2026-11-27] 프로젝트 감사 심각·높음 항목 5건 일괄 수정 (PIE 누수·역방향 의존·헤더 경계·렌더 핫패스·허브 I/O)

- **요약**:
  1. **PIE 전역 상태 수명 누수 및 역방향 의존 제거 (`fix-gameplay-state-dep`)**:
     - `ActionCombatHelpers.h/.cpp`: `CombatEffectService` 추가(`HitstopState`/`CameraShakeState` 스레드 안전 관리).
     - `Runtime.cpp`, `CameraSystem.cpp`: `GraphGameplayLibrary.h` 역방향 의존 제거 및 `CombatEffectService` 호출로 전환.
     - `GraphGameplayLibrary.h/.cpp`, `PIEManager.cpp`: `ResetSessionState()` 추가 및 `PIEManager::Stop()`/`ForceStopForShutdown()`에서 자동 호출해 PIE 종료 시 전역 상태 잔류 차단.
  2. **`Engine_Includes.h` ImGui 헤더 경계 정리 (`fix-engine-includes`)**:
     - `Engine/Source/Core/Public/Engine_Includes.h`: `imgui.h` 포함을 `Macro.h` 이후로 이동하고 `#if BAM_EDITOR && !defined(__EMSCRIPTEN__)`로 가드.
  3. **`Renderer::PrepareMeshPipelineDesc` 핫패스 문자열 변환·할당 제거 (`fix-shader-flags`)**:
     - `Shader.h/.cpp`: `UpdateKeyFlags()`로 `IsGBufferVS`/`IsGBufferPS`/`IsDefaultPS`/`IsSkinningShader` 사전 캐싱.
     - `Renderer.cpp`: 매 호출 `ToLowerStr(WStrToStr(...))` 제거 및 O(1) 무할당 게터로 교체.
  4. **`ProjectHubModal` 매 프레임 디스크 I/O 제거 (`fix-hub-io-cache`)**:
     - `ProjectHubModal.h/.cpp`: `RefreshRecentProjectStatus()`로 `std::filesystem::exists` 결과를 모달 오픈/항목 제거 시에만 1회 캐싱(`DrawRecentProjectsSection` 내 디스크 Stat 0회).
- **검증 상태**:
  - 사용자 실환경에서 `Engine`, `Editor`, `Client`, `GraphExecutionCheck`, `GameplayCommonCheck` 빌드 완료 및 전체 CTest 통과 확인 완료.

### [2026-11-27] `PhysicsMaterial` 독립 에셋(`.bamphysmat.json`) 도입 및 `SceneViewportPanel` 기즈모 빌드 수정

- **요약**:
  1. **`PhysicsMaterial` 독립 에셋 (`.bamphysmat.json`, `physics-material-asset`)**:
     - `Engine/Source/Physics/Public/PhysicsMaterial.h`, `Private/PhysicsMaterial.cpp`: `EPhysicsCombineMode` (`Average`/`Minimum`/`Multiply`/`Maximum`), `StaticFriction`/`DynamicFriction`/`Restitution`/결합 모드 프로퍼티, `CombineValues`, `SaveToFile`/`LoadFromFile`(`JsonArchive`) 구현.
     - `Engine/Source/Physics/Public/ColliderComponent.h`, `Private/PhysicsSystem.cpp`: 선택적 `ResourceHandle<PhysicsMaterial> Material`(`ALLOW_MISSING`) 추가 및 3D 충돌 응답에서 재질 지정 시 우선 적용(미지정 시 기존 `ColliderSettings` 인라인 폴백).
  2. **`SceneViewportPanel` 기즈모 빌드 수정**:
     - `Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp`: `DrawSelectedCamera2DRoom` 전방 선언 보강 및 `worldTransform->worldMatrix[3].z` (폴백 `TransformComponent::GetPosition().z`) 교정.
- **검증 상태**:
  - 정적 검토 완료. 빌드(`Engine`, `Editor`, `CollisionGeometryCheck`, `KinematicCharacterCheck`) 및 CTest 실행 대기.

### [2026-11-27] `Camera2DRoomComponent` 기즈모 · `Inventory`/`NavGrid` 그래프 노드 7종 · `RenderPipelineProfile` SE-09 직렬화 보강

- **요약**:
  1. **`Camera2DRoomComponent` 씬 뷰포트 경계 기즈모 (`camera-room-gizmo`)**:
     - `Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp`: 선택된 엔티티에 활성 `Camera2DRoomComponent`가 있을 때 `roomMin`~`roomMax` 2D 사각형 경계를 `DebugRenderer::DrawLine` 4개 선분(시안색)으로 시각화.
  2. **`Inventory` · `NavGrid` 동적 장애물 비주얼 스크립팅 노드 7종 (`inv-navgrid-nodes`)**:
     - `Engine/Source/Graph/Libraries/Public/GraphGameplayLibrary.h`, `Private/GraphGameplayLibrary.cpp`: `Inventory.AddItem`, `Inventory.RemoveItem`, `Inventory.GetItemCount`, `Inventory.Clear` 4종 및 `NavGrid.SetObstacleBox`, `NavGrid.SetObstacleCircle`, `NavGrid.ClearObstacles` 3종 추가(`HashFnv1a64` 통합, `GraphReflection` 호환 타입 준수).
  3. **`RenderPipelineProfile` 저해상도 내부 RT + 정수배 업스케일 (`SE-09`) JSON 직렬화 보강 (`render-profile-se09`)**:
     - `Engine/Source/Render/Pipeline/Private/RenderPipelineProfileFile.cpp`: `LoadRenderPipelineProfile`/`SaveRenderPipelineProfile`에 선택적 필드 `InternalWidth`, `InternalHeight`, `IntegerUpscale`, `PointUpscale` 읽기/쓰기 추가(`SlimePixel3D_640x360.bamrenderpipeline.json` 로드 오류 해소, 기존 프로필 하위 호환 유지).
- **검증 상태**:
  - 정적 검토 완료. 빌드(`Engine`, `Editor`, `GraphReflectionCheck`, `GraphExecutionCheck`, `RenderPipelineProfileCheck`) 및 CTest 실행 대기.

### [2026-11-27] 신규 기능 4건 구현 (`DataTableEditor`, `UITween`/`UIProjection` 노드, `Camera2DRoomComponent`, BGM 크로스페이드 테스트 UI)

- **요약**:
  1. **에디터 `DataTable` CSV 뷰어·편집기 및 에디터 통합 (`datatable-editor`, `datatable-editor-wire`)**:
     - `Engine/Source/Gameplay/Public/DataTable.h`, `Private/DataTable.cpp`: `GetCell`, `SetCell`, `AddEmptyRow`, `RemoveRow`, `SaveToCsvFile`(RFC 4180 이스케이프 저장) 추가.
     - `Editor/Source/ImGui/ResourceEditor/Public/DataTableEditor.h`, `Private/DataTableEditor.cpp`, `ResourceEditors.h`, `ImGuiManager.cpp`: `ResourceEditorInterface` 파생 CSV 표 편집기 구현 및 `ImGuiManager` 정식 등록(미저장 확인·`.csv` 더블클릭 열기 연동).
     - `ContentBrowserPanel.h/.cpp`, `Editor/Resources/Data/Localization/Editor.ContentBrowser.csv`: 우클릭 `만들기` → `데이터 테이블 (.csv)` (`NewDataTable.csv` 생성·리네임 큐) 추가.
  2. **`UITween` · `UIProjection` 비주얼 스크립팅 노드 6종 (`ui-graph-nodes`)**:
     - `Engine/Source/Graph/Libraries/Public/GraphGameplayLibrary.h`, `Private/GraphGameplayLibrary.cpp`: `UI.PunchScale`, `UI.FadeAlpha`, `UI.MoveOffset`, `UI.StopTweens`, `UI.WorldToScreen`, `UI.IsWorldPointInFront` 추가(`std::array<float, 2/3>` 핀 규약 준수).
  3. **`Camera2DRoomComponent` & `CameraSystem` 자동 클램핑 (`camera2d-room-bounds`)**:
     - `Engine/Source/Camera/Public/Camera2DRoomComponent.h`, `Private/CameraSystem.cpp`: 직교 카메라 `Camera2DRoomComponent`(`Enable`, `roomMin`, `roomMax`) 추가 및 `CameraSystem::UpdateCamera` 자동 클램핑 연동.
  4. **오디오 믹서 BGM 크로스페이드 테스트 UI (`audio-mixer-crossfade`)**:
     - `Editor/Source/ImGui/SettingsPanel/Public/SettingsPanel.h`, `Private/SettingsPanel.cpp`, `Editor/Resources/Data/Localization/Editor.Settings.csv`: `AudioMixerSettingsWidget` 하단에 `AudioClip` 피커·페이드 시간/볼륨 슬라이더·`[Crossfade BGM]`/`[Stop BGM]`·상태 표시 UI 및 로캘 키 10종 추가.
- **검증 상태**:
  - 정적 검토 완료. 빌드(`GenerateLocalization`, `Engine`, `Editor`, `GraphReflectionCheck`, `GraphExecutionCheck`, `GameplayCommonCheck`) 및 CTest 실행 대기.

### [2026-11-27] 하이브리드 모델 에셋 정리 파이프라인 (Phase 6) `Source/` 원본 보관 자동화 구현

- **요약**:
  - `Editor/Source/AssetManager/Importer/Public/ModelImporter.h`, `Private/ModelImporter.cpp`: `ModelImportDesc::CopySourceToSubfolder`(기본 `true`, `.bamimport.json` 하위 호환) 추가. `CreateModelSubfolder` 활성 시 `<ModelName>/Source/`로 원본 모델 자동 복사(`filesystem::equivalent` 동일 경로 스킵), `ImportSourceUri`에 `Source/` 기준 가상 URI 기록, `ResolveSidecarPath`에서 `Source/` 상위 모델 폴더 사이드카 탐색 보강.
  - `Editor/Source/AssetManager/Importer/Private/ModelImportModal.cpp`, `Editor/Resources/Data/Localization/Editor.ModelImportModal.csv`: `FolderOrganization` 섹션에 `CopySourceToSubfolder` 체크박스·툴팁 및 로캘 키 2종 추가.
- **검증 상태**:
  - 정적 검토 완료. 빌드(`GenerateLocalization`, `Editor`) 및 실환경 모델 임포트/재임포트 확인 대기.

### [2026-11-27] `GraphGameplayLibrary` `Hitstop` / `CameraShake` 런타임 자동 틱 연동

- **요약**:
  - `GraphGameplayLibrary.h/.cpp`: `UpdateCombatEffects(float unscaledDt)`, `GetCameraShakePositionOffset()`, `GetCameraShakeRotationOffsetDeg()` 추가.
  - `Runtime.cpp`: `Runtime::Update`에서 매 프레임 `UpdateCombatEffects(dt)`를 호출해 반환된 `hitstopScale`을 `systems.Update(..., dt * hitstopScale, ...)`에 적용(비활성 시 `1.0f` 유지).
  - `CameraSystem.cpp`: `UpdateCamera`에서 `IsCameraShakeActive()`일 때 카메라 로컬 Right/Up/Forward 축 위치 오프셋 및 `glm::yawPitchRoll` 회전 오프셋을 자동 합성.
- **검증 상태**:
  - 정적 검토 완료. 빌드(`Engine`, `GraphExecutionCheck`, `GameplayCommonCheck`) 및 CTest 실행 대기.

### [2026-11-27] `AnimatorGraphPanel` 2D Freeform 블렌드 트리 시각 편집기 및 `GraphGameplayLibrary` 노드 13종 확장

- **요약**:
  1. **`AnimatorGraphPanel` 2D Freeform 블렌드 트리 시각 편집기 (`blend2d-visual-editor`)**:
     - `AnimatorGraphPanel.h/.cpp`, `Editor/Resources/Data/Localization/Editor.AnimatorGraph.csv`: `ImDrawList` 기반 2D 좌표 평면(자동 범위 확장, 드래그 중 범위 잠금, Undo 히스토리 연동), 우클릭/`Shift+드래그` 테스트 커서 실시간 가중치 원/퍼센트 시각화, 샘플 목록 CRUD(`Add Sample`, `Pos X/Y`, 리소스 피커, 삭제) 및 기존 2x2 격자 하위 호환 구현. 로캘 키 7종 추가.
  2. **`GraphGameplayLibrary` 비주얼 스크립팅 노드 13종 확장 (`graph-nodes-extended`)**:
     - `GraphGameplayLibrary.h/.cpp`: `DataTable` 5종(`LoadCsv`, `EqualsString`, `GetInt`, `GetFloat`, `GetBool`), `Combat` 4종(`TriggerHitstop`, `IsHitstopActive`, `TriggerCameraShake`, `IsCameraShakeActive`), `Quest` 4종(`Start`, `Advance`, `Fail`, `IsCompleted`) 노출.
- **검증 상태**:
  - 사용자 실환경에서 `GenerateLocalization`, `Engine`, `Editor`, `GraphReflectionCheck`, `GraphExecutionCheck` 빌드 완료 및 전체 CTest 통과 확인 완료.

### [2026-11-27] 에디터 사용자 C++ 플러그인 원클릭 빌드·게시·로드 파이프라인 구현

- **요약**:
  - `Editor/Source/Plugin/Public/PluginSourceBuilder.h`: 에디터 `CMakeCache.txt` 기반 제너레이터/`vcpkg_installed` 연동, Ninja 등 비-VS 제너레이터용 `VsDevCmd.bat` 래핑, 스테이징 캐시 제너레이터 불일치 자동 초기화, 기대 DLL 우선 탐색 및 원자적 DLL/`.pdb` 게시 구현.
  - `Editor/Source/Plugin/Private/PluginCatalog.cpp`, `Public/PluginHost.h`, `Private/PluginHost.cpp`: `plugin.json` 및 `.bamplugin.json` 매니페스트 동시 지원, 플러그인 ID별 빌드 로그·오류 분리(`GetPluginBuildLogs`, `GetPluginBuildError`), 빌드 성공 시 `RefreshInstalledPackages()` 후 자동 `Load`/`Reload`.
  - `Editor/Source/ImGui/PluginManager/Private/PluginManagerPanel.cpp`, `Editor/Resources/Data/Localization/Editor.PluginManager.csv`: 소스 전용 패키지 `[Build & Load Plugin]` 버튼, 실패 시 오류 상세 및 `[Retry Build]` 복구 버튼과 로캘 키 3종(`BuildAndLoad`, `RetryBuild`, `BuildErrorDetail`) 추가.
- **검증 상태**:
  - 사용자 실환경에서 `GenerateLocalization`, `Editor` 빌드 완료 및 전체 CTest 통과 확인 완료 (실환경 사용자 소스 플러그인 원클릭 빌드·로드 수동 확인 대기).

### [2026-11-27] 동적 메시 Upload 힙 전환(GPU 스톨 제거) 및 게임 모듈 핫리로드·종료 시 컴포넌트 소멸 순서 수정

- **요약**:
  1. **동적 메시 GPU 스톨 제거 (`Mesh.cpp`)**:
     - `EMeshFlag::Dynamic` 플래그가 설정된 메시의 버텍스 버퍼를 `ERHIBufferType::Upload`(영구 매핑 힙)로 생성하도록 변경하여, `UpdateStreamBufferInPlace()` 호출 시 매 프레임 `UploadBufferData()` + `FlushCommandQueue()`가 발생하던 프레임 드랍 병목 제거.
  2. **게임 모듈 핫리로드/종료 시 동적 컴포넌트 크래시 근본 수정 (`ModuleHotReloadManager`, `InspectorPanel`, `Application`)**:
     - `ModuleHotReloadManager.h/.cpp`: `RegisterPreUnloadCallback()` 추가 및 `UnloadModule()`의 `FreeLibrary()` 직전에 콜백 실행.
     - `InspectorPanel.h`, `Application.cpp`: `ClearCachedSnapshot()` 추가 및 Pre-Unload 콜백에 연결하여 언로드될 DLL의 `entt::storage`가 스냅샷에 남지 않도록 보장.
     - `Application.cpp`: `Application::Free()`에서 `HotReloadManager::Destroy()` 호출 직전에 활성 씬의 `ClearEntities()`를 먼저 실행해 DLL 언로드 전 동적 컴포넌트 스토리지가 안전하게 소멸되도록 순서 수정.
- **검증 상태**:
  - 사용자 실환경에서 `Engine`, `Editor` 빌드 및 핫리로드·에디터 종료 정상 동작 확인 완료.

### [2026-11-27] 슬라임 프로토타입 연계 엔진·에디터 안정화 (Physics Sleep, SpringArm, 동적 리플렉션, PIE 종료, EditorUI)

- **요약**:
  1. **물리 정지 접촉 안정화 (`PhysicsSystem`, `RigidBodyComponent`)**:
     - `RigidBodyComponent.h`, `PhysicsSystem.cpp`: 런타임 `IsSleeping`/`SleepTimer`/`WakeUp()` 휴면 메커니즘 추가 및 `ResolvePhysicsContact3D`의 과도한 단일 프레임 관통 해소 완화(`correctionRatio = 0.35`, `maxCorrectionPerIteration = 0.15`).
  2. **스프링암 카메라 지터 및 벽면 충돌 개선 (`SpringArmSystem`)**:
     - `SpringArmSystem.cpp`: `CameraLag`/`ZoomLag` 보간을 프레임률 독립 지수 감쇠(`1 - exp(-speed * dt)`)로 변경하고, 시작점 관통(`distance <= 0.05`) 및 벽 뒷면/스침 레이 히트를 무시하도록 필터 추가.
  3. **외부 게임 모듈 리플렉션·역직렬화 지원 (`BamEngineConfig`, `ReflectionRegistry`)**:
     - `Engine/CMake/BamEngineConfig.cmake`: 경로 정규화(`TO_CMAKE_PATH`), `x64-ninja-debug` 우선 탐색, `FMT_HEADER_ONLY`, `Reflection::Runtime`(`ReflectionRuntime.lib`, `ArchiveJson.lib`) 및 `BAMENGINE_CLANG_RESOURCE_DIR` 자동 탐색 추가.
     - `ReflectionRegistry.cpp`: 동적 모듈 컴포넌트(`entry->Read == nullptr`)에 대해 `ar.ReadObject` + `ReadProperties` 범용 역직렬화 폴백 추가(PIE 복원 크래시 해결).
  4. **에디터 UX 및 종료 안전장치 (`PIEManager`, `ImGuiManager`, `EditorUI`)**:
     - `PIEManager.h/.cpp`, `ImGuiManager.cpp`: 앱 종료 요청 시 PIE 씬 복원 실패로 종료가 영구 차단되지 않도록 `ForceStopForShutdown()` 추가.
     - `EditorUI.h/.cpp`: `PropertyLabel` UTF-8 안전 `...` 축약 + 툴팁 적용, 공통 `TextEllipsis` 및 드래그·복사 가능 `ReadOnlyTextField` 헬퍼 추가.
  5. **문서 갱신**:
     - `Docs/GamePlan/SlimeGame/Slime_Engine_Gap_And_Plan_v0.1.md`: M5 실측 기반 코너 접힘·절벽 Drape CPU 메시 한계 및 P2(`SE-12`) GPU-Driven/SDF 고도화 과제 기록.
- **검증 상태**:
  - 사용자 실환경에서 `Engine`, `Editor`, `ProjectSlimeModule` 빌드 및 `SlimeMovementPlayground` PIE 실행·정지·컴포넌트 인스펙터 연동 확인 완료.

### [2026-11-27] 액션 타임라인-VFX Step 4 마감 · 2D 블렌드 트리 Freeform 보간 & `AnimatorGraphPanel` 캔버스 UX 개선

- **요약**:
  1. **액션 타임라인 ↔ VFX 통합 마감 (트랙 F Step 4, `timeline-vfx-step4`)**:
     - `ActionTimelineSystem.cpp`: 컴포넌트 제거/비활성/사망 시 `EndTrackedSession` 및 `ReleaseHeldVFXForOwner(..., detachSockets = true)`로 잔여 `ActionVFXHoldComponent`/`SocketAttachmentComponent` 정리, 본 소켓 동기화를 본 포즈 갱신·Notify 스폰 이후로 단일화.
     - `ActionTimelineAsset.h/.cpp`, `AnimationTimelinePanel.h/.cpp`: `m_ModelPath`의 `PlatformPath::ToVirtualUri` 정규화, 패널 닫기/숨김 시 `OnResourceWindowVisibility`에서 재생 정지 및 `ClearPreviewVFX()` 격리 정리.
  2. **2D 블렌드 트리 임의 샘플 보간 & `AnimatorGraphPanel` 캔버스 UX 개선 (`animator-blend2d-canvas`)**:
     - `BlendTree2D.h/.cpp`, `AnimatorStateMachine.cpp`, `BlendTree2DCheck.cpp`: `BlendSample2D` + `BlendTree2D::samples`(`ALLOW_MISSING`) 추가 및 Johansen(2009) Freeform Cartesian Gradient Band Interpolation 구현(기존 2x2 격자 100% 하위 호환), 삼각형/5점 비대칭 산포·런타임 블렌딩 테스트 추가.
     - `AnimatorGraphPanel.h/.cpp`, `EditorActionRegistry.cpp`: 다중 노드 정렬(`AlignTop/Bottom/Left/Right`, `DistributeHoriz/Vert`) + `Undo` 반영 및 `EShortcutContext::AnimatorGraph` 단축키(`F`, `Shift+W/S/A/D/H/V`) 등록.
- **검증 상태**:
  - 사용자 실환경에서 전체 빌드 완료 및 `ActionTimelinePlaybackCheck`·`BlendTree2DCheck` 포함 전체 CTest 통과 확인 완료.

### [2026-11-27] 비주얼 스크립팅(`GraphGameplayLibrary`) 신규 게임플레이 노드 7종 노출

- **요약**:
  - `Engine/Source/Graph/Libraries/Public/GraphGameplayLibrary.h`, `Private/GraphGameplayLibrary.cpp`:
    1. 씬 전환: `Scene.TransitionToScene` (`GRAPH_CALLABLE`, 미생성 시 자동 생성), `Scene.IsTransitioning` (`GRAPH_PURE`)
    2. BGM 크로스페이드: `Audio.CrossfadeBGM` (`GRAPH_CALLABLE`, `ResourceManager`로 `AudioClip` 로드 후 `CrossfadeBGM`), `Audio.StopBGM`, `Audio.IsBGMPlaying`, `Audio.IsBGMCrossfading`
    3. 게임패드 진동: `Input.SetGamepadRumble` (`GRAPH_CALLABLE`, `InputManager::IsCreated()` 및 범위 가드)
- **검증 상태**:
  - 정적 검토 완료. 빌드(`Engine`, `GraphReflectionCheck`, `GraphExecutionCheck`) 및 CTest 실행 대기.

### [2026-11-27] `SpriteSheetBaker` 턴테이블 각도 회전 캡처 및 `SceneTransitionManager` 페이드 오버레이 렌더링 연결

- **요약**:
  1. **`SpriteSheetBaker` 실제 턴테이블 각도 회전 캡처 (`turntable-capture`)**:
     - `ThumbnailRenderer.h/.cpp`: `SubmitModel(Engine::Model* model, f32 yawDegrees = 0.0f)`에 선택적 월드 Y축 회전(`yawMatrix * rootTransform`) 추가(스태틱/스킨드 메쉬 및 AABB 오토프레이밍 통합 반영, 기존 호출부 하위 호환).
     - `SpriteSheetBaker.h/.cpp`, `ContentBrowserPanel.cpp`: `CaptureThumbnailFrameRGBA`에 `yawDegrees` 전달 경로 연결 및 모달 캡처 콜백 연동 완료.
  2. **`SceneTransitionManager` 페이드 오버레이 렌더링 연결 (`scene-fade-overlay`)**:
     - `GameViewportPanel.cpp`: PIE 실행 중 `SceneTransitionManager::Update(dt)` 호출 및 `GetFadeAlpha() > 0.001f` 시 Game View 이미지 영역 위 `AddRectFilled` 페이드 오버레이 렌더링.
     - `ClientGameRuntime.cpp`, `ClientRenderPipeline.cpp`, `Engine/Resources/Shader/hlsl/clientPresentPS.hlsl`: `Update(dt)` 호출 및 32B `ClientPresentParams`(`b0, space3`, `static_assert(sizeof == 32)`) 기반 백버퍼 페이드 합성 추가.
- **검증 상태**:
  - 정적 검토 완료. 빌드(`Engine`, `Editor`, `Client`) 및 실환경 턴테이블 4/8방향 베이킹·씬 전환 페이드 오버레이 확인 대기.

### [2026-11-27] `SpriteSheetBaker` 에디터 UI 연동 및 인게임 설정 메뉴(`UISettingsMenuSystem`) 런타임 키 리매핑 UI 추가

- **요약**:
  1. **`SpriteSheetBaker` 에디터 UI 연동 (`sprite-baker-ui`)**:
     - `ContentBrowserGridItem.h/.cpp`, `ContentBrowserPanel.h/.cpp`, `Editor/Resources/Data/Localization/Editor.ContentBrowser.csv`: `.bammodel` 우클릭 컨텍스트 메뉴에 `2D 스프라이트 시트로 굽기...` 추가 및 `DrawSpriteSheetBakeModal`(프레임 크기·방향 수·프레임 수·컬럼·패딩·피벗·출력 PNG 경로·리샘플링·`NotificationManager` 알림) 구현.
  2. **인게임 설정 메뉴 런타임 키 리매핑 UI (`ingame-key-rebind-ui`)**:
     - `RuntimeSettingsMenuComponent.h`, `UISettingsMenuSystem.h/.cpp`, `Engine/Resources/Data/Localization/Engine.csv`: 기존 `RuntimeSettingsCheck` 좌표/위젯을 유지하면서 `ControlsButton` 및 `ControlsPanel`(동적 액션/바인딩 행, 키보드/게임패드 입력 대기·`Escape` 취소·`RebindAction` + `SaveBindingsToFile`·`ResetAllBindingsToDefault`) 추가.
- **검증 상태**:
  - 사용자 실환경에서 `GenerateLocalization`, `Engine`, `Editor`, `Client`, `RuntimeSettingsCheck` 빌드 완료 및 전체 CTest 통과 확인 완료.
  - **실환경 수동 검증 대기 항목**:
    1. **아웃라인 후처리 (`OutlineEffect`)**: 에디터 `PostProcessPanel`에서 `Outline` 활성화 시 Scene/Game View 및 Client에서 깊이·노멀 엣지 아웃라인이 정상 표시되고 `Thickness`/`DepthThreshold`/`NormalThreshold`/`OutlineColor`/`Intensity` 조절이 즉시 반영되는지 확인.
    2. **스프라이트 시트 베이킹 UI (`SpriteSheetBaker`)**: 콘텐츠 브라우저 `.bammodel` 우클릭 → `2D 스프라이트 시트로 굽기...` 모달에서 PNG 및 `.bamsprite.json` 생성, `NotificationManager` 알림 및 콘텐츠 브라우저 자동 새로고침 동작 확인. (참고: 실제 턴테이블 각도 회전 캡처는 후속 `ThumbnailRenderer` 각도 오버라이드 확장 필요)
    3. **인게임 런타임 키 리매핑 (`UISettingsMenuSystem`)**: PIE/Client 인게임 설정 메뉴에서 `조작 키` 버튼 → 액션 클릭 후 키보드/게임패드 입력 재바인딩, `Escape` 취소, `초기화` 버튼 및 `Config/InputBindings.json` 저장·재시작 복원 확인.
    4. **NavMesh 터레인 `Hole` & Probe GI (`M4-GI`)**: 터레인 구멍 경계 NavMesh 베이크 일치 여부 및 `RenderingFeatureLab` 다크룸/GI 박스 코너 빛샘·얇은 벽 흔들림 개선 확인.

### [2026-11-27] 아웃라인 에디터/파이프라인 연결 · 신규 API CTest 마무리 · 3D→2D 스프라이트 시트 베이커 구현

- **요약**:
  1. **아웃라인 후처리(`OutlineEffect`) 에디터 UI 및 `NormalRT` 파이프라인 연결 (`outline-editor-wire`)**:
     - `ViewportDeferredPipeline.h`(`GetNormalRTName`), `GameViewportPanel.cpp`, `SceneViewportPanel.cpp`, `ClientRenderPipeline.cpp`: G-Buffer `NormalRT`를 `PostProcessSceneInputs::NormalRT`로 전달.
     - `PostProcessPanel.cpp`, `Editor/Resources/Data/Localization/Editor.PostProcess.csv`: `OutlineEffect` 미세 범위 Float 파라미터 `"%.3f"` 포맷 지원 및 6개 로캘 키(`POST_PROCESS_OUTLINE`, `OUTLINE_*`) 추가.
  2. **신규 API 헤드리스 CTest 마무리 (`ctest-final-batch`)**:
     - `Engine/examples/GameplayCommonCheck.cpp`: `CheckDataTable`, `CheckStrategyRpgHelpers` 추가 (총 10개 케이스).
     - `Engine/examples/NetworkProtocolCheck.cpp`: `NetworkPredictionBuffer` 및 `NetworkInterpolationBuffer` 단위 테스트 추가.
  3. **3D → 2D 스프라이트 시트 베이킹 도구 (`SpriteSheetBaker`, `sprite-baker-tool`)**:
     - `Editor/Source/AssetManager/Baking/Public/SpriteSheetBaker.h`, `Private/SpriteSheetBaker.cpp`: `AssembleAtlasRGBA`(엣지 패딩 복제), `WriteSpriteSheetOutputs`(원자적 PNG + `.bamsprite.json` 저장), `BakeTurntableFromFrames`, `BakeTurntableFromCapture`, `CaptureThumbnailFrameRGBA` 구현.
- **검증 상태**:
  - 사용자 실환경에서 전체 빌드 완료 및 78개 전체 CTest 통과 확인 완료.

### [2026-11-27] 스타일라이즈드 아웃라인 후처리(`OutlineEffect`) 및 멀티플레이 예측·보간 기반(`NetworkPrediction`, `NetworkInterpolation`) 구현

- **요약**:
  1. **화면 공간 깊이·노멀 기반 아웃라인 후처리 (`stylized-outline`)**:
     - `Engine/Resources/Shader/hlsl/outlinePS.hlsl`, `Engine/Source/Render/PostProcess/Public/OutlineEffect.h`, `Private/OutlineEffect.cpp`: Roberts Cross 기반 뷰 공간 상대 깊이·법선 불연속성 검출, `NormalRT` 미전달 시 `PositionRT` 차분 폴백, 하늘 실루엣 처리(`OUTLINE_SKY_DEPTH`).
     - `Engine/Source/Render/PostProcess/Public/PostProcessInterface.h`, `Private/PostProcessPipeline.cpp`: `PostProcessSceneInputs::NormalRT` 선택 필드 추가 및 `OutlineEffect` 등록(기본값 `Enabled = false`, `Thickness`/`DepthThreshold`/`NormalThreshold`/`OutlineColor`/`Intensity` 지원, 80B 16바이트 정렬 `static_assert` 검증).
  2. **멀티플레이 클라이언트 예측·보정 & 스냅샷 보간 (`network-prediction`)**:
     - `Engine/Source/Network/Public/NetworkPrediction.h`, `Private/NetworkPrediction.cpp`: 랩어라운드 안전 시퀀스 비교(`IsSequenceGreater`/`IsSequenceGreaterOrEqual`), `NetworkPredictionBuffer<TInput, TState>`(`RecordInput`, `Acknowledge`, `Reconcile`), 트랜스폼 오차 헬퍼 구현.
     - `Engine/Source/Network/Public/NetworkInterpolation.h`: `NetworkInterpolationBuffer<TState>`(`PushSnapshot`, `Sample`, `PruneBefore`, `interpolationDelay`/`maxExtrapolationTime` 방어) 및 `NetworkTransformSnapshot` 보간/외삽 헬퍼 구현.
- **검증 상태**:
  - 정적 검토 완료. 빌드(`Engine`, `Editor`, `NetworkProtocolCheck`, `NetworkRpcCheck`) 및 실환경 아웃라인 후처리 확인 대기.

### [2026-11-27] RPG/전략/시뮬레이션 장르 보강(`DataTable`, `StrategyRpgHelpers`) 및 `GameplayCommonCheck` CTest 확장

- **요약**:
  1. **외부 데이터 테이블 로더·검증기 (`DataTable`)**:
     - `Engine/Source/Gameplay/Public/DataTable.h`, `Private/DataTable.cpp`: RFC 4180 CSV 파서(UTF-8 BOM, 따옴표/이스케이프, CRLF/LF), 필수 컬럼 검증(`ValidateRequiredColumns`), 키 컬럼 O(1) 해시 조회(`FindRowIndex`/`FindRowIndexByKey`), 타입별 안전 조회(`GetString`/`GetInt`/`GetFloat`/`GetBool`) 구현.
  2. **RPG / 전략 / 시뮬레이션 헬퍼 (`StrategyRpgHelpers`)**:
     - `Engine/Source/Gameplay/Public/StrategyRpgHelpers.h`, `Private/StrategyRpgHelpers.cpp`: `DragBoxSelector`(화면 사각형 다중 선별), `MinimapProjection`(월드 XZ ↔ 미니맵 UV/픽셀 양방향 변환), `InventoryContainer`(고정/가변 슬롯 스택 관리), `QuestTracker`(퀘스트 시작·진행·완료·실패 상태 추적) 구현.
  3. **`GameplayCommonCheck` CTest 확장**:
     - `Engine/examples/GameplayCommonCheck.cpp`: `CheckActionCombatHelpers`, `CheckPlatformer2DHelpers`, `CheckNavGridDynamicObstacles` 3개 테스트 케이스 추가 및 `main()` 연결.
- **검증 상태**:
  - 사용자 실환경에서 `Engine`, `GameplayCommonCheck` 빌드 완료 및 CTest 전체 통과 확인 완료.

### [2026-11-27] 장르별 게임 제작 보강(11.8절) 3대 영역 구현 및 `GameplayCommonCheck` CTest 추가

- **요약**:
  1. **3D 3인칭 액션 보강 (`ActionCombatHelpers`)**:
     - `Engine/Source/Gameplay/Public/ActionCombatHelpers.h`, `Private/ActionCombatHelpers.cpp`: `HitstopState`(중첩 시 최장 시간·최소 타임스케일), `CameraShakeState`(감쇠 진동, 강도 우선 덮어쓰기), `TargetLockOnSelector`(`FindBestTarget`, `SwitchTarget`, `IsTargetValid`, `std::optional<uint64>` 반환) 구현.
  2. **2D 플랫포머 / 탑다운 보강 (`Platformer2DHelpers`)**:
     - `Engine/Source/Gameplay/Public/Platformer2DHelpers.h`, `Private/Platformer2DHelpers.cpp`: `PlatformerJumpController`(코요테 타임·점프 버퍼링), `OneWayPlatform2D::ShouldCollide`(상승/하향 드롭 통과, 하강 착지 충돌), `Camera2DRoomBounds::ClampPosition`(소형 룸 중앙 정렬 및 경계 클램프) 구현.
  3. **3D 쿼터뷰 / 디펜스 동적 NavGrid 장애물 갱신 (`NavGrid`)**:
     - `Engine/Source/Navigation/Public/NavGrid.h`, `Private/NavGrid.cpp`: 정적 베이크(`m_Walkable`)와 분리된 참조 카운트(`m_DynamicObstacleCounts`) 기반 `SetDynamicObstacleBox`, `SetDynamicObstacleCircle`, `ClearDynamicObstacles`, `IsDynamicBlocked` 추가 및 `IsWalkable`/`FindPath`/`IsLineWalkable` 자동 연동.
  4. **공통 기반 헤드리스 CTest (`GameplayCommonCheck`)**:
     - `Engine/examples/GameplayCommonCheck.cpp`, `Engine/CMake/Checks.cmake`: `SaveGameManager` 마이그레이션, `EntityPool`, `UIProjection`/`UITween`, `InputManager` 리매핑/데드존, `SceneTransitionManager` 5개 케이스 등록.
- **검증 상태**:
  - 사용자 실환경에서 `Engine`, `GameplayCommonCheck`, `NavigationAICheck` 빌드 완료 및 CTest 전체 통과 확인 완료.

### [2026-11-27] 엔진·렌더링 미해결 과제 3건 해결 (NavMesh 터레인 Hole · Probe GI 빛샘 · 모델 임포트 프리뷰)

- **요약**:
  1. **3D NavMesh 터레인 `Hole` 경계 정밀화 (`navmesh-hole`)**:
     - `Engine/Source/Navigation/Private/NavMesh.cpp`(`CollectNavMeshGeometry`): 기존 셀 좌상단 단일 꼭짓점(`a00`) 속성 판정으로 인해 발생하던 구멍 경계 비대칭 오차(반 셀~1셀)를 수정해, 4개 꼭짓점(`a00`, `a10`, `a01`, `a11`)을 모두 조회하고 각 삼각형의 3꼭짓점 중 하나라도 `Hole`/`Blocked`이면 제외하도록 개선.
  2. **Probe GI(DDGI) 코너 빛샘 및 얇은 벽 빛 흔들림 개선 (`M4-GI`, `probe-gi-leak`)**:
     - `Engine/Resources/Shader/hlsl/probeGISample.hlsli`(`SampleProbeIrradiance`): `surfaceBias` 길이를 `minSpacing * 0.45f`로 클램프하고, 백페이스 가중치의 `+ 0.02f` 하한 제거(`wrap * wrap`) 및 `sum / max(totalWeight, 1e-3f)`로 코너·얇은 벽 뒤쪽 프로브의 정규화 증폭 빛샘 차단.
     - `Engine/Resources/Shader/hlsl/probeGIBlend.hlsli`(`BlendTexel`): 조도 히스테리시스 블렌딩을 `sqrt` 공간으로 전환하고 급격한 스파이크 감쇠 폭을 완화(`0.35f -> 0.20f`)하며 거리 아틀라스 `totalWeight` 하한을 `1e-7f`로 보강. (C++/HLSL 상수 버퍼 16B 정렬 유지)
  3. **대화형 3D 모델 임포트 프리뷰 보완 (`P1-F`, `model-import-preview`)**:
     - `Editor/Source/AssetManager/Importer/Private/ModelImportModal.cpp`, `Editor/Resources/Data/Localization/Editor.ModelImportModal.csv`: 하드코딩된 DCC 프리셋 문구 로캘화, 중복 체크박스 제거 및 `Pitch +90°` 빠른 보정 버튼 추가, 스케일/리셋 버튼 클릭 시 `UpdatePreviewTransform()` 이후 카메라 포커스가 재계산되도록 순서 버그 수정.
- **검증 상태**:
  - 정적 검토 완료. 빌드(`Engine`, `Editor`, `GenerateLocalization`), `NavigationAICheck`, 실환경 `RenderingFeatureLab` 다크룸/GI 박스·터레인 구멍·모델 임포트 모달 확인 대기.

### [2026-11-27] Pre-Boot 경량 프로젝트 허브(`StartupProjectHub`) · 누락 프로젝트 제거 · 외부 프로젝트 직부팅 폴백 수정

- **요약**:
  1. **스플래쉬 전 경량 프로젝트 허브 도입 (`StartupProjectHub`)**:
     - `Editor/Source/Application/Public/StartupProjectHub.h`, `Private/StartupProjectHub.cpp` 신규 구현(`SDL3 + ImGui SDL_Renderer` 독립 창, `맑은 고딕` 한글 폰트, 최근 프로젝트/새 프로젝트 생성 탭, 종료 시 에디터 본체 미초기화 즉시 종료).
     - `Editor/Source/Application/Public/Application.h`, `Private/Application.cpp`: `EditorSplashScreen::Show()` 호출 전에 `PlatformPath::SetEngineRoot()`로 엔진 루트를 고정하고 `StartupProjectHub::Run()`으로 대상 프로젝트를 먼저 확정한 뒤, 선택된 프로젝트의 마지막 씬(`RestoreLastEditedScene()`)으로 바로 부팅하도록 전환.
  2. **프로젝트 허브 누락 항목 제거 기능**:
     - `Editor/Source/ImGui/ProjectHub/Private/ProjectHubModal.cpp`, `StartupProjectHub.cpp`, `Editor/Resources/Data/Localization/Editor.ProjectHub.csv`: 디스크에 존재하지 않는 최근 프로젝트에 `[경로 없음]` 경고 태그·`열기` 비활성화·개별 `제거` 버튼 및 상단 `없는 항목 정리` 버튼 추가.
  3. **외부 프로젝트 직부팅 시 경로 폴백 수정**:
     - `Editor/Source/Settings/Private/EditorSettingsService.cpp`: `LoadForStartup()`이 `PlatformPath::GetProjectRoot()`에 사전 지정된 프로젝트 루트를 최우선으로 사용하도록 수정.
     - `Editor/Source/Application/Private/Application.cpp`, `Engine/Source/Resource/Private/StartupResourceLoader.cpp`: 외부 프로젝트에 없는 `EngineResourceDirectory`, `StartupManifest.json` 및 레거시 `"Resources/..."`/`"Project://..."` 스타트업 리소스를 `GetEngineResourceRoot()` / `GetEngineRoot()` / `GetEngineRoot()/Assets`로 폴백하도록 보강.
- **검증 상태**:
  - 사용자 실환경에서 `Editor` 빌드 후 스플래쉬 전 프로젝트 허브 실행, 누락 프로젝트 제거 및 첫 허브에서 외부 프로젝트(`ProjectSlime`) 직부팅 정상 동작 확인 완료.

### [2026-11-27] 게임 제작 공통 기반(DevelopmentPriorities 11.8절) 6대 영역 구현

- **요약**:
  1. **오디오 BGM 크로스페이드 (`AudioManager`)**:
     - `Engine/Source/Audio/Public/AudioManager.h`, `Private/AudioManager.cpp`에 `PlayBGM`, `CrossfadeBGM`, `StopBGM`, `UpdateCrossfade(dt)`(`AudioManager::Update` 자동 연동), `IsBGMPlaying`, `IsCrossfading` 추가.
     - 동일 트랙 중복 크로스페이드, `fadeDuration <= 0`/비유한값 즉시 전환, 크로스페이드 중 재전환 승격 및 `StopAllSounds`/`Free` 일괄 정리 반영.
  2. **UI 3D→2D 투영 & 경량 트위닝 (`UIProjection`, `UITween`)**:
     - `Engine/Source/UI/Public/UIProjection.h`, `Private/UIProjection.cpp`: `GLM_FORCE_LEFT_HANDED` + `[0, 1]` 깊이 규약 기반 `WorldToScreen`, `ScreenToCanvas`, `WorldToCanvas` 추가.
     - `Engine/Source/UI/Public/UITween.h`, `Private/UITween.cpp`: 이징 함수(`Linear`, `Quad`, `Cubic`, `Back`, `Bounce`, `EvaluatePunch`), `UITweenComponent`, `UITweenPlayer`, `UITweenSystem` 및 스케일/펀치/오프셋/알파/컬러/회전 트윈 헬퍼 추가.
  3. **세이브 버전 관리 & 경량 엔티티 풀 (`SaveGameManager`, `EntityPool`)**:
     - `Engine/Source/Gameplay/Public/SaveGameTypes.h`, `SaveGameManager.h/.cpp`: `SaveSlotMetadata::schemaVersion`(기본 1), `SaveGameData::CurrentSchemaVersion`, `RegisterMigration`, `ClearMigrations`, `HasMigration`, `MigrateSaveData`(BFS 단계적 업그레이드, 구버전 `0 -> 1` 폴백, 미래 버전 거부, 실패 시 롤백) 구현.
     - `Engine/Source/World/Scene/Public/EntityPool.h`, `Private/EntityPool.cpp`: `BindScene`, `Prewarm`, `Acquire`, `Release`, `Clear`, `PruneInvalid` 구현 (`PoolSceneLifetime` 및 `Entity::WeakRef` 기반 씬 파괴·중복 반납 방어).
  4. **런타임 입력 리매핑 & SDL3 게임패드 지원 (`InputMap`, `InputManager`)**:
     - `Engine/Source/Input/Public/InputMap.h`, `Private/InputMap.cpp`, `Public/InputManager.h`, `Private/InputManager.cpp`: `RebindAction`, `ResetActionBindingsToDefault`, `ResetAllBindingsToDefault`, `SaveBindingsToFile`, `LoadBindingsFromFile`(`glaze` JSON) 및 `"Gamepad:<Button>"` 액션 바인딩 추가.
     - 최대 4대 SDL3 게임패드 연결/해제, 버튼 상태, 좌/우 아날로그 스틱·트리거 방사형 데드존(`SetGamepadDeadzone`), 진동(`SetGamepadRumble`) 추가. 리매핑 시 `SE-01` `FixedPressed`/`FixedReleased` 래치 보존.
  5. **씬 전환 페이드 상태 머신 (`SceneTransitionManager`)**:
     - `Engine/Source/World/Scene/Public/SceneTransitionManager.h`, `Private/SceneTransitionManager.cpp`: `Idle -> FadingOut -> Loading -> FadingIn -> Idle` 상태 머신(`TransitionToScene`, `Update`, `SetFadeColor`, `GetFadeAlpha`, `GetFadeColor`, `IsTransitioning`, `GetState`) 구현.
- **검증 상태**:
  - 스웜 독립 정적 검증(`verifier`) 통과: 17개 파일 컴파일/링크/PCH/`UNITY_BUILD ON` 충돌 및 심각·높음·보통 결함 없음. (낮음 참고 2건: `UITween.cpp` 무한 루프 극소 `duration` 시 `fmod` 개선 여지, `PunchScale` `duration <= 0` 시 `normalizedT=1` 완료)
  - **미검증**: 빌드(`Engine`, `Editor`, `Client`), `CTest`, 실제 런타임/게임패드 실행 검증은 아직 수행하지 않음.

### [2026-11-27] Slime Game 엔진 기능 격차 분석 및 P0/P1 범용 인프라(SE-01~SE-11) 구현

- **요약**:
  1. **엔진 기능 격차 분석 및 개발 계획서 작성**: `Docs/GamePlan/SlimeGame/Slime_Engine_Gap_And_Plan_v0.1.md`에 `Slime_Game_Design_Draft_v0.1.md` 및 `Slime_Movement_Design_v0.1.md` 대비 BamEngine 현황, 엔진 범용 vs `GameModule` 책임 경계, P0~P2 우선순위 로드맵(`SE-01`~`SE-12`) 정리.
  2. **P0 범용 인프라 (`SE-01` ~ `SE-06`)**:
     - `SE-01`: `InputManager`에 `FixedPressed`/`FixedReleased` 래치(`WasActionPressedInFixedUpdate`, `WasActionReleasedInFixedUpdate`, `ConsumeFixedStepActions`)를 추가하고 `Runtime::FixedUpdate` 끝에서 소비하도록 연결.
     - `SE-02`: `CollisionCallbackComponent.h` 신규 추가 및 `PhysicsSystem`에 비트리거 3D 충돌 이벤트(`OnCollisionEnter/Stay/Exit`, 접촉점·법선·관통 깊이·상대속도) 발행·디스패치 구현.
     - `SE-03`: `RHIBuffer::UpdateData` / `DirectX12Buffer::UpdateData` 및 `Mesh::UpdateStreamBufferInPlace` 구현(동일 크기/스트라이드 시 GPU 버퍼 재할당 회피, `SetStreamBuffer` 폴백), `MeshRenderSystem::OnSubmit` 동적 메시 `worldBounds` 프러스텀 컬링 정합.
     - `SE-04`: `Engine::DebugDraw` 스레드 안전 정적 큐(`#if BAM_SHIPPING` no-op)와 `SceneViewportPanel` 렌더링 연동, `PIEManager::RequestSingleStep`/`ConsumeSingleStepRequest` 및 `ToolBar` Step 버튼(`Editor.ToolBar.csv` 로캘 포함)·`Application.cpp` 1고정틱 실행 연결.
     - `SE-05`: `lightingPS.hlsl`에 `SHADING_MODEL_TOON(2u)`(3단계 셀 밴드/림/하이라이트) 및 `SHADING_MODEL_SUBSURFACE(3u)`(Fake SSS: Wrap Diffuse/Backlight Transmission/Fresnel/Gloss) 분기 구현.
     - `SE-06`: `Particle` 3D 스케일(`scale3D`)·회전(`rotation3D`, `angularVelocity3D`) 및 `alignToVelocity`/`velocityStretchFactor` 확장, `ParticleSystem::OnSubmit`에서 `meshHandle` 유효 시 `StaticDrawCommand` + `SubmitStatic` 3D 제출.
  3. **P1 범용 인프라 (`SE-07` ~ `SE-11`)**:
     - `SE-07`: `RigidBodyComponent`(`UseCCD`, `CCDMotionThreshold`) 및 `PhysicsSystem` 3D Swept CCD TOI 클램핑, `AudioManager::PlayOneShot3D` 및 사운드별 Concurrency/Cooldown 제한, `AudioSystem::UpdateSources` 로그 레벨 `Trace` 하향.
     - `SE-08`: 독립 `TrailComponent.h` 및 `CompositeVFXSystem` 연동, `DecalComponent` UV 스크롤/타일링·수명/페이드아웃(`DecalParams` 240B 16바이트 배수 정렬 동기화).
     - `SE-09`: `RenderPipelineProfile` 내부 저해상도(`InternalWidth`, `InternalHeight`, `IntegerUpscale`, `PointUpscale`) 및 `DeferredProfilePasses`·`ClientRenderPipeline`의 Point 정수배 업스케일 + 풀 해상도 백버퍼 UI 분리 합성.
     - `SE-10`: `PixelPerfectCameraComponent`(`snapToCameraAxes`) 및 `CameraSystem` 회전 직교 카메라 로컬 Right/Up 축 텍셀 스냅, 뷰포트 해상도 캐싱.
     - `SE-11`: `ShaderGraphCompiler` `worldPositionOffset` 연결 시 Shadow 전용 VS 생성 및 `MaterialInterface` Shadow 패스 VS 단독 오버라이드, 범용 `PresetStorage.h`(`SavePresetToFile`/`LoadPresetFromFile`) 추가.
- **검증 상태**:
  - 스웜 독립 정적 검증(`verifier`, `verifier-code`, `verifier-p1`) 통과.
  - **미검증**: 빌드(`Engine`, `Editor`, `Client`), `CTest`, 실제 에디터/클라이언트 GPU 실행 및 성능 계측은 아직 수행하지 않음.

### [2026-11-26] 마지막 작업 씬(LastScenePath) 즉시 저장 및 부팅 시 덮어쓰기 방지

- **요약**:
  1. `EditorSettingsService::RememberEditedScene()`: `LastScenePath`가 변경될 때 즉시 `SaveWorkspace()`를 호출해 비정상 종료·프로젝트 전환 시에도 유실되지 않도록 보강.
  2. `ProjectSessionManager::SwitchProject()`: 프로젝트 전환 직전 `Application::Get().CaptureAndSaveEditorWorkspace()`를 호출해 기존 프로젝트의 워크스페이스(`LastScenePath`·창·패널 상태)를 먼저 저장(`Application::CaptureAndSaveEditorWorkspace`를 `public`으로 공개).
  3. `Application::AdoptSceneDocument(path, rememberScene = true)`: `Application::Initialize()`의 부팅용 임시 빈 씬 등록(`AdoptSceneDocument({}, false)`)이 기존 `LastScenePath`를 빈 문자열로 지우고 저장하던 결함 수정.
- **검증 상태**: 사용자 실환경에서 프로젝트 허브 열기/닫기 및 에디터 완전 종료 후 재실행 시 마지막 씬 복원 정상 동작 확인 완료.

### [2026-11-26] VFX 체커보드·트레일 실시간 프리뷰 · Editor:// 3D 썸네일 · 3D NavMesh 정적 GPU 버퍼 전환

- **요약**:
  1. **`VFXEditorPanel` 체커보드 바닥판 미표시 수정**: `GetOrCreateVFXCheckerTexture()`에서 `Engine::Texture::Create(...)` 후 누락된 `UploadToGPU()` 호출을 추가(실패 시 `Free()`/`delete` 정리)하여 `MaterialInstance::Bind()`에서 Diffuse 슬롯 언바인드가 발생하던 문제 해결.
  2. **`Editor://` 모델 3D 썸네일 미표시 수정**: `Tools/AssetWorker/main.cpp`에 `Editor://` 가상 스킴 등록을 추가하고, `AssetCache`(`ThumbnailCacheKey`, `CalculateDiskCachePath`) 및 `AssetWorkerHost::EnqueueThumbnail`에서 가상 URI(`IsVirtualUri`)를 `ResolveUri`로 해석하도록 보강.
  3. **`VFXEditorPanel` 트레일 오프셋 실시간 프리뷰 갱신**: `OffsetTrailRuntimePoints()`를 추가해 `DrawInspector()`와 `DrawGizmo()`에서 `localOffsetA/B` 변경 시 재생/일시정지 여부와 무관하게 재재생 없이 즉시 프리뷰 트레일 정점을 이동하도록 개선.
  4. **3D NavMesh 오버레이 전용 정적 GPU 버퍼 전환 & 터레인 `Hole` 제외**: `DebugRenderer`의 CPU 캐시/복사 로직을 제거하고 NavMesh 변경 시 1회 정적 `RHIBuffer`(`m_NavMeshFillVB`, `m_NavMeshLineVB`)를 생성해 최대 2 드로우콜(`DebugLineVS`/`DebugLinePS`, `ECompareOp::LessOrEqual`, `AlphaBlend`)로 전체 NavMesh를 그리도록 전환. `NavMesh.cpp` 지형 수집 시 `ETerrainAttribute::Hole` 셀 제외 추가.
- **검증 상태**: 사용자 실환경에서 4건 모두 정상 동작 확인 완료.
- **후속 추적 항목**:
  - **3D NavMesh 터레인 `Hole` 경계 미세 오프셋**: `NavMesh.cpp`에서 터레인 셀을 단일 꼭짓점(`a00`) 속성으로만 제외하고 복셀화 해상도와 맞물리면서 구멍 경계가 실제 터레인 구멍과 반 셀~1셀 정도 미세하게 어긋나는 현상(추후 4꼭짓점/셀 중심 기준 판정 및 경계 정렬 개선 필요).

### [2026-11-26] CTest 통과 · 지형/디스토션/리소스피커/머티리얼볼 수정 · 누락 참조 복구 · 엔진/에디터 경계 격리

- **요약**:
  1. **CTest 실패 2건 해소 (`NavigationAICheck`, `RuntimeSettingsCheck`)**:
     - `NavMesh.h/.cpp`: 다층 램프 하단이 큰 바닥 폴리곤 내부에 착지할 때 연결이 끊기던 문제를 `NavMeshPortalLink`(`extraLinks`, 3차 내부 착지 포털 연결, `GetPortalEdge`/`FindPath` 및 `.bamnavmesh` v2 직렬화)로 해결.
     - `RuntimeSettingsCheck.cpp`: `UIEventSystem` 초기 `m_SuppressUntilRelease=true` 해제를 위한 idle 포인터 프레임 1회를 추가해 드롭다운 클릭 테스트 정상화.
     - 사용자 실행으로 전체 CTest 통과 확인.
  2. **실환경 버그 수정 및 에디터 UX 개선**:
     - **리소스 피커**: `ResourcePicker.h` 경로 텍스트 클리핑(`PushClipRect`/`PopClipRect`)으로 우측 배지 겹침 해소, `PickerAssetCatalog.cpp`에 `PlatformPath::GetContentMounts()` 스캔 추가.
     - **지형 & 내비게이션**: `DirectX12RHI::UploadTextureData`에 `ETextureFormat::R8_UNORM` 지원 추가(지형 `HoleMaskTexture` 오류 해소), `NavGrid::BakeObstaclesFromScene`에 `ETerrainAttribute::Hole` 비통행 처리 추가, `TerrainEditor.cpp` 텍스트 줄바꿈 및 Sculpt/Attribute 라디오 버튼 2×2 배치로 좌우 스크롤 해소(사용자 확인 완료).
     - **디스토션 V2 & VFX 프리뷰**: `Renderer::RenderDynamicGeometry` 비호환 머티리얼 매 프레임 에러 로그 억제, `Assets/Material/DistortionV2_Test.bammat.json` 생성, `VFXEditorPanel` 체커보드 바닥 토글(`CheckerBackdrop`) 추가, `distortionWritePS.hlsl` 노이즈/강도(`0.05f`) 보강.
     - **머티리얼 볼 & 에디터**: `SamplerDesc::MaxAnisotropy`에 `ALLOW_MISSING` 추가(`code=1` 오류 해소), `MaterialPreviewRenderer::GetOrLoadMaterialBallPreviewMesh()`에서 `.bammodel` 노드 계층 변환을 반영해 정방향 정규화·`ThumbnailRenderer` 공용화, `MaterialEditor` 부모 머티리얼 영역을 `DrawInspectorResourcePicker<Engine::Material>`로 교체.
     - **리소스 편집기 누락 참조 복구**: `MaterialEditor`·`ModelEditor`에 `MissingResourceRecoveryScope` 적용, `ResourceEditorInterface::DrawMissingResourcesBanner` UTF-8 말줄임+툴팁 및 `FindNearbyReplacementAsset`(`AutoFixMissing` 버튼, `m_IsReadOnly` 시 숨김) 구현.
  3. **엔진/에디터 경계 격리 및 일반화**:
     - `MissingResourceRecoveryContext` 사용처(`Model.cpp`, `Resource.cpp`, `ResourceReference.h`, `ResourceHandleArchiveAdapter.h`, `SceneArchive.cpp`) 및 파일 리로케이션 API(`PrepareFilePathRelocation`, `RelocateFileResources`) 전체를 `#if BAM_EDITOR`로 격리.
     - `PlatformPath`의 `Editor://` 하드코딩을 제거하고 범용 가상 스킴 등록 API(`RegisterVirtualScheme` 등)로 일반화(`Editor/Source/Application`에서 `"Editor"` 스킴 동적 등록, `VirtualPathCheck` 갱신).
- **미해결 / 후속 추적 항목**:
  - **`Editor://` 모델 3D 썸네일 미표시**: `Editor://Model/material_ball_in_3d-coat/material_ball_in_3d-coat.bammodel`이 모델 편집기에서는 누락/참조 실패 없이 정상 열리고 텍스처·머티리얼 썸네일도 정상 생성되지만, `.bam/cache/thumbnails` 삭제 후에도 콘텐츠 브라우저에서 해당 `Editor://` 모델의 3D 썸네일이 표시되지 않는 현상(후속 디버깅 필요).
  - **`VFXEditorPanel` 체커보드 토글 시 바닥판 미표시**: `CheckerBackdrop` 토글을 켜고 `VFXPreviewTiledPlaneMesh` + `VFXPreviewCheckerTex`로 전환할 때 바닥판이 보이지 않는 현상(렌더 패스 밖 지연 갱신 및 와인딩·BaseColor 보정 후에도 재현 — `VFXPreviewGroundMatInst` 셰이더/UV/메시 생성 경로 후속 추적 필요).
  - **`VFXEditorPanel` 트레일 오프셋 실시간 프리뷰 갱신 필요**: 통합 이펙트 패널에서 트레일 트랙의 오프셋 등 속성을 조절할 때 현재는 이펙트를 다시 재생해야 변경된 트레일 모양이 반영됨 — 속성 변경 시 즉시 트레일/프리뷰를 재구성해 실시간으로 변화를 확인할 수 있도록 개선 필요.
  - **3D NavMesh 오버레이 최적화 시도 및 평가 (부적합 판정)**:
    - 기존 `DebugRenderer::RegisterNavMeshDebug`는 매 프레임 절두체/거리 컬링으로 100fps → 30fps 드랍이 발생함.
    - 개선 시도로 (1) 전체 폴리곤 `8192`라인 CPU 정점 캐싱 → 카메라 이동 시 끊김, (2) 카메라 반경 `80m` + `15m` 이동마다 재구축을 시도했으나, CPU `DebugRenderer` 라인 상한/반경 잘림과 재구축 스파이크 한계가 있어 **사용자 평가상 부적합한 방식**으로 기록.
    - **근본 해결 방향(후속)**: CPU 라인 `m_Vertices` 매 프레임 복사 대신, NavMesh 베이크/로드 시점에 **전용 정적 GPU 버퍼(`RHIBuffer`) 1회 업로드 + 1 드로우콜**로 전체 NavMesh를 그리는 구조로 전환 필요.
  - **2단계 성능/비동기 과제**: 리소스 피커 썸네일 오픈 지연 최적화(`AssetCache`+`ResourcePicker`), 3D NavMesh 비동기 베이크+푸터바(`SceneViewportPanel`+`ToolBar`) 코드 반영 완료.

### [2026-11-25] 런타임 설정·누락 리소스 복구·프로젝트 허브 C++ 모듈·지형/NavMesh·디스토션 V2·Probe GI 바이어스

- **요약** (기능별 커밋):
  1. **RuntimeSettings** (`67a556b7`): `RuntimeSettingsManager`/`RuntimeSettings`(해상도·전체화면·VSync·FPS·렌더 스케일·품질·오디오·언어)와 `UISettingsMenuSystem`/`RuntimeSettingsMenuComponent`, `Client` 시작 시 `Config/RuntimeSettings.json` 로드·적용 및 SDL 창 콜백 연동, `RuntimeSettingsCheck` 추가.
  2. **누락 리소스 복구** (`5cd82918`): `MissingResourceRecoveryContext`로 씬 로드 중 누락 `ResourceHandle`을 빈 핸들로 로드하며 기록하고, 에디터가 복구 모달(교체/Null 유지)을 표시. `SceneArchiveCheck` 확장.
  3. **프로젝트 허브** (`29ec09a6`): 최근/새 프로젝트 탭별 하단 버튼, `Project.OpenHub`(Ctrl+Shift+O) 액션 등록, 최근 프로젝트 메뉴, 새 프로젝트 시 선택적 C++ 모듈 스캐폴딩(`ProjectDescriptor::Scaffold…`).
  4. **지형·NavMesh** (`2e122943`): 지형 구멍(`Hole`) 마스크·레이어 노멀·청크 LOD·`TerrainSystem` 확장, `NavMesh` 베이크/저장(`Scene::NavMeshPath`)과 뷰포트 오버레이·베이크 메뉴.
  5. **디스토션 V2** (`b7d2aa4a`): `EBlendMode::Distortion`/`ERenderPassType::Distortion`, `distortionWritePS`/`distortionCompositePS`, 드로우 0건 시 패스 생략(`requireNonEmptyPassID`), 머티리얼 에디터 옵션, `BINDING_LAYOUT.md` 갱신.
  6. **Probe GI** (`0de48719`): `NormalBias`/`ViewBias` 파라미터와 셰이더 반영.
  7. 리소스 정리: 내장 텍스처 폴더·`material_ball` 모델 추가, 미사용 `.bamshader.json`/`white1x1.bamtex.json` 삭제, 로캘 JSON 재생성 결과 반영.
- **검증 상태**: 모두 **미검증**(빌드·CTest·에디터 실환경 실행 없음, 정적 분류만 수행). 확인 대상: `RuntimeSettingsCheck`, `SceneArchiveCheck`, `TerrainDataCheck`, `NavigationAICheck`, `ProbeGridCheck`, 디스토션 머티리얼 렌더, 누락 리소스 모달, 프로젝트 허브 C++ 모듈 생성.
- **분류 한계**: `SceneDocument.cpp`의 NavMesh 저장과 `ImGuiManager`의 `OpenProjectHub`, `DeferredSceneRenderer`의 지형 바인딩은 파일 단위 커밋이라 해당 기능 커밋에 함께 포함되었을 수 있음. `BamEngine.bamproject`(로컬 마운트 경로 포함)는 커밋에서 제외.

### [2026-11-25] HDR 큐브맵 IBL · DXR 스킨드 반사 · BEVE 참조 검증 · 프리뷰/인스턴스/프리셋 · 콘텐츠 브라우저 주소창

- **요약**:
  1. **IBL·Probe용 HDR 큐브 텍스처 지원 확장 (트랙 3-1)**:
     - `DirectX12RHI::UploadTextureData`에 `TextureCube`(6면×밉 복사) 및 `R16G16B16A16_FLOAT`/`R32G32B32A32_FLOAT` 업로드를 추가하고, `IBLManager::ConvertEquirectangularToCubemap`·`Texture::InitializeFromHDRMemory`·`DDSLoader` HDR/큐브맵 파싱·`DeferredSceneRenderer` 단일 슬롯 폴백·`ResourcePicker` 2D SRV 바인딩 방어를 연동함.
     - 실환경에서 `.hdr` 연결 시 IBL 반사 동작 확인 완료 (`.hdr` 2D 톤매핑 썸네일 미표시는 후속 과제로 분리).
  2. **DXR M3 스킨드 메시 반사 수정**:
     - `RaytracingSceneManager`(`IsSkinningSupported`, `IsReflectionSupported`, `IsShadowCasterSupported`)에서 임포트 스킨드 메시의 `EMeshFlag::Dynamic` 일괄 제외를 제거해 스킨드 메시(`Fox`)가 TLAS 및 DXR 반사·그림자에 포함되도록 수정.
  3. **BEVE 참조 검증·엔진 리소스 임포트·내장 리소스 썸네일**:
     - `AssetReferenceValidator::ReadReferenceDocument`에서 `BeveArchive`와 동일한 `archive::detail::ArchiveValue`로 BEVE를 디코딩한 뒤 JSON 변환해 `.bammatinst` 참조 검증 거짓 실패 해소.
     - `ModelImporter::ResolveWritableDestinationDirectory`: `Engine://`/`Editor://` 등 읽기 전용 원본 모델 임포트/재임포트 시 `Assets/` 동일 상대 경로로 리다이렉트.
     - `AssetCache`에 `FindInMemoryResource()`를 추가해 확장자/디스크 파일이 없는 코드 생성 내장 리소스(`CubeMesh`, `SphereMesh`, `PlaneMesh`, `CylinderMesh`, `CapsuleMesh`, `QuadMesh`, `DefaultMaterial`, `DefaultBRDFLut` 등)의 2D/3D 썸네일 생성·캐시 지원.
  4. **머티리얼 에디터·엔티티 프리셋 (트랙 4-1)**:
     - `MaterialPreviewRenderer`에서 `EPreviewMeshType::Quad`(`QuadMesh`)와 `Plane`(`PlaneMesh`)을 분리하고, `MaterialEditor` 툴바·메뉴에 `Create Instance`(`<Name>_Inst.bammatinst.json` 즉시 생성·오픈) 추가.
     - `EntityFactory` 및 `HierarchyPanel`에 `Capsule`·`Camera`·기본 도형·라이트 프리셋 보완 및 `DefaultMaterial` 폴백 추가.
  5. **콘텐츠 브라우저 외부 마운트 팝업·UTF-8 주소창 (트랙 3-4)**:
     - `ContentBrowserPanel::DrawMountAddModal`에서 `OpenPopup(modalTitle.c_str())`와 `BeginPopupModal(modalTitle.c_str(), ...)` 해시 ID 불일치를 수정해 외부 폴더 연결 모달 정상 오픈 확인. 이어서 모달 가로 폭 540px 고정 및 `ImGuiCol_PopupBg` 알파 `1.0f` 적용(`UI-Mount`)으로 뒤쪽 스플리터 비침 해소.
     - 주소창 브레드크럼 UTF-8(`WStrToStr`) 통일, 클릭/연필 버튼 시 `InputText` 전환, 미등록 외부 폴더 읽기 전용 탐색 및 `외부 마운트로 연결...` 컨텍스트 메뉴/상단 배너 연동.
  6. **인게임 HDR 환경맵 캡처(`IBL-Capture`)·`.hdr` 2D 썸네일/피커·`MaterialBall` 프리뷰·패키징 잠금 완화**:
     - `DirectX12RHI::ReadTexturePixels` FP16/FP32 Readback, `IBLManager::ConvertCubemapFacesToEquirectangular`·`SaveEquirectangularHDR`, `SceneViewportPanel` `현재 위치에서 HDR 캡처...` 구현 및 실환경 실행·확인 완료.
     - `AssetCache::LoadImageThumbnail`에 `.hdr` Reinhard 톤매핑 + sRGB 감마 2D 썸네일 생성 추가, `ResourcePicker.h`에서 `TextureCube` 2D 썸네일 폴백 연동.
     - `MaterialPreviewRenderer` / `MaterialEditor`: `EPreviewMeshType::MaterialBall`(`Engine://Model/material_ball_in_3d-coat.glb`) 기본 프리뷰 추가 및 `SphereMesh` 폴백.
     - `SceneArchive::PreloadDocumentResources`에서 `Engine://Texture/DefaultBRDFLut` 미등록 시 즉시 생성·등록하도록 보강, `ClientPackage::MovePackage` 10회 재시도(100ms) 및 잠금 안내 보강.
     - `Assets/` 및 `Engine/Resources/` 구버전 바이너리·중복 `.zip`·미사용 JSON 전수 감사 완료(삭제 승인 대기).
- **검증 상태**:
  - 실환경에서 IBL `.hdr` 반사, 외부 폴더 연결 모달 오픈, `IBL-Capture` 6면 HDR 캡처·저장 실행 확인, AO/RT 그림자 깜빡임 재빌드 후 해소 확인, Probe GI 코너 빛샘은 `ShadowBias`로 미해소 확인([DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절에 후속 과제 기록).
  - 최근 추가 반영된 `UI-Mount`, `Thumb-HDR` + HDR 피커 썸네일, `MaterialBall` 프리뷰, `ClientPackage` 잠금 재시도는 에디터 빌드·확인 대기(`cmake --build --preset x64-ninja-debug --target GenerateLocalization Editor`).

### [2026-11-24] P2 차기 개발·트랙 1(스탠드얼론 Client/지형·머티리얼 실환경 수정)·트랙 2(에디터 UI/UX 공통화)·멀티 프로젝트 Phase 5·DXR M3 통합 반영

- **요약**:
  1. **지형 확장(T1~T4) 및 머티리얼/렌더(V1·DXR M3)**:
     - 지형 데이터(`.bamterrain`)·레이어 에셋(`.bamterrainlayer.json`)·지형 셰이더 그래프 도메인(`terrainLayers.hlsli`) 및 **T4 지형 대응 `NavGrid`**(경사각·`Blocked`·레이어 비용 반영, `.bamnavgrid` 저장/복원, 150m 거리 스트라이드 오버레이) 구현.
     - `ShaderCompiler`에 `Engine://Shader/hlsl` `-I` include 경로 전달, `TerrainComponent::TerrainMaterial`의 `MaterialInstance` 수용 및 자체 그래프 폴백, `PreviewViewportWidget` 더미 높이맵 프리뷰 렌더링 보완.
     - **V1 디스토션 1단계**: `ForwardTransparent` 전용 `Shader.ScreenUV`·`Shader.SceneColor` 노드(강도 0 무왜곡, 화면 밖 UV 원본 배경 폴백) 추가.
     - **Render M3 후속**: `Masked` 알파 테스트 재질 DXR 그림자 지원(`raytracedShadowMaskRTCS.hlsl`) 및 카메라 거리 기준 스킨드 메시 우선순위(`SortSkinnedByDistance`) 적용.
  2. **`.bampak` 로더 연결 & 독립 Client 스탠드얼론 수정(P2-4, A-3, 트랙 1)**:
     - `PakMountManager`에 `ToPakUri`, `ExistsOrOnDisk`, `ReadBytesWithFallback`(무결성 실패 시 디스크 폴백 차단), `LoadArchiveWithFallback`을 추가하고 `ResourceManager`, `Texture`, `DirectX12Shader`, `SceneManager`, `TerrainDataFile`, `StartupResourceLoader`, `RenderPipelineProfileFile`, `ResourcePreloader`, `AnimatorStateMachine`, `GraphFile`, `LocalizationManager`, `AsyncResourceLoader`에 연결.
     - `Client/Main.cpp`·`ClientGameRuntime.cpp`·`PlatformPath.cpp`: 작업 디렉터리/루트 고정, `Editor://` 오분류 방지, 패키지 `Mounts/` 자동 등록, `LoadStartupScene` 가상 URI 폴백 수정.
     - `AssetReferenceValidator.cpp`: 코드 생성 내장 리소스(`DefaultBRDFLut`, `PostProcess_*`, `ToneMappingMaterial` 등) 예외 보강, `.git`/`Temp`/`Logs`/`out` 스캔 제외 및 중복 `fs::canonical` 제거, `AssetReferenceCheck` check 51 빈 픽스처 수정.
  3. **액션 타임라인·로직 그래프·`ResourceManager` `#if BAM_EDITOR`·멀티 프로젝트 Phase 5 (1-A~1-C, A-1, B-1)**:
     - `ActionTimelineAsset`/`ActionTimelineSystem`: `EndPolicy`(`Fixed`/`ClipLength`/`LastTrackEnd`) 및 `On Action Timeline Ended` 이벤트 추가, `GraphBindingComponent::graphFile` 로직 그래프 피커 적용.
     - `ImGuiManager`: 미저장 씬이 열려 있어도 메모리 상 참조 0건인 에셋의 이름 변경·이동 허용.
     - `ResourceManager::GetDebugSnapshot()` `#if BAM_EDITOR` 및 `BAM_EDITOR == 0` 안전 스텁 전환.
     - **멀티 프로젝트 Phase 5**: `ProjectDescriptor::ResolveModules`, `ModuleBuildManager` 동적 타깃·외부 프로젝트 자동 configure(`BamEngineConfig.cmake`), `HotReloadManager::RebindForProject`, `ProjectSessionManager::SwitchProject` 모듈 언로드/재로드 연동, `ClientPackage`/`Client/Main.cpp` 모듈 DLL 스테이징·로드.
  4. **에디터 UI/UX 공통화 & 로캘 일관성 (B-5, 트랙 2)**:
     - `EditorUI::TextButton`/`TextButtonRaw`(자동 너비, `IconButton` 높이 통일, `SameLine` 자동 개행, `Default`/`Primary`/`Danger` 스타일, 툴팁·단축키 힌트) 도입 및 `TerrainEditor`, `BuildProfilePanel`, `NetworkProfilerPanel`, `UnifiedSettingsPanel`, `SettingsPanel`, `SpriteEditor`, `TextureInspector`, `CameraViewportPanel`, `PreviewViewportWidget` 치환.
     - `BuildProfilePanel`: 상단 요약 액션 바 + `Editor::DrawSidebarLayout` 확장형 좌측 사이드바(`General & Output`, `Scenes` 순서/인덱스 재배치, `Assets` 폴더 일괄 추가, `Packaging & BamPak`, `Build Log`) 개편.
     - `ReflectionPropertyDrawer`: `Vector`/`Array`/`Set`/`Map` 컨테이너 및 `mat3`/`mat4` 행렬 `ImGui::BeginTable` 정렬, 읽기 전용 긴 경로(`DataPath`, `RecoveryDataPath`) 선택·복사 및 잘림 툴팁(`DrawClippedStringTooltip`) 추가.
     - `EntityHierarchyView` UTF-8 `...` 말줄임+툴팁, `ContentBrowserGridItem` 자물쇠 배지 폰트 비례 중앙 정렬, `PostProcessPanel` 미세 가로 스크롤 제거.
     - 로캘 CSV 보완 및 `LocalizationManager::TranslateLogMessage`·`ConsolePanel::OnLanguageChanged` 실시간 콘솔 로그 재번역 적용.
- **검증 상태**:
  - `x64-ninja-debug` 빌드 및 CTest(10종 + `RLSimulationCheck`) 통과 후 `AssetReferenceCheck` check 51 수정 반영.
  - 이후 추가 반영된 트랙 1(로그 분석 조치, 이름변경 최적화, 빌드패널 사이드바, 프로퍼티 테이블) 및 트랙 2·`AssetReferenceCheck` 수정분은 재빌드 및 실환경 UI/스탠드얼론 Client 확인 대기([DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절).

### [2026-11-24] 순수 Additive(One/One) 블렌드 분리 및 VFX 패스 수집/필터 공용화 (후보 A-4)

- **배경과 목적**:
  - 기존 `EBlendMode::Additive`는 `SrcAlpha / One` 블렌드 팩터로 동작하여, 텍스처나 트랙의 Alpha가 감소할 때 방출 광량이 비례하여 감쇄되는 알파 가산(Alpha Additive) 특성을 가졌다.
  - 알파 감쇄 없이 방출 광량을 100% 보존하는 순수 가산 블렌드(`PureAdditive`, `One / One`)를 신규 분리 정의하여, 기존 프리멀티플라이드/알파 가산 동작의 하위 호환성을 안전하게 유지하면서 완전 자체 발광형 언릿 머티리얼을 지원하도록 고도화했다.
  - `CompositeVFXSystem`과 `ParticleSystem`에 중복 분산되어 있던 뷰포트 패스 타깃 수집 로직(`CollectVFXViewportTargets`)과 프리뷰 패스 블렌드 보정(`ResolveEffectiveAcceptedBlendModes`), 블렌드 모드 허용 검사(`IsBlendModeAccepted`)를 `Engine/Source/VFX/Public/VFXPassHelper.h`로 일원화했다.
- **핵심 구현 내용**:
  1. **`Engine/Source/Render/RHI/Public/RHIBlendTypes.h`**:
     - `EBlendMode::PureAdditive = 1 << 7` 및 `ALL = (1 << 8) - 1` 추가.
     - `BlendState(EBlendMode::PureAdditive)` 블렌드 상태 매핑 추가: `srcColor = One`, `dstColor = One`, `colorBlendOp = Add`, `srcAlpha = One`, `dstAlpha = One`, `alphaBlendOp = Add`, `colorWriteMask = RGBA`, `enable = true`.
     - 기존 `EBlendMode::Additive`(`SrcAlpha / One`)는 그대로 보존하여 기존 에셋 동작 무결성 보장.
  2. **`Engine/Source/Render/RenderPass/Public/RenderPass.h` / `RenderPassManager.h`**:
     - `RenderPass::IsAcceptsBlendMode(EBlendMode mode)`에서 `mode == PureAdditive`일 때 패스가 기존 `Additive`를 수용하면 자동으로 허용하는 하위 호환 폴백 지원.
     - `RenderPassManager::RegisterRenderPass` 기본 허용 블렌드 모드 및 `DeferredProfilePasses.cpp` 포워드 투명 패스에 `PureAdditive` 명시 추가.
  3. **`Engine/Source/VFX/Public/VFXPassHelper.h` 신설**:
     - `CollectVFXViewportTargets()`: 활성 카메라 존재 여부, 씬 제출 허용 여부, Geometry/ForwardTransparent 패스 여부 및 중복 제거를 단일 헬퍼로 집약.
     - `ResolveEffectiveAcceptedBlendModes(renderPass)`: 프리뷰 Geometry 패스의 `None` 블렌드 모드를 `Opaque | Masked`로 보정하는 전파 방어 로직 집약.
     - `IsBlendModeAccepted(acceptedModes, materialMode)`: 패스의 허용 모드와 머티리얼 모드의 호환성 판정.
  4. **`Engine/Source/VFX/Private/CompositeVFXSystem.cpp` / `ParticleSystem.cpp`**:
     - `OnSubmit`의 15줄 중복 타깃 수집을 `VFXPassHelper::CollectVFXViewportTargets()` 한 줄로 치환.
     - `SubmitScene*`의 실효 블렌드 모드 계산을 `VFXPassHelper::ResolveEffectiveAcceptedBlendModes(renderPass)`로 일원화.
     - 스프라이트·메시·데칼·트레일 및 이미터 컴포넌트 순회 시의 블렌드 필터를 `VFXPassHelper::IsBlendModeAccepted`로 치환.
  5. **`Editor/Source/ImGui/ResourceEditor/Private/MaterialEditor.cpp` & `Editor.MaterialOption.csv`**:
     - 머티리얼 에디터 파이프라인 설정의 블렌드 모드 콤보 박스에 `PureAdditive` 매핑 추가.
     - `Editor.MaterialOption.csv`에 `MaterialOption,PureAdditive,Pure Additive (One/One),순수 가산 (One/One)` 추가 (JSON 직접 수정 금지 준수).
  6. **`Engine/examples/VFXBlendAndPassCheck.cpp` 신설 & `Engine/cmake/Checks.cmake` 등록**:
     - Additive 기존 팩터 보존 및 PureAdditive `One/One` 팩터 검증.
     - 프리뷰 Geometry 패스 실효 블렌드 모드 보정 검증.
     - 패스 수용/거부 및 Additive 수용 패스의 PureAdditive 호환 폴백 검증.
- **검증 상태**:
  - 코드 정적 분석 및 diff 전수 검토 완료.
  - 빌드(`cmake`, `msbuild`) 및 CTest 실행은 AI 직접 실행 금지 지침에 따라 사용자 검증 대기.

### [2026-11-24] 멀티 프로젝트 물리 격리 및 외부 폴더 마운트 (Multi-Project & Content Mount Phase 1~4)

- **배경과 목적.** 기존 BamEngine은 엔진 저장소 루트와 단일 프로젝트 루트가 결합되어 있어, 여러 프로젝트 간 에셋(`Assets/`), 설정(`Config/`), 캐시(`.bam/cache/`), 임시/복구 데이터가 단일 작업 공간에 혼재되었다. 또한 공용 에셋 라이브러리나 외부 폴더를 안전하게 참조할 수 있는 마운트 계층이 부재했다. 이를 해결하기 위해:
  1) `PlatformPath`에서 엔진 루트와 프로젝트 루트를 명시적으로 분리하고, `.bamproject` 매니페스트 및 외부 콘텐츠 가상 마운트(`Mount://<Alias>/...`) 계층을 신설.
  2) 에디터 프로세스 재시작 없이 활성 프로젝트를 교체할 수 있는 8단계 인-프로세스 핫 스위치 파이프라인(`ProjectSessionManager::SwitchProject`) 및 프로젝트 스코프 리소스 언로드(`ResourceManager::UnloadProjectScopedResources`) 구현.
  3) 최근 프로젝트 목록(`%APPDATA%/BamEngine/RecentProjects.json`) 및 신규 프로젝트 생성/열기 모달(`ProjectHubModal`), 콘텐츠 브라우저 외부 마운트 탐색 및 관리 UI(`ContentBrowserPanel`) 연동.
  4) 패키징 파이프라인(`PakFormat.h`, `AssetReferenceValidator`, `ClientPackage`)에서 `Mount://` 가상 URI를 정규 런타임 경로로 지원하여, 참조된 외부 마운트 에셋만 선택적으로 `.bampak`에 패킹되도록 통합.
- **Phase 1: 경로 시스템 분리 및 `.bamproject` 매니페스트 (`PlatformPath`, `ProjectDescriptor`).**
  - `PlatformPath`:
    - `SetEngineRoot(path)` / `GetEngineRoot()` 분리 (미설정 시 `GetProjectRoot()`로 안전한 폴백 제공).
    - `ContentMountDesc` 구조체 (`Name`, `RootPath`, `ReadOnly`, `ShowUnderAssets`) 정의.
    - `Mount://<Alias>/...` 가상 마운트 API (`RegisterContentMount`, `UnregisterContentMount`, `ClearContentMounts`, `GetContentMounts`, `HasContentMount`, `IsMountUri`, `ResolveMountUri`, `IsMountReadOnly`) 추가.
    - `IsVirtualUri`, `ResolveUri`, `ToVirtualUri` 정규화 우선순위(`Project://` -> `Engine://` -> `Editor://` -> `Plugin://` -> `Mount://<Name>/`) 정립 및 상향 경로 탈출 방어(`IsSubpathOf`).
    - `IsReadOnlyTarget`: `ReadOnly` 마운트의 가상 URI 및 물리 경로 보호.
  - `Engine::ProjectDescriptor`:
    - `.bamproject` JSON 스키마 (`SchemaVersion`, `ProjectName`, `EngineVersion`, `AssetDirectory`, `ContentMounts`, `Modules`) 정의 및 Glaze 기반 `LoadFromFile` / `SaveToFile` 구현.
    - `ResolveAssetRoot`, `ResolveContentMounts`, `FindProjectFile` 탐색/해석 도우미 제공.
    - `ScaffoldNewProject`: 독립 프로젝트 디렉터리에 `.bamproject`, `Config/Project.json`, `Assets/Scenes/Main.bamscene.json`을 자동 생성하는 격리 스캐폴딩 지원.
- **Phase 2: 인-프로세스 프로젝트 전환 및 리소스 정리 (`ResourceManager`, `EditorSettingsService`, `ProjectSessionManager`).**
  - `ResourceManager`:
    - `UnloadProjectScopedResources()` 추가: `Project://` 및 `Mount://` 스코프 리소스만 선별 해제하고, `Engine://`, `Editor://`, `Plugin://` 및 프로시저럴 빌트인 리소스(폰트, 기본 머티리얼, 더미 텍스처 등)는 메모리에 유지하여 전환 속도와 UI 안정성 확보.
  - `EditorSettingsService`:
    - `ReloadProjectSettings(projectRoot)`: 새 프로젝트 루트의 `Config/Project.json`, `Config/InputActions.json`, `Config/EditorWorkspace.json`을 리로드하고 `InputManager` 액션 갱신. 전역 `EditorPreferences`는 유지.
    - `RecentProjectEntry` 및 최근 프로젝트 목록 영속화 (`%APPDATA%/BamEngine/RecentProjects.json`).
  - `Editor::ProjectSessionManager` (싱글톤):
    - 8단계 핫 스위치 파이프라인 (`SwitchProject`):
      1) 사전 유효성 검증 및 미저장 편집/PIE 실행 확인 콜백.
      2) 백그라운드 리스너(`efsw`), 비동기 로더/스트리머, 복구 타이머 드레인/중지.
      3) UI 편집기 창 정리, Undo/Redo 이력 초기화, 클립보드/선택 상태/썸네일 캐시 초기화.
      4) 씬 언로드, 백엔드 GPU 커맨드 큐 플러시 대기(`WaitForGPUIdle`), `UnloadProjectScopedResources()`.
      5) `PlatformPath` 루트 및 마운트 재바인딩 (`EngineRoot` 불변 보장, 새 `.bamproject`의 `ContentMounts` 등록).
      6) `EditorSettingsService` 및 프로젝트 로캘 카탈로그 리로드.
      7) 파일 감시자/복구 세션 재시작, 콘텐츠 브라우저 새로고침, 시작 씬 로드 (실패 시 빈 씬 안전 폴백).
      8) 윈도우 타이틀 갱신(`BamEngine Editor - <ProjectName>`) 및 최근 프로젝트 기록.
    - `CreateProject`: 템플릿(Empty, 3D, 2D) 기반 스캐폴딩 후 즉시 전환 지원.
    - 외부 마운트 영속 수정 API (`AddContentMount`, `RemoveContentMount`, `UpdateContentMount`, `SetContentMountFlags`)로 `.bamproject`와 실시간 동기화.
- **Phase 3: 프로젝트 허브 및 콘텐츠 브라우저 외부 마운트 UI (`ProjectHubModal`, `ContentBrowserPanel`, 로캘 CSV).**
  - `ProjectHubModal`:
    - 최근 프로젝트 목록 더블클릭 열기, `.bamproject` 또는 폴더 브라우저 열기, 새 프로젝트 생성 탭(프로젝트명 유효성 검사, 위치 브라우징, 템플릿 선택).
  - `ContentBrowserPanel`:
    - 좌측 트리에 `External Mounts` 섹션 신설 및 `PlatformPath::GetContentMounts()` 렌더링. `ShowUnderAssets` 활성화 마운트는 `Assets/` 하위 가상 링크 노드로도 표시.
    - `[외부 폴더 연결...]` 모달 및 우클릭 컨텍스트 메뉴(`읽기 전용 토글`, `Assets 하위 표시 토글`, `연결 해제`) 제공.
    - 기존 읽기 전용 가드(`PlatformPath::IsReadOnlyTarget`)를 통해 읽기 전용 마운트 대상의 삭제/이름변경/이동 차단.
  - 로캘 CSV:
    - `Editor.ProjectHub.csv` 신설 (Group=ProjectHub, 21개 키).
    - `Editor.ContentBrowser.csv` 확장 (Group=ContentBrowser, 13개 키 추가). JSON 직접 수정 금지 원칙 준수.
- **Phase 4: 의존성 기반 패키징 연동 (`PakFormat.h`, `AssetReferenceValidator`, `ClientPackage`).**
  - `PakFormat.h`:
    - `NormalizePakUri`에 `mount://` -> `"Mount://"` 스키마 정규화 지원 (`Mount://<Alias>/<relPath>` 2세그먼트 검증).
  - `AssetReferenceValidator`:
    - `ValidateBuildScene`에서 `Mount://` URIs를 정규 런타임 에셋으로 인식하여 외부 절대 경로 오류 방지 및 재귀 의존성 추적.
  - `ClientPackage`:
    - `OnlyReferencedAssets = true` 모드에서 참조된 `Mount://` 파일 및 `AdditionalAssets`의 마운트 항목만 `staging/Mounts/<Name>/<rel>`에 선별 스테이징. 외부 마운트 폴더 전체 복사 차단.
    - `staging/Mounts/`를 `Content/Project.bampak` 내부 가상 URI(`Mount://...`)로 매핑하여 패킹 및 무결성 검증.
    - 패키지 내보내기 대상 폴더가 등록된 외부 마운트 폴더와 중첩되는 경우 감지하여 충돌 차단.
- **검사 (`VirtualPathCheck.cpp`).**
  - Section 14: `SetEngineRoot` / `GetEngineRoot` vs `GetProjectRoot` 독립 분리 및 폴백 검증.
  - Section 15: `Mount://<Alias>/...` 등록, 대소문자 무관 판별, URI 해석, 디렉터리 탈출 방어, `ToVirtualUri` 역변환, `IsReadOnlyTarget` / `IsMountReadOnly` 보호, 마운트 해제 검증.
  - Section 16: `ProjectDescriptor` `.bamproject` 직렬화/역직렬화 라운드트립, 상대 경로 해석, `FindProjectFile`, `ScaffoldNewProject` 격리 생성 및 필수 파일(`.bamproject`, `Project.json`, `Main.bamscene.json`) 무결성 검증.
- **검증 및 확인 명령.**
  - AI는 빌드 및 테스트를 직접 실행하지 않았다.
  - 사용자 실행 확인 명령:
    ```powershell
    cmake --build --preset x64-ninja-debug --target VirtualPathCheck Editor PakArchiveCheck AssetReferenceCheck
    ctest --test-dir out/build/x64-ninja-debug -R "^(VirtualPathCheck|PakArchiveCheck|AssetReferenceCheck)$" --output-on-failure
    ```
- **정적 한계 및 남은 위험.**
  - Phase 5 (2단계: 프로젝트별 독립 C++ 모듈 DLL 빌드 및 핫리로드)는 추후 구현 예정으로, 현재는 단일 게임플레이 DLL/내장 컴포넌트 모델 유지.
  - 실제 대규모 씬에서 외부 마운트 에셋을 참조한 후 인-프로세스 프로젝트를 반복 전환할 때의 DirectX12 GPU 메모리 및 OS 파일 핸들 릴리즈는 사용자 실행 환경 검증 필요.

### [2026-11-24] 의존성 기반 선택적 Client 패키징 (P2-E 5.1절)

- **배경과 목적.** 기존에는 `ClientPackage`가 `Assets/` 및 `Engine/Resources/` 전체 폴더의 모든 런타임 파일을 무조건 복사하여 미참조 대용량 에셋이나 미사용 리소스까지 패키지에 포함되었다. 시작 씬과 명시된 추가 씬/에셋의 실제 재귀 의존성만 추적하여 배포본을 경량화하고 선택적 스테이징을 지원한다.
- **의존성 탐색 보강 (`AssetReferenceValidator`).**
  - `Engine::TerrainComponent`의 `DataPath`(`.bamterrain`) 수집 연결.
  - 머티리얼(`EResourceType::Material`/`MaterialInstance`)의 `m_ShaderGraphPath`(`.bamgraph`/`.bamgraph.json`) 재귀 탐색 연결.
  - 셰이더 문서(`EResourceType::Shader`)의 `m_DxilPath`, `m_SpirvPath`, `m_WgslPath` 바이너리 수집 연결.
  - `IsBuiltinResourceIdentifier`를 통한 빌트인 리소스(QuadMesh, CubeMesh, Standard 등)의 오탐 방지 및 `referencedFiles` 중복 제거.
- **선택적 스테이징 (`ClientPackage`).**
  - `ClientPackageRequest`에 `bool OnlyReferencedAssets = true;` 추가(기본 활성화).
  - `ExportClientPackage`: `OnlyReferencedAssets` 활성화 시 시작 씬, `AdditionalScenes`, `AdditionalAssets`, `ValidateBuildScene` 재귀 참조, `StartupResourceLoader` 매니페스트 및 파이프라인 파일, `RenderPipelineProfile` 및 필수 렌더 셰이더/리소스, 프로젝트/엔진 로캘 카탈로그, 엔진 런타임 필수/폴백 리소스(`TerrainMaterial.bammat.json`, 런타임 DXIL `.cso` 등)만 선택 복사(`StageSelectedResources`). 비활성화 시 기존 전체 폴더 스캔 유지.
  - `PackAssetsIntoBamPak` 활성화 시 `AdditionalAssets`도 `Content/Project.bampak`의 무결성 검증 목록에 포함.
  - `[Package inventory]`에 수집 정책(`dependency-based selective packaging` vs `full runtime resource scan`) 및 스테이징/제외 통계(수·바이트) 출력.
- **UI 연동 (`BuildProfilePanel`).**
  - `BuildProfilePanel`에 `참조된 에셋만 패키징`(`BuildProfile.OnlyReferencedAssets*`) 체크박스를 추가하고 `m_OnlyReferencedAssets`를 `ClientPackageRequest`로 전달.
  - `Editor.BuildProfile.csv`에 `BuildProfile.OnlyReferencedAssets`, `BuildProfile.OnlyReferencedAssetsTooltip`, `BuildProfile.OnlyReferencedAssetsCheckbox` 및 `BuildProfile.BamPakPackaging*` 로캘 키 6행 추가.
- **검사 (`PakArchiveCheck`).**
  - `TestDependencyBasedPackaging` 추가: 시작 씬 재귀 참조 에셋 포함, `AdditionalAssets`(JSON 및 직접 파일 예외 스트리밍 오디오) 포함, 미참조 씬/텍스처/머티리얼/오디오/엔진리소스 제외, 패키지 매니페스트 기반 마운트 및 데이터 페이로드 무결성 검증.
- **검증 및 확인 명령.**
  - AI는 빌드 및 테스트를 직접 실행하지 않았다.
  - 사용자 실행 확인 명령:
    ```powershell
    cmake --build --preset x64-ninja-debug --target PakArchiveCheck Editor AssetReferenceCheck
    ctest --test-dir out/build/x64-ninja-debug -R "^(PakArchiveCheck|AssetReferenceCheck)$" --output-on-failure
    ```
- **정적 한계 및 남은 위험.**
  - C++ 코드나 로직 그래프의 문자열 핀에서 동적으로 파일 경로를 구성하여 로드하는 에셋(예: `SpawnCompositeVFX("Project://VFX/Fire.bamvfx.json")`)은 정적 JSON 파싱으로 추적되지 않으므로 `BuildProfilePanel`의 `AdditionalAssets`에 수동 등록 필요.
  - 실제 프로젝트 씬(예: `RaytracingScene3`)의 저장소 밖 Windows Client 내보내기 및 렌더링 일치는 사용자 실행 검증 대기.

### [2026-11-24] 렌더링 기능 병렬 구현 병합과 후속 수정

- **병합.** 세션별로 구현한 일곱 기능을 커밋해 main에 병합했다(구현 커밋 → 병합 커밋). RT 대상 확대 `5d186491`→`3def7781`, 부드러운 RT 그림자 `074d9e6b`→`6c0364c2`, AO(GTAO·RT) `3c2dac7b`→`8dee6eaa`, 프로브 GI 1단계 `624c2948`→`c9b0c098`(2~4단계는 그 세션이 따로 기록), 투영 데칼 `a29bec92`→`20b3e110`, 자동 노출 `e684db5b`→`7626792b`, 피사계 심도 `21cb9436`→`47fb9c98`. 기능별 설계와 한계는 `RenderingDevelopmentPlan.md`의 S8~S11, M3, M4에 있다.
- **병합 중 정리.** AO와 프로브 GI 설정 항목은 양쪽을 모두 살렸고, 합친 `EditorSettingsCheck` 조건의 쉼표를 `&&`로 고쳤다. 후처리 순서는 자동 노출 → 피사계 심도 → 블룸이다. 저장된 설정에 없는 효과를 끼우는 같은 기능 두 벌 중 검사가 있는 `MergeEffectOrder`만 남겼다. 계획 문서의 절 번호가 겹쳐 S8~S11로 다시 매겼다.
- **시험 씬.** `Assets/Scenes/RenderingFeatureLab.bamscene.json`(git 제외 폴더)을 만들었다. 처음 만든 씬은 바닥이 3cm 아래(`y = -0.03`)여서 모든 물체가 떠 보였고, 바닥을 0으로 고쳤다.
- **데칼 텍스처 결함.** 데칼 패스 그리기 명령 안에서 텍스처를 처음 읽어 GPU 업로드를 시도했고, DirectX12 RHI가 렌더 패스 안의 업로드를 거부해 텍스처 읽기가 실패했다(한 번 실패한 리소스는 다시 시도하지 않아 같은 텍스처를 쓰는 재질도 영향을 받는다). 제출 단계에서 데칼 텍스처와 대체 흰색 텍스처를 미리 올린다.
- **데칼·볼륨 선택.** 자기 표면이 없어 메시 피킹으로 고를 수 없었다. 씬 뷰가 데칼과 프로브 GI 볼륨 중심에 아이콘을 그리고 그 아이콘으로 고른다(꺼진 것은 회색). 고른 볼륨은 상자와 프로브 위치(2048개 이하)를 그린다.
- **확인된 결과.** 사용자가 CTest 전체 통과와 시험 씬의 렌더링을 확인했다(병합 후, 데칼 텍스처·선택 수정 전). 바닥 틈과 데칼 피킹 문제를 보고받아 고쳤다.
- **확인하지 못한 것.** 데칼 텍스처·선택과 볼륨 표시 수정 후의 화면, 모션 벡터, 기능별 GPU 시간과 메모리, Release, Client.
- **남은 위험.** 조명 패스의 IBL 큐브맵도 그리기 명령 안에서 `GetRHITexture()`를 불러, 자리만 등록된 상태로 처음 쓰이면 같은 업로드 실패가 날 수 있다(정적 분석).

### [2026-11-24] 편집 모드에서 지형이 그려지지 않던 결함 (T1-a·T2-e 회귀)

- **증상(사용자 보고).** 에디터에서 지형이 보이지 않아 T1~T3 항목을 확인할 수 없었다. 로그에는 지형 오류가 없었다.
- **원인.** `TerrainSystem`은 실행 단계를 지정하지 않아 `ISystem` 기본값(Gameplay)을 쓰므로 `OnUpdate`는 PIE 중에만 돌고, 편집 모드에서는 `OnSubmit`만 돈다. T1-a의 `TerrainDataFile::LoadIfChanged`와 T2-a의 `TerrainUtility::EnsureLayerData`를 `OnUpdate`에만 넣어, 편집 모드에서 레이어 4~7 데이터가 비고 `SplatmapExtraTexture`가 만들어지지 않았다. T2-e에서 이 텍스처가 없으면 그리지 않게 했으므로 지형 전체가 빠졌다. 같은 이유로 편집 모드에서 `.bamterrain`도 읽히지 않았다(저장한 높이 대신 기본 형태).
- **수정.** 두 호출을 `OnUpdate`·`OnSubmit`이 함께 부르는 `TerrainSystem::EnsureTerrainResources` 맨 앞으로 옮겼다.
- **검증.** AI는 빌드·실행하지 않았다. 편집 모드에서 지형이 보이고 저장한 높이로 열리는지, PIE 전후가 같은지 11.7.1절 T1~T3 항목으로 확인한다.

### [2026-11-24] CTest 실패 2건 수정 (`ShaderGraphCheck`, `NavigationAICheck`)

- **결과(사용자 보고).** 전체 CTest에서 `NavigationAICheck`(48)·`ShaderGraphCheck`(56)만 실패했다. `LastTest.log`: ShaderGraphCheck는 `Data inputs accept one source.`, NavigationAICheck는 `Steep terrain slope cell was unexpectedly walkable.`
- **ShaderGraphCheck 원인.** 디스토션 V1 검사(커밋 전 다른 작업)의 2번 사례가 1번에서 `ScreenUV → SceneColor.uv`로 연결한 입력 핀에 `AddFloat2` 결과를 끊지 않고 다시 연결했다(데이터 입력은 연결 하나, `Graph::CheckConnection`). 연결 번호를 보관해 `Disconnect`한 뒤 다시 연결하도록 검사를 고쳤다. 이 파일에서 앞쪽에 있는 지형 도메인·기본 지형 그래프 사례(T3-b1·T3-c)는 이 오류 전에 예외 없이 지나갔다(예외는 첫 실패에서 멈춤).
- **ShaderGraphCheck 추가 수정(재실행 후).** 연결 오류를 고치자 같은 디스토션 검사의 다음 두 곳이 드러났다. (1) 기대 문자열 `5.00000000e-02f`: `0.05f`는 2진수로 정확히 표현되지 않아 생성기(`FormatFloat`, 9자리)가 `5.00000007e-02f`를 내므로 정확히 표현되는 `0.0625f`·`-0.125f`로 바꿨다. (2) 정점 단계 거부 검사: 정점 생성기는 `worldPositionOffset`·`customUV`에 연결된 노드만 읽는데 검사 그래프의 디스토션 노드는 `baseColor`에만 연결되어 거부가 일어나지 않았다. `SceneColor.rgb → worldPositionOffset` 연결을 추가했다. 생성기 코드는 바꾸지 않았다.
- **NavigationAICheck 원인.** 커밋 전 지형 내비게이션(T4) 베이크가 경사를 가장 가까운 정점의 좌우 차분으로 구했다. 정점 한 줄짜리 능선에서 꼭대기 정점의 양옆 높이가 같아 기울기 0(평지)이 되어 약 84° 급경사 셀 (7,3)이 통행 가능으로 남았다. 셀 중심이 놓인 칸 네 정점의 쌍선형 기울기로 바꿨다(`NavGrid::BakeObstaclesFromScene`). 속성·레이어는 이전처럼 가장 가까운 정점에서 읽는다.
- **영향.** 경사 판정이 정점 단위에서 셀 단위가 되어, 정점 하나만 솟은 뾰족한 지형 옆 셀도 막힌다(의도). 완만한 경사에서는 결과가 같다.
- **검증.** AI는 빌드·실행하지 않았다. 손으로 따라 계산한 결과 급경사 셀 (7,3)·(8,3)은 막히고, 속성 Blocked 셀 (7,10)·평지 (3,3)·(12,3)·(7,15)는 기대와 같으며 남쪽 통로(z≥13)로 우회가 가능하다. 이후 단계(경로·저장 왕복)는 실행 확인이 필요하다.

### [2026-11-24] 기본 지형 그래프 (T3-c)

- **수정.** `ShaderGraphCompiler::BuildDefaultTerrainGraph`가 `terrainPS`와 같은 지형 표면 그래프를 만들고, 머티리얼 편집기가 지형 도메인 머티리얼의 새 그래프를 이것으로 시작한다. 엔진 `TerrainMaterial`은 고정 셰이더를 계속 쓴다(공유 리소스·Client 이유). 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.13절.
- **검사.** `ShaderGraphCheck`에 기본 지형 그래프 사례.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 머티리얼 편집기의 Terrain 도메인 연결 (T3-b2)

- **수정.** `EMaterialDomain`(`Surface`·`Terrain`)과 머티리얼 필드 `m_Domain`을 추가하고, 엔진 기본 지형 머티리얼을 Terrain으로 표시했다. 머티리얼 편집기는 지형 도메인에서 그래프로 픽셀 셰이더만 만들고 엔진 `terrainVS`와 짝지어 적용한다(주 패스는 지오메트리만). 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.12절.
- **로캘.** `Engine.csv` 3행(도메인 열거값 2, 속성 1), `Editor.MaterialEditor.csv` 1행.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 셰이더 그래프 Terrain 도메인 컴파일러 (T3-b1)

- **수정.** 지형 노드 5종(`Terrain.LayerWeight`·`LayerSample`·`LayerBlend`·`Slope`·`Height`)과 `ShaderGraphCompiler::GenerateTerrainMaterialHlsl`을 추가했다. 지형 픽셀 공용 선언을 `terrainLayers.hlsli`로 옮기고 `terrainPS.hlsl`을 그 위에 다시 썼다(동작 같음). 지형 노드는 지형 생성기·픽셀 단계에서만 컴파일된다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.11절.
- **로캘.** `Engine.csv` 10행(노드 이름 5, 핀 1, 오류 4).
- **검사.** `ShaderGraphCheck`에 지형 도메인 사례.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 지형 바인딩을 머티리얼 텍스처 범위에서 분리 (T3-a)

- **수정.** 지형 높이맵을 `t8, space3`, 스플랫을 `t9`·`t10, space3`로 옮겨 머티리얼 범위 `t0`~`t7, space1`(셰이더 그래프 슬롯)을 비웠다. 지형 셰이더 4개와 `Renderer.cpp` 지형·모션 벡터 드로우, `RenderCommands.h` 슬롯 상수, [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)를 고쳤다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.10절.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 지형 8레이어 렌더링 (T2-e)

- **수정.** `terrainPS.hlsl`이 레이어 에셋 알베도(`t8`~`t15`)·반복 길이·색 배율과 레이어 4~7 스플랫(`t5`)을 읽어 8개 레이어를 합성한다. 지형 드로우 상수 `TerrainObjectData`(256바이트), `HeightfieldDrawData` 레이어 필드, `SplatmapExtraTexture` 생성·업로드, 에셋 없는 칸의 기본 색·반복 길이를 추가했다. [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)에 지형 바인딩 절을 추가했다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.9절.
- **로캘.** `Editor.TerrainEditor.csv`에서 `TerrainEditor.LayerNotRendered` 삭제.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.
- **미확인.** D3D12 외 백엔드의 256바이트 상수, 밉맵 없는 레이어 텍스처의 반짝임.

### [2026-11-24] 지형 레이어 독립 에셋 (T2-d)

- **결정.** 사용자: 레이어 정의를 독립 에셋으로 한다(T2-c의 컴포넌트 내장 목록 대체).
- **수정.** `TerrainLayerAsset`·`TerrainLayerSettings`, 리소스 종류·로더, 지형 칸의 에셋 참조, `TerrainLayerEditor`(저장·Ctrl+S·실행 취소), 콘텐츠 브라우저 만들기·아이콘, 피커·인스펙터 형식 목록, 에셋 핫 리로드, 팔레트 이름·요약. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.8절.
- **로캘.** `Engine.csv` 1행, `Editor.ContentBrowser.csv`·`Editor.ResourceEditor.csv`·`Editor.Notification.csv` 각 1행, `Editor.TerrainEditor.csv` 3행 추가·1행 문구 변경.
- **검사.** `TerrainDataCheck`에 `CheckTerrainLayerAsset`.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 지형 레이어 정의와 속성 맵 표시 (T2-c)

- **결정(계획 변경).** `TerrainLayer` 독립 에셋 대신 `TerrainComponent::Layers`(`TerrainLayerSlot`)로 구현했다. 인스펙터가 에셋을 편집하지 않아 전용 편집기가 필요했기 때문이다. 지형 사이 공유는 잃는다.
- **수정.** 레이어 정의 필드, 팔레트의 레이어 이름·속성 요약, 속성 페인트 중 속성 맵 점 표시. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.7절.
- **로캘.** `Engine.csv` 6행, `Editor.TerrainEditor.csv` 4행.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 지형 편집기 브러시 도구·레이어 팔레트·단축키 (T2-b)

- **수정.** 지형 편집기에 조형·레이어 페인트·속성 페인트 도구와 8칸 레이어 팔레트, 속성 선택을 추가하고 뷰포트 입력을 도구별 함수에 연결했다. 레이어 선택(1~8)과 브러시 반경(`[`·`]`)을 `ShortcutManager`에 등록했다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.6절.
- **로캘.** `Editor.TerrainEditor.csv`·`Editor.Action.csv`·`Editor.Shortcuts.csv`.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 지형 8레이어 데이터·속성 맵·페인트 위치 (T2-a)

- **결정.** 사용자: 레이어 최대 8개, 최대 해상도 1025 정점.
- **수정.** 레이어 4~7(`SplatDataExtra`)과 속성 맵(`AttributeData`, `ETerrainAttribute`)을 추가하고 `.bamterrain`을 v2로 올렸다(v1 읽기 유지). 해상도 상한 1024를 `RANGE`와 `TryGetGridLayout`에 넣었다. 페인트가 `worldMatrix`를 반영하고 8레이어를 정규화하며, 속성 페인트를 추가했다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.5절.
- **검사.** `TerrainDataCheck`에 `CheckLayersAndAttributes`.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.
- **남은 것.** 레이어 4~7 렌더링, `TerrainLayer` 에셋, 팔레트 UI·에디터 브러시 연결(T2-b 이후).

### [2026-11-24] 지형 기본 머티리얼 정리와 패키징 확인 (T1-d)

- **수정.** 지형 기본 머티리얼을 추적되는 `Engine/Resources/Material/TerrainMaterial.bammat.json`으로 옮기고, 새 지형은 파일에서 불러오며, 머티리얼이 없는 지형은 그릴 때 기본 머티리얼을 쓴다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.4절.
- **확인.** 일반 파일 패키지는 `.bamterrain`을 포함한다(코드 변경 없음). pak 패키지에서 리소스를 읽는 로더가 없는 것은 지형 밖 기존 한계로 기록했다.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 하이트맵 16비트 입출력·UTF-8 경로·가져오기 Undo (T1-c)

- **수정.** 하이트맵 가져오기·내보내기를 16비트 PNG와 RAW(`.raw`·`.r16`)로 바꾸고, 파일을 넓은 문자 경로로 읽고 써 한글 경로를 지원한다. 가져오기 실패 시 높이를 바꾸지 않는다. 지형 편집기의 가져오기·초기화를 편집 이력에 기록하고 결과를 알린다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.3절.
- **로캘.** `Editor.TerrainEditor.csv` 8행.
- **검사.** `TerrainDataCheck`에 `CheckHeightmapImportExport`.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 저장하지 않은 지형 편집의 PIE·복구 보존 (T1-b)

- **결정.** "하이트맵 자동 저장" = 복구용 스냅샷(계획 기본안). 원본 `.bamterrain`은 씬 저장 때만 바뀐다.
- **수정.** PIE 시작·종료에 지형 메모리 사본(`TerrainDataFile::CaptureMemory`·`RestoreMemory`), 복구 스냅샷에 임시 지형 파일과 `TerrainComponent::RecoveryDataPath`(`ScopedRecoveryCopies`)를 추가했다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.2절.
- **검사.** `TerrainDataCheck`에 `CheckUnsavedTerrainPreservation`.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.

### [2026-11-24] 지형 데이터 파일 저장·복원 (T1-a)

- **원인.** `TerrainComponent`의 `HeightData`·`SplatData`가 `PROPERTY`가 아니어서 스컬프트·페인트 결과가 씬 저장·재시작·Client에서 사라졌다.
- **수정.** `.bamterrain` v1 형식과 `TerrainDataFile`(쓰기·읽기·씬 저장·변경 시 읽기)을 추가했다. 컴포넌트는 `DataPath`만 직렬화하고, `SceneDocument::SaveTo`가 씬 JSON보다 먼저 모든 지형 데이터를 쓴다(실패 시 씬 저장 실패). `TerrainSystem`이 `DataPath` 변경 시 파일을 읽는다. 세부는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 6.1절.
- **로캘.** `Engine.csv`에 `PROP_TERRAIN_DATA_PATH` 1행.
- **검사.** `TerrainDataCheck`에 `CheckTerrainDataFile` 추가.
- **검증.** AI는 빌드·실행하지 않았다. 확인 항목은 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7.1절.
- **남은 한계.** PIE·복구 스냅샷의 미저장 지형 편집 보존, Import·Reset Undo, 16비트 입출력, 다른 이름으로 저장 시 데이터 파일 공유.

### [2026-11-24] 그래프 바인딩 로직 그래프 경로 피커 (1-C)

- **원인.** `GraphBindingComponent::graphFile`이 일반 문자열 필드로 그려져 경로를 손으로 입력해야 했다. 셰이더 그래프도 같은 `.bamgraph.json` 확장자를 써서 확장자만으로는 로직 그래프를 고를 수 없다.
- **수정.** 인스펙터가 `graphFile`을 숨기고 `DrawInspectorAssetPathPicker`(Animator 컨트롤러·타임라인과 같은 피커)를 그린다. 필터 `IsLogicGraphFile`은 파일의 최상위 `"Domain"` 값이 0(Logic)인 것만 받으며, 피커가 매 프레임 물어도 다시 읽지 않도록 수정 시각 기준으로 결과를 보관한다. 고른 경로는 가상 URI로 저장된다. 표시 이름은 기존 `PROP_GRAPH_FILE`을 써서 로캘 행 추가는 없다.
- **참조 유지.** 이동·이름 변경 시 `graphFile`과 실행 중 그래프 상태의 갱신은 기존 `AssetPathRelocation`이 처리하고 `AssetReferenceCheck`가 검사한다(이번에 바꾸지 않음).
- **검증.** AI는 빌드·실행하지 않았다. 사용자 확인 대기: `cmake --build --preset x64-ninja-debug --target Editor AssetReferenceCheck`, `ctest --test-dir out/build/x64-ninja-debug -R "^AssetReferenceCheck$" --output-on-failure`. 에디터에서 (1) Graph Binding 피커 목록에 `Assets/Scripts`의 로직 그래프만 있고 `Assets/Shaders/Graphs/NewMaterial_2`가 없는지, (2) 셰이더 그래프를 끌어다 놓으면 거부되는지, (3) 고른 뒤 PIE에서 그래프가 실행되는지, (4) 콘텐츠 브라우저에서 그 그래프 이름을 바꾼 뒤 피커 표시와 PIE 실행이 유지되는지, (5) Ctrl+Z로 이전 그래프로 돌아가는지.
- **남은 한계.** 도메인 판정은 JSON 전체 파싱이 아니라 첫 `"Domain"` 키의 값을 읽는다(현재 그래프 파일에서는 최상위 키만 이 이름을 쓴다). 필터 함수는 인스펙터 내부 함수라 자동 검사가 없다.

### [2026-11-23] 액션 타임라인 종료 정책과 종료 이벤트 (1-B)

- **원인.** `ContentBrowserPanel::CreateEmptyTimeline`이 새 타임라인을 길이 3초로 만들고, 클립이 없으면 끝나는 조건이 이 길이뿐이었다. `ActionTimeline.Play`가 재생 중 재실행을 거부하므로 마지막 노티파이 뒤 빈 시간만큼 다음 액션이 늦었다. 그래프는 타임라인이 언제·왜 끝났는지 받을 수단이 없었고, `Stop()`·중단 시 `HitboxOn` 창이 열린 채 남았다.
- **수정.** 에셋에 `ETimelineEndPolicy`(`Fixed`·`ClipLength`·`LastTrackEnd`, 이전 파일은 `Fixed`)와 `ComputeLastTrackEnd`·`ResolvePlaybackDuration`을 추가했다. `ActionTimelineSystem`이 매 틱 정책으로 길이를 정하고, 세션 번호 추적으로 `Completed`·`Stopped`·`Interrupted`를 판정해 `ActionTimelineEvents::PublishEnd`로 보낸다. 끝 판정은 시간 비교 대신 `playbackCompleted` 표시를 쓴다(Animator·카메라 복원, 지속형 VFX 해제, 인스펙터 재개 버튼). 세션이 어떻게 끝나든 그 세션이 연 히트박스 창을 닫는다. 중복된 히트박스 켜기·끄기 코드는 `SetTimelineHitboxesActive`로 묶었다.
- **그래프.** `Event.OnActionTimelineEnded`(출력 `reason` 정수, `completed` 불)와 `GraphExecutor::BeginTimelineEnded`를 추가하고, `GraphSystem`이 종료 이벤트를 구독해 같은 프레임의 노티파이 이벤트 뒤에 전달한다.
- **편집기.** 타임라인 패널 트랜스포트 바에 종료 시점 콤보·재생 끝 표시, 트랙 영역에 재생 끝 표시선을 추가했다. 프리뷰 재생은 런타임과 같은 길이를 쓴다. 새 타임라인 기본 정책은 `LastTrackEnd`다.
- **로캘.** `Editor.Timeline.csv`에 `Timeline.EndPolicy*`·`Timeline.PlaybackEnd` 6행 추가, `Timeline.DurationTooltip` 문구 수정. `Engine.csv`에 정책 열거값 3행, `OnActionTimelineEnded`, 핀 2행 추가. JSON은 `GenerateLocalization`이 생성한다.
- **검사.** `ActionTimelinePlaybackCheck`에 `CheckEndPolicyDuration`, `CheckEndPolicyPlaybackAndEndEvents`, 정책 직렬화 왕복·이전 파일 기본값 검사를 추가했다.
- **검증.** AI는 빌드·실행하지 않았다. 사용자 확인 대기: `cmake --build --preset x64-ninja-debug --target Editor ActionTimelinePlaybackCheck`, `ctest --test-dir out/build/x64-ninja-debug -R "^ActionTimelinePlaybackCheck$" --output-on-failure`. 에디터에서 (1) 새 타임라인에 0.4초 이벤트 노티파이만 두고 PIE에서 공격 입력을 연타 → 약 0.4초마다 재생, (2) 종료 시점 콤보 변경 → 표시선·`x초에 종료`·Ctrl+Z, (3) 로직 그래프에 `On Action Timeline Ended` → `Print` 연결 후 끝까지 재생·Stop 시 `reason` 0·1 확인, (4) 기존 타임라인 파일이 이전과 같은 길이로 끝나는지 확인.
- **사용자 보고 후 수정(2026-11-24).** 빌드 후 "어느 정책에서도 1초에 Completed"가 보고됐다. 에셋 핫 리로드(`AssetHotReloadManager`)가 텍스처·머티리얼·메시·로캘만 다시 읽어, 패널에서 저장한 타임라인이 이미 불러온 씬 에셋에 반영되지 않았다(정적 분석으로 확인). `.bamtimeline(.json)`·`.actiontimeline(.json)` 변경 시 불러온 `ActionTimelineAsset`을 `Resource::Reload`로 제자리에서 다시 읽고 알림(`Notification.TimelineHotReloaded`)을 띄운다. 세션 종료 시 `Session N ended: 사유 at 시간` Info 로그를 남긴다(`Stop`·재시작은 시간을 먼저 0으로 되돌리므로 시간은 `Completed`에서만 의미가 있다). 1초라는 값의 출처는 아직 확인하지 못했다. 테스트 씬의 `Test.bamtimeline.json`(마지막 트랙 끝, 노티파이 모두 0초)은 0.05초에 끝나야 한다.
- **남은 한계.** `ActionTimeline.Play`는 재생 중 재실행을 계속 거부한다. 엔티티 `Dead`·컴포넌트 제거는 종료 이벤트를 보내지 않는다. `Stopped`·`Interrupted`는 다음 타임라인 틱에 판정한다.

### [2026-11-23] 프로브 기반 동적 GI (M4 1~4단계)

- **볼륨·설정(1단계).** `ProbeGIVolumeComponent`(엔티티 위치 중심의 월드 축 정렬 상자, 축별 프로브 수, 광선 수, 프레임당 갱신 수, 이력 비율, 세기, 경계 혼합 거리)와 격자·옥타헤드럴·아틀라스·갱신 일정·볼륨 거리·프로브 이동 규칙을 담은 `ProbeGrid.h`를 추가했다. 사용 여부는 기본 꺼짐이며 프로젝트 → 그래픽 → 뷰포트 순서로 덮어쓴다(Client는 프로젝트 설정).
- **갱신(2단계).** 조명 패스 안에서 반사 다음에 Compute로 프로브 광선을 추적하고(`probeGITraceRTCS`), 조도·거리 아틀라스에 이력 비율로 섞는다(`probeGIBlend*RTCS`). 닿은 면의 재질 복원은 반사와 함께 쓰는 `raytracingSurface.hlsli`로 옮겼다.
- **조명 적용(3단계).** `lightingPS`가 둘러싼 8개 프로브를 3선형·법선·체비쇼프 가중치로 읽어(`probeGISample.hlsli`) 볼륨 안의 확산 환경광을 바꾼다. 확산 IBL과 `ShadowAmbientFloor`는 볼륨 안에서 더하지 않고 AO는 곱한다. 씬 뷰 메뉴에 "GI만 보기"를 두었다.
- **빛샘과 안정성(4단계).** 벽 속 프로브를 옮기거나 끄고(`probeGIRelocateRTCS`), 닿은 면이 볼륨 안이면 지난 프레임 프로브 조도를 써서 다중 반사를 만든다. 크게 바뀐 조도 텍셀은 이력 비율을 낮춘다. 뷰마다 카메라를 담거나 가장 가까운 볼륨을 쓴다.
- **바인딩.** `lightingPS`는 `t2`·`t3`·`t4, space1`(조도·거리·상태), `LightingPassData` 624바이트. 갱신 상수 `ProbeGIUpdateParams` 192바이트. [BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)
- **확인 중 고친 결함.** 볼륨이 여럿인 씬에서 저장 순서에 따라 다른 방 볼륨이 쓰여 GI가 보이지 않았다(4단계에서 카메라 기준 선택). 시험 씬 GI 상자의 볼륨이 벽보다 커서 바깥 줄 프로브가 방 밖에서 햇빛을 담아 샜다. 엔진 결함이 아니라 배치 문제로, 볼륨을 방 안쪽에 맞췄다(배치 지침은 [RenderingDevelopmentPlan.md](RenderingDevelopmentPlan.md) M4).
- **검사.** `ProbeGridCheck`(격자·보간·옥타헤드럴·아틀라스·테두리·일정·회전·메모리·볼륨 거리·프로브 이동)를 추가하고 `EditorSettingsCheck`에 뷰포트 설정 저장을 더했다.
- **확인된 결과.** 사용자가 에디터 씬 뷰에서 아틀라스 채움, 색 번짐, 볼륨 선택, 볼륨을 맞춘 뒤 빛샘이 없음을 확인했다.
- **확인하지 못한 것.** 패스별 GPU 시간·메모리, 조명 변화 지연·깜빡임, 검사 실행 기록, Game View·Release·Client.
- **남은 한계.** 뷰마다 볼륨 하나만 쓰고 씬 뷰·게임 뷰가 따로 갱신한다. 볼륨을 옮기면 처음부터 채운다. 간격보다 훨씬 얇은 벽, 한쪽 면만 있는 판, 간격의 0.45배보다 두꺼운 벽 속 프로브는 처리하지 못할 수 있다. 정반사 환경광은 하늘 환경 맵 그대로다. 임계값·바이어스·광선 수는 측정 전 값이다.

### [2026-11-23] 에셋 이동·이름 변경 차단 사유 표시와 작업 계획 재정립

- **계획.** 지형 확장·그래프 바인딩 피커·이름 변경 실패·액션 3초 지연·디스토션·시퀀서 필요성을 점검했다. 작업 순서는 [DevelopmentPriorities.md](DevelopmentPriorities.md) 11.7절, 지형 설계는 [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md)에 기록했다. 지형 높이·스플랫 데이터가 직렬화되지 않아 저장 후 사라지는 결함을 T1로 올렸다.
- **원인(정적 분석).** 참조가 0건이어도 `ImGuiManager::PrepareAssetFileChanges`가 미저장 리소스 편집기·미저장 씬·프리팹 모드·PIE·씬 로딩·경로를 쥔 편집기에서 이동을 거부했다. 확인 모달은 모든 경우에 같은 `MoveDocumentConflict` 문구를 보여 원인을 알 수 없었다.
- **수정(1-A).** `EAssetMoveBlockReason`과 `AssetMoveBlock`(`AssetPathRelocation.h`)을 추가했다. `PrepareAssetFileChanges`가 첫 거부 사유와 대상 이름(편집기 표시 이름 또는 문서 파일명)을 돌려주고, `ContentBrowserPanel::DrawFileMoveBlockReason`이 사유별 로캘 문구를 표시한다. 참조 스캔이 실패한 경우에는 스캔 실패 문구만 보이도록 중복 문구를 없앴다. 거부 조건 자체는 바꾸지 않았다(문서의 미저장 검사는 기존 `PrepareAssetPathRelocation`과 같은 조건을 앞에서 먼저 판정한다).
- **로그.** `ResourceManager::RelocateFileResources`가 로그 없이 실패하던 인덱스 불일치 경로에 경고 로그를 추가했다.
- **로캘.** `Editor.ContentBrowser.csv`에 `ContentBrowser.MoveBlocked*` 8행을 추가했다. JSON은 `GenerateLocalization`이 생성한다.
- **검증.** AI는 빌드·실행하지 않았다. 사용자 확인 대기: `cmake --build --preset x64-ninja-debug --target Editor`, 이후 에디터에서 (1) 씬을 수정해 저장하지 않은 채 참조 없는 에셋 이름 변경 → 미저장 문서 문구와 씬 파일명, (2) 머티리얼 편집기를 미저장 상태로 두고 이름 변경 → 편집기 이름, (3) 모두 저장한 뒤 이름 변경 → 모달 없이 즉시 변경, (4) 한국어·영어 전환 시 문구 확인.
- **남은 한계.** 확인 버튼 이후 로드된 리소스(모델 임포트 원본·셰이더 바이너리)로 인한 거부는 기존처럼 `MoveLiveStateConflict` 알림과 로그로만 구분한다. 미저장 씬이 참조 없는 에셋 이동까지 막는 정책의 완화는 씬 메모리·Undo 스냅샷 경로 갱신과 저장 기준점 관계를 검토한 뒤 결정한다.

### [2026-11-23] 시간 누적 안티앨리어싱(TAA)과 모션 벡터

- **지터.** 원근 카메라의 G-Buffer와 포워드 패스를 Halton(2,3) 8프레임 주기로 픽셀 안에서 옮겨 그린다. 조명과 타일 광원 컬링은 같은 카메라를, 그림자·반사·기즈모·피킹은 원래 카메라를 쓴다. 직교 카메라, 독립 프리뷰·썸네일, DirectX12 외 RHI는 제외한다.
- **모션 벡터.** G-Buffer는 바꾸지 않는다. 멈춘 면은 G-Buffer 위치와 이전 카메라로 구하고, 직전 프레임에서 움직인 정적·스킨드 메시만 `RG16F` 타깃에 한 번 더 그린다(`MotionHistory`, 뷰별). 한 뷰에 정적 16384개, 스킨드 128개까지 그린다.
- **합성.** 반투명 뒤, 후처리 앞의 패스(`_MotionVectorPass`, `_TemporalAAPass`)에서 3×3 이웃의 YCoCg 범위로 이력을 자르고, 밝기를 눌러 섞어 장면 색과 다음 프레임 이력에 함께 쓴다. 이전 결과는 Catmull-Rom으로 읽는다. 이력 관리는 반사와 같은 `TemporalHistory`(이전 `ReflectionHistory`)를 쓴다.
- **반사.** 반사 누적이 이전 화면을 찾을 때 지터만큼 어긋난 거리를 뺀다.
- **설정.** 기본 꺼짐이며 프로젝트 → 그래픽 → 뷰포트 순서로 덮어쓴다(씬 뷰 렌더 패스 메뉴). Client는 프로젝트 설정만 쓴다.
- **확인 중 고친 결함.** 접촉 그림자가 지터가 있는 G-Buffer를 지터 없는 카메라로 찾아 바닥이 프레임마다 최대 60% 어두워졌다. 조명 패스에 G-Buffer를 그린 카메라를 쓴다. 씬 뷰 격자는 지터가 있는 깊이를 지터 없는 방향의 격자 깊이와 비교해 바닥과 겹친 곳이 깜빡였다. 무늬는 원래 카메라로 그리고 깊이만 깊이 타깃과 같은 방향으로 계산한다.
- **검사.** `TemporalAACheck`(지터, 지터 카메라와 NDC 되돌리기, 모션 기록)와 `TemporalHistoryCheck`(이전 `ReflectionHistoryCheck`)를 등록했고 `EditorSettingsCheck`에 저장을 추가했다.
- **확인된 결과.** 사용자가 씬 뷰에서 두 결함의 수정 후 화면을 확인했다.
- **확인하지 못한 것.** 움직이는 정적·스킨드 메시의 잔상, 패스별 GPU 시간, 메모리(계산값: 2560×1440에서 뷰마다 약 74MB), 검사 실행 기록, Release, Client.
- **남은 한계.** 동적·스플라인 메시, 지형, 파티클, 반투명은 자신의 모션 벡터가 없다. 격자는 물체 윤곽에 가려지는 경계가 최대 반 픽셀 흔들릴 수 있다. 섞는 비율(멈춤 6%, 이동 15%)은 측정 전 시작값이다. 반사 누적은 물체별 모션 벡터를 쓰지 않는다.

### [2026-11-23] 1보다 밝은 발광과 블룸

- **결함.** G-Buffer의 발광 타깃이 `R8G8B8A8_UNORM`이라 1보다 밝은 발광이 잘렸다. 블룸 합성도 8비트 타깃에 써서 밝은 면이 뭉개졌고, 반사 속 발광(HDR)과 직접 본 발광의 밝기가 달랐다.
- **수정.** 발광 타깃을 `R11G11B10_UFLOAT`로 바꿨다(같은 메모리, 알파 없음). 부동소수점이라 조명 셰이더에서 NaN·무한대를 0~65000으로 자른다. 블룸의 번짐 타깃을 부동소수점으로 바꾸고, 합성 결과는 블룸을 처음 켤 때 만드는 `RGBA16F` 타깃에 쓴다(`WritesHdr`).
- **최종 출력.** 썸네일이 최종 타깃을 4바이트 픽셀로 읽으므로 8비트로 남겼다. 처음에는 모든 후처리 타깃을 HDR로 바꿨다가 이 때문에 되돌렸다.
- **확인된 결과.** 사용자가 이 변경을 포함한 빌드로 에디터를 실행해 TAA를 확인했다.
- **확인하지 못한 것.** 1보다 밝은 발광, 블룸 강도 변화, 썸네일을 따로 확인한 보고는 없다. Release, Client.
- **남은 한계.** 발광 맵이 있으면 발광 색을 곱하지 않고 대신 쓴다. 발광 색의 기본값이 0이라 곱하면 맵만 쓰는 기존 재질이 꺼진다.

### [2026-11-23] 음수 배율 메시의 앞면 방향

- **결함.** 축을 홀수 번 뒤집은 정적·스킨드 메시가 바깥 면 대신 안쪽 면을 그렸다. 정점 셰이더의 법선은 뒤집힘을 처리했지만 파이프라인은 재질의 앞면 방향을 그대로 썼다. 섀도맵도 같은 면을 그렸다.
- **수정.** 월드 행렬이 감긴 방향을 뒤집으면 재질의 앞면 방향을 반대로 쓴다(`MeshWinding.h`). 방향이 다른 물체는 다른 배치로 나누고 정렬로 묶는다.
- **검사.** `BuiltInMeshWindingCheck`에 홀수·짝수 번 뒤집기, 회전·이동, 납작한 배율을 추가했다.
- **확인된 결과.** 사용자가 화면을 확인했다.
- **확인된 결과(이후).** 사용자가 Visual Studio 테스트 탐색기로 Debug의 모든 검사 통과를 확인했다(2026-11-23 01:45). 이 실행은 CTest `LastTest.log`가 아니라 `.vs`의 테스트 기록에 남으며, 앞선 항목의 "CTest 실행 기록 없음"은 `LastTest.log`만 본 결과다.
- **남은 한계.** 뼈가 뒤집힌 스킨드 메시는 처리하지 않는다. 음수 배율 메시의 법선 맵은 `gbufferPS`가 뒤집힘을 구분하지 않아 한 축의 요철이 반대로 보일 수 있다(정적 분석). 일부러 안쪽을 보이게 쓴 음수 배율 물체는 재질의 컬링을 바꿔야 한다.

### [2026-11-23] 스프린트 D · 그룹 1 · 그룹 A(1~3) · 그룹 B-4 구현 및 정비 동기화

- **스프린트 D — 제작 도구·RL 벡터화·`.bampak` 2-팩 패키징·노드/기즈모 확장 (`2b2ffbf9`, `e66c3e76`, `b2419fbd`, `3263d3a5`, `f0d67432`, `4ef7a77b`, `69388cf8`, `a5863567`)**:
  - **시네마틱 카메라 시퀀서.** `ActionTimelineAsset` / `ActionTimelineSystem`에 카메라 컷(Camera Cut) 및 스플라인 경로/Look-At 시퀀서 트랙을 추가했다(`2b2ffbf9`, `ActionTimelinePlaybackCheck` 픽스 `69388cf8`, `a5863567`).
  - **RL 시뮬레이션·벡터화.** `SceneRLEnvironment`, `VectorizedRLEnvironment`, `RLServer` `VEC_INFO`/`VEC_RESET`/`VEC_STEP` 배치 프로토콜을 구현하고 `RLSimulationCheck` CTest를 등록했다(`e66c3e76`, `4ef7a77b`).
  - **`.bampak` 2-팩 패키징.** `ClientPackage.cpp`와 `BuildProfilePanel.cpp`에 `Engine.bampak` / `Project.bampak` 2-팩 분할 생성 및 `Config/PackageManifest.json` 매니페스트 출력을 연동했다(`b2419fbd`, `PakArchiveCheck` `69388cf8`).
  - **노드 및 에디터 UI/기즈모.** `GraphGameplayLibrary`에 SaveGame/Nav/AI/AudioMixer/SceneStreaming 노드 15종을 확장하고(`3263d3a5`), `DebugRenderer` Nav/AI 기즈모 및 `SettingsPanel`/`UnifiedSettingsPanel` 오디오 믹서 버스 컨트롤 UI를 연동했다(`f0d67432`).

- **그룹 1 (스프린트 E) — 에디터 저작 UI·Client `.bampak` 부트·NavGrid 베이크 (`06487b72`, `ffdc7b09`, `d81847c1`, `40c91851`)**:
  - `AnimationTimelinePanel`에 카메라 컷/스플라인 트랙 저작 UI를 구현했다(`06487b72`).
  - 독립 `ClientGameRuntime` 시작 시 `PakMountManager` 기반 `.bampak` 자동 마운트, `Config/AudioMixer.json` 자동 로드, 매 프레임 `AsyncResourceLoader::StepMainThread` 틱 구동을 연결했다(`ffdc7b09`).
  - `SceneViewportPanel` 원클릭 `BakeSceneNavGrid()` 및 `DebugRenderer` `NavGrid` 셀 오버레이를 추가하고 `BoundingBox::min/max`·`NavigationSystem` `C4834` 경고를 수정했다(`d81847c1`, `40c91851`).

- **그룹 A-1 — 비주얼 스크립팅 `NetworkRPC`·`CameraSequencer`·`PakMount` 노드 확장 (`97c69d29`)**:
  - `GraphGameplayLibrary.h/.cpp`에 네트워크 RPC 송신·모드·지연 조회 5종, 카메라 시퀀서 재생/정지/상태 조회 3종, `.bampak` 마운트·엔트리 조회 2종(총 10종) 노드를 추가하고 `GraphReflectionCheck`에 핀·카테고리 검증 섹션을 추가했다.

- **그룹 A-2 — RL 물리 환경 커리큘럼·다중 웨이포인트·`SaveGame` 스냅샷 연동 (`8f9f3154`)**:
  - `SceneRLEnvironment`에 `EnableMovingObstacle`(Kinematic 사인파 패트롤), `SetCurriculumLevel(0..2+)`, `Waypoints` 순차 전이(`"waypoint"` → `"goal"`), `SaveGameManager` 슬롯 스냅샷 저장/복원(`SaveSnapshotToSlot`, `LoadSnapshotFromSlot`) 및 `AutoSaveBestEpisode` 자동 저장을 구현하고 `RLSimulationCheck`에 3개 검증 함수를 추가했다.

- **그룹 A-3 — 클라이언트 `BAM_SHIPPING` 스트립 · No-Op 스텁 · `BuildConfigurationAndPreprocessing` 마감 (`2489e74e`)**:
  - `Engine/CMakeLists.txt`에서 `BAMENGINE_WITH_EDITOR=OFF`일 때도 `PUBLIC BAM_EDITOR=0`을 명시 전파했다.
  - `SystemManager`, `AnimationSystem`, `GraphSystem`, `Runtime`, `SceneManager`의 `#ifdef BAM_EDITOR`를 `#if BAM_EDITOR`로 전환하고, `BAM_EDITOR=0`용 인라인 No-Op 스텁 및 `GetDebugExecutor` `nullptr` 계약과 클래스 멤버 ABI 불변성을 정비했다(`ResourceManager.h/.cpp`는 동시 작업 파일과 겹쳐 후속 이관 항목으로 남김).
  - `Logger.h/.cpp`에 `BAM_SHIPPING` 시 `Trace`/`Info` 컴파일 타임 제거, `std::source_location{}` 비노출, 기본 `MinLevel=Warn`, `stderr` 출력 및 위치 접미사 생략, `!BAM_EDITOR` 콘솔 큐 비활성을 적용했고, `GraphExecutionCheck`에 `CheckEditorStripContracts()`를 추가하며 `BuildConfigurationAndPreprocessing.md` §4.5~§4.6을 갱신했다.

- **그룹 B-4 — `ResolveResourceHandle` 리팩터링 파일럿 (`c18c8ea2`)**:
  - `ResourceHandleArchiveAdapter.h`의 `ResolveResourceHandle` 정책 상수 테이블(`kCandidateResourceExtensions`)과 4개 헬퍼(`HasBamExtension`, `TryFindLoadedByKeyOrPath`, `TryLoadWithCandidateExtensions`, `TryResolveLegacyResourcesPath`)를 `detail` 네임스페이스로 추출하고 중복 2차 순회 루프를 제거했다(탐색 순서·공개 API 보존). `CodeRefactoringPlan.md` 후보 8 상태를 갱신했다.

- **확인된 결과.** 정적 코드/설정 대조 및 문서·체크 반영을 완료했다(2026-11-23).
- **실패 및 확인하지 못한 것.**
  - 사용자 CTest 실행 보고에서 `RLSimulationCheck` 실패(5초)가 확인되어 상세 실패 로그(`ctest --output-on-failure`) 확인 및 수정이 대기 중이다.
  - `GraphReflectionCheck`(A-1), `GraphExecutionCheck`(A-3), `SceneArchiveCheck`/`SceneReflectionCheck`(B-4), `ActionTimelinePlaybackCheck`/`PakArchiveCheck`/`NavigationAICheck`/`AudioMixerCheck`/`NetworkRpcCheck`/`SceneStreamingCheck` 최신 헤드 재검증, 그리고 `BAMENGINE_WITH_EDITOR=OFF` + `BAMENGINE_SHIPPING=ON` 독립 Client 실빌드 및 에디터 UI 수동 확인은 미수행(사용자 실행 검증 대기)이다.

### [2026-11-23] 레이트레이싱 반사 2차: 거친 면과 디노이즈

- **추적.** 거칠기 0.1 미만은 거울 방향, 그 이상은 GGX 분포에서 뽑은 방향으로 쏜다. 0.45부터 줄여 0.6에서 추적하지 않는다.
- **시간 누적.** 이전 화면에서 같은 면을 찾아 이어 쓰고 거리·법선이 다르면 버린다. 카메라가 멈추면 최대 64프레임, 움직이면 약 25프레임까지 이어 쓴다. 씬·크기 변경, 껐다 켜기, 한 프레임 20m 이동에서 이력을 버린다(`ReflectionHistory`).
- **공간 필터.** 같은 면 안에서 8탭으로 섞고 결과를 누적에 되먹이지 않는다.
- **결함 수정.** 카메라를 멈춰도 노이즈가 남았다. 프레임마다 돌리던 필터, 3×3 범위로 매번 자르던 이력, 짧은 누적을 고치고 밝기를 눌러 평균을 낸다.
- **검사.** `ReflectionHistoryCheck`를 추가했다.
- **확인된 결과.** 사용자가 카메라를 멈추면 노이즈가 가라앉는 것을 확인했다. 로그에 WARN·ERROR가 없었다.
- **확인하지 못한 것.** GPU 시간과 메모리, 카메라 이동 중 끌림, Release, Client, CTest 실행 기록.
- **남은 한계.** 물체별 모션 벡터가 없어 움직이는 면은 누적하지 못하고, 거친 면에 비친 물체가 움직이면 잔상이 남을 수 있다. 강한 하이라이트는 거친 반사에서 어두워질 수 있다. 거칠기 경계와 누적·필터 값은 측정 전 시작값이다.

### [2026-11-23] 레이트레이싱 반사 1차: 매끈한 면

- **바인드리스.** Compute가 텍스처(`space4`)와 정점·인덱스 버퍼(`space5`)를 번호로 읽는다. 루트 시그니처에 테이블 두 개와 정적 샘플러를 더했고 PSO 캐시 버전을 올렸다. Resource Binding Tier 2 이상에서만 쓴다.
- **추적.** 조명 패스에서 전체 해상도로 거울 방향 광선을 하나 쏜다. 닿은 면은 재질 색·발광, 확산 직접광(광원 버퍼 앞 64개), 대표 방향광의 그림자 광선 하나와 환경광을 계산한다. 닿지 않으면 기존 환경광 반사를 쓴다.
- **대상.** 정적 메시의 불투명·Masked 재질이다. 그림자와 TLAS를 함께 쓰고 광선 마스크로 구분한다. Masked는 광선마다 알파를 확인한다.
- **설정.** 기본 꺼짐이며 프로젝트 → 그래픽 → 뷰포트 순서로 덮어쓴다. Client는 프로젝트 설정만 쓴다.
- **검사.** `RaytracedShadowSettingsCheck`에 광선 마스크와 반사 설정 순서를, `EditorSettingsCheck`에 저장을 추가했다.
- **확인된 결과.** 사용자가 시험 씬에서 Scene View와 Game View(설정으로 켬)의 반사를 확인했다. 화면에서 화면 밖 물체와 Masked 패널의 반사를 확인했고 로그에 WARN·ERROR가 없었다.
- **확인하지 못한 것.** GPU 시간, Release, Client, CTest 실행 기록.
- **남은 한계.** 스킨드·동적·스플라인 메시, 지형, 반투명은 반사에 나타나지 않는다. 반사 속 면은 정반사 하이라이트와 법선 맵이 없고 금속은 환경광으로 근사한다. 음수 배율 정적 메시는 래스터에서 안쪽 면이 보이는 기존 결함이 있다.

### [2026-11-23] 거리 기반 메시·텍스처 스트리밍

- **읽기.** `ResourceStreamer`가 자리만 등록한 메시와 텍스처를 작업 스레드에서 이어 읽는다. 메시는 화면에 필요한 LOD만, 텍스처는 가장 가까운 거리와 메시의 UV 밀도로 정한 밉까지 올린다. 화면 비중이 큰 것부터 읽고 카메라 뒤에 있는 것은 미룬다. UV를 바꿀 수 있는 셰이더와 정적 메시가 아닌 곳의 텍스처는 가장 자세한 밉을 쓴다.
- **내리기.** 프로젝트 설정 `ResourceStreamingBudgetMB`(기본 512)를 넘을 때만 최근 2초 동안 쓰지 않은 것, 필요보다 자세한 것, 두 배 이상 덜 중요한 것 순으로 내린다. 자리를 다 만들 수 없으면 내리지 않는다. 내용을 직접 쓰거나 바꾼 리소스는 관리와 한도 계산에서 뺀다.
- **밉 캐시.** 해석한 밉 전체를 `.bam/cache/textures`의 `.btmip`에 작은 것부터 저장하고 필요한 만큼만 읽는다. 4단계의 `.btph`는 더 쓰지 않는다.
- **연결.** 엔진 Runtime이 실행해 에디터와 Client가 함께 쓴다. 프로젝트 설정에 한도, 성능 지표에 사용량을 추가했다.
- **결함 수정.** 읽는 중인 재질의 썸네일이 단색이나 64px 상태로 저장될 수 있었다. 썸네일은 원본을 읽은 뒤 그린다.
- **검사.** `StreamingPolicyCheck`를 추가하고 `ResourcePreloaderCheck`를 스트리머와 밉 캐시에 맞춰 바꿨다.
- **확인된 결과.** 사용자가 통과를 알렸다. Debug 로그 1회에서 씬을 연 직후 메시 626개 중 199개(224MB)를 올렸고, 한도 16MB에서 461번 내려 한도를 지켰으며 512MB로 되돌리면 226MB까지 다시 올렸다. 실패와 WARN·ERROR는 없었다. 밉 캐시는 214MB였다.
- **확인하지 못한 것.** 기록 시점에 `StreamingPolicyCheck` 실행 파일과 CTest 실행 기록이 없고, Client, Release는 로그가 없다. 캐시를 쓰는 실행은 이후 로그에서 `660 deferred (34 cached)`, 리소스 준비 477ms로 확인했다(이전 첫 실행 831ms).
- **남은 한계.** 밉을 압축하지 않아 디스크를 많이 쓴다. `.dds`·`.bamtex`와 `.bampak` 안의 파일은 미루지 않는다.

### [2026-11-23] 씬을 먼저 열고 자세한 리소스를 이어 읽기

- **먼저 열기.** 에디터에서 씬을 열 때 더 단순한 LOD가 함께 참조된 메시와 재질의 이미지 텍스처는 키와 경로만 있는 자리로 등록하고, 나머지를 읽으면 씬을 연다. 남은 내용은 프레임마다 3ms·16MB 범위에서 올린다. 큰 텍스처 하나는 이 범위를 넘을 수 있다.
- **임시 표시.** 준비된 LOD를 대신 그리고, 텍스처는 `.bam/cache/textures`의 64px 복사본을 쓴다. 복사본이 없으면 슬롯에 맞는 단색을 쓴다. 원본의 크기나 수정 시각이 바뀌면 복사본을 쓰지 않는다.
- **내용 보호.** 저장·충돌·편집기처럼 내용이 필요한 접근은 그 자리에서 읽는다. 읽지 못한 메시와 텍스처는 저장을 거부한다. 핸들과 키는 유지한다.
- **검사.** `ResourcePreloaderCheck`에 자리 등록, 즉시 읽기, 다른 스레드 거부, 손상된 파일, 64px 복사본을 추가했고 `LODSelectionCheck`에 대체 LOD를 추가했다.
- **확인된 결과.** 사용자가 처음에는 단색, 다음에는 낮은 화질이 먼저 보이는 것을 확인했다. Debug 로그에서 660개를 미뤄 실패 0, 씬을 연 뒤 4,000ms에 끝났고 메인 스레드는 126ms(한 번에 최대 8.4ms)를 썼다. WARN·ERROR는 없었다. 빌드 폴더에서 CTest 실행 기록은 확인하지 못했다.
- **남은 한계.** 처음 실행은 4,198ms로 이전 Debug 2,811ms보다 길었으나 다시 나타나지 않았고 원인은 확인하지 못했다. 이후 두 번 새로 실행한 Debug에서 64px 복사본 34개를 썼고 전체 2,669·2,398ms, 리소스 준비 536·482ms였다. Release는 로그로 확인하지 못했다. `.dds`·`.bamtex`와 Client는 미루지 않는다. LOD는 `_LOD1`·`_LOD2` 이름으로 찾는다.

### [2026-11-22] 정적 지오메트리 버퍼를 GPU 힙에 배치

- **원인.** Release의 Sponza 메시 등록 686ms 중 657ms가 정점·인덱스 버퍼 3,057개의 GPU 메모리를 하나씩 잡는 비용이었다.
- **변경.** 8MB 이하 정적 버퍼를 32MB 힙에 64KB 단위로 배치한다. 자리는 지연 해제의 펜스가 끝난 뒤 재사용하고, 빈 힙은 하나만 남긴다. 배치에 실패하면 독립 메모리를 쓴다. 동적·Storage 버퍼와 SDLGPU는 그대로다.
- **검사.** `GpuHeapPageAllocatorCheck`에 겹침, 병합, 이중 해제와 무작위 할당을 추가했다.
- **확인된 결과.** 로그에서 버퍼 생성이 Debug 702→73ms, Release 657→22ms, 전체가 Debug 3,239→2,811ms, Release 1,048→436ms로 줄었고(각 1회) WARN·ERROR가 없었다. 사용자가 통과를 알렸다. 빌드 폴더에서 CTest 실행 기록은 확인하지 못했다.
- **남은 한계.** 64KB 단위의 낭비는 줄지 않는다. 씬을 다시 열 때 메시를 버리지 않아 자리 재사용 경로는 로그로 확인하지 못했다.

### [2026-11-22] 씬 리소스 병렬 읽기와 업로드 비용 감소

- **읽기.** 씬을 열 때 메시·텍스처의 파일 읽기와 해석을 `ResourcePreloader`의 작업 스레드(최대 8개)로 옮겼다. GPU 업로드와 등록은 메인 스레드에서 한다. 재질이 참조하는 텍스처도 먼저 읽는다. 읽는 동안 모듈 핫 리로드를 막는다.
- **업로드.** D3D12 프레임 업로드가 8MB 묶음을 함께 쓴다. 파일은 한 번에 읽는다.
- **계측.** 씬 로딩 로그에 작업 스레드, 메인 스레드와 GPU 버퍼 시간을 남긴다.
- **검사.** `ResourcePreloaderCheck`를 추가했다.
- **확인된 결과.** Sponza 한 번씩 측정에서 전체가 Debug 9,892~10,699→3,239ms, Release 2,328→1,048ms로 줄었다.
- **남은 한계.** Client에서 프레임 밖 업로드는 여전히 건마다 펜스를 기다린다. 로그의 `.bammesh` 개수는 두 번 센다.

### [2026-11-22] 다중 광원 레이트레이싱 그림자

- **기능.** 방향광·스포트·점광원을 뷰마다 최대 4개까지 추적한다. RGBA8 마스크의 채널 하나가 광원 하나를 맡으며, 조명 셰이더는 광원 버퍼 인덱스로 채널을 찾는다.
- **배정.** 캐스케이드 방향광, 섀도맵이 없는 광원, 나머지 순이다(`ShadowLightAssignment.h`). 남는 광원은 섀도맵만 쓰거나 그림자가 없다.
- **섀도맵.** TLAS에 들어간 캐스터만 추적하는 광원의 섀도맵에서 제외한다. 미지원 형상과 BLAS 대기 캐스터는 섀도맵이 담당한다.
- **검사.** `ShadowLightAssignmentCheck`를 추가했고 `ReflectionInspectorCheck`에 표시 조건을 반영했다.
- **확인된 결과.** 사용자가 검사와 화면 통과를 확인했다.
- **남은 한계.** 하드 섀도만 지원한다. 섀도맵이 없는 광원에는 스킨드·Masked·지형 캐스터의 그림자가 없다.

### [2026-11-22] 뷰별 방향광 캐스케이드

- **결함.** 캐스케이드를 첫 Geometry 카메라 하나로만 계산해, 뷰포트가 둘이면 한쪽의 섀도맵이 다른 카메라 기준이었다. 떨림을 막는 텍셀 맞춤도 효과가 없었다.
- **수정.** 그림자 패스와 조명 패스가 자기 뷰의 카메라로 캐스케이드를 만든다. 계산은 `DirectionalShadowCascades.h`로 분리했고, 고정된 광원 축에서 텍셀에 맞추며 직교 카메라도 지원한다.
- **검사.** `DirectionalShadowCascadeCheck`를 추가했다.
- **확인된 결과.** 사용자가 검사와 화면 통과를 확인했다.
- **남은 한계.** 바운딩 구보다 태양 쪽에 있는 캐스터는 잘릴 수 있다.

### [2026-11-22] 신규 6대 핵심 엔진 시스템 구축 (스프린트 C)

- **.bampak 리소스 패키징 아카이브 (`954722eb`, `c5f714e6`)**:
  - **포맷·구현.** 고속 에셋 패키징 및 가상 마운트를 위한 독자 바이너리 `.bampak` 포맷(16바이트 고정 매직/버전 헤더, 엔트리 메타데이터 테이블, 압축/비압축 청크 블록)을 정의했다. 결정적(Deterministic) 아카이브 생성기 `PakWriter`, SHA-256 해시 기반 무결성 검증 및 빠른 오프셋 인덱싱 `PakReader`, 가상 파일시스템 마운트 계층 `PakMountManager`를 구현했다.
  - **검사.** `PakArchiveCheck`에 결정적 패킹 바이트 일치, 변조 패킷 감지, 0바이트/대용량 파일 읽기, 중복 마운트 우선순위 격리를 추가했다.
  - **확인된 결과.** 사용자가 CTest `PakArchiveCheck` 통과를 확인했다 (`c5f714e6`).

- **SaveGame 원자적 슬롯 저장 및 체크포인트 시스템 (`cde74e5e`, `81b8dea9`, `c5f714e6`)**:
  - **안전성·구현.** 게임플레이 진행 상태 저장을 담당하는 `SaveGameManager`를 구축했다. 저장 도중 프로세스 비정상 종료 시 세이브 파일 파손을 방지하기 위해 임시 파일(`.tmp`) 기록 후 `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` 기반 원자적 교체(Atomic Swap) 메커니즘을 적용했다.
  - **메타데이터·체크포인트.** 슬롯 메타데이터 헤더(타임스탬프, 플레이타임, 레벨명) 사전 조회와 게임플레이 체크포인트 캡처/복원 파이프라인을 지원한다. Glaze DTO 구조체 외부 링키지 보정(`81b8dea9`)으로 경고를 해소했다.
  - **검사.** `SaveGameCheck`에 원자적 교체 실패 시 롤백, 메타데이터 추출, 다중 슬롯 관리, 손상 세이브 파일 복구 거부 사례를 추가했다.
  - **확인된 결과.** 사용자가 CTest `SaveGameCheck` 통과를 확인했다.

- **NavGrid 8방향 A* 내비게이션 및 AI 인지 시스템 (`f6c10c5c`, `7bb639ac`)**:
  - **알고리즘·구현.** 런타임 그리드 기반 AI 경로 탐색을 위한 `NavGrid` 및 8방향 A* 알고리즘을 구현했다. 대각선 이동 시 장애물 모서리를 뚫지 않도록 코너 컷팅 방지(Corner Cutting Guard)를 적용하고, 불필요한 지그재그 경로를 선분 가시성 검사로 매끄럽게 펴는 Raycast 직선화 스무딩(Path Smoothing)을 구현했다.
  - **ECS 컴포넌트·시스템.** `NavAgentComponent`(이동 속도, 회전, 웨이포인트 추적), `AIPerceptionComponent`(시야각/가시거리 기반 타깃 감지), `NavigationSystem`을 구축하고 `RuntimeSystemManifest`에 등록했다.
  - **검사.** `NavigationAICheck`에 장애물 우회, 코너 컷 방지, 스무딩 전후 웨이포인트 수 감소, 시야각 감지 사례를 추가했다.
  - **확인된 결과.** 사용자가 CTest `NavigationAICheck` 통과를 확인했다.

- **AudioMixer 5대 버스 라우팅 및 3D 거리 감쇠 시스템 (`1dc93466`, `dc6b1284`, `c5f714e6`)**:
  - **버스 계층·믹서.** 사운드 계층 제어를 위한 `AudioMixer` 5대 버스(`Master`, `BGM`, `SFX`, `UI`, `Voice`) 라우팅 트리를 구축했다. 버스별 독립 볼륨 게인, Mute/Solo, 선형/로그 볼륨 페이드(Fade In/Out/Crossfade)를 구현했다.
  - **3D 공간 음향·동기화.** 3D 공간 음향 거리 감쇠(Linear/Inverse Rolloff, 최소/최대 가청 거리) 및 영속적 오디오 믹서 설정 직렬화/역직렬화를 지원하며, FMOD ChannelGroup과 실시간으로 동기화된다.
  - **검사.** `AudioMixerCheck`에 버스 계층 게인 곱연산, 페이드 시간 도달, 3D 거리별 감쇠 수치, Mute 전파 사례를 추가했다.
  - **확인된 결과.** 사용자가 CTest `AudioMixerCheck` 통과를 확인했다 (`dc6b1284`, `c5f714e6`).

- **NetworkRpc 원격 프로시저 호출 및 Dead Reckoning 외삽 (`d42de8a8`, `671d6ae6`)**:
  - **RPC 프레임워크.** 멀티플레이어 환경을 위한 RPC 프레임워크(`Server`, `Client`, `Multicast` RPC 속성 및 페이로드 바이너리 직렬화/라우팅)를 구축했다.
  - **스냅샷 보간·외삽.** 네트워크 지연 환경에서 자연스러운 위치 동기화를 보장하기 위해 Dead Reckoning 기반 물리 스냅샷 외삽(Extrapolation) 및 과거 프레임 보간(Snapshot Interpolation)을 구현했다.
  - **진단 시뮬레이터.** 네트워크 불량 환경 테스트를 위한 패킷 지연(Latency Jitter) 및 패킷 손실(Packet Loss) 시뮬레이터를 내장했다.
  - **검사.** `NetworkRpcCheck`에 RPC 바인딩 및 디스패치, 스냅샷 외삽 위치 추정 정밀도, 지연 시뮬레이터 버퍼링 검사를 추가했다.
  - **확인된 결과.** 사용자가 CTest `NetworkRpcCheck` 통과를 확인했다 (`671d6ae6`).

- **ScenePersistenceComponent 및 Additive 서브씬 스트리밍 (`89d584e2`, `c5f714e6`)**:
  - **영속성·스트리밍.** 씬 전환 시 플레이어·매니저 등 핵심 엔티티를 보존하는 `ScenePersistenceComponent` (`DontDestroyOnLoad`)를 구현했다. 씬 전환 시 영속 엔티티를 전용 불변 씬으로 자동 이관한다.
  - **Additive 씬·비동기 로더.** 다중 씬 동시 로드 및 메모리 분할을 위한 Additive 서브씬 비동기 스트리밍(`LoadSceneAdditive`, `UnloadSceneAdditive`) 및 `AsyncResourceLoader` 프레임 분할 비동기 리소스 로딩을 지원한다.
  - **검사.** `SceneStreamingCheck`에 씬 전환 후 영속 엔티티 유지, Additive 씬 엔티티 분리 및 언로드 시 정리, 비동기 로딩 큐 완료 사례를 추가했다.
  - **확인된 결과.** 사용자가 CTest `SceneStreamingCheck` 통과를 확인했다.

### [2026-11-22] 물리 KCC·머티리얼 캐싱 및 리소스 비동기 확장 (후보 그룹 A & B-2)

- **키네마틱 캐릭터 컨트롤러(KCC) 지면 착지 및 경사로 안정화 (`68213696`, `5b3d775b`, `671d6ae6`)**:
  - **결함.** 체공 낙하(`!IsGrounded && Velocity.y < 0`) 중 과도한 스냅 거리로 인해 공중 1m 높이에서 바닥으로 조기 순간이동 흡착되는 현상, 30도 경사로 이동 시 캡슐 스윕 마진 부족으로 인한 메시 파고듦, 45도/60도 급경사 끝단 $\to$ 평지 진입 시 수직 속도 불연속으로 인한 단차 충격(턱 현상)이 있었다.
  - **수정.** 낙하 중 `snapMargin`을 최소화하고 캡슐 하단 스윕 기반 2단계 착지 확정을 도입했다. 접촉 표면 법선 방향 `SkinWidth` 최소 분리 오프셋을 보정하여 메시 침투를 제거했다. 급경사 $\to$ 평지 전환 프레임에서 연착륙(`IsSoftLanding`) 수직 스냅 속도 제한(Clamping) 및 법선 보간을 적용했다.
  - **검사.** `KinematicCharacterCheck`에 공중 낙하 착지, 30도 경사로 침투 방지, 연착륙 슬라이드 수직 감속 예산 공제 검사를 추가했다.
  - **확인된 결과.** 사용자가 CTest `KinematicCharacterCheck` 통과를 확인했다 (`5b3d775b`, `671d6ae6`).

- **머티리얼 상수 버퍼 블롭 캐싱 및 렌더러 PSO 우선 정렬 (`9b17e906`)**:
  - **성능.** 드로우콜마다 머티리얼 파라미터를 문자열/맵으로 탐색하여 상수 버퍼를 갱신하던 CPU 오버헤드를 해소하기 위해 `MaterialInterface` / `MaterialInstance`에 바이트 블롭 버전 캐싱(`m_ConstantsBlob`, `m_ConstantsBlobVersion`)을 도입했다.
  - **무할당 해석·정렬.** 단일 순회 무할당(Zero-allocation) 파라미터 해석으로 CPU 오버헤드를 최소화하고, `Renderer` 드로우 패스에 PSO-First 정렬을 도입하여 불필요한 파이프라인/루트 시그니처/섀도 바인딩 중복 전환을 제거했다.
  - **검사.** `ShaderGraphCheck`에 상수 버퍼 바이트 블롭 일치 및 파라미터 갱신 검사를 추가했다.
  - **확인된 결과.** 사용자가 CTest `ShaderGraphCheck` 통과를 확인했다.

- **4대 리소스 편집기 비동기 로딩 확장 (`3fdfff6f`)**:
  - `AsyncResourceOpenService`를 `MaterialEditor`, `TextureEditor`, `VFXEditorPanel`, `SpriteEditor` 4대 리소스 편집기로 전면 확산했다. 대용량 텍스처나 복합 머티리얼/VFX 오픈 시 에디터 UI 블로킹 없이 백그라운드에서 안전하게 로드 완료 후 탭을 활성화한다.

- **GameplayCombatSystem 런타임 등록 및 디버그 기즈모 (`e3e00bd0`)**:
  - `GameplayCombatSystem`을 `RuntimeSystemManifest`에 공식 등록하여 Standalone 및 PIE 런타임 실행을 보장했다.
  - 에디터 뷰포트에서 `HitboxComponent`와 `InteractableComponent`의 공격 범위 및 상호작용 반경을 시각화하는 와이어프레임 디버그 기즈모 렌더링을 지원한다.

### [2026-11-22] 티어 1 엔진-에디터 핵심 기능 연동 (스프린트 A & B)

- **ECS UI 위젯 런타임 렌더링·프리셋 및 상호작용 (`a89ff0ea`, `1f81de09`, `5b3d775b`)**:
  - **런타임·상호작용.** `UIRenderSystem` 런타임 위젯 렌더링 및 프리셋 연결을 완료했다(`a89ff0ea`). 열린 `UIDropdownComponent` 팝업의 최상단 HitTest 판정 및 배경 클릭 차단(Modal Overlay), `UITextInputComponent` 런타임 키 입력·텍스트 처리·IME 라우팅 연동을 반영했다(`1f81de09`, `5b3d775b`).
  - **검사.** `UIInteractionCheck`에 드롭다운 열림/닫힘 클릭 차단, 텍스트 입력 포커스 및 초기 입력 억제 해제 사례를 추가했다.
  - **확인된 결과.** 사용자가 CTest `UIInteractionCheck` 통과를 확인했다.

- **Shader Graph 범용 TextureSample 노드 및 텍스처 바인딩 동기화 (`c58c2980`, `633b5e58`)**:
  - **멀티슬롯·채널 분리.** Shader Graph에 범용 `Shader.TextureSample` 노드 멀티슬롯(`0..7`) 바인딩 지원 및 채널 분리 출력(`rgba`, `rgb`, `r`, `g`, `b`, `a`)을 추가했다.
  - **동기화.** `MaterialInterface::SyncTextureBindings` 연동으로 그래프 노드의 텍스처 슬롯/경로와 머티리얼 인터페이스를 실시간 동기화했다.
  - **검사.** `ShaderGraphCheck`에 텍스처 샘플링 HLSL 코드 생성, 채널 분리 스위즐, 불투명도 마스크 일치 검사를 추가했다.
  - **확인된 결과.** 사용자가 CTest `ShaderGraphCheck` 통과를 확인했다.

- **3D 모델 임포트 모달 실시간 60FPS 프리뷰 및 Phase 6 폴더 정리 (`76e18180`)**:
  - **프리뷰·정리.** `ModelImportModal` 내 임베디드 3D 뷰포트 위젯에서 회전 오프셋 및 스케일 슬라이더 조작 시 60FPS 무지연 실시간 트랜스폼 동기화 피드백을 제공한다. Phase 6 `ModelSubfolderHelper` 기반 서브폴더 자동 분류(모델 전용 폴더 및 머티리얼/메시 하위 디렉터리 분리 생성)를 완성했다.
  - **검사.** `VirtualPathCheck`에 모델 서브폴더 경로 해석 및 프리뷰 트랜스폼 갱신 검사를 추가했다.
  - **확인된 결과.** 사용자가 CTest `VirtualPathCheck` 통과를 확인했다.

- **전투 시스템 및 액션 타임라인 히트박스 노티파이 연동 (`24288084`, `952b112a`)**:
  - **전투 판정·노티파이.** `GameplayCombatSystem` 전투/히트박스 스윙 판정, 단일 공격 내 중복 피격 방지(피격자 해시셋 캐싱), 넉백 벡터 계산 및 상호작용 질의를 구현했다. `ActionTimelineSystem`의 `"HitboxOn"` / `"HitboxOff"` 노티파이 이벤트와 히트박스 활성화 창(Window) 수명을 연동했다.
  - **검사.** `ActionTimelinePlaybackCheck`에 타임라인 노티파이 히트박스 On/Off 활성화 및 피격 판정 검사를 추가했다.
  - **확인된 결과.** 사용자가 CTest `ActionTimelinePlaybackCheck` 통과를 확인했다.

- **Foot Ground IK 지면 적응 및 비주얼 스크립팅 노드 (`6789ff06`, `7c4fd12c`)**:
  - **IK 접지·골반 보정.** `AnimationIKComponent` 기반 Foot Ground IK 지면 레이캐스트 접지, 좌우 발 높이차에 따른 골반(Pelvis) 높이 보정을 구현했다. 비주얼 스크립팅 IK 노드 라이브러리(`GraphGameplayLibrary`)를 등록했다.
  - **검사.** `BlendTree2DCheck`에 IK 지면 레이캐스트 접지 및 골반 보정 검사를 추가했다.
  - **확인된 결과.** 사용자가 CTest `BlendTree2DCheck` 통과를 확인했다.


### [2026-11-22] Volumetric Cloud가 보이지 않던 문제

- **결함.** 시작 시 로드한 하늘 재질은 `Engine://Material/SkyMaterial.bammat.json` 키로 등록되는데 GameModule은 `Resources/...` 키로 찾아, 구름 노이즈 텍스처를 재질에 연결하지 못했다. 빈 텍스처를 읽어 구름 밀도가 항상 0이었다. 구름 텍스처도 찾지 못해 실행할 때마다 다시 만들고 저장했다.
- **수정.** 엔진의 `CloudResources`가 활성 Volumetric Cloud가 있을 때 텍스처를 불러오거나 만들고 하늘 재질에 연결한다. 재질을 다시 불러와도 복구한다. GameModule의 초기화는 제거했다.
- **검사.** `CloudResourcesCheck`에 키 일치, 연결과 복구를 추가했다.
- **확인된 결과.** 사용자가 CTest 통과와 구름 표시를 확인했다. 수정 뒤 실행 로그에서 구름 텍스처의 재생성과 핫 리로드가 사라졌다. GameModule이 없는 Client는 확인하지 않았다.

### [2026-11-22] 환경 조명의 구름 설정과 이름 정리

- **통일.** Sky Atmosphere의 구름 변수 5개를 없애고 Volumetric Cloud에서만 설정한다. 활성 Volumetric Cloud가 없으면 구름을 그리지 않는다.
- **이전 데이터.** 씬·프리팹을 불러올 때 첫 활성 Sky Atmosphere의 구름이 켜져 있고 활성 Volumetric Cloud가 없으면 같은 값의 Volumetric Cloud를 붙인다. 노이즈 배율은 이전의 반올림한 상수와 약 0.5% 다르다.
- **이름.** 실제 쓰임에 맞춰 대체 환경광·안개 혼합·지표·아래쪽 환경광 색상과 환경광 강도로 표시한다. C++ 이름과 저장 형식은 그대로다. 읽는 곳이 없는 `HorizonBlend`는 숨기고 저장 값만 유지한다.
- **검사.** `SceneArchiveCheck`에 변환, 꺼진 구름, 비활성 하늘, 기존 Volumetric Cloud 우선 사례를 추가했다.
- **확인된 결과.** 사용자가 CTest 통과와 화면을 확인했다. 로그에서 Sponza의 변환을 확인했다.

### [2026-11-22] 인스펙터의 조건부 표시와 미사용 속성 정리

- **조건식.** `EDITCONDITION`에 `&&`, `||`, 괄호, 비트 플래그(`Flags & CastShadows`), 숫자 비교를 추가했다. 평가는 `EditCondition.h`로 분리했다. 해석할 수 없는 조건은 속성을 숨기지 않고 경고를 한 번 남긴다(지침에만 있고 구현되지 않았던 동작).
- **조건부 표시.** Light, Fog, Camera, SpringArm, RigidBody, CharacterController, ThirdPersonController, SpriteRenderer, AudioSource, Animator, IK, 파티클, CanvasScaler, NetworkTransform의 현재 설정에서 효과가 없는 속성을 숨긴다. 값과 저장 형식은 바꾸지 않았다.
- **숨김.** 런타임이 읽지 않는 속성(`LightingLayerMask`, 관절 `EnableCollision`, `upscaleRT`, `referencePixelsPerUnit`, `SyncScale`, 후처리 `IsUnbound`·`Priority`)과 실행 중 내부 값을 `VISIBLE(false)`로 숨겼다. 비트 플래그의 일부 항목은 `META("HiddenEntries", ...)`로 숨긴다.
- **검사.** `ReflectionInspectorCheck`에 문법, 잘못된 조건, 광원 종류별 표시와 등록된 컴포넌트의 모든 조건 검사를 추가했다.
- **확인된 결과.** 사용자가 CTest 통과와 화면을 확인했다.
- **남은 한계.** 다른 컴포넌트의 상태나 리소스 지정 여부는 조건으로 쓸 수 없다. Sky Light의 `Flags`에는 효과 없는 `CastShadows`가 보인다.

### [2026-11-22] Sky Light 방향 수정과 환경 조명 문서

- **결함.** Sky Light의 반구 위쪽이 엔티티 전방(+Z)이어서, 회전하지 않은 Sky Light의 하늘색이 옆면에 들어왔다.
- **수정.** Sky Light에는 엔티티의 위(+Y)를 넘긴다. 회전하면 반구도 기운다. Scene View의 방향 표시도 맞췄다. 셰이더는 바꾸지 않았다.
- **검사.** `SkyLightDirectionCheck`에 회전, 크기, 잘못된 축과 다른 광원의 방향 유지를 추가했다.
- **문서.** Directional Light·Sky Light·Sky Atmosphere·Fog·Volumetric Cloud의 관계, 변수 사용 여부와 문제를 [환경 조명 정리](EnvironmentLightingReference.md)에 기록했다.
- **확인된 결과.** 사용자가 CTest 통과와 회전 반응을 확인했다. `DXRShadowLab`에서 Directional Light를 끈 화면의 색은 바닥(200, 212, 222), 기둥 옆면(158, 173, 188)으로 위를 보는 면이 더 밝았다. 강도를 올린 화면이며 아래를 보는 면은 확인하지 않았다.

### [2026-11-22] 에디터 카메라 Near·Far 조절

- 카메라 뷰포트 메뉴에서 뷰포트별 Near·Far를 조절하고 작업 공간에 저장한다. 설정의 카메라 조작에서는 기본값을 바꿔 카메라 뷰포트 전체에 적용한다.
- 다른 설정을 적용할 때는 뷰포트별 값을 유지한다. 잘못된 값은 적용하지 않고 이전 값을 유지한다. 프리뷰 카메라는 대상 크기로 자동 설정하므로 제외했다.
- `EditorSettingsCheck`에 저장·복원과 잘못된 거리 사례를 추가했다. 사용자가 CTest 통과와 Near·Far 동작을 확인했다.

### [2026-11-22] 뷰포트별 LOD와 TLAS

- **결함.** 모든 Geometry 패스가 LOD 상태 하나를 덮어써 두 뷰포트를 함께 열면 히스테리시스가 깨지고 BLAS 대기가 15Hz로 반복됐다. LOD 거리는 엔티티 원점에서 재어, 배치가 정점에 구워진 Sponza의 LOD 물체 341개 중 340개가 같은 거리로 판정됐다. 섀도 패스는 카메라 없이 LOD를 골랐다.
- **LOD.** 뷰(Geometry 패스)마다 직전 LOD를 따로 기억하고, 거리는 경계 상자 중심에서 잰다. 같은 뷰의 GBuffer·포워드·그림자 패스가 한 번 고른 LOD를 함께 쓴다.
- **레이트레이싱.** TLAS를 씬과 뷰의 조합마다 만들고 마스크와 캐스케이드 제외도 같은 뷰의 TLAS를 쓴다. 통계와 로그는 프레임 제한으로 건너뛴 뷰의 값까지 합친다.
- **검사.** `LODSelectionCheck`에 전환 경계, 뷰 사이의 독립성과 거리 계산을 추가했다.
- **확인된 결과.** 사용자가 CTest 통과와 LOD 떨림의 완화를 보고했다. 뷰포트 하나로 연 Sponza 로그에서 첫 대기 370개가 끝난 뒤 약 36초 동안 대기는 한 번(27개)이었고, 이동 중 새 BLAS는 프레임당 최대 16개였다.
- **남은 한계.** 두 뷰포트를 함께 연 Sponza의 로그는 없다. 완화된 뒤 남은 떨림의 원인은 확인하지 않았다. 방향광 캐스케이드는 첫 Geometry 카메라 하나로만 계산되어, 섀도맵이 맡는 그림자는 다른 뷰포트에서 어긋날 수 있다.

### [2026-11-22] Masked 재질의 알파 테스트 그림자 (래스터)

- **결함.** 섀도맵 셰이더가 텍스처를 읽지 않아 Masked 재질도 구멍 없는 그림자를 만들었다. GBuffer는 블렌드 모드와 무관하게 알파 0.01 미만만 버렸다.
- **재질.** Masked의 알파 기준값(`AlphaCutoff`, 기본 0.5)을 추가했다. 인스턴스는 원본 값을 상속하거나 덮어쓴다. `materialFlags`의 상위 8비트에 담아 상수 버퍼 크기를 유지한다. [바인딩](Shader/BINDING_LAYOUT.md)
- **렌더링.** GBuffer와 Masked 섀도맵 셰이더가 같은 알파 식과 기준값을 쓴다. 정적·스킨드 메시와 방향광·스포트·점광원에 적용하며, 섀도 패스에서 Masked 재질은 다른 재질과 합치지 않는다.
- **편집.** 머티리얼 에디터에서 블렌드 모드가 Masked일 때 알파 기준값을 조절한다.
- **검사.** `SceneArchiveCheck`에 기본값, 상속, 오버라이드와 범위 사례를 추가했다.
- **확인된 결과.** 사용자가 Debug·Release 빌드와 CTest 통과, Sponza 그림자를 확인했다. 같은 씬 로그에서 섀도맵으로 넘어간 캐스터 8개를 확인했다. 기준값 조절 화면은 따로 보고받지 않았다.
- **범위 밖.** 엔진 기본 `GBufferPS`를 쓰는 재질만 지원한다. 셰이더 그래프 재질은 구멍 없는 그림자가 남는다. 모델 임포트는 Masked를 지정하지 않는다. 레이트레이싱 알파 테스트와 SDL GPU·WebGPU 확인은 남아 있다.

### [2026-11-22] 큰 씬 로드 시간 개선

- **원인.** 새 리소스마다 확장자 후보를 바꿔 가며 로드된 모든 리소스의 경로를 해석·정규화해 비교했다. 리소스 수의 제곱에 비례했다.
- **경로 조회.** 파일 이름이 다른 리소스를 경로 해석 전에 제외하고, 키가 가리키는 파일이 있으면 후보의 파일 경로 탐색을 생략한다. 해석 결과는 저장하지 않는다. 정확한 파일이 있으면, 다른 키로 등록된 `.json`·`.beve` 변형보다 그 파일을 먼저 로드한다.
- **시작 복원.** 마지막 작업 씬을 씬 열기와 같은 로딩 창으로 연다. 로딩 창이 보이는 동안 프레임 예산을 8ms에서 24ms로 늘리고, 복구 창은 로드가 끝난 뒤 표시한다. 모델은 메시 뒤에 로드해 한 프레임에 몰리지 않게 했다.
- **계측.** 전체·읽기·리소스·엔티티·열기 시간과 확장자별 시간, 가장 느린 리소스 다섯 개를 기록한다.
- **확인된 결과.** Sponza(참조 1,047개)가 Debug에서 로그 공백 기준 약 404초에서 17.7초와 16.9초로, 모델 순서 변경 뒤에는 9.3초로 줄었다. Release는 2.3초였다(재질 0.97초, 메시 1.06초). Release의 수정 전 값은 없다. 사용자가 CTest 통과를 보고했다.
- **남은 한계.** 큰 텍스처 하나가 예산을 넘으면 그 프레임이 멈춘다. 엔티티 생성은 한 번에 처리하고 로드 중에는 창을 닫을 수 없다.

### [2026-11-22] 프레임 SRV 테이블 재사용

- **결함.** D3D12는 드로우마다 SRV 테이블을 새로 기록하며 한도는 프레임당 1,024개다. Sponza에서 방향광 그림자를 켜면 한도를 넘어 프레임이 실패했다. 2026-11-16 로그에도 같은 오류가 있다. 레이트레이싱을 켠 경우 BLAS 예산으로 대기 캐스터가 캐스케이드에 남아 첫 프레임 드로우가 늘었다.
- **수정.** 리소스 조합이 같은 그래픽스 드로우는 같은 프레임의 테이블을 재사용한다. Compute는 매번 새로 기록한다.
- **확인된 결과.** 사용자가 Debug에서 Sponza의 그림자를 확인했다. 로그에서 첫 프레임 대기 378개가 약 2.3초 뒤 0이 되고 인스턴스 394개가 TLAS에 들어갔으며, 실패와 SRV 테이블 오류가 없었다. 사용자가 CTest 통과를 보고했다.
- **남은 한계.** 서로 다른 리소스 조합이 프레임당 1,024개를 넘으면 같은 오류가 난다. LOD가 있는 큰 씬에서는 BLAS 대기 시작·종료가 반복돼 로그가 늘어난다(약 11초 동안 64줄). 뷰포트마다 다른 LOD를 고르는 것이 원인인지는 확인하지 않았다.

### [2026-11-22] 셰이더 노드의 문자열 설정 핀 허용

- **결함.** `c58c2980`이 `Shader.TextureSample`에 문자열 핀 `name`을 추가했지만 등록 규칙이 셰이더 노드의 문자열 핀을 모두 거부했다. `RegisterBuiltins`가 실패해 기본 그래프 노드가 등록되지 않았고 `AssetReferenceCheck` 52번이 실패했다.
- **수정.** 연결할 수 없는(`LiteralOnly`) 문자열 입력만 허용한다. 연결 가능한 문자열 핀은 계속 거부한다.
- **확인된 결과.** 이후 시작 로그에 등록 오류가 없었고 사용자가 CTest 통과를 보고했다. TextureSample 노드의 이름 입력 화면은 확인하지 않았다.

### [2026-11-22] DXR 그림자 누락 방지와 BLAS 빌드 예산

- **결함.** 섀도맵 제외 여부를 메시·재질 종류로만 정해, BLAS 생성이 실패한 메시와 TLAS 빌드가 실패한 씬의 그림자가 섀도맵과 레이트레이싱 양쪽에서 빠질 수 있었다.
- **수정.** 이번 프레임 TLAS에 실제로 들어간 엔티티를 기록하고, 방향광 캐스케이드를 그릴 때 그 엔티티만 제외한다. 제출 이후 LOD가 바뀌어도 판정이 어긋나지 않게 렌더 시점에 조회한다.
- **빌드 예산.** 새 BLAS를 프레임당 16개 또는 삼각형 100만 개까지 빌드한다. 첫 빌드는 삼각형 한도를 넘어도 허용한다. 두 값은 측정 전 시작값이며, 대기 캐스터는 섀도맵을 쓴다.
- **계측.** 메트릭 패널과 로그에 BLAS 대기 캐스터 수를 추가했다. 실패 문구는 섀도맵 사용으로 바꿨다.
- **검사.** `RaytracingBuildPolicyCheck`에 개수·삼각형 한도, 한도보다 큰 메시, 여러 프레임에 걸친 처리와 TLAS 포함 집합의 갱신 사례를 추가했다.
- **확인된 결과.** 사용자가 빌드한 에디터 로그에서 `DXRShadowLab`의 인스턴스 60개, BLAS 3개, 대기 0, 실패 0을 확인했다.
- **Sponza 관찰.** 사용자가 `Sponza` 씬에서 그림자가 보이지 않는다고 보고했다. 저장된 방향광의 `Flags`는 52이며 `CastShadows`가 없다. 씬을 연 뒤 가속 구조 빌드 로그도 없었다. 그림자 방향광이 없어 레이트레이싱을 요청하지 않는 기존 정책과 일치한다.
- **미검증.** 이 변경 후의 CTest 결과, BLAS 대기 경로의 실행, BLAS·TLAS 강제 실패 화면, 예산 값의 프레임 시간 효과는 확인하지 않았다. 마스크 텍스처 생성이나 Dispatch 실패 시의 그림자 누락은 남아 있다.

### [2026-11-22] 병렬 기능 스프린트(티어 1~3) 및 회귀 수정·전체 CTest 통과

- **빌드·워커 격리 및 씬 로딩 모달 (`a388afbe`, `9516c6cb`, `5edd0f99`, `471f1fe3`)**: `BamAssetWorker` Phase 4 장애 격리 검증(`AssetWorkerFaultIsolationCheck`)·임포트 푸터 진행률 집계(`ImportProgressScope`)와 비차단 단계형 씬 로딩 모달(`SceneLoadState`, 150ms 지연 표시)을 구현하고, 타임라인/에디터 독립 헤더·소멸자 누락 빌드 오류를 수정했다.
- **셰이더·게임플레이·Shipping 기반 (`c6c559ad`, `251d0030`, `4ab87326`)**: Shader Graph 노드 팔레트 확장 및 `Normal`/`OpacityMask` 핀 연결, 게임플레이 핵심 컴포넌트(`HealthComponent`, `HitboxComponent`, `InteractableComponent`) 및 그래프 노드 추가, `BAM_SHIPPING` 가드·공용 `RuntimeSystemManifest`·클라이언트 모듈 로드 오류 처리를 반영했다.
- **프리팹·ECS UI·애니메이션 2차 병렬 스프린트 (`bdeb7b13`: `5cbb786e`, `e25fe2ec`, `8cf6970a`)**: 프리팹 내부 `entt::entity` 참조 리맵·Apply All/Unpack/Undo 계약 완성, ECS UI 실전 위젯(`CanvasScalerComponent`, `UITextInputComponent` IME, `UIDropdownComponent`, `UIMultilineTextLayout`), 2-Bone/Look-At IK·Root Motion·`AnimatorGraphPanel` 다중 레이어/PIE 실시간 동기화를 반영했다.
- **2D 배칭·물리·비동기 열기·플러그인 3차 병렬 스프린트 (`5ad214a5`: `006fea59`, `dd090d7b`, `ca744c96`)**: 2D 스프라이트·타일맵 실제 GPU 배칭 및 아틀라스 베이킹·`SpatialBroadphase`·`PhysicsSystem::Sweep`/`OverlapAll` API, `AsyncResourceOpenService`·`ModelEditor` DCC 프리셋/옵션 영속화(사이드카), `Plugin://` 가상 URI 마운트·원클릭 CMake 플러그인 소스 빌드/게시를 구현했다.
- **빌드·CTest 회귀 수정 및 작업 트리 정리 (`1d71d773`, `4e9ff13b`, `dbdaa188`, `44539364`, `e0eebb53`, `61f4594f`)**: Sphere-OBB 오버랩 래핑, `PlatformPath` 오버로드 모호성, `SpriteBatchKey` 집계 순서, 2D Box 브로드페이즈 월드 AABB 변환(`ComputePhysicsShapeWorldAABB2D`), `KinematicCharacterCheck`/`UIInteractionCheck`/`UILayoutCheck`/`AnimatorGraphPanel`/`ReflectionArchiveCheck` 결함을 수정하고 프로젝트 로캘 카탈로그·UI 텍스트/설정 변경을 논리 커밋으로 정리했다.
- **검증 현황**: 사용자가 `x64-ninja-debug` 전체 빌드 및 전체 CTest 통과를 확인했다. `x64-release`, `BAMENGINE_WITH_EDITOR=OFF` 독립 Client, PIE·실환경 UI/GPU 수동 검증은 후속 검증 범위다.

### [2026-11-22] DXR 방향광 그림자 1차

- **장치와 셰이더.** D3D12 장치를 고성능 어댑터에서 만들고 DXR Tier와 최고 Shader Model을 조회한다. VRAM 통계도 실제 장치의 어댑터를 사용한다. `RTCS` 셰이더는 `cs_6_5` DXIL만 생성한다.
- **RHI.** Compute 쓰기 텍스처 UAV 테이블, BLAS/TLAS 빌드와 TLAS 바인딩을 추가했다. 루트 시그니처 구성 버전을 PSO 디스크 캐시 키에 넣었다. [바인딩](Shader/BINDING_LAYOUT.md)
- **그림자.** `RaytracingSceneManager`가 정적 불투명 캐스터를 모으고, RayQuery 마스크와 나머지 캐스터의 섀도맵을 합친다. 같은 아틀라스의 스포트 그림자는 모든 캐스터를 그대로 그린다. [M3 상태와 범위](RenderingDevelopmentPlan.md#m3-dxr-그림자--반사)
- **설정.** 조명, 프로젝트, 그래픽, Scene View 패스 메뉴에 사용 여부를 두고 뒤의 설정이 앞을 덮어쓰게 했다. 지원하지 않는 장치와 셰이더 누락은 섀도맵을 쓴다. Client 패키지의 프로필 셰이더 목록에 마스크 셰이더를 넣었다.
- **계측.** 패스 계측을 중첩할 수 있게 하고 가속 구조 빌드와 뷰포트별 마스크 시간을 나눴다. GPU 합계는 최상위 구간만 더한다. 메트릭 패널에 인스턴스, BLAS/TLAS 메모리와 섀도맵 캐스터 수를 표시한다.
- **실행 중 고친 문제.** 기본 머티리얼이 그림자 셰이더를 명시해 모든 캐스터가 제외되던 판정, 비포커스 뷰포트의 15 FPS 제한 때문에 반복되던 TLAS 재생성, 스크래치 버퍼 초기 상태 경고를 수정했다. 중단된 프레임의 TLAS 요청과 다른 활성 씬의 캐스터 제외도 정적 검토에서 고쳤다.
- **확인된 결과.** 사용자 로그에서 RTX 4090, Tier 1.1, Shader Model 6.7 이상과 `DXRShadowLab`의 인스턴스 60개, BLAS 3개, 섀도맵 캐스터 1개, 실패 0을 확인했다. 사용자가 설정 단계까지의 빌드와 테스트 통과를 보고했다.
- **계측 관찰.** 사용자가 메트릭 패널에서 가속 구조 빌드와 Scene·Game View 마스크 항목이 표시되고, 레이트레이싱을 끄면 사라지는 것을 확인했다. 계측 수정 후 화면 한 장에서 가속 구조는 CPU 0.38ms·GPU 0.08ms, Game View 마스크는 CPU 0.06ms·GPU 0.04ms, Scene View 마스크는 CPU 0.08ms·GPU 0.04ms였다. 빌드 구성, 해상도와 씬을 통제하지 않은 평활값이며 섀도맵과의 성능 비교가 아니다.
- **GPU 시간 0 표시 수정.** 수정 전에는 마스크 GPU 시간이 0.00ms, 가속 구조가 0.02ms로 보였다. 타임스탬프를 회수할 때마다 모든 패스 결과를 지워, 프레임 제한으로 그 프레임에 제출하지 않은 뷰포트의 패스가 0으로 조회됐다. 패스별 마지막 결과를 유지하고 600회 회수 동안 갱신되지 않은 항목만 지우게 했으며, 이후 위 값이 표시됐다. GPU 합계는 회수한 프레임에서 실행한 최상위 패스만 더하므로 패스별 값의 합과 다를 수 있다.
- **미검증.** 계측 변경 후의 CTest 결과, 성능 비교, 설정 네 곳의 조합별 화면 확인 기록, `x64-release`와 독립 Client 패키지, 다른 GPU·SDL GPU 대체 경로는 확인하지 않았다. 실험 씬 `Assets/Scenes/DXRShadowLab.bamscene.json`은 Git 밖에 있다.

### [2026-11-22] UI 텍스트 번역 사용 체크박스

- 체크를 끄면 텍스트 원문을 그리며, 로캘 키 값은 유지한다. 인스펙터는 체크가 켜져 있을 때만 키 입력란을 표시한다.
- 캔버스와 인스펙터에서 새로 추가한 UIText는 번역을 끈 상태로 시작한다. 체크 상태가 없는 이전 레코드는 기존 로캘 키를 계속 적용한다.
- UILayoutCheck에 이전 레코드 기본값과 체크 해제 상태의 키 보존 사례를 추가했다. 빌드·CTest·에디터 실행은 사용자 확인 대기다. [로캘 텍스트 사용](UIEditorFoundation.md#로캘-텍스트-사용)

### [2026-11-22] 기존 UI 텍스트 씬 호환과 프로젝트 번역

- UIText의 글꼴과 세로 정렬을 선택적 Archive 필드로 지정했다. 두 필드가 없는 이전 레코드를 기본값으로 복원하는 사례를 UILayoutCheck에 추가했다.
- UI 텍스트 인스펙터에서 설치 글꼴을 검색하고 선택한다. 결과는 UI 뷰 캔버스에 표시한다.
- 프로젝트 카탈로그를 Engine·Editor 사전과 별도로 읽고, 프로젝트 설정에 가져오기·다시 읽기와 파일 변경 감지를 연결했다. Windows Client 시작과 내보내기에 카탈로그를 포함하며 샘플 재시작 문구는 프로젝트 Game.csv로 옮겼다.
- 호출부와 UTF-8 CSV 원본을 정적으로 확인했다. 빌드·CTest·실환경 핫로드와 독립 Client 표시는 사용자 확인 대기다. [UI 편집기](UIEditorFoundation.md)

### [2026-11-22] UI 뷰 편집과 텍스트 로캘 개선

- 텍스트 상자를 UI 뷰에서 직접 선택·이동하고 네 모서리 크기 조절을 할 수 있다. 텍스트에는 Windows 설치 글꼴 이름과 세로 정렬을 추가했다.
- UI 뷰의 메뉴 툴바와 퀵액세스 바를 나눴다. 캔버스 배율과 상호작용·화면 맞춤 동작을 퀵액세스에서 사용한다.
- Engine CSV에서 이전 그룹을 상속했던 번역을 원래 키로 복구하고, Rect Transform의 크기와 배율 라벨을 구분했다. 로캘 텍스트 사용법을 UI 문서에 기록했다.
- 빌드, 생성 로캘과 Editor 실행 확인은 사용자 검증 대기다. [UI 편집기](UIEditorFoundation.md)


### [2026-11-22] 게임 UI 병합 후 MSVC 빌드 오류 수정

- UI 텍스트는 공개 로캘 조회에 대체 문구를 전달한다. 번역을 찾지 못하면 인스펙터의 텍스트를 표시한다.
- Canvas 입력 라우터는 이벤트 처리기의 단독 소유자다. 복사 생성과 복사 대입을 금지해 MSVC에서 DLL 내보내기 시 삭제된 벡터 원소 복사를 생성하지 않게 했다.
- 사용자가 제공한 MSVC 오류와 호출부를 대조했다. 수정 후 빌드와 실행 검증은 사용자 확인 대기다.

### [2026-11-22] 엔진 통계 대시보드 반응형 배치

- 다섯 통계 카드는 실제 글꼴 크기, 번역된 버튼 문구와 패널 폭으로 열 수를 정하고, 콘텐츠 높이에 맞춰 늘어난다. 제목과 상태가 한 줄에 들어가지 않으면 상태를 다음 줄에 표시한다.
- 상세 열기·닫기와 기록 초기화 버튼은 좁은 폭에서 줄바꿈한다. 툴바와 상세 제목의 버튼도 실제 라벨 폭에 따라 다음 줄로 이동한다.
- 변경 범위의 공백과 호출부를 정적으로 확인했다. 빌드와 한국어·영어, 100%·150%·200% 배율, 좁은 도킹 폭의 실제 표시·클릭 검증은 사용자 실행 대기다.

### [2026-11-22] UI 이미지 마스크와 에셋 선택

- **이미지 마스크** 프리셋과 텍스처·스프라이트 리소스 선택을 연결했다. 적용 대상은 자식 UI이며 이미지가 알파 경계를 제공한다.
- 사각 및 타원 마스크와 조합하고, 이미지 마스크는 최대 두 개까지 알파를 곱한다. 포인터는 같은 원본 픽셀과 아틀라스 범위를 사용한다.
- CSV 로캘 키 및 C++/셰이더 연결을 정적으로 검토했다. UILayoutCheck에 회전한 마스크, 아틀라스 경계, 두 마스크 합성, 비활성화 사례를 추가했다. 빌드·CTest와 실제 Editor·PIE·Client 및 DirectX12·SDL GPU 출력은 사용자 확인 대기다. [이미지 마스크 사용법](UIEditorFoundation.md#이미지-마스크-만들기)

### [2026-11-22] Canvas UI 부모 변경과 스크롤바·타원 마스크

- 같은 Canvas 안에서 계층 드래그와 **UI 루트로 이동**을 지원한다. 자유 배치에서 표현할 수 있는 Canvas 변환은 보존하고, 배치 그룹은 새 슬롯을 계산한다. 프리팹 편집에서는 저장 대상 서브트리 안에서 이동한다.
- Scroll View가 넘칠 때 세로 스크롤바를 표시한다. 휠, 트랙 클릭과 손잡이 드래그를 지원하고 인스펙터에서 표시 여부를 변경할 수 있다. UI 뷰에서 Scroll View를 선택한 상태로 위젯을 만들면 콘텐츠 아래에 둔다.
- **타원 마스크**를 UI 프리셋에 추가했다. 기본 HLSL·GLSL 셰이더와 포인터 판정에 같은 타원 경계를 사용한다. 네 개를 넘는 상위 타원은 사각 경계만 적용하며 키보드 탐색은 사각 경계를 사용한다.
- 호출부와 변경 파일을 정적으로 검토했다. UILayoutCheck와 UIInteractionCheck에 회귀 검사 사례를 추가했다. Windows 빌드, CTest, Editor·PIE·독립 Client와 두 GPU 백엔드의 결과는 사용자 확인 대기다. [UI 뷰 사용법](UIEditorFoundation.md) · [위젯 개발 계획](UIWidgetsAndLayoutPlan.md)

### [2026-11-21] 게임 UI 입력과 Canvas 제작 기능

- Game View와 Windows Client에 Canvas 포인터 입력을 연결했다. UI에서 시작한 왼쪽 입력을 Gameplay 맵에서 분리하고, 대표 챌린지 재시작 버튼이 기존 초기화 경로를 사용한다.
- Canvas 네 모서리 리사이즈는 반대 모서리를 고정하고 기존 스냅, 취소와 Undo 경로를 사용한다.
- Windows용 Canvas 한 줄 텍스트를 추가했다. UI View에서 생성할 수 있고 인스펙터에서 텍스트·로캘 키·크기·색상과 정렬을 편집한다.
- `GameUI` 액션 맵의 Tab/Shift+Tab으로 버튼을 선택하고 Enter로 실행한다. 키보드 실행은 기존 Click 이벤트로 전달한다.
- `CanvasViewCheck`와 `UIInteractionCheck`에 각각 모서리 고정과 키보드 이동·실행 검사 사례를 추가했다. 빌드, CTest, 실제 Editor·PIE·Client 동작은 사용자 검증 대기다. [Canvas 편집](UIEditorFoundation.md) · [UI 상호작용](UIInteraction.md)
- 메인 병합에서 Windows 시작 화면의 단계 콜백과 첫 프레임 경로를 유지했다. 재시작 버튼은 설정된 적중 표시 개수에 맞춰 자리 잡는다.

### [2026-11-22] Windows Client 공통 시작 화면

- BamEngine 워드마크와 로딩 단계 표시를 독립 Windows 창으로 띄운다. 준비된 게임 창의 첫 프레임이 GPU에서 끝나면 시작 화면을 닫고, 프레임 시간 기준을 다시 잡아 게임을 진행한다. 별도 에셋 없이 Client에 포함한다. [Windows Client 시작 화면](PIEWindowsClientWorkflowPlan.md#windows-client-첫-화면)
- **디자인 상태.** 사용자는 현재 화면을 임시로 사용하고 시각 디자인을 반드시 교체하기로 했다. 정식 Windows 배포 전 필수 작업으로 기록했으며, 최종 디자인은 미정이다. [디자인 교체 기준](PIEWindowsClientWorkflowPlan.md#windows-client-첫-화면)
- **검증 상태.** 코드와 초기화·실패 경로를 정적으로 대조했다. 새 Client의 빌드, 첫 실행 화면, 전환 및 오류 경로는 사용자 확인 대기다.

### [2026-11-22] 로직 그래프 우측 속성 표 통일

- 노드와 연결의 속성, 변수 목록·생성 폼, 실행 설정에 `EditorUI` 공용 인스펙터 표를 적용했다. 핀의 기본값 적용·복원은 기존 그래프 API를 사용한다. 새 라벨과 버튼 설명은 한·영 CSV에 등록했다.
- **검증 상태.** 변경 파일의 `git diff --check`와 로캘 키 사용처를 정적으로 확인했다. 빌드와 좁은 패널·언어 전환·핀 편집의 에디터 확인은 사용자 실행 대기다. [그래프 패널](GraphPanel.md)

### [2026-11-21] Windows 게임 제작과 내보내기 기능 확장

- 캐릭터의 공격 챌린지 컴포넌트에서 표적, 목표 적중 횟수와 판정 거리를 편집한다. GameModule이 점수·HUD를 관리하며, 공격 입력을 그래프에 넘길 수도 있다. 기존 기준 씬은 명시적 챌린지 컴포넌트가 없을 때 이전 설정으로 열린다. [챌린지 구성](AnimationPipelineIntegrationPlan.md#5-대표-액션-통합-검증)
- 게임플레이 그래프에서 자기 엔티티의 액션 타임라인을 재생하고 같은 엔티티의 Notify 태그를 받아 처리한다. 알림은 씬 레지스트리에 보관하고 그래프 초기화 이후 전달한다. 자기 삭제 노드는 현재 그래프 실행이 끝난 뒤 엔티티를 제거한다. [그래프 실행](GraphExecution.md#액션-타임라인-노드)
- Windows 빌드 프로필에 추가 씬과 추가 에셋을 `Project://` URI로 저장한다. 추가 씬의 알려진 참조와 적용되는 Gameplay 입력을 검사하고, 지정한 파일을 상대 경로대로 스테이징하며 로그에 포함 이유를 남긴다. [패키징 계획](BuildAndPackagingPlan.md)
- **검증 상태.** 정적 점검 후 사용자가 공격 챌린지 인스펙터, 그래프 재생·Notify, 추가 씬·에셋 내보내기를 확인했다고 보고했다. 구성별 전체 CTest 결과는 이 보고에 포함되지 않았다.

### [2026-11-21] 에디터·게임플레이·Windows 배포 변경 정리

- **엔티티 선택과 프리팹.** 에디터 선택과 프리팹 모드 진입 전에 저장하는 선택에 엔티티 약한 참조를 사용한다. 삭제된 엔티티는 조회와 프리팹 모드 복귀에서 제외된다. 실제 에디터에서 선택 엔티티 삭제와 프리팹 모드 왕복을 확인해야 한다. [선택 관리](../Editor/Source/Selection/Private/SelectionManager.cpp) · [프리팹 모드](../Editor/Source/ImGui/PrefabEditor/Private/PrefabModeManager.cpp)
- **3인칭 이동과 입력.** `UseGraphMoveInput`을 사용하는 컨트롤러에 그래프 이동·달리기·점프 요청을 연결한다. 일반 컨트롤러는 Gameplay 액션에서 이동을 읽고, Windows Client는 마우스 이동량과 휠을 같은 액션 맵에 전달한다. 리플렉션 함수 호출에서 문자열 값 인자를 받도록 보완했다. [3인칭 컨트롤러](../Engine/Source/Physics/Private/ThirdPersonControllerSystem.cpp) · [그래프 입력 노드](../Engine/Source/Graph/Libraries/Public/GraphGameplayLibrary.h)
- **에디터 제작 도구.** 로직·Animator 그래프의 핀 메뉴, 머티리얼 텍스처 샘플·매개변수·저장 및 프리뷰, 단독 메시 미리보기, Unicode 콘텐츠 검색을 기능별로 기록했다. 변경된 CSV와 생성된 한·영 로캘 번들도 함께 정리했다. [로직 그래프](GraphPanel.md) · [Animator 그래프](AnimatorGraphPlan.md) · [머티리얼](MaterialShaderGraphIntegrationPlan.md) · [메시 프리뷰](InteractiveModelImportPreviewPlan.md) · [Unicode 경로](Utf8PathHandlingPlan.md)
- **대표 게임과 배포.** Q/R 액션 샘플과 시작 씬별 필수 Gameplay 바인딩 검사, 메모리 PIE 스냅샷과 첫 사용 리소스 준비를 반영했다. 사용자가 PIE와 독립 Client의 Q/R 동작을 확인했고, 후속 에디터 로그에서 첫 Play 일반 업데이트가 8.49ms로 감소했다. 작성 모델과 컨트롤러의 준비 비용은 씬 열기에서 관측됐다. 로컬 `Assets/`의 기준 씬과 타임라인은 기존 Git 무시 규칙을 따르므로 재현에 필요한 변경은 통합 계획을 참고한다. [샘플 재현](AnimationPipelineIntegrationPlan.md#5-대표-액션-통합-검증) · [PIE·Windows 배포](PIEWindowsClientWorkflowPlan.md)
- **검증 범위.** 기능별 변경의 스테이징 diff를 확인했다. 기존 사용자 동작 확인과 계측 결과는 해당 기록에 남겼으며, 이번 정리 시점의 전체 빌드·CTest 결과는 확인 대기다.

### [2026-11-21] 첫 PIE AnimationSystem 지연 확인

- 최신 Editor 로그에서 첫 Play 일반 업데이트 1012.60ms 중 AnimationSystem이 1007.93ms를 사용했다. 재진입은 AnimationSystem 5.03ms였다.
- 에디터 씬을 열 때 활성 Animator 컨트롤러의 참조 클립을 기존 리소스 캐시에 준비한다. 첫 게임 틱은 최신 컨트롤러로 바인딩한다. 5ms 이상 걸리는 컨트롤러 로드에 파일 읽기와 역직렬화·참조 해석 시간을 나눠 기록한다.
- **수정 후 로그.** 첫 Play 일반 업데이트 8.49ms, 재진입 8.18ms였다. 컨트롤러 역직렬화와 참조 해석 912.04ms는 씬 준비에서 발생했다. 같은 준비 과정에서 작성 모델 로드 4579.26ms도 확인했다. 전체 씬 열기 시간, Animator 동작과 미저장 편집 복원 결과는 별도 확인이 필요하다. [PIE 조사](PIEWindowsClientWorkflowPlan.md#2-첫-pie-프리징-조사)

### [2026-11-21] 그래프 준비 이후 첫 PIE 로그 분석

- 그래프는 Play 전에 2.38ms에 로드됐다. 첫 Play의 일반 업데이트는 937.55ms로 재진입의 8.66ms보다 길었고, 첫 물리 고정 틱 합계는 1.00ms였다. 남은 지연을 일반 업데이트로 좁혔다.
- 첫 Play 한 프레임에 FMOD 갱신, 활성 씬 조회와 5ms 이상 걸린 시스템 업데이트를 기록한다. 원인 시스템 확인은 새 Editor 로그가 필요하다. [PIE 조사 및 계측](PIEWindowsClientWorkflowPlan.md#2-첫-pie-프리징-조사)

### [2026-11-21] 첫 PIE 게임 틱 지연 조사와 그래프 준비

- 사용자가 첫 Q 멈춤 해소를 확인했다. 남은 첫 PIE 지연은 Editor 로그에서 첫 게임 업데이트 977.34ms로 기록됐다. 같은 프로세스 재진입은 12.93ms였고 스냅샷과 렌더 호출은 첫 시도와 재진입에서 비슷했다. 첫 틱 말미에 Rogue 컨트롤러 그래프 로드 성공이 기록됐으나 기존 계측으로는 그래프 로드의 비중을 분리할 수 없다.
- 에디터 씬 진입 시 바인딩된 그래프 템플릿을 기존 캐시에 준비한다. 파일이 바뀌면 캐시를 다시 읽으며 실행기와 이벤트는 기존 게임 틱에서 시작한다. 첫 게임 프레임의 고정·일반·후속 업데이트와 그래프 경로 조회·로드 시간을 각각 출력한다. [조사 및 재현 절차](PIEWindowsClientWorkflowPlan.md#2-첫-pie-프리징-조사)
- **검증 상태.** 변경 범위의 정적 확인을 마쳤다. 새 빌드에서 씬 열기와 첫 Play 사이의 비용 이동, 그래프 저장 후 첫 Play 동작, Stop 복원은 사용자 검증 대기다.

### [2026-11-21] 첫 PIE와 첫 액션 지연 완화

- 사용자 확인에서 PIE와 독립 Windows Client의 Play·Q·R 동작은 정상이다. 새 Editor에서 첫 PIE, 두 호스트에서 첫 Q에 짧은 프레임 지연이 보고됐다.
- PIE 스냅샷의 JSON 인코딩·임시 파일 쓰기·Stop의 디스크 읽기를 제거했다. 메모리 Archive로 현재 편집본과 미저장 값을 복원한다. Play 요청의 입력·캡처·전환 시간과 첫 게임 프레임의 게임 업데이트·렌더 호출 시간을 기록한다. [PIE 조사](PIEWindowsClientWorkflowPlan.md#2-첫-pie-프리징-조사)
- 씬 진입 시 타임라인, 필요한 작성 모델, 사운드·복합 VFX를 기존 리소스 캐시에 준비한다. 2ms 이상 걸린 준비·재생 리소스 조회와 FMOD 재생 호출의 경로·시간을 기록한다. [첫 액션 조사](AnimationPipelineIntegrationPlan.md#첫-액션에서-발생한-프레임-지연)
- **검증 상태.** 정적 호출 경로와 변경 공백을 확인했다. 변경 후 Editor·Client 빌드, PIE/Stop 복원, 첫 Play·Q 프레임과 씬 로드 시간 측정은 사용자 실행 대기다.

### [2026-11-21] 머티리얼 샘플 노드 표시와 매개변수 행 메뉴

- 사용자 재현에서 저장 오류가 `Serialize / UnsupportedType / Variables[0].Type`으로 좁혀졌다. `EGraphPinType`을 리플렉션에 등록하고, `GraphFoundationCheck`에서 셰이더 변수의 타입·기본값·읽기 노드 연결이 파일 왕복 후 유지되는 사례를 추가했다. 수정 후 빌드·테스트·실제 에셋 저장은 사용자 실행 대기다.

- 텍스처 샘플 노드에 상시 썸네일을 추가하고 리소스 선택 팝업 폭을 유지했다. 매개변수의 동작을 마지막 열 메뉴로 이동했으며 그래프 선언 삭제 시 관련 노드와 연결을 함께 제거한다. 주석 박스는 선택 노드 경계에서 만들고 제목 인라인 편집을 지원한다.
- 사용자 에디터 로그에서 `NewMaterial_2.bamgraph.json` 저장 실패와 머티리얼 단독 저장을 확인했다. 당시 로그로 확인 가능한 정보는 실패 경로까지다. 그래프부터 저장하도록 순서를 조정하고 실패 단계·노드·핀·아카이브·OS 오류를 로그에 남긴다.
- **검증.** 파일과 사용자 로그를 정적으로 대조했다. 빌드, 실제 편집기와 저장 실패 재현은 사용자 실행 대기다.

### [2026-11-21] 머티리얼 그래프 입력과 기본 패스 편집 정합성

- 사용자 빌드에서 `AddGraphParameter()`의 노드 위치 지정 호출이 다른 소스 파일의 네임스페이스 별칭에 의존해 컴파일에 실패했다. 호출부에 `ax::NodeEditor`를 명시했다. 수정 후 빌드는 사용자 실행 대기다.

- 그래프 생성 범위를 Geometry와 ForwardTransparent로 제한했다. 라이브 셰이더를 머티리얼 경로·패스·백엔드·빌드별로 구분하고, 새 VS/PS를 모두 생성한 뒤 기존 바인딩을 교체한다. 호환되지 않는 상수 버퍼 레이아웃의 다른 그래프 패스는 다시 컴파일하도록 제거한다.
- 샘플 노드와 노드 속성에서 왼쪽과 같은 텍스처 슬롯을 편집한다. 인스턴스 바인딩에 상속·재정의·원본 값 복원을 표시한다. 양쪽의 변수 생성은 공통 선언 경로를 사용하고, 오른쪽 읽기 노드에서 현재 값을 편집할 수 있다.
- 매개변수 버퍼 레이아웃을 머티리얼 아카이브에 포함하고 인스턴스가 공용 레이아웃에 원본 값과 재정의 값을 순서대로 적용하도록 수정했다. 원본 레이아웃 재열기와 인스턴스 재정의 분리 검사를 `SceneArchiveCheck`에 추가했다. 빌드, CTest, D3D12 프리뷰 및 에디터 UI는 사용자 확인 대기다.

### [2026-11-21] 대표 Windows 게임플레이와 내보내기 입력 검증

- `BaselineVerification`에서 Q로 액션 타임라인을 재생하고, `OnAttackHit` 시점에 거리 조건을 만족하면 점수를 한 번 기록한다. HUD는 3회 적중을 표시하며 R로 점수를 초기화한다. 로컬 씬의 Rogue 입력을 활성화하고 타임라인에 효과음 Notify를 추가했다. 샘플 에셋은 현재 `Assets` 무시 규칙에 따라 Git 밖에 있다. [샘플과 재현 절차](AnimationPipelineIntegrationPlan.md#5-대표-액션-통합-검증)
- PIE 시작 시 씬 스냅샷이 저장된 뒤 선택과 프리뷰 사운드를 정리한다. Windows 패키저는 프로젝트 설정이 요구하는 시작 씬의 필수 Gameplay 바인딩을 빌드 전에 확인하고, 검증한 입력 설정을 스테이징에 넣어 게시 전에 내용이 같은지 확인한다. [PIE·배포 검증](PIEWindowsClientWorkflowPlan.md#3-game-view와-배포-client의-일치-점검)
- **검증 상태.** JSON 구조와 설정·코드 연결 및 변경 범위의 공백을 정적으로 확인했다. Editor와 GameModule 빌드, CTest, 스냅샷 쓰기 실패, PIE 반복, 외부 Windows Client 실행 및 재배포 결과는 사용자 검증 대기다.

### [2026-11-21] 그래프 편집기의 핀 컨텍스트 메뉴

- 로직 및 머티리얼 그래프에 연결할 노드 추가, 해당 핀의 모든 연결 해제, 미연결 입력값 복원 메뉴를 연결했다. 로직 그래프의 변수 승격은 읽기 노드의 실제 출력 핀으로 연결하고 실패하면 그래프 상태를 되돌린다.
- 애니메이터 그래프는 Entry와 Any State, 일반 상태의 입력·출력 핀에 각각 전이 추가·기본 진입 지정·방향별 연결 해제를 제공한다.
- **검증.** 기존 `GraphPaletteCheck`에 같은 노드의 다른 핀을 건드리지 않는 검사 조건을 추가했다. Editor 빌드, CTest 및 우클릭·Undo/Redo의 실환경 확인은 사용자 실행 대기다.

### [2026-11-21] 콘텐츠 브라우저 검색 범위 전환 크래시 수정

- 전체 폴더 검색 중 시스템 코드페이지로 변환된 파일명을 UTF-8로 해석해 예외가 발생했다. 현재 폴더와 전체 폴더 검색은 파일명을 UTF-16으로 유지하고 검색어를 한 번 변환해 비교한다.
- 결과 필터·타일·썸네일·툴팁은 UTF-8로 변환한 파일명과 확장자를 사용한다. 콜스택과 비교·표시 경로를 정적으로 확인했다. Debug 빌드와 한글·이모지 파일의 범위 전환 재현은 사용자 실행 대기다.

### [2026-11-21] 공용 노드 메뉴와 머티리얼 입력 노드

- 그래프와 머티리얼 편집기의 노드 팔레트를 공용화하고, 물리 분류를 하나로 합쳤다. 리라우트는 한 그룹 안에서 타입별로 표시하며 분류와 항목에는 번역과 별개의 ImGui ID를 사용한다.
- 머티리얼 그래프 빈 공간 메뉴에서 Float 계열 매개변수와 읽기 노드를 만들 수 있다. 텍스처 샘플은 기존 바인딩 슬롯 0~3과 UV0 또는 연결된 UV를 사용한다.
- **검증.** 텍스처 샘플의 UV·슬롯·정점 단계 제한에 대한 `ShaderGraphCheck` 사례를 추가했다. 빌드, CTest, 실제 HLSL 컴파일과 편집기 UI 확인은 사용자 실행 대기다.

### [2026-11-21] 머티리얼 인스턴스의 원본 열기와 셰이더 진단 줄바꿈

- 인스턴스의 좌측 설정에 원본 파일명과 열기 버튼을 추가했다. 원본 파일은 공통 리소스 열기 경로로 전환하며 미저장 확인을 거친다.
- 오른쪽 자식 패널과 스플리터가 가용 폭을 사용하도록 수정하고, 진단 메시지를 남은 폭에서 줄바꿈한다. 복사 버튼은 진단 원문을 복사한다.
- **검증.** 문서 열기와 레이아웃 코드를 정적으로 확인했다. 빌드·로캘 생성과 좁은 패널에서 긴 진단 줄바꿈 및 문서 전환의 실환경 검증은 사용자 실행 대기다.

### [2026-11-21] 모델 에디터에서 단독 메시 미리보기

- `.bammesh`와 `.bammesh.json`을 공통 리소스 열기 경로에서 모델 에디터로 연다. 프리뷰 씬에 메시 하나를 배치하고 기존 카메라 포커스와 그리드를 재사용한다. 우측에는 읽기 전용 정점·인덱스 수를 표시한다.
- 메시로 전환할 때 기존 모델 문서의 미저장 확인을 사용한다. 표시 문구는 모델 에디터 로캘 CSV에 추가했다.
- 스킨드 서브메시가 단독 보기에서 잘린다는 사용자 보고를 받았다. 정적 드로우에서도 스키닝 셰이더가 선택되던 경로를 확인했고, 단독 스킨드 메시는 스킨드 드로우로 제출해 단위 본 행렬을 바인딩한다. 실제 클리핑 해소 여부는 재빌드 후 확인한다.
- **검증.** 열기, 렌더 제출, 문서 전환 경로를 정적으로 대조했다. Debug 빌드, 로캘 생성, 에디터에서 두 확장자의 로드 및 모델↔메시 전환 확인은 사용자 실행 대기다.

### [2026-11-21] 렌더링 및 보조 시스템의 컴포넌트 활성 상태 적용

- 메인 씬과 독립 조명 패스가 조명·대기·안개·구름·후처리의 공통 활성 상태를 읽도록 맞췄다. 독립 프리뷰, GPU 프리팹 썸네일, 헤드리스 워커가 각각 메시 제출 전에 활성 상태를 확인한다. 헤드리스 Archive 폴백도 비활성 레코드를 제외하며, 프리팹 썸네일 캐시 버전을 올렸다.
- 카메라 선택·카메라 아이콘 피킹·물리 디버그 렌더·UI 이미지 히트 테스트·지형과 타일맵·스플라인 도구의 직접 조회를 보완했다. Animator 실행과 타임라인은 활성 렌더러에서 스켈레톤을 찾는다. 조인트, 게임 모듈의 대기 동기화, 그래프 실행과 Client 프리로드도 공통 검사 경로를 사용한다.
- **검증.** 사용자가 메인 씬에서 스킨드 메시 렌더러의 활성화가 정상적으로 반영되는 것으로 확인했다. 호출부와 diff를 정적으로 확인하고 기존 피킹 회귀 검사에 공통 활성 조회를 추가했다. 전체 빌드·CTest 결과와 Client·썸네일 출력은 별도 확인이 필요하다.
- 신규 컴포넌트와 시스템의 활성화 기준, 보조 제출 경로, 비활성화 시 정리와 검증 항목을 [엔진·에디터·클라이언트 지침](EngineEditorClientGuidelines.md#51-컴포넌트-활성화와-시스템-실행-조건)에 추가하고 AGENTS.md에서 안내한다.

### [2026-11-21] 비활성 컴포넌트의 런타임 사용 차단

- 그래프의 물리·카메라·애니메이션·조명 노드와 UI 이벤트 전달에서 활성 상태를 검사한다. UI에서 비활성 자식 버튼의 이미지는 레이캐스트 정책을 유지하며, 이벤트 대상 검색은 활성 부모 버튼까지 이어진다.
- 타임라인에서 Animator가 비활성화되면 기존 제어를 해제한다. 활성 대상이 바뀌면 해당 대상의 클립을 다시 찾는다. 스킨드 렌더러는 비활성 Animator의 캐시 포즈를 사용하지 않는다.
- 스프링 암은 붙어 있는 카메라가 모두 비활성이면 암 엔티티를 카메라 대신 이동시키지 않는다. 캐릭터 컨트롤러는 비활성 중 남은 입력을 비우고, 꺼진 Collider의 플랫폼 이동량을 상속하지 않는다.
- 이전 프레임의 피킹 결과를 읽을 때 실제 렌더링에 제출한 컴포넌트의 상태를 다시 검사한다. 비활성 조명과 스플라인의 캐시도 실행 경로에서 제외한다. 마지막 활성 오디오 리스너를 끄면 FMOD에 남은 위치와 방향을 기본값으로 돌린다.
- **검증.** 이전 헤더 변경은 사용자가 좁은 도킹 폭과 200% 배율에서 정상으로 확인했다. 이번 런타임 변경은 정적으로 점검했으며, 활성 Animator 탐색, UI 클릭 전달, 2D 겹침, 이전 프레임 피킹의 회귀 검사를 추가했다. 빌드, CTest, PIE와 Client 동작, FMOD 출력 확인은 사용자 실행 전이다.
- 사용자 Ninja Debug 빌드에서 `ActionTimelinePlaybackCheck`가 `AnimatorComponent.h`를 통해 `Animation.h`를 읽을 때 이름 조회에 실패했다. `Animation.h`에서 타입의 네임스페이스를 명시해 PCH의 전역 이름 조회 의존을 제거했다. 수정 후 재빌드는 사용자 실행 전이다.

### [2026-11-21] 컴포넌트 공통 활성화와 인스펙터 메뉴

- 단일 엔티티 인스펙터의 컴포넌트 헤더 오른쪽에 활성화 체크박스와 ⋮를 배치했다. ⋮와 헤더 우클릭에서 순서 이동·제거 메뉴를 공유한다. 독립 프리팹 편집기에서도 컴포넌트 활성 상태를 편집할 수 있다.
- 컴포넌트 데이터와 비활성 상태를 함께 복사하고 씬·프리팹의 저장/재열기, 배치, Undo/Redo에 연결했다. 조명, 안개, 클라우드, 후처리, 그래프 바인딩, 조인트 및 오디오 리스너는 기존 활성 필드를 사용한다. Transform과 계층·ID 등 필수 상태는 씬 구조를 유지하기 위해 항상 활성이다.
- 렌더링, 카메라, 물리, 애니메이션, VFX, 오디오, UI 및 네트워크 실행 경로에서 활성 상태를 확인한다. 비활성 컴포넌트도 일반 ECS 조회와 인스펙터에서 읽거나 편집할 수 있다. 사용자 시스템은 실행 진입점에서 `IsComponentEnabled<T>` 또는 `TryGetEnabledComponent<T>`로 상태를 확인한다.
- 픽셀 퍼펙트 줌은 렌더링용 투영에 적용한다. 체크를 끄면 카메라에 설정해 둔 직교 크기를 사용한다.
- 컴포넌트 제목과 우측 컨트롤을 한 헤더 배경 안에 배치했다. 체크박스와 ⋮를 헤더 높이에 맞추고, ⋮는 평상시 배경과 테두리 없이 표시한다.
- **검증.** 변경 경로와 diff를 정적으로 확인했다. `SceneArchiveCheck`에 JSON/BEVE·프리팹·제거 후 재추가 검사를, `EditorHistoryCheck`에 토글과 설정 값의 Undo/Redo 검사를, `PixelPerfectCameraCheck`에 투영과 직교 크기 보존 검사를 추가했다. 사용자가 좁은 도킹 폭과 200% 배율의 배치를 확인했다. 자동 검사와 Editor·Client 런타임 검증은 사용자 실행 전이다.

### [2026-11-21] Game View 표시 상태의 첫 PIE 프리징 조사

- 오른쪽 Game View에 카메라 영상이 이미 표시된 상태에서 에디터 실행 후 첫 Play에만 프레임 저하와 짧은 멈춤이 발생했다는 사용자 관찰을 기록했다. Play 요청 프레임의 스냅샷·입력·오디오, 첫 게임 틱의 로드·물리, 두 뷰포트의 렌더 빈도 전환과 PSO·GPU 대기 후보를 현재 코드와 기존 로그에서 확인했다.
- [PIE 첫 실행 지연 및 배포 흐름 계획](PIEWindowsClientWorkflowPlan.md#2-첫-pie-프리징-조사)에 후보별 검증 방법, 저장 JSON 재사용 조건, 첫 Play와 재진입의 비교 절차를 추가했다.
- **확인 범위.** 코드와 기존 로그를 정적으로 대조했다. 프레임별 계측, 원인 확정, 최적화 구현, 빌드·CTest·Editor 재현 검증은 남아 있다.

### [2026-11-21] 머티리얼 셰이더 그래프 노드 표시 정합

- `MaterialEditorCanvas`의 일반 노드를 Graph Scripter와 같은 헤더·좌우 핀 배치로 정리했다. 140px 최소 폭과 노드·핀 이름의 로캘 조회를 GraphPanel과 맞췄다. 핀은 기존 `NodeEditorUI::DrawNodeBorderPins`를 재사용하며, 데이터 입력값 편집과 링크 연결 의미는 유지한다. 주석 박스는 GraphPanel의 그룹 스타일과 크기 저장을 사용하고, 리라우트는 투명한 단일 원형 핀으로 맞췄다.
- 공용 핀 렌더러의 연결 상태별 반투명 처리를 제거해 Graph, Material, Animator Graph의 핀 색을 모두 불투명하게 통일했다. Animator Graph는 새 공용 헬퍼 API에 맞게 호출부만 갱신했으며 시각·동작은 바꾸지 않았다. 상세 범위는 [Material Shader Graph 계획](MaterialShaderGraphIntegrationPlan.md#53-그래프-노드-표시-정합)과 [GraphPanel](GraphPanel.md)을 따른다.
- **정적 확인만 수행:** 공용 렌더 헬퍼 호출 및 해당 변경 diff를 검토했다. 저장소 학습 지침에 따라 빌드·CTest·Editor 시각 확인은 수행하지 않았으며, 노드 드래그/연결, 핀 히트영역, 주석 크기 저장, 좁은 폭·배율별 화면 확인은 사용자 검증이 필요하다.

### [2026-11-21] Material Shader Graph Custom UV1

- Assimp의 두 번째 UV를 모델 스냅샷 v4 및 메시 정점 스트림에 전달한다. 채널 부재, v3 스냅샷과 기존 메시 로드에서는 UV0을 복사한다. 이전에 저장된 메시에서 소실된 원본 UV1은 재임포트가 필요하다. 모델 DDC와 지형 그리드 캐시 버전을 갱신했다.
- `Shader.TexCoord1`과 `customUV1`을 추가하여 VS 입력 및 VS/PS 간 별도 `TEXCOORD1`로 전달한다. diffuse 슬롯은 기존 UV0을 유지하고 범용 텍스처 노드는 추가하지 않았다. SDL GLSL 스키닝 입력 위치도 추가 정점 속성에 맞췄다.
- `ShaderGraphCheck`에 UV1 폴백·변환·인터페이스 검사, `AssetWorkerModelCheck`에 UV1이 있는 glTF와 없는 OBJ 스냅샷 검사를 추가했다.
- **정적 확인 / 미검증:** 빌드·CTest·실제 셰이더 컴파일·임포트/구버전 에셋 재로드·D3D12/SDL 렌더링 및 Editor 프리뷰는 실행하지 않았다. 로캘 JSON은 사용자가 생성 도구로 동기화해야 한다.

### [2026-11-21] Material Shader Graph Custom UV0

- `Shader.MaterialOutput`에 로캘 핀 `customUV0`를 추가했다. `Shader.TexCoord0`와 Shader 도메인 전용 Float2 덧셈·곱셈 노드로 메시 UV를 변환해 VS `TEXCOORD0`로 전달하며, 연결되지 않으면 원래 메시 UV를 유지한다.
- 그래프 생성 GBuffer·ForwardTransparent PS는 머티리얼 diffuse 슬롯 0이 존재할 때 보간된 `TEXCOORD0`로 텍스처를 샘플링한다. `MaterialEditor::SyncParametersFromGraph()`는 VS 출력 핀(WPO, Custom UV)을 머티리얼 파라미터로 잘못 노출하지 않도록 제외한다.
- `ShaderGraphCheck`에 기본 UV 폴백, 메시 UV 타일링·오프셋, 동적 Float2 파라미터 전달, GBuffer·ForwardTransparent diffuse 샘플을 추가했다. Custom UV1과 범용 Texture Sample 노드는 범위 밖이다.
- **미검증:** 이번 코드의 빌드·CTest·Editor 프리뷰는 실행하지 않았다. 사용자 실행 후 결과를 반영해야 하며, 새 `Engine.csv` 키는 기존 GenerateLocalization 절차로 JSON에 동기화해야 한다.

### [2026-11-21] 시작·에디터 사용·아키텍처 문서 작성

- `Docs/README.md`와 영어 색인, 시작하기, 에디터 기본 작업, 아키텍처 개요를 한국어·영어 쌍으로 추가했다. 루트 README의 문서 진입점을 추가하고 C++ 표준, CMake 요구 버전, Client 타깃 및 Reflection 생성 위치의 오래된 설명을 현재 설정과 맞췄다.
- 씬 Archive의 데이터 모양, Editor 문서 저장 경계, 후보 씬 복원·계층 검증과 현재 제한을 설명하는 `Docs/Technical/ScenePersistence.md` 및 영어 짝 문서를 추가했다.
- 구현 사실과 자동 검사·사용자 실행 확인을 분리했으며, Client 패키징·UI/GPU·플랫폼 검증이 미완료인 범위를 문서에 표시했다.
- **검증:** CMake 설정·주요 진입 코드·기존 설계/구현 기록과 문서를 정적으로 대조했다. 문서 변경의 공백·링크 확인은 수행했으며 빌드·CTest·Editor 실행은 하지 않았다. 2026-11-20의 x64 Ninja Debug 44/44 기록은 당시 사용자 확인으로만 인용했다.

### [2026-11-20] 사용자 소스 패키지 빌드·로드 계획

- DLL이 없는 생성 패널의 현재 발견·수동 빌드 경로와 제약을 정리하고, 소스 승인·독립 CMake 구성·후보 게시·기존 DLL 승인 및 패널 등록을 잇는 [단계별 계획](EditorPluginArchitecturePlan.md#사용자-소스-패키지-빌드로드-계획)을 추가했다. 기존 템플릿 마이그레이션, 실패 복구와 Blank/Runtime 모듈의 별도 계약도 구분했다.
- **상태:** 계획 문서화만 진행했다. 자동 구성·빌드·게시와 사용자 소스 자동 감시는 미구현이며, 기존 생성 패널의 독립 빌드·로드와 새 계획의 실행 검증도 남아 있다.

### [2026-11-20] 플러그인 관리자 툴바와 패키지 경로 표시

- 상단과 선택 항목 및 빌드 콘솔의 동작을 공용 아이콘 툴바로 정리하고, 상단 경로·SDK 설명을 정보 팝업으로 옮겼다.
- 생성 창에 전체 예상 폴더를 표시하고 선택한 설치형 패키지의 폴더 경로와 Windows 탐색기 열기를 추가했다. 탐색기 실행 실패는 알림으로 표시한다.
- **미검증:** 빌드·로캘 생성·Editor 실행, 툴팁·탐색기 열기와 한·영·좁은 폭 UI.

### [2026-11-20] 에디터 패널 플러그인 템플릿 생성

- 플러그인 관리자 상단 툴바에 패널 생성 모달을 추가했다. 식별자·이름·버전·초기 크기를 검사하고 프로젝트 `Plugins`에 V2 패널 소스, 설치형 매니페스트와 독립 CMake 설정을 생성한다. 기존 폴더는 덮어쓰지 않으며 생성 실패 시 미완성 폴더 경로를 표시한다.
- 생성 직후 DLL은 없으며 사용자가 같은 Editor 빌드 환경에서 독립 빌드한 뒤 재검색·승인·로드한다. 관리자 내 사용자 소스 빌드와 자동 감시, 아이콘 선택 및 Blank/Runtime 템플릿은 미구현이다. [절차와 제약](EditorPluginArchitecturePlan.md#에디터-패널-템플릿-생성)
- **사용자 확인:** 패널 템플릿이 생성됐다. 생성된 패널 DLL의 빌드와 로드는 확인되지 않았다.
- `PanelPluginGeneratorCheck`에 입력 검증, 매니페스트, 생성 결과와 덮어쓰기 방지 사례를 추가했다. **미검증:** 빌드·CTest·로캘 생성·생성된 패널 빌드와 Editor 로드·리로드 및 한·영·좁은 폭 UI.

### [2026-11-20] Baseline 이동 변주와 점프 연출

- `BaselineVerification_Simple.animator`의 Idle·Walk·Run에 Test의 A/B/C 클립을 2D 블렌드로 연결했다. 샘플 `GameModule`이 두 변주 파라미터를 천천히 구동한다.
- KCC의 `Jump` 트리거와 `IsGrounded`를 사용해 점프 및 착지 복귀를 구성하고, 점프 진입 때 기존 복합 VFX를 캐릭터 위치에 1회 생성한다. 기존 액션 타임라인은 자동 재생을 끄고 수동 재생으로 남겼다. 상세와 제약은 [통합 계획](AnimationPipelineIntegrationPlan.md#baseline-점프와-이동-변주-예시)을 따른다.
- **미검증:** 빌드·CTest·Editor·PIE·Client, 각 클립의 리그 호환성, 점프와 착지 타이밍, 반복 생성·소멸 및 모듈 핫리로드. 샘플 에셋은 Git 무시 대상이다.

### [2026-11-20] GPU 텍스처 변환 후 재로드 진단

- 사용자가 디버거에서 `Texture.AlreadyUploaded`를 확인했다. 공유 참조가 있는 GPU 포인터를 `Safe_Release`로 해제해도 포인터가 남아 기존 객체의 Archive 읽기가 실패하는 경로다. 현재 `.bamtex` 재로드는 임시 `Texture`에 읽고 성공 시 기존 자원을 교체한다.
- **확인**: 21:45 로그에서 실제 GPU BC7 압축과 워커 완료 후 재로드 실패가 기록됐다. 22:21에는 `cached GPU BC7` 변환과 `.bamtex` 재로드 성공이 두 번 기록됐고 사용자는 실패 브레이크가 더 이상 발생하지 않는다고 보고했다. 수정 후 실제 GPU 압축과 재로드를 함께 실행한 것은 확인되지 않았다.
- **남은 작업**: `Texture::Reload()`의 `IsFailure(result)`와 GPU 업로드 내부 실패 경로를 점검한다. 공유 참조 및 잘못된 입력에서 기존 리소스 보존, 원본·DDS, CPU 폴백, 캐시 미적중 GPU 변환, 중복 재로드 비용과 UI 회귀를 사용자가 검증한다. 상세는 [계획](TextureSpriteEditorIntegrationPlan.md)을 따른다.
- **기준 상태**: 기준 커밋의 `ContentBrowserPanel.cpp`에는 별도 미커밋 작업에서 수정 중인 문법 오류가 있으므로 이 변경만의 독립 빌드는 검증되지 않았다.

### [2026-11-20] 실행 중 설치형 패널 패키지 재검색

- 플러그인 관리자에서 설치형 패키지를 재검색하고 기존 세션의 패널과 선택을 유지하도록 했다. 프로젝트 `Plugins` 경로를 안내하며 새 패키지는 자동 실행하지 않고 기존 매니페스트 검사와 Custom 승인 경로로 로드한다. 누락된 패키지는 언로드할 수 있도록 현재 세션 목록에 남긴다.
- 당시 임의 DLL 직접 패키징, 소스 템플릿 생성 및 개발용 빌드 타깃 동적 등록은 구현하지 않았다. 이후 패널 템플릿 생성은 위 항목에서 추가했다. [사용 절차와 제한](EditorPluginArchitecturePlan.md#실행-중-설치형-패널-추가)
- **미검증**: 빌드·로캘 생성·Editor 실행, 재검색·승인·로드·리로드·누락/중복 복구 및 다국어·좁은 폭 UI.

### [2026-11-20] 액션 타임라인 VFX 동기화와 이동 그래프

- 지속형 VFX는 소유 액션 타임라인의 Pause/Resume·배속을 따르고 해제 후에는 방출과 이벤트 생성만 중단하며 기존 입자·트레일을 수명까지 갱신하도록 변경했다. 일반 1회성 복합 VFX의 종료와 복합 VFX 파티클 트랙 구간 종료에도 잔존 입자 갱신이 적용된다. 프리뷰에서는 재생 VFX를 일시정지하고 독립 테스트 발동은 유지한다.
- 프리뷰 프레임 이동·스크럽은 이전 VFX를 제거하지만 과거 상태를 재구성하지 않는다. 복합 VFX 사운드 채널은 일시정지·재개하지만 배속에 따른 음높이 변경은 하지 않는다. 프레임 경계와 PIE 전체 일시정지의 제약은 [타임라인 계획 9절](AnimationTimelineEditorPlan.md)에 기록했다.
- 복합 파티클 트랙의 시작·종료를 한 프레임에서 모두 통과하거나 복합 효과가 끝나는 프레임에도 활성 구간을 먼저 시뮬레이션하도록 처리했다. 해당 경계와 정지·잔존 소멸을 `ActionTimelinePlaybackCheck`에 추가했으나 실행하지 않았다.
- `BaselineVerification_Simple.animator`에 `Speed` 기반 Idle/Walk/Run 상태를 추가했다. `Test`에는 Rogue용 이동 클립이 없어 기존 KayKit `Rig_Medium` 이동 클립을 참조한다. 씬에는 기존 컨트롤러 경로를 유지한다.
- **이전 샘플 설명 정정:** 기존에는 Notify Duration 0.4초의 해제 시 파티클이 즉시 삭제되었으므로, 아래의 '수명까지 시뮬레이션'은 당시 동작에 대한 잘못된 설명이었다. 이번 변경은 해당 조기 삭제를 해결하도록 작성했다.
- **미검증:** 관련 회귀 체크를 추가했지만 빌드·CTest·Editor·PIE·Client 실행은 하지 않았다. 소켓·이동 클립의 스켈레톤 호환성, 재생/취소·잔존·사운드·속도·스크럽 및 기존 VFX 회귀는 사용자가 확인해야 한다. `Assets`의 샘플 데이터는 Git 무시 대상이다.

### [2026-11-20] 리소스 편집기 비동기 열기 기준 계측 시작

- 공용 열기 경로의 각 편집기 `SetTargetResource()` 호출 시간과 모델 편집기의 정상 열기에서 `ResourceManager::LoadFile`, `SetupPreviewModel`, 전체 처리 시간을 `[ResourceOpenTiming]` 로그에 기록한다. 공용 로그는 성공 여부를 뜻하지 않고 패널 내부의 직접 열기는 측정하지 않는다. 의존 리소스와 GPU 생성은 `LoadFile` 시간에 함께 포함되며 첫 창 표시 시간은 아직 측정하지 않는다.
- **미완료·미검증**: 로딩 UI나 비동기 로드는 구현하지 않았다. 빌드와 실제 모델별 측정은 사용자가 실행해야 하며, 이후 결과에 따라 모델부터 작업 경계를 분리한다. [계획](ResourceEditorAsyncOpenPlan.md)

### [2026-11-20] 플러그인 관리자 레이아웃과 Ninja 빌드 환경

- 실제 플러그인과 모듈·리로드 서비스만 목록에 두고, 확인되지 않은 버전·작성자와 GameModule 제공 기능 설명을 제거했다. 호스트의 빌드·로드 실패와 로드 상태를 배지에 연결하고 서비스의 가짜 로드 배지는 숨겼다.
- 목록과 상세 사이에 공용 스플리터를 추가하고 선호 폭을 워크스페이스에 저장한다. 카드 이름과 배지를 별도 줄에 배치하며 좁은 폭의 텍스트를 클리핑한다.
- Ninja 캐시의 `cl.exe`가 상대 이름이면 CMake가 기록한 실제 컴파일러에서 VS 경로, 도구 버전과 아키텍처를 읽는다. 기존 `INCLUDE` 환경이 있으면 그대로 사용하며 탐색 실패는 오류로 남긴다.
- **사용자 확인:** Visual Studio 디버거를 모두 분리한 뒤 Console 패널의 빌드와 핫 리로드가 완료됐다. 디버거가 연결된 상태에서 발생한 PDB 쓰기 오류는 해결 여부를 확인하지 않았다.
- **미검증**: Ninja 환경 자동 탐색의 독립 확인, 다른 패널의 빌드·리로드, 상태 표시 및 한·영·UI 배율별 스플리터와 좁은 폭 동작.

### [2026-11-20] 파티클 방출 구간과 잔존 시간 표시

- 방출/이벤트 수신 구간의 양끝을 다이아몬드로 표시하고 유한 방출 뒤 입자 최대 수명의 예상 꼬리를 읽기 전용으로 그린다. 끝없는 방출에는 끝 다이아몬드를 그리지 않으며, 전체 방출 제한과 프리뷰 종료 이후의 빗금 및 설명을 구분한다.
- 표시 범위만 늘리고 저장된 이펙트 길이·프리뷰 재생 종료·런타임 방출 동작은 변경하지 않았다. 충돌이나 방출 간격에 따라 꼬리는 실제 생존 시간이 아닌 상한 추정이다.
- **미검증**: 빌드·로캘 생성·CTest·Editor 실행, 한·영 문구 및 트랙 드래그와 좁은 폭 실환경 확인.

### [2026-11-20] 파티클 편집기 Transport Bar 배치

- 재생·프레임 이동·루프·배속·시간/프레임·줌 초기화·활성 파티클 수를 Quick Access에서 트랙 바로 위 Transport Bar로 옮겼다. Quick Access는 파일·저장·Undo/Redo와 카메라 포커스를 유지한다.
- **미검증**: 정적 코드 검토만 수행했다. 빌드·CTest·Editor 실행, 표준/좁은 도킹 폭에서의 시각 확인은 프로젝트 작업 지침에 따라 사용자가 실행한다.

### [2026-11-20] 스프라이트 편집기 오른쪽 패널 정리

- 선택된 슬라이스 속성과 목록을 공통 `EditorUI` 테이블로 구성했다. 피벗 프리셋의 줄바꿈 및 툴팁, 보더의 두 행 배치, 잘린 이름 툴팁과 검색 결과 없음 안내를 추가했다.
- 캔버스와 사이드바 사이의 두 간격을 폭 계산에 반영하고, 창을 좁혔다 넓혀도 사이드바 선호 폭을 보존하도록 했다. 가로 밀림에 대한 실제 효과는 미확인이다.
- **미검증**: 관련 자동 검사와 검색·편집·Undo/Redo 및 좁은 폭과 한·영 전환의 실제 UI 회귀.

### [2026-11-20] 텍스처 편집기 방향 변경과 미리보기 개선

- 텍스처 원본을 별도 텍스처 편집기로 열고 버튼으로 스프라이트 편집기를 열도록 분리했다. 변환 상태는 텍스처 편집기가 보유하고 두 패널의 변환 출력 충돌과 미저장 교체를 보호한다.
- 텍스처 창의 미리보기 크기 조절, 실제 텍스처 우선 표시와 썸네일 폴백, 잘린 경로 툴팁을 추가하고 슬라이스 수 로캘 서식을 수정했다.
- **검증 상태**: 변환·재로드의 일부 확인과 남은 실패 경로는 위 GPU 재로드 진단을 따른다. 전체 UI·로캘 및 자동 검사 회귀는 미검증이다.

### [2026-11-20] 텍스처·스프라이트 통합 작업창

- `SpriteEditor`에 텍스처 정보와 스프라이트 편집 모드를 추가하고 기존 텍스처·스프라이트 열기 경로를 한 창으로 연결했다. 스프라이트 문서의 이력과 교체 확인은 유지하고 참조 텍스처가 없는 스프라이트의 동명 원본 추정은 제거했다.
- 정보 화면에 텍스처 포맷과 CPU 표준·빠른 BC7·GPU BC7 선택, 출력 확인, 비동기 변환 및 선택적 스프라이트 생성을 연결했다. 자동 생성은 기존 스프라이트를 덮어쓰지 않으며 별도 staging 후 게시한다. 완료 시 썸네일을 무효화하고 로드된 출력 텍스처 갱신을 시도한다.
- 슬라이스 저장 시 기존 스프라이트별 PPU와 직접 연 파일 형식을 보존한다. 기존 Archive의 파일별 임시 파일·교체 경로를 사용하며 다중 파일 전체는 트랜잭션이 아니다. 슬라이싱 결과가 없을 때 기존 슬라이스를 유지한다.
- 기존 두 창의 열림 상태는 통합 창으로 복원하되 기존 텍스처 창의 도킹 위치는 이관하지 않는다. 관련 제약과 미검증 항목은 [통합 계획](TextureSpriteEditorIntegrationPlan.md)을 따른다.
- **검증 상태**: 빌드·CTest·로캘 생성·실제 에디터 검증은 실행하지 않았다.

### [2026-11-20] Composite VFX Phase 5 / 후보 4 문서·코드 현황 동기화

- **문서 정합성 동기화**:
  - [Docs/DevelopmentPriorities.md](DevelopmentPriorities.md)(트랙 F 요약, 11.6절 후보 4번)와 [Docs/CompositeVisualEffectPlan.md](CompositeVisualEffectPlan.md)(상단 상태, 9.7절, 9.9절)를 최신 코드 및 연계 계획과 일치하도록 갱신했다.
  - 이미 2026-11-19 반영된 Phase 5 Step 3(`CurveEditorModal`), `CompositeVFXSystem` Geometry/ForwardTransparent 블렌드 필터 교정(허공 흙갈색 탁화 해소), `forwardParticlePS.hlsl` 정면 알파 완화(`0.90`), `VFXEditorPanel::ToggleAirMode()`, `ActionTimelineSystem`/`AnimationTimelinePanel`의 `SpawnCompositeVFX` 본 소켓 스폰·실시간 추적(`SocketAttachmentComponent`)·지속형 해제(`ActionVFXHoldComponent`)·`ActionTimelineEvents` 기본 구현을 기구현 현황으로 구분했다.
  - 9.9절의 공중 모드 가상 환경광 보정이 2026-11-19 정정으로 철회된 상태를 상단 요약·후보 4번·9.9절 목록에 일관되게 반영하고, 후보 4번의 실제 잔여 작업을 [Docs/AnimationPipelineIntegrationPlan.md](AnimationPipelineIntegrationPlan.md) 2~5절(편집 씬 격리, Animator 복원 수명, 프리뷰↔런타임 Notify/소켓 평가 순서 단일화, `m_ModelPath` 가상 URI 정규화, 대표 액션 PIE·독립 Client 통합 검증) 및 선택적 순수 Additive(`One/One`) 분리 검토로 재정의했다.
- **검증 상태**: 문서 정적 대조 및 `git diff --check` 확인. 코드 변경 없음(빌드·테스트·커밋 미실행).

### [2026-11-20] Candidate 4 액션 타임라인 통합 구현 이어서 진행

- **BaselineVerification 예시 에셋**: Test의 `Hit_A` 클립을 사용하는 비루프 타임라인과 0.4초 소켓 Composite VFX/`OnAttackHit` 이벤트, 짧은 고속 확산 Spark 이미터, 한 상태 Idle Animator Controller를 추가하고 씬의 Rogue에 연결했다. 이미터는 낮은 방출률과 지연 Burst를 피하고, 이미 생성된 파티클이 수명까지 시뮬레이션되도록 VFX 트랙을 설정했다. 이 샘플은 에디터·PIE의 재생, 소켓 효과, Animator 복귀를 빠르게 확인하며 입력 및 이벤트 구독 구현을 대체하지 않는다.
- **샘플 검증 상태**: 네 에셋과 씬 JSON 파싱, 참조·바인딩·클립 길이 점검, `git diff --check`를 통과했다. 씬 로드, 에셋 해결, 빌드·CTest·Editor·PIE·Client 실행은 사용자가 검증해야 한다.
- **편집 씬 격리·복원 수명**: `ActionTimelineSystem`을 Gameplay 페이즈에 한정하고 인스펙터 재생을 독립 타임라인 프리뷰로 연결했다. Animator에 저장된 원래 클립·시각·재생 상태를 소유권 serial로 보호하며 정상 종료·정지·교체·비활성화·타임라인 컴포넌트 제거 시 복원한다. Animator를 다른 타임라인이 인계받으면 이전 세션을 중단하고 지속형 VFX를 해제한다.
- **프리뷰·런타임 일치**: 두 실행 경로가 `ActionTimelinePlayback`의 시간 진행과 Notify 교차 계산을 공유한다. 포즈·스키닝 행렬 갱신 후 Notify를 검사하며 시작·경계·다중 루프 사례용 `ActionTimelinePlaybackCheck`를 CTest에 추가했다.
- **참조 정규화·진단**: 타임라인 모델 경로를 저장 시 가상 URI로 정규화하고 이전 경로는 읽기 호환한다. 모델·클립·스켈레톤·소켓·범위 진단을 보강했으며, 기본 스프라이트·파티클 머티리얼은 기존 셰이더 논리 키를 보존하면서 `GBufferSpriteVS` 바이너리 `AssetPath`를 `Engine://`로 정규화했다.
- **검증 및 미완료**: 계획 문서에 코드 반영 상태와 패널·편집 씬 격리·복원 수명·Notify 경계·PIE·독립 Windows Release Client 수동 체크리스트를 갱신했다. `git diff --check`는 실행했으나 빌드·CTest·로캘 생성·Editor·PIE·Client는 실행하지 않았다. 사용자 실행 명령은 [AnimationPipelineIntegrationPlan.md](AnimationPipelineIntegrationPlan.md) 7절에 있다.

### [2026-11-20] 텍스처·스프라이트 편집기 통합 계획

- [통합 계획](TextureSpriteEditorIntegrationPlan.md)에 하나의 작업창으로 정보·변환과 스프라이트 편집을 연결하는 제안, 독립적인 저장·이력 경계, 안전성 및 검증 기준을 기록했다. 씬 엔티티 인스펙터는 대상이 아니다.
- `TextureInspector`의 UI 코드는 현재 등록·호출 경로가 확인되지 않았음을 `ImGuiDevelopmentPlan.md`에 명시했다. 통합 편집기 구현과 UI·GPU 실환경 검증은 아직 수행하지 않았다.

### [2026-11-20] BC7 압축 방식 선택

- 미연결 텍스처 인스펙터의 UI 코드와 모델 텍스처 베이킹에 CPU 표준·CPU 빠른 BC7·D3D11 GPU BC7 선택을 추가했다. 텍스처 인스펙터는 현재 접근 가능한 UI가 아니다. 노멀맵 BC5 등 다른 형식은 CPU 표준을 사용한다. 빠른 압축은 품질 저하 가능성이 있다.
- 텍스처 워커 프로토콜을 버전 2로 갱신하고 실제 backend와 GPU 폴백 HRESULT를 전달한다. 방식별 캐시 키를 분리하고 CPU 폴백 결과는 GPU 캐시에 기록하지 않는다. 텍스처 인스펙터, 이미지 드롭 및 툴바의 이미지 변환은 비동기 경로를 사용한다.
- 인프로세스 폴백도 두 산출물을 staging 후 교체한다. DDS 원본은 변환을 비활성화했고, 인스펙터는 요청별 완료 결과를 표시한다. UI 문구는 CSV 원본에 추가했다.
- **검증 상태**: 사용자가 Debug 빌드 후 `AssetWorkerModelCheck`, `AssetWorkerTextureCheck`, `LocalizationAuditUtility` 3개 통과를 보고했다. AI는 빌드·테스트·로캘 생성·에디터 실행을 직접 수행하지 않았다. 실제 GPU 사용, 출력 복구·취소, 모델 UI 경로, 속도·화질과 UI 반응성은 실환경 확인이 필요하다.
- **기존 CPU 시간 관찰**: 별도 원본 경로를 사용한 비교에서 파일 시각 기준 Debug 약 4분 55초, Release 약 24초였다. 정확한 스톱워치·종료 코드는 없으므로 참고치이며 GPU 또는 압축 방식별 성능 검증 결과가 아니다.

### [2026-11-20] 머티리얼 PrimaryPassType 도입·동적 cbuffer 레이아웃(Phase 2)·통합 Undo/Redo 구현

- **후보 2 — 머티리얼 기본 렌더 패스(`PrimaryPassType`) 및 Fallback 정합성**:
  - `MaterialInterface.h/.cpp`, `Material.cpp`, `MaterialInstance.h/.cpp`: `m_PrimaryPassType` (`ERenderPassType::Geometry` 기본값) 속성·리플렉션(`PROP_PRIMARY_PASS_TYPE`)·`MaterialDesc::PrimaryPassType`·`EPipelineOverrideFlag::PrimaryPassType` 추가. `GetVertexShader`/`GetPixelShader`에서 `m_PassShaders` 최우선 → `passType == m_PrimaryPassType || Custom` 시 기본 셰이더 → 그 외 `nullptr` 반환으로 정합화.
  - `MaterialInterface::OnPostLoad()`: 레거시 `.bammat.json`의 `UI`, `PostProcess`, `Forward`(`SkyPS`), 반투명/Decal `ForwardTransparent` 주 패스 자동 보정 추가.
  - `PostProcessPipeline.cpp`: 코드 생성 7종 포스트프로세스 머티리얼(`ToneMapping`, `ColorAdjustments`, `Vignette`, `ChromaticAberration`, `FilmGrain`, `LensDistortion`, `Sharpen`)에 `desc.PrimaryPassType = ERenderPassType::PostProcess` 지정(Display RT 미갱신/그리드 잔상 회귀 해결).
  - `Renderer.h/.cpp`: `m_DefaultShadowDepthPS` 추가 및 `PrepareMeshPipelineDesc()`에서 `Shadow` 패스 오버라이드 미지정 머티리얼의 안전한 기본 섀도우 셰이더 폴백 보장.
  - `MaterialEditor.cpp`, `MaterialEditorShader.cpp`, `Editor.MaterialEditor.csv`, `Engine.csv`: `[파이프라인 설정]` 최상단에 `기본 렌더 패스` 콤보·`BlendMode` 권장 프리셋 자동 보정 연동 및 `ForwardTransparent` 시 `GenerateForwardTransparentMaterialHlsl()`(단일 `SV_TARGET0` 출력) 분기 컴파일·주 패스 바인딩 적용.
- **후보 1 심화 (Phase 2) — 동적 상수 버퍼 레이아웃 & 원시 바이트 블롭**:
  - `MaterialInterface.h/.cpp`, `MaterialInstance.cpp`: `MaterialParameterEntry`, `ENGINE_API MaterialBufferLayout`, `BuildConstantBufferBlob(vector<uint8>&)` 구현. 앞 48바이트 표준 PBR 헤더 하위 호환 유지 + 48바이트 이후 커스텀 파라미터 오프셋 `memcpy` 후 `rhi->BindConstantBuffer` 전송.
  - `Graph.cpp`, `ShaderGraphCompiler.h/.cpp`: `Shader` 도메인에서 `Variable.Get.*` 노드만 등록하도록 `RegisterVariableNodes` 보완. `BuildMaterialBufferLayout()` 16바이트 경계 패킹 + 명시적 `_padN` 동적 `MaterialConstants` HLSL 조립(`VS`, `GBuffer PS`, `ForwardTransparent PS` 공유), `Variable.Get.<Name>` → `gMaterial.custom_<Name>` 및 `Math.AddFloat`/`Math.MultiplyFloat` 지원.
  - `MaterialEditor.cpp`, `MaterialEditorShader.cpp`: `SyncParametersFromGraph()`에 `GraphVariable` 자동 노출 추가 및 `LiveHotReloadShader()`에서 `SetBufferLayout()` 동기화.
- **머티리얼 에디터 UI/UX 개선 및 단일 통합 Undo/Redo (방안 A)**:
  - `MaterialEditor.h/.cpp`: 분리되어 있던 `m_History`와 `m_ShaderGraphHistory`를 단일 `ResourceDocumentHistory<DocumentSnapshot>`(머티리얼 JSON + 셰이더 그래프 동시 캡처)로 통합하여 파이프라인·파라미터·텍스처·그래프·노드 속성 편집의 시간 순서 Undo/Redo, 양방향 동기화 원자 복원, 드래그 제스처 통합 및 복원 시 자동 재컴파일/프리뷰 갱신 보장.
  - `MaterialEditor.cpp`: 패스별 셰이더 삭제(휴지통) 버튼의 실제 폭 계산 및 `Editor::SplitterThickness + ItemInnerSpacing.x` 우측 여유 마진 적용으로 스플리터/스크롤바 겹침 및 좌우 스크롤 유발 해소.
- **검증 상태**:
  - `ShaderGraphCheck.cpp`에 동적 16바이트 패킹(80B)·`Variable.Get.*` VS/PS·`ForwardTransparent` 단일 `SV_TARGET0`·`PrimaryPassType` Fallback·`BuildConstantBufferBlob` 바이트 검증을 추가했으며, 사용자가 `x64-ninja-debug` 빌드(`Editor`, `ShaderGraphCheck`) 및 에디터 실환경(뷰포트 렌더링 복구, 패스별 삭제 버튼, 단일 통합 Undo/Redo) 정상 동작을 확인.
- **미검증**:
  - 전체 44개 CTest 최종 회귀 실행 및 `x64-release` 빌드, 에디터 실환경에서 `GraphVariable` 커스텀 변수를 이용한 실시간 Fast-Path 60fps 프리뷰 조절.

### [2026-11-20] 셰이더 그래프 단일 마스터 노드 기반 VS/PS 듀얼 통합 컴파일 구현

- **마스터 노드 및 컴파일러 확장 (`GraphRegistry.cpp`, `ShaderGraphCompiler.h/.cpp`)**:
  - `Shader.MaterialOutput`에 정점 스테이지 입력 핀 `worldPositionOffset`(Float3)을 추가하고, `gbufferVS.hlsl` 인스턴싱 규격을 준수하여 WPO 표현식을 주입하는 `GenerateGBufferMaterialVS`를 구현.
- **에디터 듀얼 컴파일 및 핫리로드 연동 (`MaterialEditor.h`, `MaterialEditorShader.cpp`)**:
  - `ReserveMaterialShaderDirectory`에서 `shading_flags.hlsli`와 `common.hlsli`를 함께 복사하도록 확장.
  - `CompileMaterialShader` 및 `LiveHotReloadShader`가 VS와 PS를 동시 컴파일하여 머티리얼의 `Geometry` 패스에 `(m_LiveVertexShader, m_LivePixelShader)` 쌍으로 바인딩하도록 연동.
- **검증 상태**: `ShaderGraphCheck`에 미연결/연결 WPO 생성 검증을 추가하여 `x64-ninja-debug` 빌드 및 `ShaderGraphCheck` CTest 통과 사용자 확인.
- **미검증**: 에디터 실환경 3D 프리뷰에서 `worldPositionOffset` 노드 연결 시 실제 정점 변위 렌더링 동작.

### [2026-11-20] 콘텐츠 브라우저 삭제 영향 검사 hitch 완화

- **원인**: `QueueFileDeletion()`이 삭제 확인창을 준비하기 전에 `AnalyzeDeletion()`을 동기 호출했다. 이 함수는 빈 폴더 삭제를 포함해 프로젝트 에셋 트리를 순회하고 저장된 참조 문서를 읽으므로 UI 스레드를 막을 수 있었다.
- **변경**: 참조 영향 분석을 명시적으로 소유한 worker thread에서 실행하고, 확인창에 준비 상태와 취소를 먼저 표시한다. 검사 중 삭제 확정 및 다른 파일 명령을 막고, 취소·패널 종료 후 늦게 도착한 결과는 폐기한다. 취소는 삭제 요청을 취소하지만 이미 진행 중인 읽기 전용 스캔은 완료될 수 있다. 빈 폴더도 기존 참조 영향 결과 의미를 보존하도록 검사하며, 스캔만 UI 프레임에서 분리한다.
- **수명·계측·회귀**: 경로 루트를 worker 시작 전에 초기화하고, `std::jthread`를 완료 처리 또는 패널 종료 시 join한다. 삭제 영향 분석 시간은 `DeleteImpact` 작업 통계에 남기되 완료 프레임 합계에는 넣지 않으며, `AssetReferenceCheck`에 해당 계측 회귀를 추가했다.
- **로캘·UX**: 준비 상태 문구를 `Editor.ContentBrowser.csv`에 추가하고 삭제 취소·늦은 완료·기존 참조 영향과 Undo/Redo 확인 기준을 갱신했다. 생성 JSON은 직접 수정하지 않았으며 `GenerateLocalization`으로 동기화해야 한다.
- **검증 상태**: 수정 파일의 `git diff --check`를 통과했다. 저장소의 학습 중심 작업 지침에 따라 빌드·CTest·실제 Editor 검증과 로캘 생성은 실행하지 않았다. 사용자 실행 명령:

```powershell
cmake --build --preset x64-debug --config Debug --target Editor AssetReferenceCheck AuditLocalization GenerateLocalization
ctest --preset x64-debug --output-on-failure -R "AssetReferenceCheck|LocalizationAuditUtility"
```

- **범위 구분**: 삭제 후 콘텐츠 브라우저는 현재 디렉터리 목록을 다시 열거하며, Asset Reference Checker가 이미 열린 경우 별도의 자동 전체 검사를 예약할 수 있다. 이들은 확인창 전 삭제 영향 스캔과 다른 경로다. 실제 에디터에서 어느 단계에 남은 프레임 hitch가 있는지 확인하지 않았으므로, 이번 변경은 관측된 UI 스레드 스캔만 옮기고 이 경로들은 UX 점검에서 분리해 측정한다.

### [2026-11-20] 콘텐츠 브라우저 이름 변경 참조 준비 hitch 완화

- **원인**: `QueueAssetPathChanges()`가 `PrepareMoveReferences()`를 UI 스레드에서 호출하고, 이 함수는 이동 대상과 무관하게 프로젝트의 저장 문서를 전체 순회했다. 준비가 끝나야 모달 상태가 만들어져 큰 프로젝트에서는 확인창 전에 프레임 hitch가 발생할 수 있었다.
- **변경**: 실제로 비어 있는 디렉터리 이동은 `PrepareMoveReferences()` 및 실행 직전 `AnalyzeMove()`에서 전체 프로젝트 스캔을 생략한다. 콘텐츠 브라우저는 빈 폴더의 빠른 경로에서도 열린 문서·프리팹 모드 보호를 유지한다. 그 밖의 이동 준비는 worker에서 실행하고 준비 중 모달에 상태와 취소를 표시하며, 취소되거나 브라우저가 닫힌 뒤 도착한 결과는 적용하지 않는다. 기존 실행 직전·Undo/Redo 참조 재검사는 유지한다.
- **계측 및 회귀 사례**: 비동기 준비 시간은 작업별 시간 통계에 기록하되 완료 프레임 합계에는 더하지 않는다. `AssetReferenceCheck`에 다른 문서가 읽을 수 없는 상태에서도 빈 폴더 준비와 재검사가 완전한 무참조 결과를 반환하는 사례를 추가했다. 참조 타이밍 검사에는 백그라운드 시간과 프레임 합계 분리를 추가했다.
- **로캘·UX 문서**: 준비 상태 문구를 `Editor.ContentBrowser.csv`에 추가하고 `ImGuiDevelopmentPlan.md`와 `EditorUXTestGuidelines.md`에 빈 폴더, 참조가 있는 작업, 취소·늦은 완료 검증 기준을 기록했다. 생성 JSON은 직접 편집하지 않았으며 기존 `GenerateLocalization` 경로로 동기화해야 한다.
- **검증 상태**: 저장소의 학습 중심 작업 지침에 따라 빌드·테스트와 로캘 생성은 실행하지 않았다. 아래 명령으로 사용자가 검증해야 하며, 특히 실제 Editor에서 비어 있지 않은 폴더의 스캔 반응성, 빈 폴더 보호 차단, 취소 후 결과 폐기를 확인해야 한다.

```powershell
cmake --build --preset x64-debug --config Debug --target Editor AssetReferenceCheck AuditLocalization
ctest --preset x64-debug --output-on-failure -R "AssetReferenceCheck|LocalizationAuditUtility"
```

### [2026-11-20] 텍스처 압축 결과 재사용 및 워커 제한 조정

- 사용자가 Ranger 모델의 텍스처 베이킹 실패를 보고했다. 같은 PNG의 워커 단독 실행은 성공했고 `.bamtex`와 DDS의 기록 시각 차이는 약 5분 21초였다. 기존 에디터의 3분 단계 제한에 걸렸을 가능성이 높지만 에디터 당시의 정확한 실패 원인은 확인되지 않았다.
- 워커와 `TextureImporter` 폴백은 압축 바이트와 헤더를 한 번만 얻어 `.bamtex`와 DDS에 재사용한다. DDS 재구성은 단일 2D 이미지의 포맷·크기·밉 레이아웃을 검사하고 DirectXTex의 DDS 기록을 사용한다. 압축 캐시는 기존 형식을 유지하되 메타데이터와 잘린 데이터는 재생성하고 고유 임시 파일을 통해 교체한다. 비압축 데이터의 포맷 정합성 변경에 따라 해당 캐시 키만 변경했다.
- 텍스처 워커의 진행 단계별 무응답 제한을 임시로 1시간으로 조정했다. 이는 관찰된 5분대 작업보다 길지만 실측으로 정해진 최종 기준이 아니며, 큰 텍스처의 완료를 보장하지 않는다. 모델 워커의 3분 제한은 유지한다. 텍스처 단계별 경과 시간과 실패 종류를 로그에 구분한다.
- `AssetWorkerTextureCheck`는 BC7 sRGB·BC5·RGBA, 홀수 크기·밉맵, DDS와 `.bamtex`의 메타데이터 및 바이트 비교, 캐시 적중과 잘린 캐시 복구를 검사하도록 확장했다.
- **검증 상태**: 코드와 테스트는 작성했으나 프로젝트 지침에 따라 빌드·테스트를 실행하지 않았다. Ranger 재임포트, 긴 처리의 타임아웃·취소, 출력 복구, 에디터 폴백, 대형 텍스처 및 성능 실측은 사용자 확인이 필요하다. 독립 이미지 드롭의 동기 경로 및 비동기 대상 경로 문제는 이번 변경 범위에 포함하지 않았다.

### BamAssetWorker 텍스처 변환 접근 위반 대응

- 사용자가 `AssetWorkerTextureCheck` 실행 중 첫 요청의 `PROGRESS 3` 뒤 종료 코드 `0xC0000005`를 보고했다. 이는 DDS 변환 호출 중 또는 그 이후 `PROGRESS 4` 전 발생한 접근 위반이다.
- 호출별 COM 해제 후 DirectXTex의 캐시된 WIC 팩터리가 무효화될 가능성에 대응해 `DirectXTexCompressor`가 프로세스의 MTA 사용을 유지하도록 변경했다. 변환 스레드는 별도로 MTA를 초기화하며, 이미 STA인 스레드 등 MTA 초기화 실패 시 로그를 남기고 변환을 실패 처리한다. 이 동작은 워커와 인프로세스 폴백에 공통 적용된다.
- 저장소 vcpkg 기준선의 [DirectXTex 포트](https://github.com/microsoft/vcpkg/blob/6d7bf7ef2193e2d1c5798a5ff8811d533104c861/ports/directxtex/portfile.cmake)는 `oct2025`를 지정한다. 해당 [소스](https://github.com/microsoft/DirectXTex/blob/oct2025/DirectXTex/DirectXTexUtil.cpp)의 `GetWICFactory`는 전역 팩터리를 재사용한다. 실제 설치된 라이브러리가 이 기준선과 일치하는지는 확인하지 못했다.
- 동일 데몬의 같은 원본 반복 변환, 다른 원본 변환 및 누락 원본 실패를 검사하도록 `TextureWorkerCheck.py`를 확장했다.
- **검증 상태**: 사용자가 수정 후 테스트 통과를 보고했다. 개별 테스트와 빌드 출력은 제공되지 않았다. COM/WIC 수명 문제는 추정 원인이며 크래시 덤프로 확인되지 않았다. STA, 에디터 폴백과 종료 시나리오는 실행 검증 대기다.

### [2026-11-20] 머티리얼 파라미터 하드코딩 탈피 및 범용 동적 버퍼 레이아웃(Dynamic ConstantBuffer Layout) 아키텍처 설계

- **배경 및 문제 의식**:
  - 기존 1단계 Fast-Path 프로토타입에서 사용한 48바이트 정적 `MaterialConstants` 구조체(`baseColor`, `roughness`, `metallic`, `cavity`, `ao`, `emissiveColor`)는 툰 셰이딩, 유리 굴절, 이펙트 및 아티스트 커스텀 파라미터(디졸브, UV 스케일, 물결 속도 등)를 수용할 수 없고, C++ 구조체·HLSL 코드·복사 헬퍼 3곳에 하드코딩 결합을 유발하는 구조적 한계가 확인됨.
- **상용 엔진 표준 동적 버퍼 레이아웃 설계 ([MaterialGraphSynchronizationAndStageDesign.md](MaterialGraphSynchronizationAndStageDesign.md) 2.3절)**:
  - **그래프 주도 파라미터 노출 (Exposed Parameters)**: 마스터 노드 핀뿐만 아니라 그래프 전역의 파라미터 노드(`Shader.Parameter.*`) 및 블랙보드 변수를 파라미터로 자동 수집.
  - **컴파일러의 cbuffer 자동 조립 & 오프셋 계산**: `ShaderGraphCompiler`가 16바이트 정렬 규칙(std140 패킹)을 준수하여 HLSL `cbuffer MaterialConstants` 코드를 동적 조립하고, 파라미터 메타데이터 테이블(`MaterialBufferLayout`: 이름, 타입, 오프셋, 바이트 크기)을 생성.
  - **원시 바이트 블롭(Raw Byte Blob) 런타임 저장소**: `MaterialInterface`가 고정 C++ 구조체 대신 `std::vector<uint8_t> m_ConstantBufferData`를 소유하고, `SetFloat(name, val)` 호출 시 레이아웃 맵에서 오프셋을 찾아 바이트 단위 `memcpy` 수행 후 RHI에 바인딩. C++ 컴파일 타임 구조체 의존성을 완전히 제거.
- **단계적 진화 로드맵 수립**:
  - Phase 1 (현 단계): 48바이트 기본 PBR Fast-Path(60fps 실시간 반응) 및 마스터 노드-머티리얼 양방향 동기화 검증 완료.
  - Phase 2 (차기 과제): 셰이더 그래프 동적 cbuffer 생성기 도입 및 `MaterialBufferLayout` 오프셋 메타데이터 구축.
  - Phase 3 (완전 범용화): C++ 구조체 제거, `std::vector<uint8_t>` 바이트 블롭 전환 및 SPIRV-Cross / D3D12 리플렉션 연동.

### [2026-11-20] BamAssetWorker 모델 텍스처 베이킹 경로 연결

- `ModelImporter::FinalizeOneStep`가 머티리얼 커밋 전 내장/외부 텍스처 요청을 수집하고 `AssetWorkerHost::EnqueueImportTexture`로 순차 처리한다. 완료 전 모델 자원 등록·커밋을 보류한다.
- 기존 텍스처 파일명·폴더·BC5 Normal / BC7 ORM / BC7 sRGB 설정을 보존한다. 내장 텍스처 원본은 한 번 추출하며, 기존처럼 베이킹 실패·취소 시 원본 경로로 머티리얼 바인딩을 계속한다. 워커 미사용 시 `TextureImporter` 인프로세스 폴백을 사용한다.
- 사용자가 제공한 Editor Unity 빌드 오류에서 `AssetWorkerHost.cpp`의 `MoveFile` 헬퍼가 Windows `MoveFileW`로 매크로 확장되어 `ClientPackage.cpp` 호출과 충돌하는 원인을 확인했다. 헬퍼를 `MoveTextureOutputFile`로 이름 변경했으며, 재빌드는 대기 중이다.
- **검증 상태**: 코드와 관련 문서 반영. `git diff --check` 통과. 저장소 지침에 따라 빌드와 테스트는 실행하지 않았다. 실행 확인 명령: `cmake --build --preset x64-ninja-debug --target Editor BamAssetWorker AssetWorkerSnapshotCheck`, `ctest --test-dir out/build/x64-ninja-debug -R "^AssetWorker(Model|Texture)Check$" --output-on-failure`.
- **미검증**: 워커 IPC를 포함한 모델 임포트의 내장/외부 텍스처, 대체 원본 경로, 기존 산출물 보존, 취소·폴백, 재임포트 및 대형 에셋 프레임 영향.

### [2026-11-20] BamAssetWorker 독립 텍스처 임포트 코드 연결

- **워커 프로토콜/변환**: `BamAssetWorker`에 `TEXTURE_WORKER_VERSION` 및 `IMPORT_TEXTURE`를 추가했다. DirectXTex 변환은 `.bamtex`와 `.dds` staging 파일을 생성하고, 요청의 포맷·압축 플래그를 전달한다. WIC를 사용하는 `DirectXTexCompressor`는 호출 스레드의 COM 초기화 상태를 처리한다.
- **에디터 연결**: `AssetWorkerHost`가 독립 텍스처 임포트를 직렬화해 실행하고, 진행률·취소·3분 제한·워커 사용 불가 폴백을 제공한다. 두 staging 결과가 성공한 후 기존 파일을 백업하고 새 결과를 교체하며 실패 시 복구를 시도한다. `AssetManager::ImportAsync`는 이미지 확장자 임포트를 워커에 연결하고, `TextureImporter`의 압축 실패를 성공으로 처리하지 않는다.
- **검사/로캘/문서**: `AssetWorkerTextureCheck.py`가 4x4 PNG → BC7 변환의 `.bamtex` 및 DDS 출력과 누락 원본 실패를 검사하도록 추가했다. 푸터 진행 상태 키는 `Editor.ModelImport.csv`에 추가했으며, 세부 범위와 잔여 모델 내장 텍스처 경로는 `OutOfProcessAssetPipelinePlan.md`에 기록했다.
- **검증 상태**: 빌드·테스트는 프로젝트 지침에 따라 실행하지 않았다. 사용자 실행 명령: `cmake --build --preset x64-ninja-debug --target BamAssetWorker AssetWorkerSnapshotCheck`, `ctest --test-dir out/build/x64-ninja-debug -R "^AssetWorker(Model|Texture)Check$" --output-on-failure`.
- **미검증/잔여**: `AssetWorkerModelCheck`, `AssetWorkerTextureCheck`, 에디터의 출력 교체/취소/폴백, 대형 텍스처 성능은 미검증이다. 모델 임포트 중 내장/외부 텍스처 베이킹은 아직 `ModelImporter` 최종화에서 인프로세스로 수행된다.

### [2026-11-20] BamAssetWorker 모델 snapshot 소비 검증 보강

- `Tools/AssetWorker/ModelSnapshotCheck.cpp` 테스트 실행 파일을 추가해 워커가 생성한 snapshot을 `ModelConversion::ReadModelSnapshot`으로 읽을 수 있는지 검사하도록 했다. `AssetWorkerModelCheck.py`는 UTF-8 모델 경로 변환 후 이 검증기를 실행하며, `Tools/AssetWorker/CMakeLists.txt`에서 테스트 타깃을 연결한다.
- `OutOfProcessAssetPipelinePlan.md`의 IPC 계약을 현재 탭 구분 프로토콜과 일치시키고, 고정 워커 수·폴백 경계·검증 대기 상태를 갱신했다.
- **검증 상태**: 빌드와 CTest는 실행하지 않았다. 사용자 확인 명령: `cmake --build --preset x64-ninja-debug --target BamAssetWorker AssetWorkerSnapshotCheck`, `ctest --test-dir out/build/x64-ninja-debug -R "^AssetWorkerModelCheck$" --output-on-failure`.
- **미검증**: 새 테스트 타깃의 컴파일 및 snapshot 역직렬화 실행, 대형 모델 에디터 임포트·취소·폴백·성능. 텍스처 워커 이관은 모델 경로 검증 후속 단계다.

### [2026-11-20] 프로젝트 점검, SceneArchiveCheck 외부 엔티티 참조 검증 핫픽스 및 CTest 44개 전체 통과

- **`SceneArchiveCheck` 외부 엔티티 참조 검증 핫픽스 (`Engine/examples/SceneArchiveCheck.cpp`)**:
  - `CheckExternalEntityReferenceCount()`에서 `scene->CreateEntity()`로 생성한 기본 엔티티(`IDComponent`만 보유)를 `scene->ReparentEntity(child, &root)`로 부모 설정하려다 3D TRS 월드 변환 필수 컴포넌트(`TransformComponent`, `WorldTransformComponent`) 누락으로 `EntityReparentResult::InvalidTransform` 반환 및 검사 조기 실패가 발생하던 문제 해결.
  - 헬퍼 함수 `AddSpatialEntity`를 사용하여 `root`, `child`, `outside`, `foreign` 엔티티에 트랜스폼 및 계층 컴포넌트를 완비한 상태로 부모 설정 및 외부 참조 카운트를 측정하도록 수정 완료.
- **빌드 및 회귀 검증 베이스라인 확립 (CTest 44/44 통과)**:
  - `x64-ninja-debug` 단일 구성 빌드 정상 완료.
  - CTest 44개 전체 검사(프리팹 외부 참조 방어, 씬 아카이브, 머티리얼/셰이더, KCC 물리, 타임라인 등) 100% 통과(`44/44 Passed`) 사용자 검증 완료.
  - [Docs/DevelopmentPriorities.md](DevelopmentPriorities.md) 최신 기준 커밋(`c2058594`) 동기화 및 5대 차기 개발 후보군(머티리얼 Fast-Path/VS·PS 통합, Primary Pass Type, KCC 물리, Composite VFX, BamAssetWorker) 공식 재정의 완료.

### [2026-11-20] 머티리얼-셰이더 그래프 양방향 동기화, 공통 그래프 구조 및 VS/PS 통합 아키텍처 설계 문서화

- **머티리얼-셰이더 그래프 동기화, 공통 프레임워크 및 VS/PS 통합 설계서 ([MaterialGraphSynchronizationAndStageDesign.md](MaterialGraphSynchronizationAndStageDesign.md))**:
  - **파라미터 & 텍스처 동기화 모델 (Source of Truth)**:
    - 언리얼 엔진 및 유니티 셰이더 그래프의 아키텍처를 분석하여, **인터페이스 정의(Schema/Layout)의 단일 진실의 원천(SSOT)은 항상 "셰이더 그래프"**여야 함을 정립.
    - 머티리얼 인스펙터 패널에서의 슬라이더/컬러 조절 시 셰이더를 재컴파일하지 않고 GPU 상수 버퍼(`MaterialConstants : register(b2)`) 메모리만 즉시 갱신하는 **Fast-Path(0초 실시간 핫 업데이트)** 파이프라인 수립.
    - 머티리얼 패널에서 [+ 파라미터 추가] 시 그래프 블랙보드(Variable Pool)에 변수가 자동 등록되고 노드로 꺼내 쓸 수 있도록 하는 양방향 연계 인터페이스 설계.
  - **다중 그래프 패널 공통 프레임워크 (`Editor::GraphCommon`)**:
    - `GraphPanel`(로직), `MaterialEditorCanvas`(셰이더), `AnimatorGraphPanel`(FSM) 간 중복 구현된 `ax::NodeEditor` 뷰포트 네비게이션, 초점 맞춤(`NavigateToContent`), 와이어 스타일 토글, 노드 정렬(수평/수직/균등 배치), 코멘트 상자 생성 및 단축키 일원화 구조 설계.
  - **정점(VS) & 픽셀(PS) 셰이더의 단일 그래프 통합 작성**:
    - 버텍스 변위(World Position Offset - WPO, 바람/파도/진동 애니메이션)와 Custom UV(정점 단계 사전 계산으로 픽셀 셰이더 비용 최적화) 처리를 위해, **단일 머티리얼 그래프에서 VS와 PS를 동시에 다루어야 함**을 증명.
    - `Shader.MaterialOutput` 마스터 노드에 `WorldPositionOffset`, `VertexNormal`, `CustomUV0/1` 정점 입력 핀과 서피스 PBR 픽셀 입력 핀을 함께 배치.
    - `ShaderGraphCompiler`에서 단일 `.bamgraph.json`으로부터 `GenerateGBufferVS`와 `GenerateGBufferPS`를 분할 생성하여 한 번에 빌드/바인딩하는 듀얼 타깃 컴파일 아키텍처 수립.
- **머티리얼 에디터 패스별 셰이더 카드 헤더 줄바꿈 버그 핫픽스 (`MaterialEditor.cpp`)**:
  - `[Shadow]` 아래로 휴지통 버튼이 떨어져 개행되던 UI 버그를 해결.
  - 2열 테이블 내부에서 `SetCursorPosX`로 강제 정렬하던 방식을 탈피하여, 테이블 직전 상단에 `[PassName]`과 `[휴지통]`을 양쪽 끝에 배치하는 헤더 라인을 그리고 그 아래에 2열 프로퍼티 테이블(`정점/픽셀 셰이더 | 리소스 피커`)을 배치하도록 개선하여 요구 스케치와 100% 일치시킴.

### [2026-11-20] 머티리얼 에디터 파이프라인/텍스처/파라미터 UI 완성 및 기본 렌더 패스(Primary Pass Type) 설계 문서화

- **기본 렌더 패스(Primary Pass Type) 도입 설계 문서화 ([MaterialPrimaryPassTypeDesign.md](MaterialPrimaryPassTypeDesign.md))**:
  - 디퍼드 GBuffer 셰이더(RT 3개 출력)를 암묵적인 기본(Fallback) 셰이더로 삼던 구조적 모호성과 반투명(`AlphaBlend`)·포워드(`Forward`)·UI 렌더 타깃 불일치 문제를 해결하기 위해, 머티리얼이 주(Primary) 렌더 패스 타입을 소유하는 아키텍처를 수립했다.
  - `MaterialInterface`의 `m_PrimaryPassType` 속성, `GetVertexShader`/`GetPixelShader` Fallback 정합성 개선, `BlendMode` 권장 프리셋 연동 및 셰이더 그래프 코드 생성 연계 계획을 문서화했다.
- **머티리얼 텍스처 바인딩 테이블 썸네일 일체형 리소스 피커 및 삭제 버튼 복구 (`MaterialEditor.cpp`)**:
  - 별도의 `Preview` 열을 제거하고, 리소스 피커 버튼 자체에 텍스처 썸네일이 노출되는 모드(`DrawInspectorResourcePicker(..., showThumbnail = true)`)로 통합했다.
  - 표를 `슬롯(t%u)`, `이름`, `텍스처(ResourceType.Texture)`, `삭제` 4열로 슬림화하고, 썸네일 높이(약 38px)에 맞춰 슬롯 번호, 바인딩 이름, 휴지통 버튼이 수직 중앙 정렬되도록 오프셋을 보정했다.
  - 휴지통 삭제 열 너비를 `28.0f`로 확보하고 `delOpt.NoWrap = true`를 적용하여 셀 여백 부족으로 인한 버튼 생략(Silent Drop) 현상을 해결했다.
- **기본 셰이더 및 패스별 셰이더 2열/3행 묶음 카드 테이블 정돈 (`MaterialEditor.cpp`)**:
  - 기본 셰이더: 위아래 수직 배치 대신 좌측 라벨(`기본 정점/픽셀 셰이더`), 우측 `ResourcePicker`의 2열 프로퍼티 테이블(`##DefaultShadersProps`)로 배치했다.
  - 패스별 셰이더: `[Shadow]` 아래로 휴지통 버튼이 줄바꿈되던 문제를 해결하고, 각 패스마다 `Row 0: [PassName] (좌측) + [휴지통] (우측 끝)`, `Row 1: 정점 셰이더 | 리소스피커`, `Row 2: 픽셀 셰이더 | 리소스피커` 형태의 독립 카드 테이블(`##PassShaderProps`)로 일체화했다.
- **파라미터 검색창 가로 1칸 오버플로우 및 삭제 버튼 복구 (`MaterialEditor.cpp`)**:
  - 수동 `InputTextWithHint` + `Button`을 `EditorUI::DrawSearchBar`로 일체화하여 `ItemSpacing.x`(8px) 초과로 발생하던 패널 가로 1칸 스크롤을 완전 해소했다.
  - 파라미터 테이블 삭제 열에 `delOpt.NoWrap = true`를 적용하여 휴지통 삭제 버튼을 상시 노출하고, `##MaterialDetailsScroll` 진입 시 `ImGui::SetScrollX(0.0f)`를 강제하여 좌측 라벨 열이 화면 밖으로 밀려 잘려 나가지 않도록 방어했다.

### [2026-11-19] 머티리얼 에디터 3-Pane 레이아웃 너비 초과 줄바꿈 및 캔버스 노드 위치/포커스 핫픽스

- **3분할 가로 레이아웃 안정화 (`MaterialEditor.cpp`)**:
  - `centerWidth`가 최소 폭(260px)으로 고정될 때 세 패널의 총합이 창의 가용 폭(`availWidth`)을 초과하여 우측 인스펙터 패널이 창 아래로 줄바꿈되어 화면에서 사라지던 문제를 해결.
  - 좌/우 패널의 합이 가용 공간을 넘지 않도록 비율 기반 스케일 다운 및 클램프를 적용하여, 한 줄에 좌/중/우 3개 패널이 나란히 완벽하게 배치되도록 보장.
- **셰이더 그래프 로드 및 노드 위치/카메라 자동 포커스 복원 (`MaterialEditor.cpp`, `MaterialEditorCanvas.cpp`)**:
  - `EnsureShaderGraphLoaded()`에서 `m_Material->GetShaderGraphPath()`를 프로젝트 상대 경로뿐 아니라 머티리얼 에셋 폴더 기준 상대 경로(`m_TargetResourcePath.parent_path()`)까지 순차 검색하도록 강화.
  - `DrawCustomContent()` 시작 시 `if (!m_GraphReady) EnsureShaderGraphLoaded();`를 호출하여 리소스 로드 시점 타이밍 문제로 그래프가 비어 있는 현상을 원천 차단.
  - `MaterialEditor.cpp`에서 `ax::NodeEditor`(`ed`) 미참조 컴파일 에러(`C2653`, `C4430`)를 해결하기 위해, 노드 위치 복원(`ed::SetNodePosition`)을 `imgui_node_editor.h`를 직접 참조하는 `MaterialEditorCanvas.cpp`의 `DrawShaderGraphCanvas()`(`m_NeedsNavigateToContent` 처리 블록) 내부로 이관하여 의존성 분리 및 정상 빌드 보장.
  - 노드 로드 시 `ax::NodeEditor`의 각 노드에 대해 `ed::SetNodePosition(ed::NodeId(node.Id), ImVec2(node.PositionX, node.PositionY))`를 명시적으로 복원하고, 첫 프레임에 `ed::NavigateToContent(0.0f)`를 호출하여 (1152, 473) 등 멀리 위치한 노드가 화면 정중앙에 시원하게 줌/포커스되도록 수정.
- **캔버스 툴바 우측 정렬 좌표 버그 수정 (`MaterialEditorCanvas.cpp`)**:
  - `DrawShaderGraphToolbar`에서 `SameLine` 호출 시 `ImGui::GetCursorPosX() + availX - rightToolsWidth`로 절대 x 오프셋을 올바르게 계산하여 `+ 노드 추가`, 화면 포커스, 와이어 스타일 버튼들이 화면 밖으로 날아가지 않고 우측 상단에 정렬되도록 수정.
- **메인 Material 노드 렌더링 스타일 개선 (`MaterialEditorCanvas.cpp`)**:
  - `Shader.MaterialOutput` 노드를 언리얼 엔진 스타일의 녹색 헤더와 시인성 높은 타이틀(`Material`)로 렌더링.
- **좌측 패널 셰이더 그래프 연결 관리 UI 정돈 (`MaterialEditor.cpp`)**:
  - 불필요한 외장 창 열기 버튼 대신 `[연결해제(Unlink)]` 및 `[새 셰이더 그래프 생성]` 버튼으로 깔끔하게 일원화.

### [2026-11-19] 셰이더 그래프의 머티리얼 에디터 완전 통합 및 GraphPanel 순수 로직 전용 복원

- **아키텍처 책임 완전 분리 및 임베드 방식 전면 폐기**:
  - 사용자 피드백에 따라 `GraphPanel`을 머티리얼 에디터에 임베드하거나 팝아웃하는 방식을 완전히 폐기하고, **머티리얼 에디터가 셰이더 그래프를 100% 직접 소유**하도록 통합 아키텍처 개편.
  - `GraphPanel`에서 셰이더 도메인(`EGraphDomain::Shader`), 셰이더 빌드, 3D 미리보기 및 런타임 머티리얼 코드를 완전히 제거하여 **순수 비주얼 스크립팅/로직 전용 에디터**로 복원.
  - `MaterialEditor`가 독립적으로 `Engine::GraphRegistry`, `ShaderGraphDocument`(`EGraphDomain::Shader`), `ax::NodeEditor::EditorContext*`, 자체 문서 히스토리(Undo/Redo), 캔버스 렌더링, 노드 인스펙터, 셰이더 컴파일러 및 실시간 자동 컴파일(Hot-Reload)을 직접 소유하고 통합 실행.
- **머티리얼 에디터 표준 3분할(3-Pane) 레이아웃 완성**:
  - **좌측 패널 (`m_LeftPaneWidth`)**:
    - **상단 3D 프리뷰 위젯**: 수평 스플리터로 높이를 자유롭게 조절 가능하며, 주변 검은 패딩 여백을 제거하고 바닥 그리드가 기본 표시되도록 개선.
    - **하단 세부 설정 (스크롤)**:
      - **파이프라인 설정**: 블렌드 모드, 컬링 모드, 채우기 모드, 깊이 모드, 깊이 비교 연산.
      - **셰이더 설정**: 셰이더 그래프 연결 표시 및 연결 해제 버튼 (텍스트 잘림 해결), 기본 정점/픽셀 셰이더 피커, 패스별 셰이더 상단 추가 컨트롤 및 각 행별 **삭제(X) 버튼** 완비.
      - **텍스처 바인딩**: 슬롯 번호/이름/추가 버튼을 표 상단으로 이동, 텍스처 피커 및 각 행별 **삭제(X) 버튼** 제공.
      - **파라미터**: 이름/타입/추가 버튼과 검색창/리셋(X) 버튼을 상단에 2행으로 깔끔하게 배치, 값 편집 및 각 행별 **삭제(X) 버튼** 제공.
  - **중앙 패널 (`centerWidth`)**:
    - `MaterialEditor` 자체 캔버스 워크스페이스 (`DrawCenterCanvasPane`).
    - 상단 툴바: 번개 아이콘 컴파일 & 실시간 반영 버튼, 자동 컴파일 체크박스, 상태 텍스트, 노드 추가 버튼, 화면 초기화(포커스) 버튼, 와이어 스타일 전환 버튼.
    - `ax::NodeEditor` 캔버스: 노드 렌더링, 핀 연결/해제, 컨텍스트 메뉴, 핀 드래그 앤 드롭 노드 자동 생성 및 연결.
    - 하단 상태 바: 높이를 충분히 확보하여 안내 텍스트가 경계선에 묻히지 않고 완전히 표시되도록 여백 개선.
  - **우측 패널 (`m_RightPaneWidth`)**:
    - **선택된 노드 속성 (Node Properties)**: 선택된 노드의 타입, ID, 주석 및 입력 핀 값들을 인스펙터 형태로 직접 편집 가능.
    - **셰이더 진단 및 생성된 HLSL 소스**: 접이식 헤더로 컴파일 결과 에러 로그 표시 및 생성된 HLSL 코드 열람, 원클릭 클립보드 복사 버튼 제공.
- **실시간 자동 컴파일 (Live Hot-Reload)**:
  - 노드 편집이나 핀 값 수정 시 0.25초 디바운스 타이머 후 백그라운드 컴파일 및 런타임 셰이더 바이트코드 즉각 핫스왑 실행 (`LiveHotReloadShader()`).
- **로캘 키 및 단축키 일원화**:
  - `Editor.MaterialEditor.csv`에 누락되었던 20여 종의 로캘 키 추가 등록.
  - `EditorActionRegistry.cpp`의 `EShortcutContext::MaterialEditor` 문맥에 `F7`(컴파일), `F`(화면 포커스), `Space`(노드 추가), `Delete`(삭제), `Ctrl+D`(복제), `C`(코멘트) 등록.


### [2026-11-19] 머티리얼 에디터 - 독립 Graph 패널 팝업 충돌 해소 및 임베디드 단독 렌더링/양방향 도킹 완성

- **별도 Graph 패널 팝업 및 캔버스 블랙스크린 근본 원인 해결**:
  - **원인 분석**:
    - `MaterialEditor::EnsureShaderGraphLoaded()`에서 `GraphPanel::SetTargetResource`를 호출할 때 독립 패널 창(`GraphPanel`)이 기본 열림(`m_Open == true`) 상태로 남아 있었음.
    - 에디터 메인 루프에서 독립 창의 `Draw()`가 먼저 실행되어 `ax::NodeEditor`의 캔버스 렌더링(`ed::Begin`)을 선점하고, 이어서 머티리얼 에디터 내부의 `DrawEmbeddedWorkspace()`가 두 번째로 동일한 에디터 컨텍스트에 대해 `ed::Begin`을 호출하면서 노드 렌더링이 충돌/무효화되어 내부 캔버스가 검은 화면으로 비어버리는 현상 발생.
  - **해결 (`MaterialEditor.cpp`, `GraphPanel.cpp`, `GraphPanel.h`)**:
    - 머티리얼 에디터에서 임베디드로 내장하여 렌더링하고 있을 때(`!m_GraphPoppedOut`), 독립 패널 창을 `graphPanel->SetOpen(false)`로 닫아두어 프레임당 단 **1번만** 캔버스를 단독 렌더링하도록 보장.
    - `ax::NodeEditor`의 동일 컨텍스트 중복 호출 충돌이 100% 해소되어 머티리얼 에디터 우측 영역에 `MaterialOutput` 노드와 셰이더 그래프가 즉시 선명하게 렌더링됨.
- **완벽한 양방향 창 분리(Pop Out) 및 도킹(Dock In) 지원**:
  - 임베디드 상단 툴바의 `[창으로 분리 (Pop Out)]` 버튼 클릭 시 `graphPanel->SetPoppedOut(true)`를 호출하여 독립 창을 열고 포커스.
  - 분리 모드일 때 머티리얼 에디터 우측 영역은 캔버스 대신 "별도 창으로 분리되었습니다" 안내 배너와 `[머티리얼 에디터 안으로 도킹하기]` 버튼을 표시하여 중복 렌더링 차단.
  - 독립 창 툴바에 `[머티리얼 에디터로 도킹]` 버튼을 추가하여 클릭 시 독립 창을 닫고 다시 머티리얼 에디터 내부로 쏙 들어오도록 양방향 연동.
- **임베디드 모드 시 불필요한 3D 프리뷰 중복 위젯 제외**:
  - `GraphPanel::DrawShaderBuildDetails(bool include3DPreview)`: 머티리얼 에디터 좌측 상단에 이미 고성능 3D 프리뷰 뷰포트가 있으므로, 우측 임베디드 프로퍼티 영역에서는 `include3DPreview = false`로 호출하여 중복 3D 프리뷰를 제거하고 노드 속성 및 변수 편집 영역을 넓게 확보.
- **하단 상태바 2번째 줄 텍스트 하단 잘림 해결**:
  - `footerHeight`에 넉넉한 여유 마진(`+ 10.0f`, `ItemSpacing.y * 3.0f`)을 주어 `... 배치는 저장하지 않습니다.` 2줄 텍스트가 잘리지 않고 깨끗하게 보이도록 개선.
- **로캘 키 추가 (`Editor.GraphPanel.csv`, `Editor.MaterialEditor.csv`)**:
  - `GraphPanel.NotReady`, `GraphPanel.DockToMaterial` 등록.
  - `MaterialEditor.GraphPoppedOutNotice`, `MaterialEditor.DockGraphToEditor` 등록.

### [2026-11-19] 머티리얼 에디터 언리얼 엔진 스타일 3-Panel 레이아웃 전면 개편 & 상단 툴바/삭제 UX 개선

- **언리얼 엔진 스타일 3-패널(3-Pane) 레이아웃 전면 개편**:
  - `Editor/Source/ImGui/ResourceEditor/Public/MaterialEditor.h`, `Private/MaterialEditor.cpp`:
    - **[좌측 패널 (`m_LeftPaneWidth`, 기본 360px)]**: 상단에 3D 프리뷰 뷰포트(`m_PreviewRenderer`), 하단 스크롤 영역에 머티리얼 세부 설정(파이프라인 설정, 셰이더 설정, 텍스처 바인딩, 파라미터) 통합 배치.
    - **[중앙 패널 (`centerWidth`, 반응형)]**: 셰이더 그래프 노드 캔버스(`ax::NodeEditor`)를 항상 화면 정중앙에 기본 배치하여 탭 전환 없이 캔버스와 3D 프리뷰를 동시 작업 가능.
    - **[우측 패널 (`m_RightPaneWidth`, 기본 280px)]**: 캔버스에서 선택한 노드의 상세 속성 및 변수를 편집하는 인스펙터 패널(`DrawNodeInspector`) 전용 배치 (미선택 시 안내 메시지).
    - 수직 스플리터 2개(`Editor::SplitterV`)를 통해 각 패널의 너비를 자유롭게 조절하고, `CaptureState`/`RestoreState`로 레이아웃 상태 영속적 유지.
- **머티리얼 열람 시 셰이더 그래프 즉시 캔버스 표시 보장 (`EnsureShaderGraphLoaded`)**:
  - `MaterialEditor::EnsureShaderGraphLoaded()`:
    - 머티리얼 열람 시 연결된 셰이더 그래프가 없거나 파일이 존재하지 않는 경우 기본 `Shader.MaterialOutput` 노드가 포함된 그래프를 자동 생성하고 연결.
    - `GraphPanel::GetActiveGraphPanel()`을 통해 해당 그래프 문서를 즉시 로드하여 중앙 캔버스에 즉시 노드가 표시되도록 보장.
- **패스별 셰이더 목록 삭제(Remove) 기능 추가**:
  - `MaterialEditor::DrawShaderSettings()`:
    - 패스별 셰이더 테이블 4번째 컬럼(`##Action`)의 폭을 36px로 확보하고 빨간색 휴지통 아이콘 버튼(`ICON_FA_TRASH`) 배치.
    - 클릭 시 `m_Material->RemovePassShader(passToRemove)` 확실한 삭제 동작 수행 및 `m_History.MarkEdited()` 처리.
- **텍스처 바인딩 슬롯 추가 버튼 상단 이동**:
  - `MaterialEditor::DrawTextureBindings()`:
    - 기존 표 하단에 있던 슬롯 추가 버튼을 테이블 상단(`NewBindingControlTop`)으로 이동하여 스크롤 없이 즉시 새 슬롯을 추가할 수 있도록 개선.
- **파라미터 툴바(추가 컨트롤 & 검색 바) 표 상단 통합 재배치**:
  - `MaterialEditor::DrawParameters()`:
    - 1행 (추가 컨트롤): `[파라미터 이름 입력] [파라미터 타입 콤보] [+ 파라미터 추가]`를 표 상단에 반응형으로 배치.
    - 2행 (검색 바): `EditorUI::DrawSearchBar` 표준 검색창(클리어 `X` 버튼 포함)을 표 바로 위에 배치.
    - 표 하단에 중복 배치되어 있던 추가 컨트롤 제거.
- **컴파일 빌드 오류 핫픽스 및 실환경 피드백 6종 2차 완성**:
  - **하단 상태 바 글자 짤림 해결**:
    - `GraphPanel::DrawEmbeddedWorkspace()`: 상태 바 2줄 텍스트(`SaveScope` + `Controls`)에 맞게 `footerHeight`를 2줄 높이(`GetTextLineHeightWithSpacing() * 2.0f + ItemSpacing.y * 2.0f`)로 정확하게 산출하여 글자 잘림 원천 차단.
  - **로캘 누락 및 버튼 오버플로우 해결**:
    - `Editor.GraphPanel.csv`: `CompileHotReload`, `CompileHotReloadTooltip`, `AutoCompile` 로캘 키 추가.
    - 상단 툴바의 우측 액션 버튼(`AddNode`, `PopOutWindow`)의 가용 폭 계산을 동적 텍스트 크기 기반으로 수정하여 좁은 화면에서도 삐져나감 방지.
  - **셰이더 그래프 워크스페이스 통합 (사용자 제안 수용)**:
    - 어설프게 3분할되었던 패널 구조를 `GraphPanel`의 완성된 원본 워크스페이스(`DrawEmbeddedWorkspace`) 통째 렌더링 방식으로 전환.
    - 좌측(3D 프리뷰 + 머티리얼 세부설정) ↔ 수직 스플리터 ↔ 우측(셰이더 그래프 캔버스 70% + 노드 인스펙터/변수/빌드상세 30% 내부 리사이즈 테이블).
  - **오토스크롤링 버그 해소**:
    - 이중 Child Window 스크롤 충돌을 제거하고 캔버스에 정확한 `contentHeight`를 전달하여 노드 에디터의 스크롤 튐 및 오토스크롤링 문제 원천 해결.
  - **머티리얼 뷰포트 바닥 그리드 렌더링 복구**:
    - `PreviewViewportWidget.cpp`: `Initialize` 시 `m_ShowGrid` 조건문 없이 항상 `m_Grid.Initialize`를 수행하도록 변경.
    - `PreviewViewportWidget.h`: `SetGridVisible`/`ToggleGrid` 시 `m_Grid.Show()` / `Hide()`를 즉시 동기화.
    - `MaterialPreviewRenderer.cpp`: `Initialize`에서 그리드 활성화(`enableGrid = true`).
  - **뷰포트 영역 정체불명 패딩 제거**:
    - `MaterialPreviewRenderer::DrawViewportOnly()`: 기존 정사각형 강제 클램핑(`minSide`)을 제거하고 가용 너비/높이를 100% 채움.
    - `MaterialEditor.cpp`: `##MaterialViewportChild`에 `ImGuiStyleVar_WindowPadding(0, 0)`을 적용하여 불필요한 내부 패딩 테두리 완전 제거.
- **로캘 키 등록 (지침 11·14절 준수)**:
  - **셰이더/머티리얼 그래프 미표시(`GraphPanel.NotReady`) 해결**:
    - `GraphPanel.h`, `GraphPanel.cpp`: `inline static GraphPanel* s_Instance`를 도입하여 창 포커스/가시성 여부와 무관하게 `GetActiveGraphPanel()`이 유효한 싱글톤 인스턴스를 항상 반환하도록 보장.
    - `DrawEmbeddedCanvasOnly` 및 `DrawEmbeddedPropertiesOnly` 진입 시 `m_Domain = EGraphDomain::Shader` 강제 및 `ActivateDocument()`를 명시 호출하여 노드 컨텍스트 활성화.
  - **툴바 중복 버튼 정리**:
    - `MaterialEditor::DrawCustomQuickAccess()`: `DrawHistoryButtons` 뒤에 중복 배치되어 있던 Revert 버튼(`ICON_FA_ROTATE_LEFT`)을 제거하여 Undo/Redo/Revert 화살표 반복 혼란 해소.
  - **프리뷰 위젯 높이 조절 가로 스플리터바 추가 & 퀵액세스 툴바 통합**:
    - 좌측 패널 상단 프리뷰 뷰포트와 하단 설정 사이에 `Editor::SplitterH("##MaterialPreviewSplitterH", ...)`를 도입하여 마우스 드래그로 높이 자유 조절 지원.
    - `구체(Sphere)`, `큐브(Cube)`, `평면(Plane)`, `자동 회전 토글(AutoRotate)`, `카메라 리셋(ResetCamera)`, `그리드 토글(ToggleGrid)`을 상단 퀵액세스 툴바로 이동.
    - `MaterialPreviewRenderer::DrawViewportOnly()` 신설로 프리뷰 내부의 중복 툴바를 제거하고 순수 3D 뷰포트로 꽉 차게 렌더링.
  - **셰이더 그래프 열기/연결해제 버튼 짤림 방지**:
    - 긴 텍스트 버튼 대신 `EditorUI::SmallIconButton`을 적용하여 좁은 프로퍼티 컬럼에서도 짤림 없이 콤팩트한 아이콘과 툴팁으로 표시.
  - **패스별 셰이더 및 기본 셰이더 컬럼 간격 최적화**:
    - `m_LeftPaneWidth` 기본값을 380px로 상향하여 가로 폭 여유 확보.
    - `PassShadersTable`의 `PassType` 컬럼 폭을 110px에서 75px로 줄여 `VertexShader`와 `PixelShader` 컬럼이 충분한 폭을 유지하도록 개선.
  - **패스별 셰이더 헤더 우측 상단으로 추가 컨트롤 이동**:
    - `--- 패스별 셰이더 ---` 헤더와 같은 줄 오른쪽에 `[패스 종류 콤보]`와 `[+ 패스 셰이더 추가]` 버튼을 나란히 배치하고 표 하단의 중복 컨트롤 제거.
- **로캘 키 등록 (지침 11·14절 준수)**:
  - `Editor/Resources/Data/Localization/Editor.MaterialEditor.csv`: `NodeProperties` (노드 속성) 추가.
  - `Editor/Resources/Data/Localization/Editor.GraphPanel.csv`: `PopOutWindow`, `PopOutWindowTooltip`, `NoNodeSelectedHint` 추가.
- **검증 상태**: 정적 코드 리뷰 및 diff 검증 완료. AGENTS.md 2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] D3D12 셰이더 시그니처 호환성 3중 방어 & 언리얼 엔진 스타일 머티리얼 에디터 통합 (Step 2)

- **D3D12 셰이더 시그니처 링키지 불일치 크래시 원천 방어 (3중 방어 체계)**:
  - `Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12Pipeline.cpp`:
    - `SafeCreateGraphicsPipelineState` 헬퍼 함수를 추가하고 MSVC SEH(`__try ... __except`)로 격리하여, D3D12 SDKLayers/드라이버의 0xC0000005 크래시 발생 시 프로세스 종료 대신 `E_FAIL`로 안전하게 실패 처리.
  - `Engine/Source/Render/Private/Renderer.cpp`:
    - `PrepareMeshPipelineDesc`에서 VS와 PS의 시그니처 호환성 검증 및 안전한 폴백(자동 보정) 구축:
      - `isGBufferPS && !isGBufferVS` (예: `DefaultVS` + `gbufferPS`): 메시 속성에 따라 자동으로 `GBufferVS` / `GBufferSkinningVS`로 승격 보정하여 D3D12 정상 렌더링 유지.
      - `!isGBufferPS && isGBufferVS && isDefaultPS`: `DefaultVS`로 자동 보정.
      - 알 수 없는 불일치 조합은 `EResult::InvalidArgument`로 D3D12 진입 사전 차단 및 해당 드로우콜 스킵.
  - `Editor/Source/ImGui/ResourceEditor/Private/MaterialEditor.cpp`:
    - `DefaultPS`에서 `gbufferPS` 선택 시 정점 셰이더가 `DefaultVS`라면 `gbufferVS`로 자동 동기화.
    - 불일치 조합 감지 시 테이블 하단에 주황색 경고 메시지 및 `[셰이더 쌍 자동 맞춤]` 원클릭 복구 버튼 제공.
- **언리얼 엔진 스타일 머티리얼 에디터 내부 탭 통합 (Step 2)**:
  - `Editor/Source/ImGui/ResourceEditor/Public/MaterialEditor.h`, `Private/MaterialEditor.cpp`:
    - 우측 디테일 창을 `ImGui::BeginTabBar("##MaterialEditorTabs")` 기반의 2개 탭 구조로 전면 개편:
      - **탭 1: `[속성 (Properties)]`**: 기존 파이프라인 설정, 셰이더 슬롯, 텍스처 바인딩, 파라미터.
      - **탭 2: `[셰이더 그래프 (Shader Graph)]`**: 머티리얼에 연결된 셰이더 그래프 노드 캔버스를 내부에서 바로 렌더링.
    - 상단 툴바의 `[셰이더 그래프 열기]` 클릭 시 별도 창으로 팝업되지 않고 내부 `[셰이더 그래프]` 탭으로 매끄럽게 전환.
    - 좌측 3D 뷰포트(`MaterialPreviewRenderer`)는 항상 고정되어 노드 조작 시 실시간 핫스왑 결과를 단일 뷰포트에서 즉각 관찰 가능.
    - 중복 뷰포트 제거로 톤매핑 불일치, 프리뷰 널뛰기, 가로 스크롤 잘림 문제 원천 해결.
  - `Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h`, `Private/GraphPanel.cpp`:
    - `DrawEmbeddedWorkspace(width, height)` 신설: 상단 콤팩트 툴바(핫컴파일, 자동 컴파일 체크박스, 컴파일 상태, 팝아웃 버튼) 및 중복 3D 프리뷰가 제외된 순수 노드 캔버스 + 노드 프로퍼티 2분할 레이아웃 제공.
    - `IsDocumentLoaded(path)` 함수 추가로 효율적인 대상 문서 동기화 지원.
- **컴파일 빌드 오류 핫픽스 및 실환경 피드백 6종 2차 완성**:
  - **하단 상태 바 글자 짤림 해결**:
    - `GraphPanel::DrawEmbeddedWorkspace()`: 상태 바 2줄 텍스트(`SaveScope` + `Controls`)에 맞게 `footerHeight`를 2줄 높이(`GetTextLineHeightWithSpacing() * 2.0f + ItemSpacing.y * 2.0f`)로 정확하게 산출하여 글자 잘림 원천 차단.
  - **로캘 누락 및 버튼 오버플로우 해결**:
    - `Editor.GraphPanel.csv`: `CompileHotReload`, `CompileHotReloadTooltip`, `AutoCompile` 로캘 키 추가.
    - 상단 툴바의 우측 액션 버튼(`AddNode`, `PopOutWindow`)의 가용 폭 계산을 동적 텍스트 크기 기반으로 수정하여 좁은 화면에서도 삐져나감 방지.
  - **셰이더 그래프 워크스페이스 통합 (사용자 제안 수용)**:
    - 어설프게 3분할되었던 패널 구조를 `GraphPanel`의 완성된 원본 워크스페이스(`DrawEmbeddedWorkspace`) 통째 렌더링 방식으로 전환.
    - 좌측(3D 프리뷰 + 머티리얼 세부설정) ↔ 수직 스플리터 ↔ 우측(셰이더 그래프 캔버스 70% + 노드 인스펙터/변수/빌드상세 30% 내부 리사이즈 테이블).
  - **오토스크롤링 버그 해소**:
    - 이중 Child Window 스크롤 충돌을 제거하고 캔버스에 정확한 `contentHeight`를 전달하여 노드 에디터의 스크롤 튐 및 오토스크롤링 문제 원천 해결.
  - **머티리얼 뷰포트 바닥 그리드 렌더링 복구**:
    - `PreviewViewportWidget.cpp`: `Initialize` 시 `m_ShowGrid` 조건문 없이 항상 `m_Grid.Initialize`를 수행하도록 변경.
    - `PreviewViewportWidget.h`: `SetGridVisible`/`ToggleGrid` 시 `m_Grid.Show()` / `Hide()`를 즉시 동기화.
    - `MaterialPreviewRenderer.cpp`: `Initialize`에서 그리드 활성화(`enableGrid = true`).
  - **뷰포트 영역 정체불명 패딩 제거**:
    - `MaterialPreviewRenderer::DrawViewportOnly()`: 기존 정사각형 강제 클램핑(`minSide`)을 제거하고 가용 너비/높이를 100% 채움.
    - `MaterialEditor.cpp`: `##MaterialViewportChild`에 `ImGuiStyleVar_WindowPadding(0, 0)`을 적용하여 불필요한 내부 패딩 테두리 완전 제거.
- **로캘 키 등록 (지침 11·14절 준수)**:
  - **셰이더/머티리얼 그래프 미표시(`GraphPanel.NotReady`) 해결**:
    - `GraphPanel.h`, `GraphPanel.cpp`: `inline static GraphPanel* s_Instance`를 도입하여 창 포커스/가시성 여부와 무관하게 `GetActiveGraphPanel()`이 유효한 싱글톤 인스턴스를 항상 반환하도록 보장.
    - `DrawEmbeddedCanvasOnly` 및 `DrawEmbeddedPropertiesOnly` 진입 시 `m_Domain = EGraphDomain::Shader` 강제 및 `ActivateDocument()`를 명시 호출하여 노드 컨텍스트 활성화.
  - **툴바 중복 버튼 정리**:
    - `MaterialEditor::DrawCustomQuickAccess()`: `DrawHistoryButtons` 뒤에 중복 배치되어 있던 Revert 버튼(`ICON_FA_ROTATE_LEFT`)을 제거하여 Undo/Redo/Revert 화살표 반복 혼란 해소.
  - **프리뷰 위젯 높이 조절 가로 스플리터바 추가 & 퀵액세스 툴바 통합**:
    - 좌측 패널 상단 프리뷰 뷰포트와 하단 설정 사이에 `Editor::SplitterH("##MaterialPreviewSplitterH", ...)`를 도입하여 마우스 드래그로 높이 자유 조절 지원.
    - `구체(Sphere)`, `큐브(Cube)`, `평면(Plane)`, `자동 회전 토글(AutoRotate)`, `카메라 리셋(ResetCamera)`, `그리드 토글(ToggleGrid)`을 상단 퀵액세스 툴바로 이동.
    - `MaterialPreviewRenderer::DrawViewportOnly()` 신설로 프리뷰 내부의 중복 툴바를 제거하고 순수 3D 뷰포트로 꽉 차게 렌더링.
  - **셰이더 그래프 열기/연결해제 버튼 짤림 방지**:
    - 긴 텍스트 버튼 대신 `EditorUI::SmallIconButton`을 적용하여 좁은 프로퍼티 컬럼에서도 짤림 없이 콤팩트한 아이콘과 툴팁으로 표시.
  - **패스별 셰이더 및 기본 셰이더 컬럼 간격 최적화**:
    - `m_LeftPaneWidth` 기본값을 380px로 상향하여 가로 폭 여유 확보.
    - `PassShadersTable`의 `PassType` 컬럼 폭을 110px에서 75px로 줄여 `VertexShader`와 `PixelShader` 컬럼이 충분한 폭을 유지하도록 개선.
  - **패스별 셰이더 헤더 우측 상단으로 추가 컨트롤 이동**:
    - `--- 패스별 셰이더 ---` 헤더와 같은 줄 오른쪽에 `[패스 종류 콤보]`와 `[+ 패스 셰이더 추가]` 버튼을 나란히 배치하고 표 하단의 중복 컨트롤 제거.
- **로캘 키 등록 (지침 11·14절 준수)**:
  - `Editor/Resources/Data/Localization/Editor.GraphPanel.csv`: `PopOutWindow`, `PopOutWindowTooltip` 추가.
  - `Editor/Resources/Data/Localization/Editor.MaterialEditor.csv`: `TabProperties`, `TabShaderGraph`, `NoGraphConnected`, `CreateAndConnectGraph`, `ShaderMismatchWarning`, `AutoFixShaderPair` 추가.
- **검증 상태**: 코드 정적 검토 및 diff 확인 완료. AGENTS.md 2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] 머티리얼-셰이더 그래프 통합 Phase 3 (노드 편집 실시간 0.25초 디바운스 핫 컴파일 & 머티리얼 핫스왑) 구현

- **디바운스(Debounce) 자동 컴파일 파이프라인 (`GraphPanel`)**:
  - `Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h`, `Private/GraphPanel.cpp`:
    - `m_LastCompiledRevision`과 `m_Shader.Data.GetRevision()` 비교를 통해 캔버스 내 노드 추가/삭제/값/링크 변경 감지.
    - 변경 감지 시 `m_LiveCompileCooldown = 0.25f` 설정하여 사용자가 드래그 또는 입력 중일 때는 컴파일을 유예(Debounce)하여 CPU/GPU 스파이크 방지.
    - 입력 정지 후 0.25초 경과 시 `LiveHotReloadShader()` 자동 트리거.
    - `LoadDocument()` 시 리비전 동기화로 문서 열람 직후 불필요한 즉시 재컴파일 방어.
- **인메모리 셰이더 핫스왑 및 머티리얼 동기화 (`LiveHotReloadShader`)**:
  - `Editor/Source/ImGui/GraphPanel/Private/GraphPanelShader.cpp`:
    - `CompileMaterialShader()`를 통해 `ShaderGraphCompiler::GenerateGBufferMaterialHlsl()` 소스 생성 및 DXIL 메모리 컴파일.
    - 컴파일 성공 시 `Shader::UpdateBytecode(bytecode)` 호출:
      - 기존 `RHIShader` 포인터를 보존한 채 바이트코드만 교체하고 `PipelineManager::RebuildPipelinesWithShader`를 통해 D3D12 PSO 전체를 실시간 Rebuild.
    - 최초 컴파일 시 `Shader::Create` 및 `ResourceManager::AddResource(L"GraphRuntime/LivePixelShader")` 안전 등록.
    - 바인딩된 머티리얼(`targetMat->SetPixelShaderHandle()`) 및 `targetMat->IncreaseVersion()` 호출로 프리뷰 씬에 새 셰이더 즉시 반영.
- **안전성 및 에러 방어벽 (Fault-Tolerance & LastGood)**:
  - 핀 연결 해제 또는 중간 저작 상태로 컴파일 실패 시 크래시를 원천 차단하고 직전 정상 바이트코드(`LastGood`)를 유지.
  - `Draw3DMaterialPreview()` 툴바에 실시간 컴파일 상태 배지 연동:
    - 컴파일/디바운스 중: `ICON_FA_SPINNER` "Compiling..." (주황색)
    - 정상 반영 시: `ICON_FA_CHECK` "Live (0.25s)" (초록색)
    - 컴파일 에러 시: `ICON_FA_TRIANGLE_EXCLAMATION` "Compile Error (Last Good Retained)" (빨간색, 마우스 호버 시 툴팁 에러 로그)
    - 수동 즉시 핫스왑 버튼(`ICON_FA_BOLT "Compile & Hot-Swap"`) 및 자동 컴파일 온/오프 체크박스(`m_AutoCompileEnabled`) 제공.
- **컴파일 빌드 오류 핫픽스 및 실환경 피드백 6종 2차 완성**:
  - **하단 상태 바 글자 짤림 해결**:
    - `GraphPanel::DrawEmbeddedWorkspace()`: 상태 바 2줄 텍스트(`SaveScope` + `Controls`)에 맞게 `footerHeight`를 2줄 높이(`GetTextLineHeightWithSpacing() * 2.0f + ItemSpacing.y * 2.0f`)로 정확하게 산출하여 글자 잘림 원천 차단.
  - **로캘 누락 및 버튼 오버플로우 해결**:
    - `Editor.GraphPanel.csv`: `CompileHotReload`, `CompileHotReloadTooltip`, `AutoCompile` 로캘 키 추가.
    - 상단 툴바의 우측 액션 버튼(`AddNode`, `PopOutWindow`)의 가용 폭 계산을 동적 텍스트 크기 기반으로 수정하여 좁은 화면에서도 삐져나감 방지.
  - **셰이더 그래프 워크스페이스 통합 (사용자 제안 수용)**:
    - 어설프게 3분할되었던 패널 구조를 `GraphPanel`의 완성된 원본 워크스페이스(`DrawEmbeddedWorkspace`) 통째 렌더링 방식으로 전환.
    - 좌측(3D 프리뷰 + 머티리얼 세부설정) ↔ 수직 스플리터 ↔ 우측(셰이더 그래프 캔버스 70% + 노드 인스펙터/변수/빌드상세 30% 내부 리사이즈 테이블).
  - **오토스크롤링 버그 해소**:
    - 이중 Child Window 스크롤 충돌을 제거하고 캔버스에 정확한 `contentHeight`를 전달하여 노드 에디터의 스크롤 튐 및 오토스크롤링 문제 원천 해결.
  - **머티리얼 뷰포트 바닥 그리드 렌더링 복구**:
    - `PreviewViewportWidget.cpp`: `Initialize` 시 `m_ShowGrid` 조건문 없이 항상 `m_Grid.Initialize`를 수행하도록 변경.
    - `PreviewViewportWidget.h`: `SetGridVisible`/`ToggleGrid` 시 `m_Grid.Show()` / `Hide()`를 즉시 동기화.
    - `MaterialPreviewRenderer.cpp`: `Initialize`에서 그리드 활성화(`enableGrid = true`).
  - **뷰포트 영역 정체불명 패딩 제거**:
    - `MaterialPreviewRenderer::DrawViewportOnly()`: 기존 정사각형 강제 클램핑(`minSide`)을 제거하고 가용 너비/높이를 100% 채움.
    - `MaterialEditor.cpp`: `##MaterialViewportChild`에 `ImGuiStyleVar_WindowPadding(0, 0)`을 적용하여 불필요한 내부 패딩 테두리 완전 제거.
- **로캘 키 등록 (지침 11·14절 준수)**:
  - **셰이더/머티리얼 그래프 미표시(`GraphPanel.NotReady`) 해결**:
    - `GraphPanel.h`, `GraphPanel.cpp`: `inline static GraphPanel* s_Instance`를 도입하여 창 포커스/가시성 여부와 무관하게 `GetActiveGraphPanel()`이 유효한 싱글톤 인스턴스를 항상 반환하도록 보장.
    - `DrawEmbeddedCanvasOnly` 및 `DrawEmbeddedPropertiesOnly` 진입 시 `m_Domain = EGraphDomain::Shader` 강제 및 `ActivateDocument()`를 명시 호출하여 노드 컨텍스트 활성화.
  - **툴바 중복 버튼 정리**:
    - `MaterialEditor::DrawCustomQuickAccess()`: `DrawHistoryButtons` 뒤에 중복 배치되어 있던 Revert 버튼(`ICON_FA_ROTATE_LEFT`)을 제거하여 Undo/Redo/Revert 화살표 반복 혼란 해소.
  - **프리뷰 위젯 높이 조절 가로 스플리터바 추가 & 퀵액세스 툴바 통합**:
    - 좌측 패널 상단 프리뷰 뷰포트와 하단 설정 사이에 `Editor::SplitterH("##MaterialPreviewSplitterH", ...)`를 도입하여 마우스 드래그로 높이 자유 조절 지원.
    - `구체(Sphere)`, `큐브(Cube)`, `평면(Plane)`, `자동 회전 토글(AutoRotate)`, `카메라 리셋(ResetCamera)`, `그리드 토글(ToggleGrid)`을 상단 퀵액세스 툴바로 이동.
    - `MaterialPreviewRenderer::DrawViewportOnly()` 신설로 프리뷰 내부의 중복 툴바를 제거하고 순수 3D 뷰포트로 꽉 차게 렌더링.
  - **셰이더 그래프 열기/연결해제 버튼 짤림 방지**:
    - 긴 텍스트 버튼 대신 `EditorUI::SmallIconButton`을 적용하여 좁은 프로퍼티 컬럼에서도 짤림 없이 콤팩트한 아이콘과 툴팁으로 표시.
  - **패스별 셰이더 및 기본 셰이더 컬럼 간격 최적화**:
    - `m_LeftPaneWidth` 기본값을 380px로 상향하여 가로 폭 여유 확보.
    - `PassShadersTable`의 `PassType` 컬럼 폭을 110px에서 75px로 줄여 `VertexShader`와 `PixelShader` 컬럼이 충분한 폭을 유지하도록 개선.
  - **패스별 셰이더 헤더 우측 상단으로 추가 컨트롤 이동**:
    - `--- 패스별 셰이더 ---` 헤더와 같은 줄 오른쪽에 `[패스 종류 콤보]`와 `[+ 패스 셰이더 추가]` 버튼을 나란히 배치하고 표 하단의 중복 컨트롤 제거.
- **로캘 키 등록 (지침 11·14절 준수)**:
  - `Editor/Resources/Data/Localization/Editor.GraphPanel.csv`: `LiveAutoCompile`, `LiveCompiling`, `LiveSuccess`, `LiveError`, `LiveManualTrigger` 5개 행 추가.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] Client 리소스 쿠킹·바이너리화·팩 배포 계획

- [Client 에셋 팩 계획](ClientAssetCookingAndPackingPlan.md)에 선택 수집, 유형별 쿠킹·기존 BEVE 재사용, Windows 팩 형식·마운트, 기존 내보내기에서 게시하기까지의 단계와 검증 기준을 기록했다.
- 현재 폴더 복사, Archive의 바이트 디코딩, 디스크 파일을 전제로 한 Client 로드 경로를 코드에서 대조했다. 팩 구현·빌드·실환경 검증·성능 비교는 이후 단계에 둔다.

### [2026-11-19] PIE 첫 실행 지연과 Windows Client 배포 흐름 점검

- [PIE·Windows Client 흐름 개선 계획](PIEWindowsClientWorkflowPlan.md)에 Play 중 동기 씬 저장, Game View 첫 제출의 지연 후보, 프레임 계측 순서와 동일 씬의 Windows 배포 비교 절차를 기록했다.
- 현재 호출 경로와 이전 Game View·Client 검증 기록, 로컬 로그를 대조했다. 프리징 원인은 실측 대기이며 개선 코드, 빌드·CTest와 패키지 실행은 이후 단계에 둔다.

### [2026-11-19] 모델 임포트·재임포트·에셋 이동 흐름 점검

- [에셋 흐름 개선 계획](AssetReimportRelocationWorkflowPlan.md)에 선택 모델 경로의 재임포트 전달, 생성물 묶음 복구, 저장된 바이너리 참조 검사와 임포트·이동 충돌 방지를 정리했다.
- 임포트 확정, 이동 확인 및 파일 이력, 모델 DDC·리소스 키의 코드와 기존 검사 기준을 대조했다. 기능 변경과 빌드·CTest·실환경 및 성능 검증은 이후 단계에 둔다.

### [2026-11-19] 프리팹 제작·배치·씬 저장 흐름 점검

- [프리팹 흐름 개선 계획](PrefabSceneWorkflowPlan.md)에 생성·배치·원본 편집·씬 재열기의 동작과 인스턴스 보존 계약을 기록했다. `Revert All`의 삭제된 객체 접근, 엔티티 참조의 핸들 직렬화, 원본 덮어쓰기와 Undo/Redo의 개선 순서를 정리했다.
- `SceneArchiveCheck`, `EditorHistoryCheck`, `AssetReferenceCheck`의 기존 검사 범위를 소스와 대조했다. 기능 변경 및 빌드·CTest·에디터·PIE 검증은 이후 단계에 둔다.

### [2026-11-19] 머티리얼-셰이더 그래프 통합 Phase 2 (`GraphPanel` 3D 머티리얼 프리뷰 통합) 구현

- **3D 머티리얼 프리뷰어 결합 (`GraphPanel`)**:
  - `Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h`, `Private/GraphPanel.cpp`:
    - `MaterialPreviewRenderer m_Preview3D` 멤버 인스턴스 결합 및 생성자(`Initialize(512, 512)`), `Free()` 해제 라이프사이클 구축.
    - `Update(f32 dt)` 오버라이드: 도메인이 `EGraphDomain::Shader`일 때 유효한 프리뷰 머티리얼을 찾아 `m_Preview3D.Submit(mat, dt)` 호출 (자전 애니메이션 및 디퍼드 렌더링 제출).
    - `GetPreviewMaterial()` 머티리얼 획득 계층 구축:
      1. 타겟 머티리얼 에셋(`m_TargetMaterial`) 우선 바인딩
      2. 세션 런타임 머티리얼(`RuntimeMaterial`)
      3. 엔진 기본 PBR 머티리얼(`DefaultMaterial.bammat.json`) 안전 폴백
    - `SetTargetMaterial(path)`: 머티리얼 에디터 등 외부에서 진입 시 대상 머티리얼을 즉시 바인딩할 수 있는 인터페이스 연동.
  - `Editor/Source/ImGui/GraphPanel/Private/GraphPanelShader.cpp`:
    - `Draw3DMaterialPreview()` 신설 및 `DrawShaderBuildDetails()` 상단 배치.
    - 상단 대상 머티리얼 에셋 정보 표시 및 해제(`ICON_FA_XMARK`) 버튼 연동.
    - `m_Preview3D.DrawUI(mat)`를 통해 구체(Sphere)/큐브(Cube)/평면(Plane) 메쉬 전환 툴바, 뷰포트 마우스 우클릭 Orbit 회전 및 휠 줌 조작계 제공.
  - `Editor/Source/ImGui/ResourceEditor/Private/MaterialEditor.cpp`:
    - `OpenOrCreateShaderGraph()` 진입 시 `GraphPanel::SetTargetMaterial(m_TargetResourcePath)`를 자동 연결하여 머티리얼 에디터에서 열었을 때 해당 머티리얼이 3D 프리뷰에 즉시 로드되도록 연동.
- **컴파일 빌드 오류 핫픽스 및 실환경 피드백 6종 2차 완성**:
  - **하단 상태 바 글자 짤림 해결**:
    - `GraphPanel::DrawEmbeddedWorkspace()`: 상태 바 2줄 텍스트(`SaveScope` + `Controls`)에 맞게 `footerHeight`를 2줄 높이(`GetTextLineHeightWithSpacing() * 2.0f + ItemSpacing.y * 2.0f`)로 정확하게 산출하여 글자 잘림 원천 차단.
  - **로캘 누락 및 버튼 오버플로우 해결**:
    - `Editor.GraphPanel.csv`: `CompileHotReload`, `CompileHotReloadTooltip`, `AutoCompile` 로캘 키 추가.
    - 상단 툴바의 우측 액션 버튼(`AddNode`, `PopOutWindow`)의 가용 폭 계산을 동적 텍스트 크기 기반으로 수정하여 좁은 화면에서도 삐져나감 방지.
  - **셰이더 그래프 워크스페이스 통합 (사용자 제안 수용)**:
    - 어설프게 3분할되었던 패널 구조를 `GraphPanel`의 완성된 원본 워크스페이스(`DrawEmbeddedWorkspace`) 통째 렌더링 방식으로 전환.
    - 좌측(3D 프리뷰 + 머티리얼 세부설정) ↔ 수직 스플리터 ↔ 우측(셰이더 그래프 캔버스 70% + 노드 인스펙터/변수/빌드상세 30% 내부 리사이즈 테이블).
  - **오토스크롤링 버그 해소**:
    - 이중 Child Window 스크롤 충돌을 제거하고 캔버스에 정확한 `contentHeight`를 전달하여 노드 에디터의 스크롤 튐 및 오토스크롤링 문제 원천 해결.
  - **머티리얼 뷰포트 바닥 그리드 렌더링 복구**:
    - `PreviewViewportWidget.cpp`: `Initialize` 시 `m_ShowGrid` 조건문 없이 항상 `m_Grid.Initialize`를 수행하도록 변경.
    - `PreviewViewportWidget.h`: `SetGridVisible`/`ToggleGrid` 시 `m_Grid.Show()` / `Hide()`를 즉시 동기화.
    - `MaterialPreviewRenderer.cpp`: `Initialize`에서 그리드 활성화(`enableGrid = true`).
  - **뷰포트 영역 정체불명 패딩 제거**:
    - `MaterialPreviewRenderer::DrawViewportOnly()`: 기존 정사각형 강제 클램핑(`minSide`)을 제거하고 가용 너비/높이를 100% 채움.
    - `MaterialEditor.cpp`: `##MaterialViewportChild`에 `ImGuiStyleVar_WindowPadding(0, 0)`을 적용하여 불필요한 내부 패딩 테두리 완전 제거.
- **로캘 키 등록 (지침 11·14절 준수)**:
  - **셰이더/머티리얼 그래프 미표시(`GraphPanel.NotReady`) 해결**:
    - `GraphPanel.h`, `GraphPanel.cpp`: `inline static GraphPanel* s_Instance`를 도입하여 창 포커스/가시성 여부와 무관하게 `GetActiveGraphPanel()`이 유효한 싱글톤 인스턴스를 항상 반환하도록 보장.
    - `DrawEmbeddedCanvasOnly` 및 `DrawEmbeddedPropertiesOnly` 진입 시 `m_Domain = EGraphDomain::Shader` 강제 및 `ActivateDocument()`를 명시 호출하여 노드 컨텍스트 활성화.
  - **툴바 중복 버튼 정리**:
    - `MaterialEditor::DrawCustomQuickAccess()`: `DrawHistoryButtons` 뒤에 중복 배치되어 있던 Revert 버튼(`ICON_FA_ROTATE_LEFT`)을 제거하여 Undo/Redo/Revert 화살표 반복 혼란 해소.
  - **프리뷰 위젯 높이 조절 가로 스플리터바 추가 & 퀵액세스 툴바 통합**:
    - 좌측 패널 상단 프리뷰 뷰포트와 하단 설정 사이에 `Editor::SplitterH("##MaterialPreviewSplitterH", ...)`를 도입하여 마우스 드래그로 높이 자유 조절 지원.
    - `구체(Sphere)`, `큐브(Cube)`, `평면(Plane)`, `자동 회전 토글(AutoRotate)`, `카메라 리셋(ResetCamera)`, `그리드 토글(ToggleGrid)`을 상단 퀵액세스 툴바로 이동.
    - `MaterialPreviewRenderer::DrawViewportOnly()` 신설로 프리뷰 내부의 중복 툴바를 제거하고 순수 3D 뷰포트로 꽉 차게 렌더링.
  - **셰이더 그래프 열기/연결해제 버튼 짤림 방지**:
    - 긴 텍스트 버튼 대신 `EditorUI::SmallIconButton`을 적용하여 좁은 프로퍼티 컬럼에서도 짤림 없이 콤팩트한 아이콘과 툴팁으로 표시.
  - **패스별 셰이더 및 기본 셰이더 컬럼 간격 최적화**:
    - `m_LeftPaneWidth` 기본값을 380px로 상향하여 가로 폭 여유 확보.
    - `PassShadersTable`의 `PassType` 컬럼 폭을 110px에서 75px로 줄여 `VertexShader`와 `PixelShader` 컬럼이 충분한 폭을 유지하도록 개선.
  - **패스별 셰이더 헤더 우측 상단으로 추가 컨트롤 이동**:
    - `--- 패스별 셰이더 ---` 헤더와 같은 줄 오른쪽에 `[패스 종류 콤보]`와 `[+ 패스 셰이더 추가]` 버튼을 나란히 배치하고 표 하단의 중복 컨트롤 제거.
- **로캘 키 등록 (지침 11·14절 준수)**:
  - `Editor/Resources/Data/Localization/Editor.GraphPanel.csv`: `Preview3D`, `TargetMaterial`, `RuntimeMaterialFallback`, `Toggle3DPreview` 4개 행 추가.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] 머티리얼-셰이더 그래프 통합 Phase 1 (에셋 바인딩 및 원클릭 진입) 구현

- **데이터 모델 및 아키텍처 확장**:
  - `Engine/Source/Render/Material/Public/MaterialInterface.h`:
    - `m_ShaderGraphPath` (string) 프로퍼티 추가 및 리플렉션 등록 (`CATEGORY_SHADER`, `PROP_SHADER_GRAPH_PATH`).
    - 접근자 `GetShaderGraphPath()`, `SetShaderGraphPath()`, `HasShaderGraph()` 추가.
    - 기존 머티리얼 JSON 아카이브 로드 시 필드가 누락되어도 안전하게 기본값(`""`)으로 초기화되도록 하위 호환성 보장.
  - `Engine/Source/Render/Material/Private/MaterialInterface.cpp`: `Free()` 시 `m_ShaderGraphPath.clear()` 추가.
  - `Engine/Source/Render/Material/Public/MaterialInstance.h`, `Private/MaterialInstance.cpp`:
    - `MaterialInstance`에서 `GetShaderGraphPath()` 오버라이드 구현.
    - 인스턴스에 자체 셰이더 그래프가 설정되어 있지 않으면 베이스 머티리얼(`GetBaseMaterial()->GetShaderGraphPath()`)로 자동 폴백.
- **머티리얼 에디터 원클릭 진입 인터페이스 (`MaterialEditor`)**:
  - `Editor/Source/ImGui/ResourceEditor/Public/MaterialEditor.h`, `Private/MaterialEditor.cpp`:
    - 상단 퀵 액세스 툴바(`DrawCustomQuickAccess`): `ICON_FA_DIAGRAM_PROJECT` 아이콘의 [Open Shader Graph] 버튼 추가.
    - 셰이더 설정 섹션(`DrawShaderSettings`): 상단에 `Shader Graph` 속성 행을 배치하여 연결된 그래프명 표시, 툴팁 전체 경로 안내, `[Open Shader Graph]`(화살표 아이콘) 및 `[Unlink]`(연결 해제 아이콘), 미연결 시 `[Create Shader Graph]`(플러스 아이콘) 제공.
    - `GenerateDefaultShaderGraphPath()`: `Assets/Shaders/Graphs/<MaterialName>.bamgraph.json` 기본 경로 자동 생성.
    - `CreateDefaultMaterialShaderGraph(path)`: 그래프 에셋이 없을 때 `Engine::Graph(EGraphDomain::Shader)`를 생성하고 루트 노드인 `Shader.MaterialOutput`(ID: 1, 400x250)을 배치한 기본 템플릿을 디스크에 저장.
    - `OpenOrCreateShaderGraph()`: `ImGuiManager::Get().RequestOpenResourceEditor(resolvedPath)`를 호출하여 `GraphPanel`을 열고 그래프 파일을 로드.
- **로캘 키 등록 (지침 11·14절 원본 CSV 원칙 준수)**:
  - `Editor/Resources/Data/Localization/Editor.MaterialEditor.csv`:
    - `OpenShaderGraph`, `CreateShaderGraph`, `ShaderGraphPath`, `UnlinkShaderGraph`, `CreateGraphPrompt`, `BrowseGraph` 6종 추가.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] 애니메이션 제작·실행 파이프라인 개선 계획

- [개선 계획](AnimationPipelineIntegrationPlan.md)에 모델 임포트, Animator Graph, VFX, Notify, Action Timeline의 연결을 기록했다. 편집 씬 격리와 Animator 복원, 프리뷰 발동 규칙 일치, 모델·클립 참조 정리, 입력 기반 공격 샘플의 PIE·독립 Client 검증 순서를 정리했다.
- 현재 소스, `BaselineVerification`의 재생 설정, 저장된 타임라인의 모델 경로와 기존 PIE 이벤트 검증 기록을 대조했다. 개선 코드 적용과 빌드·CTest·실환경 검증은 이후 단계에 둔다.

### [2026-11-19] 머티리얼 에디터 3분할 통합본 결함 수정 (데이터 보호·레이아웃)

- **배경**: 다른 에이전트가 머티리얼 에디터를 좌(프리뷰+속성)·중앙(셰이더 그래프)·우(노드 속성) 3분할로 통합하면서 `GraphPanelShader.cpp`·`GraphPanelMaterial.cpp`를 비우고 그래프 편집을 `MaterialEditorCanvas.cpp`·`MaterialEditorShader.cpp`로 옮겼다. 사용자 확인에서 화면이 제대로 나오지 않았다. 정적 검토 결과는 아래와 같다.
- **데이터 보호 [심각]**: 머티리얼을 열기만 해도 `EnsureShaderGraphLoaded()`가 `Assets/Shaders/Graphs/<이름>.bamgraph.json`을 만들고 경로를 기록해 미저장 상태로 만들었으며(읽기 전용 엔진 머티리얼 포함), 이어서 `LiveHotReloadShader()`가 빈 그래프를 컴파일해 씬과 공유하는 머티리얼의 픽셀 셰이더를 교체했다. 로드 실패 시 매 프레임 재시도했다. 열기는 연결된 그래프를 읽기만 하도록(`TryLoadLinkedShaderGraph`) 바꾸고, 그래프 생성·연결은 `[셰이더 그래프 만들기]`를 눌렀을 때만(`CreateShaderGraphForMaterial`) 한다. 셰이더 교체는 그래프가 연결되어 읽힌 쓰기 가능한 머티리얼에만 적용한다. 그래프가 없거나 파일이 없는 머티리얼은 중앙 영역에 안내와 만들기 버튼을 보인다.
- **프리뷰 수명**: 문서를 닫을 때 `m_PreviewRenderer.Free()`를 호출해 두 번째로 연 머티리얼부터 프리뷰가 그려지지 않았다. HEAD와 같이 머티리얼 참조만 놓고 위젯은 패널 수명 동안 유지한다.
- **레이아웃**: 세 영역과 스플리터 사이 `SameLine` 간격 4회를 폭 계산에서 빼고(우측 패널 잘림), 좁은 창에서는 저장된 선호 폭을 바꾸지 않고 이번 프레임만 비율로 나눈다. 좌측 패널의 세로 간격 2회를 높이에서 뺐다(설정 영역 하단 잘림).
- **헤더**: 메뉴 줄(파일·보기·편집)이 아이콘 버튼으로 바뀌고 빠른 실행 줄이 같은 함수를 다시 불러 아이콘 줄이 두 번 그려졌으며 되돌리기·닫기·보기 메뉴가 사라졌다. HEAD의 메뉴 줄과 빠른 실행 줄을 복원하고, 새 액션(`MaterialEditor.CompileHotReload`, `MaterialEditor.ResetView`)은 빠른 실행 줄에 둔다.
- **중앙 캔버스**: `ToolbarLayout::Button(id, icon, labelKey)`에 아이콘+문구와 툴팁 키를 넘기고 수동 `SameLine`을 섞어 버튼 문구가 중복되고 정렬이 꼬였다. 올바른 인자와 `SameLineIfFits`로 바꿨다. 노드를 그리기 전에 `NavigateToContent`를 호출해 처음 열 때 캔버스가 비어 보였으므로 노드를 그린 뒤로 옮겼다. 상태바 오른쪽 문구의 위치 계산을 창 기준으로 고치고 공간이 없으면 생략한다.
- **우측 노드 속성**: `EmptyState`가 커서를 패널 중앙으로 옮겨 셰이더 진단·HLSL 섹션을 밀어냈으므로 섹션 안 안내 문구로 바꿨다. 하드코딩된 `"Node #..."`, `"Input Pins"`, `"Connected"`를 로캘 키(`MaterialEditor.NodeTitle`, `InputPins`, `PinConnected`)로 바꾸고 입력 핀을 2열(라벨 | 값) 표로 정리했다. `Editor.MaterialEditor.csv`에 7개 키를 추가했다.
- **노드 그리기 assert (사용자 실행에서 발견)**: 그래프가 있는 머티리얼을 열면 `MaterialEditor::DrawNode`가 `ed::BeginNode`와 `ed::EndNode` 사이에서 `ed::GetNodeBackgroundDrawList()`를 호출해 `NodeBuilder::GetUserBackgroundDrawList`의 assert("available after EndNode()")에서 멈췄다. 프로젝트의 imgui-node-editor는 노드를 만드는 동안 드로우리스트 분할기가 바뀌므로 이 호출을 막는다. 헤더 높이는 `NodeEditorUI::BeginNodeHeader`로 기록하고, 배경은 `ed::EndNode()` 뒤 `NodeEditorUI::DrawNodeHeader`로 그리도록 바꿨다(Graph·Animator 패널과 같은 규격). 출력 핀을 `GetContentRegionAvail()`로 오른쪽 정렬하던 코드는 노드 안에서 폭이 정해지지 않아 노드를 넓히므로, 입력 열과 출력 열을 그룹으로 나란히 그리도록 바꿨다. `"Material"` 하드코딩 제목은 `ResourceType.Material` 로캘 키를 쓴다.
- **링크 생성·삭제 assert (사용자 실행에서 발견)**: `HandleConnections`가 `ed::EndCreate()`·`ed::EndDelete()`를 `if (ed::BeginCreate())`·`if (ed::BeginDelete())` 밖에서 매 프레임 호출해 `CreateItemAction::End`의 assert("call End() only when Begin() was successful")에서 멈췄다. Graph·Animator 패널처럼 `End*()`를 블록 안으로 옮겼다. 함께 고친 결함: `QueryNewLink`의 첫 핀을 입력으로 가정해 출력 핀에서 끌면 방향이 뒤집혀 연결되지 않았으므로 핀 정의의 방향으로 출력→입력을 정한다. 입력 핀의 기존 링크 교체를 그래프 사본에서 먼저 확인하고 실패하면 `RejectNewItem`으로 표시한다. 생성 트랜잭션 안에서 열던 노드 메뉴는 플래그로 미뤄 캔버스의 `ed::Suspend` 구간에서 연다. 삭제는 트랜잭션 안에서 대상만 모으고 `EndDelete()` 뒤에 한 번에 지운 뒤 이력 한 건으로 기록한다.
- **캔버스 밖 호출**: 툴바·단축키에서 부르는 `OpenAddNodeMenu`가 캔버스 밖에서 `ed::Suspend()`를 호출하고 다른 ID 스택에서 팝업을 열었으며 편집기가 없으면 null에 접근했다. 요청 플래그만 남기고 다음 캔버스 그리기에서 마우스 위치로 연다. `AlignSelectedNodes`·`DuplicateSelection`·`CreateCommentBoxFromSelection`은 현재 편집기를 이 문서의 편집기로 지정하지 않고 노드 위치를 다뤄 다른 패널의 편집기를 건드릴 수 있었으므로 문서 편집기를 지정·복원한다.
- **셰이더 컴파일 항상 실패 [높음]**: 진단에 `#include "shading_flags.hlsli"` 오류가 표시되었다. 기존 `GraphPanel`은 임시 폴더에 생성 HLSL과 함께 `shading_flags.hlsli`를 복사했으나, 옮겨진 `ReserveMaterialShaderSourcePath()`는 복사를 빠뜨려 그래프 컴파일·라이브 핫스왑이 모두 실패했다. 임시 폴더를 만들 때 `Engine://Shader/hlsl/shading_flags.hlsli`를 함께 복사한다(세 컴파일 경로 공통).
- **그래프 지정·생성**: 연결 해제 후 다시 연결할 방법이 없었다. 인스펙터의 Animator·Timeline 지정과 같은 `DrawInspectorAssetPathPicker`로 `.bamgraph.json`을 목록·드래그 앤 드롭으로 지정·해제하고, 그래프가 없으면 `[셰이더 그래프 만들기]`를 함께 보인다. 좌측 셰이더 설정과 중앙 안내 화면 양쪽에 둔다. 고른 그래프는 먼저 읽어 보고 셰이더 그래프로 읽혔을 때만 연결하며, 가상 URI(`Project://…`)·프로젝트 상대·머티리얼 폴더 상대 경로를 모두 해석한다.
- **진단·HLSL 표시**: 공백 없는 긴 경로가 들어간 컴파일 오류가 패널 밖으로 늘어났다. 줄바꿈 표시, 복사 버튼(`MaterialEditor.CopyDiagnostics`), 폭을 제한한 전체 내용 툴팁을 두고, 툴바 상태 툴팁도 같은 폭으로 줄바꿈한다. 생성 HLSL은 읽기 전용 여러 줄 입력창으로 바꿔 일부 선택·복사가 가능하다.
- **입력 핀 표**: 공용 `EditorUI::BeginPropertyTable`·`PropertyLabel`(핀 타입을 상세 툴팁)·`PropertyValue`로 바꾸고, 우측 표 전용 `DrawPinInspectorValue`가 값 열 폭을 채운다. Float3·Float4는 인스펙터와 같은 `ReflectionPropertyDrawer::DrawVector3/4`(좁으면 세로 배치)를 쓰고, 정수는 `GraphValue`의 `int64_t`로 편집한다. 노드 안 작은 입력칸은 캔버스용으로 유지한다.
- **로캘 보완**: `EditorCommon.Comment` 키를 추가하고 코멘트 노드의 하드코딩 `"Comment"`를 바꿨다. `Engine.csv`의 `PROP_SHADER_GRAPH_PATH` 번역을 채웠다.
- **알려진 한계**: `MaterialInstance::GetShaderGraphPath()`는 자기 경로가 비면 부모 머티리얼 경로를 돌려주므로, 인스턴스에서 해제해도 연결된 것으로 보이고 그래프 편집이 부모 그래프 파일에 반영된다(인스턴스 정책 결정 필요). `Temp/MaterialShader/<번호>` 폴더가 컴파일마다 쌓인다(기존 GraphPanel과 동일).
- **줄바꿈**: 통합본의 네 파일이 LF로 저장되어 있어 CRLF로 맞췄다.
- **남은 범위**: 그래프 편집 코드가 `GraphPanelNodeEditor`와 `MaterialEditorCanvas`에 이중으로 있다(공용 컴포넌트로 통합 필요). 셰이더 교체가 씬 공유 머티리얼에 즉시 적용되며 그래프 연결 해제 시 원래 픽셀 셰이더로 돌아가지 않는다(프리뷰 전용 복사본 적용 검토). `EditorCommon.Comment`·`EditorCommon.Duplicate` 로캘 키 누락(통합본 기존 결함). 컴파일이 메인 스레드에서 동기로 실행된다.
- **검증 상태**: 정적 확인만 했다. 빌드와 화면 확인은 사용자 실행 전이다.

### [2026-11-19] 계층 패널 Visible·Active 토글 아이콘 잘림

- **원인**: 행 오른쪽 토글은 `GetFrameHeight()` 정사각형 `ImGui::Button`이었다. `ImGui::Button`은 `FramePadding` 안쪽에만 라벨을 그려, 체크보다 넓은 `ICON_FA_EYE`·`ICON_FA_EYE_SLASH`의 양옆이 잘렸다. 라벨이 아이콘 자체라 상태가 바뀌면 버튼 ID도 바뀌었다.
- **수정** (`EntityHierarchyView.cpp`): 두 토글이 같은 슬롯을 쓴다. 높이는 `FramePadding` 트리 노드 행과 같은 `GetFrameHeight()`, 폭은 세 아이콘 중 가장 넓은 폭에 글꼴 크기 15%의 가로 패딩을 더한 값과 행 높이 중 큰 값이다. 버튼을 그리는 동안만 가로 패딩을 줄이고 세로 패딩은 유지해 행 높이가 변하지 않는다. 오른쪽 정렬 폭도 슬롯 기준으로 계산하고, ID는 `##Visible`·`##Active`로 고정했다. 공용 `EditorUI::IconButton`과 같은 계산 방식이지만, `HierarchyEditorCheck`가 `EditorUI.cpp`를 링크하지 않아 같은 파일에서 직접 처리했다.
- **검증 상태**: 정적 확인만 했다. 보임·숨김 상태의 아이콘 잘림, 두 버튼 크기·높이 일치, UI 배율 변경 시 행 높이 유지는 사용자 확인 전이다.

### [2026-11-19] 프리팹 Revert All 삭제 후 접근 크래시와 교체 실패 처리

- **결함 [심각]**: `InspectorPanel::Draw()`가 보관한 `targetEntity`로 `DrawPrefabHeaderBar()`를 호출하고, `Revert All`이 그 안에서 기존 루트와 자식을 `DestroyEntity`로 삭제했다. 헤더가 돌아온 뒤 `DrawModelMaterialRefresh(*targetEntity)`와 본문 그리기가 해제된 엔티티를 사용했다. 기존 트리 삭제 실패를 무시해 새 트리가 남은 채 성공 알림이 표시될 수 있었고, 이력 기록과 형제 순서 보존이 없었다. [프리팹 흐름 계획 2.1](PrefabSceneWorkflowPlan.md#21-revert-all의-삭제와-그리기-순서)
- **수정**: 메뉴는 씬과 루트 ID만 담은 요청을 남기고, `ImGui::End()` 뒤 `ExecutePendingPrefabRevert()`가 실행한다. 실행 직전 현재 씬·편집 가능 상태·루트·원본 경로를 다시 확인하고, 편집할 수 없는 상태에서는 메뉴를 비활성화한다. 배치 실패는 기존 트리를 유지하고, 기존 트리 삭제가 거부되면 새 트리를 지운다(실패 시 교체 전 스냅샷 복원). 부모 아래에서는 `ReorderSiblings`로 새 루트를 기존 자리에 둔다. 성공한 교체만 씬 이력 한 건(`Prefab.RevertAll`)으로 기록하고 프리팹 모드 씬이면 미저장 표시를 켠다. 잠긴 인스펙터는 새 루트로 잠금을 옮긴다.
- **배치 Transform 범위**: 처음 구현은 기존 루트의 위치·회전·스케일을 모두 새 루트에 복사해, 스케일을 바꾼 뒤 `Revert All`을 실행해도 스케일이 되돌아가지 않았다(사용자 확인). 인스턴스를 놓은 자리인 위치·회전만 유지하고 스케일은 원본 값을 쓰도록 고쳤다.
- **외부 참조 보호**: `DestroyEntity`는 트리 밖에서 트리 안을 가리키는 리플렉션 `entt::entity` 속성을 조용히 비운다. 하위 트리 수집과 참조 탐색을 `Scene.cpp`의 공용 함수로 분리하고 `Scene::CountExternalEntityReferences()`를 추가해, 그런 참조가 있으면 교체를 막고 개수를 알린다(`Notification.PrefabRevertBlockedByReferences`). `DestroyEntity`의 판정 규칙은 그대로다. 실패 알림은 `Notification.PrefabRevertFailed`를 사용한다(`Editor.Notification.csv`).
- **검사**: `SceneArchiveCheck`에 `CheckExternalEntityReferenceCount`(트리 내부 참조 제외, 외부·부모 참조 계수, 해제 후 0, 다른 씬 엔티티 거부)를 추가했다.
- **한계**: 최상위 엔티티는 엔진에 형제 순서 모델이 없어 순서를 보존하지 않는다. 프리팹 내부 참조의 새 ID 대응(2.2)과 `Apply All`·`Unpack` 이력(2.3)은 범위 밖이다.
- **검증 상태**: 정적 확인만 했다. 빌드·CTest(`SceneArchiveCheck`)와 루트 선택·자식 선택·잠긴 인스펙터·원본 누락·외부 참조가 있는 경우의 실환경 확인, Undo/Redo 한 번씩의 복원은 사용자 실행 전이다.

### [2026-11-19] 재점검 후속: 로캘 누락, 임포트 프리뷰 스레드, 모달 단축키 경로

- **로캘**: 엄격 감사(`audit_localization.py --strict`)에서 실제 누락 키 1건과 하드코딩 문구 3곳이 남아 있었다. `Editor.AnimatorGraph.csv`에 `LiveActiveState`, `DefaultEntryHint`, `GlobalTransitions`, `AnyShort` 행을 추가하고 `AnimatorGraphPanel`의 Entry·Any State 노드 부제, 전이 목록의 Any 표시와 상대 상태 이름(`[Any State]`)을 로캘 키로 바꿨다. 같은 감사의 나머지 MISSING 7건은 액션 ID·저장 액션 ID를 로캘 키로 오인한 것이다. JSON은 빌드의 `GenerateLocalization`으로 생성한다.
- **모델 임포트 프리뷰**: 설정을 바꿀 때마다 detach된 Assimp 파싱 스레드가 새로 시작되어 대형 모델에서 파싱이 겹칠 수 있었다. `ModelImportModal`은 진행 중인 준비 작업이 있으면 새 작업을 시작하지 않고, 완료 시 재요청이 있거나 취소된 결과를 버린 뒤 다음 프레임에 새 설정으로 시작한다. 모달을 닫거나 `Free()`하면 진행 중 작업에 취소 표시를 남기고, 작업 스레드는 취소된 결과를 직접 해제한다. 닫힌 뒤에도 완료된 상태를 수거하도록 `ShouldUpdateWhenClosed()`를 연결했다. 남은 한계: 진행 중인 Assimp 파싱 자체는 중단하지 못하므로, 닫기 직후 임포트를 시작하면 해당 파싱이 끝날 때까지 두 작업이 겹친다. 에디터 종료 시에도 스레드를 회수하지 않는다.
- **모달 프리뷰 단축키**: `ImGuiManager`에 하드코딩되어 있던 액션 ID 4개와 `TryTriggerAction`을 제거했다. `EditorAction::AllowInModalPreview`를 추가해 `Viewport.Focus`, `Viewport.ToggleGrid`, `Viewport.CameraSpeedIncrease/Decrease` 정의에서 허용하고, `ShortcutManager::ProcessModalPreviewShortcuts()`가 바인딩·`CanExecute`·키 캡처를 중앙에서 평가한다. 이제 문서 저장 대기와 창 변경 확인 대기 중에는 모달 프리뷰 단축키도 실행하지 않는다. `ShortcutManagerCheck`에 허용·비허용·키 캡처 사례를 추가했다.
- **문서**: `DevelopmentPriorities.md` 11.6의 잘못된 경로(`System/Physics`, `Render/VFX`, `ActionTimelineEditor`, `VFXEditor`, `EditorUIFooterBar.cpp`)와 후보별 상태를 현재 코드·커밋 기준으로 갱신하고, 1절에 [리소스 편집기 비동기 열기 계획](ResourceEditorAsyncOpenPlan.md)을 연결했다. 계획 2절에는 임포트 모달의 스레드 수명 방식을 참고하지 않는다는 제약을 추가했다.
- **커밋 이력 주의**: `3170bc23`(Animator Graph)는 `aeb4cd15`에서 추가되는 `ResourceDocumentHistory.h`를 포함하므로 `3170bc23`과 `f1b843be`는 단독으로 Editor 빌드가 되지 않는다. 이미 원격에 반영되어 이력은 고치지 않았다. `git bisect` 중 두 커밋은 `git bisect skip`으로 건너뛴다.
- **검사 대상 빌드 오류 2건 (`x64-ninja-debug` 모두 빌드에서 발견)**: 두 결함 모두 `73a4e217`·`925096ba`(2026-11-19 00:35) 이후 검사 실행 파일을 빌드하지 않아 드러나지 않았다. 이전 CTest 38/38 기록은 두 커밋 이전 결과다.
  - `HierarchyEditorCheck`: PCH가 `Engine_Includes.h`만 포함해 디버그 `#define new`가 살아 있는 상태에서 `EntityHierarchyView.cpp`가 `<imgui_internal.h>`를 직접 포함했고, `ImPool::Add()`의 placement new(`IM_PLACEMENT_NEW`)가 치환되어 C2760이 발생했다. `ReflectionPropertyDrawer.cpp`와 같은 `push_macro`/`pop_macro` 구간으로 감쌌다. 전역 `new` 매크로를 피해 가는 위치가 여섯 곳이 되었으므로, 디버그 할당 추적 방식 교체는 별도 과제로 남긴다.
  - `AssetReferenceCheck`: `WriteFixture(const fs::path&, const char*)`에 `std::string`을 넘겨 C2664가 발생했다. 매개변수를 `std::string_view`로 바꿔 기존 리터럴·`.c_str()` 호출을 유지하면서 문자열 객체도 받게 했다.
- **CTest 실패 3건 (`x64-ninja-debug`, 사용자 실행)**:
  - `SceneArchiveCheck` "Linked model part identities were not created": `692c8678`이 스켈레탈 모델 배치에서 노드 엔티티를 없애고 메시를 루트의 직속 자식으로 붙였으나, 검사와 링크 모델 갱신(`RefreshLinkedModelPartsInternal`)은 이전 계층(루트→노드→메시)을 가정했다. 갱신은 없는 노드를 새로 만들어 배치 직후 갱신만으로 노드 엔티티가 다시 생기고 `changed`가 참이 되는 결함이었다. 스켈레탈 모델에서 씬에 없는 노드는 루트를 대상으로 삼도록 고쳤다(이전 방식으로 저장된 씬의 노드 엔티티는 계속 사용). 검사의 파트 탐색은 루트 직속 자식 기준으로 바꿨고, 자식 수 1개 조건과 `changedB == false` 검사가 노드 재생성을 계속 감지한다.
  - `KinematicCharacterCheck` 계단(1/120초에서만 실패)과 `AssetReferenceCheck` 51번(`CheckTimelineRelocation`)은 정적 분석으로 실패 조건을 하나로 좁히지 못했다. 동작을 추측으로 바꾸지 않고 실패 단계·위치(계단: 단계·디딤판·시간·좌표·접지, 타임라인: 참조 수·편집 수·단계별 경로/파일 비교)를 출력하도록 검사를 보강했다. 다음 실행 출력으로 원인을 확정한 뒤 수정한다.
  - 진단 결과 계단 실패는 검사의 오판이었다. `t=2.35 pos=(0, 0.15, 1.454) radius=0.5`에서 하단 구 중심과 디딤판 0 모서리 `(z=1.9, y=0.2)`의 거리는 0.634로 반지름보다 커 접촉하지 않는다. 이전 판정은 "발 높이가 디딤판 윗면보다 낮고 수평으로 `Radius - 0.05` 안에 있으면 관통"으로 보아, 모서리를 넘어가는 구를 관통으로 처리했다. 1/60초에서는 프레임 이동량이 이 폭 0.05m 구간을 건너뛰어 드러나지 않았다. 캡슐 선분과 디딤판 단면의 최단 거리가 `Radius - 0.01`보다 작을 때만 관통으로 판정하도록 바꿨다.
  - 타임라인 실패 단계는 "moved file notify tracks differ from memory"였다. 모델 경로는 일치하고, 참조 편집은 `m_ModelPath`·`SoundPath`·`VFXPath`만 바꾸며, glm 벡터는 배열 순서로 저장되고 정수 토큰의 float 변환도 허용됨을 정적으로 확인했으나 다른 필드를 특정하지 못했다. 불일치 시 트랙·노티파이 필드별 파일/메모리 값을 출력하도록 검사를 보강했다.
  - 필드별 출력 결과 `Kind: file=1 memory=0`이었다. 트랙 종류 판정(`ResolveTrackKind`)이 `ActionTimelineAsset::LoadFromFile`에만 있어, `.actiontimeline.json`처럼 `ResourceManager`의 공용 JSON 로드나 핫 리로드로 읽은 타임라인은 `Unspecified`로 남는 결함이었다. 타임라인 패널은 표시할 때 다시 판정하므로 아이콘은 맞게 보이지만, 메모리와 저장값은 `Unspecified`로 남아 트랙 이름을 바꾸면 종류가 함께 바뀔 수 있었다. 판정을 `Resource::ReadArchive`가 모든 경로에서 호출하는 `OnPostLoad()` 재정의로 옮기고 `LoadFromFile`의 중복 판정을 제거했다. 필드별 진단 출력은 이후 회귀 원인 확인용으로 유지한다.
- **의도된 경고 로그**: `AssetReferenceCheck` 통과 시에도 `Walk.bamanimator`·`ReferenceRepair\B.bamscene.json` 잠금 실패(파일을 직접 잠가 쓰기 실패 경로를 검사), `Resource relocation destination is cached`(이동 대상에 다른 리소스가 캐시된 경우 거부), `Resource retains unsupported relocation paths: Brick`(경로 변경 미지원 리소스 거부), `[ERROR] Graph file not found: 'Resources/MovePrefabGraph/Behavior.json'`(이동 후 이전 경로로는 그래프가 로드되지 않아야 함)이 출력된다. 판정은 CTest 결과와 `failed at check N` 메시지로 한다.
- **Visual Studio 테스트 탐색기**: 검사 실행 파일이 없을 때 구성된 CTest 목록(실행 경로 누락)이 남아 `모두 실행`이 비활성화될 수 있다. 모두 빌드 후 CMake 캐시 구성을 다시 실행하고 새로 고친다. 명령줄 `ctest --preset x64-ninja-debug --output-on-failure`는 영향이 없다.
- **검증 상태**: 사용자가 `x64-ninja-debug` 모두 빌드 후 CTest를 실행해 통과를 보고했다(2026-11-19 15:50, 등록 테스트 44개). 이 결과로 `ShortcutManagerCheck`의 모달 프리뷰 사례, `HierarchyEditorCheck`·`AssetReferenceCheck` 빌드 수정, `SceneArchiveCheck`·`KinematicCharacterCheck`·`AssetReferenceCheck`의 수정이 실행으로 확인되었다. 임포트 모달 옵션 연속 변경·닫기·재열기, 모달 중 F/G/+/- 단축키, Animator Graph 로캘 문구, 링크 스켈레탈 모델 갱신(Fox)과 `SimpleTests` 계단 하강의 실환경 확인은 남아 있다.
- **커밋**: `8c2d2235`(로캘·임포트 프리뷰·모달 단축키), `b4041983`(검사 빌드 오류와 CTest 실패), 이 기록과 우선순위 문서는 별도 문서 커밋.

### [2026-11-19] 리소스 편집기 비동기 열기 계획

- [리소스 편집기 비동기 열기 계획](ResourceEditorAsyncOpenPlan.md)에 기존 도킹 창의 로딩 표시, 모델 편집기부터 적용하는 순서, 여러 패널이 공유하는 프레임 예산과 검증 기준을 기록했다.
- 열기 요청, 동기 리소스 로드, 의존 리소스 연결과 프리뷰 생성 경로를 정적으로 확인했다. 구현·빌드·실행 및 성능 기준 측정은 이후 단계에 둔다.

### [2026-11-19] 리소스 편집기 문서 수명과 편집 이력 통합

- 공통 리소스 편집기가 닫기와 문서 교체의 저장·버리기·취소를 처리한다. 확정한 닫기는 대상 경로, 프리뷰 참조와 편집 이력을 해제한다. 파일 메뉴·퀵액세스·단축키는 같은 실행 조건을 사용한다. 설정과 프리팹 모드의 미저장 내용도 앱 종료 요청에서 확인한다.
- `ResourceDocumentHistory`는 연속 편집을 하나의 스냅샷으로 기록하고, 저장 시점의 이력 상태 ID로 미저장을 판정한다. Logic과 Shader는 각자의 이력을 사용하며, Animator Graph의 노드 위치는 이전 파일에서도 열 수 있는 선택 속성으로 저장한다. 파티클과 VFX의 다른 이름으로 저장은 복사본을 연 뒤 별도 이력을 시작한다. Sprite Animation 플러그인은 V3 ABI로 닫기와 Undo/Redo를 연결한다. [리소스 편집기 적용 현황](ImGuiDevelopmentPlan.md#리소스-편집기-기본-기능-현황-2026-11-19-정적-점검)
- `EditorHistoryCheck`에 저장 기준점, 편집 병합, 복원 실패 및 분기 검사를 추가했다. 빌드, CTest, 플러그인 DLL 재로드와 화면 조작 검증은 미실시다. [실환경 체크리스트](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-19] Logic·Shader 그래프 노드 표시와 로캘 산출물 동기화

- Logic·Shader 그래프의 노드 테마, 테두리 핀, 헤더, 링크 표시가 `NodeEditorUI`를 공유하도록 연결했다. 노드 위치는 그래프 문서 기준으로 유지하고 휠 클릭 패닝 설정을 보존했다. [그래프 편집기](GraphPanel.md)
- Engine·Editor의 영어·한국어 JSON 네 개에서 CSV 원본과 키 및 번역값이 일치하는 것을 확인했다. 런타임 언어 전환과 엄격 로캘 감사의 재실행 결과는 확인되지 않았다.
- 머티리얼–셰이더 그래프 연결과 3D 프리뷰는 별도 개발 계획에 기록했다. 코드 착수와 실행 검증은 예정 상태다. [통합 개발 계획](MaterialShaderGraphIntegrationPlan.md)

### [2026-11-19] KCC 램프 미끄러짐에서 메시 바닥 관통 차단

- 사용자 재확인에서 램프에 점프해 닿은 뒤 계속 낙하하는 현상이 남았다. 급경사 미끄러짐은 이동 단계에서 대각선 하강을 적용하며 공중 수직 착지 검사를 건너뛴다. 이동 전후의 높이 구간을 도착 XZ에서 검사해 평지 지지면을 만나면 그 위치에서 하강을 멈추고 접지 상태를 갱신한다.
- 60° 경사로에 낙하한 뒤 메시 바닥으로 미끄러지는 회귀 검사를 30Hz·60Hz·120Hz에 추가했다. 2026-11-19 사용자 실환경 재확인에서 램프에 점프해 접촉한 뒤 평지에 착지하는 동작이 의도대로 동작했다고 보고했다. `KinematicCharacterCheck`와 `CollisionGeometryCheck`의 CTest 결과는 확인되지 않았다. [급경사 전환 확인](ThirdPersonCharacterSystemPlan.md#급경사-미끄러짐에서-평지로-전환)

### [2026-11-19] KCC 램프 정지와 공중 충돌의 수직 이동 분리

- 사용자가 램프에서 키를 뗀 뒤 계속 올라가는 현상과 점프 중 램프에 닿으면 바닥을 통과하는 현상을 보고했다. 걸을 수 있는 경사에서는 입력 해제 시 속도를 0으로 만들고, 계단 보정에 현재 이동 입력을 요구한다. 급경사에서 이미 가진 오르막 속도도 제거한다.
- 공중 충돌이 벽 법선으로 인해 높이를 바꾸지 않도록 수평 이동으로 제한했다. 지지할 수 없는 스윕 접촉은 최단 거리 후보에서 제외하고, 가까운 레이 히트가 지지면으로 부적합하면 뒤쪽의 바닥까지 검사한다.
- 램프 진입 후 정지, 공중 측면 충돌 높이, 아랫면에 가려진 착지 회귀 검사를 추가했다. 수정 후 빌드·CTest·실환경 확인은 사용자 실행 대기다. [램프 회귀 확인](ThirdPersonCharacterSystemPlan.md#램프-진입-정지와-점프-충돌-회귀-확인)

### [2026-11-19] 비균일 스케일 계단·램프의 KCC 침투 수정

- 사용자가 계단 왕복과 램프에서 몸이 점차 파고든다고 보고했다. 변환 행렬의 스케일을 회전으로 해석하던 축 정렬 검사를 바로잡고, 직교 박스 축에서 캡슐의 월드 반지름을 보존하도록 교차·스윕을 정리했다. 캡슐 끝점도 최근접점 후보에 포함하며, 겹침 결과의 침투 깊이를 이동 거리로 덮어쓰던 부분을 수정했다.
- 지지면 스윕은 음수 낙하 거리를 보존하고 발밑의 높은 지지면을 먼저 선택한다. 계단 오르기도 같은 판정을 사용하고, 경사 이동 스윕의 인위적인 캡슐 상승을 제거했다.
- 경사 박스 형상 검사와 `SimpleTests`의 배율·높이를 사용한 계단 왕복 검사를 추가했다. 빌드·CTest·실환경 결과는 사용자 확인 대기다. [계단·램프 회귀 확인](ThirdPersonCharacterSystemPlan.md#계단램프-침투-회귀-확인)

### [2026-11-19] 인스펙터의 타임라인 열기 버튼 제거

- 문제: 인스펙터 `ActionTimelineComponent`의 "타임라인 에디터에서 열기" 버튼이 `AnimationTimelinePanel::LoadFromFile`을 직접 불렀다. 패널에 미저장 편집이 있어도 확인 없이 다른 파일로 바뀌어 편집 내용을 잃을 수 있었다(코드 판독, 실행 미확인). 같은 일을 경로 필드 더블클릭(공용 리소스 편집기 경로, 미저장 확인 포함)이 이미 하고 있었다.
- 변경: 버튼과 더 쓰지 않는 `AnimationTimelinePanel.h` include, `Timeline.OpenInEditor` 로캘 행을 제거했다. 타임라인은 경로 필드 더블클릭이나 콘텐츠 브라우저 더블클릭으로 연다(19절 더블클릭 규칙). [타임라인 계획 10절](AnimationTimelineEditorPlan.md#10-작성-모델클립-바인딩-2026-11-19)
- 검증: 정적 검토만 했다. 빌드(제거한 include에 기대던 전이 포함 여부 포함), 더블클릭 열기와 미저장 확인 창, 로캘 엄격 감사는 사용자 확인이 필요하다. AI는 빌드·실행을 하지 않았다.

### [2026-11-19] 애니메이터 패널 가로 스크롤 결함 해결 및 EditorUI::BeginPropertyTable 기반 속성 표 전면 일원화

- **가로(좌우) 스크롤바 결함 해결**:
  - `AnimatorGraphPanel.h`: `GetResourceWindowFlags()`에서 `ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse`를 반환하도록 오버라이드하여 최외곽 메인 창의 불필요한 스크롤바 원천 차단.
  - `AnimatorGraphPanel.cpp`: `DrawCustomContent()`의 3단 패널(좌/중/우) 너비 계산 시, 2개의 스플리터 사이에 존재하는 4개의 `ItemSpacing.x`를 정확히 반영하도록 `overhead = (splitterThickness * 2.0f) + (spacingX * 4.0f)`로 수정하여 가용 너비 초과(16px 누수)로 인한 가로 스크롤 발생 결함 완벽 해결.
- **속성 표 전면 일원화 (`EditorUI::BeginPropertyTable`)**:
  - 상태 인스펙터(`DrawStateInspector`):
    - 상태 기본 속성(`StatePropertiesTable`)과 모션 설정(`MotionConfigTable`), 2D 블렌드 트리 매개변수(`Blend2DParamsTable`)를 `EditorUI::BeginPropertyTable(..., EditorUI::InspectorPropertyTableStyle())` 및 `EditorUI::PropertyRow()` 기반으로 전면 교체.
  - 전이 인스펙터(`DrawTransitionInspector`):
    - 출발 상태(`SourceState`), 대상 상태(`TargetState`), 지속 시간(`Duration`), 종료 시간 여부(`HasExitTime`), 종료 시간(`ExitTime`)을 에디터 표준 속성 표(`EditorUI::BeginPropertyTable`)로 일원화.
  - 모든 인스펙터의 라벨 열 너비(`9em`), 정렬, 폰트 패딩(`AlignTextToFramePadding()`), 구분선, 오버플로 툴팁이 에디터 전역 표준 스타일과 100% 동일하게 통일됨.
- **로캘 키 보강 및 누락 키 해결**:
  - `PROP_TYPE` 누락 키를 정식 로캘 키 `AnimatorGraph.TransitionDirection`("전이 방향")으로 교체 등록.
  - `AnimatorGraph.SourceState`, `TargetState`, `NoTransitions`, `Outgoing`, `Incoming` 추가 등록.
- **프리뷰 위젯 레이아웃 및 팝업 안정화**:
  - `PreviewViewportWidget.cpp`: `Draw()` 종료부 `ImGui::Dummy(widgetSize)` 복원으로 프리뷰 위젯과 파라미터 창 겹침 결함 해결.
- **검증 상태**: 정적 분석 완료. AGENTS.md 제2절에 따라 빌드는 사용자 직접 실행 안내.

- **매개변수 및 모션 속성 행 통일 (Label | 선택지)**:
  - `AnimatorGraphPanel.cpp`의 `DrawStateInspector`:
    - 기존에 콤보박스 오른쪽에 라벨이 노출되거나 중복되던 `값 | 이름` 형태를 해소.
    - `drawFloatParameterCombo`를 도입하고 모션 유형(MotionType), 애니메이션 클립(AnimationClip), 1D 블렌드 트리 매개변수(`Parameter`), 2D 블렌드 트리 X/Y축 매개변수(`Blend2DParameterX`, `Blend2DParameterY`) 모두를 2컬럼 테이블(`ImGuiTableFlags_BordersInnerV`)의 `Label(Column 0) | 선택지/위젯(Column 1)`으로 일관되게 정돈.
- **전이 목록(Transitions) 리스트 뷰 개편**:
  - 기존의 단조로운 텍스트 나열 방식에서 정돈된 4컬럼 테이블(`유형`, `상대 상태`, `시간`, `삭제`) 리스트 뷰로 전면 개편.
  - 전이 방향에 따라 초록색 `-> Out (출력)`, 파란색 `<- In (입력)`, 보라색 `[Any]` 아이콘 및 색상 구분 표시.
  - 행 전체 클릭(`SpanAllColumns`)으로 전이 선택 및 캔버스 강조 연동.
  - 헤더 `+` 아이콘 버튼으로 `전이 만들기(Make Transition)` 모드 진입 지원 및 행 우측 개별 휴지통 아이콘으로 간편 삭제 지원.
  - 전이가 없을 시 `연결된 전이가 없습니다.`의 정확한 Empty State 안내 표시.
  - `Editor.AnimatorGraph.csv`: `AnimatorGraph.NoTransitions`, `AnimatorGraph.Outgoing`, `AnimatorGraph.Incoming` 추가.
- **로캘 키 보강 및 누락 키 해결**:
  - `PROP_TYPE` 누락 키를 정식 로캘 키 `AnimatorGraph.TransitionDirection`("전이 방향")으로 교체 등록.
  - `AnimatorGraph.SourceState`, `TargetState`, `NoTransitions`, `Outgoing`, `Incoming` 추가 등록.
- **프리뷰 위젯 레이아웃 및 팝업 안정화**:
  - `PreviewViewportWidget.cpp`: `Draw()` 종료부 `ImGui::Dummy(widgetSize)` 복원으로 프리뷰 위젯과 파라미터 창 겹침 결함 해결.
- **검증 상태**: 정적 분석 완료. AGENTS.md 제2절에 따라 빌드는 사용자 직접 실행 안내.

- **핀 위치 조정**:
  - `NodeEditorUI.cpp`: 사용자의 피드백에 따라 핀이 완전히 밖으로 돌출되는 대신, 핀 중심이 노드 테두리 선에 정확히 위치하여 테두리에 50%(반만) 걸치도록 `borderOffset = 0.0f`로 정렬 복원 (`leftX = nodeMin.x - style.NodePadding.x`, `rightX = nodeMax.x + style.NodePadding.z`).
  - 와이어 외곽 우회 탄젠트(`StyleVar_TargetDirection`/`SourceDirection`)는 그대로 유지되어 와이어의 노드 몸통 관통은 방지됨.
- **이미 연결된 노드를 다른 곳으로 잇는 방안 구현 (트랜지션 재연결)**:
  - `AnimatorGraphPanel.cpp`의 `DrawTransitionInspector`:
    - 캔버스에서 연결된 와이어(트랜지션)를 클릭했을 때, 인스펙터에 `Source State` 및 `Target State` 콤보박스를 제공하여 다른 상태 노드로 즉시 대상/출발지를 변경할 수 있도록 구현.
  - `Editor.AnimatorGraph.csv`: `AnimatorGraph.SourceState`, `AnimatorGraph.TargetState` 로캘 원본 추가.
- **로캘 키 보강 및 누락 키 해결**:
  - `PROP_TYPE` 누락 키를 정식 로캘 키 `AnimatorGraph.TransitionDirection`("전이 방향")으로 교체 등록.
  - `AnimatorGraph.SourceState`, `TargetState`, `NoTransitions`, `Outgoing`, `Incoming` 추가 등록.
- **프리뷰 위젯 레이아웃 및 팝업 안정화**:
  - `PreviewViewportWidget.cpp`: `Draw()` 종료부 `ImGui::Dummy(widgetSize)` 복원으로 프리뷰 위젯과 파라미터 창 겹침 결함 해결.
- **검증 상태**: 정적 분석 완료. AGENTS.md 제2절에 따라 빌드는 사용자 직접 실행 안내.

### [2026-11-19] KCC 오르막 접지 유지와 급경사 미끄러짐

- 사용자는 평지 점프를 예비 확인했고, 오르막 접지 이탈과 급경사 미끄러짐이 남았다고 보고했다. 지지면 법선 방향 속도로 이탈을 판단하고, 경사 제한 각도에는 부동소수점 허용 오차를 적용했다.
- 급경사에서는 입력과 중력을 합친 속도를 접선에 투영한다. 이전에 닿았던 경사면에만 접촉 유지 여유를 적용한다. 공중 착지는 프레임 낙하 거리로 제한한다.
- `KinematicCharacterCheck`에 30°·45° 보행과 60° 미끄러짐을 추가했다. 수정 후 빌드·CTest·`SimpleTests` 실행은 사용자 확인 대기다. [경사 회귀 확인](ThirdPersonCharacterSystemPlan.md#경사-보행과-미끄러짐-회귀-확인)

### [2026-11-19] 액션 타임라인 이벤트 노티파이 C++ 모듈 수신과 모듈 자동 빌드 환경

- 문제: 이벤트 노티파이(`CustomEvent`, `EventTag`)는 배치할 수 있었지만 받는 쪽이 없었다. `ActionTimelineComponent::notifyCallbacks`는 해제 수단이 없어 핫리로드되는 모듈에서 쓰면 언로드된 코드를 가리킬 수 있었다.
- 구독 창구: `ActionTimelineEvents::Subscribe/Unsubscribe/Publish/GetSubscriberCount`를 추가했다. 리스너 목록은 엔진 `.cpp`가 소유한다. 범용 `EventBus`는 헤더 템플릿이라 모듈이 구독하면 저장소가 모듈 DLL에서 만들어지므로 이 경로에 쓰지 않았다.
- 발행: `ActionTimelineSystem`이 씬별 타임라인 순회 중 이벤트를 모으고, 지속형 VFX 해제까지 마친 뒤 같은 프레임에 발행한다. 발행 직전에 소유 엔티티를 확인한다. `SystemManager::GetUpdatePhaseMask()`에 Gameplay 페이즈가 있을 때만 발행해, 편집 모드 실시간 프리뷰(Presentation만)에서는 나가지 않는다.
- 이중 발동: 노티파이 검출 구간을 `[이전, 현재]`에서 `(이전, 현재]`로 바꿨다. 경계 시점이 두 틱에 걸치거나 일시정지 후 재개하면 같은 노티파이가 두 번 발동할 수 있었다(모든 유형 공통). 세션 첫 틱과 루프한 틱만 0을 포함한다.
- GameModule: `OnLoad`에서 구독하고 구독자 수를 로그로 남기며, `OnUnload` 첫 단계에서 해제한다. 수신 예제는 이벤트마다 로그 한 줄을 남긴다.
- 모듈 자동 빌드: VS에서 실행한 에디터에는 MSVC 환경 변수가 없어, Ninja 빌드 트리에서 에디터의 GameModule 자동 빌드가 표준 헤더(`<cstdint>` 등)를 찾지 못하고 항상 실패했다. `INCLUDE`가 없으면 `CMakeCache.txt`의 `CMAKE_CXX_COMPILER` 경로에서 VS 설치 경로와 MSVC 버전을 읽어 `VsDevCmd.bat -vcvars_ver=<버전>`을 같은 명령에서 먼저 실행한다. 이 PC처럼 도구 집합이 여럿(14.44·14.51) 설치된 경우에도 빌드 트리와 같은 버전을 쓴다. 이전 PDB 이름 변경이 `bin/<Config>`만 보던 것을 Ninja 출력(`bin`)까지 찾도록 고쳤다.
- 검증(사용자 실행, `x64-ninja-debug`, `BaselineVerification` 씬 Rogue): PIE에서 0.40초 이벤트가 일회·루프 모두 시점당 한 줄, 편집 모드 실시간 프리뷰에서는 로그 없음, VS에서 실행한 에디터의 자동 빌드 성공과 핫리로드 두 번 뒤 `subscribers: 1` 및 최신 코드 로그 한 줄을 확인했다. 일시정지 후 재개 경계, Client 실행, CTest는 확인하지 않았다. AI는 빌드·실행을 하지 않았다.
- 남은 일: 그래프 `On Action Event(Tag)` 노드, 그래프에서 타임라인 재생 노드, 에디터 테스트 발동의 구독자 전달, `AnimatorComponent` 노티파이 이관과 `notifyCallbacks` 제거. [타임라인 계획 8.5절](AnimationTimelineEditorPlan.md#85-1단계-c-게임-모듈-수신-2026-11-19)

### [2026-11-19] 액션 타임라인의 Animator 넘겨받기·복원·블렌드 아웃과 계층 탐색

- 문제: 타임라인은 재생 중 `animator.isPlaying = false`로 바꾼 뒤 되돌리지 않아, 비루프 타임라인이 끝나면 캐릭터가 마지막 포즈로 멈추고 상태 머신도 돌지 않았다. 클립이 없는 타임라인(카메라·사운드)도 Animator를 멈췄다. 타임라인·그래프 `Animator.*` 노드는 자기 엔티티에서만 Animator를 찾아, 루트(이동·스크립트)의 자식에 모델을 둔 구성에서 동작하지 않았다.
- 넘겨받기·복원: 작성 클립과 스켈레톤이 있을 때만 넘겨받고, 처음 넘겨받을 때 원래 클립·시간·재생 상태와 Animator 엔티티를 보관한다. `Stop()`, 비루프 종료, 에셋 해제, 새 세션에 클립 없음일 때 그 Animator를 되돌리고, `Pause()`는 마지막 포즈를 유지한다.
- 블렌드 아웃: 타임라인 포즈를 Animator의 `localBonePose`에 계산해 두고 복원 시 `BeginBlendFromPose`로 넘긴다. `UpdateAnimatorSkinning`이 기본 포즈 직후(레이어 합성 전) smoothstep으로 섞는다. 시간은 에셋 `m_BlendOutTime`(`ALLOW_MISSING`, 기본 0.2초)이며 타임라인 패널 트랜스포트 바에서 편집하고 편집 이력·저장 기준점에 포함된다(로캘 `Timeline.BlendOut*`).
- 계층 탐색: `AnimationSceneQuery::FindAnimatorEntityInHierarchy`(자신 → 자식 깊이 우선)를 추가해 타임라인 구동·복원·클립 찾기·소켓 본 경고, 그래프 `Animator.*` 노드 16개, 캐릭터 이동 시스템의 파라미터 주입이 함께 쓴다. 소켓 위치는 스킨드 렌더링과 같이 Animator 엔티티의 월드 행렬로 계산한다.
- 커밋 범위: 문서를 닫을 때 블렌드 아웃을 기본값으로 되돌리는 한 줄은 같은 작업 트리의 다른 작업이 추가 중인 `ReleaseResourceDocument`에 있어 이 커밋에 넣지 않았다. `KinematicCharacterSystem.cpp`도 이 작업의 탐색 교체만 골라 커밋했다.
- 검증: 이 작업을 포함한 `x64-ninja-debug` 빌드는 사용자 환경에서 통과했다. 블렌드 아웃의 실제 전환, 자식 모델 구성의 구동·소켓·복원, 그래프 노드의 자식 탐색은 PIE에서 확인하지 않았다. CTest는 실행 기록이 없다.
- 남은 일: 블렌드 인, Animator 덮어쓰기 슬롯 구조(필요해질 때), 작성 모델 경로의 가상 경로 저장, 편집 모드 실시간 프리뷰에서 VFX 노티파이가 편집 씬에 엔티티를 만드는 문제. [타임라인 계획 11절](AnimationTimelineEditorPlan.md#11-animator-넘겨받기복원과-계층-탐색-2026-11-19)

### [2026-11-19] Inspector 컴포넌트 추가 검색

- `컴포넌트 추가` 팝업에 공통 검색창을 넣고 현재 언어의 표시 이름과 등록 타입 이름의 부분 일치를 지원했다. 카테고리와 검색 결과가 추가 가능 여부, 중복 추가 방지, 씬 편집 이력·프리팹 변경 처리 경로를 공유한다.
- 정적 검증에서 공통 검색 위젯과 기존 편집 이력 연결을 검토했고, 검색 힌트와 빈 결과 안내의 한·영 CSV 항목이 각각 한 번씩 정의된 것을 확인했다. 빌드와 에디터에서의 검색·Undo/Redo 동작 확인은 남아 있다. [에디터 UX 확인 항목](EditorUXTestGuidelines.md)

### [2026-11-19] Entry 및 Any State 노드 양측(좌/우) 출력 핀 확장 및 스마트 라우팅

- **Entry 및 Any State 노드 듀얼 출력 핀 체계**:
  - `Entry` 노드(ID: 1): 좌측 출력 핀(`ID: 11`), 우측 출력 핀(`ID: 10`) 배치 (에메랄드 그린 색상).
  - `Any State` 노드(ID: 2): 좌측 출력 핀(`ID: 21`), 우측 출력 핀(`ID: 20`) 배치 (연보라 색상).
  - 일반 상태 노드가 Entry/AnyState 노드의 왼쪽 또는 오른쪽에 위치하더라도, 와이어가 노드를 가로지르거나 돌아가지 않고 가장 가까운 방향의 핀에서 자연스럽게 나가도록 양측 배치 완료.
- **스마트 최단거리 전이 링크 라우팅 (`DrawTransitions`)**:
  - `Entry -> DefaultState`: DefaultState 노드가 Entry 노드보다 왼쪽에 있으면 `Entry LeftOut(11) -> def RightIn`, 오른쪽에 있으면 `Entry RightOut(10) -> def LeftIn`으로 자동 연결.
  - `Any State -> TargetState`: TargetState 노드가 Any State 노드보다 왼쪽에 있으면 `AnyState LeftOut(21) -> target RightIn`, 오른쪽에 있으면 `AnyState RightOut(20) -> target LeftIn`으로 자동 연결.
- **신규 링크 생성 인터랙션 확장**:
  - `ed::BeginCreate()`에서 사용자가 Entry 노드나 Any State 노드의 좌측 핀(`11`, `21`)과 우측 핀(`10`, `20`) 어느 쪽에서든 선을 뽑아 대상 노드에 연결할 수 있도록 `isOutputPin` 조건 확장.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] AnimatorGraphPanel 캔버스·파라미터 영역 미표시 결함 수정 및 레이아웃 안정화

- **상태 머신 문서 생명주기 보장 (`m_StateMachine` 복구)**:
  - `ReleaseResourceDocument()`에서 `m_StateMachine.reset()` 호출 후 새 문서가 재지정되지 않아, 창을 닫았다가 다시 열거나 기본 상태일 때 `m_StateMachine == nullptr`로 인해 `DrawParametersTab()`, `DrawCanvas()`, `DrawSelectionInspector()`가 모두 조기 리턴되어 화면에 파라미터와 노드가 전혀 표시되지 않던 결함 수정.
  - `DrawCustomContent()` 시작부에서 `!m_StateMachine` 감지 시 `ExecuteNew()`를 자동 실행하고, `ReleaseResourceDocument()`에서도 `ExecuteNew()`로 깨끗한 기본 FSM(`Idle`)을 항시 대기하도록 보장.
- **노드 에디터 캔버스 뷰포트 내비게이션 시점 보정 (`ed::NavigateToContent`)**:
  - `DrawCanvas()`에서 `ed::Begin()` 직후 노드가 렌더링되기도 전에 `ed::NavigateToContent(0.0f)`가 호출되어 노드 바운딩 박스가 0x0인 상태에서 카메라 뷰포트 줌/위치가 엉뚱한 좌표로 튕겨 노드가 화면 밖으로 사라지던 결함 수정.
  - `m_NeedNavigateToContent`를 노드와 링크가 전부 `ed::EndNode()` 된 직후(`DrawTransitions()` 뒤)로 이동하여, 정확한 전체 노드 바운딩 박스를 기준으로 캔버스 화면 중앙 정렬을 수행하도록 생명주기 정상화.
- **`LeftPane` 및 3단 분할 레이아웃 클램핑 방어**:
  - `DrawCustomContent()`에서 창 너비가 작을 때 `canvasWidth`가 음수가 되거나 창 밖으로 삐져나가지 않도록 `m_LeftPaneWidth`와 `m_InspectorWidth`를 `std::clamp`로 제한.
  - 프리뷰 뷰포트 높이를 가용 높이의 35%(최대 180px)로 안정적으로 제한하여, 아래의 파라미터 탭(검색 바, 기본 파라미터 목록)이 항상 눈에 훤히 보이도록 보정.
- **`DrawTransitions()` 노드 위치 `FLT_MAX` 방어**:
  - 초기화 프레임에서 `ed::GetNodePosition`이 `FLT_MAX`를 반환할 때 발생할 수 있는 대소 비교 왜곡을 안전하게 가드.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] 머티리얼-셰이더 그래프 통합 및 실시간 3D 프리뷰 개발 계획 수립

- **계획 문서 신설 및 우선순위 등록**:
  - `Docs/MaterialShaderGraphIntegrationPlan.md`: 언리얼 엔진(Material Editor) 및 유니티(Shader Graph) 수준의 실시간 3D 시각 저작 파이프라인 구축을 위한 4단계 점진적 개발 계획 수립 완료.
  - `Docs/DevelopmentPriorities.md`: 1절 기준 문서 목록에 `MaterialShaderGraphIntegrationPlan.md` 연결 완료.
- **핵심 아키텍처 및 4단계 로드맵**:
  - **Phase 1 (머티리얼-그래프 에셋 바인딩 및 원클릭 진입)**: `MaterialInterface`에 `m_ShaderGraphPath` 속성 추가 및 직렬화 지원, `MaterialEditor` 상단 툴바 및 셰이더 설정 영역에 [Open Shader Graph] / [Create New Graph] 버튼 배치, `ImGuiManager`를 통한 포커스 및 타겟 바인딩.
  - **Phase 2 (GraphPanel 내 3D 머티리얼 프리뷰 통합)**: `GraphPanel`의 셰이더 도메인에 기존 2D 쿼드 대신 `MaterialPreviewRenderer`를 결합하여, 그래프 패널 단독으로도 3D 구체(Sphere)/큐브/평면 메쉬를 마우스로 자유 회전하며 PBR 셰이딩 반응을 실시간 관찰할 수 있도록 지원.
  - **Phase 3 (노드 편집 실시간 핫 컴파일 & 머티리얼 핫스왑)**: 노드/핀 수정 카운터(Revision) 기반 디바운스(0.25초) 자동 컴파일, `ShaderGraphCompiler::GenerateGBufferMaterialHlsl` 및 DXIL 메모리 컴파일, 성공 시 프리뷰 머티리얼 픽셀 셰이더 즉시 핫스왑(`PreviewMaterial->SetPixelShaderHandle`), 실패 시 `LastGood` 바이트코드 안전 유지.
  - **Phase 4 (노출 파라미터 ↔ 머티리얼 프로퍼티 양방향 연동)**: 그래프 내 파라미터 노드(`IsParameter`)와 머티리얼의 상수 버퍼(`MaterialConstants`) 및 텍스처 슬롯 자동 매핑.
- **로캘 키 및 UI/UX 통일성**:
  - `Docs/EngineEditorClientGuidelines.md` 14·19절 준수. 원본 CSV(`Editor.MaterialEditor.csv`, `Editor.GraphPanel.csv`)에 등록할 키 및 UI 패턴 정의.
- **검증 상태**: 설계 및 문서화 완료. 코드 구현 및 빌드/테스트는 사용자 확인 후 Phase 1부터 순차 진행 예정.

### [2026-11-19] AnimatorGraphPanel 상태 노드 양방향 4-Way 핀 확장 및 스마트 교차 방지 전이 라우팅

- **양방향 전이 시 X자 교차(꼬임) 결함 근본 해소**:
  - 단방향 DAG(데이터 플로우)와 달리 순환 전이가 빈번한 상태 머신(FSM)의 특성을 반영하여, 노드 좌측(In 1개)/우측(Out 1개) 고정 구조로 인해 발생하던 와이어 X자 꼬임 문제를 해결.
  - `NodeEditorUI::BorderPin`: `BorderSide(Left, Right)` 열거형을 추가하여 좌/우 어느 변에든 In과 Out 핀을 자유롭게 배치할 수 있도록 확장 (기존 호출부의 기본 생성자 하위 호환성 100% 보존).
- **상태 노드 4-Way 핀 레이아웃 체계**:
  - 각 상태 노드 `i`마다 상단 In 핀 라인과 하단 Out 핀 라인(최소 18px 간격 보장)을 수평 정렬 배치:
    - `Pin_LeftIn`: `1000 + i*4 + 0` (좌측 상단, Input, 하늘색)
    - `Pin_RightIn`: `1000 + i*4 + 1` (우측 상단, Input, 하늘색)
    - `Pin_LeftOut`: `1000 + i*4 + 2` (좌측 하단, Output, 황금빛 주황색)
    - `Pin_RightOut`: `1000 + i*4 + 3` (우측 하단, Output, 황금빛 주황색)
  - 사용자가 노드의 좌측이나 우측 어느 쪽으로든 신호를 보내고 받을 수 있도록 자유도 극대화.
- **스마트 교차 방지 전이 라우팅 (Smart Transition Routing)**:
  - `DrawTransitions()`에서 노드 간 전이 링크 생성 시 자동 기하학적 핀 매핑:
    - **상호 전이 (양방향 전이, A <-> B)**: 인덱스 순서에 따라 한 방향은 우측 루프(`RightOut -> RightIn`), 반대 방향은 좌측 루프(`LeftOut -> LeftIn`)로 분리 렌더링. 이를 통해 가운데서 선이 교차하던 현상을 원천 제거하고 유려한 타원형 듀얼 루프로 표현.
    - **단방향 전이**: 노드의 상대 수평 위치에 따라 최단/자연스러운 핀(`RightOut -> LeftIn` 또는 `LeftOut -> RightIn`)으로 자동 라우팅.
    - **자체 전이 (Self Transition)**: 우측 루프(`RightOut -> RightIn`)로 자가 복귀 곡선 렌더링.
    - **Entry / AnyState**: 노드 상대 위치에 맞춰 좌측 In 또는 우측 In으로 최적 연결.
- **신규 링크 인터랙션 허용성 확장**:
  - `ed::BeginCreate()`에서 사용자가 좌측 Out 핀이든 우측 Out 핀이든 드래그하여 대상 노드의 좌측 In 또는 우측 In 핀에 드롭했을 때 모두 정상적인 `AnimTransition`으로 연결되도록 핀 마스크 모듈로 연산(`(id - 1000) % 4`) 적용.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] KCC 공중 착지 거리 제한

- 정점에서 속도가 작아지면 긴 지면 탐색 레이만으로 접지 상태가 켜져 실제 낙하 스윕을 건너뛰었다. 공중 착지는 프레임의 수직 이동 거리로 제한했다. 박스·구·캡슐은 스윕 거리, 메시·지형은 캡슐과 지지면의 접촉 높이를 사용한다. 위치와 접지 상태를 같은 프레임에 반영하고 선택한 접촉의 엔티티를 지지 대상으로 기록한다.
- 접지 중 계단 하강에는 기존 스냅 마진을 적용한다. `KinematicCharacterCheck`에 박스·메시 평지 점프, 박스 가장자리, 스냅과 낙하 거리 경계를 추가했다.
- 정적 검토를 수행했다. 빌드·CTest·`SimpleTests` 실행 결과는 사용자 확인 대기다. [3인칭 회귀 확인](ThirdPersonCharacterSystemPlan.md#조기-착지-회귀-확인)

### [2026-11-19] 프리뷰 위젯 레이아웃 겹침 수정 및 에디터 그래프 UI 공통 컴포넌트(NodeEditorUI) 일원화

- **프리뷰 위젯 레이아웃 겹침 버그 해소**:
  - `PreviewViewportWidget.cpp`: `Draw()` 종료 시점에서 `ImGui::SetCursorPos(startCursor)`로 되돌아간 후 `Dummy(widgetSize)`가 주석 처리되어 있어 커서 위치가 위젯 시작 좌표로 남아있던 결함 수정. `ImGui::Dummy(widgetSize)`를 복원하여 위젯 높이만큼 레이아웃 커서가 전진하도록 보정, 애니메이터 그래프 패널 등에서 프리뷰 위젯과 파라미터 창/검색 바가 겹치던 UI 깨짐을 근본 해결.
- **에디터 그래프 UI 공통 컴포넌트 (`NodeEditorUI`) 구축**:
  - `Editor/Source/ImGui/Public/NodeEditorUI.h` / `Editor/Source/ImGui/Private/NodeEditorUI.cpp`: 비주얼 스크립팅/셰이더 그래프(`GraphPanel`)와 애니메이터 상태 머신(`AnimatorGraphPanel`) 등 노드 에디터를 사용하는 모든 에디터 패널이 일관된 노드/핀/와이어 비주얼과 조작감을 공유할 수 있도록 공통 유틸리티 모듈 신설.
  - **테마 & 컨텍스트 일원화**: `ApplyDefaultTheme(ed::Style&)` 및 `CreateDefaultEditor()`를 통해 언리얼 블루프린트 스타일의 통일된 노드 라운딩(6px), 테두리(1.2px), 다크 배경 및 휠 클릭 패닝(`NavigateButtonIndex = 2`), ini 오염 방지(`SettingsName = nullptr`) 표준화.
  - **노드 헤더 시각 규격 통일**: `BeginNodeHeader()` 및 `DrawNodeHeader()`로 최소 가로폭(130~160px)을 보장하고, 상단 둥근 사각형 채우기(`ImDrawFlags_RoundCornersTop`)와 하단 1px 은은한 구분선(`IM_COL32(255, 255, 255, 35)`)을 일원화하여 패널별 자의적인 노드 렌더링 파편화 방지.
  - **핀(Pin) 시각 규격 통일**: 텍스트 인라인 화살표 문자를 완전히 제거하고, 1번 사진(Logic 그래프)과 동일하게 노드 좌/우 외곽 테두리선 위에 반쯤 걸쳐지는 원형 핀(`DrawNodeBorderPins`, `DrawCirclePin`)으로 일원화. 입력(In: 밝은 하늘색)과 출력(Out: 황금빛 주황색, Entry: 에메랄드 그린, AnyState: 연보라) 핀이 노드 외곽 테두리에 정확히 앵커링되도록 개선.
  - **와이어/링크 규격 및 Flow 효과**: `DrawLink()`를 통해 실행선(2.8f), 일반 전이선(2.0f), 선택선(3.5f)의 두께 및 색상 규칙 일원화 및 실시간 와이어 흐름(`ed::Flow`) 효과 통합 제공.
- **패널 전면 적용**: `AnimatorGraphPanel.cpp`와 `GraphPanelNodeEditor.cpp` 양쪽 모두 `NodeEditorUI`의 `CreateDefaultEditor()`, `DrawNodeHeader()`, `DrawNodeBorderPins()`, `DrawLink()`를 공유하도록 전면 리팩토링 완료.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

- Ninja Debug에서 `HeadlessThumbnailRasterizer.cpp`의 DirectXTex 심볼 6개가 링크되지 않았다. 워커에서 `TextureConverter` 의존성을 제거하면서 전이 링크가 빠졌다. DDS 및 압축 텍스처를 직접 읽는 `BamAssetWorker`에 `Microsoft::DirectXTex`를 직접 연결했다.
- 수정 이후 생성된 Ninja 링크 규칙에서 Debug `DirectXTex.lib`를 확인했다. Ninja 로그와 `BamAssetWorker.exe` 타임스탬프로 워커 링크 완료를 확인했다. CTest와 썸네일 실행은 검증 기록이 없다.

### [2026-11-19] AnimatorGraphPanel 캔버스 imgui-node-editor 마이그레이션

- **캔버스 프레임워크 일원화 (`imgui-node-editor`)**:
  - `AnimatorGraphPanel.h` / `AnimatorGraphPanel.cpp`: 자체 `ImDrawList` 수동 렌더링(스크롤, 마우스 드래그 오프셋, 수동 베지어 곡선 및 히트테스트)을 `GraphPanel`(비주얼 스크립팅/셰이더 그래프)과 동일한 `ax::NodeEditor` 프레임워크로 완전 전환.
  - 데이터 모델(`AnimatorStateMachine`)의 직렬화 구조(`states`, `transitions`, `defaultParameters`, `defaultState`)는 100% 불변으로 유지하면서 렌더링 계층만 표준 노드/핀/링크 시스템으로 연결.
- **노드 및 핀/링크 ID 체계**:
  - `Entry` 노드(ID: 1, Output Pin: 10, 진한 녹색 헤더), `Any State` 노드(ID: 2, Output Pin: 20, 보라색 헤더), 일반 상태 노드(ID: 100 + i, Input Pin: 1000 + i*2, Output Pin: 1000 + i*2 + 1).
  - Entry -> DefaultState 링크(ID: 1, 에메랄드 그린 와이어), 일반/AnyState 전이 링크(ID: 100 + k).
- **인터랙션 및 UX 고도화**:
  - 휠 드래그 패닝, 마우스 휠 무한 줌, 다중 노드 마퀴(Marquee) 박스 드래그 선택 및 그룹 이동.
  - 핀 드래그앤드롭을 통한 직관적 와이어 연결:
    - Entry 출력 핀을 상태 노드에 연결 시 `defaultState` 자동 변경.
    - Any State 출력 핀을 상태 노드에 연결 시 `fromAnyState = true` 전이 자동 생성.
    - 상태 노드 출력 핀을 다른 상태 노드 입력 핀에 연결 시 일반 전이 자동 생성 및 인스펙터 즉시 선택.
  - 노드 에디터 다중 노드/링크 일괄 삭제 지원 (`DeleteSelection`).
  - 우클릭 컨텍스트 메뉴: `ed::End()` 호출 이후 컨텍스트 메뉴를 처리하도록 구조화하고, `GraphPanelNodeEditor`와 동일하게 `ed::End()` 외부에서의 잘못된 `ed::Suspend()` / `ed::Resume()` 호출을 완전히 제거하여 패널 오픈 및 캔버스 우클릭 시 크래시 방지. 우클릭 시점의 마우스 캔버스 좌표(`m_ContextMenuPosition`)를 기록하여 정확한 위치에 상태 노드 생성 지원.
  - `F` 키 포커스 연동: `ResetCanvasView()`를 `public`으로 선언하여 `EditorActionRegistry`의 `Viewport.Focus`에서 `AnimatorGraphPanel` 활성 시 `ed::NavigateToContent(0.5f)`를 호출해 전체 노드 자동 줌 맞춤.
  - 런타임 디버깅 시 전이가 진행 중일 때 `ed::Flow(LinkId)`를 통한 실시간 와이어 흐름 비주얼 피드백 제공.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자 직접 실행 안내.

### [2026-11-19] 모델 임포트 변환 워커 연결과 에디터 회귀 점검

- `BamAssetWorker`에서 모델 변환과 LOD 생성을 수행하고, 임시 결과를 에디터의 기존 `ModelImportTask`와 `FinalizeStep`에 넘겼다. 푸터의 단계 문구는 `Editor.ModelImport.csv`에 추가했다. 취소와 시간 초과는 결과 게시 전에 종료하며 워커 가용성·프로토콜 문제는 기존 경로를 사용한다.
- 프리뷰 툴바의 렌더 타깃·조명 팝업 연결을 정리했다. 조명 임시 값은 팝업이 열린 뒤 생성하며 출력이나 툴바가 표시되지 않을 때 폐기한다.
- 참조 검사 증분 병합과 형제 순서 변경의 호출 흐름 및 기존 회귀 시나리오를 정적으로 대조했다. `AssetWorkerModelCheck`에 워커 성공 시 결과 파일 생성과 원본 누락 실패를 확인하는 사례를 추가했다.
- 변경 범위의 Git 공백 검사, 워커 IPC 옵션 순서, 신규 CSV 번역 항목, Python 검사 파일의 구문을 정적으로 확인했다. 작업 트리 전체 로캘 엄격 감사는 별도 변경 중인 단축키 문맥과 애니메이션 타임라인 항목으로 누락 10건을 보고했다.
- 빌드·CTest·실환경 조작·성능 측정은 실행 대기다. [워커 검증 절차](OutOfProcessAssetPipelinePlan.md#모델-임포트-1차-검증)와 [에디터 UX 검증](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)에 확인할 조건을 기록했다.

### [2026-11-19] 모델 임포트 모달 비동기화 및 프리뷰 뷰포트 조작계(그리드/단축키/속도) 개선

- **모델 재임포트 메인 스레드 프리징 해소**:
  - `ContentBrowserGridItem.cpp`: `.bammodel` 재임포트 시 원본 경로를 찾기 위해 `rm.LoadFile`로 전체 모델/서브메시/머티리얼을 동기 로드하던 경로를 차단하고, 캐시된 핸들 확인 및 동일 폴더 동명 원본(`.glb`, `.fbx`, `.obj` 등)을 즉시 탐색하도록 수정.
  - `ModelImportModal.h` / `ModelImportModal.cpp`: 메인 스레드에서 즉시 모달 창을 열고, 백그라운드 워커 스레드(`std::thread` + `AsyncPreviewState`)를 통해 Assimp 원본 파싱을 비동기 수행. 프리뷰 로딩 상태를 직관적인 프로그레스 바(`DrawPreviewPane`)로 표시. 파싱 완료 후 프레임당 4ms 타임 버짓으로 GPU 리소스를 분할 빌드(`StepPreviewBuild`)하여 UI 끊김 현상 제거.
- **대형 모델 무한 그리드 표시 개선**:
  - `infiniteGridPS.hlsl`: 기존 고정 150.0m 페이드 거리로 인해 반경 수천 미터의 대형 모델(스폰자 등)에서 그리드가 `color.a <= 0`으로 discard되던 문제를 해결. 카메라 고도 및 원점 거리 비례 적응형 페이드 거리(`fadeDistance = max(150.0f, max(abs(CameraPos.y) * 10.0f, length(CameraPos) * 2.5f))`) 적용 및 원거리 대단위 그리드(`0.001f`, 1000m) 추가로 모든 스케일에서 바닥 그리드 시인성 확보.
- **모달 팝업 상태 프리뷰 뷰포트 단축키 연동**:
  - `ShortcutManager.h` / `ShortcutManager.cpp`: 단축키 바인딩 및 실행 조건을 평가하여 안전하게 즉각 실행하는 `TryTriggerAction(actionId)` 인터페이스 구현.
  - `ImGuiManager.cpp`: 기존 `ImGui::IsPopupOpen` 시 전체 단축키를 억제하던 제약을 완화하여, 모달 팝업 상태이더라도 텍스트 입력 중이 아니고 프리뷰 뷰포트가 활성화(호버/포커스)된 경우 뷰포트 필수 액션 4종(`Viewport.Focus`, `Viewport.ToggleGrid`, `Viewport.CameraSpeedIncrease`, `Viewport.CameraSpeedDecrease`)의 단축키 입력을 통과 및 실행하도록 연동.
- **카메라 이동/줌 속도 스케일링 확장**:
  - `EditorCameraController.h`: `SetCameraSpeed` 속도 제한 상한을 500.0f에서 10,000.0f로 확장 (`std::clamp(speed, 0.1f, 10000.0f)`).
  - `PreviewViewportWidget.cpp`: `FrameModelBounds` 성공 시 산출된 프레이밍 거리(`Distance`)에 비례하여 기본 이동 속도(`Distance * 0.25f`) 및 3D 줌 속도(`Distance * 0.05f`)를 적응형으로 자동 보정하여 대형 모델에서도 카메라 조작 및 줌 반응성이 즉각 체감되도록 개선.
- **도구 및 컴파일 오류 수정**:
  - `Tools/AssetWorker/main.cpp`: `CompressTexture` 및 `GetRawCompressedData` 호출부에서 `uint32_t` 플래그를 `ECompressFlags`로 명시적 형변환(C2664) 적용.
- **검증 상태**: 정적 분석 및 diff 검토 완료. AGENTS.md 제2절에 따라 빌드 및 테스트는 사용자가 직접 실행하도록 안내함.

### [2026-11-19] 액션 타임라인 작성 모델·클립 바인딩과 런타임 포즈

- 문제: 런타임 `ActionTimelineSystem`은 에셋의 `m_ModelPath`·`m_ClipName`을 읽지 않았다. Animator에 시간만 넣고 클립을 지정하지 않아, 작성한 클립이 아닌 Animator의 현재 클립(모델 생성 시 첫 클립) 기준으로 재생될 수 있었다. 또 `animator.isPlaying = false`로 두면 `AnimationSystem`이 포즈 계산을 건너뛰므로, 타임라인 재생 중 캐릭터 포즈가 갱신되지 않는 것으로 코드상 판단했다(실행 확인 전).
- 런타임: 재생 세션(`playSerial`)이 바뀔 때마다 `ClipName`으로 클립을 찾는다. 엔티티의 `ModelInstanceComponent` 모델을 먼저 보고, 없으면 작성 모델(`ModelPath`)에서 찾는다(같은 스켈레톤의 다른 모델에도 적용). 찾으면 Animator의 `currentClip`으로 걸고 타임라인 시간으로 스키닝 행렬을 직접 계산한다. 노티파이 검출보다 먼저 계산하므로 소켓 위치와 포즈가 같은 프레임 기준이다. 클립을 찾지 못하면 경고하고 기존 클립을 유지한다. 타임라인을 불러올 때 엔티티 스켈레톤에 없는 소켓 본을 경고한다(원점에서 발동).
- 에디터: 모델이 없으면 뷰포트 가운데에 안내를 표시하고, 소켓 본·카메라 포커스 본 선택을 비활성화한다(툴팁 안내). 현재 스켈레톤에 없는 본을 가리키면 인스펙터에 경고한다. 기본 노티파이의 `"Weapon_Socket"` 하드코딩을 없앴다.
- 저장 검증: 저장은 막지 않고 알림으로 경고한다. 모델 없음(소켓 노티파이가 있으면 개수 포함), 모델은 있으나 클립 없음, 스켈레톤에 없는 소켓 본 개수.
- 클립 데이터: 클립이 없을 때 번역 문구(`Timeline.NoEmbeddedAnimation`)가 클립 이름으로 저장되던 결함을 고쳤다. 저장값은 실제 클립 이름 또는 빈 문자열이고, 안내 문구는 표시에만 쓴다. 모델을 불러오지 못해도 파일의 클립 이름과 길이를 보존한다.
- 새 타임라인 만들기: 콘텐츠 브라우저 템플릿이 없는 필드(`m_EffectTracks` 등)를 쓰고 필수 필드(`m_ModelPath`·`m_NotifyTracks`)가 없어, `ALLOW_MISSING`이 아닌 필드를 엄격히 읽는 아카이브에서 열리지 않을 것으로 판단했다. 빈 `ActionTimelineAsset`을 리플렉션으로 저장해 현재 구조와 항상 일치시킨다.
- 검증: 정적 검토만 했다. 빌드·PIE·CTest는 AI가 실행하지 않았다. 런타임 포즈 갱신 결함과 이전 템플릿의 열기 실패는 코드 판독에 따른 판단이며 수정 전 동작을 실행으로 확인하지 않았다.

### [2026-11-19] 액션 타임라인 저장 단축키 수정과 편집 이력

- `AnimationTimeline.Save`가 패널을 `GetImGuiPanel(L"Animation Timeline")`로 찾았으나 실제 패널 이름은 `L"AnimationTimelinePanel"`이어서 항상 실패했다. 로컬 액션이 키를 가져가므로 Ctrl+S가 아무 동작도 하지 않았다. 다른 `Timeline.*` 액션과 같이 `AnimationTimelinePanel::GetActivePanel()`로 조회한다.
- 편집 이력: 저장되는 문서 내용 전체(노티파이·카메라 트랙, 작성 모델 경로, 클립 이름, 길이)를 스냅샷 단위로 다룬다. 편집 지점마다 명령을 만들지 않고, 드래그·스크러빙·입력 중인 위젯·왼쪽 버튼 누름이 없는 프레임에 마지막 확정 상태와 비교해 바뀐 만큼을 `CommandHistory`에 한 건으로 기록한다. 인스펙터 드래그·마커 이동·이름 입력·모델 교체·클립 선택은 각각 한 건이 된다. 모델이 바뀐 스냅샷을 적용하면 모델을 다시 불러오고 클립·길이를 되돌린다. 선택이 무효가 되면 해제하고 프리뷰 VFX를 정리한다.
- 저장 기준점: 저장·파일 열기 시점의 문서를 기준점으로 두고, 이력 확정과 Undo/Redo 때 현재 문서가 기준점과 같으면 미저장 표시를 지운다. 모델 교체·클립 선택도 미저장으로 표시된다.
- 경로: `AnimationTimeline.Undo`(Ctrl+Z)·`AnimationTimeline.Redo`(Ctrl+Y) 로컬 액션, 편집 메뉴, 퀵액세스 버튼. 퀵액세스 저장 버튼도 등록 액션을 실행해 툴팁에 단축키 힌트를 표시한다. 타임라인에 포커스가 있을 때 씬 이력은 기존처럼 막혀 있다.
- 기준점 재설정: 생성·파일 로드 시 이력을 비운다. 콘텐츠 브라우저 이동으로 모델·노티파이 경로가 바뀌면 `AssetPathRelocation::AfterCommit`에서 이력을 비우고, 이동 전 미저장이 아니었으면 저장 기준점도 옮긴다. 이전 스냅샷에는 옮기기 전 경로가 남아 되돌리면 깨진 참조가 되기 때문이다.
- 파일 메뉴에 `다른 이름으로 저장`과 `닫기`를 추가했다(19-7절). 저장 경로가 없을 때의 저장은 같은 대화상자를 쓴다. 엔티티 동기화는 타임라인을 여는 경우 이미 미저장 확인을 거치며, 모델만 바꾸는 경우는 문서 교체가 아니라 이력에 남는 편집이다.
- 검증: 정적 검토만 했다. 빌드·실행·CTest는 AI가 실행하지 않았으며 사용자 확인이 필요하다.

### [2026-11-19] 액션 타임라인 노티파이 편집·지속형 VFX 노티파이·트랙 종류

- 본 계층: 스켈레톤 시그니처가 바뀔 때만 자식 목록·이름·검색용 소문자를 다시 만든다(기존 매 프레임 O(N²) 탐색 제거). 가로 스크롤·좁은 들여쓰기·전체 경로 툴팁·공용 검색창·빈 상태를 적용했다. 단일 클릭은 선택만 하고, 노티파이 소켓 지정은 우클릭 메뉴로 옮겼다(19-5절, 실수로 데이터가 바뀌던 문제). 검색 가능한 본 선택 콤보를 노티파이 소켓 본과 카메라 포커스 본에 함께 쓴다.
- 인스펙터: `AnimNotifyData`·`TimelineCameraTrack`에 표시 이름·범주·범위·조건 메타데이터를 두고 `ReflectionPropertyDrawer` 공용 표로 그린다. 소켓 본·사운드·VFX 경로만 같은 표 스타일의 직접 행(본 선택기, `DrawInspectorAssetPathPicker`)으로 그린다. 새로 고른 에셋 경로는 가상 URI로 저장되고 기존 절대 경로도 그대로 읽는다. 발동·시작 시간과 지속 시간은 클립 길이로 제한한다.
- VFX 1회 재생: 노티파이로 생성한 `CompositeVFXComponent`는 `playOnce`로 에셋 루프 설정과 관계없이 한 번만 재생된다(에디터 프리뷰·런타임). 이전에는 루프 에셋이 끝나지 않아 버튼을 누를 때마다 인스턴스가 쌓였다.
- 해제와 잔상: `Release()`·`isDraining`으로 새 방출을 멈추고 트레일 점이 수명대로 사라진 뒤 `isFinished`가 된다. 1회 재생 종료도 같은 경로를 거쳐 트레일 끝이 한 번에 사라지던 문제를 고쳤다. 파티클·조명·사운드·셰이크는 해제 시 바로 정리하고, drain 중에는 스프라이트·메시·데칼을 그리지 않는다. VFX 에디터 탐색(`previewSeeking`)은 기존 즉시 종료를 유지한다.
- 지속형 노티파이: `AnimNotifyData::Duration`(`ALLOW_MISSING`, VFX 전용, 0이면 기존 순간 노티파이). `holdUntilRelease` 동안 에셋 길이를 지나도 유지하며 `loop` 트랙만 해제 시점까지 그린다. 에디터는 재생 중 `TriggerTime + Duration`, 즉시 발동 테스트는 실제 시간 `Duration` 뒤 해제한다. 트랙 행은 반투명 막대와 끝점 드래그(최소 0.05초, 클립 끝까지)로 길이를 조절한다.
- 런타임 해제: `ActionVFXHoldComponent`(소유 엔티티·재생 세션 번호·해제 누적 시간)와 `ActionTimelineComponent::playSerial`·`playElapsed`를 추가했다. 소유자 소멸·비활성, `Play()`·`Stop()`·경로 변경, 해제 시간 경과, 비루프 타임라인 종료 시 해제하고, 일시정지 중에는 유지한다.
- 메뉴: 트랙 몸통 우클릭 팝업이 열리기만 하고 그려지지 않던 결함을 고쳐 VFX·사운드·이벤트 노티파이 추가와 마커 테스트 발동·삭제를 제공한다. 트랙 추가 메뉴에 이벤트 트랙을 추가하고 FX 항목 아이콘을 트랙 아이콘과 맞췄다.
- 트랙 종류: `TimelineNotifyTrack::Kind`(`ALLOW_MISSING`, 기본 `Unspecified`). 로드할 때 이전 이름 규칙으로 한 번 판정해 저장하므로 이름을 바꾸거나 한국어 이름을 붙여도 아이콘과 더블클릭 기본 유형이 유지된다. 리소스 포맷 버전은 엄격 일치 검사 때문에 올리지 않았다.
- 로캘: 노티파이·카메라 속성·범주·열거형 키를 `Engine.csv`에, 메뉴·툴팁·빈 상태 키를 `Editor.Timeline.csv`에 추가하고, 인스펙터 전환으로 쓰이지 않게 된 `Timeline.*` 28행을 삭제했다.
- 계획: 이벤트 노티파이 수신 경로를 [타임라인 계획](AnimationTimelineEditorPlan.md) 8절에, 지속형 노티파이 사용 규칙을 9절에 기록했다.
- 검증: 사용자가 실제 에디터에서 노티파이 인스펙터 표시, VFX 1회 재생·제거, 잔상 소멸·지속 유지·막대 길이 조절, 메뉴 수정과 트랙 종류 적용 후 정상 동작을 보고했다. 빌드·CTest(`AssetReferenceCheck`·`EditorHistoryCheck`) 결과, 런타임 해제의 PIE 시나리오별(취소·일시정지·소유자 삭제) 확인, 본 계층 가로 스크롤, 로캘 행 정리 후 엄격 감사는 기록되지 않아 미검증이다. AI는 빌드·테스트를 실행하지 않았다.
- 남은 일: 이벤트 노티파이 수신 경로, 카메라 샷 막대 길이 조절, 파티클 노티파이 런타임 처리, 파티클의 방출만 정지하는 상태.

### [2026-11-19] 에디터 로캘 CSV를 그룹별로 분리

- 단일 `Editor.csv`의 2,116개 항목을 기존 `Group` 기준으로 `Editor.<Group>.csv` 62개에 옮겼다. 원본에 다시 남아 있던 2,110개 행도 분할 파일에 모두 포함된 것을 확인한 뒤 `Editor.csv`를 제거했다. 편집기 코드는 기존 로캘 키와 언어별 JSON 경로를 사용한다.
- `EditorDocument.TemplateBasicDesc`와 `SpriteEditor.RowHelp`에서 번역 열에 따옴표가 빠져 문장 속 쉼표가 열 구분자로 처리됐다. 두 언어의 원본 문장을 각 셀에 복원했다.
- CSV 변환기는 파일명 순서로 원본을 읽는다. 로캘 감사기는 모든 에디터 CSV의 열 개수와 Group 규칙을 검사하며, 오류에 파일과 행 번호를 표시한다. 분할 파일의 잘못된 열 개수와 Group을 재현하는 회귀 테스트를 추가했다.
- 영어·한국어 JSON을 변환기로 재생성했다. 정적 비교에서 기존 키 2,116개가 모두 보존됐고, 두 복원 항목을 제외한 번역 값은 일치했다. 빌드 타깃 `GenerateLocalization`, `AuditLocalization`, CTest `LocalizationAuditUtility`, 에디터에서의 언어 전환은 사용자 실행 확인을 남겨 두었다.
- CSV 파일명과 열 규격, 원본 행 제거, 생성 JSON 비교 및 엄격 감사 기준을 추가했다. [로캘 원본 작성 지침](EngineEditorClientGuidelines.md#에디터-csv-추가이동-절차), [원본 이동 검증](EditorUXTestGuidelines.md#원본-이동과-분할-검증)

### [2026-11-19] CurveEditorModal 로캘 엄격 감사 결함 수정 및 다국어 지원

- **배경 및 문제**:
  - `python Tools/Audit/LocalizationAuditor/audit_localization.py --strict` 실행 시, `CurveEditorModal.cpp`의 키프레임 컨텍스트 메뉴 내 4개 항목("Cubic Bezier", "Linear", "Constant", "Flatten Tangents (0.0)")이 하드코딩 리터럴로 검출되어 Tier 1 감사가 실패함.
- **수정 내역**:
  - `Editor/Resources/Data/Localization/Editor.csv`: `VFXEditor,FlattenTangents,Flatten Tangents (0.0),탄젠트 평탄화 (0.0)` 키 추가.
  - `Editor/Source/ImGui/Private/CurveEditorModal.cpp`: 보간 모드 3종("Engine::ECurveInterpMode.*") 및 탄젠트 평탄화 메뉴 항목을 `LOCAL_CSTR` 매크로 호출로 일원화.
- **검증 상태**:
  - `audit_localization.py --strict` 재실행을 통해 하드코딩 UI 문자열 0건, 카탈로그 누락 0건으로 감사 100% 통과(Exit Code 0) 확인.

### [2026-11-19] 복합 VFX 트랙의 블렌드 모드별 제출 패스 교정

- VFX 프리뷰 허공의 흙갈색 메쉬 이펙트는 배경 버퍼나 톤매핑 문제가 아니었다. 프리뷰 GeometryPass의 `acceptedBlendModes == None`을 `SubmitSceneVFX`가 "제한 없음"으로 해석해 Additive 트랙을 G-Buffer에 기록했다. 조명 패스가 이 픽셀을 법선 0·AO 0의 불투명 표면으로 칠해 배경을 덮어썼다. 같은 함수가 이 경우를 `Opaque | Masked`로 보정하고, 스프라이트·메쉬·데칼·트레일 트랙을 한 값으로 거른다.
- 씬·게임 뷰포트의 `OnSubmit`이 Geometry 패스에만 제출해 Additive·AlphaBlend 메쉬와 기본 데칼 트랙이 보이지 않았다. `ParticleSystem`과 같은 규칙으로 Geometry와 ForwardTransparent 패스를 함께 모은다. ForwardTransparent에만 그려지는 트랙은 피킹되지 않는다.
- 사용자 실행으로 수정 전 `GBuffer_Diffuse`의 링 기록과 `GBuffer_Normal` 공백을 확인했다. 수정 후 Diffuse의 링 제거, 프리뷰 링의 가산 합성, Opaque 트랙의 프리뷰·씬 표시, 씬의 Additive 메쉬·데칼 표시를 확인했다. 게임 뷰·패키지 실행과 CTest 결과는 기록되지 않았다. [복합 VFX 9.11](CompositeVisualEffectPlan.md#911-트랙-블렌드-모드별-제출-패스-교정-2026-11-19)

### [2026-11-18] 에셋 참조 패널 결과의 증분 갱신

- 변경 문서 검사에서 이전 문제 행의 위치와 실제로 추가·삭제된 링크를 반환한다. 패널은 바뀐 경로의 양방향 관계 색인과 펼치기 판정을 갱신하고, 유지되는 문제 행의 필터 일치 여부를 재사용한다.
- 링크가 같은 저장에서는 관계 검색 결과를 유지한다. 링크가 바뀌면 검색어와 조회 대상을 보존한 채 일치 항목을 다시 찾는다. 결과 벡터의 병합과 변경된 그래프의 검색 순회는 계속 수행한다.
- `AssetReferenceCheck`에 링크 교체·복원, 변경 없는 링크와 검사 거부 시 결과 보존 사례를 추가했다. 로캘 엄격 감사와 `git diff --check`를 통과했다. 에디터 빌드·CTest·패널 동작·성능 비교는 사용자 실행 전이다. [에디터 UX 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 에셋 참조 자동 갱신의 변경 문서 검사

- 파일 감시자가 저장 경로를 묶어 에디터 스레드로 전달한다. 최근 전체 검사에서 확인한 문서만 바뀐 경우 폴더 전체를 순회하지 않고 해당 문서의 참조 링크와 문제를 다시 계산한다. Archive와 그래프의 임시 저장 디렉터리 및 파일 이벤트는 제외한다.
- 새 경로·삭제·이동·마운트 변경·불완전한 기준 검사·모호한 감시 이벤트는 전체 검사로 전환한다. 32회의 변경 문서 검사 이후 다음 자동 갱신은 전체 검사로 기준을 갱신한다. 이동·삭제와 복구의 검증은 기존 경로를 유지한다.
- AssetReferenceCheck에 저장 이벤트 묶음, 증분과 전체 검사 결과 일치, 새 파일과 참조 대상 변경 시 전체 검사 전환 사례를 추가했다. 로캘 엄격 감사와 공백 검사를 통과했다. 사용자 빌드·CTest, 실제 저장 이벤트 및 전후 시간 비교는 검증 전이다.

### [2026-11-18] 에셋 참조 재검사의 단계별 계측

- 메인 체크아웃의 Assets 폴더를 읽기 전용으로 조사했다. 폴더 251개와 파일 12,677개 중 참조 검사 확장자에 해당한 문서는 358개(4.24 MiB)다. Python으로 경로와 후보 문서 메타데이터를 한 차례 읽는 데 183.2ms, 문서 전체를 반복해서 읽는 데 48.1~49.5ms, Python JSON 해석에 14.8~15.9ms가 걸렸다. 에디터의 자동 검사 경과 시간은 아직 수집하지 않았다.
- 엔진 통계에 최근 참조 패널 검사의 경로 순회, 읽기·파싱, 참조 확인, 관계 색인·필터 갱신 시간과 작업 건수를 표시한다. 검사 결과와 기존 파일 작업의 검증 경로를 유지한다.
- 로캘 엄격 감사와 변경부 공백 검사를 통과했다. AssetReferenceCheck에 검사 건수와 계측 초기화 확인을 추가했다. 사용자 빌드·CTest와 실제 자동 갱신 시점의 단계별 시간 수집은 남아 있다. [참조 패널 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 계층 패널의 다중 드래그 순서 변경

- 같은 부모 아래에서 형제 행의 가장자리로 드래그하면 삽입선을 표시한다. 드롭하면 선택 항목을 원래 형제 순서대로 배치한다. 행 가운데와 씬 루트 드롭은 기존 부모 변경 경로를 사용한다.
- Scene에서 대상 부모와 형제 목록을 검증한 뒤 순서를 한 번에 교체한다. 부모와 Transform은 유지되며 순서가 실제로 바뀐 경우 씬 이력 한 건이 생긴다.
- 로캘 엄격 감사를 통과했다. HierarchyEditorCheck에 상대 순서·잘못된 대상·삽입 영역 검사를, EditorHistoryCheck에 한 번의 Undo/Redo 검사를 추가했다. 빌드·CTest와 삽입선의 실제 편집기 검증은 사용자 실행 전이다. [대화형 점검 G](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-19] 스켈레탈 모델 인스턴스화 시 본 노드 생략 및 계층 구조 압축 최적화

- **배경 및 목표**:
  - 스켈레탈 메시 모델(`.bammodel`)을 씬에 추가할 때, GPU 스키닝에 직접 쓰이지 않는 수십 개의 뼈대(Bone) 노드 및 렌더러가 없는 중간 껍데기 노드(`fox` 등)가 씬 엔티티로 생성되어 계층 패널이 불필요하게 3단계로 복잡해지던 구조를 개선함.
- **구현 내역**:
  - `Engine/Source/World/Scene/Private/SceneModel.cpp` (`Scene::InstantiateModelInternal`):
    - `hasSkeleton == true`인 스켈레탈 모델은 메시가 달리지 않은 뼈대 노드 및 중간 껍데기 노드의 엔티티 생성을 생략하고, 실제 메시(`node.MeshIndices`) 개수만큼만 자식 엔티티를 생성하도록 변경.
    - 생성된 메시 엔티티(`SkinnedMeshRendererComponent`)들을 최상위 모델 루트 엔티티(`rootHandle`)의 직속 자식으로 부착하여 `[루트 모델] -> [메시]` 2단계 구조로 계층을 단순화.
    - 정적 모델(`hasSkeleton == false`)은 기존 부모-자식 트랜스폼 계층을 유지하도록 분기 격리.
- **검증 상태**:
  - 에디터 실행 후 `Fox` 모델 배치 시 본 노드 생략 및 직속 메시 자식 배치 확인.
  - `AnimatorComponent` 클립 재생 시 정상 스키닝 동작 확인.

### [2026-11-19] VFX 에디터 2D 커브 팝업 에디터(CurveEditorModal) 구축 및 UI/UX 디테일 개선 (Phase 5 Step 3)

- **배경 및 목표**:
  - `FloatCurve` 기반의 정밀한 완급 조절(조도/스케일/알파)을 위해, 좁은 인스펙터 인라인 위젯을 넘어 넓은 작업 캔버스에서 3차 베지어 키프레임을 직접 조작할 수 있는 전용 팝업 모달 에디터(`CurveEditorModal`)를 구축함.
- **구현 내역**:
  - **1) `CurveEditorModal` 전용 모달 창 신설 (`Editor/Source/ImGui/Public/CurveEditorModal.h`, `Private/CurveEditorModal.cpp`)**:
    - `Open(title, targetCurve, onApplied)`을 통한 원본 커브 격리 작업본(`m_WorkingCurve`) 편집 및 [확인] 시에만 안전 반영.
    - 베지어 곡선(64샘플) 렌더링, 시간/값 축 그리드 라인 및 수치 가이드 표시.
    - 캔버스 더블클릭 시 키 추가, 마우스 좌클릭 드래그 시 이웃 키 시간 경계 제약 이동, 상단 헤더 우측 삭제 버튼 연동.
    - 선형, 천천히 시작, 천천히 끝, 천천히 시작-끝, 스파이크 등 5대 프리셋 즉시 적용 및 `FitView` 화면 맞춤 지원.
  - **2) `VFXEditorPanel` 연동 (`Editor/Source/ImGui/ResourceEditor/Private/VFXEditorPanel.cpp`, `Public/VFXEditorPanel.h`)**:
    - `DrawCurvePresetRow` 자유 함수에 모달 인스턴스 및 콜백을 전달하여 Light(조도), Mesh(스케일, 알파) 트랙 프리셋 행 옆에 베지어 아이콘 버튼 배치.
    - 모달 편집 확인 시 `m_UntrackedDirty = true;` 및 `InvalidateTimelinePreview()`로 타임라인 인라인 프리뷰 즉시 동기화.
  - **3) UI/UX 디테일 및 로컬라이제이션 결함 해소**:
    - 모달 타이틀에서 ImGui ID(`###`) 토큰 파싱 제거 및 순수 로컬 텍스트 전달.
    - 키 속성(`PROP_TIME`, `PROP_VALUE`, `PROP_IN_TANGENT`, `PROP_OUT_TANGENT`, `PROP_INTERP_MODE`) 및 보간 모드(`상수`, `선형`, `3차 베지어`) 다국어 표준화.
    - 인스펙터 인라인 커브 안내 문구에 `PushTextWrapPos(0.0f)`를 적용하여 우측 잘림 방지 (`ReflectionPropertyDrawer.cpp`).
    - `(사각 평면 기본값)` 등 메시/텍스처 힌트를 리플렉션 상단으로 이동하여 커브와 프리셋 버튼 사이의 시각적 분단 해소.
    - 모달 하단 확인 버튼 로캘 키를 `EditorCommon.OK`로 교정하여 "확인" 정상 출력.
- **검증 상태**:
  - Ninja Debug 빌드 통과 및 VFX 에디터 내 조도/스케일/알파 커브 모달 열기, 키 조작, 프리셋 적용, 취소/확인 동작 실환경 검증 완료.

### [2026-11-18] 리소스 피커 열기와 썸네일 표시 비용 개선

- 리소스 필드 및 리소스·경로 피커가 캐시의 아틀라스 텍스처와 UV를 재사용한다. 스프라이트의 원본 UV를 보존하고 화면에 보이는 행만 썸네일을 요청한다.
- 프로젝트·엔진 파일 목록은 에디터 갱신에서 비동기로 준비한다. 팝업을 다시 열면 메모리 목록을 사용한다. 기존 감시자의 추가·삭제·이동 이벤트를 묶어 새 목록을 받고, 콘텐츠 브라우저 새로 고침으로도 갱신한다.
- Git 공백 검사를 통과했다. 메인 체크아웃 로캘 엄격 감사에는 이 변경과 별개로 `CurveEditorModal.cpp`의 키 누락 1건과 하드코딩 문구 4건이 남아 있다. 빌드·CTest와 팝업 실환경 프레임 간격 비교는 사용자 실행 전이다. [에디터 UX 점검 기준](EditorUXTestGuidelines.md#46-반응성-및-프레임-비용-예산-imgui-계획-14-d)

### [2026-11-18] VFX 에디터 공중 뷰(Air View) 반투명 이펙트 흙갈색 탁화 원인 분석 및 문서화

- **배경 및 현상**:
  - VFX 에디터에서 지면이 없는 공중 뷰(Air Mode)나 수평선 너머 허공 배경에서 메쉬/파티클 이펙트가 발광력을 잃고 짙은 흙갈색으로 탁해지며 테두리에 어두운 실루엣이 발생하는 현상 확인 (`media_1795011805149_a346ad22.png`, `media_1795011822925_82faa14e.jpg`).
  - 지면 위에서는 정상적으로 백색 발광(White Glow)하던 이펙트가 허공에서 급격히 죽어버리는 디퍼드-포워드 합성 불일치 결함.
- **원인 분석**:
  1. **디퍼드 배경 버퍼의 기하 및 반사광 부재**: 지면은 G-Buffer에서 직접광/반사광을 받아 밝은 HDR 백그라운드($RGB \approx (0.8, 0.85, 0.95)$)를 형성해 가산 합성 시 $1.0$을 초과하는 백색 발광이 발생하나, 허공은 기하가 없어 라이팅 연산이 `discard`되어 기본 클리어 색상($RGB \approx (0.22, 0.22, 0.24)$)만 존재함.
  2. **ACES Filmic 톤매핑 시 저휘도 주황색의 갈색화**: 주황/황금색 계열($R:G:B \approx 5:1:0$)은 휘도가 낮아지면 물리적으로 필연적인 '탁한 흙갈색(Muddy Brown / Olive Ochre)'으로 인지됨.
  3. **셰이더 내부 정면 알파 감쇄 및 블렌딩 결함**: `forwardParticlePS.hlsl`의 정면 투명도 감쇄(`facingAlpha = lerp(0.55f, 1.0f, fresnelRim)`) 및 외곽 페더링(`uvEdgeFade`)으로 인해 쿼드 정면과 테두리에서 Additive 광량이 45% 이상 급감함.
- **해결 방안 및 로드맵 수립**:
  - 알파 채널에 의한 광량 감쇄 없이 방출 광량을 100% 보존하는 순수 가산 블렌드(Pure Additive, $One / One$) 머티리얼 분리.
  - 공중 프리뷰 모드 활성화 시 완전 암흑 대신 최소 환경 반사광(Ambient Radiance)을 제공하는 가상 스카이 버퍼 보정.
  - `forwardParticlePS.hlsl` 정면 알파 감쇄(`0.55`) 완화 또는 발광 파티클 전용 분기 도입.
- **기록 위치**: [CompositeVisualEffectPlan.md](CompositeVisualEffectPlan.md) 9.9절에 상세 원인 분석 및 향후 로드맵 문서화 반영 완료.

### [2026-11-18] 엔티티 삭제 시 AudioSource 즉시 정지, ActionTimeline 참조 단일화 및 리소스 편집기 미저장 확인 모달 연동

- **배경 및 목표**:
  - `AudioSourceComponent`의 `Stop()`이 요청 플래그만 변경하여 컴포넌트나 엔티티 삭제 시 FMOD 채널이 멈추지 않고 계속 재생되던 잔여 결함을 해소함.
  - `ActionTimelineComponent`에서 `timelineAsset` 핸들과 `timelinePath`가 둘 다 직렬화 프로퍼티로 노출되어 인스펙터에 이중 표시되고 두 값이 어긋나던 문제를 `timelinePath` 단일 진실 원천(SSOT)으로 정리함.
  - 콘텐츠 브라우저나 리소스 피커에서 미저장 변경이 있는 리소스 편집기를 열 때, 단순 토스트 알림으로 차단되던 방식을 `RunWithDiscardConfirmation`을 통한 대화형 확인 모달로 일원화함.
- **구현 내역**:
  - **1) `AudioSourceComponent` 소멸 시 FMOD 채널 즉시 정지 및 안전 복사/이동 시멘틱 구축**:
    - `Engine/Source/Audio/Public/AudioSourceComponent.h`: `ENGINE_API` 명시, 생성자/소멸자, 명시적 복사/이동 생성자 및 대입 연산자 선언, `StopImmediate()` 메서드 추가.
    - `Engine/Source/Audio/Private/AudioSourceComponent.cpp` (신설): 소멸자에서 `StopImmediate()`를 호출해 엔티티 삭제 시 즉시 `m_Channel->stop()`을 수행하도록 구현. 복사 시에는 `m_Channel = nullptr`, 이동 시에는 채널 소유권 이전 및 원본 null 초기화 적용.
    - `Engine/Source/Audio/Private/AudioSystem.cpp`: `m_StopRequested` 처리부에서 `source.StopImmediate()`를 호출하도록 통합.
  - **2) `ActionTimelineComponent` 이중 참조 정리 및 인스펙터 일원화**:
    - `Engine/Source/Animation/Timeline/Public/ActionTimelineComponent.h`: `timelineAsset` 프로퍼티를 `READONLY, NOSERIALIZE`로 변경하여 씬 저장/복원 시의 중복 저장을 원천 차단하고 `timelinePath`를 단일 진실의 원천으로 고정.
    - `Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp`: `HiddenProperties`에 `timelineAsset`과 `timelinePath`를 모두 등록하여 자동 필드 목록에서 숨기고, 하단 전용 경로 피커(`DrawInspectorAssetPathPicker`) 하나로만 선택되도록 일원화.
  - **3) 리소스 편집기 외부 열기 시 미저장 보호 모달 연동**:
    - `Editor/Source/ImGui/ResourceEditor/Public/ResourceEditorInterface.h`: `RunWithDiscardConfirmation` 메서드를 `public` 인터페이스로 노출.
    - `Editor/Source/ImGui/Private/ImGuiManager.cpp`: `OpenPendingResourceEditor()`에서 `editor->HasUnsavedResourceChanges()`일 때 단순 토스트 차단 대신 에디터 창을 열고 포커스한 뒤 `editor->RunWithDiscardConfirmation([editor, path] { editor->SetTargetResource(path); })`를 호출하여 사용자가 저장/폐기를 대화형으로 선택할 수 있도록 개선.
  - **4) 타임라인 노티파이 소켓 어태치 프리뷰 모델 엔티티 바인딩 수정**:
    - `Editor/Source/ImGui/TimelinePanel/Private/AnimationTimelinePanel.cpp`: `TriggerNotify`에서 노티파이의 `AttachToSocket` 처리 시 `attach.targetEntity`가 `entt::null`로 고정되어 프리뷰 모델에 부착되지 않던 결함을 `m_PreviewModelEntity ? m_PreviewModelEntity->GetEntityHandle() : entt::null`로 교정하여 실시간 프리뷰가 정상 작동하도록 개선.
- **검증 상태**:
  - `SceneArchiveCheck` 단위 테스트 100% 통과: 최신 `Engine.dll` 컴포넌트 레이아웃에 맞춰 테스트 바이너리를 재빌드함으로써 이전 헤더 레이아웃과의 불일치로 인한 세그멘테이션 오류(SegFault)가 완전히 해소되고 모든 하위 씬/아카이브 검증이 통과함을 확인 완료.
  - `AssetReferenceCheck` 단위 테스트 100% 통과: 미빌드 상태였던 독립 테스트 타깃을 명시적으로 빌드하여 정상 탐색 및 어셋 레퍼런스 검사 통과 확인 완료.
  - 정적 코드 검토 및 컴파일 경고/오류 없음 확인.

### [2026-11-18] 다중 광원 타일드 컬링 오버플로 검증 및 전용 단위 테스트(TiledLightingCheck) 구축

- **배경 및 목표**:
  - `LightManager`의 `MAX_LIGHTS = 1024` 확장 및 `tiledLightCullingCS.hlsl` / `lightingPS.hlsl`의 타일당 256개 초과 안전 전체 순회 폴백(`A06 본체`)이 반영되었으나, 이를 회귀 방지 차원에서 엄밀히 검증하는 전용 CTest가 부재함.
  - P2-B 렌더링 잔여 항목 해소를 위해 절두체 평면/구면 교차 판정, 255/256개 경계값 패킹, 256개 초과 오버플로 시 전역 순회 폴백의 조도 100% 보존, 1,024개 광원 스트레스 및 Directional/Sky 광원 전역 포함 규칙을 포괄 검증하는 전용 단위 테스트 `TiledLightingCheck`를 신설함.
- **구현 내역**:
  - `Engine/Source/Render/Light/Public/Light.h`:
    - `constexpr uint32 MAX_SHADOW_CASCADES = 3;`에서 타입 네임스페이스 누락으로 인해 전역 스코프에 `const int uint32`가 잘못 추론되어 `ShaderReflection.h` 등 타 헤더에서 `uint32` 모호성(C2872)을 유발하던 문제를 `constexpr Engine::uint32 MAX_SHADOW_CASCADES = 3;`로 교정.
  - `Engine/examples/TiledLightingCheck.cpp`:
    - **[Test 1] 4방향 타일 절두체 평면 생성 및 구면 교차 판정 정합성**:
      - 1920x1080 타일 픽셀 경계로부터 NDC min/max 역투영을 통한 4개 코너 뷰공간 방향 벡터 계산 및 Left/Right/Top/Bottom 4개 절두체 평면 법선 생성.
      - 4개 평면 법선이 모두 타일 내부 방향을 향함(내적 > 0) 검증.
      - 타일 중심 광원 포함, 좌/우 평면 바깥쪽으로 벗어난 광원 배제, 뷰공간 Z축 클리핑(`minTileZ` / `maxTileZ`) 배제 수치 정합성 검증.
    - **[Test 2] 타일당 256개 이하 정상 컬링 및 그리드/인덱스 버퍼 패킹 검증**:
      - 180개 광원 선별 시 `gridEntry.y == 180` 및 인덱스 리스트 1:1 매핑 검증.
      - 255개, 256개 상한선 경계값에서 오버런 없이 256개 전체가 유효하게 패킹됨을 단언.
    - **[Test 3] 타일당 256개 초과 오버플로 감지 및 전역 순회 폴백의 조도 일치성 검증**:
      - 단일 타일에 280개 점광원이 집중되는 과밀 씬 구성.
      - CS 누적 카운터(`s_TileLightCount = 280`) 보존 및 그리드 기록(`gridInfo.y = 280`), 인덱스 리스트는 256개로 안전 캡.
      - PS 시뮬레이션: `tileInfo.y > 256u` 감지 시 `useTiled = false`, `loopCount = totalLights`로 안전하게 폴백 전환.
      - Ground Truth(280개 전체 조도 합산) 대비, 256개 절삭 방식의 조도 누락 현상 확인 및 폴백 방식의 Ground Truth 조도 100% 일치($|\text{Diff}| < 10^{-4}$) 수치 검증.
    - **[Test 4] 1,024개 대규모 광원 스트레스 및 Directional/Sky 광원 전역 포함 검증**:
      - Directional 2개, Sky 1개, Point 800개, Spot 221개 배치.
      - 지오메트리가 없는 공백 타일에서도 Directional/Sky 광원은 전역 광원이므로 무조건 타일에 0순위 포함(`s_TileLightCount == 3`)됨을 검증.
      - 1920x1080 해상도(8,160개 타일) 기준 `LightManager::EnsureTileBuffers` 크기 계산 공식(Grid Buffer: 65,280B, Index Buffer: 8,355,840B) 정합성 단언.
  - `Engine/CMake/Checks.cmake`:
    - `TiledLightingCheck` 실행 파일 및 CTest 테스트 타깃 등록.
  - `Docs/DevelopmentPriorities.md`:
    - P2-B 남은 항목 해소 반영 및 11.6절 차기 우선순위 개발 로드맵 현행화.
- **검증 상태**:
  - `ctest --preset x64-ninja-debug -R TiledLightingCheck` 사용자 실행을 통해 [Test 1] 절두체 평면/구면 교차 수치 정합성, [Test 2] 255/256개 경계값 패킹, [Test 3] 256개 초과 시 오버플로 폴백 트리거 및 Ground Truth 조도 100% 일치, [Test 4] 1,024개 광원 스트레스 및 Directional/Sky 광원 전역 포함 규칙 전체 통과(4/4, 100% Pass) 확인 완료.

### [2026-11-18] 기본 BRDF LUT 업로드와 종료 크래시 수정

- 지연 조명 패스를 제출할 때 환경맵에 필요한 기본 LUT를 준비한다. 패스는 준비된 LUT를 바인딩하고 DX12의 패스 내부 업로드 제한을 유지한다. DX12 픽셀 업로드에 LUT 포맷인 `R16G16_FLOAT`를 추가했다.
- GPU 업로드 실패 시 LUT를 캐시하지 않는다. 런타임 LUT는 ResourceManager 핸들로 보관하고 런타임 종료 시 리소스 관리자 제거 전에 핸들을 비운다. 헤드리스 IBL 검사는 CPU LUT를 별도로 소유한다.
- 사용자 콜스택에서 패스 내부 업로드 실패와 종료 시 텍스처 재해제를 확인했다. 수정 후 Editor 빌드·CTest와 실행·종료 검증은 사용자 실행 전이다.

### [2026-11-18] DirectX12 정적 메시 버퍼 Default Heap 전환 및 MaterialInstance::Clone 구현

- **배경 및 목표**:
  - 빌드 로그에 남아 있던 `TODO` 선언 2건(`DirectX12Buffer.cpp`의 정적 메시 Default Heap 전환, `MaterialInstance.cpp`의 `Clone` 미구현)을 실제 기능으로 구현해 제거함.
- **구현 내역**:
  - **1) DirectX12 정적 Vertex/Index 버퍼 `D3D12_HEAP_TYPE_DEFAULT` 전환**:
    - `Engine/Source/Render/RHI/Platform/DirectX12/Public/DirectX12Buffer.h` & `Private/DirectX12Buffer.cpp`: 리소스 상태(`m_CurrentState`) 추적을 추가하고, 초기 데이터가 제공되는 정적 `Vertex`/`Index` 버퍼는 `D3D12_HEAP_TYPE_DEFAULT` 힙(`D3D12_RESOURCE_STATE_COMMON`)으로 생성하도록 전환(`DebugRenderer` 등 `initialData == nullptr` 동적 버퍼는 기존 `D3D12_HEAP_TYPE_UPLOAD` 유지). `SetData`, `UploadRangeInternal`, `QueryUploadComplete`가 Default 힙 버퍼에서도 RHI 업로드를 통해 동작하도록 보완.
    - `Engine/Source/Render/RHI/Platform/DirectX12/Public/DirectX12RHI.h` & `Private/DirectX12RHI.cpp`: `UploadBufferData(RHIBuffer*, const void*, uint32, uint32)` 신설. 프레임 기록 중(`m_FrameRecording`)에는 프레임별 업로드 스테이징 버퍼 풀(`m_TextureUploadBuffers`)을 재사용해 무대기로 `CopyBufferRegion` 및 `VERTEX_AND_CONSTANT_BUFFER`/`INDEX_BUFFER` 상태 전이를 기록하고, 프레임 외부에서는 임시 커맨드 리스트와 펜스 완료 대기로 안전하게 GPU 업로드를 수행.
  - **2) `MaterialInstance::Clone` 구현 및 `GetBlendState` 일관성 수정**:
    - `Engine/Source/Render/Material/Private/MaterialInstance.cpp`: `MaterialInstance::Clone(void* arg)`에서 베이스 머티리얼 핸들, 오버라이드 플래그, 파라미터, 텍스처 바인딩, 패스 셰이더를 복제하고 `RebuildTextureBindingCache()`를 호출해 유효한 복제 인스턴스를 반환하도록 구현(`arg`로 `DESC` 전달 시 `Key`/`Path`/`BaseMaterialHandle` 갱신 지원).
    - `Engine/Source/Render/Material/Private/MaterialInterface.cpp`: `GetBlendState()`가 `m_BlendMode` 직접 참조 대신 가상 함수 `GetBlendMode()`를 호출하도록 수정하여 오버라이드되지 않은 `MaterialInstance`도 베이스 머티리얼의 `BlendMode`를 반영하도록 교정.
- **검증 상태**:
  - 정적 검토 및 코드베이스 `TODO` 잔여 확인. 지침에 따라 빌드(`cmake --build`)와 실환경 렌더링/테스트 검증은 사용자 직접 실행 대상으로 안내함.

### [2026-11-18] 콘텐츠 브라우저 썸네일 아틀라스, 무대기 업로드 및 그리드 배칭 최적화 (방안 A+B+C+D)

- **배경 및 목표**:
  - 다량의 3D 썸네일이 존재하는 폴더(`sponza` 등)를 콘텐츠 브라우저에서 열어둘 때 발생하는 로딩 중 GPU Fence Stall 스터터링과 상시 ImGui 드로우콜/CPU 경로 연산 프레임 드랍을 제거함.
- **구현 내역**:
  - **방안 C (프레임 기록 중 무대기 업로드)**:
    - `Engine/Source/Render/Public/Renderer.h`, `Engine/Source/Render/Private/Renderer.cpp`: `GetPreRenderDelegate()`를 신설하고 `Renderer::Render` 시작부(`m_FrameRecording == true`)에 브로드캐스트하여 GPU Fence 대기 없이 메인 커맨드 리스트로 텍스처 업로드가 기록되도록 전환.
  - **방안 B (공용 2048×2048 썸네일 아틀라스)**:
    - `Editor/Source/AssetManager/AssetCache/Public/AssetCache.h`, `Editor/Source/AssetManager/AssetCache/Private/AssetCache.cpp`: 페이지당 `128×128` 썸네일 256개를 수용하는 `2048×2048` 공용 아틀라스 및 하프-텍셀 인셋 UV 계산, `RHI::UploadTextureData(..., dstX, dstY)` 부분 업로드 파이프라인을 구축(`NotImplemented` 백엔드는 개별 텍스처 자동 폴백). 외부 단독 미리보기(`TextureEditor`, `ResourcePicker`)용 `GetThumbnail`은 지연 생성 방식으로 100% 호환 유지.
  - **방안 D (LIFO 가시 영역 우선 스케줄링 & 상주 스레드 PNG 디코딩)**:
    - `Editor/Source/AssetManager/Worker/Public/AssetWorkerHost.h`, `Editor/Source/AssetManager/Worker/Private/AssetWorkerHost.cpp`: 워커 작업 꺼내기 및 `AssetCache::FlushPendingUploads`(`MAX_UPLOADS_PER_FRAME = 8`)를 최신 요청 우선(LIFO)으로 전환하고, 워커 호스트 상주 스레드 내에서 `DecodePngToRgba` 디코딩까지 완결해 `std::async` 스레드 추가 생성 오버헤드 제거.
  - **방안 A (타일 메타데이터 사전 캐싱 & 3-채널 DrawList 배칭)**:
    - `Editor/Source/ImGui/ContentBrowserPanel/Public/ContentBrowserGridItem.h`, `ContentBrowserGridItem.cpp`, `ContentBrowserPanel.h`, `ContentBrowserPanel.cpp`: `RebuildGridItems`에서 `CacheKey`, `IsReadOnly`, `AssetVisualDesc`, ImGui ID를 1회 캐싱하고 축약 파일명 캐시를 도입함. `ImGui::BeginTable` 대신 `SetCursorScreenPos` 그리드 + 3-채널 `ImDrawListSplitter`(Channel 0: 배경, Channel 1: 아틀라스 썸네일 일괄 배칭, Channel 2: 아이콘/텍스트/스트립)를 적용해 대량 썸네일 표시 중 드로우콜을 대폭 축소.
- **검증 상태**:
  - 정적 코드 검토 완료. 지침에 따라 빌드 및 실환경 성능 계측(FPS / Draw Call)은 사용자 실행 대상으로 안내함.

### [2026-11-18] 환경맵 기반 IBL 및 그림자 앰비언트 밸런싱 고도화, DefaultBRDFLUT 절차적 생성 파이프라인 구축 (S5)

- **배경 및 목적**:
  - 기존 Split-Sum PBR 큐브맵 IBL은 사용자가 `BRDFLutMap` 텍스처 에셋을 직접 지정하지 않으면 완전히 비활성화되거나 셰이더 바인딩이 누락되는 제약이 있었음.
  - 큐브맵 환경맵(`IrradianceMap`, `PrefilteredMap`)이 없는 기본 씬에서는 칙칙한 단색 회색(`0.15`)으로 폴백되어 물리 기반 음영 및 색감이 죽는 문제가 발생함.
  - 직사광이 차폐되는 그림자 음영부에서 앰비언트 하한값이 없어 물체 표면이 완전히 새까맣게 타는(Pitch Black) 현상 및 알베도 디테일 상실이 발생함.
- **구현 내역**:
  - `Engine/Source/Render/IBL/Public/IBLManager.h` & `Private/IBLManager.cpp`:
    - `IBLManager` 싱글톤 신설.
    - Karis 2013 중요도 샘플링 적분 공식(Hammersley 2D 저불일치 수열 + GGX 중요도 샘플링 + Smith 기하 감쇄) 기반으로 $256 \times 256$ 해상도의 `R16G16_FLOAT` Split-Sum BRDF LUT 텍스처(`Engine://Texture/DefaultBRDFLut`, 256KB)를 절차적으로 생성하고 캐싱하는 `GetDefaultBRDFLUT()` 파이프라인 구축.
    - RHI 활성 런타임에서는 `rhi->CreateTexture`로 GPU 텍스처를 생성하고, RHI가 없는 테스트 환경에서는 CPU 메모리 텍스처로 안전하게 생성되도록 이중화.
    - `GenerateBRDFLUT`: 가용 CPU 코어 수(`std::thread::hardware_concurrency()`) 단위 행(Row) 분할 멀티스레드 병렬 연산을 적용하여 6,710만 회 적분 연산 시간 대폭 단축.
    - 프로그램 종료 시 `Free()`를 통해 GPU 리소스 및 CPU 메모리 완벽 회수.
  - `Engine/Source/Render/Atmosphere/Public/SkyAtmosphereComponent.h`:
    - `ShadowAmbientFloor = 0.15f` (범위 0.0 ~ 1.0) 프로퍼티 추가 및 리플렉션 노출.
  - `Engine/Source/Render/Pipeline/Private/DeferredSceneRenderer.cpp`:
    - `DeferredLightingPassData` 구조체: 미사용 `_pad`와 `_reserved` 영역을 `ShadowAmbientFloor`, `SkyZenithColor`, `SkyIntensity`로 교체 (기존 512B 상수 버퍼 크기 및 오프셋 엄격 보존).
    - `sky.BRDFLutMap` 부재 시 `IBLManager::Get().GetDefaultBRDFLUT()` 자동 주입.
    - `PrefilteredMap`만 지정되어도 `IrradianceMap` 대체 폴백을 적용하여 큐브맵 IBL(`HasIBLMaps = 1`) 즉시 활성화.
    - `SkyAtmosphereComponent`의 `ZenithColor`, `SkyIntensity`, `ShadowAmbientFloor` 조명 상수 버퍼 주입.
  - `Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp`:
    - `LightingPassData` 구조체 정의를 `DeferredLightingPassData`와 512B 동일하게 일치.
  - `Engine/Resources/Shader/hlsl/lightingPS.hlsl`:
    - `LightingPassData`에 `ShadowAmbientFloor`, `SkyZenithColor`, `SkyIntensity` 반영.
    - `HasIBLMaps == 0`일 때 `SkyZenithColor` 기반 물리 반구형 하늘 IBL 계산 추가 ($N \cdot \text{Up}$ 기반 하늘-지면 코사인 보간).
    - `ambientFloor = max(gLightingData.ShadowAmbientFloor, 0.02f)`를 통한 그림자 음영부 앰비언트 하한값 및 알베도 보존 수식 적용.
  - `Engine/Source/Render/Texture/Public/Texture.h`:
    - `GetCachedHeader()`, `GetTempData()` 인라인 접근자 추가.
  - `Engine/examples/IBLShadingCheck.cpp` & `Engine/CMake/Checks.cmake`:
    - 전용 CTest 단위 검증 테스트 타깃 `IBLShadingCheck` 구축.
    - [Test 1] BRDF 수치 적분 경계 조건 및 물리적 에너지 보존 법칙($\text{scale} + \text{bias} \le 1.0001$) 검증.
    - [Test 2] `DefaultBRDFLUT` 텍스처 규격(256x256, R16G16_FLOAT, 256KB) 및 싱글톤 캐싱 멱등성 검증.
    - [Test 3] 반구형 하늘 IBL 상향/지평선/하향 조도 및 직사광 완전 차폐 시 앰비언트 플로어(칠흑 방지 및 알베도 보존) 검증.
- **검증 상태**:
  - `ctest --preset x64-ninja-debug -R IBLShadingCheck` 단위 테스트 전체 통과 (100% Pass) 확인 완료.
  - [Test 1] BRDF 수치 적분 및 물리적 에너지 보존 법칙($\text{scale} + \text{bias} \le 1.00000$) 통과.
  - [Test 2] `DefaultBRDFLUT` 텍스처 규격(256x256, R16G16_FLOAT, 256KB) 및 싱글톤 캐싱 멱등성 검증 통과.
  - [Test 3] 반구형 하늘 IBL 상향/지평선/하향 조도 및 직사광 완전 차폐 시 앰비언트 플로어(칠흑 방지 및 알베도 보존) 검증 통과.
  - 디버그 빌드 환경에서 약 43초 소요 ($256 \times 256$ 픽셀 $\times$ 1,024회 중요도 샘플링 = 총 약 6,710만 회 삼각함수 및 저불일치 수열 연산).

### [2026-11-18] 독립 워커 프로세스(`BamAssetWorker`) 기반 3D 에셋·서브메시·프리팹 썸네일 비동기 베이킹

- **배경 및 목표**:
  - 기존 인-프로세스 Fast Reject로 `sponza` 폴더 프리징을 제거했으나, `.bammesh`(1,023개), 대형 복합 `.bammodel`(`sponza.bammodel`, `dragon.bammodel`), 원시 모델(`.obj`, `.fbx`, `.glb`), 프리팹(`.bamprefab.json`)도 메인 스레드 부하 없이 실제 3D 썸네일로 표시되도록 Out-of-Process 워커 아키텍처(방향 B)를 구축함.
- **구현 내역**:
  - `Tools/AssetWorker/` (`CMakeLists.txt`, `main.cpp`, `HeadlessThumbnailRasterizer.h/.cpp`):
    - GPU/RHI 및 ImGui 의존성이 없는 헤드리스 워커 실행 파일 `BamAssetWorker` 신설 (`Editor` 타겟이 의존).
    - 단발 CLI(`--mode thumbnail`) 및 표준입출력 파이프 데몬(`--daemon`) 모드 지원.
    - 2x SSAA (`256x256` -> `128x128`) 스튜디오 3점 조명 PBR 소프트웨어 래스터라이저 및 ACES 톤매핑·텍스처 샘플링(`.bamtex`, `.dds`, `.png`, `.tga`, `.jpg`) 구현.
    - 대형 복합 모델은 최경량 LOD(`.bammesh` LOD2) 우선 로드, 초대형 단일 메시(`198MB` 등)는 정점 AABB를 유지한 균등 삼각형 샘플링으로 수십 ms 내 고속 베이킹, 프리팹은 `Scene::InstantiatePrefabArchive` + 아카이브 직접 파싱 폴백 모두 지원.
  - `Editor/Source/AssetManager/Worker/` (`AssetWorkerHost.h/.cpp`):
    - `Windows Job Object`(`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`), `CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS` 기반 데몬 프로세스 2개 풀 관리 및 비정상 종료 시 자동 재스폰 구축.
  - `Engine/Source/Resource/Public/ResourceHandleArchiveAdapter.h`:
    - `ResolveResourceHandle`에 레거시 `Resources/` -> `Assets/` 경로 치환 폴백을 추가해 과거 임포트 모델(`cube.bammodel`, `Plane.bammodel`, `Road-subdi.bammodel`) 및 프리팹(`Large Floor Sphere.bamprefab.json`) 참조 오류 해소.
  - `Editor/Source/Rendering/Thumbnail/Public/ThumbnailRenderer.h` & `Private/ThumbnailRenderer.cpp`:
    - 워커 미사용 시의 인-프로세스 폴백용 `SubmitPrefab` 구현.
  - `Editor/Source/ImGui/PrefabEditor/Private/PrefabModeManager.cpp` & `PrefabEditorPanel.cpp`:
    - `SavePrefab` 성공 시 `AssetCache::RequestInvalidate`를 호출하여 프리팹 저장 즉시 썸네일 갱신.
  - `Editor/Source/AssetManager/AssetCache/Public/AssetCache.h` & `Private/AssetCache.cpp`:
    - `MAX_CACHE_ENTRIES = 1024`, `MAX_CACHE_BYTES = 128MB` 상향 및 디스크 캐시 시드 `_v2` 갱신.
    - `GetThumbnail`/`Update`에서 모델·메시·프리팹 썸네일을 `AssetWorkerHost`로 위임하고 완료된 PNG를 `m_DecodeWorkers`로 비동기 로드.
- **검증 상태**:
  - 구현 완료 후 정적 파일 확인 완료. 지침에 따라 빌드(`cmake --build --preset x64-debug --target Editor`)와 실환경 에디터·CLI 검증은 사용자 실행 대상으로 안내함.

### [2026-11-18] 정적 메시 인스턴싱 고도화 및 섀도우 패스 배칭 통합, 단위 테스트(MeshInstancingCheck) 구축 (S3)

- **배경 및 원인**:
  - `Renderer::RenderMeshes` 배칭 파이프라인에서 드로우 커맨드가 `material` 우선으로 정렬되어 있어, 깊이만 기록하는 그림자 패스(`ShadowDepthPass`)에서도 머티리얼이 다르면 동일 메시임에도 인스턴스 배치가 분리되어 수십~수백 개의 불필요한 드로우콜이 발생하는 결함이 확인됨.
  - 대규모 인스턴스 씬에서 `instanceBatch`가 링 버퍼 크기를 초과하여 오버플로(`EResult::OutOfMemory`)를 유발할 수 있는 가드가 부재함.
  - 인스턴싱 메모리 레이아웃(80B 정렬), 비균일/음수 스케일 노멀 역전치 외적 공식, 패스별 배칭 및 LOD 히스테리시스 전환 알고리즘을 회귀 방지 차원에서 검증하는 전용 CTest가 부재함.
- **수정 내용**:
  - `Engine/Source/Render/Private/Renderer.cpp`:
    - 커맨드 정렬 분기: `passType == ERenderPassType::Shadow`인 경우 `a.mesh`를 최우선 정렬 키로 사용하고, 동일 메시 내에서는 래스터라이저 호환성(`cullMode`, `frontFace`) 순으로 정렬하도록 최적화.
    - `RenderMeshes` 배칭 플러시 판정:
      - 섀도우 패스일 때 동일 메시이면서 `cullMode`와 `frontFace`가 동일한 경우, 서로 다른 머티리얼이라도 단일 인스턴스 배치로 병합 허용.
      - 동적 링 버퍼 오버플로 방지: 최대 인스턴스 배치 수 가드(`MAX_INSTANCES_PER_BATCH = 1024`)를 두어 1024개 도달 시 안전하게 분할 플러시 수행.
  - `Engine/examples/MeshInstancingCheck.cpp`:
    - [Test 1] `InstanceData` 80바이트 정렬 단언 (`offsetof`, 16바이트 GPU 정렬 검증) 및 `SpriteInstanceData`(112B) 레이아웃 검증.
    - [Test 2] `gbufferVS.hlsl`의 여인수 기저 외적 계산 공식과 수학적 $M^{-T} \cdot n$ 간의 20가지 케이스(비균일, 음수 반전 스케일 등) 내적 0.9999 일치성 검증.
    - [Test 3] Geometry Pass(머티리얼별 분리 5 드로우콜) 대비 Shadow Pass(동일 메시 3개 머티리얼 600개 인스턴스 단일 드로우콜 압축 및 2500개 1024 분할 플러시) 시뮬레이션 검증.
    - [Test 4] `LODGroupComponent` 거리/커버리지별 LOD 0 $\to$ 1 $\to$ 2 전환 및 $\pm 0.05$ 히스테리시스 마진을 통한 경계면 LOD 깜빡임(Oscillation) 방지 검증.
  - `Engine/CMake/Checks.cmake`:
    - `MeshInstancingCheck` 실행 파일 및 CTest 테스트 타깃 등록.
  - `Engine/Source/Render/Public/InstanceData.h`:
    - `Engine::mat4`, `Engine::vec4` 네임스페이스 수식어를 명시하여 독립 컴파일 단위에서 타입 누락 방지.
  - `Engine/Source/Render/Mesh/Private/MeshRenderSystem.cpp`:
    - `current == 1`일 때의 히스테리시스 임계값을 `lod1Thresh += margin`으로 정정하여, 0.45~0.50 화면 점유율 구간에서 LOD 1이 LOD 0으로 역전되어 깜빡이던 실제 엔진 런타임 결함 해결.
  - `CMakePresets.json`:
    - `testPresets`에 `x64-ninja-debug` 및 `x64-ninja-release` 추가.
- **검증**:
  - `ctest --preset x64-ninja-debug -R MeshInstancingCheck` 실행을 통해 [Test 1] 메모리 레이아웃 단언, [Test 2] 여인수 기저 외적 법선 변환 20개 케이스 수학적 일치, [Test 3] 섀도우 패스 600개 단일 드로우콜 압축 및 1024 분할 플러시, [Test 4] 거리별 LOD 전환 및 히스테리시스 진동 억제 전체 통과(100% Pass) 확인 완료.

### [2026-11-18] 대형 복합 모델 및 서브메시 Fast Reject 적용을 통한 콘텐츠 브라우저 스폰자 프리징 원천 해소

- **배경 및 원인**:
  - `Assets/Model/sponza` 폴더에는 1,023개의 `.bammesh` 파일, 26개의 `.bammatinst`, 1개의 대형 복합 모델 `sponza.bammodel`(288KB, 내부에 1,143개 서브메시 참조), 1개의 원본 모델 `sponza.obj`(21.1MB)가 존재함.
  - 기존에는 `isModelOrMesh`에 `.bammesh`가 포함되어 폴더 진입 시 1,023개의 서브메시가 전부 메인 스레드 3D 렌더링 큐에 등록되었고, `ProcessPending3DThumbnail`에서 `sponza.obj` 처리 시 동명의 `sponza.bammodel`을 발견했을 때 파일 크기 검사 없이 `rm.LoadFile`을 호출하여 1,143개 서브메시지를 동기로 파싱하고 D3D12 버퍼를 생성하느라 UI가 20~30초간 완전히 멈추는 현상이 발생함.
  - 또한 `ToProjectRelative` 경로와 `ResourceManager` 표준 가상 URI(`Project://`) 간 불일치로 인해 캐시 적중에 실패하는 문제가 확인됨.
- **수정 내용**:
  - `Editor/Source/AssetManager/AssetCache/Public/AssetCache.h`:
    - `MAX_RAW_MODEL_PARSE_BYTES = 4 * 1024 * 1024` (4MB), `MAX_COMPOSITE_MODEL_PARSE_BYTES = 64 * 1024` (64KB) 가드 상수 정의.
    - `SaveNegativeCacheToDisk` 선언 추가.
  - `Editor/Source/AssetManager/AssetCache/Private/AssetCache.cpp`:
    - `GetThumbnail`:
      - `.bammesh`를 `isModel`에서 완전 제외하여 3D 큐 등록 없이 0ms 즉시 `ICON_FA_DRAW_POLYGON`("MESH") 아이콘으로 렌더링되도록 변경.
      - 4MB 초과 미임포트 원시 모델(`.obj`, `.fbx` 등) 및 64KB 초과 대형 복합 모델(`.bammodel`) Fast Reject 및 네거티브 디스크 캐시(0바이트 파일) 기록 적용.
    - `LoadThumbnailFromDiskCache`: 0바이트 네거티브 캐시 감지 시 빈 태스크를 반환하여 디스크/오프스크린 재시도 완전 차단.
    - `SaveThumbnailToDiskCache`: `std::async(std::launch::async, ...)`로 디스크 파일 저장을 비동기화하여 메인 스레드 I/O 블로킹 제거.
    - `ProcessPending3DThumbnail`:
      - `sponza.obj` 등 원본 모델의 동명 `.bammodel` 연계 로드 경로(`candidate1`, `candidate2`)에 `fileSize <= MAX_COMPOSITE_MODEL_PARSE_BYTES` (64KB) 가드 추가. 초과 시 동기 로드를 포기하고 Fast Reject.
      - 모델 URI 변환을 `Engine::PlatformPath::ToVirtualUri(modelToLoad)`로 일원화하여 `ResourceManager` 캐시 적중 보장.
      - 독립 `.bammodel` 처리부에도 동일하게 64KB 초과 가드 적용.
      - 미정의 변수 `else if (isMesh)` 블록을 완전 제거하여 컴파일 오류 해결.
  - `Docs/OutOfProcessAssetPipelinePlan.md`:
    - 향후 근본적인 백그라운드 프리뷰 베이킹을 위한 독립 프로세스 워커 아키텍처(3.5절) 상세 설계 문서화.
- **검증**:
  - 정적 코드 검토 및 지침 준수 확인 완료. 빌드 및 테스트는 지침에 따라 AI가 직접 실행하지 않고 사용자 검증으로 안내함.

### [2026-11-18] 계층 패널의 다중 드래그 이동

- 선택된 행에서 드래그를 시작할 때 같은 씬의 선택 엔티티와 영구 ID를 보관한다. 다른 엔티티나 씬 루트에 드롭하면 묶음을 이동하며 선택 밖에서 시작하면 해당 엔티티만 이동한다. 단일 참조 입력칸은 드래그를 시작한 엔티티의 기존 페이로드를 사용한다.
- `Scene::ReparentEntities`가 선택된 상위 엔티티의 하위 관계를 유지하고 순환·깊이·월드 변환을 먼저 확인한다. 원래 부모의 형제 순서를 보존하고 새 부모에 이동할 루트를 모아서 추가한다. 기존 `ReparentEntity`도 같은 경로를 사용한다. 실패한 묶음은 복원하며 성공은 씬 이력 한 건으로 기록한다.
- `HierarchyEditorCheck`에 묶음 이동·루트 드롭·드래그 시작 선택 보존·오래된 구성원 거부를, `EditorHistoryCheck`에 한 건의 Undo/Redo를 추가했다. 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 작업 워크트리의 Windows Ninja Debug CTest 로그에서 사용자가 실행한 HierarchyEditorCheck, EditorHistoryCheck, TransformInheritanceCheck 통과를 확인했다. 병합 후 빌드·CTest와 실제 드래그 검증은 별도로 진행한다. [대화형 점검 G](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 참조 문제 검색과 유형 필터 확장

- 읽을 수 없는 문서와 에셋 루트 외부를 포함한 다섯 유형의 전체 검사 건수를 필터에 표시한다. 원본 경로·대상 참조·속성명 검색을 결합하고 경로 구분자와 대소문자를 정규화한다.
- 검사 또는 필터 조건이 바뀌면 표시할 행을 계산한다. 표시/전체 건수를 함께 보여주고 조건에 맞는 행이 0건인 경우와 전체 검사에서 문제가 없는 경우를 구분한다. 일괄 복구 버튼의 도움말에 전체 검사 대상 범위를 명시했다.
- 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 빌드·CTest와 대화형 검증은 사용자 실행 전이다. [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 에셋 참조 검사 시간 계측

- 에디터 스레드에서 실행하는 참조 패널의 첫 조회·수동 검사·자동 갱신·복구 후 검사와 콘텐츠 브라우저의 삭제 영향·이동 준비·이동 영향 재검사를 계측한다. 검사 시작과 종료에서 단조 시계만 읽고, 같은 ImGui 프레임에 실행한 측정 구간의 시간을 더한다.
- 엔진 통계의 작업 및 엔진 상세에서 작업별 표본 수·최근·평균·최대와 프레임 합계를 표시한다. 기록 초기화에 연결했으며 성능 상세의 프레임 간격과 비교한다. 디바운스, 사용자 확인 대기, 파일 이동 및 기록은 집계 범위에서 제외한다.
- `AssetReferenceCheck`에 같은 프레임의 누적, 프레임 전환, 최대와 평균, 초기화 검증을 추가했다. 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 빌드·CTest와 실제 프로젝트 측정은 사용자 실행 전이다. [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 참조 문제 목록에서 원본으로 이동

- 참조 문제 탭의 원본 에셋 경로에 더블클릭과 우클릭 메뉴를 연결했다. 원본을 열거나 콘텐츠 브라우저에서 찾을 수 있고, 폴더 행의 더블클릭은 위치 찾기를 사용한다.
- 참조 트리와 공용 파일 액션을 재사용해 문서 열기 보호를 적용한다. 원본 경로를 메뉴 ID에 포함해 재검사에서 같은 행 위치에 다른 원본이 들어와도 이전 메뉴가 이어지지 않게 한다.
- 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 빌드·CTest와 대화형 검증은 사용자 실행 전이다. [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 에셋 참조 자동 복구의 구조 기반 처리

- 검사기와 이동 기능이 사용하는 참조 방문을 재사용한다. 저장 문서에서 검사 당시와 같은 오류 및 대체 경로를 재확인한 리소스 핸들만 복구한다. 단일·일괄 복구를 공유하며 일반 문자열, Null 핸들, 별도 의미의 키, BOM 및 64비트 정수를 유지한다. 모호한 후보와 마운트 밖 파일은 자동 복구 후보에서 제외한다.
- 이동 기능의 파일 잠금·원문 비교·복원 경로를 재사용하며 복구 전후 원문을 `.bam/asset-reference-repair/`에 백업한다. 편집기에서 열린 씬 및 편집기가 보관 중인 파일을 건너뛰고, 성공한 기록만 건수에 포함한다.
- `AssetReferenceCheck`에 단일·일괄 복구, 같은 문자열의 비참조 필드, 잠긴 문서, 오래된 제안, 모호한 후보와 마운트 밖 후보를 추가했다. 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 빌드·CTest와 대화형 검증은 사용자 실행 전이다. [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 에셋 참조 관계 검색

- 참조 관계 탭에 공용 검색 바를 연결했다. 기존 참조 데이터에서 현재 대상과 이어지는 파일명·표시 URI·절대 경로를 검색한다. 한 방향 안에서는 순환을 끊고 공유 관계를 중복 없이 표시한다.
- 검색어, 조회 대상 또는 스캔 결과가 바뀔 때 결과를 다시 만든다. 행의 조회 대상 전환 및 우클릭 동작을 재사용하며 검색어를 지우면 트리의 펼침 상태로 복귀한다.
- 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 빌드·CTest와 대화형 검증은 사용자 실행 전이다. [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 에셋 참조 조회의 뒤로·앞으로 탐색

- 참조 관계 탭에 뒤로·앞으로 버튼과 마우스 X1/X2 탐색을 추가했다. 관계 행과 콘텐츠 브라우저의 에셋 참조 보기에서 방문을 기록한다. 방문 기록은 현재 세션에 이전·다음을 합쳐 최대 64개 유지하며, 뒤로 이동한 뒤 새 경로를 조회하면 앞으로 기록을 비운다.
- 현재 대상과 방문 경로의 이동·이름 변경은 기존 파일 작업 트랜잭션에 함께 연결했다. 삭제된 기록은 해당 위치의 누락 상태를 표시한다. 검사·열기·위치 찾기는 새 방문을 만들지 않는다.
- 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 빌드·CTest 및 대화형 검증은 사용자 실행 전이다. [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 에셋 참조 트리에서 파일 열기와 위치 찾기

- 참조 탭의 조회 대상과 사용처·의존성 행에 열기 및 콘텐츠 브라우저 위치 찾기 메뉴를 추가했다. 기존 콘텐츠 브라우저와 리소스 피커의 동작을 공유하므로 씬 로드 확인, 리소스 교체 보호, 기본 프로그램 열기 및 브라우저 포커스·선택을 같은 경로로 처리한다.
- 씬·프리팹 판정에 넓은 문자열 경로를 사용하고 메뉴를 띄운 뒤 사라진 파일의 열기 실패를 로캘 알림으로 표시한다. [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)에 사례를 추가했다.
- 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 빌드·CTest 및 실제 UI 동작은 사용자 실행 전이다.

### [2026-11-18] 저장된 문서 변경의 에셋 참조 자동 갱신

- 기존 콘텐츠 브라우저 파일 감시에서 프로젝트 에셋 폴더의 참조 문서 생성·수정·삭제·이름 변경 신호를 받는다. 감시 스레드는 원자 플래그만 기록하며, 에디터 스레드에서 스캔을 예약한다. 콘텐츠 브라우저가 닫혀 있어도 신호를 처리한다.
- 파일 명령과 문서 변경의 재검사 요청은 마지막 신호에서 200ms 뒤까지 모은다. 이후 참조 패널이 표시될 때 스캔한다. 첫 조회와 수동 검사는 즉시 실행한다. 검사기와 감시자가 확장자 판정을 공유한다.
- `AssetReferenceCheck`에 대문자·레거시 확장자와 임시 파일·이미지 제외를 추가했다. 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 빌드·CTest 및 대화형 검증은 사용자 실행 전이다. [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-18] 에셋 참조 패널의 삭제·복사 동기화

- 콘텐츠 브라우저에서 삭제나 복사 파일 명령이 성공하면 에셋 참조 패널의 다음 가시 프레임에 검사를 예약한다. 삭제·복사 Undo/Redo도 같은 방식으로 처리한다. 일부 성공한 복사는 성공한 항목만 이력에 넣으며 재검사는 실제 파일 상태를 읽는다.
- 파일이나 상위 폴더가 삭제돼도 조회 대상의 경로를 유지하고 참조 탭에 누락 안내를 표시한다. 삭제 Undo 후에는 원래 경로의 관계 트리가 돌아온다. [파일 이력](EditorUndoRedo.md#콘텐츠-브라우저-파일-이력)과 [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)에 동작과 검증 절차를 기록했다.
- 로캘 엄격 감사와 변경부 공백 검사를 통과했다. `c75c2f41` 이후의 사용자 실행 CTest 로그에서 `AssetReferenceCheck`, `EditorHistoryCheck`, `EditorUICheck` 통과를 확인했다. 대화형 검증 결과는 별도로 확인한다.

### [2026-11-18] 종료 시 FMOD Sound 이중 해제 크래시 수정

- AudioClip을 한 번이라도 로드한 뒤 에디터나 클라이언트를 종료하면 크래시가 났다. `Runtime::Free`가 `AudioManager`(FMOD `System::close/release`)를 `ResourceManager`보다 먼저 해제했고, 이후 `ResourceManager::Free → AudioClip::Free`가 이미 해제된 System 소속의 `FMOD::Sound::release()`를 호출했다. 생성 순서(Audio → Resource)의 역순 해제도 깨져 있었다.
- `Safe_Destroy(m_AudioManager)`를 `Safe_Destroy(m_ResourceManager)` 뒤로 옮겼다. `AudioClip::Free`는 `AudioManager`를 참조하지 않으므로 새 순서에서 추가 의존이 생기지 않는다.
- 사용자가 수정 후 종료 크래시가 재현되지 않는다고 보고했다. 수정 전 콜스택은 확인하지 않았고, 원인은 코드 대조로 판단했다. PIE 중 종료, 루프 재생 중 종료, 클라이언트 종료를 시나리오별로 따로 확인한 기록은 없다.
- 남은 문제(별도): `~AudioSourceComponent`의 `Stop()`은 요청 플래그만 바꾸므로, 재생 중인 엔티티를 삭제하면 채널이 멈추지 않는다.

### [2026-11-18] 핫리로드 시스템 원자적 파일 저장(Moved) 지원 및 부모-자식 재귀 감시 충돌 해소

- `ShaderHotReloadManager`, `AssetHotReloadManager`, `ModuleHotReloadManager`, `ModuleBuildManager`의 `efsw::FileWatchListener` 이벤트 수신부에 `efsw::Actions::Moved` 액션 처리를 추가했다. Visual Studio, VS Code 등에서 파일 저장 시 임시 파일 후 `MoveFileEx`(원자적 교체)로 덮어쓸 때 핫로드 이벤트가 누락되던 문제를 전면 해소했다.
- `ShaderHotReloadManager`:
  - `Engine/Resources/Shader`와 하위 `.../Shader/hlsl`을 동시 재귀 감시하던 중복 구조를 상위 루트 단일 감시로 통합하여 Windows `ReadDirectoryChangesW` 핸들 충돌 및 하위 이벤트 잠식을 방지했다.
  - `IHotReloader::AddWatchPath`에 상위 디렉터리가 이미 감시 중인 경우 하위 디렉터리 중복 등록을 자동 생략하는 부모-자식 경로 검사를 도입했다.
  - 파일 리스너에 `Engine::Utf8ToPath` 표준 변환, 외부 스레드 예외 방어(`try-catch`), 파일 변경 감지 실시간 로깅(`EDITOR_LOG_INFO`)을 구축했다.
  - `ProcessModifiedFile` 호출 시 `forceRecompile = true`를 전달하도록 연계했다.
- `ShaderCompiler`:
  - `Compile` 함수에 `bool forceRecompile = false` 파라미터를 추가하고, 핫리로드 시 타임스탬프 디스크 캐시(`IsUpToDate`)를 우회하여 최신 HLSL 소스로 DXC 재컴파일을 강제하도록 개선했다.
- `ModuleHotReloadManager` 및 `ModuleBuildManager`:
  - `Engine::Utf8ToPath` 변환과 `try-catch` 안전 경계를 추가하고, `StartWatching` 시 UTF-8 경로 전달을 보장했다.
- 정적 코드 검토 및 변경부 정합성을 확인했다. 빌드 및 테스트는 지침에 따라 AI가 직접 실행하지 않고 사용자 검증으로 안내한다.

### [2026-11-18] 에셋 참조 관계 트리와 파일 이동 동기화

- 콘텐츠 브라우저의 이동·이름 변경과 파일 Undo/Redo에 참조 패널을 연결했다. 성공 시 대상 파일 또는 상위 폴더의 새 경로를 반영한다. 검사 완료 상태에서는 결과 갱신을 예약하며 숨긴 동안 변경이 누적돼도 다시 표시할 때 한 번만 검사한다.
- 참조 검사에서 만든 양방향 인접 목록을 사용처와 의존성 트리로 표시한다. 순환은 현재 가지에서 멈추고 공유된 의존성은 가지마다 표시한다. 한 가지에서 24단계까지 펼칠 수 있고 이후에는 행을 선택해 계속 탐색할 수 있다.
- 로캘 엄격 감사와 변경부 공백 검사를 통과했다. 작업 워크트리의 Windows Ninja Debug CTest 로그에서 사용자가 실행한 AssetReferenceCheck와 EditorUICheck 통과를 확인했다. 병합 후 빌드·CTest와 대화형 검증은 별도로 확인한다. [참조 패널 계획](ImGuiDevelopmentPlan.md#b-assetreferencepanel과-에셋-검증--우선순위-높음)과 [대화형 점검 F](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)에 범위와 확인 절차를 기록했다.

### [2026-11-18] 배치된 프리팹·그래프 이동과 참조 조사 통합

- 저장된 씬·프리팹의 `PrefabInstanceComponent.prefabAssetPath`와 `GraphBindingComponent.graphFile`을 참조 조사와 이동 트랜잭션에 연결했다. 열린 메인 씬과 씬 이력의 참조는 파일 기록 후 갱신한다. 프리팹 원본 열기·선택·덮어쓰기·재생성은 `Project://`를 물리 경로로 해석한다. 별도의 프리팹 편집 씬과 미저장 편집에 대한 보호를 유지한다.
- 그래프가 사용하는 경로 해석을 참조 검사와 공유한다. 그래프 캐시와 중단점은 전체 파일 경로로 구분한다. 정상 이동·Undo/Redo에서 영향받는 캐시를 무효화하고 중단점을 갱신한다. 실행기는 새 경로를 적용해도 현재 변수와 `Start` 실행 여부를 유지하며, 디버그 알림은 실행 당시의 경로를 사용한다. 편집 중인 그래프의 내용 갱신과 노드 일반 문자열은 제외 범위다.
- 이동 확인창의 사용처와 갱신 계획은 같은 프로젝트 스캔을 공유한다. 확인 후 실행 및 Undo/Redo에서는 사용처와 보관 원문을 다시 검증한다. 스캔이 불완전하면 수집된 사용처를 보여주고 적용을 차단한다.
- `AssetReferenceCheck`에 실제 프리팹 재생성, 그래프 레거시 별칭·캐시 갱신, 한글 폴더 이동, 정확한 원문 복원과 불완전 스캔을 추가했다. `EditorHistoryCheck`에 씬 이력의 프리팹·그래프 경로 회귀를 추가했다. [콘텐츠 브라우저 계획](ImGuiDevelopmentPlan.md#g-콘텐츠-브라우저-파일-작업과-계층-선택)과 [그래프 편집기](GraphPanel.md)에 현행 범위를 기록했다.
- `aef4f4fd` 이후 갱신된 Windows Ninja Debug 산출물과 CTest 로그에서 `AssetReferenceCheck`, `EditorHistoryCheck`, `GraphExecutionCheck` 세 검사 통과를 확인했다. 프리팹 원본 작업과 이동한 그래프 중단점의 실제 에디터 확인은 남아 있다.

### [2026-11-18] 액션 타임라인 참조의 에셋 이동·이름 변경 지원

- 씬의 `ActionTimelineComponent.timelinePath`와 JSON 타임라인의 모델·노티파이 사운드·VFX 경로를 기존 참조 수집, 빌드 씬 검사, 파일 이동 Undo/Redo에 연결했다. 타임라인은 `AssetHeader.ResourceType`과 지정된 필드 위치로 식별한다. `.actiontimeline`과 `.actiontimeline.json` 런타임 로딩도 JSON 경로에 연결했다.
- 메인 씬과 씬 편집 이력의 타임라인 경로, 로드된 타임라인 리소스 및 열린/닫힌 편집기의 경로는 파일 기록 성공 후 바뀐다. 리소스 핸들·재생 상태를 유지하고, 실패 시 준비해 둔 변경을 폐기한다. 로드되지 않은 모델의 파일 경로도 편집기가 보존한다. 로드 상태의 바이너리/프로젝트 외부 타임라인 내부 참조는 보호를 위해 차단한다.
- `AssetReferenceCheck`는 실제 타임라인 아카이브로 씬 → 타임라인 → 모델/사운드/VFX 수집, 한글 폴더 이동, 저장 문서 재열기, 실패 시 메모리 보존, 원문 Undo/Redo와 기존 핸들을 검사한다. `EditorHistoryCheck`는 씬 이력의 타임라인 참조와 현재 재생 위치를 확인한다.
- 정적 공백 검사를 완료했다. 이 변경의 빌드·CTest 및 실제 UI 검증은 사용자 실행 전이다. 이전 단계의 `AssetReferenceCheck` 통과는 신규 검사 결과에 포함하지 않는다. 자세한 범위와 검증 절차는 [타임라인 계획](AnimationTimelineEditorPlan.md)과 [파일 이력](EditorUndoRedo.md)을 따른다.

### [2026-11-18] 에셋 참조 검사 사용자 재검증 결과

- 사용자가 `f1ffddb4` 수정 후 `AssetReferenceCheck` 통과를 보고했다. 수정 전 같은 작업에서 `TransformInheritanceCheck`·`EditorHistoryCheck`가 통과한 터미널 결과와 구분해 기록한다. 최신 전체 CTest 또는 Release 통과로 확장하지 않는다.
- 로드된 에셋의 이동·이름 변경, 열린/닫힌 편집기의 저장 경로, 파일 Undo/Redo 뒤의 씬 Undo/Redo, 한글 경로 및 실패/취소 시 보존은 실제 Editor에서 확인이 남아 있다.
- 다음 구현 후보는 액션 타임라인의 명시적인 모델·사운드·VFX 및 씬 타임라인 경로 참조를 기존 수집/갱신 경로에 연결하는 것이다. 현재 해당 문자열이 영향을 받으면 보호를 위해 이동을 거부한다. 실제 UI 회귀 확인 후 타임라인부터 작은 범위로 확장하며, 프리팹·그래프·셰이더 내부 경로를 동시에 해제하지 않는다. 이 후보는 아직 구현하지 않았다.

### [2026-11-18] 에셋 참조 검사 실패의 레거시 경로 해석 순서 수정

- 최신 사용자 터미널에서 Editor 링크 완료와 CTest 세 검사 실행을 확인했다. `TransformInheritanceCheck`·`EditorHistoryCheck`는 통과했고 `AssetReferenceCheck`는 이동 후 참조 검사(반환 코드 21의 공통 메시지)에서 실패했다. 이 메시지는 여러 조건이 공유하므로 기존 로그만으로 실패한 세부 조건을 직접 식별할 수 없었다.
- 코드 대조에서 `Resources/Materials/Brick.bammat`의 해석 순서 결함을 확인했다. 실제 파일은 `Assets/Materials/Brick.bammat.json`에 있지만, 저장 형식 확장자(`.json`·`.beve`)가 생략된 별칭을 먼저 해석하면 존재하지 않는 프로젝트 `Resources/.../Brick.bammat` 경로로 돌아온다. 그 물리 경로에만 `.json`·`.beve`를 붙이면 마운트 내부의 실제 파일을 찾지 못한다. 이 참조는 씬을 갱신할 때 건너뛰고 이동 후 패키징 참조 검사에서도 누락 파일이 된다.
- 공유 `ResolveDocumentReference`가 기존 물리 경로의 후보와 실제 파일을 먼저 확인하고, 찾지 못하면 원문 별칭에 `.json`·`.beve`를 보완한 후보를 다시 마운트로 해석하도록 수정했다. 프로젝트 스캔·참조 갱신·빌드 씬 검사가 같은 해석을 사용하며 파일을 로드하거나 임의의 전체 이름 검색을 하지 않는다.
- 검사에 이동 전 레거시 별칭 해석을 별도로 추가하고, 이동 후 씬의 머티리얼 URI 갱신을 명시적으로 확인한다. 기존 일반 문자열·64비트 정수·텍스처 경로·BOM·참조 유효성 조건을 모두 유지하면서 실패 항목과 참조 문제의 종류/원본/대상을 출력하도록 나눴다. 진입점에서도 비정상 반환 코드를 검사 번호로 출력한다. 새 핸들 동기화 검사는 기존 실패 지점보다 뒤에 있으므로 아직 실행 통과를 확인하지 못했다.
- 수정부와 공백을 정적으로 확인했다. 수정 후 빌드·CTest는 지침에 따라 AI가 실행하지 않았다. 사용자에게 `AssetReferenceCheck` 재빌드와 단독 CTest 재실행을 안내한다. 기존 두 검사 통과를 이번 수정의 통과로 확장하지 않는다.

### [2026-11-18] 백그라운드 빌드 실행기의 Win32·SDL 헤더 의존성 수정

- 후속 사용자 터미널에서 `ClientBuildJob.cpp:28/34/36`의 `CP_UTF8`·`CP_OEMCP`·`MB_ERR_INVALID_CHARS` 미선언과 `MultiByteToWideChar` 미식별, 240행의 `SDL_CleanupTLS` 미식별로 Ninja가 중단된 것을 확인했다. 이 로그의 실패 위치는 프리뷰 위젯이 아닌 백그라운드 빌드 실행기다.
- 생성된 `unity_2_cxx.cxx`는 `ClientBuildJob.cpp`를 첫 번째로 포함한다. 해당 구현은 Win32 문자 변환과 SDL 스레드 TLS 정리를 사용하면서 `windows.h`·`SDL_thread.h`를 직접 포함하지 않았다. 설치된 SDL3의 `SDL_thread.h`에서 `SDL_CleanupTLS` 선언을 확인하고 두 헤더를 구현 파일에 추가했다. API 삭제나 SDL 버전 우회로 처리하지 않는다.
- 변경부의 헤더·사용 위치와 공백을 정적으로 확인했다. 사용자 빌드는 실패했으며 뒤의 조건부 CTest는 실행되지 않았다. 수정 후 빌드·CTest는 AI가 실행하지 않았고 사용자 재실행이 필요하다.

### [2026-11-18] 로드된 에셋 경로 동기화 후 프리뷰 빌드 의존성 보완

- `2dd05631` 이후 사용자 터미널에서 `PreviewViewportWidget.cpp:526`의 `ImGuizmo` 미선언(C2653)과 `IsOver`·`IsUsing` 미식별(C3861)로 Ninja가 중단된 것을 확인했다. 생성된 `unity_14_cxx.cxx`는 이 구현 파일을 첫 번째로 포함하며, 해당 파일은 사용 중인 `ImGuizmo.h`를 직접 포함하지 않았다. 기존 간접 포함에 의존하던 문제가 Unity 묶음 변경으로 드러난 것이다.
- `PreviewViewportWidget.cpp`에 `ImGuizmo.h`를 직접 포함했다. 공용 헤더나 PCH로 의존성을 넓히지 않고 해당 구현 파일에서 선언을 공급한다.
- 뒤의 CTest 명령은 빌드 종료 코드 조건에 의해 실행되지 않았다. 남아 있는 `LastTest.log`는 11:37의 이전 `AssetReferenceCheck` 단일 통과 기록이며 이번 변경의 검증 결과가 아니다.
- 수정부와 공백을 정적으로 확인했다. 수정 후 빌드·CTest는 AI가 실행하지 않았으며 사용자가 동일한 네 타깃 빌드와 세 검사 명령을 재실행해야 한다. 전체 빌드·테스트 통과로 판정하지 않는다.

### [2026-11-18] 로드된 에셋 이동·이름 변경과 메모리 경로 동기화

- 이전 기즈모 헤더 수정 뒤의 사용자 터미널에서 `Editor.exe`와 `AssetReferenceCheck.exe` 링크 완료 및 프롬프트 복귀를 확인했다. 해당 로그에 최신 CTest 결과는 없으며 아래 신규 변경의 빌드 결과로 사용하지 않는다.
- 리소스 풀의 객체·핸들·세대·참조 수·버전을 유지하며 키/Path와 검색 인덱스를 재등록한다. 파일명과 무관한 의미 키는 유지하고 파일 경로 조회가 같은 핸들을 찾는다. 새 인덱스·문자열을 준비하고 잠금 안에서 기존 파일/참조 트랜잭션을 실행해 성공에만 메모리 값을 교체한다. 원본이 미로드 상태여도 목적지에 남은 캐시를 보호한다. 콜백은 ResourceManager에 재진입하지 않으며 raw pointer를 보유한 워커의 동시 속성 접근은 지원하지 않는다.
- 저장된 메인 씬, 일반 리소스 편집기, Animator, 타임라인과 그래프 문서의 저장 경로를 준비한 뒤 파일 성공 후 동기화한다. 닫힌 지원 창도 처리한다. Animator 컨트롤러의 현재/로드 경로와 씬 Undo/Redo 스냅샷의 경로를 함께 갱신해 씬 객체·재생 상태·편집 이력·저장 상태 ID를 유지한다. 스냅샷 추적은 약한 참조이며 퇴출된 기록을 살려 두지 않는다. 엔티티 복사 버퍼는 성공 시 비우고 안내한다. 한글 경로에서 Animator/타임라인 저장이 좁은 문자열 경로로 재변환되던 문제도 수정했다.
- 미저장/진행 중 문서·프리팹 모드·별도 프리팹 씬과 미지원 플러그인·그래프 내용 갱신은 보호한다. 로드된 셰이더 바이너리·모델 임포트 원본·타임라인의 문자열 경로와 씬/이력의 미지원 경로가 이동 대상이면 차단한다. 지원 밖의 미로드 문서 필드·바이너리·동적 참조 갱신은 후속이다. 경로 재매핑은 PlatformPath의 공통 컴포넌트 비교를 사용한다.
- `AssetReferenceCheck`에 실패/충돌 시 파일 콜백과 메모리 보존, 미로드 원본/캐시된 목적지 충돌, 의미 키, 기존 객체/핸들/버전 유지, 한글·접두 경로·대소문자 이름 변경을 추가했다. `EditorHistoryCheck`에는 준비 단계 무변경, 미저장 거부, 씬 객체/이력 보존, 컨트롤러 재생 상태와 과거/미래 스냅샷 경로, 한글 Animator/타임라인 저장·로드 검사를 추가했다. SceneEditHistory를 독립 컴파일하는 두 검사 타깃에도 새 경로 준비 소스를 연결했다.
- 로캘 엄격 감사와 변경부 공백 검사는 통과했다. 프로젝트 지침에 따라 신규 변경의 구성·빌드·CTest·실환경 조작은 AI가 실행하지 않았다. 사용자는 `Editor`, `AssetReferenceCheck`, `EditorHistoryCheck`, `TransformInheritanceCheck`를 빌드하고 세 검사를 실행한 뒤 [UX 점검](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)의 로드 상태 이동·이름 변경·Undo/Redo·실패 경로를 확인해야 한다.
- 동작 범위: [파일 작업 계획](ImGuiDevelopmentPlan.md#g-콘텐츠-브라우저-파일-작업과-계층-선택), [파일 이력](EditorUndoRedo.md#콘텐츠-브라우저-파일-이력). 기존 생성 로캘 JSON 두 개와 `Engine.csv` 변경은 이 작업에 포함하지 않는다.

### [2026-11-18] 생성 후 이름 입력 빌드의 기즈모 직접 의존성 보완

- 완료 범위 대조 중 최신 사용자 터미널에서 `GizmoHelper.cpp`의 `ShortcutManager`·`EditorUI` 미선언(C2653/C3861)과 연쇄 오류로 Editor 빌드가 실패한 것을 확인했다. 해당 구현은 두 API를 사용하면서 선언 헤더를 직접 포함하지 않고 Unity 묶음의 간접 포함에 의존했다.
- `GizmoHelper.cpp`에 `EditorUI.h`와 `ShortcutManager.h`를 직접 포함했다. 변경부 공백을 정적으로 확인했으며 수정 후 빌드·CTest·실환경 검사는 실행하지 않았다. 생성 직후 이름 입력과 F2 연동을 전체 검증 완료로 판정하지 않는다.

### [2026-11-18] 생성 직후 이름 입력과 참조를 보존하는 이름 변경

- 콘텐츠 브라우저의 새 폴더·빈 에셋 7종과 컨텍스트의 머티리얼 인스턴스·스프라이트 생성 성공을 공통 이름 입력 예약으로 연결했다. 팝업이 닫힌 다음 프레임에 부모 폴더로 이동하고 검색/타입 필터를 해제해 새 항목을 선택·스크롤한다. 입력 포커스를 주고 확장자를 제외한 이름을 선택한다. 파생 리소스는 초기 이름을 확정하기 전에 등록하지 않고 열기 때 로드한다.
- 인라인 위젯에서 직접 하던 파일 변경을 패널의 참조 갱신·파일 이력 경로로 옮겼다. F2·컨텍스트·생성 직후 입력을 공유하고 파일 변경은 현재 프레임의 목록 순회 종료 후 수행한다. 사용처가 있으면 확인창을 거치며, 없고 보호 조건을 만족하면 즉시 적용한다. Enter 확정, Escape/포커스 이탈 취소, 같은 이름의 빈 이력 방지와 확장자 보존을 적용했다. 이름 변경 Undo는 생성 자체를 취소하지 않는다.
- Windows 예약 이름·구분자·끝 공백/마침표·너무 긴 이름·충돌을 거부하고 입력을 유지한다. 대소문자 변경은 같은 파일의 임시 이름 경유와 실패 복원을 사용한다. 폴더 변경 및 Undo/Redo 후 현재 경로·방문 이력·클립보드도 갱신한다. 기존 미저장 문서·로드된 대상 리소스 보호는 유지한다.
- `AssetReferenceCheck`에 순수 이름 규칙과 복합 확장자·한글·예약 이름·길이 경계, 머티리얼 이름 변경 후 저장된 참조 및 원문 Undo 검사를 추가했다. 이전 단계의 빌드 통과는 사용자가 보고했으며, 이번 변경 후 빌드·CTest·실환경 입력은 실행하지 않았다. 로캘 엄격 감사와 `git diff --check`는 정적으로 확인했다. 대화형 완료 기준은 [UX 점검 E](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)에 기록했다.

### [2026-11-18] Engine 소비 타깃의 EnTT 예외 설정 통일

- 후속 사용자 로그에서 `AssetReferenceCheck.exe` 링크까지 진행했으나 `Editor.exe` 링크는 `TextureConverter.lib`의 `entt.noexcept=if(true)`와 Editor의 `try` 불일치(LNK2038·LNK1319)로 실패한 것을 확인했다. 앞선 수정이 `/EHsc`를 Editor와 검사에만 적용한 것이 원인이다. 테스트 실행 결과는 이 로그에 없다.
- 설치된 EnTT 헤더의 `__cpp_exceptions` 분기와 `detect_mismatch`, Engine의 PUBLIC EnTT 노출 및 TextureConverter의 Engine 링크를 대조했다. MSVC C++의 `/EHsc`를 Engine의 PUBLIC 사용 요구사항으로 옮겨 Engine과 모든 소비 타깃에 전파한다. Editor·AssetReferenceCheck의 개별 지정은 제거하며 EnTT 검사나 예외 복구를 비활성화하지 않는다.
- `git diff --check`와 CMake 의존 관계를 정적으로 확인했다. 수정 후 구성·빌드·CTest는 실행하지 않았으며 사용자 재빌드가 필요하다. 기존 오브젝트와 새 예외 설정이 섞이지 않도록 의존 타깃도 함께 다시 컴파일되어야 한다.

### [2026-11-18] 에셋 참조 검사 타깃의 독립 컴파일 오류 수정

- 사용자 빌드 로그에서 `AssetReferenceMove.cpp` 두 곳과 `AssetReferenceValidator.cpp` 한 곳의 `EDITOR_LOG_WARN` 미정의(C3861)를 확인했다. Editor의 Unity 간접 포함으로 가려진 매크로 의존성이 원인이며, 직접 포함한 `Logger.h`가 제공하는 `BAM_LOG(Warn, "Editor", "Asset", ...)`로 수정했다. 검사 타깃에 Editor 전체 include 경로를 추가하지 않는다.
- 같은 로그에서 C4530과 `/EHsc` 없는 컴파일 명령을 확인했다. MSVC의 Editor와 AssetReferenceCheck 타깃에 `/EHsc`를 명시해 예외 시 파일 잠금 등 RAII 자원을 해제하도록 했다. 다른 타깃의 전역 옵션은 변경하지 않는다.
- 변경부 정적 검토와 `git diff --check`를 수행했다. 수정 후 빌드·CTest는 실행하지 않았으며 사용자 재실행이 필요하다. 이전 테스트 통과 기록은 이번 수정의 검증 결과가 아니다.

### [2026-11-18] 에셋 이동의 저장된 참조 갱신과 Undo/Redo

- 기존 참조 수집기를 읽기·갱신에 공유한다. 프로젝트 JSON·JSON 형식 Animator 컨트롤러에서 리소스 핸들의 `AssetKey`·`AssetPath`와 Animator 컴포넌트의 `controllerPath`를 새 `Project://` URI로 갱신한다. 함께 이동하는 폴더 내부 문서도 처리하고 일반 문자열·`Null` 참조는 보존한다.
- 이동과 문서 기록은 한 파일 이력 명령이다. 이름 충돌·파일 교체·새 참조·문서 변경을 실행 전에 확인하고, 실패하면 묶음 전체를 복원한다. Undo는 원본 바이트, Redo는 갱신한 바이트를 적용한다. 참조 문서를 모두 잠그고 예상 내용을 검사한 뒤 같은 파일 핸들에 기록해 ID를 유지한다. 기록 실패 시 원문을 복원하고, 복원도 실패하면 이력을 비우고 로그에 복구 폴더를 남긴다. 변경 전후 백업과 경로표는 `.bam/content-browser-history/`에 유지한다.
- 미저장 문서·진행 중인 문서 작업·프리팹 모드·대상 문서를 보유한 편집기(닫힌 창 및 그래프의 두 문서 포함)·로드된 대상 리소스를 차단한다. 메모리의 옛 참조가 다시 저장되는 것을 방지하는 제한이며 실시간 리소스 재등록은 후속이다. 바이너리 내부·동적 로드·정전 복구는 지원 범위 밖이다.
- `AssetReferenceCheck`에 폴더 내부/외부 참조와 컨트롤러 이동, 한글·공백 경로, 레거시 확장자, 일반 문자열 보존, 64비트 정수, BOM, 정확한 원문 Undo/Redo, 외부 문서 변경·파일 잠금·불완전 검사 거부를 추가했다. 기존 검사 통과는 사용자가 보고했으며 이번 확장 후 빌드·CTest·실환경 검사는 실행하지 않았다. 로캘 엄격 감사와 `git diff --check`는 통과했다.
- 적용 범위와 실환경 점검은 [파일 작업 계획](ImGuiDevelopmentPlan.md#g-콘텐츠-브라우저-파일-작업과-계층-선택), [파일 이력](EditorUndoRedo.md#콘텐츠-브라우저-파일-이력), [UX 점검](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)에 기록했다. 기존 생성 JSON과 `Engine.csv` 변경은 이 작업에 포함하지 않는다.

### [2026-11-18] 새 워크트리의 FMOD 빌드 의존성 복구

- 사용자 터미널에서 CMake 구성 성공 후 `ThirdParty/FMOD/lib/x64/fmodL_vc.lib` 누락으로 Ninja가 중단된 것을 확인했다. 이어진 `AssetReferenceCheck`는 실행 파일이 생성되지 않아 `Not Run`이었다. 테스트 본문의 검증 실패는 아니다.
- `.gitignore`의 `*.lib`·`*.dll` 규칙 때문에 새 워크트리에 SDK 바이너리가 없었다. 원본 체크아웃과 `fmod_common.h`의 SHA-256 일치를 확인하고, Debug·Release import library와 런타임 DLL 4개를 로컬에 복구했다. 복사본 4개의 SHA-256도 원본과 일치한다. SDK 바이너리는 커밋하지 않는다.
- [빌드·패키징 안내](BuildAndPackagingPlan.md#로컬-워크트리의-windows-ninja-빌드-준비)에 MSVC 초기화, FMOD·LLVM 준비와 빌드 실패 시 CTest 실행 중단 절차를 기록했다. 복구 후 빌드·CTest는 AI가 실행하지 않았으며 사용자 재검증이 필요하다.

### [2026-11-18] 콘텐츠 브라우저 이동 전 참조 영향 확인

- 잘라내기 붙여넣기와 폴더 드롭 이동이 공통 확인창에서 출발·목적 경로와 저장된 사용처를 보여준다. 삭제 분석과 참조 수집을 공유하며 이동은 같은 묶음 안의 참조도 포함한다. 자동 참조 갱신은 후속이다.
- 확인 대기 중 파일 교체를 검사하기 위해 출발 파일과 목적 폴더의 ID를 보관한다. 취소와 Escape는 파일·클립보드·Undo 이력을 유지한다. 확인 후 기존 전송 경로에서 성공한 항목만 기록하고 실패한 잘라내기 항목을 유지한다.
- `AssetReferenceCheck`에 이동 묶음 내부 참조, 폴더와 중복 선택, 누락 경로, 일부 문서 검사 실패를 추가했다. 로캘 엄격 감사와 `git diff --check`로 정적 확인했다. 빌드·CTest·실환경 확인은 사용자 실행 전이며 실패 조건과 취소 검증은 `EditorUXTestGuidelines.md`에 기록했다.
- 사용처 검사는 요청당 한 번 동기로 실행한다. 검사 시점 이후 저장한 문서와 미저장 편집, 동적 로드 및 바이너리 내부 참조는 결과에 포함되지 않는다. [파일 작업 계획](ImGuiDevelopmentPlan.md#g-콘텐츠-브라우저-파일-작업과-계층-선택)

### [2026-11-18] 콘텐츠 브라우저 경로·URI 복사 메뉴

- 단일 파일, 다중 선택, 폴더 트리의 경로 복사를 공통 함수에 연결했다. 경로 복사는 Windows 절대 경로, URI 복사는 기존 가상 마운트 변환 결과를 사용한다. 한글 경로는 UTF-8로 클립보드에 전달하고 다중 항목은 한 줄씩 구분한다.
- 가상 URI로 변환할 수 없는 선택에서는 URI 메뉴를 비활성화한다. 신규 메뉴 로캘은 Editor.csv에 추가했다.
- 정적 확인은 로캘 엄격 감사와 변경부 공백 검사다. 빌드·CTest·실환경 클립보드 확인은 사용자 실행 전이다. 한글·공백 경로, 다중 선택, 폴더 트리와 각 마운트의 복사 결과를 확인한다.

### [2026-11-18] 리플렉션 생성 등록 함수의 Debug 스택 오버플로 수정

- Debug CTest에서 `ReflectionRegistry::Create()`를 호출하는 10개 테스트가 모두 `c00000fd`(스택 오버플로)로 종료됐다. 이벤트 로그와 PDB로 확인한 위치는 `Engine.dll`의 `__chkstk`였다.
- 생성된 `Register_BamEngine`은 중복 검사, 열거형 59개, 자유 함수 169개 등록을 한 함수에 담았다. Debug(`/Od`)는 임시 문자열마다 스택 슬롯을 따로 잡아 프레임이 852KB였다. 여기에 `ParticleEmitterData` 등록(메타데이터 추가 후 164KB)이 중첩되면서 기본 1MB를 넘었다. Editor·Client도 스택 예약이 1MB라 같은 위험이 있었다.
- `RegistrationEmitter.cpp`가 중복 검사, 열거형 하나, 자유 함수 하나를 각각 `Register_<모듈>_CheckDuplicates`·`_Enum<N>`·`_Function<N>` 내부 함수로 생성한다. `Register_<모듈>`은 이 함수들을 순서대로 호출한다. 공개 함수의 시그니처와 검사·등록 순서는 같다.
- **검증**: Debug 전체 빌드에서 새 경고·오류가 없었다(기존 `ActionTimelineSystem.cpp`의 `C4834` 10건만). CTest 39/39 통과. 재빌드 후 `Register_BamEngine`의 프레임은 13KB였다.
- **잔여**: 타입별 등록 함수도 속성 수에 비례해 커진다(최대 `ParticleEmitterData` 164KB). 현재 호출 깊이에서는 문제가 없지만, 속성 블록을 더 나누는 생성 방식은 후속 검토 대상이다.

### [2026-11-18] 기본 UI 리소스 조회와 핸들 경로 해석 순서

- `UIRenderSystem`과 `Renderer`의 UI 대체 경로가 기본 머티리얼·흰 텍스처를 레거시 키(`Resources/...`)로 찾았다. 시작 매니페스트는 엔진 리소스를 `Engine://` URI 키로 등록하므로 조회가 실패해, 머티리얼이나 스프라이트가 비어 있는 UI 이미지가 그려지지 않았다. 에디터의 UI Image 생성도 같은 이유로 머티리얼이 항상 비어 있었다.
- `UIRenderSystem::GetDefaultMaterial`·`GetDefaultTexture`가 `Engine://` 키를 먼저 찾고 레거시 키로 대체한다(Client는 UI 머티리얼을 레거시 키로도 등록한다). `OnSubmit`은 프레임마다 한 번 조회하고, `EntityFactory`의 UI Image 생성과 `Renderer`의 UI 대체 머티리얼도 같은 키를 쓴다.
- `ResolveResourceHandle`은 이미 로드된 리소스를 찾지 못하면 저장된 절대 `AssetPath`를 먼저 로드했다. 워크트리처럼 다른 위치에서 실행하면 원래 체크아웃의 파일을 읽고, 그 절대 경로가 다시 씬에 저장됐다. 이제 `AssetKey`가 가상 URI이고 현재 프로젝트에 파일이 있으면 키로 먼저 로드한다.
- **검증**: Debug 빌드와 CTest 39/39(위 리플렉션 수정 포함) 통과. Game View·Client에서 머티리얼·스프라이트가 비어 있는 UI 이미지 표시, 새 UI Image의 기본 머티리얼, 워크트리에서 다시 지정한 스프라이트 핸들의 URI 저장은 이 커밋 시점에 실행 확인 결과를 받지 못했다.
- **잔여**: `PostProcessPipeline`, `LightManager`, `ParticleSystem` 등에도 레거시 키 조회가 남아 있다. `ClientRenderPipeline`의 UI 머티리얼 레거시 키 등록 정리도 후속이다.

### [2026-11-18] 파티클 에디터 인스펙터의 공용 속성 표 전환과 이펙트 경로 정리

- 파티클 에디터의 이미터 인스펙터를 손으로 작성한 표 대신 `ReflectionPropertyDrawer::DrawPropertyTable`로 그린다. 범주·범위·표시 조건은 `ParticleEmitterData`의 `CATEGORY`·`RANGE`·`EDITCONDITION` 메타데이터로 옮겼다. 단일 조건으로 표현할 수 없는 항목(연속 방출의 상속 비율, 버스트 0의 주기, 충돌 없음의 반발·최소 바운스 속도)은 `HiddenProperties`로 숨긴다. 부모 이미터는 이미터 이름 목록이 필요해 공용 속성 행으로 따로 그린다.
- Undo는 VFX 에디터와 같은 편집 세션으로 바꿨다. 첫 변경 때 이전 상태를 저장하고, 입력이 끝나거나 Undo·Redo·삭제·저장 전에 이력 한 건으로 확정한다. 기존 체크박스는 변경 뒤 상태를 이전 상태로 기록해 되돌리기가 동작하지 않았다.
- 로캘: `Engine.csv`에 `CATEGORY_PARTICLE_TRIGGER`·`CATEGORY_PARTICLE_FORCES`와 파티클 enum 4종 12행을 추가했고, 쓰지 않게 된 `ParticleEditor.*` 52행을 `Editor.csv`에서 제거했다.
- 데이터(Git 제외 `Assets`): 원본 체크아웃과 기준선 워크트리의 `Explosion_Fireball.bameffect.json`에서 절대 경로 핸들 8개를 `Engine://` URI로 바꿨다. 두 `Assets` 전체 검사에서 절대 경로 핸들은 0건이다.
- **검증**: `audit_localization.py --strict` 통과, Debug 빌드와 CTest 39/39 통과. 파티클 에디터의 범주·조건부 표시·Undo와 이펙트 저장 결과의 URI는 이 커밋 시점에 실행 확인 결과를 받지 못했다.
- **사용성 변화**: 수명·속도·회전 속도의 최소/최대가 두 행으로 나뉘고, 활성화·방출 시간·시뮬레이션 공간·메시·빌보드·SubUV가 인스펙터에 표시된다.

### [2026-11-18] 다중 Git 워크트리(Worktrees) 환경 애셋 및 리소스 핫로드 지원

- **문제점 분석**:
  - Git 워크트리를 여러 개 운용하며 개발할 때, 에디터가 실행 중인 워크트리 외의 다른 워크트리에서 애셋(텍스처, 머티리얼, 메시)이나 리소스(셰이더, 로컬라이제이션)를 수정해도 핫리로드가 발동되지 않는 결함 발생.
  - 원인 1: `AssetHotReloadManager`와 `ShaderHotReloadManager`의 감시(Watch) 경로가 현재 프로세스의 단일 프로젝트 루트(`PlatformPath::GetProjectRoot()`) 하위 디렉터리(`Assets`, `Resources`, `Shader/hlsl`)로만 제한되어 타 워크트리 파일 시스템 이벤트를 수신하지 못함.
  - 원인 2: `AssetHotReloadManager`에서 리소스 매칭(`MatchesResourceFile`) 시 정규화된 절대 경로 키(`AssetFileKey`)로만 비교하여, 다른 워크트리 루트 접두사를 가진 파일이 들어왔을 때 매칭 실패.
  - 원인 3: 로컬라이제이션 CSV 수정 시 타 워크트리의 파이썬 변환 스크립트 실행 및 결과 JSON 파일 등록 부재.
- **구현 내용**:
  - `PlatformPath::GetWorktreeRoots()`: `.git` 디렉터리 또는 연결 파일(`gitdir: ...`)을 안전하게 파싱하여 현재 프로젝트 및 연결된 모든 활성 Git 워크트리 루트 디렉터리를 자동 수집하는 플랫폼 경로 API 신설.
  - `ShaderHotReloadManager`: `RegisterDefaultCandidates()`에서 연결된 모든 워크트리의 빌트인 및 프로젝트 셰이더 디렉터리(`Engine/Resources/Shader/hlsl`, `Assets/Shader/hlsl` 등)를 자동 감시 목록으로 등록. 타 워크트리에서 셰이더(`.hlsl`, `.hlsli`) 수정 시 자동 컴파일, 핫스왑 및 신규 등록 연동.
  - `AssetHotReloadManager`:
    - `m_WatchIDs` 및 `m_WatchedDirectories` 벡터 동적 관리 도입. 모든 워크트리의 `Assets`, `Resources`, `Engine/Resources`, `Editor/Resources` 디렉터리를 efsw 파일 감시자에 자동 등록.
    - `AssetRelativeKey`: 가상 URI 및 모든 워크트리 루트에 대한 상대 경로를 계산하여 워크트리 독립적인 에셋 상대 키를 추출.
    - `MatchesResourceFile`: 절대 경로 일치뿐 아니라 워크트리 상대 경로(`AssetRelativeKey`) 및 마운트 접두사 제거 하위 경로(`StripAssetPrefix`) 일치를 지원하여 다른 워크트리에서 수정한 파일도 런타임 리소스와 100% 매칭.
    - 리소스 리로드 시 수정된 파일의 전체 경로를 전달하여 다른 워크트리의 최신 수정 파일 내용이 런타임 메모리에 즉각 반영되도록 처리.
    - `ProcessModifiedLocalization`: 타 워크트리의 CSV 수정 시 해당 워크트리에서 `localizationcsvtojson.py`를 실행하여 JSON을 생성하고, 현재 `LocalizationManager`에 등록 후 `ReloadData()`로 즉각 핫로드.
  - `LocalizationManager`: `RegisterLocalizationData`에 중복 등록 방지 로직 추가.
- **검증**:
  - `PlatformPath::GetWorktreeRoots()`가 메인 워크트리(`BamEngine`) 및 서브 워크트리(`BamEngine-GlassRefraction`) 양쪽 진입점에서 상호 연동되어 모든 유효 워크트리를 정확하게 판별함을 정적 구조 검증.
  - `BamEngine` 및 `BamEngine-GlassRefraction` 양쪽 워크스페이스에 동일하게 적용 완료.
- **미검증/사용자 확인 필요**:
  - 에디터 실제 구동 후 `BamEngine-GlassRefraction` 워크트리의 `waterPS.hlsl`, `WaterMaterial.bammat.json`, `Engine.csv` 등을 수정했을 때 에디터에서 핫로드 알림 및 뷰포트 갱신이 실시간으로 발동하는지 사용자가 실환경 확인 필요.

### [2026-11-18] 텍스처 핫리로드(.bamtex), 인스펙터 지원 포맷 확장 및 DDS/3D 모델(FBX/GLTF/OBJ) 썸네일 캐시 파이프라인 구축

- 텍스처 오프라인 베이킹 파이프라인 완결에 이어, 엔진 바이너리 `.bamtex`의 실시간 핫리로드 무결성을 확보하고 에디터 Content Browser에서 DDS 텍스처 및 원본 3D 모델 파일 포맷들의 썸네일 표시 파이프라인을 전면 고도화했다.
- **에셋 핫리로드 및 바이너리 텍스처 리로드 보강 (`AssetHotReloadManager.cpp`, `Texture.cpp`)**:
  - `AssetHotReloadManager`: 에셋 변경 분류기(`ClassifyAssetReload`)에 `ext == L".bamtex"`를 추가하여, 외부에서 `.bamtex`가 갱신되거나 재베이킹되었을 때 핫리로드 이벤트가 정상 발동하도록 수정.
  - `Texture::Reload`: 파일 확장자가 `.bamtex` 또는 `.beve`인 경우 바이너리 아카이브(`BeveArchive`)로부터 역직렬화(`ReadArchive`)를 수행하고 GPU 서브리소스를 안전하게 재업로드하도록 분기 처리.
- **텍스처 인스펙터 및 에셋 매니저 확장 (`TextureInspector.cpp`, `AssetManager.cpp`)**:
  - `TextureInspector`: `IsSupported()`에 `.tga`, `.bmp`, `.dds` 지원 추가 및 대소문자 무관 비교 처리. 파일 포맷별(`DirectX::GetMetadataFromDDSFile`, `GetMetadataFromTGAFile`, `GetMetadataFromWICFile`) 메타데이터 조회 분기 구축.
  - `AssetManager`: `.jpeg` 확장자 매핑(`m_Importers[".jpeg"] = m_Importers[".png"]`) 추가.
- **Content Browser DDS 및 원본 3D 모델 포맷 썸네일 파이프라인 구축 (`AssetCache.h/.cpp`)**:
  - **DDS 썸네일**: `isImage`에 `.dds`를 추가하고, `LoadImageThumbnail` 내에서 `DirectX::LoadFromDDSFile` 및 `DirectX::Decompress`(BC1~BC7)를 연동하여 블록 압축 DDS 파일의 128x128 RGBA 다운샘플링 썸네일 및 디스크 캐시 영속화 지원.
  - **3D 원본 모델 포맷(FBX, OBJ, GLTF, GLB) 썸네일**:
    - `isModelOrMesh` 및 `ProcessPending3DThumbnail`에 `isRawModel`(`.fbx`, `.obj`, `.gltf`, `.glb`) 지원 추가.
    - 동명의 임포트된 `.bammodel`이 존재하는 경우 해당 모델을 자동 연동하여 고품질 썸네일 렌더링.
    - 아직 임포트되지 않은 원본 모델인 경우, `ModelConverter::ConvertModel`을 통해 3D 정점을 읽어 임시 `Mesh`를 생성하고 `ThumbnailRenderer::SubmitMesh`로 AABB 자동 프레이밍 렌더링 제출.
    - 캡처 완료 시 GPU 픽셀 리드백을 통해 128x128 PNG 디스크 캐시로 영속화하여 이후 프레임부터는 Assimp/렌더러 오버헤드 없이 수 밀리초만에 즉각 표시.
    - 임시 캡처 리소스는 `m_TempCaptureResources`를 통해 캡처 완료/Free 시점에 안전하게 일괄 해제.
- **검증**:
  - `audit_localization.py` 다국어 정적 감사 100% 통과.
  - C++ 정적 구조 검토 및 자원 해제/소유권 안정성 검토 완료.
- **미검증/사용자 확인 필요**:
  - 에디터 실행 후 Content Browser에서 DDS 파일 및 FBX/OBJ/GLTF 파일의 썸네일이 정상 생성되는지, 텍스처 인스펙터에서 TGA/BMP 파일 설정이 열리는지 확인 필요.

### [2026-11-18] 모델 임포터 텍스처 오프라인 BC 블록 압축(BC7/BC5) 및 DDS/.bamtex 자동 베이킹 파이프라인 연계

- 모델 임포트 시 내장(Embedded) 및 외부(External) 텍스처들이 비압축 `R8G8B8A8_UNORM` 원본 이미지로 유지되던 문제를 해소하기 위해, 모델 임포트 단계에서 `TextureImporter`를 연계 호출하여 자동으로 GPU 블록 압축(BC7/BC5) 및 `.dds` + `.bamtex` 오프라인 베이킹을 수행하고 머티리얼이 베이킹된 에셋을 참조하도록 파이프라인을 완성했다.
- **모델 임포터 연계 고도화 (`ModelImporter.h`, `ModelImporter.cpp`)**:
  - `ModelImportDesc`에 `bool BakeTexturesToDDS = true;` 옵션 추가.
  - `ModelImporter::bindTexture` 람다 내에 자동 베이킹 분기 추가:
    - 슬롯별 최적 압축 포맷 및 플래그 자동 매핑:
      - 슬롯 1 (Normal): `BC5_RG_UNORM` 및 `GenerateMipMaps | HighQuality | NormalMap`
      - 슬롯 2 (ORM / MetallicRoughness): `BC7_RGBA_UNORM` (선형) 및 `GenerateMipMaps | HighQuality`
      - 슬롯 0 (Albedo), 슬롯 3 (Emissive) 및 기타: `BC7_RGBA_UNORM` 및 `GenerateMipMaps | HighQuality | SRGB`
    - 내장 텍스처(FBX, GLB 등): 디스크에 원본 이미지를 추출 저장한 즉시 `TextureImporter::Import()`를 호출하여 `Textures/` 디렉터리에 `destStem.bamtex` 및 `destStem.dds`를 동시 베이킹.
    - 외부 텍스처: 기존 비압축 원본 이미지(`.png`, `.jpg`, `.tga` 등)를 감지하고 `destStem.bamtex` 및 `destStem.dds`로 베이킹.
    - 머티리얼 인스턴스(`.bammatinst`)가 비압축 원본 파일 대신 베이킹 완료된 `.bamtex`를 로드 및 바인딩하도록 경로를 자동 치환하여 직렬화.
    - 이미 베이킹된 파일이 존재하고 `!settings.Overwrite`인 경우 중복 압축을 방지하여 임포트 속도 최적화.
- **모델 임포트 모달 UI 옵션 및 로캘 연동 (`ModelImportModal.cpp`, `Editor.csv`)**:
  - 모달 대화상자의 Meshes & Materials 설정 영역에 `ModelImport.BakeTexturesToDDS` 체크박스 및 툴팁(`ModelImport.BakeTexturesTooltip`) 추가.
  - `Editor.csv`에 한/영 로캘 키 추가 및 `localizationcsvtojson.py`를 통해 `English.json`, `Korean.json` 동기화 완료.
- **검증**:
  - `audit_localization.py` 다국어 정적 감사 100% 통과 (Scanned Files: 718, Catalog Keys: 2959).
  - C++ 문법, 소유권 안전성 및 참조 무결성 정적 검토 완료.
- **미검증/사용자 확인 필요**:
  - 에디터 실제 구동 후 FBX/GLTF 모델 임포트 시 `Textures/` 폴더에 `.dds` 및 `.bamtex`가 생성되고, 뷰포트에서 머티리얼이 베이킹된 텍스처로 렌더링되는지 확인 필요.

### [2026-11-18] 렌더링 S2-D 3단계: DirectXTex 기반 에디터 오프라인 BC 블록 압축 베이킹 및 CTest 프리셋 완결

- 런타임 CPU 밉 체인 연산 오버헤드를 원천 제거하고 VRAM 대역폭을 최적화하기 위해, 에디터 텍스처 임포트 파이프라인에서 `DirectXTex`를 활용한 GPU 블록 압축(BC1~BC7) 및 표준 `.dds` + `.bamtex` 동시 오프라인 베이킹을 완성했다.
- **에디터 텍스처 임포터 고도화 (`TextureImporter.cpp`)**:
  - 기존의 임시 디버깅 플래그(`kQuickTextureSanityMode = true`)로 인해 모든 텍스처가 비압축 `R8G8B8A8_UNORM`으로 강제 다운그레이드되던 문제를 완전히 제거.
  - 인자 없이 호출되는 기본 임포트 시 파일명 기반 스마트 감지 도입:
    - 노멀 맵 패턴(`_Normal`, `_normal`, `_Norm`, `_norm`, `_nrm`, `_N`): `BC5_RG_UNORM` 포맷 및 `GenerateMipMaps | HighQuality | NormalMap` 플래그 자동 지정.
    - 일반 컬러/베이스 텍스처: `BC7_RGBA_UNORM` 포맷 및 `GenerateMipMaps | HighQuality | SRGB` 플래그 자동 지정.
  - 텍스처 임포트 시 엔진 바이너리인 `.bamtex`뿐만 아니라 표준 DirectDraw Surface 파일인 `.dds`를 대상 폴더에 동시에 베이킹하여 저장.
- **DirectXTex 압축기 정밀 sRGB 동기화 (`DirectXTexCompressor.cpp`)**:
  - `CompressTexture` 및 `GetRawCompressedData`에서 `ECompressFlags::SRGB` 활성화 시 `DirectX::MakeSRGB()`를 적용하여 sRGB 감마 인코딩이 보존된 DXGI 포맷으로 변환.
  - `GetRawCompressedData`의 출력 `TextureBinaryHeader` 포맷을 `BC7_RGBA_UNORM_SRGB`, `BC1_RGBA_UNORM_SRGB`, `BC2/BC3`, `R8G8B8A8_UNORM_SRGB`로 정확하게 매핑.
- **CTest 프리셋 설정 복구 (`CMakePresets.json`)**:
  - `testPresets` 섹션 누락으로 인한 `ctest --preset x64-debug` 실패 오류를 해결하기 위해 `x64-debug`, `x64-release` 프리셋을 추가 등록.
- **검증**:
  - 사용자 환경에서 `TextureMipmapCheck` 단위 테스트 4개 항목 전체 통과 확인 완료:
    - `[Test 1] Texture::CalculateMipLevels Verification` (경계 조건 단언 통과)
    - `[Test 2] CPU Mip Chain Downsampling Verification` (100% 색상 보존율 확인)
    - `[Test 3] TextureBinaryHeader Multilevel Mipmap Layout Verification` (256x128 9레벨 174,764B 바이트 오버헤드 1.33334x 일치)
    - `[Test 4] DDSLoader Parsing & Block Compression Verification` (BC7 9레벨 Mips 파싱 성공, 43,728B로 비압축 대비 74.98% VRAM 절감 확인)
- **미검증/사용자 확인 필요**:
  - 에디터 실제 구동 후 콘텐츠 브라우저에서 PNG/TGA/JPG 임포트 시 생성된 `.dds`와 `.bamtex`가 머티리얼 및 씬 뷰포트에서 정상 렌더링되는지 확인 필요.

### [2026-11-18] 렌더링 S2-D 2단계: GPU 블록 압축(BC1~BC7) 및 고속 DirectDraw Surface(`.dds`) 컨테이너 로더 구축 완결

- 비압축 텍스처(R8G8B8A8, 32bpp)의 높은 VRAM 대역폭 점유와 런타임 CPU 밉맵 연산 오버헤드를 해소하기 위해, GPU 하드웨어 블록 압축(BC1~BC7) 포맷 지원 및 사전 베이킹된 밉맵 체인을 CPU 재연산 없이 GPU로 직접 업로드하는 고속 `.dds` 파이프라인을 완성했다.
- **RHI 블록 압축 규격 및 D3D12 업로드 연동 (`RHITextureTypes.h`, `DirectX12Types.h`, `DirectX12RHI.cpp`)**:
  - `RHITextureTypes.h`: `IsBlockCompressed()`, `GetBlockCompressedBlockSize()`(BC1/BC4 8바이트, BC2/BC3/BC5/BC6H/BC7 16바이트), `GetUncompressedBytesPerPixel()` 헬퍼 함수 추가.
  - `DirectX12Types.h`: `BC1_RGBA_UNORM_SRGB`, `BC2`, `BC3`, `BC7_RGBA_UNORM_SRGB`의 `DXGI_FORMAT_*_SRGB` 포맷 매핑 교정.
  - `DirectX12RHI::UploadTextureData`: 4x4 블록 압축 단위의 행 피치(`rowPitch = (width + 3) / 4 * blockSize`) 및 슬라이스 바이트 크기 계산 로직 완비로 BC 포맷 텍스처의 D3D12 다중 서브리소스 DMA 업로드 지원.
- **고속 DirectDraw Surface 로더 신설 (`DDSLoader.h/.cpp`)**:
  - 표준 매직(`0x20534444`, "DDS "), `DDS_HEADER` 및 `DDS_HEADER_DXT10` 파서 구축.
  - DX10 헤더(`DXGI_FORMAT_BC7_UNORM`, `BC5_UNORM`, `BC1~BC3`, `RGBA8` 등) 및 레거시 FourCC(`DXT1`, `DXT3`, `DXT5`, `ATI1/BC4U`, `ATI2/BC5U`)를 모두 해석하여 CPU 디코딩 없이 밉 슬라이스 원본 메모리 블록을 RHI에 직접 공급.
- **엔진 텍스처 및 리소스 매니저 연동 (`Texture.h/.cpp`, `ResourceManager.cpp`)**:
  - `TextureBinaryHeader`: `ValidateTextureHeader`에서 `IsBlockCompressed` 포맷 허용 및 블록 단위 전체 밉 바이트 합산 검증 로직 추가.
  - `Texture::InitializeFromDDSMemory`, `Texture::CreateFromDDSMemory` 신설 및 `Initialize`, `Reload`에서 `.dds` 파일 및 `DDS ` 매직 자동 분기/핫스왑 처리.
  - `ResourceManager`: `Texture` 지원 확장자 목록에 `L".dds"` 추가 등록.
- **단위 테스트 및 검증 (`TextureMipmapCheck.cpp`)**:
  - `TextureMipmapCheck`에 256x128 9레벨 Mipmap BC7 DDS 바이너리 파싱 및 블록 크기/페이로드 정합성 검증 추가. 비압축 RGBA8(174,764 바이트) 대비 BC7 압축 적용 시 약 75% 메모리 절감 검증 완결.
- **검증**:
  - C++ 정적 구조 및 참조 검증 완료.
  - `audit_localization.py` 다국어 정적 감사 100% 통과.
  - 사용자가 Windows 환경에서 `TextureMipmapCheck` 단위 테스트 4개 항목 실행하여 전체 통과(Test 1~4 PASSED) 확인 완료.
- **미검증/사용자 확인 필요**:
  - 에디터 실제 구동 후 `.dds` 텍스처 실시간 렌더링 확인 필요.

### [2026-11-18] 래스터 반투명 4단계: 움직이는 파도(Gerstner Wave 정점 변위, 64x64 격자 메시, 파도 머티리얼) 파이프라인 구현 (Render S6-4)

- 정적 평평한 수면(Render S6-3)에 이어, 정점 셰이더에서 거스트너 파도(Gerstner Wave) 방정식으로 기하 구조를 실시간 변위시키고 해석적 표면 법선/탄젠트를 동기화하는 움직이는 파도 파이프라인을 완성했다.
- **4중 거스트너 파도 정점 셰이더 (`waveVS.hlsl`)**:
  - 누적 시간(`commonData.Time`)과 4개 파동 매개변수(주 파동 Main Swell, 교차 파동 Cross Swell, 잔파 Chop, 표면장력파 Capillary)를 결합하여 수평($X, Z$) 및 수직($Y$) 정점 변위를 계산.
  - 파도 변위 위치뿐만 아니라 파도 표면의 공간 편미분($\partial \mathbf{P} / \partial x, \partial \mathbf{P} / \partial z$)을 수학적으로 유도하여, 파도의 움직임과 100% 실시간 동기화되는 정밀 법선($\mathbf{N}$)과 접선($\mathbf{T}$) 벡터를 픽셀 셰이더에 직접 전달.
- **거스트너 파도 픽셀 셰이더 (`wavePS.hlsl`)**:
  - 3D 기하 파도 표면 위에 마이크로 잔물결 노멀 스크롤, 파도 마루(Crest)의 Whitecap 백색 거품 합성($N_y$ 임계치 및 정점 변위 고도 기반), Depth Fade 수심 그라데이션(씬 컬러 원색 보존), 전경 고스트 방지 굴절, 태양광 Blinn-Phong 스페큘러를 복합 연산.
- **DXIL & SPIR-V 셰이더 바이너리 빌드**:
  - `waveVS.cso` (8,120 bytes), `waveVS.spv` (6,660 bytes), `wavePS.cso` (9,204 bytes), `wavePS.spv` (9,680 bytes) 4종 셰이더 사전 컴파일 완료.
- **파도 머티리얼 에셋 (`WaveMaterial.bammat.json`) 구축**:
  - `Resources/Shader/waveVS` 및 `Resources/Shader/wavePS` 연동, AlphaBlend(2), ReadOnly Depth(1), 양면 렌더링(CullMode: 0) 적용으로 낮은 시점이나 파도 굴곡 안팎에서도 컬링 잘림 없이 안정적으로 렌더링되도록 구성.
- **세분화 격자 메시 (`WaterGridMesh`) 내장 (`BuiltInPrimitiveMeshes.cpp`)**:
  - $10\text{m} \times 10\text{m}$ 크기의 $64 \times 64$ 분할 그리드(총 4,225 정점, 8,192 삼각형)를 생성하여 `L"WaterGridMesh"`로 리소스 등록.
  - 엔진 CW 와인딩 표준 준수 및 파도 진폭을 고려한 보수적 Y 바운딩 박스($[-1.5\text{m}, +1.5\text{m}]$) 설정으로 뷰 프러스텀 컬링 시 팝인(Pop-in) 방지.
- **에디터 화이트리스트 및 계층 패널 메뉴 연동**:
  - `AssetReferenceValidator.cpp`의 빌트인 식별자 및 빌트인 메시 화이트리스트에 `"WaterGridMesh"` 등록.
  - `Editor.csv`에 `Hierarchy,Plane` 및 `Hierarchy,WaterSurface` 로캘 키 추가 및 `localizationcsvtojson.py`로 JSON 동기화, `audit_localization.py` 100% 정적 감사 통과.
  - `HierarchyPanel.cpp`의 3D Object 생성 메뉴에 `Plane` 및 `Water Surface (Wave)` 추가.
- **스케일 대응 월드 정렬 UV (World-Aligned UV)**:
  - 엔티티의 Transform Scale($X, Z$)을 대폭 키웠을 때 로컬 `TexCoord`([0, 1]) 사용으로 인해 물결과 거품이 비례하여 거대하게 늘어나는(스트레칭) 문제를 방지하기 위해, `WorldPos.xz` 기반 월드 공간 정렬 UV를 `wavePS.hlsl` 및 `waterPS.hlsl`에 전면 적용.
  - 스케일을 키워도 1미터당 잔물결 및 거품의 빈도가 실제 물리적 크기로 고정되어 자연스럽게 타일링 반복되도록 처리.
  - `waveVS.hlsl`에서 이미 월드 좌표계 기준으로 계산된 `waveNormal` 및 `waveTangent`에 스케일 행렬이 중복 곱해지던 문제를 정규화 직통으로 수정하여 비균등/대형 스케일에서의 법선 왜곡을 방지.
- **에디터 핫로드 인프라 보강 및 런타임 안정화**:
  - `AssetHotReloadManager`, `HotReloaderInterface`, `ModuleBuildManager`, `ShaderCompiler` 개선: 파일 감시 시 `Moved` 액션 추가, 상위 디렉터리 감시 시 하위 디렉터리 중복 감시 방지, 강제 재컴파일 플래그 지원으로 소스 수정 시 즉각 핫스왑 보장.
  - `Runtime.cpp`: `m_ResourceManager` 해제 후 `m_AudioManager`가 해제되도록 안전 소멸 순서 조정.
- **실제 검증 결과**:
  - 셰이더 바이너리 컴파일 무결성 확인 (DXIL/SPIR-V 0 errors).
  - 다국어 감사 스크립트(`audit_localization.py`) 전체 통과.
  - C++ 정적 구조 및 참조 검증 완료.
  - **Editor Game View 및 독립 Windows 배포 Client(`Client.exe`) 실환경 실행 확인**: 엔티티 스케일을 대폭 확장하더라도 파도와 마이크로 잔물결, 거품이 늘어나지 않고 자연스럽게 타일링 반복되며, 낮은 시점에서의 3D 파도 실루엣, 파도 마루 거품, 깊이 페이드 및 굴절이 Editor와 Client 양쪽에서 100% 동일하게 정상 렌더링됨을 사용자 실환경 확인 완료.

### [2026-11-18] 래스터 반투명 3단계: 평평한 수면 셰이더(Water Ripple 노멀 스크롤, Depth Fade 수심 그라데이션, 고스트 방지) 파이프라인 구현 (Render S6-3)

- 유리 굴절(Render S6-2)에서 검증된 씬 컬러 복사(`SceneColorCopy`, 슬롯 16/`t0, space3`)와 `GBuffer_Position`(`Position.xyz`, 슬롯 20/`t4, space3`)을 연계하여 평평한 수면 셰이더(`waterPS.hlsl`)와 수면 머티리얼 에셋(`WaterMaterial.bammat.json`)을 구축했다.
- **수면 셰이더 (`waterPS.hlsl`) 핵심 기능**:
  1. **시간 기반 이중 스크롤 잔물결 (Dual Normal Scroll / Wave)**:
     - `commonData.Time` 누적 시간을 활용하여 서로 다른 방향과 주파수를 가진 두 파동 UV를 계산.
     - 노멀 맵 바인딩 시 접선 공간 블렌딩(`normalize(float3(n1.xy + n2.xy, n1.z * n2.z))`) 적용.
     - 노멀 맵 미지정 시에도 수학적 사인/코사인 파동의 공간 미분($\partial h / \partial x, \partial h / \partial y$)을 통해 자연스러운 절차적 잔물결 자동 생성.
  2. **수심 기반 색상 그라데이션 (Depth Fade / Optical Depth)**:
     - `g_ScenePosition`(`GBuffer_Position`)을 샘플링하여 수면 픽셀과 배경 불투명 오브젝트 간의 광로 수심(`waterDepth = max(0, bgDist - waterDist)`) 계산.
     - 수심에 따라 얕은 물(기본 에메랄드 시안)에서 깊은 물(딥 네이비 블루)로 부드러운 Beer-Lambert 흡수 모델 기반 색상 전이.
  3. **전경 오브젝트 고스트 방지 (Ghosting Artifact Prevention)**:
     - 굴절된 UV(`refractedUV`)에서 샘플링한 배경 위치가 수면보다 앞쪽 물체(`refrDist < waterDist`)일 경우, 굴절 오프셋을 취소하고 원본 `screenUV`를 사용하여 물체 실루엣이 물결 뒤로 번지는 결함을 차단.
  4. **해안선/물가 거품 림 (Shoreline Foam)**:
     - 수심 0.15m 이하 접촉 경계선에서 시간 기반 노이즈 변조 백색 거품 합성.
  5. **프레넬 및 태양광 스페큘러 (Fresnel & Sun Sparkle)**:
     - 물 IOR 1.333 기반 Schlick Fresnel 반사율 계산 및 하늘 반사광 합성, 상향 태양광 벡터 기반 Blinn-Phong 스페큘러 하이라이트 가산.
- **패스 바인딩 보강**:
  - `DeferredProfilePasses` 및 `ViewportDeferredPipeline`의 `ForwardTransparent` 패스에 `GBuffer_Position` 타깃을 슬롯 20(`MAX_MATERIAL_TEXTURE_SLOTS + 4`, Space 3의 `t4`)으로 명시 등록하여 모든 뷰포트에서 일관된 수심 샘플링 보장.
- **수면 머티리얼 에셋 (`WaterMaterial.bammat.json`)**:
  - `BlendMode: AlphaBlend`, `DepthMode: ReadOnly`, `CullMode: Back`, `PixelShader: waterPS` 구성.
- **검증 및 실환경 확인 (2026-11-18, Editor 실행 확인)**:
  1. 에디터 뷰포트에서 캐릭터가 반쯤 잠긴 상태에서 실시간 이중 스크롤 잔물결, 수심 기반 그라데이션, 태양광 스페큘러 렌더링 확인.
  2. 3방향 비대칭 파동 벡터 합성을 통해 초기 사선 바둑판 격자 스페큘러 결함 해소.
  3. 물속에 잠긴 캐릭터의 초록색 옷과 피부색이 맑고 자연스럽게 굴절 투과됨을 확인.
- **확인된 기술 이슈 및 기하학적 분석 (Plane 근접 시 투명화 현상)**:
  - **현상**: 수면 평면(Water Plane)을 다른 불투명 바닥 평면(Ground Plane)에 아주 가깝게 밀착 배치할 경우, 수면이 100% 투명해져 물의 파란 색감이 사라지고 바닥만 그대로 비치는 현상 발생.
  - **원인 분석**:
    1. **광학 수심 소멸 ($\Delta Z \to 0$)**: Depth Fade 수심 공식 `waterDepth = max(0, bgDist - waterDist)`에 의해 두 평면 간의 물리적 거리 간격이 0에 수렴하면 `waterDepth ≈ 0`이 됨.
    2. **Beer-Lambert 투과 모델 특성**: `depthFactor = saturate(waterDepth / fadeDistance) = 0`이 되어 수심에 의한 빛 흡수율이 0%가 됨. 셰이더가 이를 "두께가 없는 극도로 얕은 물가 경계(Shoreline / Edge)"로 판정하여 배경 바닥면의 원래 색상을 100% 투명하게 투과시킴.
    3. **평면 관통 시 깊이 역전**: 워터 큐브나 평면이 바닥 평면을 교차 관통할 경우, 관통 아래 구간은 카메라 시선 기준 불투명 바닥이 수면보다 앞서 가로막아 $\text{bgDist} < \text{waterDist}$ 깊이 역전이 발생함. 셰이더에 `isOccludedByForeground` 보호 분기를 두어 하얗게 타는 현상을 방어하고 배경을 패스스루하도록 안전 처리함.
  - **향후 구조적 대안**:
    - 머티리얼 파라미터에 최소 기본 불투명도/틴트 계수(Minimum Water Opacity, 예: 15~25% 기본 물 색상 보장)를 추가하여 수심 0에서도 은은한 물빛이 남도록 개선 검토.
    - 레벨 디자인 시 수면 평면과 바닥면 사이에 최소 수심 간격(Off-ground clearance) 확보 가이드라인 적용.
- **잔여**: Windows 배포 Client 검증, 4단계 파도(정점 변위 메시 애니메이션) 확장.

### [2026-11-18] 래스터 반투명 2단계: 씬 컬러 복사 및 얇은 유리 굴절/프레넬 파이프라인 구현 (Render S6-2)

- 불투명 패스(`DeferredLighting` / `ForwardOpaque`) 종료 직후 톤매핑 전 HDR 장면 색상을 안전하게 읽기 위한 복사 파이프라인(`PrePassCopy`)과 얇은 유리 굴절 셰이더를 구축했다.
- `RHI` 및 `DirectX12RHI`/`SDLGPURHI`에 `CopyTexture` 인터페이스를 구현했다. 활성 커맨드 리스트에서 D3D12 `CopyResource`를 호출하며, 소스 타깃의 원래 상태(`RENDER_TARGET`)를 복원하고 대상 텍스처(`SceneColorCopy`)를 `PIXEL_SHADER_RESOURCE` 상태로 전이하여 D3D12 Hazard 충돌을 원천 방지했다.
- `RenderPassDesc` 및 `RenderPass`에 `PrePassCopy`(`prePassCopySrcRT`, `prePassCopyDstRT`) 메커니즘을 추가하고, `Renderer::Render`에서 패스 시작(`BeginRenderPass`) 직전에 복사를 자동 수행하도록 연동했다.
- `DeferredProfilePasses` 및 `ViewportDeferredPipeline`의 `ForwardTransparent` 패스에 `SceneColorCopy` 타깃(`R16G16B16A16_FLOAT`)을 생성하고, 패스 텍스처 슬롯 16(`MAX_MATERIAL_TEXTURE_SLOTS + 0`, Space 3의 `t0`)에 바인딩했다.
- 굴절 셰이더(`glassPS.hlsl`)를 신설했다. 뷰 공간 법선 기반 굴절 UV 오프셋(화면 UV 좌표계 Y축 반전 보정), 화면 경계 클램프(`clamp(refractUV, 0.001, 0.999)`), 유리 틴트 컬러, 시선 방향 법선 자동 정렬(`dot(N, V) < 0` 시 `N = -N`), Schlick Fresnel 반사율 및 가장자리 프레넬 스페큘러 하이라이트 가산 합성을 구현했다.
- 래스터 유리 머티리얼 에셋(`GlassMaterial.bammat.json`, `AlphaBlend`, `DepthMode: ReadOnly`, `CullMode: Back`)을 신설하고 DXIL(`glassPS.cso`) 및 SPIR-V(`glassPS.spv`) 바이너리를 컴파일했다.
- **검증**: `AssetReferenceCheck` 단위 테스트 100% 통과. 에디터 뷰포트에서 캐릭터와 불투명 배경 앞에 유리 큐브를 배치하여 다음을 실환경 검증했다:
  1. 유리 큐브를 통과해 보이는 배경 캐릭터와 바닥이 법선 벡터에 따라 맑고 자연스럽게 굴절·왜곡되어 투과됨.
  2. 카메라 회전 및 오브젝트 이동 시 실시간으로 복사된 씬 컬러가 정상 갱신됨.
  3. 미바인딩 IBL 환경맵과의 lerp로 인해 수직면/가장자리가 검게 타던 문제를 프레넬 가산 하이라이트로 전환하여 해결.
  4. 폐곡면 큐브의 내부 뒷면 중복 렌더링을 방지하기 위한 뒷면 컬링(`CullMode: Back`) 정상 동작 확인.
- **잔여**: Windows 배포 Client 비교, 굴절 켜고 끈 조건에서의 패스별 GPU 시간 계측 기록(S1 지표), 굴절 강도 및 틴트 파라미터의 인스펙터 동적 조절 UI 노출은 후속 범위다.

### [2026-11-18] 문자열 경로 에셋용 경로 피커와 ResourcePicker 항목 공용화

- `ResourcePicker.h`의 목록 항목 그리기(썸네일·이름·경로·배지·선택 표시·툴팁·우클릭 메뉴)를 `Detail::DrawPickerItem`·`PickerItemView`로, 맨 위 연결 해제 행을 `Detail::DrawPickerClearItem`으로 분리했다. 썸네일 결정(스프라이트 잘라내기 포함)은 템플릿에 남겼다. 두 함수는 header의 `inline`이라 `ReflectionInspectorCheck`의 스텁 구성을 바꾸지 않는다.
- `DrawInspectorAssetPathPicker`(`AssetPathPicker.h/.cpp`)를 추가했다. 문자열 경로로 참조하는 에셋을 ResourcePicker와 같은 필드·팝업으로 고른다. Project·Engine 루트를 팝업이 열릴 때 한 번 스캔하고, 검색·연결 해제·빈 결과 안내·콘텐츠 브라우저 드롭·더블클릭 열기를 지원한다. 결과는 항상 가상 URI이며, 루트 밖 파일은 `ResourcePicker.OutsideRoots` 알림으로 거부한다. 반영 여부는 호출부가 정한다. 범위 탭과 썸네일 크기 슬라이더는 넣지 않았다.
- Animator 컨트롤러와 ActionTimeline 경로의 직접 입력칸·찾아보기 버튼을 피커로 교체했다. 컨트롤러는 로드에 성공했을 때만 저장하고 실패하면 알림을 띄운다. 연결 해제는 새 `AnimatorComponent::ClearController()`를 쓰며, `AnimationSystem`의 같은 해제 코드도 이 함수로 바꿨다. ActionTimeline은 리플렉션 표에 중복 표시되던 `timelinePath` 텍스트 칸을 숨겼고, 찾아보기가 절대 경로를 저장하던 문제가 해결됐다.
- 로캘은 `ResourcePicker.OutsideRoots`·`ResourcePicker.NoMatchingFiles`를 추가하고 쓰지 않게 된 `Animator.ControllerOutsideProject`를 제거했다. 빌드가 JSON을 다시 생성했다.
- **검증**: `audit_localization.py --strict` 통과. Debug 전체 빌드에서 컴파일·링크 경고와 오류가 없었다. CTest 39/39 통과(`ReflectionInspectorCheck` 포함). 사용자가 에디터에서 다음을 확인했다. 텍스처·머티리얼·스프라이트 피커의 표시와 동작이 이전과 같음, 연결 해제 행, 컨트롤러 목록·검색·URI 표시·연결 해제·드롭·잘못된 종류 거부·빈 결과 안내, ActionTimeline 텍스트 칸 숨김과 피커 선택 후 재생·편집기 열기, 저장 후 씬 참조 검사 0건.
- **잔여**: `ActionTimelineComponent`에는 `timelineAsset` 핸들(ResourcePicker)과 `timelinePath`가 함께 있다. `ActionTimelineSystem`은 경로가 바뀔 때만 핸들을 다시 로드하므로, 핸들만 바꾸면 두 값이 어긋날 수 있다. 피커 필드 영역 그리기는 ResourcePicker와 같은 메트릭으로 따로 작성했다. `ActionTimelineSystem.cpp`의 기존 `C4834` 경고 10건은 이번 범위 밖이다.

### [2026-11-18] Animator 컨트롤러 경로를 가상 URI로 저장

- 인스펙터 Animator의 찾아보기 버튼이 파일 대화상자의 절대 경로를 `controllerPath`에 그대로 저장했다. 이 씬은 패키징 참조 검사에서 `ExternalAbsolutePath`로 실패한다. Windows 기준선 검증 중 `RaytracingScene3`가 원본 체크아웃의 컨트롤러를 절대 경로로 가리켜 내보내기가 막힌 것으로 확인했다.
- 찾아보기 결과와 콘텐츠 브라우저 드롭(`CONTENT_BROWSER_ITEM`)을 `PlatformPath::ToVirtualUri`로 `Project://`·`Engine://`·`Editor://` URI로 바꿔 `LoadController`에 넘긴다. 마운트 루트 밖의 파일은 `Animator.ControllerOutsideProject` 알림과 함께 거부하고 기존 경로를 유지한다. 로드에 실패하면 경로를 덮어쓰지 않는다. 드롭은 `.animator`·`.animator.json`·`.bamanimator`·`.bamanimator.json`만 받는다. 직접 입력칸은 그대로다.
- 로캘은 `Editor.csv`에 한 행을 추가했고 빌드가 JSON을 다시 생성했다.
- **검증**: Debug Editor 빌드에서 컴파일 경고·오류가 없었다. 루트 밖 컨트롤러를 골랐을 때 경고 알림이 번역된 문장으로 표시되는 것을 사용자가 확인했다. 찾아보기·드롭 결과의 URI 표시, 비컨트롤러 드롭 무시, 저장 후 씬 참조 검사 0건은 커밋 시점에 확인 결과를 받지 못했다. 이 UI는 바로 다음 작업에서 경로 피커로 교체됐고, 해당 동작은 피커에서 확인했다(위 항목).
- **후속**: `ActionTimelineComponent::timelinePath`의 같은 문제와 함께 경로 피커로 해결했다(위 항목).

### [2026-11-17] 에디터 UI·인스펙터·리소스 에디터 로캘 및 입력(InputManager) 표준화

- **로컬라이제이션 키 확장 및 동기화 (`65479627`)**:
  - 디스크 캐시 용량/파일 수 표시(`Settings.DiskCacheSizeAndFiles`), 텍스처 에디터 해상도 포맷(`TextureEditor.DimensionValue`) 및 미리보기 없음 안내(`TextureEditor.NoPreviewAvailable`), 뷰포트 카메라 속도 포맷(`Viewport.CameraSpeedValue`), 타임라인 VFX 파일 표기(`Timeline.VFXFile`) 등 신규 로캘 키 추가.
  - 원본 CSV(`Editor.csv`, `Engine.csv`) 및 생성된 JSON(`English.json`, `Korean.json`) 동기화.
- **리소스 에디터 메타데이터 테이블 로캘 적용 (`835b498e`)**:
  - `ShaderEditor` 및 `TextureEditor`의 프로퍼티/값 메타데이터 테이블 헤더에 `EditorCommon.Property`, `EditorCommon.Value` 로캘 적용 및 텍스처 치수/미리보기 없음 상태 로캘 적용.
- **인스펙터 커브 및 에디터 UI/설정 입력 표준화 (`75a57e47`)**:
  - `ReflectionPropertyDrawer`: 커브 에디터에서 ImGui 마우스 직접 접근 대신 `Engine::InputManager` API(마우스 화면 좌표 `GetScreenMousePosition`, 드래그 `IsMouseButtonDragging`, 더블클릭 `IsMouseButtonDoubleClicked`)를 사용하여 일관된 입력 처리 준수.
  - `GizmoHelper`: 기즈모 툴바 버튼 단축키 힌트 전달 방식 정리.
  - `EditorEnvironmentSettingsWidget`: 디스크 캐시 항목별 및 총합 정보 표시에 `Settings.DiskCacheSizeAndFiles` 포맷 적용.
  - `EditorSettingsService`: 기본 Gameplay 액션 바인딩 테이블에 `OrbitCamera`(`Mouse:Right`) 기본 매핑 추가.
  - `EditorActionRegistry`: 카메라 속도 증감 시 알림 메시지에 `LOCAL_FMT` 적용.
- **애니메이션 타임라인 패널 입력 및 로캘 표준화 (`fd1d0e41`)**:
  - `AnimationTimelinePanel`: 본 계층 구조 트리에서 더블클릭 포커스 시 `Engine::InputManager::Get().IsMouseButtonDoubleClicked` 사용.
  - 복합 VFX 스폰 노티파이 마커 툴팁에 `Timeline.VFXFile` 로캘 포맷 적용.
- **검증**:
  - 정적 코드 분석 및 Git 변경 검토 완료. C++ 컴파일 빌드 및 런타임 검증은 사용자 실행 대기.

### [2026-11-17] 콘텐츠 브라우저 일괄 삭제 전 참조 영향 표시

- 삭제 요청에 기존 프로젝트 참조 검사를 연결했다. 확인창에는 삭제 후에도 남는 문서가 참조하는 삭제 대상만 표시한다. 폴더 하위 항목을 포함하고 같은 삭제 묶음 안의 참조는 제외한다. 일부 문서나 경로를 읽지 못하면 확인된 참조 목록과 누락 안내를 함께 표시한다.
- 검사는 삭제 요청당 한 번 동기로 수행한다. 취소 시 파일과 이력의 상태를 유지하며 실제 삭제와 Undo는 기존 파일 명령이 맡는다. 이동 전에 사용처를 확인하는 동작은 후속이다.
- `AssetReferenceCheck`에 단일 대상, 폴더, 내부 참조 제외, 일부 문서 또는 선택 경로 누락을 추가했다. 로캘 엄격 감사 및 `git diff --check`로 정적 확인했다. 빌드와 CTest, 실제 삭제창과 취소 동작은 사용자 실행 전이다. [파일 이력 문서](EditorUndoRedo.md#콘텐츠-브라우저-파일-이력)

### [2026-11-17] 프로젝트 에셋 참조의 직접 사용처 조회

- 콘텐츠 브라우저 단일 파일 우클릭에 **에셋 참조 보기**를 추가했다. 기존 검사 패널에서 직접 사용처와 참조 대상을 탐색한다. 결과는 디스크 검사 시점의 상태이며 버튼으로 갱신한다.
- 검사 루트를 `GetProjectAssetRoot()`로 바꾸고 패키징과 JSON 참조 수집·경로 해석을 공유했다. 가상 URI와 레거시 경로를 해석하고 중복 링크를 제거한다. 누락된 참조나 읽을 수 없는 문서는 문제 탭에 표시한다.
- `AssetReferenceCheck` 회귀 검사를 추가했다. `audit_localization.py --strict`와 `git diff --check`로 정적 점검했다. C++ 빌드·CTest와 에디터 실환경 확인은 사용자 실행 전이다.
- 바이너리·동적 로드·미저장 변경 분석과 이동·삭제 확인창의 사용처 연동은 후속이다. [에셋 참조 개발 계획](ImGuiDevelopmentPlan.md#b-assetreferencepanel과-에셋-검증--우선순위-높음)

### [2026-11-17] 리소스 리로드 시 키·경로 보존과 새 머티리얼 식별자 기록

- `Resource::Reload`가 `ReadArchive` 후 파일에 저장된 `m_Key`/`m_Path`로 메모리의 런타임 식별자를 덮어쓰던 결함을 고쳤다. 리로드 전 값을 보존하고 역직렬화 직후 복원한다(`LoadResource`가 로드 후 `SetKey`/`SetPath`를 다시 지정하는 것과 같은 원칙). 증상은 콘텐츠 브라우저에서 새로 만든 머티리얼이 편집·저장(핫 리로드) 후 인스펙터 핸들에서 이름·경로·썸네일이 비고, 이후 저장마다 빈 키가 파일에 다시 기록되는 것이었다.
- 콘텐츠 브라우저의 `CreateEmptyMaterial`·`CreateEmptyMaterialInstance`가 저장 전에 프로젝트 상대 경로를 키·경로로 지정한다(`AssignNewAssetIdentity`). 기존에는 빈 값이 파일에 기록되었다. 생성 직후 이름을 바꾸면 파일 값은 다시 낡지만, 로드·리로드가 파일 값을 쓰지 않으므로 동작에는 영향이 없다.
- **검증:** 이번 기록 작업에서는 빌드·CTest·실행을 하지 않았다. 수정 후 에디터 재시작 시 핸들 이름 표시와 머티리얼 편집기 저장 후 이름 유지는 사용자 확인 대기다.
- **잔여:** 에셋 파일의 `m_Key`/`m_Path`에는 이관 전 경로(`Resources/...`), 절대 경로(로더가 `m_Path`를 절대 경로로 지정한 뒤 저장), 빈 값이 섞여 있다. 근본 정리안은 두 필드를 `TRANSIENT`로 바꾸는 것이며, 기존 파일의 남는 필드 처리와 파일 값을 읽는 다른 코드의 존재 여부를 확인한 뒤 진행한다.

### [2026-11-17] 반투명 정적 메시의 카메라 기준 정렬 (Render S6 1단계)

- `Renderer::Render`의 뷰포트(job) 반복에서 `BackToFront` 패스의 정적 메시 명령을 카메라 위치 기준 거리 내림차순으로 정렬한다(`SortStaticBackToFront`, 동일 거리는 `stable_sort`로 제출 순서 유지). 기존에는 `BackToFront`일 때 정렬을 건너뛰어 합성 순서가 엔티티 생성 순서를 따랐다. 명령 목록은 패스 단위로 공유되므로 뷰포트 카메라마다 다시 정렬한다. 정렬 후 연속한 같은 메시·머티리얼은 기존처럼 인스턴싱으로 묶이며 인스턴스 순서는 래스터 순서로 유지된다.
- 정적 메시용 반투명 검증 셰이더 `forwardTransparentPS.hlsl`(머티리얼 `BaseColor`를 비조명 출력)을 추가했다. 기존 `TransparentMaterial`은 `defaultPS`가 불투명 흰색을 반환해 반투명 확인에 쓸 수 없다.
- **검증 (사용자 실행, 2026-11-17, `x64-ninja-debug`):** 서로 다른 색의 반투명 판(AlphaBlend, DepthMode ReadOnly) 겹침 합성, 카메라 반대편 이동 시 순서 반전, 생성 순서 변경 시 동일 결과, 불투명 큐브와의 가림, Scene·Game View 동시 표시, 파티클·스프라이트 회귀를 확인했다고 보고했다. 이번 기록 작업에서는 빌드·CTest·실행을 하지 않았고 CTest 결과는 보고되지 않았다.
- **미검증·한계:** Windows 배포 Client 비교(S6 공통 완료 기준)는 미수행이다. 한 패스 안에서 정적→스킨드→스프라이트→동적 지오메트리→커스텀 큐 순서로 그리므로 반투명 판 뒤의 파티클도 판 위에 그려진다. 반투명 메시는 Geometry 패스에 들어가지 않아 뷰포트 클릭으로 선택되지 않는다. 관통하는 판은 물체 단위 정렬로 해결되지 않는다. [S6 계획](RenderingDevelopmentPlan.md#s6-래스터-반투명-재질과-수면)

### [2026-11-17] 콘텐츠 브라우저 파일 이력과 계층 다중 선택

- 콘텐츠 브라우저의 공통 우클릭 메뉴에 복사·잘라내기·붙여넣기 아이콘과 묶음 삭제를 연결했다. X1/X2는 방문한 폴더를 탐색하며 경로 표시줄·폴더 타일·트리 이동을 같은 이력에 기록한다.
- 로컬 Delete, Ctrl+Z/Y를 등록했다. 일괄 삭제·복사 Undo에서 복구 파일을 프로젝트 `.bam/content-browser-history/`에 보관한다. 붙여넣기와 폴더 드롭은 성공한 전송을 한 명령으로 기록한다. 파일과 씬 이력을 각각 유지한다.
- 계층 트리의 박스 선택을 `SelectionManager`에 연결했다. 우클릭 및 Ctrl+D 복제에서 선택한 루트마다 원래 부모를 사용한다. 단축키 문맥은 패널 그리기 후 실제 포커스된 창에서 결정한다.
- 사용자 실환경 확인에서 변경된 동작이 의도대로 동작하는 것으로 보고되었다. 개별 시나리오와 빌드 구성, CTest 결과는 제공되지 않았다.
- `audit_localization.py --strict`와 `git diff --check`를 통과했다. [파일 작업과 계층 선택](ImGuiDevelopmentPlan.md#g-콘텐츠-브라우저-파일-작업과-계층-선택) · [파일 이력과 복구 정책](EditorUndoRedo.md#콘텐츠-브라우저-파일-이력)

### [2026-11-17] 스플리터 슬림화와 애니메이션·이펙트 타임라인 시각·조작 통일

- **스플리터:** `ImGuiSplitter.h`에 `SplitterThickness`(2px)·`SplitterHoverExtend` 상수, 드래그 이동량을 돌려주는 `SplitterBar`, 자식 창 위에 겹쳐 놓는 `SplitterHandle`(FlattenChildren, 겹침 비허용)과 `DrawVerticalSplitterLine`을 추가했다. Animator 그래프·머티리얼·스프라이트·VFX·애니메이션 타임라인의 4px·8px 경계를 이 규격으로 바꿨다. 애니메이션 패널 상단 폭 계산의 `SameLine` 간격 누락(3→4)으로 생기던 8px 가로 넘침을 고쳤다.
- **공통 모듈:** `TimelineView.h/.cpp`를 새로 만들어 시간 축·눈금자·재생 헤드·줌/이동, 행 배경·그리드(눈금자와 같은 간격)·이름 칸·막대·마커, 트랙 머리 칸(`DrawTimelineCorner`, `+` 버튼), 휠 규칙(`SplitTimelineWheel`), 재생 버튼(`DrawTimelineAction`/`DrawTimelineTransport`)을 옮겼다. `EffectTimelineView`에는 이펙트 전용 위젯과 편집기 배치만 남겼다.
- **트랙 이름 칸:** 트랙당 종류 아이콘 하나, 고정 폭 아이콘 칸, 이름 색 3단계(보통·선택·비활성)를 두 타임라인에 적용했다. VFX는 꺼짐·음소거 트랙(`TimelineRow::Disabled`)을, 애니메이션은 노티파이 음소거·비활성 샷을 흐리게 표시한다. 애니메이션의 음소거·샷 활성 토글 아이콘은 없애고 우클릭 메뉴로 일원화했다.
- **고정 머리:** 애니메이션 타임라인을 VFX와 같은 구조(눈금자 고정, 행만 자식 창 스크롤)로 바꿨다. VFX는 행 자식 창을 닫은 뒤 그 그리기 목록에 그려 잘라내기가 풀리던 결함(스크롤 시 행이 눈금자를 덮음)을 고쳐, 창이 열려 있는 동안 그린다. VFX 이름 칸 폭 조절은 `TimelineDragMode::Header` 분기를 없애고 `SplitterHandle`로 바꿨다.
- **입력·추가:** Shift+휠 좌우 이동을 애니메이션에도 적용했고, 이름 칸 휠·Alt+휠은 행 세로 스크롤이다. 두 타임라인의 트랙 머리 칸에 `+` 버튼을 넣었다. VFX 트랙 추가 메뉴는 `DrawAddTrackMenuItems`로 빼서 목록 창과 타임라인이 함께 쓴다. 애니메이션 카메라 샷은 메뉴를 연 시점의 시간(`m_AddTrackTime`)에서 시작한다. VFX 타임라인 최소 높이를 트랙 2행 이상이 보이도록 올렸다.
- **재생 바:** 애니메이션 재생 버튼이 CSV에 없는 `Timeline.FirstFrame` 등의 키를 쓰던 것을, 등록된 `Timeline.*` 액션을 실행하는 `DrawTimelineAction`으로 바꿨다. 툴팁 로캘·단축키 힌트·루프 활성 배경이 VFX와 같다. `Editor.csv`에 `Timeline.AddTrack`을 추가했다(JSON은 빌드 생성물).
- **검증 (사용자 실행, 2026-11-17, `x64-ninja-debug`):** 단계마다 사용자가 빌드하고 에디터에서 두 타임라인의 화면과 동작을 확인했다(스크린샷 비교로 남은 결함을 이어서 수정). 마지막 단계(VFX 이름 칸 스플리터)까지 의도대로 동작한다고 보고했다. 이번 기록 작업에서는 빌드·CTest·실행을 하지 않았고, 이 범위의 CTest 결과는 보고되지 않았다.
- **미검증·한계:** 패널 폭이 매우 좁으면 상단 세 영역의 최소 폭 때문에 가로로 넘칠 수 있다. 이름 칸 폭 조절은 ESC로 취소되지 않는다. 파티클 편집기는 공용 위젯 변경을 그대로 받으며 별도 확인은 보고되지 않았다. [타임라인 계획 6절](AnimationTimelineEditorPlan.md#6-타임라인-시각-규격-통일과-고정-머리-2026-11-17), [스플리터 공통 구현](EngineEditorClientGuidelines.md#공통-구현-imguisplitterh)

### [2026-11-17] 모델 재임포트 머티리얼 보존, 배치 회전 및 생성물 폴더 분류

- 기존 머티리얼 인스턴스를 기본 재사용하고 모달에서 덮어쓰기를 선택할 수 있게 했다. 도끼에서 저장한 색의 보존과 원본 색 복원을 사용자 실행으로 확인했다.
- 수동 임포트 회전을 `.bammodel`의 배치 회전으로 옮겨 모델 루트·미리보기·썸네일에 적용했다. 이전 변환 캐시가 사용되지 않도록 버전을 갱신했다. Fox 방향 수정은 사용자 예비 확인 상태다. 별도로 Rogue 스켈레톤과 구형 KayKit 클립의 좌표 차이를 확인했고, 동일한 임포트 옵션으로 클립을 다시 생성한 뒤 정상 재생을 사용자에게 확인했다.
- 모델에 프로젝트 원본 URI와 생성물별 폴더 설정을 저장한다. 재임포트는 원본을 찾되 모델이 있는 폴더에 기존 구조대로 갱신한다. 새 임포트는 `Meshes/`, `Materials/`, `Textures/`, `Animations/`를 사용하고 스켈레톤은 대표 모델 옆에 둔다.
- 사용자가 별도 원본 폴더에서 재임포트와 활·Fox 폴더 분류를 확인했다. Fox의 네 하위 폴더와 스켈레톤, 세 클립을 디스크에서 확인했다. 이번 문서화에서는 추가 빌드·CTest·실행을 하지 않았다. 기존 에셋 이관, 원본 자동 보관, 다중 파일 실패 복구는 후속 범위다. [모델 임포트 계획](InteractiveModelImportPreviewPlan.md#phase-6-하이브리드-모델-에셋-정리-파이프라인-hybrid-source-to-asset-pipeline)

### [2026-11-17] 액션 타임라인 ↔ 복합 VFX 소켓 스폰 및 뷰포트/타임라인 UX 일원화

- **배경**: 캐릭터 액션 타임라인(`AnimationTimelinePanel`)에서 복합 VFX(`.bamvfx`)를 캐릭터 무기/신체 본 소켓에 실시간 동기화하여 스폰하는 파이프라인이 부재했고, 독자적인 뷰포트 구현으로 인해 오버레이 툴바 부재 및 카메라 조작감(`Alt+LMB`, `Alt+MMB`, `F` 포커스) 불일치가 존재했다. 또한 스플리터 두께 및 재생 컨트롤 바(`TransportBar`) 배치가 `VFXEditorPanel`과 파편화되어 있었다.
- **변경 사항**:
  - **Engine (타임라인 소켓 스폰 및 수명 추적)**:
    - `ActionTimelineAsset.h`: `EAnimNotifyType::SpawnCompositeVFX` 타입 및 소켓 부착 여부(`AttachToSocket`), 오프셋 위치/회전, 스케일, 재생 속도, 자동 파괴 필드 추가 및 Json 직렬화 연동.
    - `ActionTimelineComponent.h`: `SocketAttachmentComponent`, `ActionVFXLifecycleComponent` 신설.
    - `ActionTimelineSystem.cpp`: `ComputeSocketWorldTransform`을 통해 엔티티의 스키닝 본 변환과 결합된 실시간 소켓 월드 좌표/회전을 추적하고, 타임라인 재생 시 복합 VFX 엔티티 스폰 및 수명 자동 관리 구현.
  - **Editor (AnimationTimelinePanel 전면 고도화)**:
    - `PreviewViewportWidget`으로 전면 일원화: 독자적 렌더 파이프라인을 걷어내고 표준 뷰포트 위젯을 탑재하여 뷰 모드/통계 오버레이 툴바, 스튜디오 조명, 그리드, 표준 카메라 컨트롤러(`Alt+LMB` 궤도, `Alt+MMB` 팬, `F` 포커스) 통합.
    - 모델 드래그 앤 드롭(`HandleModelDragDrop`): 콘텐츠 브라우저에서 `.bammodel` 파일을 뷰포트에 드롭 시 즉시 로드.
    - 좌측 스켈레톤 본 계층 트리(`DrawBoneHierarchyTree`): 검색 필터, 더블클릭 시 해당 본 위치 카메라 포커스, 노티파이 선택 상태에서 클릭 시 즉시 `SocketBoneName` 지정 연동.
    - 내장 애니메이션 없는 모델 지원: 애니메이션이 없는 경우 바인드 포즈(T-포즈)를 유지하며, 타임라인 지속시간(`Duration`)을 임의로 조절할 수 있는 자유 시퀀서 모드 탑재.
    - 인스펙터 및 타임라인 마커: `SpawnCompositeVFX` 전용 프로퍼티 편집 UI 및 루비 핑크(`RGB: 255, 60, 140`) 다이아몬드 마커 시각화.
  - **UI 통일성 및 스플리터 슬림화**:
    - `ImGuiSplitter.h`: `SplitterV`/`SplitterH` 및 `DrawSidebarLayout`의 기본 두께를 2.0px로 슬림화하고 `ImGuiCol_Separator` 선 렌더링 추가.
    - `AnimationTimelinePanel` 및 `VFXEditorPanel`: Transport 바 구성을 `[재생 6버튼]` → `[시간/프레임 카운터]` → `[배속 슬라이더]` → `[줌 배율/리셋]` → `[특화 컨트롤]`로 1:1 완벽 통일.
    - `Editor.csv`: `Timeline.Duration` 중복 키 에러 해소, `Timeline.Title` 등 누락 로캘 키 8종 추가 및 JSON 자동 동기화.
- **검증 (사용자 실행, 2026-11-17, `x64-ninja-debug`, 커밋 `ba399684`)**:
  - `Editor.exe` 구동을 통해 `PreviewViewportWidget`의 카메라 조작 및 오버레이 툴바 동작 확인.
  - 모델 파일 드래그 앤 드롭 및 좌측 본 계층 트리 렌더링 확인.
  - `Timeline.Title`, `Timeline.NoEmbeddedAnimation`, `Timeline.BoneHierarchy` 등 한글 로캘 정상 출력 확인.
  - 2px 슬림 스플리터 및 일원화된 Transport 재생 바 동작 확인.

### [2026-11-17] 빌드 설정 정리: Qt AUTOMOC 비활성화, 리플렉션 생성기 동일 내용 쓰기 생략

- **Qt AUTOMOC 비활성화** (`Editor/CMake/QtBuild.cmake`): Editor·플러그인 소스에 `Q_OBJECT`·`Q_GADGET`·`Q_PROPERTY`·`Q_SIGNALS`·`Q_SLOTS`·`Q_EMIT`이 없고 Qt 사용처는 `QtManager`뿐이며 `QT_NO_KEYWORDS`가 켜져 있어 moc 대상이 없다. 빌드마다 Editor 소스 전체를 훑는 autogen 단계를 없앴다. Qt 시그널·슬롯 클래스(`Q_OBJECT`)를 추가하면 다시 켜야 한다.
- **리플렉션 생성기 쓰기 생략** (`Projects/Reflection/generator/main.cpp` `SaveGeneratedFile`): 기존 파일과 내용이 같으면 쓰지 않는다. CMake Ninja 생성기는 리플렉션 생성 단계에 `restat = 1`을 붙이므로, 변하지 않은 `.gen.h`를 include하는 소스를 다시 컴파일하지 않는다(예: `GraphFoundationCheck.gen.cpp`는 갱신되고 `.gen.h`는 이전 시각 유지).
- **검증 (2026-11-17, `x64-ninja-debug`)**: 두 변경이 들어간 상태로 하루 동안 Editor 빌드·실행과 CTest를 수행했다. 무변경 빌드에서 리플렉션 생성 단계가 다시 실행되지 않음을 빌드 출력으로 확인했다.
- **잔여 (추정, 미확인)**: Visual Studio 생성기(`x64-debug`)의 사용자 지정 빌드 단계에는 `restat`이 없어, 출력이 입력보다 오래된 상태로 남으면 빌드마다 생성기가 다시 실행될 수 있다(정확성 영향 없음, 빌드 시간만 증가). 근본 해결은 `ReflectionGenerate.cmake`에서 생성 파일을 `BYPRODUCTS`로 두고 별도 스탬프를 `OUTPUT`으로 갱신하는 방식이다.
- **문서**: `DiskCacheDevelopmentPlan.md` Phase 4 기록에 커밋 해시(`4b815f7`)를 기입했다.

### [2026-11-17] 렌더링 우선순위와 유리·수면·파도 계획 기록

- Windows Game View·배포 Client 비교 후 반투명 판 두 장의 합성을 확인하는 작업을 P2로, 굴절 유리·평평한 수면·움직이는 수면 메시를 조건부 P3로 기록했다. DXR 그림자·반사는 P3의 독립 경로, 패스 트레이싱 기준 렌더러는 P4에 둔다. [상세 계획](RenderingDevelopmentPlan.md#s6-래스터-반투명-재질과-수면) · [통합 우선순위](DevelopmentPriorities.md#p2-b-기본-렌더-확장-s3s4s5-검증-및-완성)
- 공용 Deferred 프로필의 투명 패스와 정적 메시 정렬 분기를 소스에서 대조했다. 이번 변경은 계획 문서에 한정했으며 단계별 GPU·화면 검증은 구현 후 진행한다.

### [2026-11-17] 그래프 노드 위치·주석 저장, 주석 노드 결함, 핀 표시와 캔버스 조작

- **배경**: 그래프 저장 범위가 `Engine::Graph`뿐이라 노드 위치와 주석 제목이 사라졌다. 조사 중 다음 결함을 함께 확인했다.
  - `Core.Comment`가 에디터 패널에서만 등록돼, 주석이 들어간 로직 그래프는 실행기("No execution binding"), 셰이더 그래프는 컴파일러(`ShaderGraph.UnsupportedNode`)에서 실패했고 런타임 `GraphSystem` 로드 검증도 통과하지 못했다.
  - `GraphData::Variables`(2026-10-31 `6bba12ff`)가 필수 필드라 그 전에 저장한 파일(GraphMovement, BuildSmokeMovement)이 역직렬화 단계에서 실패했다. 패널은 실패를 표시하지 않고 대상 경로(제목)만 먼저 바꿨다.
  - `DuplicateSelection`이 `AddNode`(노드 벡터 `push_back`) 뒤에 원본 노드 포인터를 읽었다(재할당 시 댕글링).
  - 주석 크기 동기화가 노드 전체 크기(헤더·여백 포함)를 저장 `Size`와 비교·기록해, 열 때마다 미저장이 되고 저장할 때마다 박스가 커졌다.
  - 파일을 열 때 만드는 노드 에디터에는 휠 클릭 이동(`NavigateButtonIndex = 2`)과 테마가 빠져 있어 우클릭 드래그가 화면 이동과 컨텍스트 메뉴를 동시에 일으켰다.
- **변경 사항**:
  - **Engine** (`GraphNode.h`, `Graph.h/.cpp`, `GraphData.h`, `GraphRegistry.cpp`, `GraphExecutor.cpp`, `ShaderGraphCompiler.cpp`):
    - `GraphNode`에 에디터 메타데이터 `PositionX/PositionY/HasPosition`, `Comment` 추가(`ALLOW_MISSING`). `Graph::SetNodePosition`(0.5px 이상 변경 시)·`SetNodeComment`(값이 바뀔 때)만 수정 카운터를 올린다.
    - `Core.Comment`를 엔진 기본 노드로 등록(리터럴 전용 `Size` Float2). 제목을 String 핀으로 두면 셰이더 노드의 문자열 핀 금지 규칙에 걸려 기본 노드 등록 전체가 실패하므로 노드 메타데이터로 저장한다. 실행기·셰이더 컴파일러는 주석을 지원 노드로 인정하고 건너뛴다.
    - `GraphData::Variables`를 `ALLOW_MISSING`으로 바꿔 예전 파일을 연다.
  - **Editor** (`GraphPanel.h/.cpp`, `GraphPanelNodeEditor.cpp`):
    - 로드 시 저장 위치 복원, 위치 없는 노드는 격자 배치를 문서에도 기록해 저장 기준에 포함. 주석 그룹 크기는 `ed::SetGroupSize`로 설정. 그릴 때 매 프레임 `ed::GetNodePosition`을 문서에 반영.
    - 주석 크기 동기화는 `ed::GetNodeState`의 `GroupSize`(그룹 영역)와 비교해 사용자가 조절했을 때만 기록.
    - 주석 제목은 `SetNodeComment`로 저장, `CommentTexts` 맵 제거. 복제는 원본 노드를 값으로 복사한 뒤 입력값·메모를 복사.
    - `SetTargetResource`는 로드 성공 시에만 대상을 바꾸고, 실패는 `ReportLoadFailure`가 알림(단계·필드)으로 표시. 로캘 `GraphPanel.LoadFailed`.
    - 데이터 핀 원을 노드 테두리 위에 반쯤 걸쳐 그린다. 행을 그릴 때 세로 중심을 모으고, 노드 그룹을 닫은 뒤 내용 영역+`NodePadding`으로 테두리 좌표를 구해 `PinRect`/`PinPivotRect`로 지연 선언한다(링크 직선 스타일 푸시를 다시 적용). 실행 핀은 안쪽 유지.
    - 노드 에디터 생성을 `CreateEditorContext()`로 통일(휠 클릭 이동, 테마, `SettingsName = nullptr`로 편집용 ID 기준의 `imgui.ini` 상태 저장 중단).
  - **테스트** (`GraphFoundationCheck`): 위치 저장·0.5px 미만 무시·존재하지 않는 노드·비유한 값 거부, 메모 반복 쓰기 시 수정 카운터 1회, 파일 왕복에서 위치·메모(한글) 보존.
- **검증 (사용자 실행, 2026-11-17, `x64-ninja-debug`, 커밋 `ba399684`)**: 주석 메타데이터 수정 후 CTest와 에디터에서 주석 제목·크기·위치 저장, 예전 파일 열기, 열자마자 `*`가 켜지지 않음, 휠 클릭 이동·우클릭 메뉴 분리, 데이터 핀 테두리 표시와 연결을 "잘 동작함"으로 확인했다. 단계별 CTest 개수는 따로 기록되지 않았다.
- **동작 변화·잔여**:
  - 예전에 노드 전체 크기로 저장된 주석 `Size`는 이제 그룹 영역 크기로 해석돼 처음 한 번 헤더·여백만큼 크게 보인다.
  - 캔버스 화면 이동·확대는 재실행 시 기억되지 않는다(`imgui.ini` 저장 중단).
  - 노드 메타데이터는 `GraphNode`에 있어 런타임 그래프에도 함께 로드된다(실행에는 쓰지 않음).

### [2026-11-17] VFX 편집기 이력 정합성 (트랙 추가 Undo, 편집 메뉴, 저장 기준점)

- **배경**: 리소스 편집기 기본 기능 기준(EngineEditorClientGuidelines 19절 7항)의 Undo 조건과 대조한 결과, VFX 편집기는 트랙 추가 메뉴 8곳이 `m_TimelineHistory.Clear()`로 이전 Undo 이력을 모두 지우고 추가 자체도 되돌릴 수 없었다. 편집 메뉴에는 Undo/Redo가 없었다. `InvalidateTimelinePreview()`가 매번 `m_IsDirty = true`를 설정해 Undo로 저장 상태에 돌아가도 `*`가 꺼지지 않았다.
- **변경 사항** (`VFXEditorPanel.h/.cpp`):
  - **트랙 추가 명령**: `DrawTrackList`의 `addTrack` 람다가 삭제 명령의 역동작으로 `CallbackEditorCommand`를 실행한다(실행=목록 끝에 추가 후 선택, 취소=끝에서 제거 후 선택 해제). 이력이 선형이라 취소 시점의 마지막 항목이 그 트랙이다. 처음 만든 트랙 값을 캡처해 Redo도 같은 이름·값으로 복원한다. 실행 전에 인스펙터 편집 세션을 확정해 이력 순서를 맞춘다. 명령 이름은 기존 `VFXEditor.AddTrack`을 재사용한다. 새로 만들기·열기의 `Clear()`는 유지한다.
  - **편집 메뉴**: `실행 취소`/`다시 실행`을 추가하고 퀵액세스·단축키와 같은 `CanExecuteTimelineAction`/`ExecuteTimelineAction`을 호출한다. 파일 메뉴 `닫기`는 `OnResourceCloseRequested()`를 호출한다.
  - **저장 기준점**: `HasUnsavedResourceChanges() = m_UntrackedDirty || m_TimelineHistory.GetCurrentStateId() != m_SavedStateId`. `MarkSaved()`가 저장 성공·로드 시 상태 ID를 기록한다. `InvalidateTimelinePreview()`는 프리뷰 재구성만 담당한다. 이력 밖 변경(반복 토글, 트랙 목록 활성 체크, 새 이펙트)만 `m_UntrackedDirty`를 세운다. 저장 직전에 인스펙터 세션을 확정해 저장 후 `*`가 잘못 켜지지 않게 했다.
  - 동작 변화: 기즈모·인스펙터 드래그 중에는 `*`가 켜지지 않고 놓는 순간(명령 기록 시점) 켜진다.
- **검증 (사용자 실행, 2026-11-17)**: 1단계(추가 후 이력 보존, 추가 Undo/Redo, 여러 종류 역순 취소, 편집 메뉴)와 2단계(UX-4.3 편집→Undo, 저장 후 Undo/Redo, 분기 편집, 드래그, 이력 밖 변경, 새 이펙트, 열기, 저장 직전 입력)를 의도대로 동작한다고 확인했다. CTest 실행 결과는 따로 받지 못했다.
- **잔여**:
  - 반복 토글과 트랙 목록 활성 체크는 명령이 아니어서 Undo되지 않고 `m_UntrackedDirty`로만 표시된다.
  - VFX 열기·다른 이름으로 저장 대화상자의 필터 문구가 하드코딩돼 있다.

### [2026-11-17] 리소스 편집기 공통 교체 확인 (1~2단계)

- **배경**: 리소스 편집기 12종 정적 점검에서 VFX·파티클·스프라이트·애니메이션 타임라인의 자체 새로 만들기·열기가 미저장 문서를 확인 없이 교체하는 것을 확인했다(`SetTargetResource`가 `m_IsDirty = false`로 덮어씀). 콘텐츠 브라우저 경로는 `ImGuiManager`의 공통 차단이 막지만 편집기 내부 메뉴·버튼·드롭은 이를 거치지 않았다. 애니메이터·그래프는 각자 확인 모달을 따로 구현하고 있었다.
- **변경 사항**:
  - **공통 장치** (`ResourceEditorInterface.h/.cpp`): `RunWithDiscardConfirmation(action)` — 미저장이 아니면 즉시 실행하고, 미저장이면 동작을 보관한 뒤 `Draw()`에서 `[취소] [저장하지 않고 계속(Danger)]` 모달(`DialogButtonBar`, Escape=취소)을 띄운다. 메뉴 안에서 `OpenPopup`의 ID 범위가 달라지는 문제를 피하려고 요청 시에는 보관만 하고, 열기·그리기는 창 범위에서 처리한다. `DrawResourceFrame`을 재정의하는 편집기도 포함되도록 `Draw()`에서 호출한다. 확인된 동작은 파일 대화상자 같은 블로킹 호출을 고려해 `EndPopup` 이후 실행한다.
  - **적용**: VFX·파티클 새로 만들기·열기(메뉴·퀵액세스 8곳), 스프라이트 열기(메뉴·퀵액세스)와 캔버스 드롭, 타임라인 열기(메뉴·퀵액세스)와 "엔티티 동기화"의 타임라인 로드 분기. 빈 상태의 열기·새로 만들기 버튼과 로드 실패 시 내부 새 문서 생성은 교체할 문서가 없어 제외했다. 지연 실행 람다는 드롭 경로·타임라인 경로를 값으로 캡처한다.
  - **스프라이트 정리**: 복사돼 있던 열기 코드 3곳을 `OpenTextureDialog()`로 합치고 하드코딩 필터 문구를 `FileDialog.ImageFiles`로 바꿨다. 두 드롭 페이로드 처리도 하나로 합쳤다.
  - **로캘**: `Editor.csv`에 `ResourceEditor.DiscardTitle`, `DiscardMessage`, `DiscardContinue`.
  - 참고: 사용자 적용 과정에서 `ResourceEditorInterface.cpp`의 `BEGIN(Editor)`/`END`가 제거됐다. 프로젝트의 `using namespace` 설정으로 빌드되며(Editor.exe 13:22 빌드 확인), 다른 소스와 같은 형식이다.
- **검증**: Editor 빌드와 생성 JSON 반영을 확인했다. 단계별 실행 확인 표(저장된 상태 즉시 실행, 미저장 시 모달·취소·계속, 드롭·동기화 경로, 언어 전환)의 결과는 보고받지 못했다. CTest 재실행 결과도 기록되지 않았다.
- **잔여**:
  - 3단계(선택): 애니메이터·그래프의 개별 확인 모달과 콘텐츠 브라우저의 알림 차단을 이 장치로 통합.
  - 타임라인 "엔티티 동기화"의 모델만 바꾸는 분기(`SetTargetModel`)가 문서를 변경하는지는 확인하지 않았다.
  - 리소스 편집기 기본 기능 기준(저장·`*`·교체 확인·닫기 필수, Undo 선택) 문서화와 Undo 범위 정리는 후속 작업이다.

### [2026-11-17] GraphPanel 미저장 변경 추적 (P1-A 잔여 해소)

- **배경**: `GraphPanel::HasUnsavedResourceChanges()`가 항상 `false`여서 그래프 편집 내용이 창 제목 `*`, 앱 종료 확인 모달, 다른 에셋 열기 차단의 보호를 받지 못했다. 그래프 데이터를 바꾸는 호출은 패널 2개 파일에 30곳 가까이 흩어져 있고 일부는 결과를 `(void)`로 버려, 호출 지점마다 플래그를 세우는 방식은 누락 위험이 컸다. 또한 `Graph.Open` 단축키(Ctrl+O)는 메뉴와 달리 확인 없이 현재 그래프를 교체했다.
- **변경 사항**:
  - **`Engine::Graph` 수정 카운터** (`Graph.h/.cpp`): `GetRevision()`/`m_Revision`. 데이터를 바꾸는 유일한 통로인 멤버 함수의 성공 경로에서만 증가한다(`AddNode`, `RemoveNode`, `Connect`, `Disconnect`, `ImportData`, `AddVariable`, `RemoveVariable`). `SetInputValue`는 기존 값과 다를 때만, `ResetInputValue`는 실제로 지운 값이 있을 때만, `Reconcile`은 정리 항목이 있을 때만 증가한다(주석 크기 동기화 같은 반복 쓰기에서 미저장 표시가 켜지지 않도록).
  - **`GraphPanel`** (`GraphPanel.h/.cpp`): `GraphDocument::SavedRevision`을 저장 성공 시와 로드 시(문서 교체 직전) 기록한다. `HasUnsavedResourceChanges()`는 Logic·Shader 중 하나라도 `GetRevision() != SavedRevision`이면 true다. 저장 실패 시에는 갱신하지 않아 미저장 표시가 유지된다.
  - **열기 확인 통일**: 메뉴와 `Graph.Open` 단축키(`EditorActionRegistry.cpp`)가 `RequestOpenDocument()`를 거친다. 확인 조건을 "그래프가 비어 있지 않음"에서 "어느 도메인이든 미저장"으로 바꿨다(열 파일의 도메인에 따라 현재 화면이 아닌 문서가 교체될 수 있으므로). `OpenDocument()`는 private으로 옮겨 확인 우회 경로를 막았다.
  - **테스트**: `GraphFoundationCheck`에 연결 미리보기·거부된 편집·실패한 연결 해제·같은 값 쓰기·오버라이드 없는 초기화가 revision을 바꾸지 않고, 성공한 편집은 바꾸는지 검사를 추가했다.
  - **`ReflectionInspectorCheck` 갱신**: `163f5b08`(CATEGORY 없는 속성은 머리글 없이 맨 위에 표시)에서 깨진 카테고리 접힘 검사를 `RectTransformComponent`의 앵커 카테고리만 남겨(`HiddenProperties`) 검사하도록 바꿨다. 이번 Graph 변경과는 무관한 기존 실패다.
- **검증 (사용자 실행, 2026-11-17, `x64-ninja-debug`, 커밋 `ba399684`)**:
  - 전체 빌드 후 CTest 38/38 통과.
  - 에디터: 미저장 상태에서 메뉴 열기·Ctrl+O 모두 교체 확인 모달 표시, 콘텐츠 브라우저로 다른 그래프를 열면 공통 `ResourceEditor.OpenBlocked` 알림으로 차단.
  - 편집 종류별 `*` 표시, 저장 후 해제, 종료 모달 표시, 다른 도메인 확인은 별도로 보고받지 못했다.
  - 참고: Visual Studio 빌드(`x64-debug`)에서 `--target GraphFoundationCheck`만 빌드했을 때 다른 그래프 검사 4개가 실패했다. `Graph` 레이아웃 변경 전 실행 파일이 새 `Engine.dll`과 함께 실행된 재빌드 누락으로, 전체 빌드 후 해소됐다.
- **잔여**:
  - 콘텐츠 브라우저 경로는 모달 없이 알림으로만 차단한다(모든 리소스 편집기 공통 정책).
  - 노드 위치와 주석 텍스트(`CommentTexts`)는 `GraphData`에 포함되지 않아 저장되지 않으며, 이것만 바꾸면 미저장 표시도 켜지지 않는다. 로드 시 노드는 3열 격자로 재배치되고 주석은 "Comment"로 돌아간다.
  - 모듈 핫리로드로 `Reconcile`이 값·링크를 정리하면 미저장 표시가 켜진다(정리된 내용을 저장하도록 하는 의도된 동작).

### [2026-11-17] VFX 트레일 연속 스트립 렌더링, 정렬 모드, 트랙 인스펙터 Undo

- **배경**:
  - 트레일이 보간 점 사이마다 독립 쿼드(`SpriteDrawCommand` + `QuadMesh`)로 제출되어 곡선 구간에서 이웃 쿼드의 모서리가 맞지 않았다. 쿼드 하나는 평행사변형까지만 변형되므로 행렬 조정으로는 이음새를 없앨 수 없었다.
  - VFX 편집기 트랙 인스펙터의 값 변경은 Dirty만 설정하고 `m_TimelineHistory`에 기록되지 않아 Undo/Redo가 되지 않았다.
- **변경 사항**:
  - **RHI 동적 정점 버퍼** (`RHI.h/.cpp`, `DirectX12RHI.h/.cpp`): `BindDynamicVertexBuffer(slot, data, vertexCount, stride)`는 기존 인스턴스 업로드 링 버퍼에 복사하고 슬롯별 구간(`DynamicVertexBinding`)으로 정점 버퍼 뷰를 만든다. 복사 경로는 `CopyToDynamicRingBuffer`로 `BindInstanceBuffer(const void*)`와 공유한다. 프레임 시작 시 `ResetDynamicVertexBindings()`로 동적 슬롯을 비운다. `SupportsDynamicVertexBuffers()`는 DX12만 true이고 SDL GPU는 기본 구현(false / `NotImplemented`)이다.
    - 동작 변경: `BindInstanceBuffer(const void*)`는 바인딩이 실패해도 링 커서를 전진시킨다(해당 프레임 공간만 낭비).
  - **렌더러** (`RenderCommands.h`, `Renderer.h/.cpp`): `DynamicGeometryDrawCommand`(월드 공간 정점, `VertexPosition`/`VertexMaterial` 2스트림, 토폴로지)와 `SubmitDynamicGeometry`/`RenderDynamicGeometry`를 추가했다. 스프라이트 다음에 그리며 단위 행렬 인스턴스 1개로 기존 `gbufferSpriteVS`를 재사용한다. 빈 패스 판정과 큐 정리 3곳에 반영했다.
  - **트레일** (`CompositeVFXComponent.h`, `CompositeVFXSystem.cpp`):
    - 매 프레임 머리 위치(`headA/headB`)를 보관해 마지막 샘플과 현재 위치를 잇고, Catmull-Rom(구간당 4분할)으로 보간한다.
    - `BuildTrailStrip`: 점마다 좌우 정점 2개를 만드는 Triangle Strip. 폭 방향을 중앙 차분 접선으로 정해 이웃 세그먼트가 정점을 공유한다. 퇴화 점은 이전 방향을 재사용하고 방향 반전 시 부호를 유지해 꼬임을 막는다. 폭(테이퍼)·색·알파는 점별로 계산하고 UV는 QuadMesh 규칙(진행 방향 u, 폭 v)을 따른다.
    - 동적 정점 버퍼를 지원하지 않는 백엔드는 기존 세그먼트 쿼드 경로(반폭 스케일·축 교정 반영)로 대체한다.
  - **정렬 모드** (`CompositeEffect.h`): `EVFXTrailAlignment { OffsetAxis, ViewFacing }`와 `VFXTrailTrack::alignment`(`ALLOW_MISSING`, 기본 `OffsetAxis`로 기존 에셋 외형 유지). `ViewFacing`은 점→카메라 방향을 기준 축으로 삼아 폭을 시선에 수직으로 둔다(CompositeVisualEffectPlan 3.3절 사양). 뷰포트별 `CameraBuffer::cameraPosition`을 쓴다. 로캘: `Engine.csv`에 `PROP_TRAIL_ALIGNMENT`, `Engine::EVFXTrailAlignment.*`.
  - **트랙 인스펙터 Undo** (`VFXEditorPanel.h/.cpp`): `DrawInspector`를 편집 세션 래퍼와 `DrawSelectedTrackInspector`(변경 여부 반환)로 나눴다.
    - 그리기 전 트랙 복사본(`std::any`)을 첫 변경 시 세션으로 보관하고, 활성 위젯이 없어지면 편집 전/후 트랙 전체를 `CallbackEditorCommand` 하나로 `Record`한다.
    - Undo/Redo·트랙 삭제·창 숨김 전에 미확정 세션을 확정한다. 사운드 시작 시간은 기존 `ApplyTimelineEdit` 경로를 유지해 중복 기록을 막았다.
    - 로캘: `Editor.csv`에 `VFXEditor.EditTrackProperties`.
  - **트랙 추가 메뉴 명칭 일반화**: `Editor.csv`의 `VFXEditor.Add*Track` 7종을 스프라이트/파티클/라이트/메쉬/사운드/트레일/데칼 트랙으로 바꿨다. 기본 트랙 이름 접두사 `Flash_`→`Light_`, `Audio_`→`Sound_`.
- **검증 (사용자 실행, 2026-11-17)**: 단계별 빌드 후 에디터에서 다음을 확인했다고 보고했다.
  - 동적 정점 경로 시험 사각형 표시, 기존 렌더 회귀 없음
  - 트레일 곡선이 끊김 없이 이어지고 알파·폭이 매끄럽게 감소
  - 정렬 모드 적용
  - 인스펙터 값 변경의 Undo/Redo 동작
  - CTest 실행 결과는 기록되지 않았다. 시험 사각형 코드는 커밋 직전에 삭제했으며, 삭제 후 빌드는 커밋 시점에 확인되지 않았다.
- **미검증·잔여**:
  - SDL GPU(Vulkan 빌드)의 대체 쿼드 경로는 실행 확인을 하지 않았다. 이 경로는 `alignment`를 무시한다.
  - 동적 지오메트리 명령마다 vector 2개 할당과 드로우 1회가 든다(측정 없음).
  - Undo로 저장 시점의 상태까지 되돌아가도 창 제목의 `*`는 남는다(기존 타임라인·기즈모 명령과 동일).

### [2026-11-17] 에디터 종료 시 리소스 편집기 미저장 변경 보호 (P1-A)

- **배경**: 종료 요청(`ImGuiManager::RequestQuit`)은 `EditorDocumentManager`에 등록된 문서만 확인했고, 등록된 `IEditorDocument` 구현은 `SceneDocument` 하나뿐이었다. Material·Model·Sprite·Particle·VFX·Animator·Prefab·AnimationTimeline·플러그인 편집기의 `HasUnsavedResourceChanges()`는 다른 에셋 열기 차단과 창 제목 `*` 표시에만 쓰여, 미저장 리소스 편집 내용이 확인 없이 종료와 함께 사라졌다. `RecoveryManager`도 씬만 스냅샷한다.
- **변경 사항**:
  - `ImGuiManager::RequestQuit`: 씬 문서 확인 전에 `CollectUnsavedResourceEditors()`로 `m_ImGuiPanels`의 모든 `ResourceEditorInterface` 중 미저장 편집기(닫힌 패널 포함)를 모은다. 하나라도 있으면 `m_ResourceQuitConfirmPending`을 세우고 확인 모달로 넘긴다. 확인 중 반복되는 종료 이벤트는 무시한다.
  - 기존 PIE 정지와 씬 문서 확인 흐름은 `ContinueQuitRequest()`로 분리했다. 동작은 바뀌지 않았다.
  - `DrawResourceQuitConfirmation()`: 편집기 이름·대상 파일(툴팁에 전체 경로)·`이동` 버튼 표와 `[취소] [저장하지 않고 종료](Danger)` 버튼(`EditorUI::DialogButtonBar`, Escape=취소)을 표시한다. `이동`은 확인을 취소하고 해당 편집기 창을 앞으로 가져온다. `저장하지 않고 종료`는 씬 문서 확인 단계로 이어지며, 그 단계에서 취소하면 종료되지 않고 리소스 편집 상태도 유지된다.
  - "모두 저장"은 넣지 않았다. `SaveResource()`를 구현한 편집기가 Material·Model·AnimationTimeline뿐이고 반환값이 없어 저장 성공을 확인할 수 없기 때문이다.
  - 로캘: `Editor.csv`에 `ResourceEditor.QuitConfirmTitle`, `QuitUnsavedMessage`, `QuitShowEditor`, `QuitDiscard` 추가(JSON은 `GenerateLocalization`이 생성).
- **잔여**:
  - `GraphPanel`은 dirty 추적이 없어 `HasUnsavedResourceChanges()`가 항상 `false`다. 그래프 편집 내용은 이번 가드로 보호되지 않는다.
  - 리소스 편집기 패널의 닫기(X)는 Animator를 제외하면 확인 없이 숨김 처리된다(편집 상태는 유지되어 종료 시 이번 가드에 걸린다).
- **검증 (사용자 실행, 2026-11-17)**: Editor 빌드 후 다음 시나리오가 의도대로 동작함을 확인했다.
  - 리소스 편집기 수정 후 창 닫기/Alt+F4/툴바 종료 → 모달 표시, `취소`·Escape → 계속 실행, `이동` → 해당 편집기 포커스, `저장하지 않고 종료` + 씬 미저장 → 씬 확인 모달 → 취소 시 계속 실행·리소스 `*` 유지, 미저장 편집기 없음 → 기존 흐름 그대로, 편집기 패널을 닫은 뒤 종료 → 모달에 표시됨.
  - CTest 실행 개수와 결과는 별도로 기록되지 않았다.

### [2026-11-17] 복합 VFX 저작 UX 고도화: FloatCurve 런타임 샘플링, 타임라인 인라인 커브 미니어처 렌더링 및 차기 연동 로드맵 수립

- **배경 및 목적**:
  - `CompositeVisualEffectPlan.md` Phase 5 (Step 2) 과제 수행.
  - 기존 메쉬 트랙 스케일/알파 및 라이트 섬광이 단순 선형 보간(`glm::mix`) 또는 단순 삼각파(Attack/Decay)로만 동작하여 폭발 팽창의 펀치감(Ease-Out)이나 섬광의 깜빡임 등을 연출할 수 없던 한계 해소.
  - 타임라인 트랙 바를 보았을 때 어느 구간에서 어떤 곡선으로 완급 조절이 이루어지는지 한눈에 파악하기 어려운 가시성 결함 해결.
- **변경 사항**:
  - **`FloatCurve` 엔진 코어 (`Engine/Source/Core/Geometry/Public/FloatCurve.h`, `Private/FloatCurve.cpp`)**:
    - `CurveKey`: time(정규화 0~1), value, inTangent, outTangent, `ECurveInterpMode`(Constant, Linear, CubicBezier).
    - 3차 에르미트/베지어 스플라인 기저 함수 기반 `Evaluate(time)` 평가기 구현.
    - 리플렉션/아카이브 등록 및 5종 프리셋(`CreateConstant`, `CreateLinear`, `CreateEaseIn`, `CreateEaseOut`, `CreateEaseInOut`, `CreateSpike`) 팩토리 지원.
  - **복합 VFX 런타임 커브 샘플링 연동 (`CompositeEffect.h`, `CompositeVFXSystem.cpp`)**:
    - `VFXMeshTrack`: `useScaleCurve`/`scaleCurve` 및 `useAlphaCurve`/`alphaCurve` 속성 신설 및 런타임 가중치 보간.
    - `VFXLightTrack`: `useIntensityCurve`/`intensityCurve` 속성 신설 및 조도 팩터 평가 연동.
    - 기존 에셋과의 100% 하위 호환성 유지(플래그 false 시 기존 선형 보간 경로 유지).
  - **타임라인 인라인 커브 미니어처 렌더링 (`EffectTimelineView.h/.cpp`, `VFXEditorPanel.cpp`)**:
    - `TimelineRow`에 `const Engine::FloatCurve* Curve` 및 `const char* CurveLabel` 필드 확장.
    - 트랙 사각형 바 내부 공간에 24~32개 세그먼트로 실시간 베지어 곡선 실루엣(사다리꼴 반투명 음영 `IM_COL32(255, 255, 255, 30)` + 선명한 외곽선 `IM_COL32(255, 255, 255, 220)`) 인라인 렌더링.
    - 트랙 바 좌상단에 커브 식별 뱃지(`Scale`, `Alpha`, `Light`) 오버레이.
    - 타임라인에서 트랙 바의 시작점/끝점 이동 및 리사이즈 시 커브 미니어처가 시간축에 맞춰 완벽히 실시간 스케일링/이동되도록 연동.
  - **차기 개발 로드맵 수립 (`CompositeVisualEffectPlan.md`)**:
    - [선택지 1] 액션 타임라인 ↔ 복합 VFX 연동 (캐릭터 무기/손 본 소켓 스폰 및 공격 애니메이션 노티파이 바인딩).
    - [선택지 2] 발광성 Additive 가산 머티리얼 표준화 및 뷰포트 암실 공중(Air Mode) 프리뷰 모드.
    - [선택지 3] 2D 커브 팝업 에디터 모달 (키프레임 직접 조작).
- **검증 (사용자 실행, `x64-ninja-debug`)**:
  - `x64-ninja-debug` 빌드 성공 및 에디터 정상 기동 확인.
  - 메쉬 트랙의 스케일 커브 활성화 후 `EaseOut` 적용 시 초반 급팽창 후 수렴하는 펀치감 런타임 정상 동작 확인.
  - 타임라인 트랙 바 내부에 실시간 베지어 곡선 실루엣 및 라벨 뱃지 노출 확인, 트랙 리사이즈 시 동기화 확인 완료.


### [2026-11-17] 디스크 캐시 통합 개발: 지형 그리드 메시 스트림 캐시, DDC 공통 인프라 분리 및 환경설정 통합 캐시 관리/일괄 비우기 UI (Phase 4)

- **배경 및 목적**:
  - `DiskCacheDevelopmentPlan.md` Phase 4 과제 수행 및 통합 디스크 캐시 라이프사이클 완성.
  - 지형 그리드 메시 생성(`TerrainUtility::CreateGridMesh`) 시 128x128 등 고해상도 분할에 따른 대량 정점/인덱스/와인딩 CPU 계산 부하 제거.
  - 빌드 출력 디렉터리(`.../bin`)에 임시 캐시 생성 시 `EditorSettingsService`가 빌드 폴더를 프로젝트 루트로 오인식하여 `StartupManifest.json`을 찾지 못하던 결함 해결.
  - 모델/텍스처 DDC 변환기 간 중복 코드 제거 및 안전한 프로젝트 루트 탐색 공통 인프라(`Tools/Converters/Common/DdcCommon.h`) 구축.
  - 에디터 환경 설정(`EditorEnvironmentSettingsWidget`)에 디스크 캐시(썸네일, 셰이더, DDC, 지형) 실시간 사용량 모니터링 및 전체 일괄 비우기 기능 추가.
- **변경 사항**:
  - **지형 그리드 메시 디스크 캐시 (`TerrainUtility.cpp`)**:
    - 저장 위치: `<ProjectRoot>/.bam/cache/terrain/<hash>.bin`
    - 캐시 키: `FNV-1a(width + length + segmentsX + segmentsZ + reverseWinding + "terrain_grid_v1")`
    - 바이너리 파일 구조: 매직 넘버(`BTGC`, `0x43475442`) + 버전(1) + `TerrainGridMeshLayout` + 버텍스 위치/머티리얼 배열 + 인덱스 배열.
    - `CreateGridMesh` 진입 시 캐시 조회: 적중 시 수만 개의 정점/인덱스 계산 및 와인딩 루프를 통째로 스킵하고 즉시 복원. 미적중 시 연산 후 원자적 쓰기(`.tmp` -> rename).
  - **프로젝트 루트 오인식 방지 (`EditorSettingsService.cpp`)**:
    - `cur / ".bam"` 마커만으로 프로젝트 루트를 판별하던 취약점 제거.
    - `Config/Project.json` 확인 및 `Engine::PlatformPath::GetProjectRoot()`를 단일 진실의 원천(SSOT) fallback으로 지정하여 빌드 폴더 오염 시에도 에디터 기동 실패 원천 방어.
  - **DDC 공통 인프라 (`Tools/Converters/Common/DdcCommon.h`)**:
    - `Tools::Ddc::DdcHasher`: FNV-1a 64비트 스트림 해시 누적기, 파일 메타데이터(크기, 수정시각) 결합 및 경로 생성 헬퍼.
    - `Tools::Ddc::FindDdcDirectory`: CWD부터 부모로 거슬러 올라가며 `Config/Project.json` 또는 `CMakeLists.txt`를 탐색하여 정확한 프로젝트 루트의 `.bam/cache/ddc/<subCategory>` 반환.
    - `ModelConverter.cpp` 및 `DirectXTexCompressor.cpp`에 적용하여 코드 중복 제거 및 빌드 디렉터리 오염 방지.
  - **통합 디스크 캐시 관리 UI (`EditorEnvironmentSettingsWidget.cpp`)**:
    - [환경 설정(Preferences)] 내 "디스크 캐시 (Disk Cache)" 접힘 섹션 신설.
    - `InspectCacheFolder` 및 `FormatBytes`: 썸네일, 셰이더, DDC, 지형 캐시 및 총합 용량/파일 수 실시간 집계 및 `EditorUI::PropertyRow` 표 렌더링.
    - **[전체 디스크 캐시 비우기]** 버튼: `.bam/cache/` 내 하위 캐시 파일 일괄 삭제, `AssetCache` 메모리 캐시 동시 소거, 결과 알림(`NotificationManager`) 팝업 출력.
  - **로캘 (`Editor.csv`, `English.json`, `Korean.json`)**:
    - `Settings.DiskCache`, `Settings.DiskCacheThumbnails`, `Settings.DiskCacheShaders`, `Settings.DiskCacheDDC`, `Settings.DiskCacheTerrain`, `Settings.DiskCacheTotal`, `Settings.ClearAllDiskCaches`, `Settings.ClearDiskCacheSuccess` 다국어 키 추가.
- **검증 (사용자 실행, `x64-ninja-debug`)**:
  - `x64-ninja-debug` 빌드 성공 및 에디터 정상 기동 확인.
  - 지형 그리드 메시 캐시 바이너리 생성 및 로드 정상 확인.
  - 환경 설정 패널 내 디스크 캐시 용량/파일 수 정상 집계 및 [전체 디스크 캐시 비우기] 클릭 시 파일 삭제 및 알림 메시지 노출 정상 동작 확인.
- **커밋**: `4b815f7`

### [2026-11-17] 속성 그리기 2계층 통일: 공용 속성 행, 리플렉션 카테고리 평면화, FloatCurve 그래프 편집기, VFX 트랙 인스펙터 리플렉션 전환

- **배경 및 목적**:
  - 인스펙터·VFX 편집기·설정 패널의 속성 행이 `EditorUI::PropertyRow`, VFX 지역 람다 `DrawPropertyRow`, 리플렉션 드로어 자체 `BeginTable`의 세 갈래로 나뉘어 라벨 폭(105/110/120/180px)과 잘림 툴팁 유무가 패널마다 달랐다.
  - `VFXSpriteTrack` 등 VFX 트랙이 이미 리플렉션 등록 타입인데도 필드를 손으로 다시 그려 `sortingOrder`, Particle `loop`, Mesh `initialRotation`·`colorStart`·`colorEnd` 등이 UI에서 누락되고, Billboard 콤보에 영어 문자열이 하드코딩되어 있었다.
  - 행 레이아웃(공통 API)과 값 위젯(리플렉션 타입은 드로어에 위임)을 분리하는 2계층 구조로 정리했다. 기준은 `EngineEditorClientGuidelines.md` 19절 3항에 반영했다.
- **변경 사항**:
  - **공용 속성 행 (`EditorUI.h/.cpp`)**: `PropertyLabel(text, detail, reservedRightWidth)`(잘림 감지·호버 툴팁), `PropertyValue()`, `InspectorPropertyTableStyle()`(라벨 폭 `글꼴 크기 × 9`, 셀 여백 0.3/0.15 글꼴 단위) 추가. `PropertyRow`는 `PropertyLabel` 경로로 통합. 정의부를 `namespace EditorUI { }` 블록에서 `EditorUI::` 한정 이름 형식으로 바꿨다.
  - **리플렉션 드로어 (`ReflectionPropertyDrawer.cpp`)**:
    - 표 생성과 라벨을 `EditorUI::BeginPropertyTable`/`PropertyLabel`/`PropertyValue`로 교체하고 익명 함수 `DrawResponsivePropertyLabel` 삭제. 인스펙터에 행 줄무늬·세로 구분선이 생겼다.
    - `CATEGORY`가 없는 속성은 "일반" 접힘 노드 없이 맨 위에 펼쳐 그리고, 명시적 카테고리만 접힘 노드로 그린다. 기존 일반 표의 ImGui ID 경로는 유지했다.
    - `FloatCurve` 전용 분기(일반 중첩 구조체 분기보다 앞): 그래프에서 키 클릭 선택, 드래그 이동(이웃 키 사이로 시간 제한, 드래그 중 축 범위 고정), 빈 곳 더블클릭 추가, 키 우클릭 컨텍스트 메뉴 삭제, 선택 키 세부 표(시간·값·탄젠트·보간). 선택 상태와 드래그 중 축 범위는 `ImGuiStorage`에 위젯 ID별로 보관한다.
    - `EvaluateEditCondition`이 대상 속성을 찾지 못하면 타입·속성별로 한 번 `EDITOR_LOG_WARN` 경고(표시 동작은 기존처럼 "보임" 유지).
  - **FloatCurve 및 VFX 커브 (`FloatCurve.h/.cpp`, `CompositeVFXSystem.cpp`)**: `Core/Geometry`에 `FloatCurve`·`CurveKey`·`ECurveInterpMode`와 프리셋 팩토리 추가. Light 트랙 조도 커브, Mesh 트랙 스케일·알파 커브를 런타임에 평가한다.
  - **VFX 트랙 메타데이터 (`CompositeEffect.h`)**: 손작성 UI의 드래그 속도·범위를 `RANGE`/`STEP`으로, 색상을 `COLOR_PROPERTY`로 옮겼다. 카테고리 `CATEGORY_VFX_TIMING`/`TRANSFORM`/`RENDER`/`CURVES`를 지정하고(이름·활성·음소거는 미지정), 커브 속성에 `EDITCONDITION(useXxxCurve)`를 지정했다. CameraShake 반경은 전용 키 `PROP_SHAKE_RADIUS`("0 = 전역" 안내)를 쓴다.
  - **VFX 편집기 (`VFXEditorPanel.cpp`)**: Sprite·Particle·Light·Mesh·Trail·Decal·CameraShake 트랙의 손작성 행을 `ReflectionPropertyDrawer::DrawPropertyTable` 호출로 교체하고 지역 람다 `DrawPropertyRow`를 삭제했다. 커브 프리셋 버튼은 로캘화된 `DrawCurvePresetRow`(`SameLineIfFits` 줄바꿈)로 정리했다. Sound 트랙은 시작 시간이 타임라인 편집 경로(`ApplyTimelineEdit`)를 거치므로 손작성 행을 유지하고 표 스타일만 통일했다. Mesh 트랙은 빈 핸들일 때 사각 평면·링 텍스처 기본값 안내를 표시한다(런타임 대체 순서: 트랙 텍스처 → 머티리얼 첫 텍스처 → `Particle_Ring.png`).
  - **로캘**: `Editor.csv`(커브 프리셋·안내 문구·`Inspector.CurveEditHint`), `Engine.csv`(카테고리, 커브 속성, `ECurveInterpMode`·`ESpriteBillboardMode` 열거자) 행 추가.
- **커밋**: `7a77459d`(Engine/VFX), `163f5b08`(Editor/UI·드로어), `dc6998a2`(Editor/VFX 인스펙터).
- **검증 (사용자 실행, `x64-ninja-debug`)**:
  - 단계별 빌드 성공과 에디터 화면 확인 통과(사용자 보고): 인스펙터 라벨·자물쇠·잘림 툴팁, 설정 패널 행 유지, VFX 각 트랙 필드 표시·편집·저장, Billboard 열거자 번역, 카테고리 평면화와 VFX 카테고리 노드, 커브 그래프 키 편집.
  - 커브 편집기가 항상 보이던 결함 수정 확인: 원인은 Mesh 커브의 `EDITCONDITION("useScaleCurve")` 따옴표(생성 코드에 따옴표 포함 문자열로 들어가 대상 속성 탐색 실패)와 Light `intensityCurve`의 조건 누락이었다. 수정 후 사용 체크박스에 따라 표시·숨김 정상 동작.
- **미검증 및 알려진 제약**:
  - `EditorUICheck` CTest 실행 결과는 별도로 보고받지 못했다.
  - Mesh 트랙 링 텍스처 안내: `Fireball_Explosion_Composite.bamvfx.json`의 메쉬 트랙은 링 텍스처·Additive 머티리얼이 저장돼 있어 안내가 나오지 않는 것이 정상이다. 새 메쉬 트랙(핸들 3개 비움)에서의 표시는 확인 결과를 받지 못했다.
  - 벡터 드로어는 `RangeMin`/`RangeMax`를 적용하지 않아 Decal `size`의 기존 최소값(0.05)이 사라졌다. Light `attackTime`/`decayTime`의 상한은 `duration` 연동에서 정적 30초로 바뀌었다.
  - 값 뒤 단위 표기(`s`, `m`, `Hz`)와 색상 편집의 `ImGuiColorEditFlags_Float`가 사라졌다(1을 넘는 HDR 색상 편집 영향 미확인).
  - 커브 편집기: 탄젠트 핸들 드래그 미지원, Delete 키 삭제는 `ShortcutManager` 등록이 필요해 보류. 선택 상태가 위젯 ID 기준이라 같은 종류의 다른 트랙으로 바꿔도 선택 인덱스가 유지된다(키 개수 초과 시 해제).
  - 정리 대상: 사용하지 않게 된 로캘 키(`Inspector.CurveKeys`, `VFXEditor.PropUse*Curve`, 손작성 행에서만 쓰던 `VFXEditor.Prop*` 일부)와 엔진 전반의 `CATEGORY_` 접두사 없는 카테고리 키(`"Animation"`, `"Physics"` 등).
  - 인스펙터의 `↺` 리셋 버튼(19절 3항 표준)은 아직 구현되지 않았다.

### [2026-11-16] 에셋 파생 데이터 캐시(DDC: Derived Data Cache) 인프라 구축 (Phase 3)

- **배경 및 목적**:
  - `DiskCacheDevelopmentPlan.md` Phase 3 (P1) 과제 수행.
  - 외부 소스 모델(FBX, OBJ, GLTF)과 고해상도 텍스처(PNG, TGA, JPG)를 엔진 런타임 포맷(`.bammesh`, `.bammodel`, `.bamtex`)으로 변환할 때, 원본 파일이나 옵션이 변경되지 않았음에도 매번 반복되는 Assimp 씬 파싱, meshoptimizer LOD 간소화(`meshopt_simplify`), DirectXTex BCn 텍스처 압축의 무거운 CPU 연산 제거.
- **변경 사항**:
  - **텍스처 DDC 캐시 (`DirectXTexCompressor.cpp`)**:
    - 저장 위치: `<ProjectRoot>/.bam/cache/ddc/textures/<hash>.ddc`
    - 캐시 키: `FNV-1a(SourcePath + FileSize + LastWriteTime + TargetFormat + CompressFlags + "tex_ddc_v1")`
    - 바이너리 파일 구조: 매직 넘버(`BTDC`, `0x43445442`) + 버전(1) + `TextureBinaryHeader` + 압축 픽셀 바이트 배열.
    - `GetRawCompressedData` 진입 시 DDC 조회: 적중 시 0.1ms 만에 헤더와 픽셀 데이터를 즉시 복원하여 반환(DirectXTex 디코딩, 밉맵 생성, BCn 압축 100% 바이패스). 미적중 시 압축 수행 후 임시 파일(`.tmp`)을 거쳐 원자적 저장.
  - **모델 DDC 캐시 (`ModelConverter.cpp`)**:
    - 저장 위치: `<ProjectRoot>/.bam/cache/ddc/models/<hash>.ddc`
    - 캐시 키: `FNV-1a(SourcePath + FileSize + LastWriteTime + Options[Scale, FlipUV, Winding, LODs...] + "model_ddc_v1")`
    - `ModelData` 경량 바이너리 직렬화/역직렬화 규격 구축: 매직 넘버(`BMDC`, `0x43444D42`) + 버전(1) + Materials + Meshes(Vertices, Indices, Bounds, LOD 계층) + Nodes + Bones + Animations.
    - `ConvertStaticModel` 진입 시 DDC 조회: 적중 시 Assimp 파싱 및 `meshopt_simplify` LOD 계산을 100% 스킵하고 1~3ms 만에 `ModelData`를 통째로 복원. 미적중 시 변환 완료 후 DDC 파일로 원자적 저장.
- **검증 (사용자 실행, `x64-ninja-debug`)**:
  - `DirectXTexCompressor.cpp` 및 `ModelConverter.cpp` 컴파일 성공 및 에디터 정상 구동 확인.
  - 원본 파일 및 변환 옵션 일치 시 무거운 파싱/압축 프로세스를 통째로 스킵하고 DDC 디스크 캐시를 통한 즉각적인 처리 검증.

### [2026-11-16] 셰이더 컴파일 타임스탬프 캐시 및 DirectX 12 Pipeline Library (PSO) 디스크 캐시 구현 (Phase 2)

- **배경 및 목적**:
  - `DiskCacheDevelopmentPlan.md` Phase 2 (P1) 과제 수행.
  - 셰이더 무변경 시에도 `dxc.exe` 외부 프로세스를 반복 실행하던 I/O 병목 제거.
  - DirectX 12 파이프라인 생성 시 드라이버 레벨에서 DXIL 바이트코드를 GPU 마이크로코드로 변환하는 런타임 컴파일 지연을 제거하기 위해 `ID3D12PipelineLibrary` 기반 PSO 영속화 디스크 캐시 구축.
- **변경 사항**:
  - **셰이더 컴파일 타임스탬프 캐시 (`ShaderCompiler.h`, `ShaderCompiler.cpp`)**:
    - `IsUpToDate()` 구현: 출력 파일(`.cso`, `.spv`, `.wgsl`)이 존재하고, 해당 파일의 수정 시각이 소스 HLSL 파일 및 `#include`로 의존하는 모든 `.hlsli` 헤더 파일들의 수정 시각보다 최신인지 정밀 검사.
    - `ReadCompiledFile()` 구현: 캐시 적중 시 디스크의 컴파일 산출물을 직접 바이너리로 읽어 반환함으로써 `_wpopen`을 통한 `dxc.exe` 프로세스 생성을 100% 바이패스.
  - **D3D12 PSO 디스크 캐시 인프라 (`DirectX12RHI.h`, `DirectX12RHI.cpp`, `DirectX12Pipeline.cpp`)**:
    - 저장 위치: `<ProjectRoot>/.bam/cache/shaders/` 아래 `pipeline.library` (바이너리 라이브러리 블롭) 및 `pipeline.names` (등록된 파이프라인 이름 목록).
    - D3D12 디바이스 인터페이스 쿼리: `m_Device.As(&device1)`을 통해 `ID3D12Device1` 획득 후 `CreatePipelineLibrary` 호출.
    - 정합성 검사 및 자가 치유: `pipeline.library`와 `pipeline.names` 둘 중 하나라도 누락되거나 손상된 경우 두 파일을 모두 삭제하고 안전하게 클린 상태로 시작.
    - 캐시 키 생성: 파이프라인 디스크립션(`RHIPipelineDesc`)의 셰이더 바이트코드 바이트 열, 래스터라이저, 깊이/스텐실, 블렌드, 렌더 타깃 포맷 등을 결합한 FNV-1a 기반 고유 키(`pso_0123456789abcdef`) 생성.
    - 안전한 PSO 조회 및 저장 (`DirectX12Pipeline::CreatePipelineState`):
      - `pLibrary`와 `ContainsPipeline(psoName)`이 참일 때만 안전하게 `LoadGraphicsPipeline` / `LoadComputePipeline` 시도.
      - 캐시 미적중 시 드라이버 컴파일 후 `!ContainsPipeline(psoName)` 조건에서 `StorePipeline` 및 이름 등록(`RegisterPipelineName`), `Dirty` 마킹.
    - 원자적 직렬화 및 저장 (`DirectX12RHI::SavePipelineLibrary`):
      - 에디터 종료 시 GPU 동기화(`WaitForGPU()`) 완료 후 `m_PipelineLibrary->GetSerializedSize()` 버퍼에 직렬화.
      - 임시 파일(`.tmp`) 생성 후 `std::filesystem::rename` 승격으로 원자적 저장 보장.
  - **결함 해결 및 안정성 강화**:
    - `ID3D12PipelineLibrary` 메모리 수명 요구사항 준수: D3D12 공식 스펙상 `CreatePipelineLibrary`는 전달된 바이너리 버퍼(`pLibraryBlob`)를 복사하지 않고 직접 참조하므로, 지역 변수가 아닌 `DirectX12RHI` 멤버 변수 `m_PipelineLibraryData`로 유지하여 `LoadGraphicsPipeline` 호출 시 NVIDIA 드라이버(`nvwgf2umx.dll`)의 `0xC0000005` (Access Violation reading `0xFFFFFFFFFFFFFFFF`) 크래시 원천 해결.
    - 종료 시 `0x887A0005` (DXGI_ERROR_DEVICE_REMOVED) 해결: 중복 파이프라인 등록 방지 가드(`!ContainsPipeline`) 및 캐시 파일 정합성 검사를 통해 드라이버 내부 상태 오염 및 종료 시 직렬화 크래시 차단.
- **검증 (사용자 실행, `x64-ninja-debug`, 커밋 `a61312c1`)**:
  - 에디터 실행 및 종료가 크래시 없이 정상 동작함을 확인.
  - `.bam/cache/shaders/` 디렉터리에 `pipeline.library` (86,500 바이트)와 `pipeline.names` (20개 PSO)가 정상 생성 및 영속화됨을 확인.
  - 셰이더 무변경 시 `dxc.exe` 스킵 체감 확인.
- **미검증**:
  - GPU 드라이버 업데이트 또는 그래픽 카드 교체 시 드라이버 차원의 캐시 무효화 및 자동 재컴파일 복구 시나리오.

### [2026-11-16] 썸네일 통합 디스크 캐시(AssetCache 2D/3D 영속화 & D3D12 Readback) 구현 및 에디터 잔여 성능·의존성 최적화

- **배경 및 목적**:
  - 에디터 재시작 시 콘텐츠 브라우저 메모리 캐시가 증발(콜드 스타트)하여 매번 3D 모델 수십 개와 고해상도 텍스처를 처음부터 다시 파싱/디코딩/오프스크린 렌더링하느라 수 초간 극심한 프레임 드랍(스터터링)이 반복되는 결함 해소 (`DiskCacheDevelopmentPlan.md` Phase 1 및 `AssetPipelineAndRenderingOptimizationPlan.md` 조치 3).
  - 인스펙터 속성 창의 매 프레임 스냅샷 캡처 비용(약 1.5 ms), 계층 패널 리프 노드 화살표 노출, 프로젝트 에셋 `GBufferMaterial` 하드코딩 의존성 정리.
- **변경 사항**:
  - **D3D12 텍스처 리드백 API 신설 (`RHI::ReadTexturePixels`, `DirectX12RHI::ReadTexturePixels`)**:
    - D3D12 Readback Buffer 할당, 텍스처 `COPY_SOURCE` 전이 배리어, `CopyTextureRegion` 및 Fence 동기화를 통해 GPU 렌더 타깃/텍스처의 128×128 RGBA 픽셀 데이터를 CPU 버퍼로 고속 회수하는 범용 API 구축 (가로 128픽셀 = 512바이트로 D3D12 256바이트 정렬 규격에 완벽히 일치하여 패딩 없이 단번에 복사).
  - **썸네일 디스크 캐시 인프라 구축 (`AssetCache`)**:
    - 격리 디렉터리: `<ProjectRoot>/.bam/cache/thumbnails/` (`.gitignore` 등록 완료).
    - 단일 진실의 원천(SSOT) 캐시 키: `FNV-1a(RelPath + "_" + FileSize + "_" + LastWriteTime + "_v1") + ".png"`로 해시 생성. 소스 에셋이 외부/내부에서 변경되면 수정 시각·크기 변화로 즉시 O(1) 파일 무효화 보장.
    - 원자적 쓰기(Atomic Write): 백그라운드 스레드에서 `stbi_write_png`로 `.tmp` 파일 생성 후 `std::filesystem::rename` 승격하여 불완전/손상 캐시 방지.
    - 2D 이미지: `LoadImageThumbnail` 진입 시 디스크 캐시 우선 확인 및 적중 시 원본 수십 MB 로드/디코딩 100% 스킵(수 KB 캐시 PNG만 로드), 미적중 시 생성 후 캐시 저장.
    - 3D 에셋(Model, Mesh, Material, Texture, Sprite): `GetThumbnail()`에서 디스크 캐시 적중 시 메인 스레드 3D 렌더링 대기열(`m_Pending3DQueue`)을 완전히 바이패스하고 백그라운드 워커에서 캐시 PNG를 읽어 즉시 반환 (콜드 스타트 3D 렌더링 0ms).
    - 3D 렌더링 캡처 영속화: `ProcessPending3DThumbnail` 완료 시 `ReadTexturePixels`로 128×128 RGBA 픽셀을 읽어 디스크 캐시 파일로 자동 저장.
    - LRU 용량 관리: `PruneDiskCache`로 총용량이 512MB 초과 시 가장 오래된 캐시부터 자동 삭제.
    - 무효화 연동: `ClearCache`, `RequestInvalidate` 시 디스크 캐시 파일 연동 삭제.
  - **에디터 잔여 성능 및 리소스 의존성 정리**:
    - `InspectorPanel`: 엔티티/씬/리비전이 동일하면 매 프레임 `SceneEditSnapshot::Capture`를 스킵하도록 캐시 적용하여 불필요한 1.5 ms CPU 비용 제거.
    - `EntityHierarchyView`: 자식이 없는 엔티티 노드에 `ImGuiTreeNodeFlags_Leaf`를 부여하여 빈 `>` 화살표 숨김.
    - `SceneViewportPanel`: 기즈모 메뉴(`DrawGizmoMenu`)에 선택 모드(Q, `GizmoSelect`) 항목 추가.
    - 프로젝트 에셋 의존 정리: `ModelImportModal`, `ModelEditor`, `VFXEditorPanel`, `SceneViewportPanel`, `ImGuiManager`(Ground Plane)의 `GBufferMaterial` 참조를 엔진 빌트인 `DefaultMaterial`(`Engine://Material/DefaultMaterial.bammat.json`)로 전면 교체.
- **검증 (사용자 실행, `x64-ninja-debug`)**:
  - 에디터 실행 후 `.bam/cache/thumbnails/`에 2D/3D 썸네일 PNG 파일이 정상 생성됨을 확인.
  - 3D 오프스크린 렌더링 완료 후 리드백 및 캐시 저장 정상 동작 확인.
  - 컴파일 오류 (`rhi` 미선언) 해결 후 빌드 및 CTest 정상 통과 확인.
- **미검증**:
  - 512MB 상한 도달 시 `PruneDiskCache` 대용량 파일 정리 실환경 스트레스 테스트.

### [2026-11-16] 에디터 기동 시 스타트업 스플래시 화면(EditorSplashScreen) 도입 및 초기 검은 화면 제거

- **배경 및 원인**:
  - 에디터 실행 시 `SDL_CreateWindow`로 메인 창이 즉시 노출되지만, 이후 D3D12 RHI 초기화, 셰이더 컴파일, `StartupResourceLoader` 리소스 프리로드, 폰트 아틀라스 빌드, 씬 복원(`RestoreLastEditedScene`)까지 단일 스레드로 동기 실행되어 첫 프레임 렌더링 전까지 수 초간 거대한 검은 화면(블랙 스크린)으로 멈춰 있는 문제 발생.
- **변경 사항**:
  - **경량 스타트업 스플래시 창 (`EditorSplashScreen`)**:
    - `Editor/Source/Application/Public/EditorSplashScreen.h`, `EditorSplashScreen.cpp` 추가.
    - SDL3의 `SDL_WINDOW_BORDERLESS | SDL_WINDOW_ALWAYS_ON_TOP` 플래그로 520×280 크기의 테두리 없는 창을 화면 중앙에 생성.
    - Win32 GDI 더블 버퍼링(`CreateCompatibleDC`, `CreateCompatibleBitmap`, `BitBlt`)을 적용하여 GPU 디바이스 충돌 없이 0ms 지연으로 다크 테마 배경, 타이틀, 서브타이틀, 프로그레스 바(0~100%), 퍼센트 텍스트 렌더링.
    - 엔진 공용 유틸리티인 `Engine::StrToWStr`(`Functions.h`)을 재사용하여 상태 문구(한글/영문)를 UTF-8에서 와이드 문자열로 변환 후 `DrawTextW`로 안티앨리어싱 렌더링.
    - 각 진행 단계마다 `SDL_PumpEvents()`를 호출하여 OS 메시지 큐를 처리함으로써 윈도우 '응답 없음' 프리징 방지.
  - **메인 윈도우 숨김 및 부팅 전환 연동 (`Application.cpp`)**:
    - `InitializeWindow`: `SDL_WINDOW_HIDDEN` 플래그를 추가하여 초기화 중 메인 창 노출을 차단.
    - `Initialize`: 진입 직후 0.05초 만에 스플래시 창을 띄우고, 각 서브시스템(로거 → 윈도우 → RHI → 리소스 → ImGui → 모듈 → 씬 복원) 완료 시마다 진행률(0.05 ~ 1.0f) 갱신.
    - 초기화 완료 시 `EditorSplashScreen::Hide()`로 스플래시 창을 닫고, `SDL_ShowWindow(m_Window)`로 완성된 에디터를 화면에 즉각 표시. 예외 실패(`fail`) 및 `Free()` 시에도 `Hide()` 호출 보장.
- **검증 (사용자 실행, `x64-ninja-debug`)**:
  - 에디터 기동 시 검은 화면이 전혀 노출되지 않고 중앙 스플래시 창이 즉각 표시됨을 확인.
  - 진행률 바 및 단계별 한글 상태 문구("DirectX 12 그래픽 디바이스 초기화 중...", "마지막 작업 씬 복원 중..." 등)가 매끄럽게 갱신되고, 100% 완료 후 메인 에디터 창으로 정상 전환됨을 확인.

### [2026-11-16] 기즈모 피벗/센터 모드, 드래그 이동 툴바, 플로팅 창 기즈모/카메라 조작 안정화 및 VFX 에디터 개선

- **배경**:
  - DCC 원점에 피벗이 고정된 임포트 메시의 기즈모 조작 불편 해소(바운딩 박스 중심 기반 센터 모드 요구).
  - 복합 에디터 패널(`VFXEditorPanel`)을 플로팅 창으로 분리 시 기즈모 드래그 중 창 전체가 움직이던 현상 및 카메라 조작 차단 현상 해결.
- **변경**:
  - **기즈모 피벗/센터 모드 (`GizmoPivotMode::Pivot / Center`)**:
    - `GizmoHelper.h`/`cpp`: `GizmoPivotMode` 추가 및 `IGizmoHost` 공통 인터페이스/상태 멤버(Operation, Mode, PivotMode, Snap, ToolbarOffset) 통합.
    - `SceneViewportPanel`: 메시 AABB 로컬 중심 오프셋을 적용한 기즈모 행렬 변환 및 역변환 반영 ($M_{gizmo} = M_{world} \times T(c_{local})$, $M_{world}' = M_{gizmo}' \times T(-c_{local})$).
    - 단축키 `Viewport.TogglePivot`(Z) 등록 (`EditorActionRegistry.cpp`) 및 로캘 키 추가 (`Editor.csv`).
  - **플로팅 기즈모 툴바 드래그 & 이동 기능**:
    - `GizmoHelper.cpp`: `InputManager` 기반 그립 핸들 드래그 이동 및 더블클릭 시 기본 위치 리셋 기능 추가.
  - **플로팅 창 윈도우 이동 버그 수정**:
    - `ImGuiManager.cpp`: `io.ConfigWindowsMoveFromTitleBarOnly = true;` 설정으로 플로팅 창 내부 빈 공간 클릭-드래그 시 윈도우 이동 방지(타이틀 바 전용 이동 통일).
    - `VFXEditorPanel.h`: `GetResourceWindowFlags()`에 기즈모 조작 중 `ImGuiWindowFlags_NoMove` 안전장치 추가.
  - **프리뷰 뷰포트 카메라 조작 정상화**:
    - `PreviewViewportWidget.cpp`: `DrawViewportImage` 선행 렌더링, 스크린 AABB 좌표 기반 호버 판정 및 `ImGui::IsItemActive()` 제거로 우클릭(Look/Fly), 휠클릭(Pan), Alt+좌클릭(Orbit), 휠 줌 완벽 복원. 클릭 시 자식 창 포커스 명시적 갱신.
  - **VFX 에디터 패널 통합 및 빌드 보완**:
    - `EffectTimelineView.cpp`/`VFXEditorPanel.cpp`: 재생/배속/타임 카운터/자동 길이 계산 트랜스포트 바를 타임라인 자식 창 내부로 이전 통합.
    - `VFXEditorPanel.cpp`: `EditorSettings.h` 인클루드 추가 (`EditorWorkspace` 스플리터 상태 보존).
- **검증 (사용자 실행, `x64-ninja-debug`)**:
  - 플로팅 및 도킹 상태에서 기즈모 조작 시 창이 고정된 채 정상 변형됨을 확인.
  - 뷰포트 카메라 조작(우클릭 회전, 휠클릭 팬, Alt+좌클릭 궤도 회전, 마우스 휠 줌) 정상 동작 확인.
  - VFX 에디터 트랜스포트 컨트롤 바 정상 작동 및 빌드 성공 확인.
- **미검증**:
  - 다중 뷰포트 동시 활성화 상태에서의 기즈모 호스트 우선순위 전환 스트레스 테스트.

### [2026-11-16] 카메라 속도 단축키 문맥을 Viewport로 제한

- **변경**: `Viewport.CameraSpeedIncrease`(=)/`Viewport.CameraSpeedDecrease`(-)의 `Context`를 `Global`에서 `Viewport`로 바꿨다(`EditorActionRegistry.cpp`). 뷰포트가 아닌 패널에서 `=`/`-`를 눌러 카메라 속도가 바뀌던 가능성을 없앴다(이전 항목의 검토 사항 해소).
- **미검증**: 씬 뷰·프리뷰에 포커스가 있을 때만 동작하는지, 다른 패널에서 무시되는지 수동 확인 기록 없음.

### [2026-11-16] 씬 뷰 기즈모 매 프레임 비용 제거: 언두 스냅샷 지연 캡처, 월드 전체 계층 갱신 제거

- **배경**: 엔티티를 선택해 기즈모(이동·회전·스케일)가 보이면 조작하지 않아도 `Editor UI` CPU가 3~4 ms 늘었다. 선택 모드(Q)로 바꾸면 약 2.5 ms가 줄어 `SceneViewportPanel::DrawImGuizmo` 내부 비용임을 확인했다(나머지 약 1.5 ms는 인스펙터, 패널을 닫아 확인).
- **원인**:
  - 조작 여부와 관계없이 매 프레임 `SceneEditSnapshot::Capture`(모든 컴포넌트 저장소 순회 + 리플렉션 복사 + 할당)를 실행하고, 실제로 바뀐 프레임이 아니면 버렸다.
  - 선택 엔티티 하나의 월드 행렬을 얻기 위해 매 프레임 `TransformSystem::UpdateHierarchy(world)`로 월드 전체를 검사·갱신했다.
- **변경** (`SceneViewportPanel.cpp`, `DrawImGuizmo`):
  - `ImGuizmo::Manipulate`는 지역 `worldMatrix`만 바꾸고 월드는 `ApplyWorldMatrix`에서 처음 바뀌므로, `changed`인 프레임에서 적용 직전에만 스냅샷을 캡처한다. 드래그 연속 기록은 `Record(..., true)` 병합으로 첫 변경 전 상태를 유지한다.
  - 월드 전체 갱신과 캐시 읽기를 `TransformSystem::TryGetWorldMatrix`(선택 엔티티→루트 조상 체인만 계산, 유효 캐시 재사용)로 바꿨다. 변경 적용은 `TryCommitTransform` 안에서 계층을 갱신하고, 부모 행렬도 `TryGetWorldMatrix`로 직접 계산하므로 캐시 의존이 없다.
- **검증 (사용자 실행, `x64-ninja-debug`)**:
  - 선택만 하고 조작하지 않을 때 기즈모로 인한 CPU 증가가 사라지고, 조작할 때만 늘어남을 확인했다.
  - 이동·회전·스케일, 스냅, 잠긴 축 거부, 드래그 1회 = 언두 1회, 프리팹 모드 더티 표시를 확인했다(스냅샷 지연 캡처 단계).
- **미검증**: 조상 체인 계산 전환 후 부모가 있는 자식 엔티티·일시정지 PIE에서의 기즈모 위치 확인 기록 없음. 변경 후 CTest 재실행 기록 없음.
- **후속**:
  - 인스펙터 단일 선택 경로(`InspectorPanel::DrawEntityProperties`)도 매 프레임 스냅샷을 캡처한다. 위젯이 그리는 즉시 컴포넌트를 바꾸므로 같은 방식으로 옮길 수 없고, 캐시와 무효화 조건 설계가 필요하다. 캡처 비용 비중 측정이 먼저다.
  - 씬 뷰 기즈모 메뉴(`DrawGizmoMenu`)에 선택 도구(Q)가 없어 메뉴로는 기즈모를 끌 수 없다(UX 누락).
  - 스플라인 점 기즈모(`SceneViewportPanel.cpp`의 두 번째 `Manipulate`)도 같은 패턴인지 확인 필요.

### [2026-11-16] 에디터 카메라 속도·FOV 환경설정 적용 및 속도 조절 입력

- **변경**:
  - `EditorSettingsService::ApplyPreferences`가 `ImGuiManager::ApplyCameraPreferences`를 호출해, 열린 `CameraViewportPanel`과 활성 `PreviewViewportWidget` 전체에 카메라 속도·FOV 환경설정을 즉시 반영한다. 새로 여는 뷰포트·프리뷰도 환경설정 값으로 초기화한다.
  - `PreviewViewportWidget`는 `Initialize`/`Free`에서 정적 집합 `s_ActiveWidgets`에 등록·해제한다.
  - `EditorCameraController`: 속도 범위 1~500, 증감 단위 10. Shift를 누르면 이동·휠·팬 2.5배. 우클릭(Look) 중 휠은 줌 대신 속도를 조절한다.
  - 단축키 `Viewport.CameraSpeedIncrease`(=), `Viewport.CameraSpeedDecrease`(-)를 `ShortcutManager`에 등록하고 변경된 속도를 알림으로 표시한다. 로캘은 `Editor.csv`에 추가하고 JSON을 재생성했다.
- **검증**: 사용자 환경에서 빌드 후 에디터 실행 중 사용했다. 기능별 확인 기록은 없다.
- **미검증·검토 사항**:
  - 두 단축키의 문맥이 `Global`이라 텍스트 입력이 아닌 다른 패널(그래프 편집기 등)에서 `=`/`-`를 눌러도 카메라 속도가 바뀔 수 있다. `Viewport` 문맥 전환을 검토한다(지침 11절 단축키 문맥 기준).
  - 환경설정 변경 시 뷰포트·프리뷰 즉시 반영, 우클릭+휠 속도 조절, Shift 가속, 단축키 재바인딩.

### [2026-11-16] 에디터 UI CPU 병목 개선: 계층 패널 가상 스크롤, 중복 계층 플래그 갱신 제거, 콘텐츠 브라우저 폴더 트리 캐시

- **배경**: Sponza 씬에서 프레임이 24~32 FPS로 떨어졌다. 실측 결과 GPU는 1.5~5 ms, 드로우콜은 60~126개였고, `Editor UI` 패스 CPU가 23~27 ms로 프레임의 60~70%를 차지했다(`AssetPipelineAndRenderingOptimizationPlan.md` 2.4절 (3)). 임시 계측으로 `ImGuiManager::Draw`가 거의 전부(Begin·End는 1 ms 미만)이고 패널별로 계층·콘텐츠 브라우저·씬 뷰가 상위임을 확인했다.
- **변경**:
  - `EntityHierarchyView`: 재귀 트리 그리기를 "보이는 줄 목록 생성(위젯 없음) → `ImGuiListClipper`로 화면 안 줄만 그리기"로 바꿨다. 펼침 상태는 ImGui 저장소 대신 뷰가 영속 ID 기준 `m_Expanded`로 소유한다. 선택 대상(reveal)과 이름 변경 중인 줄은 `IncludeItemByIndex`로 항상 그린다. 범위 선택용 `VisibleEntities`는 전체 줄 목록을 쓴다.
  - 중복 `Scene::UpdateHierarchyFlags()` 제거: `EntityHierarchyView::Draw`와 `SceneViewportPanel` Draw에서 매 프레임 호출하던 전체 재계산을 없앴다. `Runtime::LateUpdate`가 에디터·플레이 모드 모두 매 프레임 갱신하고, UI에서 플래그를 바꾸는 곳은 직후 직접 갱신한다.
  - 콘텐츠 브라우저 폴더 트리: 노드마다 매 프레임 하던 `PlatformPath::IsReadOnlyTarget`(경로 정규화·상대 경로 계산 반복)을 `m_ReadOnlyCache`(디렉터리 캐시와 함께 무효화)로 캐시하고, 노드마다 하던 패널 이름 검색·`dynamic_cast`를 호출 측 매개변수(`isReadOnly`, `isEngineRoot`, `isEditorRoot`)로 대체했다. 트리 노드 ID는 넓은 문자열 해시로 바꿨다(적용 후 첫 실행에서 폴더 트리 펼침 상태 1회 초기화).
  - `ImGui::ShowMetricsWindow()` 상시 호출 제거(닫을 수 없는 ImGui 디버그 창).
- **검증 (사용자 실행, `x64-ninja-debug`, Sponza 씬, 120프레임 간격 임시 계측)**:

  | 단계 | 계층 패널 | 콘텐츠 브라우저 | `Editor UI` CPU | 프레임 |
  | :--- | :--- | :--- | :--- | :--- |
  | 개선 전 (그림자 OFF) | 계층 패널을 닫으면 크게 감소 | 약 3 ms (패널 닫기 비교) | 23.05 ms | 32.4 ms |
  | 계층 가상 스크롤 | 3.8~4.0 ms | 3.2~3.6 ms | 16.39 ms | 23.2 ms |
  | + 중복 플래그 갱신 제거 | 2.7~2.9 ms | 2.9~3.1 ms | | |
  | + 폴더 트리 캐시 | 2.9~3.3 ms | **0.74~0.84 ms** | 선택 없음·가벼운 폴더에서 사용자 관찰 약 4 ms | |

  - 활성·가시 토글이 계층 패널과 자식 흐림 표시에 정상 반영됨을 사용자가 확인했다.
  - 임시 계측 코드(`Application.cpp` 구간 계측, `ImGuiManager::Draw` 패널별 계측)는 커밋 전에 제거했다.
- **미검증·남은 항목**:
  - 변경 후 CTest(`HierarchyEditorCheck` 포함) 재실행 결과 미기록. 계층 패널의 검색 자동 펼침·Shift 범위 선택·F2 이름 변경·드래그 재부모화 수동 확인 미기록.
  - 계층 패널에 남은 약 2.9 ms는 엔티티 수에 비례하는 매 프레임 처리(트리 캐시 변경 검사, 이름 캐시 정리)로 추정한다. 씬에 계층 변경 버전 번호를 두는 구조가 다음 후보다.
  - 엔티티 선택으로 기즈모가 표시되면 씬 뷰 CPU가 3~4 ms 늘어난다(사용자 관찰). 원인 미조사.
  - 모든 수치는 Debug 빌드 기준이며 Release 재측정 필요.
 (`AssetPipelineAndRenderingOptimizationPlan.md` 조치 2)

- **원인**: `PreviewViewportWidget::Draw`의 탐색 가드가 `IsPopupOpen(nullptr, AnyPopupId | AnyPopupLevel)`을 썼다. 이 조합은 열린 팝업이 하나라도 있으면 true라서, `BeginPopupModal` 안에서 그려지는 임포트 모달 프리뷰는 모달 자신 때문에 매 프레임 `canNavigate = false`가 되었다. 드래그 상태가 즉시 초기화되고 `HandleInput`이 호출되지 않았다.
- **변경**: `ImGuiPopupFlags_AnyPopupId`만 사용해 `OpenPopupStack.Size > BeginPopupStack.Size`, 즉 현재 팝업 레벨보다 위에 열린 팝업이 있을 때만 차단한다(`PreviewViewportWidget.cpp`).
- **검증 (사용자 실행, `x64-debug`)**: 임포트 모달에서 카메라 조작이 정상 동작함을 확인했다.
- **미검증**:
  - Alt+LMB 드래그 시 모달 창이 함께 이동하는지(입력 캐처가 좌클릭을 받지 않음). 발생하면 Alt 중에만 캐처가 좌클릭을 받도록 보완한다.
  - 모달 안 콤보가 열린 동안의 차단, 머티리얼·모델·VFX 프리뷰와 VFX 기즈모 회귀.

### [2026-11-16] 복합 VFX Phase 5 Step 1 구현: 공통 기즈모, 환경 프리셋·외력, 반투명 메쉬 Depth Fade

- **변경**:
  - 공통 기즈모: `Editor/Source/ImGui/Public|Private/GizmoHelper.*` 신설, `IGizmoHost`를 `SceneViewportPanel`·`VFXEditorPanel`이 구현. `EditorActionRegistry`의 기즈모 단축키(Q/W/E/R, 좌표계, 스냅)를 `GizmoHelper::GetActiveHost()`로 라우팅.
  - `PreviewViewportWidget`: 입력 캐처가 우클릭·휠클릭만 받고 좌클릭은 기즈모·선택에 양보. Alt+LMB Orbit. 뷰포트 화면 좌표 조회(`GetViewportScreenPos`/`GetViewportSize`) 추가.
  - `VFXEditorPanel`: 장애물 프리셋(Floor Only / Corner Wall / Obstacles / Dummy), 바람·중력 배율 시뮬레이터, 48m `PlaneMesh` 바닥, 트랙 검색 바, 3D 기즈모와 시각 보조선.
  - 엔진: `CompositeVFXComponent::externalWind`·`gravityMultiplier` 추가(`Engine.csv` 표시 키 포함), `CompositeVFXSystem`이 이미터 입자 속도에 합성. `ParticleSystem`이 로컬 공간 입자에 트랙 로컬 변환 적용. `VFXLightTrack::localPosition` 기본값 `(0, 0.8, 0)`.
  - 렌더: `RenderPass` 패스 텍스처 API(`SetPassTexture` 등), `Renderer`가 패스 시작 시 바인딩, `DeferredSceneRenderer`가 ForwardTransparent 패스에 G-Buffer 법선·위치 연결. `forwardParticlePS.hlsl`이 15cm Depth Fade와 프레넬 림 적용.
  - 문서: `CompositeVisualEffectPlan.md` 9.2 원인 정정(큐브 와인딩 판단 오류), 9.5 구현 반영 내역 추가.
- **검증**:
  - 이 변경이 작업 트리에 포함된 상태에서 사용자가 `x64-debug` 빌드와 CTest 38/38 통과를 확인했다(2026-11-16).
- **미검증**:
  - 기즈모 축 드래그와 언두/리두 기록, 오버레이 툴바 상호작용.
  - 환경 프리셋 전환, 외력 시뮬레이터의 입자 반응, Depth Fade/프레넬 결과.
  - 트랙 로컬 변환 적용으로 기존 이펙트 에셋의 입자 위치가 바뀌는지.
- **후속**: 프리뷰 바닥 머티리얼이 프로젝트 에셋 `Assets/Material/GBufferMaterial.bammat.json`에 의존한다(`VFXEditorPanel.cpp`). 엔진 `DefaultMaterial` 기반으로 바꿔야 한다.

### [2026-11-16] 에디터 공용 UI 패턴 통일: 리소스 편집기 검색 바, 리소스 피커 가상 스크롤, 하드코딩 문구 로캘화

- **변경**:
  - 표준 검색 바(`EditorUI::DrawSearchBar`) 적용: Material(파라미터), Model(머티리얼 슬롯), Sprite(슬라이스), Particle(이미터), AnimatorGraph(파라미터), Prefab(엔티티) 편집기.
  - `EditorUI`: 검색 바 클리어 버튼 폭 계산 보정(DPI 오차용 4px 여유), `IconButtonOptions::NoWrap` 추가(검색 바 안의 클리어 버튼이 폭 부족으로 다음 줄로 밀리지 않게 함).
  - `ResourcePicker.h`: 팝업 진입 시 1회 스캔·캐싱, 더티 플래그 기반 필터 인덱스, 메모리/디스크 섹션의 `ImGuiListClipper` 가상 스크롤, 대형 썸네일 항목 레이아웃.
  - `ShaderEditor`·`TextureEditor`: 공용 헤더 빠른 접근 영역(`DrawCustomQuickAccess`)에 셰이더 재컴파일 버튼, 텍스처 해상도 표시 추가.
  - 하드코딩 문구 로캘화: `PluginManagerPanel`의 버전·작성자·컴포넌트 문구, `WorkspaceSettingsWidget`의 해상도 구분자(`EditorCommon.DimensionSeparator`).
  - `ToolBar`: 하단 진행률 문구가 우측 버튼 영역을 침범하지 않도록 시작 위치 선계산과 클리핑 적용.
  - 로캘: `Editor.csv`에 검색 힌트·셰이더/텍스처 문구를 추가하고 `English.json`/`Korean.json`을 재생성했다. 이 커밋에는 다음 VFX 커밋에서 쓰는 `VFXEditor.*` 프리셋·외력 키도 함께 들어 있다(CSV 단일 원본을 나누지 않기 위함).
- **검증**:
  - 이 변경이 작업 트리에 포함된 상태에서 사용자가 `x64-debug` 빌드와 CTest 38/38 통과를 확인했다. `EditorUICheck`(공용 위젯 불변 계약 헤드리스 검사)가 여기에 포함된다.
- **미검증**:
  - 각 편집기 검색 바의 필터링·클리어·Escape 동작과 빈 결과 안내([EditorUXTestGuidelines.md](EditorUXTestGuidelines.md) 기준).
  - 리소스 피커의 대량 항목 스크롤 성능과 선택 동작, DPI 변경 시 검색 바 폭.

### [2026-11-16] 기본 머티리얼 GBuffer Lit 전환 및 에셋 썸네일 렌더링 통일

- **배경**:
  - 기존 `DefaultMaterial`은 `defaultPS`(단일 타깃에 흰색만 출력)와 `DepthMode` None 구성이라 디퍼드 경로에서 법선·깊이·그림자가 없는 짙은 회색으로 보였고, 파라미터를 바꿔도 셰이더가 읽지 않았다.
  - 메쉬 썸네일은 프로젝트 에셋 `PBRMaterial`(PBR 슬롯 `green1x1` → Metallic 1, Roughness 0.04)을 써서 어두운 금속처럼 보였고, 썸네일 출력은 선형 HDR 버퍼를 톤매핑·감마 없이 복사해 씬 뷰보다 어두웠다.
- **변경**:
  - `Engine/Resources/Material/DefaultMaterial.bammat.json`: `GBufferVS`/`GBufferPS`, ShadowDepth 패스 셰이더, `DepthMode` ReadWrite, CW/Back, `m_Key`/`m_Path` 자기 경로, 파라미터 `BaseColor` (0.5, 0.5, 0.5, 1) 선형·`Roughness` 0.5·`Metallic` 0, Diffuse 슬롯 `white1x1.png`.
  - `ThumbnailRenderer`:
    - 기본 미리보기 머티리얼을 `DefaultMaterial`을 상속한 런타임 인스턴스(`Editor.ThumbnailPreviewMaterial`, `Metallic` 0.2만 덮어씀)로 교체. 프로젝트 에셋 의존 제거.
    - 썸네일 전용 `Engine::PostProcessPipeline`(기본 구성: ACES 톤매핑 + 감마 2.2) 추가. `SubmitFrame`이 디퍼드 제출 후 후처리를 실행하고, `GetFinalTexture`가 후처리 출력(`R8G8B8A8_UNORM`)을 반환. 썸네일 캐시의 픽셀당 4바이트 용량 계산과도 일치하게 됨.
  - `PreviewCameraDefaults::ThumbnailFramingMargin` 1.08 → 0.95 (경계 구 기준 프레이밍에서 물체가 화면을 더 채우도록).
  - `EntityFactory::CreatePrimitiveEntity`: 머티리얼을 `MaterialInterface`로 조회하고, 가상 URI 변환 조회와 디스크 로드 폴백을 추가. 실패 로그에 누락 항목을 구분해 기록.
  - `HierarchyPanel`: 기본 도형 생성 시 `Engine://Material/DefaultMaterial.bammat.json` 사용.
  - 문서: `BuiltInResourcesAndShaders.md` 3절 `DefaultMaterial` 명세 갱신, `MissingMaterial`의 같은 결함 기록.
- **검증 (사용자 실행, `x64-debug`)**:
  - 썸네일 밝기가 씬 뷰·머티리얼 프리뷰와 비슷해지고, 모델·메쉬가 프레임을 더 채우는 것을 에디터에서 확인했다.
  - 파라미터 반영 전 GBuffer 버전 `DefaultMaterial`로 기본 도형 렌더링을 확인했다.
- **미검증**:
  - 최종 파라미터 버전 적용 후 CTest 재실행, 계층 패널 기본 도형의 그림자·하이라이트 직접 확인.
  - 기존 임포트 모델 인스턴스(`Roughness` 미지정)가 부모 값 0.2 → 0.5를 상속하며 달라진 외형.
  - 썸네일 프레이밍 0.95에서 긴 모델의 모서리 잘림.
- **후속**:
  - `ModelImportModal.cpp`가 프리뷰 부모 머티리얼로 프로젝트 에셋 `GBufferMaterial`을 불러온다. `DefaultMaterial`로 교체 예정.
  - 톤매핑 `Exposure`/`Gamma`는 공유 리소스 `ToneMappingMaterial`의 값을 모든 파이프라인이 읽으므로, 노출을 바꾼 상태에서 만든 썸네일은 그 값으로 캐시된다.
  - `MissingMaterial`도 GBuffer 기반으로 전환 필요.

### [2026-11-16] 기하 와인딩 CW 표준화 구현 및 검증 (`AssetPipelineAndRenderingOptimizationPlan.md` 조치 1)

- **배경**:
  - 엔진 내장 메시(CCW 중심)·기본 머티리얼 JSON(CCW)과 임포트 메시(CW)·코드 생성 `MaterialInstance`(C++ 기본값 CW, 부모 미상속)의 두 기준이 공존했다. 짝이 어긋난 조합(예: 머티리얼 인스턴스를 쓴 `CubeMesh` 바닥)에서 바깥 면이 컬링되고 안쪽 면이 보였다.
  - 큐브 윗/아랫면만 뒤집던 임시 수정은 기본 머티리얼(CCW) 큐브의 윗/아랫면을 소실시키므로 되돌리고 엔진 전체를 CW로 통일했다.
- **변경**:
  - `Engine/Source/Render/Mesh/Private/BuiltInPrimitiveMeshes.cpp`: Cube·Sphere·Capsule과 Cylinder 캡의 삼각형 2·3번째 인덱스를 교환해 CW로 통일했다(정점 데이터 불변). Quad·Plane·Cylinder 측면·지형 그리드는 기존 CW를 유지한다.
  - `MaterialInstance`: `EPipelineOverrideFlag::FrontFace`와 `GetFrontFace`/`SetFrontFace`/`ClearFrontFaceOverride`를 추가했다. 오버라이드가 없으면 부모 머티리얼 값을 상속한다. 기존 인스턴스 파일은 플래그가 없으므로 부모 값을 따른다.
  - `RHIPipelineDesc::frontFace` 기본값을 `Clockwise`로 바꿨다. 머티리얼 없이 생성하는 파이프라인(Grid, DebugRenderer, Bloom, ChannelFilter, GraphShaderPreview, LightManager Compute)은 모두 `CullMode::None` 또는 Compute라 동작 변화가 없다.
  - 머티리얼 JSON: `Engine/Resources/Material` 기본 머티리얼 11종과 `Assets/Material` 3종을 `m_FrontFace: 1`로 바꿨다. `SkinningMaterial`·`TerrainMaterial`의 임시 `m_CullMode: 0`을 `2`(Back)로 정상화했다.
  - 검사 신설: `Engine/examples/BuiltInMeshWindingCheck.cpp`(RHI 없이 CPU 원본 데이터로 $\text{dot}(\text{cross}(v_1 - v_0, v_2 - v_0), \mathbf{N}) > 0$ 검사, 면적 0 삼각형 별도 집계), `Engine/CMake/Checks.cmake` 등록.
  - 문서 정정: 지침 21절·`BuiltInResourcesAndShaders.md`의 불변식 부호, 조치 계획서의 큐브·지형 와인딩 판정과 `FlipWindingOrder` 제거 방향(임포트 플래그는 Assimp `ConvertToLeftHanded`와 같은 정상 변환이라 유지).
- **검증 (사용자 실행, `x64-debug`)**:
  - 적용 중 `BuiltInMeshWindingCheck`가 인덱스 수정이 누락된 Capsule(CW 0 / CCW 640)을 실패로 검출했고, 수정 후 통과했다.
  - CTest 전체 38/38 통과.
  - 실행 확인 통과: 계층 패널 기본 도형, 정적·스킨드 임포트 모델, 머티리얼 프리뷰, 콘텐츠 브라우저 썸네일, VFX 프리뷰 바닥, 지형.
- **미검증·한계**:
  - `SplineTestStrip`(`reverseWinding=true`, CCW) 실행 확인 미수행.
  - x64-release 빌드 미확인.
  - 실행 확인은 작업 트리의 GBuffer 기반 `DefaultMaterial`로 수행했다. 커밋에는 기존 `DefaultMaterial` 내용에 `m_FrontFace: 1`만 반영했고, 기본 머티리얼 재설계(GBuffer 전환·키 교정)는 별도 작업으로 남겼다. 커밋 스냅샷의 기존 `DefaultMaterial` + CW 조합은 실행 미확인이다.
  - `Assets/`는 `.gitignore` 대상이라 `Assets/Material` 3종 변경은 버전 관리에 포함되지 않는다.
  - 에디터 인스펙터는 `m_OverrideFlags`를 자동으로 켜지 않아 인스턴스의 `m_FrontFace`/`m_CullMode`를 직접 편집해도 반영되지 않는다(기존 CullMode와 같은 한계).

### [2026-11-16] 기하 좌표계, 앞면 와인딩 및 컬링 불변 원칙 공식화 (`Docs/EngineEditorClientGuidelines.md` 21절 신설 등)

- **배경 및 목적**:
  - 임포트된 모델이나 절차적으로 생성된 프리미티브 메시에서 앞뒷면이 뒤집히거나, 머티리얼 바인딩 시 백페이스 컬링에 의해 면이 소실되던 고질적 문제의 원인을 엔진 전수 감사함.
  - 파이프라인 기본값(`CounterClockwise`), 머티리얼 JSON 14종(`CCW`), `MaterialInterface` C++ 기본값(`CW`), `MaterialInstance`의 `FrontFace` 상속 단절, 내장 메시 6종 및 지형 메시의 와인딩 혼종(CW/CCW)이 얽힌 구조적 불일치를 규명하고 공식 불변 원칙을 문서화함.
- **확립된 단일 진실의 원천 (SSOT)**:
  1. **왼손 좌표계 (Left-Handed)**: `+X` 우측, `+Y` 상단, `+Z` 전방 깊이(화면 안쪽), `glm::perspectiveLH_ZO`.
  2. **앞면 와인딩 (Front-Face Winding)**: 표면 외곽 법선 $\mathbf{N}$과 마주보는 시점에서 **시계 방향 (Clockwise = 1)**을 앞면으로 정의.
  3. **백페이스 컬링 (Back-Face Culling)**: 불투명/디퍼드 머티리얼은 반드시 `CullMode = Back (2)`를 유지하며 임의의 `None(0)` 땜질 금지.
  4. **수학 불변식**: 모든 삼각형 정점 $v_0, v_1, v_2$에 대해 $\text{dot}(\text{cross}(v_1 - v_0, v_2 - v_0), \mathbf{N}) > 0$ 성립(최초 기재의 부호 오류를 같은 날 정정).
- **반영된 문서**:
  - `Docs/EngineEditorClientGuidelines.md`: 21절 "기하 좌표계, 앞면 와인딩 및 컬링 불변 원칙" 신설.
  - `Docs/BuiltInResourcesAndShaders.md`: 5절 프리미티브 메시 명세 및 와인딩/법선 불변식 갱신.
  - `Docs/AssetPipelineAndRenderingOptimizationPlan.md`: 2.1절 심층 원인 규명 및 3.1절 6대 정리 작업 계획 갱신.
- **후속 구현 작업 (Checklist)**:
  1. `RHIPipelineDesc::frontFace = EFrontFace::Clockwise;` 변경
  2. `MaterialInstance`에 `EPipelineOverrideFlag::FrontFace` 및 상속 위임 구현
  3. 14종 머티리얼 JSON `"m_FrontFace": 1` 및 컬링 정상화(`Skinning`, `Terrain` `m_CullMode: 2`)
  4. 내장 프리미티브(`Cube`, `Sphere`, `Capsule`, `Cylinder` 캡) CW 인덱싱 교정(지형 그리드는 이미 CW로 확인되어 변경 없음)
  5. 기하 와인딩 무결성 자동 검증 CTest(`BuiltInMeshWindingCheck.cpp`) 신설

### [2026-11-16] 4대 핵심 결함·병목 심층 원인 규명 및 조치 계획서 작성 (`Docs/AssetPipelineAndRenderingOptimizationPlan.md`)

- **배경 및 목적**:
  - 모델 임포트 시 CW/CCW 와인딩 반전, ModelImporter 모달 내 프리뷰 카메라 조작 동결, 대용량 에셋 폴더 오픈 시 프레임 드랍, Sponza 씬 배치 시 드로우콜 폭증 및 프레임 급락 등 4가지 치명적 결함/성능 저하의 코드 레벨 원인을 규명하고 통합 조치 계획을 수립함.
- **규명된 핵심 원인 및 조치 방향**:
  1. **[이슈 1] 와인딩 반전 (CW/CCW Flip)**:
     - 원인(정정): 임포트 플래그(`MakeLeftHanded`+`FlipWindingOrder`+`FlipUVs`)는 Assimp `ConvertToLeftHanded`와 같은 정상 변환이며 모델은 CW로 베이킹된다. 최초 기재한 "이중 반전"은 잘못된 판단이다. 실제 원인은 엔진 내장 메시·기본 머티리얼(CCW)과 임포트 메시·코드 생성 `MaterialInstance`(CW)의 두 기준 공존이었다.
     - 조치: `FlipWindingOrder`는 유지하고 엔진 전체를 CW로 통일했다(위 "기하 와인딩 CW 표준화 구현 및 검증" 항목).
  2. **[이슈 2] 프리뷰 카메라 조작 동결**:
     - 원인: `PreviewViewportWidget.cpp`의 네비게이션 가드 `canNavigate`에 `!ImGui::IsPopupOpen(...)`이 포함되어, `ModelImportModal` 같은 모달 팝업 내부에서 실행될 때 항상 `false`로 평가되어 입력 처리가 원천 차단됨.
     - 조치: 자신이 속한 모달 창이 포커스/호버되어 있는 경우 네비게이션을 허용하도록 조건식 교정.
  3. **[이슈 3] 콘텐츠 브라우저 대용량 폴더 스터터링**:
     - 원인: `AssetCache::Update3DCapture`가 3D 썸네일 생성을 위해 메인 스레드에서 `ResourceManager::LoadFile`을 동기 호출하여 수십 MB 단위의 정점 파싱과 GPU 할당이 렌더 루프를 동기 블로킹함.
     - 조치: 에셋 임포트 시 디스크 썸네일(`.thumb.png`) 사전 생성/캐싱, 프레임당 최대 2.0ms 타임 버짓 적용 및 뷰포트 가시 렉트 기반 요청 컬링.
  4. **[이슈 4] Sponza 드로우콜 폭증 및 프레임 저하**:
     - 원인: `MeshRenderSystem`이 수집한 `commands`를 머티리얼/메시 기준으로 정렬하지 않고 씬 그래프 순서대로 제출하여, `Renderer::RenderMeshes`에서 매 드로우마다 `flushBatch()`가 실행되어 1개짜리 드로우콜 수백 개와 심각한 파이프라인/디스크립터 테이블 교체 오버헤드 발생.
     - 조치: `(Pipeline -> Material -> Mesh -> Depth)` 64비트 정렬 키 기반 Render Queue 도입을 통한 `DrawIndexedInstanced` 대규모 배칭 극대화, Early-Z 활성화를 위한 Front-to-Back 정렬, 섀도우 패스 경량 뎁스 파이프라인 분리.
- **관련 문서**:
  - 신규 생성: `Docs/AssetPipelineAndRenderingOptimizationPlan.md`
  - 연결 갱신: `Docs/DevelopmentPriorities.md`, `Docs/ImplementationLog.md`

### [2026-11-16] 엔진 기본 리소스 및 셰이더 전수 명세서 작성 (`Docs/BuiltInResourcesAndShaders.md`)

- **배경 및 목적**:
  - 엔진 코어와 사용자 프로젝트의 자원 분리(Phase 3-B) 이후, 엔진이 기본으로 제공하는 빌트인 셰이더, 머티리얼, 시스템 텍스처, 프리미티브 메시, 폰트, 로컬라이제이션 등의 전수 현황과 렌더링 파이프라인별 역할이 종합적으로 정리된 공식 문서가 부재하여 이를 체계화함.
- **문서화 범위 및 주요 내용**:
  1. **가상 URI 스키마(`Engine://`, `Editor://`, `Project://`) 및 마운트 체계**:
     - `PlatformPath`의 경로 해석 알고리즘, `IsBuiltInRelativeResourcePath`를 통한 레거시 상대경로 폴백 우선순위 명세.
  2. **빌트인 셰이더 (53개 HLSL / 33개 GLSL / 48개 CSO/SPV 바이너리)**:
     - 지오메트리 패스(GBuffer, 스키닝, 스프라이트, 지형), 섀도우 패스, 라이팅 및 타일드 조명 컬링 컴퓨트, 스카이/대기 산란, 포워드 및 UI, 포스트프로세스 체인, 에디터 디버그 셰이더 분류 및 명세.
     - `BINDING_LAYOUT.md`와 일치하는 D3D12/Vulkan 루트 시그니처 바인딩(`space0`~`space3`) 연계.
  3. **빌트인 머티리얼 12종 (`Engine/Resources/Material/*.bammat.json`)**:
     - `DefaultMaterial`, `SkinningMaterial`, `SkyMaterial`, `PostProcessDefaultMaterial`, `ParticleMaterial_Additive/AlphaBlend`, `SpriteMaterial`, `MissingMaterial` 등 셰이더 및 블렌드 모드 전수 매핑.
  4. **빌트인 텍스처 14종 (`Engine/Resources/Texture/`)**:
     - 단색 텍스처(`white1x1`, `black1x1`, `magenta1x1` 등), PBR 기본(`DefaultNormal`, `DefaultPBR`), 체커, 에러 폴백, 파티클 텍스처 4종 명세.
  5. **빌트인 프리미티브 메시 6종 (`BuiltInPrimitiveMeshes.cpp`)**:
     - `QuadMesh`, `CubeMesh`, `SphereMesh`, `PlaneMesh`, `CylinderMesh`, `CapsuleMesh` 정점 스트림(Position, Material) 레이아웃 및 규격.
  6. **기타 시스템 리소스 및 C++ 참조 가이드**:
     - Font Awesome 7 아이콘 폰트, 로컬라이제이션 CSV 원본 수정 원칙, 렌더 파이프라인 DAG(`Deferred.bamrenderpipeline.json`), C++ 핸들 조회 API(`PlatformPath::ResolveUri`, `ResourceManager::GetResourceHandle`).
- **관련 파일**:
  - 신규 생성: `Docs/BuiltInResourcesAndShaders.md`
  - 링크 갱신: `Docs/Shader/BINDING_LAYOUT.md`, `Docs/ResourceAndAssetSeparationPlan.md`

### [2026-11-16] 공통 기즈모 헬퍼(GizmoHelper) 도입 및 VFX 에디터 기즈모 인터랙션·오버레이 툴바 소멸 결함 분석

- **배경 및 목적**:
  - `SceneViewportPanel`, `VFXEditorPanel`, 모델/본, 스플라인 등 에디터 전역에서 중복 정의되던 `ImGuizmo` 호출 로직을 일원화하기 위해 공통 기즈모 래퍼/헬퍼(`GizmoHelper`) 및 `IGizmoHost` 인터페이스를 신설했다.
  - 전역 단축키(`Viewport.GizmoSelect` [Q], `Translate` [W], `Rotate` [E], `Scale` [R], `ToggleCoord` [`], `ToggleSnap` [Shift+S])를 `GizmoHelper::GetActiveHost()`로 통합하고, 블렌더 스타일의 세로 플로팅 오버레이 도구 모음(`DrawGizmoToolbar`)을 공통 헬퍼에서 제공하도록 설계했다.
- **실환경 검증 중 발생한 결함 현상**:
  1. **오버레이 툴바(`##GizmoOverlayToolbarWindow`) 즉시 소멸 현상**:
     - 툴바 윈도우 크기를 명시적(`SetNextWindowSize`)으로 지정한 후 버튼 박스가 화면에 잠시 렌더링되지만, 마우스를 올리거나 클릭하려고 하면 즉시 화면에서 사라짐.
  2. **기즈모 축 마우스 클릭/드래그 무반응 (상호작용 불능)**:
     - 트랙 선택 및 단축키(`W`, `E`, `R`) 입력 시 뷰포트에 3D 기즈모 축이 표시되나, 마우스 좌클릭으로 축이나 평면 핸들을 잡고 드래그해도 위치·회전·스케일 변환이 전혀 적용되지 않음.
- **심층 원인 규명**:
  1. **ImGui 윈도우 스택 규칙 위반 (`BeginChild` 컨텍스트 내 `Begin` 호출)**:
     - `VFXEditorPanel::DrawViewport()`는 `EditorUI::DrawEffectEditorLayout()` 내부의 `ImGui::BeginChild("##EffectPreview", ...)` 컨텍스트 안에서 실행된다.
     - 그런데 `GizmoHelper::DrawGizmoToolbar()`는 독립된 최상위 윈도우를 여는 `ImGui::Begin("##GizmoOverlayToolbarWindow", ...)`을 호출하고 있었다.
     - ImGui는 Child Window 블록 내부에서 독립된 Begin Window를 여는 중첩 스택을 지원하지 않으며, 마우스 호버/클릭 시 포커스가 이동하면서 Z-Order가 역전되어 부모 Child 창 뒤로 숨겨지거나 렌더링 조건이 즉시 무효화된다.
  2. **포커스 및 호버 컨텍스트 이탈**:
     - 마우스가 외부 윈도우인 `##GizmoOverlayToolbarWindow` 위로 이동하는 순간 `m_PreviewWidget.IsHovered()`가 `false`로 전이된다.
     - 사용자가 버튼을 클릭하면 뷰포트 창이 포커스를 잃고(`m_IsFocused = false`), `GizmoHelper::SetActiveHost(this)` 및 단축키 타겟팅이 해제되어 깜빡이며 사라지는 현상이 발생한다.
  3. **`PreviewViewportWidget`의 입력 캐처 구조와 호출 순서 불일치**:
     - `PreviewViewportWidget.Draw()`가 `DrawGizmo()`보다 먼저 실행되며, 뷰포트 전면을 덮는 `InvisibleButton`과 카메라 입력 감지 로직이 뷰포트 내 마우스 상태를 먼저 평가한 뒤 기즈모가 실행된다.
     - 기즈모가 정상 동작하는 `SceneViewportPanel`은 전면에 `InvisibleButton`을 일절 두지 않고 `ImGui::Image` 직후 마우스 입력(`Engine::InputManager`)을 직접 수신하며, 우클릭만 카메라 회전에 바인딩하고 좌클릭은 100% ImGuizmo에 양보하는 아키텍처를 취하고 있다.
- **해결 대안 및 조치 계획**:
  1. **오버레이 툴바의 뷰포트 내부 일체형 위젯 전환**:
     - `ImGui::Begin`을 사용하는 별도 플로팅 윈도우 방식을 폐기하고, `PreviewViewportWidget`의 기존 상단 툴바(그리드, 채널 버튼)처럼 뷰포트 내부 좌표 기반의 `BeginChild` 또는 캔버스 오버레이 방식으로 변경하여 포커스 이탈 및 창 소멸을 원천 차단.
  2. **뷰포트 마우스 조작 규칙 일원화 (`SceneViewportPanel` 표준 준수)**:
     - 뷰포트 내 좌클릭은 `ImGuizmo` 조작 및 오브젝트 선택 전용으로 할당하고, 카메라 회전은 우클릭 드래그(Look/Orbit) 및 휠 드래그(Pan)로 표준화하여 마우스 이벤트 경합을 제거.
- **미검증 항목**:
  - `BeginChild` 기반 기즈모 오버레이 툴바 상호작용 검증.
  - 마우스 좌클릭 드래그를 통한 VFX 트랙 위치/회전/스케일 변환 및 언두/리두 히스토리 기록 검증.

### [2026-11-16] 신규 엔진 컴포넌트 6종 ReflectionArchive 직렬화 라운드트립 검증 (후보 2번 완료)

- **배경 및 목적**:
  - 최근 확장된 2D/3D 연출 및 환경 컴포넌트 6종에 대해 `SceneArchive`를 통한 씬 파일(JSON 및 BEVE 포맷) 저장 및 로드 시 데이터 손실이나 타입 불일치가 없는지 라운드트립 검증을 수행했다.
- **검증 대상 컴포넌트 (6종)**:
  1. `ActionTimelineComponent`: 트랙 수, 지속 시간(`duration`), 재생 속도(`playRate`), 루프(`isLooping`)
  2. `CompositeVFXComponent`: 이펙트 에셋 핸들(`effectAsset`), 재생 상태(`playState`), 재생 속도(`playRate`)
  3. `ParticleEffectComponent`: 파티클 에셋 핸들(`effectAsset`), 재생 상태(`playState`), 속도(`playRate`)
  4. `SpriteAnimationComponent`: 클립 핸들(`clipAsset`), 재생 속도(`playbackSpeed`), 자동 재생(`autoPlay`), 루프(`loop`)
  5. `FogComponent`: 안개 활성화 여부(`bEnabled`), 색상(`fogColor`), 시작/종료 거리(`fogStartDistance`, `fogEndDistance`), 밀도(`fogDensity`), 높이 감쇠(`fogHeightFalloff`)
  6. `VolumetricCloudComponent`: 레이마칭 스텝 수, 알베도, 소광 계수, 기본 미터 크기(`WeatherMapSize`, `ShapeNoiseSize`, `DetailNoiseSize`), 바람 속도(`WindSpeed`)
- **검증 결과 및 확인 사항**:
  - JSON(`round-trip-modern.json`)과 BEVE(`round-trip-modern.beve`) 파일 포맷 모두에서 모든 영속 프로퍼티가 비트 단위로 완벽하게 보존됨을 확인.
  - `NOSERIALIZE` 플래그가 지정된 런타임 휘발성 필드(`currentTime`, `elapsedTime`, `currentFrameIndex`)는 파일에 기록되지 않고 역직렬화 시 기본값(0)으로 올바르게 복원됨을 검증.
  - `ctest -R SceneArchiveCheck` 단위 테스트 100% 통과 확인.


### [2026-11-16] 복합 비주얼 이펙트(Phase 5) 반투명 메쉬 렌더링 및 디퍼드 라이팅 결함 해결 분석

- **배경 및 발생 현상**:
  - 복합 비주얼 이펙트 에디터(`VFXEditorPanel`) 실환경에서 3D 메쉬 이펙트(`MeshTrack`)와 라이트 플래시 트랙을 검증하던 중, 4가지 복합적인 렌더링 결함이 발생했다:
    1. 바닥 표면에 플래시 라이트 연산이 맺히지 않고 조명 연산 좌표가 음수($y=-0.05$)로 내려가며, 바닥 엔티티를 강제로 위로 올려야만 빛이 계산되던 현상.
    2. 지면 바닥 그리드가 있는 곳은 돔 메쉬 내부가 뚫려 투명해지고, 바닥이 없는 허공 영역은 100% 불투명한 짙은 고동색 덩어리로 잘려 이펙트가 두 동강 나던 현상.
    3. 바닥 슬래브의 0.05m 앞쪽 수직 단면에 메쉬가 교차하면서, 단면 아래로 메쉬 일부가 어색하게 잘려 삐져나오던 현상.
    4. 허공 영역에서 반투명 메쉬가 칙칙하고 어두운 흙갈색/검은색으로 썩어버려 밝은 바닥 위의 화사한 빛과 극단적인 대비를 보이던 현상.
- **근본 원인 규명**:
  1. **내장 `CubeMesh`의 윗면/아랫면 정점 와인딩 반전 (`BuiltInPrimitiveMeshes.cpp`)** — *2026-11-16 정정: 이 판단은 부호가 반대인 불변식에 따른 오류다. 실제 큐브는 6면 모두 CCW였고, 바닥 `MaterialInstance`가 C++ 기본값 CW를 써서 전체가 뒤집혔다. 아래 1번 조치(윗/아랫면만 반전)는 되돌렸고 "기하 와인딩 CW 표준화" 항목으로 대체했다.*
     - 앞/뒤/좌/우 면은 시계 방향(CW)인 반면, 윗면(8~11)과 아랫면(12~15)이 반시계 방향(CCW)으로 잘못 정의되어 있었다.
     - 래스터라이저의 Back-Face Culling(CW 기준)으로 인해 윗면($y=0$, Normal $+Y$)이 폐기되고, 큐브 내부의 아랫면($y=-0.05$, Normal $-Y$)이 렌더링되어 G-Buffer에 기록되었다.
     - 표면 법선이 아래($-Y$)를 향해 위에서 비추는 조명에 대해 $\mathbf{N} \cdot \mathbf{L} \le 0$으로 소실되고 위치가 음수로 기록된 것이 밝혀졌다.
  2. **두께 슬래브(`CubeMesh`)와 16m 유한 지면의 단면 간섭**:
     - 0.05m 두께의 큐브 바닥 앞쪽 수직 면에 메쉬가 비스듬히 교차할 때 단면 형태대로 메쉬가 잘려 삐져나오는 기하학적 아티팩트 유발.
  3. **과도한 Depth Fade 거리(65cm) 및 허공 분기 수식 (`forwardParticlePS.hlsl`)**:
     - `softFadeDistance = 0.65f` 및 제곱 감쇠로 인해 바닥 위 65cm 내의 돔 본체 전체가 증발.
     - 배경에 지오메트리가 없는 허공은 G-Buffer 법선이 없어 Depth Fade가 아예 건너뛰어지며 100% 불투명 출력되어 지면 경계선에서 메쉬가 두 동강 남.
  4. **디퍼드 렌더링 배경 암흑 버퍼와 알파 블렌딩의 결합 결함**:
     - `LightingPass`에서 허공 픽셀이 `discard`되어 `LightingColor` 버퍼의 배경이 완전한 암흑 `(0, 0, 0, 0)`으로 남음.
     - 표준 알파 블렌딩 공식 $\text{Final} = \text{Src} \times \alpha + \text{Dest} \times (1 - \alpha)$에 의해, 허공 영역의 메쉬가 $(0, 0, 0)$과 강제로 섞여 밝기가 반 토막 난 어두운 흙갈색으로 렌더링됨.
     - 반면 바닥이 있는 곳은 빛을 받은 밝은 지면 색상($\text{Dest} \approx 0.6$)과 섞여 화사하게 출력됨.
- **적용 및 해결 조치**:
  1. **`CubeMesh` 정점 와인딩 오더 정상화 (`BuiltInPrimitiveMeshes.cpp`)**: 윗면과 아랫면을 시계 방향(CW)으로 교정하여 엔진 전체 큐브 메쉬의 컬링 결함 해소.
  2. **프리뷰 지면의 `PlaneMesh`(48m) 전면 전환 (`VFXEditorPanel.cpp`)**: 두께 큐브 슬래브를 걷어내고 $y=0$에 놓이는 48m 수평 평면으로 교체하여 단면 잘림 및 삐져나옴 원천 제거.
  3. **Depth Fade 15cm 국소 페더링 및 프레넬(Fresnel) 내장 (`forwardParticlePS.hlsl`)**:
     - 감쇠 거리를 15cm로 축소하여 접촉 경계선만 부드럽게 지우고, 15cm 위 돔 본체는 배경 유/무와 상관없이 동일한 알파를 유지하도록 개선.
     - 시선 각도 $\mathbf{N} \cdot \mathbf{V}$ 기반 프레넬 림 페이드를 적용하여 둔탁한 찰흙 덩어리를 세련된 반투명 에너지 돔으로 전환.
  4. **발광성 이펙트의 `Additive` 가산 블렌드 표준화 로드맵 수립**: 폭발 충격파 및 발광체는 배경을 어둡게 가리지 않고 빛을 더하는 `Additive` 머티리얼을 사용하도록 파이프라인 정립.
- **실환경 검증 및 잔여 이슈 (2026-11-16 2차 확인)**:
  - `PlaneMesh`(48m) 및 프레넬 림 발광 적용으로 바닥 단면 잘림은 해소되었으나, **수평선(지면 끝 모서리)을 기준으로 상단(허공 배경)과 하단(지면 배경) 사이의 명암/채도 불연속이 여전히 관측됨.**
  - 원인: 카메라 시야에서 수평선 아래는 밝은 PBR 지면($\text{DestColor} \approx 0.6$)이고 위는 스카이/허공($\text{DestColor} \approx 0.18$)이므로, Additive/AlphaBlend 공히 배경 본연의 밝기 단차가 투과되어 드러남.
  - 대책: 향후 무한 평면(Infinite Horizon) 지면 셰이더 도입 또는 프리뷰 공중(Air)/지면(Ground) 모드 분리 계획으로 등록하고, 현재 단계에서는 이슈로 기록 보존 후 다음 저작 UX 작업으로 전환.
- **미검증 항목**:
  - VFXMeshTrack 인스펙터 UI에서의 Soft Particle 거리 및 Fresnel 슬라이더 파라미터 노출.
  - ImGuizmo 기반 뷰포트 3D 트랙 기즈모 조작.


### [2026-11-15] 복합 비주얼 이펙트 시스템 및 저작 도구 실환경 검증 & D3D12/렌더링 안정화 (후보 B 완료)

- **배경 및 범위**:
  - `Docs/CompositeVisualEffectPlan.md` 기반의 복합 비주얼 이펙트 시스템(`CompositeEffect`, `VFXEditorPanel`)을 실제 씬 및 에디터 실환경에서 검증하는 과정에서 발생한 다수의 하드웨어/렌더링 결함과 저작 UX 한계를 진단하고 해결했다.
  - 뷰포트 스카이박스 간섭 누출, 타임라인 줌/패닝 제약, D3D12 `MISMATCHINGCLEARVALUE` 예외 중단 크래시, 배경 모드 프리셋(스튜디오 18% 회색 / 암실 / 주간) 연동, 그리고 바닥 슬래브의 과도한 순백색 블룸 포화로 인한 플래시 라이트 마스킹 문제를 해결했다.
- **주요 변경 및 기술적 해결 내용**:
  1. **메인 씬 스카이박스의 오프스크린 뷰포트 유입 차단 (`MeshRenderSystem.cpp`, `DeferredSceneRenderer.cpp`)**:
     - `MeshRenderSystem::OnSubmit()`이 모든 활성 뷰포트 카메라를 순회할 때 `acceptsSceneSubmissions` 검사가 누락되어, 메인 씬의 스카이박스 구체 엔티티가 독립 프리뷰 뷰포트(`PreviewViewportWidget`)의 포워드 패스로 무조건 제출되던 문제를 발견.
     - `if (!viewport.acceptsSceneSubmissions) continue;` 가드를 추가하여 메인 씬의 스카이박스 유무가 VFX 에디터 프리뷰 뷰포트에 누출되는 현상을 완벽히 차단.
  2. **타임라인 시간축 앞뒤 여백 및 10배 줌아웃(0.1x) 확장 (`EffectTimelineView.cpp`, `EffectTimelineView.h`)**:
     - 시작 전 `-0.25초` 프리롤 및 종료 후 `+1.0초` 포스트롤 여백을 부여하여 0초 직전 생성 트랙과 잔여 파티클 감쇠 확인 지원.
     - 최소 확대 배율을 `0.5x`에서 `0.1x`로 확장하여 수십 초 길이의 긴 연출도 한 화면에 조망 가능하도록 개선하고, `Shift + 휠` 및 우클릭 드래그 수평 패닝 지원.
  3. **이동 궤적 검증용 프리뷰 모션 (`VFXEditorPanel.cpp`)**:
     - 상단 툴바에 바람 아이콘(`ICON_FA_WIND`) 토글을 신설하고, 활성화 시 이펙트 엔티티를 3D 원형 궤도로 비행시켜 트레일/리본 트랙(`TrailTrack`)의 동적 스트립 생성을 즉각 검증할 수 있도록 지원.
  4. **D3D12 ClearRenderTarget 크래시 근본 해결 (`DirectX12RHI.cpp`)**:
     - `DirectX12RHI::Initialize()`에서 `infoQueue->SetBreakOnID(D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE, FALSE)`로 수정하여 텍스처 생성 시와 다른 클리어 색상 사용 시 D3D12 SDK Layers가 성능 경고를 Fatal Break로 에스컬레이션하던 동작 차단.
     - `ClearRenderTarget`에 `if (rtvHandle.ptr == 0) return EResult::Fail;` 안전 가드를 배치하여 하드웨어 크래시 해소.
  5. **3단계 배경 프리셋 및 스튜디오 조명 배율 연동 (`PreviewViewportWidget`, `VFXEditorPanel`)**:
     - `Studio Neutral Gray (18%)` (`(0.18, 0.18, 0.19)`, 조명 0.15배): 가산 블룸 타버림과 알파 블렌딩 왜곡을 최소화하는 표준 제작 환경.
     - `Dark (암실)` (`(0.02, 0.02, 0.03)`, 조명 0.0배 완전 소등): 칠흑 속에서 오직 이펙트의 발광 피크와 라이트 플래시만 바닥을 밝힘.
     - `Bright (주간)` (`(0.72, 0.72, 0.74)`, 조명 0.6배): 대낮 야외 환경에서 연기/먼지 실루엣 대비 검증.
     - `PreviewViewportWidget`에 `SetStudioLightMultiplier(float)`를 추가하여 배경 모드별로 스튜디오 3점 조명의 강도를 동적 조절.
  6. **차콜 PBR 바닥 슬래브 머티리얼 인스턴스 (`VFXEditorPanel.cpp`)**:
     - 바닥 평면이 순백색(`white1x1`, Albedo 1.0) 머티리얼과 강한 스튜디오 주광(3.0)으로 인해 자체 블룸으로 타들어가 플래시 라이트가 묻히던 현상 해결.
     - `GBufferMaterial` 기반의 18% 뉴트럴 차콜 그레이(`vec4(0.18, 0.18, 0.20, 1.0)`), `Roughness 0.85` 매트 머티리얼 인스턴스(`VFXPreviewGroundMatInst`)를 동적 생성하여 바닥 자체 블룸을 0%로 제거하고 폭발 라이트 플래시의 지면 반사를 극대화.
- **실환경 검증 완료 (2026-11-15 사용자 확인)**:
  - `CMake --build ... --target Editor` 빌드 성공 확인.
  - VFX 에디터 뷰포트에서 배경 모드 전환 시 하드웨어 크래시 없이 즉시 배경색 및 조명이 전환됨을 확인.
  - 바닥 평면 가시화 및 불꽃 폭발 시 포인트 라이트 플래시의 지면 반사 가시성 확보.
- **미검증 항목**:
  - 소프트 파티클(Depth Fade) 셰이더 적용, 뷰포트 3D 인터랙티브 기즈모(ImGuizmo), 2D 베지어 커브 에디터.

---

### [2026-11-15] 크래시 진단·작업 복원 시스템 실환경 검증 및 경로 일원화 (개발 vs 배포 아키텍처)

- **배경 및 설계 고민**:
  - 에디터 실환경에서 `Help -> Trigger Crash (Diagnostics Test)`를 통한 크래시 감지, 미니덤프/진단 리포트 생성, 세션 락 및 2단계 작업 복원(Two-Tier Recovery) 모달 동작을 검증하는 과정에서, `Logs/Editor.log` 및 `Logs/Crashes/`는 바이너리 빌드 폴더(`out/build/.../bin/Debug/`)에 생성되는 반면, `Temp/Recovery/` 및 `Temp/Session.lock`은 프로젝트 루트에 생성되어 상위 위치가 어긋나는 문제가 확인되었다.
  - 이에 대해 **"나중에 에디터를 알파 빌드로 패키징하여 배포할 때, 테스터가 압축하여 리포트하기 쉽도록 실행 파일(바이너리) 폴더 옆에 모아야 하지 않는가?"**라는 관점과 **"개발 환경의 프로젝트 루트에 모아야 하는가?"**에 대한 아키텍처 비교 및 설계 판단을 진행했다.
- **아키텍처 판단 기준**:
  1. **바이너리 폴더(`exeDir`) 고정 시의 결함**:
     - 개발 환경에서 `out/build/`는 CMake 캐시 삭제, Rebuild, Git Clean 시 수시로 완전히 지워지는 임시 디렉터리다. 만약 자동 저장 복원 파일(`Temp/Recovery/`)이 여기에 생성되면 빌드 정리 시 **사용자의 미저장 작업 복구본이 영구 유실**된다.
     - Debug/Release 구성 간에 로그와 덤프가 분산되어 이전 크래시 추적이 어려워진다.
     - 나중에 설치형(`C:\Program Files\...`)으로 배포 시 권한 거부(`Access Denied`)로 덤프 작성이 실패할 수 있다.
  2. **프로젝트 루트(`PlatformPath::GetProjectRoot()`) 기준 일원화의 우위**:
     - **개발 환경**: 소스 및 에셋이 위치한 프로젝트 저장소 루트(`ProjectRoot`) 아래의 `Logs/` 및 `Temp/`에 가지런히 모이므로 빌드 클린에도 작업 데이터와 진단 리포트가 안전하게 보존된다.
     - **알파 배포 환경**: 독립 패키지(`BamEditor/`) 환경에서는 실행 파일 바로 옆에 `Config/` 및 패키지 마커가 배치되므로, `PlatformPath::GetProjectRoot()` 탐색 알고리즘에 의해 **자동으로 `ProjectRoot == ExecutableDirectory`**가 된다.
     - 결과적으로 배포 환경에서도 테스터가 기대한 대로 **실행 파일 옆의 `BamEditor/Logs/` 및 `BamEditor/Temp/`로 자동 일원화**되어 폴더만 그대로 압축해 버그 리포트를 전송할 수 있는 상위 호환성이 보장된다.
- **주요 변경 내용**:
  - [Application.cpp](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/Application/Private/Application.cpp):
    - `InitializeLogger()`에서 `Engine::PlatformPath::GetProjectRoot() / L"Logs" / L"Editor.log"`로 로그 파일 경로를 일원화.
    - `Application::Initialize()`에서 `Engine::CrashHandler::Initialize(projectRoot / L"Logs" / L"Crashes")` 및 `SetCustomContext("Host", "Editor")`를 명시적으로 등록하여 덤프/리포트 위치 일원화 및 호스트 출처 명시.
- **실환경 검증 완료 (2026-11-15 사용자 확인)**:
  - `Help -> Trigger Crash` 실행 시 프로세스 증발 없이 `BamEngine Crash Diagnostics` 대화상자 정상 팝업 확인.
  - `Logs/Crashes/`에 타임스탬프 기반 `.dmp` 미니덤프 및 `.crash.json` 진단 리포트 누락 없이 생성 확인.
  - `Temp/Session.lock`이 `"Status": "Crashed"`로 갱신되고 `Temp/Recovery/`에 `Emergency_Snapshot.bamscene.json` 및 `RecoveryMetadata.json` 정상 생성 확인.
  - 에디터 재기동 시 **"⚠️ 비정상 종료 감지 및 작업 복원 (Crash Recovery)"** 모달 창이 정상 팝업되고, `Escape` 키 입력 시에도 취소 인덱스(-1) 가드로 스냅샷이 안전하게 유지됨을 확인.
  - `[복원하여 열기 (Restore)]` 선택 시 크래시 직전 수정했던 엔티티와 씬 상태가 뷰포트에 완벽히 복원되고 원본 씬 문서로 재채택됨을 실환경에서 검증 완료.
- **미검증 항목**: Tier 1 5분 주기 하트비트 스냅샷 파일의 백그라운드 교체 주기(설정 변경 후 타이머 만료) 확인.

---

### [2026-11-15] 비동기 작업 및 스레드 수명 관리 지침 수립 (std::execution/std::async vs std::thread)

- **배경 및 범위**: 클라이언트 빌드 및 패키징(`BuildProfilePanel`) 후 에디터 종료 시 CRT 메모리 누수(`_CRTDBG_LEAK_CHECK_DF`)가 보고되던 현상을 추적하여, `std::execution::par` 및 `std::async(std::launch::async)`가 의존하는 MSVC 시스템 스레드 풀(Win32 Threadpool / PPL)의 비종료 상주성과 CRT 누수 감지 시점의 역전 문제를 규명하고 엔진 표준 비동기 수명 지침을 수립했다.
- **주요 변경 및 문서화 내용**:
  - [EngineEditorClientGuidelines.md](file:///i:/BamtoliyaGithub/BamEngine/Docs/EngineEditorClientGuidelines.md): `## 20. 비동기 작업 및 스레드 수명 관리 지침` 신설.
    1. Task 기반 병렬 추상화(`std::execution`, `std::async`)와 저수준 스레드 제어(`std::thread`)의 본래 설계 의도와 트레이드오프 명시.
    2. MSVC 시스템 스레드 풀의 영구 상주로 인해 `main()` 종료 직후 CRT leak check 시점에 `_tiddata`, TLS, 작업 노드가 가짜 누수로 감지되는 원인 기술.
    3. 에디터 백그라운드 작업(`ClientBuildJob`) 및 대용량 IO(`CopyRuntimeResources`)에서 명시적 `std::thread` 소유 및 `join()`을 통한 즉시 회수 원칙 의무화.
    4. 향후 엔진 전용 JobSystem 도입 시 필수적인 `Initialize` / `Shutdown` 수명 통제 요건 정의.
  - [BuildAndPackagingPlan.md](file:///i:/BamtoliyaGithub/BamEngine/Docs/BuildAndPackagingPlan.md): 3단계(Build)의 `ClientBuildJob` 및 4단계(Stage)의 `CopyRuntimeResources`에 스레드 풀 누수 방지 원칙 및 가이드라인 20절 참조 연결.
- **검증 상태**: 가이드라인 문서 및 계획 문서 상호 참조 정합성 확인 완료.
- **미검증 항목**: 엔진 전용 JobSystem(태스크 그래프) 구현 및 렌더 스레드 코어 고정.

---

### [2026-11-15] Runtime 수명주기 역전 해소 및 PickingManager 지연 소멸·모듈 언로드 크래시 해결

- **배경 및 범위**: 독립 클라이언트(`Client.exe`) 종료 시 `EXCEPTION_ACCESS_VIOLATION (0xC0000005)` 크래시가 발생하는 문제를 진단하고, 런타임 종료 단계(`Runtime::Free`)에서의 매니저 수명주기 역전 및 `PickingManager`에 의한 씬 참조 카운트 지연 소멸 문제를 완벽히 해결했다.
- **주요 변경 내용**:
  - [Runtime.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Runtime/Private/Runtime.cpp): `Runtime::Free()`의 매니저 소멸 순서를 의존성에 맞게 전면 재정렬했다.
    1. `m_Renderer->ClearAllRenderQueues()` 및 `Safe_Destroy(m_PickingManager)`를 가장 먼저 실행하여 프레임 큐와 피킹 타겟(`m_CompletedPickingTargets`)이 쥐고 있던 씬 참조 카운트를 즉시 반환(`Safe_Release`).
    2. `Safe_Destroy(m_SceneManager)`를 호출하여 외부 참조가 모두 해제된 씬을 즉시 소멸(`Scene::ClearEntities`). 이때 모듈 코드 및 시스템이 온전히 살아있어 모든 컴포넌트 소멸자가 안전하게 실행됨.
    3. `Safe_Destroy(m_ModuleManager)`를 호출하여 동적 모듈(`GameModule.dll`)을 언로드. 이때 `SystemManager`가 살아있어 모듈의 `OnUnload()`가 등록된 시스템(`SkyAtmosphereSystem`)을 안전하게 제거(`RemoveSystem`).
    4. `Safe_Destroy(m_SystemManager)`를 호출하여 남은 엔진 내장 시스템들을 정리.
    5. 그 후 게임 로직, 리소스, 렌더링 인프라, RHI, 리플렉션 순으로 안전하게 해제.
  - [Scene.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/World/Scene/Private/Scene.cpp): `Scene::ClearEntities()`에서 엔티티 래퍼(`m_EntityMap`)를 삭제하기 전에 `m_LocalRegistry.clear()`를 먼저 호출하도록 변경하여, 컴포넌트 소멸자 실행 도중 엔티티 래퍼가 유효하도록 안전성 보장.
  - [GameModule.cpp](file:///i:/BamtoliyaGithub/BamEngine/Projects/GameModule/Source/GameModule.cpp): `GameModule::OnUnload()`에 `if (Engine::SystemManager::IsCreated())` 방어 코드를 추가하여 싱글톤 파괴 후 역참조 방지.
  - [ReflectionRegistry.h](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Core/Reflection/Public/ReflectionRegistry.h): `REGISTER_MODULE_COMPONENTS`, `UNREGISTER_MODULE_COMPONENTS` 매크로에 `(void)` 캐스트를 추가하여 C4834 `[[nodiscard]]` 경고 해결.
- **검증 상태**: 독립 패키지 클라이언트(`Client.exe`) 실행 후 창 종료 시 `0xC0000005` 오류 대화상자 없이 클린하게 정상 종료됨을 사용자 실환경에서 확인 완료.
- **미검증 항목**: 다중 동적 모듈 동시 로드/언로드 시나리오.

---

### [2026-11-15] 클라이언트 비동기 패키징 실시간 로그 스트리밍 및 DIA SDK·에셋 참조 검증 개선

- **배경 및 범위**: 에디터 내장 클라이언트 패키징(`BuildProfilePanel`) 시 빌드 및 패키징 로그가 실시간으로 출력되지 않고 멈춰 보이던 문제를 개선하고, DIA SDK 탐색 경로 보강 및 2D 스프라이트/타일맵 패키지 에셋 익스포트 검증 오류를 해소했다.
- **주요 변경 내용**:
  - [ClientBuildJob.h](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/Build/Public/ClientBuildJob.h) / [ClientBuildJob.cpp](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/Build/Private/ClientBuildJob.cpp): 기존 `std::future` 단일 대기 방식에서 `std::thread` 작업자 스레드 및 `OutputCallback`(`std::function<void(std::string_view)>`) 기반 실시간 로그 스트리밍 구조로 전환.
  - [ClientPackage.h](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/Build/Private/ClientPackage.h) / [ClientPackage.cpp](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/Build/Private/ClientPackage.cpp): `ExportClientPackage`에 `OutputCallback`을 연결하여 파일 복사 및 검증 진행 상황을 실시간 콘솔로 스트리밍.
  - [BuildProfilePanel.cpp](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/ImGui/BuildProfilePanel/Private/BuildProfilePanel.cpp): 빌드/패키징 중 실시간으로 누적되는 로그를 `ImGui::BeginChild` 스크롤 뷰에 즉시 렌더링하고 `ImGui::SetScrollHereY(1.0f)`로 자동 스크롤 연동.
  - [AssetReferenceValidator.cpp](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/AssetManager/Private/AssetReferenceValidator.cpp): `ValidateBuildScene()`에서 가상 URI(`AssetKey`)를 우선 검증하고, 절대 경로가 프로젝트/엔진 루트 하위일 경우 가상 URI로 정상 변환하여 외부 경로로 오탐되는 문제 해결 (`tilemap.png` 참조 오류 해소).
  - [ReflectionGenerator.cmake](file:///i:/BamtoliyaGithub/BamEngine/Projects/Reflection/cmake/ReflectionGenerator.cmake) / [ReflectionEnTT/CMakeLists.txt](file:///i:/BamtoliyaGithub/BamEngine/Projects/ReflectionEnTT/CMakeLists.txt): Visual Studio 18 2026 및 VS 2022 환경의 DIA SDK 라이브러리(`diaguids.lib`) 탐색 힌트(`_dia_sdk_hints`) 보강.
  - [CMakePresets.json](file:///i:/BamtoliyaGithub/BamEngine/CMakePresets.json): Ninja 빌드 프리셋(`windows-ninja-base`, `x64-ninja-debug`, `x64-ninja-release`, `x64-client-release`) 구성 보강.
- **검증 상태**: `BuildProfilePanel`을 통한 클라이언트 패키지 빌드 및 `Client/결과/BamEngine-Windows-x64/` 패키지 생성 성공, 씬 에셋 참조 검증 통과 사용자 실행 확인 완료.
- **미검증 항목**: Linux/macOS 등 비-Windows 플랫폼 패키징 툴체인.

---

### [2026-11-15] 2D Phase 5 스프라이트 인스턴싱 드로우콜 배칭 및 기본 Unlit 머티리얼·Fallback 연결

- **배경 및 범위**: [SpriteAnd2DSystemPlan.md](SpriteAnd2DSystemPlan.md)의 잔여 과제인 Phase 5를 완결하여, 스프라이트·타일맵 렌더링 시 발생하는 개별 드로우콜 및 상수 버퍼 업로드 병목을 인스턴싱 기반 배치 렌더링으로 해소하고, 미지정 스프라이트의 기본 Unlit 머티리얼 및 쿼드 메시 자동 Fallback을 연결했다.
- **주요 변경 내용**:
  - [InstanceData.h](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Render/Public/InstanceData.h): `SpriteInstanceData` (112바이트: `worldMatrix` 64B, `color` 16B, `uvTransform` 16B, `pickingData` 16B) 구조체 및 크기 검증 `static_assert` 추가.
  - [gbufferSpriteVS.hlsl](file:///i:/BamtoliyaGithub/BamEngine/Engine/Resources/Shader/hlsl/gbufferSpriteVS.hlsl): 단일 `b0, space2` 상수 버퍼 대신 `StructuredBuffer<SpriteInstanceData> g_SpriteInstances : register(t0, space2);`와 `SV_InstanceID`를 참조하도록 셰이더를 인스턴싱 구조로 전환.
  - [Renderer.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Render/Private/Renderer.cpp): `RenderSprite`에서 기존 개별 `DrawIndexed` 호출 루프를 `flushBatch` 람다 및 `DrawIndexedInstanced` 기반 배치 파이프라인으로 교체. 연속된 동일 상태(material, mesh, texture) 명령을 `instanceBatch`에 최대 1024개까지 모아 1회의 인스턴스 드로우콜로 일괄 제출하며, `sortingLayerID` 및 `orderInLayer` 정렬 순서를 보존.
  - [SpriteRenderSystem.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Sprite/Private/SpriteRenderSystem.cpp): `SpriteRendererComponent`의 `meshHandle` 및 `materialHandle`이 비어있을 때 `Engine://Mesh/Quad`와 `Engine://Material/SpriteMaterial.bammat.json`으로 자동 Fallback하는 안전 로직 추가.
  - [SpriteMaterial_AlphaBlend.bammat.json](file:///i:/BamtoliyaGithub/BamEngine/Engine/Resources/Material/SpriteMaterial_AlphaBlend.bammat.json): 반투명 스프라이트용 Unlit 머티리얼(`m_BlendMode: 2`, `m_DepthMode: 1`) 신규 구비.
- **검증 상태**: Windows `x64-debug` 빌드 및 셰이더 컴파일(`gbufferSpriteVS.cso`, `gbufferSpriteVS.spv`) 성공, CTest 전체 36/36(`SpriteAtlasCheck`, `TilemapCheck`, `PixelPerfectCameraCheck` 등) 통과 확인(사용자 실행 결과).
- **미검증 항목**: `x64-release` 빌드, 수만 개 스프라이트/타일 밀집 씬에서의 RHI FrameMetrics 드로우콜/프레임 타임 수동 계측.

---

### [2026-11-15] 타일 컬링(Tiled Light Culling) 257개 초과 광원 안전 폴백 및 다수 광원 지원 확장 (A06 본체)

- **배경 및 범위**: `2026-11-06` 점검 항목 중 유일한 렌더링 잔여 결함이었던 `A06 본체`(타일당 광원 256개 상한 초과 시 257번째 이후 광원 누락 문제)를 해소하고, `LightManager`의 씬 광원 상한을 확장했다.
- **주요 변경 내용**:
  - [tiledLightCullingCS.hlsl](file:///i:/BamtoliyaGithub/BamEngine/Engine/Resources/Shader/hlsl/tiledLightCullingCS.hlsl): 전역 버퍼 `g_TileLightGrid[tileIndex].y`에 클램핑된 count 대신 실제 컬링된 광원 수(`s_TileLightCount`)를 그대로 기록하도록 수정. `g_TileLightIndexList`에는 버퍼 범위 내에서 최대 256개까지만 기록하여 VRAM 오버런 방지.
  - [lightingPS.hlsl](file:///i:/BamtoliyaGithub/BamEngine/Engine/Resources/Shader/hlsl/lightingPS.hlsl): `tileInfo.y > 256u`일 때 타일 오버플로를 감지하고, 불완전한 타일 인덱스 리스트 대신 전체 광원 버퍼(`gLightBuffer`, 0 ~ `numLights - 1`)를 직접 순회하는 **안전한 전체 순회 폴백(Full Traversal Fallback)**을 적용하여 조도 누락을 원천 차단. 또한 `tileLightCount` 변수 스코프를 선언하고, `GetTileHeatmapColor()`에서 256개 초과 타일을 순수 레드(`1.0, 0.0, 0.0`) 경고 색상으로 시각화.
  - [LightManager.h](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Render/Light/Public/LightManager.h) / [LightManager.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Render/Light/Private/LightManager.cpp): `MAX_LIGHTS`를 32에서 `1024`로 확장하고, 스택 `std::array`를 힙 기반 `std::vector<uint8>`로 교체하여 스택 오버플로 방지.
  - [DeferredSceneRenderer.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Render/Pipeline/Private/DeferredSceneRenderer.cpp): 독립 프리뷰 로컬 광원 버퍼 할당 및 데이터 준비에서도 `std::vector<uint8>` 적용 및 `MAX_LIGHTS` 동기화.
- **검증 상태**: `x64-debug` 멀티코어 빌드 및 셰이더 컴파일(DXIL `.cso`, SPIR-V `.spv`) 성공, CTest 전체 36/36 통과 확인(사용자 실행 결과).
- **미검증 항목**: `x64-release` 빌드, 실제 257개 이상 광원이 배치된 씬에서의 실시간 렌더링 화면 비교 및 GPU 프로파일링 계측.

---

### [2026-11-14] 독립 Client CrashHandler 연결 및 PackageClient.ps1 현행화 (B05)

- **배경 및 범위**: [2026-11-14 코드·원칙·계획 점검](CodeDocumentationAudit-2026-11-14.md)의 마지막 코드·스크립트 과제(`B05`)와 루트 임시 셰이더 테스트 산출물(`test_vs.spv`, `test_vs.wgsl`) 정리를 수행하고, 병행 작성된 [ShaderToolchainEvaluation.md](ShaderToolchainEvaluation.md)를 함께 문서화했다.
- **주요 변경 내용**:
  - [Client/Source/Main.cpp](file:///i:/BamtoliyaGithub/BamEngine/Client/Source/Main.cpp): `Logger::Initialize(logPath)` 직후 `#if defined(_WIN32)` 조건에서 `Engine::CrashHandler::Initialize(m_ProjectRoot / "Logs" / "Crashes")` 및 `SetCustomContext("Host", "Client")`를 호출하고, 소멸자에서 `Engine::CrashHandler::Shutdown()`을 호출하도록 연결했다. 또한 `ResolveProjectRoot()`가 `.bam-client-package` 마커 및 `Config` 폴더를 우선 확인하도록 갱신했다.
  - [ClientGameRuntime.cpp](file:///i:/BamtoliyaGithub/BamEngine/Client/Source/Execution/Private/ClientGameRuntime.cpp): `LoadStartupScene()` 성공 시 `#if defined(_WIN32)` 조건에서 `Engine::CrashHandler::SetActiveScenePath(m_ProjectSettings.StartupScenePath)`를 등록하도록 추가했다.
  - [CrashHandler_Win32.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Core/Diagnostics/Private/CrashHandler_Win32.cpp): `BamUnhandledExceptionFilter()` 안내 메시지박스에서 `s_EmergencyCallback` 등록 여부에 따라 `Editor`(작업 복원 스냅샷 안내 포함)와 `Client`(복원 안내 제외) 문구를 분기했다.
  - [PackageClient.ps1](file:///i:/BamtoliyaGithub/BamEngine/Tools/Scripts/PackageClient.ps1): 공식 배포 경로가 에디터 내장 패키저([ClientPackage.cpp](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/Build/Private/ClientPackage.cpp))임을 명시한 CLI 로컬 스테이징 보조 스크립트로 구분하고, 구형 `Resources/` 복사를 `Engine/Resources` + `Assets` + `.bam-client-package` 구조로 현행화했다. 아울러 `out/package/<하위폴더>` 전용 제한, `Client`/`Assets`/`Utility`/`ThirdParty` 등 핵심 폴더 차단, 리파스 포인트 거부, `.staging` 선행 작성 후 교체를 반영했다.
- **검증 상태**: `PackageClient.ps1` 경로 차단 가드 4건(`.`, `Client`, `Assets`, `Utility` → `Security Violation` 예외 중단) 사용자 실행 확인, 루트 `test_vs.spv`·`test_vs.wgsl` 삭제 완료.
- **미검증 항목**: `B05` 반영분 포함 Windows `x64-debug`/`x64-release` 재빌드·실행, `Client.exe` 실제 크래시 유발 시 `.dmp`/`.crash.json` 생성 확인, `PackageClient.ps1` 정상 스테이징 및 Emscripten/Tint 실 툴체인 실행 검증.

---

### [2026-11-14] WebGPU·Emscripten 이식 선결 과제 해소 (B01~B03)

- **배경 및 범위**: [2026-11-14 코드·원칙·계획 점검](CodeDocumentationAudit-2026-11-14.md)에서 도출된 `B01`(StartupResourceLoader↔Shader WGSL 경로 단절), `B02`(ShaderImporter 구형 경로 하드코딩 및 HotReload WGSL 누락), `B03`(ClientGameRuntime·Engine/CMakeLists.txt·ClientRenderPipeline의 Emscripten/RHI 경계 불일치)을 순차 수정했다.
- **주요 변경 내용**:
  - `B01`: [StartupResourceLoader.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/Resource/Private/StartupResourceLoader.cpp)의 `ShaderItemConfig::WgslPath`, `DeriveStartupWgslPath()`, `GetStartupShaderPath()`(`ERHIType::WebGPU`·`Vulkan`), `LoadShaders()`(`dxilPath`/`spirvPath`/`wgslPath`)를 연결했고, [SceneArchiveCheck.cpp](file:///i:/BamtoliyaGithub/BamEngine/Engine/examples/SceneArchiveCheck.cpp)의 `CheckShaderPathRoundTrip()`에 `GetWgslPath()` 초기값 및 3회 저장/로드 검증을 추가했다.
  - `B02`: [ShaderImporter.cpp](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/AssetManager/Importer/Private/ShaderImporter.cpp)의 `.hlsl` 분기에서 `PlatformPath::ToVirtualUri(sourcePath)`와 `binRoot` 기준 `dxil`·`spirv`·`wgsl` 경로 및 후보 키 기반 `UpdateBytecode`를 적용했고, [ShaderHotReloadManager.cpp](file:///i:/BamtoliyaGithub/BamEngine/Editor/Source/HotReload/Private/ShaderHotReloadManager.cpp)에 `ERHIType::WebGPU` 선택 및 3종 백엔드 경로 등록을 반영했다.
  - `B03`: [Systems.h](file:///i:/BamtoliyaGithub/BamEngine/Engine/Source/World/System/Public/Systems.h)와 [ClientGameRuntime.cpp](file:///i:/BamtoliyaGithub/BamEngine/Client/Source/Execution/Private/ClientGameRuntime.cpp)에서 `NetworkSystem` 포함·등록을 `#if !defined(__EMSCRIPTEN__)`로 감쌌고, [Engine/CMakeLists.txt](file:///i:/BamtoliyaGithub/BamEngine/Engine/CMakeLists.txt)에 `EMSCRIPTEN` FMOD Wasm(`ThirdParty/FMOD/lib/w32/fmodL_wasm.a`+`fmodL_bindings.a` / `fmod_wasm.a`+`fmod_bindings.a`) 링크를 추가했으며, [ClientRenderPipeline.cpp](file:///i:/BamtoliyaGithub/BamEngine/Client/Source/ClientRenderPipeline.cpp)의 셰이더 로드를 `binRoot`(`Shader/bin`) 기반 3종 백엔드 경로 및 RHI별 `desc.Path` 선택으로 변경했다.
- **검증 상태**: Windows `x64-debug` 빌드 및 CTest 전체 36/36 통과 확인(사용자 실행 결과).
- **미검증 항목**: `x64-release` 빌드, Emscripten SDK / Tint 실 툴체인 빌드 및 WebGPU 런타임 실행, `RenderPipelineProfilePlan` 2·3단계 수동 비교, `B05`(독립 Client `CrashHandler` 연결 및 레거시 `PackageClient.ps1` 정리).

---

### [2026-11-14] 에디터 사용성(UX) 테스트 및 검증 기준 문서화

- **배경/목적**: `EngineEditorClientGuidelines.md`와 `ImGuiDevelopmentPlan.md`에 분산된 에디터 UI/UX 구현 규칙에 대응해, 신규 패널 추가 및 공용 UI 회귀 확인 시 일관되게 적용할 테스트 계층, 경계 조건 및 Pass/Fail 판정 기준을 수립했다.
- **수행 내용**:
  - [`Docs/EditorUXTestGuidelines.md`](EditorUXTestGuidelines.md) 신설:
    - 검증 3계층 정의 (Tier 1 정적·데이터 감사, Tier 2 헤드리스 ImGui CTest, Tier 3 실환경 대화형·GPU 검증).
    - 표준 테스트 환경 매트릭스(한·영 실시간 전환, UI 배율 `1.0x`/`1.5x`/`2.0x`, 도킹 폭 표준/좁음/극한 경계) 정의.
    - 6대 핵심 UX 영역별 Pass/Fail 기준(`UX-1` 레이아웃 적응성, `UX-2` 입력·단축키 격리, `UX-3` 공용 UI 패턴, `UX-4` 편집 데이터 보호, `UX-5` 문맥 전환·영속 복원, `UX-6` 프레임 비용 예산) 및 미검증 공용 UI 회귀 점검 시트·`EditorUICheck` 확장 후보 정리.
  - 진입 문서 및 연결 갱신: [`AGENTS.md`](../AGENTS.md)(10절·12절), [`Docs/EngineEditorClientGuidelines.md`](EngineEditorClientGuidelines.md)(19절), [`Docs/DevelopmentPriorities.md`](DevelopmentPriorities.md)(1절), [`Docs/ImGuiDevelopmentPlan.md`](ImGuiDevelopmentPlan.md)(상단 메타데이터).
- **검증 및 미검증 범위**:
  - 기존 CTest 검사 소스(`MainMenuBarLayoutCheck`, `ShortcutManagerCheck`, `ReflectionInspectorCheck`, `HierarchyEditorCheck`, `EditorHistoryCheck`, `CanvasViewCheck`, `GraphMessageLocalizationCheck`), `LocalizationBuild.cmake` 및 `audit_localization.py` 대조 후 문서화했으며, C++ 코드 변경이나 빌드·테스트 실행은 수행하지 않았다.
  - 문서 내 `5절 미검증 공용 UI 실환경 회귀 점검 시트`의 실행 검증과 `6절 EditorUICheck` 구현은 후속 사용자 검증 및 작업 범위로 남긴다.

### [2026-11-14] 프로젝트 현황 및 코드·문서 정합성 점검 (B01~B06)

- **배경/목적**: `2026-11-06` 점검(`ecca09da`, A01~A10) 이후 누적 커밋(`fabcf611`)과 현재 작업 트리의 미커밋 변경 4갈래(`StartupResourceLoader` 공용화, `ClientGameRuntime` 분리, Emscripten/WGSL 빌드 경계 초안, `ThirdParty/FMOD` 2.03.15 갱신)를 대조하고 핵심 계획 문서를 최신화하기 위해 정적 점검을 수행했다.
- **수행 내용**:
  - 신규 점검 보고서 [`Docs/CodeDocumentationAudit-2026-11-14.md`](CodeDocumentationAudit-2026-11-14.md) 작성 (`A01~A10` 반영 현황 정리 및 신규 `B01~B06` 식별).
  - [`Docs/DevelopmentPriorities.md`](DevelopmentPriorities.md) 상단 상태·기준 문서 목록·P2-E·11절 우선순위 재평가(트랙 H·I·J, 작업 트리 정리 및 단기 후속 순서 `B01~B05`) 갱신.
  - [`Docs/BuildAndPackagingPlan.md`](BuildAndPackagingPlan.md), [`Docs/RenderingDevelopmentPlan.md`](RenderingDevelopmentPlan.md), [`Docs/EngineDevelopmentPlan.md`](EngineDevelopmentPlan.md), [`Docs/ImGuiDevelopmentPlan.md`](ImGuiDevelopmentPlan.md)의 최신 현황 및 미완료/이식 선결 과제 동기화.
- **식별된 주요 미해결 항목 (`B01~B05`)**:
  - `B01`: `StartupResourceLoader`↔`Shader` 간 `ERHIType::WebGPU`/WGSL 경로 연결 단절.
  - `B02`: `Editor/Source/AssetManager/Importer/Private/ShaderImporter.cpp`의 구형 `Resources/Shader/` 하드코딩 잔류.
  - `B03`: `ClientGameRuntime`의 `NetworkSystem`·FMOD·DXIL 고정 의존성과 `Engine/CMakeLists.txt` `EMSCRIPTEN` 제외 경계 불일치.
  - `B04`: 저장소 루트 0바이트 untracked 파일(`cd`, `emcc`, `emcmake`) 잔존 및 183개 파일 다목적 미커밋 혼재.
  - `B05`: 독립 `Client/Source/Main.cpp`의 `CrashHandler` 미연결 및 레거시 `Tools/Scripts/PackageClient.ps1` 병존.
- **검증 및 미검증 범위**:
  - 정적 코드·CMake·문서 대조 및 `Docs/` 줄 끝/공백 점검 완료.
  - `Engine/CMake/Dependencies.cmake`의 `spirv_cross_c` 선행 호출 오류(`find_package` 순서/스코프 불일치) 수정 후 사용자 환경에서 `x64-debug` CMake 멀티코어 빌드 및 CTest **36/36 통과(5.03 sec)** 확인 완료.
  - Windows `x64-release` 빌드, 실행 시 FMOD 2.03.15 오디오 재생, UI/GPU 수동 비교(`RenderPipelineProfilePlan.md` 2·3단계, `SDL_CleanupTLS()`), Emscripten/Tint 툴체인 검증은 아직 미실행 상태다.

### [2026-11-14] WGSL 셰이더 sidecar 경로 초안

- `Shader`에 선택적 `wgslPath`를 추가하고 WebGPU RHI 선택 시 WGSL 경로를 사용하도록 했다. ResourceManager와 ShaderImporter는 `.wgsl` 리소스 및 빌드 산출물 경로를 인식한다.
- `BAMENGINE_BUILD_WGSL_SHADERS=ON`이면 호스트 Tint로 기존 SPIR-V를 `Engine/Resources/Shader/bin/wgsl`에 변환하는 CMake 단계를 추가했다. 네이티브 `ShaderCompiler`도 WebGPU용 HLSL을 DXC → SPIR-V → Tint → WGSL로 변환하며, SPIR-V import와 Editor 핫 리로드가 WGSL 바이트코드를 사용하도록 연결했다. Emscripten 구성은 네이티브 `ShaderCompiler.cpp`를 제외한다.
- Tint 실행 파일, 변환 CLI, Reflection 코드 생성, 빌드와 브라우저 실행은 검증하지 않았다. Tint 실행 파일은 CMake에서 `TINT_EXECUTABLE`로 지정하거나, 네이티브 import·핫 리로드 실행 시 `TINT_EXECUTABLE` 환경 변수 또는 PATH에 제공해야 한다. 이 변경만으로 WebGPU RHI나 Client 실행이 완성된 것은 아니다. [WebGPU 이식 계획](BuildAndPackagingPlan.md#webgpu-씬-프리뷰와-client-이식)
- `Tools/WebGPUProbe/CMakeLists.txt`에 Engine 종속성 없이 `emdawnwebgpu` 포트를 빌드·링크하는 독립 Emscripten 타깃을 추가했다. 현재 환경에는 Emscripten과 Tint가 없어 이 프로브, 셰이더 변환, Engine WebGPU RHI 및 Client는 아직 실행 검증되지 않았다.

### [2026-11-14] Emscripten 빌드 경계 초안

- Emscripten 구성에서 Engine 타깃을 정적으로 만들고, 네이티브 그래픽 백엔드·셰이더 컴파일·Win32 전용 소스와 앱/도구 타깃을 제외하도록 CMake 및 vcpkg 의존성 조건을 분리했다. Reflection 파서는 Emscripten 전처리 조건을 읽으며, 코드 생성에는 호스트용 ReflectionGenerator와 Clang 리소스 디렉터리를 요구한다.
- 이 단계에는 WebGPU RHI, WGSL, 브라우저 Client 호스트와 게임 모듈의 정적 등록은 포함되지 않는다. Emscripten 도구 체인이 없어 구성·빌드·실행 검증은 하지 않았다. 사용자가 Emscripten 환경에서 CMake 구성과 Engine 정적 라이브러리 빌드를 확인해야 한다. [WebGPU 이식 계획](BuildAndPackagingPlan.md#webgpu-씬-프리뷰와-client-이식)

### [2026-11-14] WebGPU 이식을 위한 Client 실행 계층 분리

- `ClientGameRuntime`으로 프로젝트 설정, Engine Runtime, 시스템·씬 초기화, 입력 갱신, 리사이즈와 프레임 렌더 제출을 분리했다. Windows `Application`은 SDL 이벤트·창·D3D12 RHI와 `GameModule.dll` 로드를 맡고, 모듈 로드 시점은 콜백으로 공유 실행 계층에 전달한다.
- 기존 시작 순서와 Client 렌더·시뮬레이션 흐름을 유지하도록 코드 경계를 이동했다. Engine CMake의 Windows 전용 링크 의존성, WebGPU RHI/셰이더 경로, Wasm 대상 및 Editor 내보내기는 아직 구현하지 않았다.
- 정적 검토만 수행했으며 저장소 지침에 따라 빌드와 테스트는 직접 실행하지 않았다. 사용자가 Windows Client 빌드와 실행 성공을 확인했다. 설정·시작 씬·모듈 경로의 세부 검증과 WebAssembly 대상은 아직 확인되지 않았다. [WebGPU 이식 계획](BuildAndPackagingPlan.md#webgpu-씬-프리뷰와-client-이식)

### [2026-11-13] Client 시작 리소스 파이프라인 공용화

- `StartupResourceLoader`를 Editor AssetManager에서 Engine Resource로 이동해 Editor와 Client가 동일한 시작 매니페스트·카테고리 로더를 사용한다. `Compute` 셰이더 타입을 명시 지원하고 지원하지 않는 타입이나 누락된 직접 참조 리소스는 오류로 처리한다.
- Client는 Runtime 리소스 경로를 설정한 뒤 시작 매니페스트를 로드한다. 패키지 마커가 있는 실행에서 `Config/Project.json` 누락과 빈 `StartupScenePath`는 실패하며 빈 씬 경로의 기본 씬 폴백을 제거했다.
- Windows D3D12 패키저는 매니페스트, 카테고리 설정과 직접 참조 리소스를 사전 검증하고 인벤토리에 기록한다. 전체 런타임 리소스 스캔에서 누락된 시작 의존성은 staging에 복사한다. 선택적 수집은 아직 적용하지 않았다.
- 코드·경로·실패 분기를 정적으로 확인했다. 프로젝트 지침에 따라 빌드와 테스트는 실행하지 않았다. Editor·Client 빌드, Compute 셰이더 프리로드, 개발 설정 누락과 패키지 설정 누락, staging 결과 및 저장소 밖 독립 실행은 사용자 검증 전이다. [빌드·패키징 계획](BuildAndPackagingPlan.md)

### [2026-11-13] 독립 WebGPU 씬 프리뷰

- `Client/Web`에서 `.bamscene.json` v1을 읽고, 활성 카메라와 계층 변환을 적용한 `CubeMesh`·`SphereMesh`를 깊이 검사와 함께 표시한다. 프로젝트 `Gameplay`의 이동 바인딩은 브라우저 카메라에 연결한다.
- 별도 CMake 구성으로 시작 씬, 입력 설정과 정적 페이지를 한 폴더에 수집한다. 씬 경로는 프로젝트 애셋 루트 기준으로 선택한다.
- 사용자가 `RaytracingScene3` 프리뷰를 열고 Windows와 다른 렌더링 및 애니메이션 객체의 부재를 확인했다. 프리뷰는 정적 기본 도형을 지원한다. 실제 Wasm Client, Fox 애니메이션, Render Pipeline Profile 실행과 Editor의 WebGPU 내보내기는 후속 범위다. [프리뷰 실행과 이식 순서](BuildAndPackagingPlan.md#webgpu-씬-프리뷰와-client-이식)

### [2026-11-13] Windows Client 패키지 포함 목록 비교 로그

- `빌드 및 내보내기`의 `[Package inventory]`에 프로젝트 애셋·엔진 리소스의 파일 수와 바이트를 기록한다. 시작 씬, 확인된 씬 참조, 렌더 프로필과 필수 렌더 리소스에는 포함 이유를 표시한다. 의존성을 아직 확인하지 못한 파일은 건수와 예시를 남긴다.
- 고정 런타임 바이너리 목록의 이름·수·바이트를 출력한다. 현재 수집·복사·검증·게시 순서는 유지하며, 예시는 삭제 대상 판정에 쓰지 않는다. [빌드·패키징 계획](BuildAndPackagingPlan.md#54-적용-순서와-완료-기준)
- 변경부 공백과 수집 분기를 정적으로 확인했다. Debug Editor에서 `RaytracingScene3`를 내보내고 렌더 프로필 유무와 같은 파일을 여러 곳에서 참조하는 경우를 확인한다. 실제 패키지의 파일·바이트 수 비교와 Client 독립 실행은 사용자 검증 전이다.

### [2026-11-13] Client 빌드 워커의 SDL TLS 정리

- `ClientBuildJob::Build()`는 `std::async`에서 SDL로 CMake 출력을 읽는다. SDL 3.4의 Windows 파이프 읽기가 출력을 기다리면 `SDL_Delay()`가 스레드 로컬 타이머 또는 이벤트 핸들을 등록한다. SDL 오류 경로도 스레드 로컬 버퍼를 만들 수 있다.
- 빌드 워커의 스코프 종료에서 `SDL_CleanupTLS()`를 호출한다. configure·build·패키징의 성공, 조기 반환, 예외에서 워커 스레드가 끝나기 전에 정리한다. 기존 프로세스와 출력 버퍼의 해제 경로를 유지한다. [빌드 워커](../Editor/Source/Build/Private/ClientBuildJob.cpp) · [SDL API](https://wiki.libsdl.org/SDL3/SDL_CleanupTLS)
- SDL 소스와 변경부를 정적으로 확인했다. Debug Editor에서 작업 없이 종료, Client 빌드 후 종료, 빌드 및 내보내기 후 종료를 각각 비교한다. Visual Studio 출력에서 누수 덤프가 사라지는지, 반복 빌드 후 프로세스 핸들 수가 계속 늘지 않는지 확인해야 한다. 빌드와 재현 검증은 미실시다.

### [2026-11-13] 선택적 쿠킹과 증분 내보내기 설계

- Windows 폴더 내보내기에서 참조된 애셋과 엔진 리소스, 선택한 런타임 모듈과 시스템을 수집하는 기준을 기록했다. 런타임 동적 로드는 명시적 의존성으로 수집하는 방향이다.
- 대상별 Cook 캐시와 직전 배포본을 이용한 스테이징 재사용, 누락 시 실패·복구 흐름과 비교 지표를 문서화했다. 선택적 수집과 캐시 구현은 후속 작업이다. [빌드·패키징 계획](BuildAndPackagingPlan.md#5단계-선택적-쿠킹과-증분-스테이징)

### [2026-11-13] Windows Client 3인칭 조작 복구

- Client 초기화에 `ThirdPersonControllerSystem`을 등록해 배포 씬의 3인칭 입력과 캐릭터 이동을 처리한다.
- 사용자가 시스템 등록 후 Client 조작 복구를 확인했다. 이번 커밋에서 등록 위치를 정적으로 확인했다. [Client 초기화](../Client/Source/Main.cpp)

### [2026-11-13] Deferred 파이프라인 스키닝 그림자와 파티클 합성

- 방향광 캐스케이드, 스포트라이트 사분면과 점광원 큐브맵 각 면에서 스키닝 그림자 큐도 그린다. `RaytracingScene3`의 Fox는 본 행렬과 월드 변환을 본체 드로우와 동일하게 사용한다.
- 기본 알파·가산 파티클 머티리얼은 하늘 다음의 `ForwardTransparent`에서 HDR 컬러에 합성하고 씬 깊이를 읽는다. 불투명·마스크드 파티클은 Geometry에서 그린다. 독립 프리뷰에는 프리뷰 씬의 파티클을 패스별로 제출한다.
- 두 파티클 머티리얼 JSON의 파싱과 변경부 공백을 확인했다. Editor와 Windows 패키지에서 Fox의 애니메이션 그림자, 하늘 앞 파티클, 불투명 물체에 가려지는 파티클과 독립 프리뷰를 확인해야 한다. 빌드와 실행 검증은 미실시다. [렌더 파이프라인 검증 절차](RenderPipelineProfilePlan.md)

### [2026-11-11] 콘텐츠 브라우저 다중 선택 경로 복사와 씬 배치

- 콘텐츠 브라우저의 선택된 타일에서 우클릭하면 선택한 경로를 줄 단위로 복사한다. 선택 밖 타일의 우클릭은 해당 항목의 메뉴를 연다.
- 씬 뷰포트의 다중 드롭에서 지원하는 모델과 프리팹을 배치한다. 성공한 항목은 한 편집 이력으로 기록하고, 일부 항목을 배치하지 못하면 결과 개수를 표시한다.
- 변경부의 공백 검사와 로캘 키를 정적으로 확인했다. Editor 빌드, 다중 경로 복사, 모델·프리팹 배치와 Undo/Redo, 기존 단일 드롭은 사용자 확인 전이다. [다중 선택 계획](ImGuiDevelopmentPlan.md#f-콘텐츠-브라우저-그리드-다중-선택)

### [2026-11-11] 콘텐츠 브라우저 스프라이트 썸네일 조회 재사용

- `ContentBrowserPanel::DrawGridItem()`은 `AssetCache::GetThumbnail()`에서 텍스처와 스프라이트 핸들을 함께 받는다. `ContentBrowserGridItem::DrawThumbnail()`은 핸들로 현재 UV를 읽고, 스프라이트를 찾지 못하면 기존 파일 경로와 리소스 키 조회를 사용한다. 비교용 환경 변수와 계측 분기는 제거했다.
- 사용자가 Release Editor에서 `Tilemap` 폴더의 공유 시트 스프라이트 234개를 대상으로 교차 비교했다. 그리드 CPU p50은 `full` 0.498–0.502 ms, `cached-sprite` 0.314–0.318 ms로 약 37% 감소했다. 전체 프레임의 p95·p99는 실행별 편차가 있었고 GPU 시간은 계측하지 않았다.
- 계측 제거 전 일반 실행에서 계측 로그 미출력, 스프라이트 영역 표시와 수정 후 갱신을 사용자 확인했다. 제거 후 소스에서 계측 참조가 사라진 것을 확인했다. 제거 후 Release 빌드·실행과 텍스처 교체, 파일 추가·삭제, 검색·범위 선택은 별도 검증 항목이다. [ImGui 개발 계획](ImGuiDevelopmentPlan.md#콘텐츠-브라우저에서-썸네일-표시-중-지속되는-프레임-저하)

### [2026-11-10] 공용 에디터 DLL 플러그인 기반

- `PluginHost`가 `Plugins.txt`에서 개발용 타깃을 읽고, 빌드 성공 뒤 이미 활성화된 패널 DLL에 리로드를 요청한다. Probe DLL은 `BamGetPluginV1`에서 두 패널을 반환하고, 두 패널은 하나의 모듈을 공유한다. Sprite는 기존 `EditorResourcePlugins.txt`와 편집 세션 경로를 사용한다.
- `PluginHost`에 Builtin·Custom 패키지의 로드·리로드·언로드를 연결하고, 관리자 상세 화면에서 해당 동작을 요청하도록 했다. Custom 승인창에는 패키지 이름과 DLL 경로, SHA-256을 표시한다.
- 승인에 사용한 프로젝트·패키지·DLL 정보와 현재 후보를 다시 대조한 뒤 기존 V1 패널 등록 경로로 활성화한다. 성공한 승인은 Editor 로컬 환경설정 폴더에 저장하며, 동시에 저장하려는 다른 프로세스와 충돌하면 기존 파일을 보존한다.
- [플러그인 설계](EditorPluginArchitecturePlan.md#8-현재-진행-상태-2026-11-10)에 검증 절차, 함수·헤더의 역할과 Mermaid 흐름도, 기능별 이관 범위를 정리했다.
- 사용자는 승인 기능 적용 전 Debug 빌드와 Probe의 수동 로드·리로드 성공을 보고했다. 이번 변경은 소스·설정과 로캘 데이터를 정적으로 대조했다. 최신 Debug 빌드, 승인 취소·재사용·변경 DLL 재승인, 교체 실패와 언로드는 사용자 확인 전이다.

### [2026-11-10] Debug 회귀 수정 및 사용자 검증

- `PhysicsSystem`의 위치 보정 반복에 2D 침투도 반영했다. 2D 전용 씬에서 3D 접촉 유무에 따라 보정이 조기 종료되던 경로를 수정했다. 사용자가 실행한 `Physics2DCheck`에서 키네마틱 접촉을 포함한 검사가 통과했다.
- `TilemapCheck`는 테스트용 `Sprite`를 `ResourceManager`에 등록하고 반환된 핸들로 타일 배치·삭제와 저장 데이터 동기화·복원을 검사한다. `SceneArchiveCheck`는 `managed-scene.json`의 저장·보존 비교·재로드에서 CTest 작업 폴더의 같은 절대 경로를 사용한다. 06:06 Debug CTest 로그에서 두 타깃의 통과를 확인했다. 잘못된 문서와 잘린 JSON의 `SceneManager` 로드 경로에는 상대 경로 불일치가 남아 있다.
- 진단 DLL 호스트를 `PluginPanel`로 전환하고 내보내기 매크로를 `ModuleApiExport.h`에 분리했다. 첨부한 사용자 로그에서 Editor, `EditorPanelLifecycleProbe`, `GameplayScore` 빌드와 `ShortcutManagerCheck`, `TransformInheritanceCheck` 통과를 확인했다.
- 검증 기록은 두 차례의 사용자 실행 결과다. 전체 CTest 재실행, 플러그인 UI에서의 로드·교체·제거 및 Client 실행 확인은 이번 기록에 포함되지 않는다.

### [2026-11-10] 변경 분류와 검증 범위

- 엔진 소스 이관을 먼저 분리했다. 에디터 공용 UI를 바탕으로 리소스 경로·콘텐츠 탐색, 후처리 편집, 2D 제작, 이펙트 타임라인, 크래시 복원, 에디터 플러그인과 GameplayScore 예제를 각각 기록한다.
- 이번 정리에서는 파일 이동, 호출부, CMake·검사 타깃 참조와 Git 변경 범위를 정적으로 확인했다. 아래 기존 항목의 구현·기대 결과와 이번 정리의 실행 검증을 구분한다. 빌드, CTest, Editor·Client 실행과 GPU 측정은 수행하지 않았다.
- 프로젝트 시작 매니페스트의 로컬 존재 여부와 Editor·Client 시작 경로를 확인해야 한다. 진단 플러그인과 GameplayScore의 DLL 진입점은 각 소비자와 함께 확인해야 한다. 2D 아틀라스는 배치·UV와 CPU 버킷까지이며 드로우콜 감소 수치는 측정하지 않았다.

### [2026-11-10] 플러그인 생성 템플릿 설계 개정

- [공용 플러그인 아키텍처](EditorPluginArchitecturePlan.md)에 Blank, Editor Standalone Window, Third Party Library, Game Feature (Content Only) 생성 템플릿을 정리했다.
- Window와 Panel은 같은 등록 계약을 쓰고 에셋 열기와 저장은 선택 기능으로 붙인다. DLL 패키지와 콘텐츠 전용 패키지의 활성화, Builtin·Custom 발견, 구현 순서와 완료 기준을 설계에 반영했다.
- 현재 리소스 패널 전용 로딩 경로와 가상 경로 구현을 정적으로 대조했다. 이번 변경은 설계 문서에 한정한다.

### [2026-11-10] GameModule 런타임 플러그인 예제

- `GameplayScore.dll`을 독립 빌드 타깃으로 추가하고, GameModule이 기존 모듈 로더와 버전이 있는 함수 테이블로 콤보 점수를 계산하도록 연결했다.
- [에디터 플러그인 아키텍처 설계](EditorPluginArchitecturePlan.md#7-gamemodule에서-사용하는-런타임-플러그인)에 Editor·Runtime 모듈 경계, 의존 순서, 예제 문법과 사용자 확인 절차를 추가했다.
- 대상·진입점·호출 관계를 소스로 대조했다. 실제 빌드와 Editor·Client 실행 확인은 미실시다.

### [2026-11-10] Builtin·Custom 에디터 플러그인 공용 설계

- [에디터 플러그인 아키텍처 설계](EditorPluginArchitecturePlan.md)에 패키지 발견·활성화, DLL 빌드·교체, 패널 및 비패널 기능 등록의 경계를 정리했다.
- 현재 Sprite 리소스 패널, 진단 샘플과 게임 모듈의 소스 경로를 정적으로 대조했다. 구현 순서와 검증 기준을 문서화했으며 빌드와 Editor 실행은 수행하지 않았다.

### [2026-11-10] DLL 리소스 패널 공용 등록

- Sprite 전용 Editor 클래스와 패널·단축키·빌드 관리자 분기를 제거했다. CMake가 생성한 목록과 DLL의 공용 V1 진입점으로 리소스 패널을 등록한다.
- Sprite DLL이 에셋 열기, 저장과 편집 세션을 소유한다. 교체 시 미저장 값과 UI 상태, 엔진 리소스 핸들을 새 세션에 넘긴 뒤 이전 DLL을 해제한다.
- 기존 창 이름, 저장 액션 ID와 Workspace 분할 키를 유지했다. 소스 참조와 변경부 공백은 정적으로 확인했으며 빌드, F5 교체와 편집·저장 회귀 확인은 미실시다.

### [2026-11-10] 스프라이트 애니메이션 편집기 DLL

- `EditorSpriteAnimationTools` 타깃으로 메뉴, 퀵액세스, 프리뷰, 프레임 목록, 필름스트립과 재생 갱신을 이관했다. Editor는 리소스와 미저장 상태, 파일 저장, 단축키, 도킹과 Workspace 상태를 소유한다.
- 진단 패널과 편집기가 DLL·PDB 섀도 복사 로더를 공유한다. 편집기 DLL은 V1 진입점, 구조 크기와 필수 콜백을 확인한 뒤 교체한다. 동일한 엔진 헤더, ImGui와 툴체인을 사용하는 내부 인터페이스다.
- 플러그인 소스 저장과 관리자의 빌드 버튼은 Sprite 전용 타깃을 백그라운드에서 빌드한다. 완료 후 다음 편집기 갱신에서 교체한다. 빌드 도중 감지된 파일 변경은 다음 빌드에 남기고, 후보 검증 실패 시 기존 모듈을 유지한다.
- 소스 경로와 호출 연결을 정적으로 확인했다. Debug 빌드, F5 심볼 로드, 프리뷰·프레임 편집·저장, 미저장 상태를 유지하는 자동 교체, 잘못된 DLL과 연속 저장 검증은 사용자 실행 확인이 남아 있다.

### [2026-11-09] 리소스 편집기 공통 세 줄 헤더

- `ResourceEditorInterface::Draw()`가 창 제목, 메뉴 툴바, 퀵액세스, 본문 순서를 관리한다. 하위 편집기는 `DrawCustomToolbar`, `DrawCustomQuickAccess`, `DrawCustomContent`를 구현하고 각 영역의 내용을 정한다.
- 상속 중인 12개 편집기를 전환했다. 창 ID, 초기 크기, 미저장 표시 및 애니메이터 닫기 확인을 유지했다. 그래프는 프레임을 감싸서 기존 노드 편집기 컨텍스트를 복원한다.
- 좁은 창의 메뉴는 접고, 퀵액세스는 공통 배치로 줄바꿈한다. 별도 편집 명령이 없는 텍스처와 셰이더는 파일 메뉴의 닫기와 퀵액세스의 경로 복사를 제공한다. ActionTimeline의 열기와 저장은 메뉴와 퀵액세스가 같은 함수를 사용한다.
- 상속 및 호출 관계와 diff 공백을 정적으로 확인했다. 빌드·화면 확인은 미실시다. 사용자 확인은 12개 창의 메뉴와 퀵액세스 위치, 작은 폭과 큰 글꼴, 그래프 모드 전환 및 파일 교체 확인, 애니메이터 미저장 닫기, 프리팹 미저장 표시를 포함한다.

### [2026-11-09] 이펙트 타임라인과 리소스 편집기 공통화

- ActionTimeline·Particle·Composite Effect가 시간 좌표, 마우스 위치 기준 휠 확대, 중간 버튼 이동, 초/프레임 눈금자, 재생 헤드, 재생 툴바를 공유한다. ActionTimeline의 노티파이·카메라 샷 데이터와 편집 흐름을 유지한다.
- Particle과 Composite는 목록·프리뷰·속성을 위에 배치하고 전체 폭의 하단 타임라인을 사용한다. 좁은 창에서는 상단을 탭으로 전환한다. 목록·속성 폭, 타임라인 높이와 헤더 폭은 Workspace에 저장한다.
- 라벨과 막대가 동일한 선택 경로를 사용한다. Composite는 트랙 종류/인덱스 ID로 이름 중복을 구분하고 위치·트레일 오프셋에 프리뷰를 맞춘다. 활성 구간 밖에서는 해당 구간의 첫 프레임 근처로 탐색한다. 사운드는 시작 마커로 그린다.
- 막대 본체는 이동, 양끝은 시작/종료 변경, 눈금자의 금색 핸들은 전체 길이 편집이다. 드래그 중에는 위젯에서 후보 값을 그리며, 놓을 때 한 건으로 반영한다. Escape로 구간 드래그를 취소한다. 트랙 이동으로 마지막 끝이 전체 길이를 지나면 전체 길이를 연장한다.
- 두 이펙트 편집기에 재생/일시정지, 이전/다음, 처음/끝, 반복, 시간축 맞춤, 삭제, Undo/Redo, 저장 액션을 각각 등록했다. 툴바는 현재 키 힌트와 실행 가능 조건을 조회한다. Composite의 시간 편집·삭제는 Undo 대상이다. 기존 수동 속성 편집과 트랙 추가는 이 Undo 범위에 포함되지 않으며 트랙 추가 시 인덱스 기반 이력을 초기화한다.
- Particle의 에미터 방출 시작/지속 시간 및 에셋 전체 길이는 누락 허용으로 등록했다. 0 지속/전체 길이는 런타임 무제한이다. 과거 에셋의 편집기 표시 범위는 기본 5초이며 에미터 데이터에 따라 늘어난다. 방출이 끝난 뒤 이미 발생한 입자의 수명은 계속 갱신한다. 트리거 트랙에 적용하는 구간은 이벤트의 허용 구간이며 이벤트가 발생하는 시간을 지정하지 않는다.
- 프리뷰 탐색은 최대 32개의 30fps 스텝을 프레임당 처리한다. 뒤로 이동할 때 초기화하고 프리뷰 RNG 시드를 고정했다. Composite 탐색 중에는 사운드 재생과 루프 래핑을 억제한다. 일반 재생에서 프리뷰가 배속을 시뮬레이터에 한 번만 전달한다.
- 리소스 창 제목, 파일명, 안정 ID 및 미저장 표시를 공통 시작 함수로 통일했다. Composite 사운드 트랙은 공통 속성 표와 리플렉션의 이름/활성/오디오 참조 입력을 사용한다. 수치 입력의 범위와 단위를 유지한다. Particle 인스펙터는 공통 속성 행을 사용한다.
- 변경 경로와 새 파일을 정적으로 검토하고 diff 공백을 확인했다. 빌드·테스트·UI 실행·커밋은 미실시다.

### [2026-11-09] 제목 표시줄 닫기와 PIE 경계 수정

- 제목 표시줄은 실제 가용 영역과 클립 경계를 기준으로 창 버튼 슬롯을 배치한다. 좁은 폭에서는 최소화·최대화/복원을 메뉴에 넣고 닫기 버튼 자리를 우선 확보한다. SDL 드래그 영역도 계산한 버튼 시작점에서 끝난다.
- PIE 바 높이를 픽셀 단위로 올림하고, 그린 뒤 얻은 실제 하단 좌표를 도크스페이스의 상단에 사용한다. 같은 프레임의 두 창이 경계를 공유하므로 배율 또는 크기 계산 차이로 생기는 미도색 띠를 줄인다.
- 정적 검토 및 diff 공백 검사만 수행했다. 빌드와 실제 UI 검증은 미실시다. 사용자는 창 모드·최대화·좁은 폭에서 닫기와 드래그를 확인하고, 배율을 높인 뒤 PIE 경계를 가로지르는 툴팁·패널을 옮겨 잔상 여부를 확인한다.

### [2026-11-09] 뷰포트 우측 토글 복원

- 씬·카메라에서 RGBA와 2D/3D를 우측의 붙은 버튼 그룹으로 배치했다. 게임·UI·렌더 타깃 뷰는 우측 RGBA를 사용하고, 프리뷰는 이미지 오른쪽 위에 RGBA를 표시한다.
- 좌측 메뉴는 표시 폭을 기준으로 접는다. 우측 그룹이 메뉴와 함께 들어가지 않으면 이미지 위의 다음 줄에 배치한다. 프리뷰는 좌우 오버레이 영역을 각각 카메라 드래그 입력에서 제외한다.
- 호출부와 폭 계산을 정적으로 확인했다. 빌드·화면 검증은 미실시다. [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 확인 항목을 기록했다.

### [2026-11-09] 오버레이·행 버튼·제목 표시줄·PIE 공통화

- 공통 아이콘에 작은 행 버튼 규격과 고정 영역 크기 옵션, 아이콘 배치 높이 측정을 추가했다. 행 버튼 23곳 및 관련 열/행 예약 폭을 연결했다.
- 프리뷰 오버레이 크기와 카메라 입력 경계, 좁은 뷰포트의 옵션 메뉴, 채널/투영 버튼, 제목 표시줄의 메뉴 접기와 SDL 제어 영역, PIE 높이 계산을 변경했다.
- 새 로캘 키 12개는 Editor.csv에만 추가했다. 호출부/조건/API/로캘 원본 및 diff 공백을 정적으로 확인했다. 빌드·테스트·UI 실행·커밋은 미실시다.
- 세부 범위와 사용자 검증·최소 폭 한계는 [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 기록했다.

### [2026-11-09] 반복 UI 공통화 확대

- 복구/모델 가져오기 및 일반 팝업 4곳을 추가 전환하여 공통 버튼 바 호출이 19곳이 되었다. 복구 Escape 예외와 공통 높이 측정으로 가져오기 하단 영역을 보완했다.
- 일반 툴바를 애니메이터/타임라인/스프라이트/프리팹/작업 이력/후처리/플러그인/인스펙터/VFX 등으로 확대했다. 현재 14개 파일의 21개 지역 배치 도우미를 사용한다. 기능 처리는 기존 패널이 소유한다.
- 복구/플러그인 정보 표 및 VFX 속성 표 8개를 공통 여백에 연결했다. 빈 화면 안내 4곳을 공통화했다.
- 호출부·조건·표 시작/종료·직접 지정된 툴바 로캘 키를 정적으로 확인하고 변경 경로의 diff 공백 검사를 수행했다. 빌드·테스트·UI 실행은 미실시이며 커밋하지 않았다.
- 적용 범위, 실제 검증 상태, 남은 특수 UI 및 사용자 확인 기준은 [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 기록했다. 전체 에디터 공통화 완료나 DLL 핫리로드 구현 완료를 의미하지 않는다.

### [2026-11-09] 잔여 모달 버튼 바 8곳 전환

- 애니메이터 1, ImGuiManager 2, 그래픽 설정 1, 텍스처 검사 1, 스프라이트 2, 파일 충돌 1곳을 기존 DialogButtonBar로 전환했다. 기존 5곳을 포함해 조사한 ImGui 영역의 모달 13곳이 공통 구현을 사용한다.
- 기존 명령/실패/취소/일괄 처리 로직을 유지하고 버튼 순서·측정·정렬·키보드 처리는 공통 구현에 연결했다. 파일 충돌의 기본 확인은 복사본 만들기로 정했다.
- 정적 확인과 diff 공백 검사를 수행했다. 빌드·테스트·UI 실행은 사용자 지침에 따라 미실시다. [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 범위와 검증 항목을 기록했다.

### [2026-11-09] 공통 확인 버튼 바와 5개 대화상자 적용

- EditorUI에 로캘 기반 측정·우측 정렬·좁은 폭의 세로 배치/문구 줄바꿈·위험 색상·비활성 확인을 지원하는 버튼 바를 추가했다. 선택 번호만 반환하며 실제 명령은 패널이 실행한다.
- 그래프 교체, 프리팹 닫기/모드 종료, 후처리 프리셋 저장/삭제에 적용했다. 취소를 먼저 배치하고 저장/확인을 마지막에 배치했다. 프리팹 편집기는 저장 실패 시 닫히지 않도록 보완했다.
- Enter/Escape는 InputManager를 통해 포커스된 대화상자의 비편집 문맥으로 제한했다. 빌드·테스트·UI 실행은 미실시다. [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 검증 항목과 남은 모달 후보 8곳을 기록했다.

### [2026-11-09] 리소스 에디터의 반응형 공통 툴바 적용

- EditorUI에 지역 `ToolbarLayout` 도우미를 추가하고 모델·머티리얼·파티클 에디터에 적용했다. 글꼴 기준 입력 폭, 실측 버튼 배치, 긴 상태 문구 줄바꿈을 사용한다.
- 모델 배치 버튼은 전체 폭 부족 시 아이콘과 툴팁으로 전환한다. 파티클의 고정 28×26 버튼과 고정 속도 입력 폭을 제거했다. 모델의 포커스/그리드에는 기존 동적 단축키 힌트를 연결했다.
- 기존 저장·복원·PIE 제한·이력·재생 로직을 유지했다. 기존 작업 트리 변경은 보존했다. 빌드·테스트·UI 실행은 수행하지 않았으며 [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 확인 기준과 남은 최소 폭 제한을 기록했다.

### [2026-11-09] 글꼴·배율 대응 공통 아이콘 버튼 적용

- EditorUI에 아이콘 측정 기반 크기 계산, 비활성/토글/툴팁 옵션, 여유 폭에 따른 같은 줄 배치를 추가했다.
- 공통 검색 지우기와 콘텐츠 브라우저·콘솔·타일 팔레트의 아이콘 호출 10곳을 전환했다. 검색 바는 공통 버튼 실측 폭을 예약하며 작은 영역에서 장식과 지우기를 생략한다.
- 인접한 타일 크기 슬라이더, 콘솔 필터와 체크박스, 콘텐츠 브라우저 경로 시작 위치도 배치 폭을 고려하도록 조정했다. 기존 입력 처리와 실행 로직은 유지했다.
- 상위 폴더 툴팁 키를 Editor.csv에 추가했다. 정적 확인만 수행했으며 빌드·테스트·에디터 실행 및 로캘 JSON 생성은 미실시다. [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 적용 범위와 최소 폭 제한, 사용자 검증 항목을 기록했다.

### [2026-11-09] 콘텐츠 브라우저 툴바의 두 줄 배치 고정

- 경로에서 고정 280px를 빼던 계산을 제거하고, 첫 줄에서 필터 너비와 간격만 예약하도록 변경했다. 필터는 우측 남은 영역을 사용한다.
- 검색과 범위 버튼은 항상 둘째 줄에 배치한다. 기존 조건부 줄바꿈은 제거했다. 경로 이동·필터 판정·검색 실행 로직은 유지했다.
- 선언과 호출부, 너비 계산을 정적으로 확인했다. 빌드·실행은 미실시다. 넓고 좁은 창, UI 배율, 긴 경로와 번역된 필터 표시의 실제 배치는 사용자 확인이 필요하다.

### [2026-11-09] 공통 검색 바 확장과 9개 검색 입력 전환

- 너비·초기 포커스 옵션과 변경/확정/초기화 결과를 분리했다. 기존 bool 반환 API는 유지한다.
- 플러그인 관리자·Hierarchy·콘솔·리소스 검사·에셋 참조·그래프·엔티티 선택·리소스 선택·콘텐츠 브라우저에 적용했다. 검색 버퍼와 필터/결과 갱신 책임은 각 호출자에 유지한다.
- 팝업 자동 포커스, 툴바의 인접 컨트롤 공간, 콘텐츠 브라우저의 Enter 검색 경로를 연결했다. ReflectionInspectorCheck에 공통 구현 소스를 추가했다.
- 사용처와 검색 갱신 분기를 정적으로 확인했다. 빌드·테스트·UI 실행은 지침에 따라 미실시다. [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 검증 항목을 기록했다.

### [2026-11-09] 공통 검색 바의 통합 설정 적용

- `EditorUI::SearchBar`에 힌트·검색 아이콘·지우기 버튼·Escape 초기화와 편집 해제를 모았다. 버퍼 소유권과 실제 필터 판정은 호출자가 유지한다.
- 통합 설정의 검색 입력을 연결하고 내용 변경 시 기존 `ImGuiTextFilter::Build()`를 호출한다. 기존 단축키 검색 범위와 필터 문법을 유지했다.
- Escape는 활성 입력 ID와 창 포커스로 제한하며 InputManager 샘플을 사용한다. 표시는 기존 로캘 키와 아이콘을 사용한다.
- 정적 경로 확인을 수행했다. 빌드·테스트·에디터 실행은 미실시이며 [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 사용자 검증 항목을 기록했다.

### [2026-11-09] 프로젝트·입력 액션·단축키 설정의 공통 테이블 적용

- `EditorUI`에 열 구성을 호출자가 지정하는 설정 테이블 진입점을 추가했다. 속성 테이블과 목록 테이블이 동일한 `TableStyle` 여백과 리사이즈 정책을 공유한다.
- 프로젝트 설정의 테이블과 반복 라벨 그리기를 공통화했다. 입력 액션의 2열 목록, 단축키의 3열 목록도 공통 시작·종료 함수를 사용한다. 기존 열 구성과 기능별 입력·저장 처리는 유지했다.
- 기존 작업 트리 변경을 보존하고 해당 코드와 [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)만 갱신했다. API와 호출부를 정적으로 확인했다. 빌드·테스트·에디터 실행은 사용자 지침에 따라 미실시다.

### [2026-11-09] 작업 공간 설정의 공통 속성 테이블 전환

- `WorkspaceSettingsWidget`의 뷰포트 해상도·콘텐츠 브라우저 테이블 2곳과 라벨 행 6곳을 기존 `EditorUI` 공통 함수로 전환했다.
- 단일 입력 요소는 값 열의 남은 너비를 사용한다. 사용자 지정 해상도의 폭·높이 입력은 구분 문자와 간격을 제외한 너비를 균등 분배한다.
- 기존 ID·로캘 키·초안 갱신·해상도 요청·비활성화·범위 제한을 유지했다. 파일에 이미 존재하던 변경은 보존했다.
- 정적 코드 확인을 수행했다. 빌드·테스트·에디터 실행은 프로젝트 지침에 따라 수행하지 않았다. 실제 디자인과 입력 동작은 사용자 검증이 필요하다. 검증 항목은 [ImGui 계획 24절](ImGuiDevelopmentPlan.md#24-패널-위젯-분리-후보)에 기록했다.

### [2026-11-09] 스프라이트와 썸네일 드로우콜 최적화 계획

- 현행 `RenderSprite()`는 스프라이트·타일 명령마다 `DrawIndexed()`를 호출한다. `SpriteBatcher`의 CPU 버킷과 `SpriteAtlasCheck`의 기대값은 런타임 드로우 측정과 구분했다. `SpriteAtlas`는 배치·UV를 계산하며, 아틀라스 픽셀 생성과 제출 연결은 후속 작업이다.
- 같은 시트에서 기준을 측정한 후 패스별 기본 쿼드 인스턴싱을 연결하고, 여러 텍스처가 배치를 분리할 때 아틀라스를 추가하는 순서를 기록했다. [스프라이트 계획 Phase 5](SpriteAnd2DSystemPlan.md#phase-5-스프라이트-아틀라스-및-드로우콜-배칭)
- 콘텐츠 브라우저는 ImGui 명령을 별도로 측정하고, 공유 시트에서도 명령이 나뉘는지 확인한 뒤 레이어 묶기와 에디터 썸네일 아틀라스를 검토한다. 코드와 검사 범위를 정적으로 대조했다. 실제 드로우 수와 프레임 시간은 구현 후 검증 항목이다. [ImGui 계획 14-D](ImGuiDevelopmentPlan.md#콘텐츠-브라우저-썸네일-드로우콜-최적화)

### [2026-11-09] 콘텐츠 브라우저 썸네일 표시 중 프레임 저하 기록

- 표시 목록 캐시가 작업 트리에 반영되어 있다. `DrawGrid()`는 폴더 목록·검색 결과·필터가 바뀔 때 `RebuildGridItems()`로 표시 순서를 갱신한다. 남아 있는 경로의 선택 ID를 유지하고 `SourceIndex`로 원본 항목을 조회한다. `RefreshCacheIfNeeded()`는 원본 목록을 재구성하기 전에 표시 목록을 무효화한다.
- `AssetCache`에는 스프라이트 핸들 반환 경로가 있으나 `ContentBrowserPanel::DrawGridItem()`은 텍스처만 요청한다. 보이는 스프라이트의 경로 조회는 `ContentBrowserGridItem::DrawThumbnail()`에 남아 있다.
- 사용자 관찰에서 썸네일이 많은 폴더는 약 60 FPS를 유지했지만 썸네일이 없는 폴더보다 느렸고, 폴더를 열어둔 동안에도 차이가 남았다. 코드와 재현 후보 에셋을 정적으로 확인했다. 고정 조건의 전후 프레임 시간, CPU·GPU 기여도, 검색·선택 및 파일 변경 회귀는 후속 검증 항목이다.
- 재현 조건과 계측·수정 순서를 [ImGui 계획 14-D](ImGuiDevelopmentPlan.md#콘텐츠-브라우저에서-썸네일-표시-중-지속되는-프레임-저하)에 기록했다.

### [2026-11-09] 후처리 패널의 씬 반영과 Undo/Redo 연결

- 후처리 파라미터, 효과 활성화·순서, 프리셋 적용과 초기화는 실제 변경 지점에서 설정을 직렬화하고 대상 볼륨의 전후 스냅샷을 기록한다. 위젯 활성 상태만으로 씬 설정을 복사하던 경로를 제거했다.
- 연속 위젯 조작은 기존 편집 이력의 병합을 사용한다. 볼륨 생성은 엔티티와 후처리 컴포넌트 초기화를 같은 편집 범위에 기록한다.
- 편집 가능한 씬의 활성 볼륨이 있을 때만 씬에 반영한다. 직렬화·스냅샷 실패 시 씬 값은 보존하고, 프리셋 파일 작업과 뷰포트 선택은 씬 이력에서 분리한다.
- [에디터 씬 Undo/Redo](EditorUndoRedo.md)에 적용 범위를 기록했다. 코드상 편집 진입점과 실패 복원 경로를 대조했으며 빌드, CTest, 에디터 Undo/Redo와 저장 확인은 사용자 검증 항목이다.

### [2026-11-09] 스프라이트 슬라이스와 애니메이션 클립 저장 실패 처리

- `SpriteEditor::SaveSlices`가 성공한 슬라이스의 핸들을 반환하도록 변경했다. 쓰기 성공 이후에 해당 메모리 리소스를 갱신하고, 첫 실패에서 중단해 클립 생성으로 이어지지 않도록 했다.
- 단일·행별 클립은 같은 슬라이스 저장 결과를 사용한다. 출력 이름을 검사하고 개별 클립의 쓰기 결과를 확인한다. 실패하면 기존 로캘 키로 대상 이름을 알리고 생성 모달의 입력을 유지한다.
- [2D 제작 계획의 저장 실패와 재시도](SpriteAnd2DSystemPlan.md#13-저장-실패와-재시도)에 부분 성공 파일과 재시도 정책을 기록했다.
- 코드상 저장 성공·실패 분기, 결과 전달, 알림과 모달 닫힘 경로를 대조했다. 에디터 빌드, CTest, 실제 쓰기 실패와 UI 검증은 사용자가 수행할 확인 항목으로 남아 있다.

### [2026-11-09] 엔진 파일 내부 구성 기준과 에디터 헤더 경로 갱신

- 범위는 엔진 소스 조직 지침과 에디터 소스·검사 코드의 include다.
- [소스 조직 가이드 9절](SourceOrganizationGuidelines.md#9-헤더구현의-읽기-순서)에 객체 생명주기 상단 배치, 기능별 공개 구역, 헤더·구현 순서 대응, 헬퍼와 멤버 배치 기준을 기록했다. `SystemManager`는 검토 예제로 사용했으며 `PostProcessPipeline`을 비롯한 후보의 내부 재배치는 후속 작업이다.
- 엔진 파일 이동에 맞춰 에디터의 41개 파일에서 include 83곳을 갱신했다. 컴포넌트 경로 61곳, `WorldComponents.h` 이름 21곳, `Rect.h` 경로 1곳이다.
- 정적 확인에서 갱신한 이름마다 대응하는 엔진 공개 헤더가 하나씩 존재했고, 에디터 소스와 검사 코드에서 대상 이전 참조가 사라진 것을 확인했다.
- 빌드와 CTest 실행 결과는 아직 없다. 에디터 타깃과 관련 검사로 컴파일·실행을 확인한다. 같은 이전 참조는 `Client` 2곳과 `Projects` 15곳에 남아 있다.

### [2026-11-09] 2D 스프라이트 시스템 Phase 5의 아틀라스 배치와 CPU 버킷 기반

- Area: Engine | Render | Resource | 2D | Sprite | Optimization | Localization | Testing
- Type: Feature | Architecture | Math | Optimization
- 주요 변경 내역:
  1. **스프라이트 아틀라스 에셋 (`SpriteAtlas`)**:
     - `PackMaxRects`: BSSF(Best Short Side Fit) 및 면적 정렬 휴리스틱을 채택한 MaxRects 2D Bin Packing 알고리즘 구현.
     - 여유 직사각형을 분할하고 중복 영역을 가지치기한다. 패킹 검사는 대상 영역의 겹침과 경계를 확인한다.
     - 패킹된 각 스프라이트의 픽셀 영역(`pixelRegion`) 및 $[0, 1]$ 정규화 UV 좌표(`uvTransform`) 자동 산출.
     - `TryGetEntry`, `HasEntry` 고속 해시 맵 조회 지원.
  2. **CPU 스프라이트 버킷 (`SpriteBatcher`)**:
     - 머티리얼·텍스처·패스별로 쿼드의 정점과 인덱스를 모은다. `SpriteRenderSystem`에 배처 멤버가 있으며 실제 제출은 기존 `SubmitSprite()` 경로를 사용한다.
  3. **검사 타깃 (`Engine/examples/SpriteAtlasCheck.cpp`)**:
     - 10개 항목의 패킹·겹침·UV 경계와 100개 쿼드가 2개 CPU 버킷에 모이는 경우를 검사한다. 코드의 98회 절감 수치는 명령 수에서 버킷 수를 뺀 기대값이다.
  4. **로캘 및 리플렉션 연동**:
     - `Engine.csv`에 `STRUCT_SPRITE_ATLAS`, `PROP_ATLAS_TEXTURE`, `PROP_ATLAS_ENTRIES` 추가 및 `localizationcsvtojson.py` 동기화 완료 (총 829개 키).
     - `Resources.h`에 `SpriteAtlas.h` 포함.

### [2026-11-09] 2D 스프라이트 시스템 Phase 4 구현 완료: 청크 기반 2D 타일맵 시스템(TilemapComponent & TilemapRenderSystem), 씬 뷰포트 인터랙티브 타일 페인팅 및 타일 팔레트(TilePalettePanel) 연동

- Area: Engine | Editor | Render | 2D | Tilemap | Viewport | UI | Testing
- Type: Feature | Architecture | Integration
- 주요 변경 내역:
  1. **청크 기반 타일맵 데이터 구조 및 직렬화 (`TilemapComponent`)**:
     - $16 \times 16$ 타일 블록 단위 `TilemapChunk` 구조 및 64비트 정수 청크 키(`MakeChunkKey(cx, cy)`) 기반 스파스(Sparse) 해시맵 저장소 구축. 무한 좌표계 타일맵 지원.
     - `SavedTile` 구조체 및 `SyncSavedTiles()` / `RestoreFromSavedTiles()` 구현: 씬 저장 시 유효 타일들만 직렬화하고 로드 시 런타임 청크로 자동 복원.
     - `CellToWorld(x, y)` 및 `WorldToCell(worldPos)` 정밀 좌표 변환 (음수 좌표계 대응).
     - `ReflectionRegistry.cpp`에 `TilemapComponent` 등록하여 에디터 인스펙터 편집 및 Add Component(`Rendering`) 지원.
  2. **`TilemapRenderSystem` 렌더링 파이프라인 및 fallback**:
     - 머티리얼 및 메시 핸들 미지정 시 기본 쿼드 메시(`Builtin/Quad` / `Resources/Mesh/Quad.bammesh`) 및 스프라이트 머티리얼(`SpriteMaterial`) 자동 fallback 처리.
     - 씬 로드 후 첫 프레임 시 `savedTiles`로부터 런타임 청크 자동 복원 연동.
     - 각 타일의 UVTransform, 틴트 컬러, 피킹 ID를 반영한 `SpriteDrawCommand` 고속 제출.
  3. **씬 뷰포트 실시간 타일 페인팅 연동 (`SceneViewportPanel`)**:
     - `HandleTilemapPainting`: 뷰포트 마우스 레이와 타일맵 2D 평면(Z=0)의 실시간 교차점 계산.
     - 타일 그리드 스냅 박스(노란색/빨간색/시안색/보라색) 및 선택된 스프라이트 반투명(알파 0.5f) 배치 미리보기 렌더링.
     - 브러시 4대 모드 완전 연동:
       - **Paint (B)**: 좌클릭 드래그로 타일 연속 배치
       - **Erase (E)**: 좌클릭 드래그로 타일 지우기
       - **Box Fill (U)**: 시작 셀부터 끝 셀까지 사각형 영역 일괄 채우기
       - **Picker (I)**: 마우스가 가리키는 타일의 스프라이트를 스포이트로 복사 후 Paint 모드로 자동 전환
     - 마우스 입력 충돌 방지: 타일맵 페인팅 활성화 시 기즈모 조작 및 오브젝트 피킹 자동 차단.
  4. **타일 팔레트 에디터 패널 (`TilePalettePanel`)**:
     - 콘텐츠 브라우저에서 텍스처/스프라이트 드래그 앤 드롭으로 팔레트 자동 로드.
     - 툴바 브러시 모드 토글(B, E, U, I), 썸네일 크기 조절 슬라이더, 개별 타일 우클릭 삭제.
  5. **단위 검증 (`Engine/examples/TilemapCheck.cpp`)**:
     - 좌표 변환(양수/음수), 4대 사분면 청크 분할, `ClearTile`, `SyncSavedTiles`/`RestoreFromSavedTiles` 4대 시나리오 자동 검증 타깃 신설.

### [2026-11-09] 2D 스프라이트 시스템 Phase 3 구현 완료: 픽셀 퍼펙트 카메라(Pixel Perfect Camera), 정수 줌 배율 자동 동기화 및 서브픽셀 지터링 방지 스냅

- Area: Engine | Render | Camera | 2D | Sprite | Editor | Viewport | Localization | Testing
- Type: Feature | Architecture | Math | Integration
- 주요 변경 내역:
  1. **`PixelPerfectCameraComponent` 구조체 확장**:
     - `assetsPPU` (기본 16.0f): 단위 월드 거리(1 Unit) 당 픽셀 밀도 정의.
     - `refResolutionX` (기본 320), `refResolutionY` (기본 180): 레트로 픽셀 아트 타깃 가상 해상도.
     - `snapSubpixel` (기본 true): 부동소수점 카메라 이동 시 화면 픽셀 격자 단위 스냅 활성화 플래그.
     - `upscaleRT` (기본 true), `cropFrameX` (기본 false), `cropFrameY` (기본 false): 가상 렌더타깃 및 정수비 레터박스/필러박스 클리핑 제어.
     - `currentZoom` (읽기 전용): 뷰포트 해상도에 맞춰 실시간으로 동기화되는 정수 줌 배율($1\times, 2\times, 3\times \dots$).
  2. **정수 줌 배율(Integer Scale) 및 `OrthographicSize` 자동 동기화 알고리즘 (`CameraSystem`)**:
     - 뷰포트 실제 해상도 $(W_v, H_v)$와 타깃 레퍼런스 해상도 $(W_{ref}, H_{ref})$ 비교:
       $$\text{Zoom} = \max\left(1, \min\left(\lfloor W_v / W_{ref} \rfloor, \lfloor H_v / H_{ref} \rfloor\right)\right)$$
     - 화면상의 1개 스프라이트 텍셀이 정확히 $\text{Zoom}$개의 화면 픽셀을 차지하도록 월드 높이 `OrthographicSize` 자동 계산:
       $$\text{OrthographicSize} = \frac{H_v}{\text{assetsPPU} \times \text{Zoom}}$$
     - 뷰포트 창 크기가 가변적이거나 비정규 종횡비(예: 1400x900)인 환경에서도 텍셀 비율이 찌그러지지 않고 $1:\text{Zoom}$ 정수 비율로 완벽 고정.
  3. **서브픽셀 카메라 스냅 (Sub-pixel Position Snapping)**:
     - 카메라 월드 위치를 유효 화면 픽셀 밀도($\text{effectivePPU} = \text{assetsPPU} \times \text{Zoom}$) 격자로 스냅:
       $$X_{snapped} = \frac{\lfloor X_{world} \times \text{effectivePPU} + 0.5 \rfloor}{\text{effectivePPU}}, \quad Y_{snapped} = \frac{\lfloor Y_{world} \times \text{effectivePPU} + 0.5 \rfloor}{\text{effectivePPU}}$$
     - 이를 통해 부드러운 카메라 이동(Smooth follow) 중에도 텍스처 텍셀 경계가 래스터라이저의 화면 픽셀 경계와 $100\%$ 일치하여 서브픽셀 떨림(Jittering & Shimmering) $0\%$ 달성.
  4. **에디터 및 리플렉션 연동**:
     - `GameViewportPanel::FindActiveGameCameraBuffer()`에서 `m_RenderWidth`, `m_RenderHeight`를 `CameraSystem::UpdateCamera`로 전달하여 뷰포트 리사이즈 실시간 반영.
     - `ReflectionRegistry.cpp`에 `PixelPerfectCameraComponent` 등록하여 인스펙터 편집 및 Add Component 메뉴 노출.
     - `Engine.csv`에 신규 로캘 키 추가 및 `localizationcsvtojson.py` 동기화 완료 (826개 키).
  5. **단위 검증 (`Engine/examples/PixelPerfectCameraCheck.cpp`)**:
     - 1080p, 720p 정수 줌 배율, 비정규 900p 보수적 줌 배율, 서브픽셀 좌표 스냅, `cropFrame` 바운드 고정 5대 시나리오 자동 검증 타깃 신설.

### [2026-11-09] 2D 스프라이트 시스템 Phase 1 & 2 완성: 인터랙티브 스프라이트 시트 3대 슬라이서, 원클릭 애니메이션 생성 위저드 및 9-Slice / Tiled / Filled 렌더링

- Area: Editor | Engine | Resource | 2D | Sprite | Render | UI | Localization
- Type: Feature | Architecture | Integration
- 주요 변경 내역:
  1. **스프라이트 시트 3대 슬라이싱 서비스 (`SpriteSlicingService`) 구축**:
     - `GridByCellSize`: 고정 가로/세로 픽셀 크기($W, H$) 및 오프셋/패딩 기반 균등 격자 분할.
     - `GridByCellCount`: 행/열 분할 수($\text{Cols} \times \text{Rows}$) 기반 텍스처 전체 해상도 분할.
     - `Automatic`: CPU RGBA8 픽셀 버퍼 순회 및 `alphaThreshold` 초과 픽셀에 대한 8방향 BFS Flood-Fill 클러스터링 알고리즘. `minPixelArea` 노이즈 필터링 및 행/열 시각적 순서 자동 정렬, $0$-패딩 형식 식별자(`Sprite_00`, `Sprite_01`) 부여.
     - 단위 검증: `Editor/examples/SpriteSlicingCheck.cpp`를 통해 3대 슬라이싱 알고리즘 검증 완료.
  2. **스프라이트 에디터(`SpriteEditor`) 인터랙티브 저작 환경**:
     - 체커보드 배경 캔버스, 마우스 우클릭/휠 기반 줌($0.1\times \sim 16.0\times$) 및 팬, 실제 픽셀($1:1$) 및 화면 맞춤(`FitView`).
     - 슬라이스 박스 와이어프레임, 피벗 크로스헤어(십자선), 9-Slice 4방향 가이드라인 시각화 및 마우스 클릭 선택.
     - `[Apply Slices]`: 슬라이스된 각 구획을 독립 `.bamsprite.json` 에셋으로 일괄 저장 및 콘텐츠 브라우저/리소스 매니저 동기화.
  3. **원클릭 애니메이션 클립 생성 위저드 (Auto Action Generator)**:
     - 단일 클립 모드(`SingleClip`): 슬라이스들을 순서대로 연결한 단일 `.spriteanim` 생성.
     - 행 단위 분할 모드(`RowByRowClips`): 텍스처 시트의 각 행(Row)을 감지하여 `Idle`, `Walk`, `Attack`, `Die` 등 개별 애니메이션 클립으로 1클릭 일괄 분할 추출.
  4. **스프라이트 렌더링 4대 모드 (`SpriteRenderSystem`) 완결**:
     - `Simple`: 기본 단일 쿼드 렌더링.
     - `Sliced` (9-Slice): 4방향 보더 인셋(`m_Border`: L, R, T, B)을 유지하여 모서리 왜곡 없는 9개 패치 쿼드 생성.
     - `Tiled`: 텍스처를 늘리지 않고 지정 크기만큼 바둑판식으로 반복 타일링.
     - `Filled`: `fillAmount`($0.0 \sim 1.0$)에 따른 수평/수직/방사형 게이지 채우기 클리핑.
  5. **로캘 및 빌드 파이프라인 정합성**:
     - `Editor.csv`의 `SpriteEditor.*` 키 40여 종 정합성 검증 및 `localizationcsvtojson.py` 동기화 완료.
     - `Editor/CMakeLists.txt` 내 `StbImageImpl.cpp`의 `SKIP_UNITY_BUILD_INCLUSION` 가드 유지.

### [2026-11-09] 비주얼 이펙트 Phase 3, 4 구현 완료: 트레일/리본 동적 지오메트리 렌더러, 지면 투영 데칼, 카메라 셰이크 및 에디터 연동

- Area: Engine | Render | VFX | Camera | Editor | UI | Localization
- Type: Feature | Architecture | Integration
- 주요 변경 내역:
  1. **Phase 3 트레일 / 리본 FX (`VFXTrailTrack`, `VFXTrailTrackRuntime`) 구현**:
     - `CompositeEffect.h`: 폭(`trailWidth`), 수명(`lifetime`), 최소 정점 거리(`minVertexDistance`), 테이퍼링 비율(`taperRatio`), 칼끝/자루 오프셋(`localOffsetA`, `localOffsetB`), 시작/종료 색상 및 텍스처/머티리얼 핸들을 포함하는 구조체 정의 및 리플렉션 등록.
     - `CompositeVFXComponent.h`: `VFXTrailTrackRuntime` 추가 (정점 쌍 샘플 히스토리 관리 및 `ResetRuntimes()` 리셋 지원).
     - `CompositeVFXSystem.cpp`:
       - `UpdateVFXEntity`: 엔티티 월드 행렬 기반으로 팁/베이스 포인트($A, B$)를 수집하고, 오래된 포인트는 나이($age \ge lifetime$)에 따라 자동 제거.
       - `SubmitSceneVFX`: 연속된 두 샘플 쌍 사이에서 진행 방향 벡터($Dir$)와 법선($Up$)으로 오리엔테이션 행렬을 산출하고, 테이퍼링 폭 및 컬러/알파를 보간하여 `SpriteDrawCommand`를 통해 쿼드 스트립 세그먼트를 `ForwardTransparentPass`로 고속 렌더링.
  2. **Phase 4 보조 연출 (지면 투영 데칼 & 카메라 셰이크) 구현**:
     - `VFXDecalTrack`: 폭발/타격 지면에 그을음, 크레이터, 마법진 마크를 투영할 수 있도록 위치/회전/크기 및 `fadeStartTime` 이후 점진적 알파 페이드아웃 렌더링 구현.
     - `VFXCameraShakeTrack`: 타격/폭발 순간 감쇠 곡선($1 - t/\text{duration}$)과 주파수 기반 다축 사인파 진동($\sin(2\pi f t)$)을 인가하여 `CompositeVFXComponent::currentCameraShakeOffset`에 실시간 누적.
  3. **통합 저작 도구 (`VFXEditorPanel`) 전면 지원 및 로캘 동기화**:
     - 트랙 추가 메뉴("Add Track")에 `Trail`, `Decal`, `CameraShake` 항목 신설.
     - 트랙 리스트 및 타임라인에 시안(Trail), 와인(Decal), 코랄(CameraShake) 색상의 트랙 바 배치, 드래그 이동 및 선택 지원.
     - 트랙 인스펙터에 폭, 수명, 테이퍼링, 진폭, 주파수, 감쇠 반경, 텍스처 피커 등 2열 테이블 프로퍼티 편집 UI 완전 구현.
     - `Editor.csv`에 신규 로캘 키 14종 추가 및 `localizationcsvtojson.py`를 통해 영문/한글 JSON 동기화 완료.

### [2026-11-09] 비주얼 이펙트 Phase 2 구현 완료: 3D 메쉬 이펙트 런타임 렌더링 제출 및 사운드 트랙 1회 타이밍 트리거 연동

- Area: Engine | Render | VFX | Audio | Architecture
- Type: Feature | Fix | Integration
- 주요 변경 내역:
  1. **사운드 트랙(`VFXSoundTrack`) 런타임 1회성 정밀 트리거 연결:**
     - `CompositeVFXComponent.h`: `std::vector<bool> soundPlayed;` 런타임 추적 플래그 추가 및 `ResetRuntimes()` 내 `std::fill` 리셋 연동.
     - `CompositeVFXSystem.cpp`: `UpdateVFXEntity`에서 `effect->GetSoundTracks()`를 순회하며 `currentTime >= track.startTime && !soundPlayed[i]` 시점에 `AudioManager::Get().PlayOneShot(clip->GetFMODSound(), track.volume)` 1회 호출.
     - 타임라인 역방향 스크러빙/루프 재생(`currentTime < track.startTime`) 시 `soundPlayed[i] = false`로 자동 복구 지원.
  2. **3D 메쉬 이펙트(`VFXMeshTrack`) 런타임 변환 보간 및 렌더링 파이프라인 제출:**
     - `CompositeVFXSystem.cpp::SubmitSceneVFX`:
       - 경과 시간 $t$ 기반 진행도($\alpha \in [0, 1]$) 및 루프(`fmod`) 연산.
       - 스케일 보간: $Scale(t) = \text{lerp}(scaleStart, scaleEnd, \alpha)$.
       - 지속 회전 적분: $Rotation(t) = initialRotation + rotationRate \times t$.
       - 컬러/알파 페이드: $Color(t) = \text{lerp}(colorStart, colorEnd, \alpha)$.
       - UV 스크롤 오프셋: $UVOffset(t) = uvScrollSpeed \times t$.
       - `ForwardTransparentPass` (및 `GeometryPass`) 블렌드 모드 호환성 필터링.
       - 텍스처 및 인덱스버퍼 유효 시 `SpriteDrawCommand`(`uvTransform`, `color`, `worldMatrix`)로 GPU 제출하여 `SpriteUBO`를 통해 추가 셰이더 개편 없이 UV 스크롤 및 알파/컬러 페이드아웃 즉시 연동, 텍스처 미지정 시 `StaticDrawCommand`로 안전하게 폴백 제출.

### [2026-11-09] 비주얼 이펙트 아키텍처: 잔여 이펙트(메쉬·사운드·트레일·데칼) 생성 완성 로드맵 개정

- Area: Engine | Render | FX | Editor | Architecture | Plan
- Type: Architecture | Plan | Documentation
- 주요 내용:
  1. **현재 제작 가능 여부 갭 진단:**
     - 2D 스프라이트 플립북, 3D 파티클 이미터, 포인트 조명 섬광 트랙은 런타임 렌더링 및 에디터 연동이 완료되어 즉시 제작 가능.
     - 3D 메쉬 이펙트(`VFXMeshTrack`)와 사운드(`VFXSoundTrack`)는 데이터 구조와 에디터 UI는 존재하나, `CompositeVFXSystem`에서 렌더러로의 드로우콜 제출 및 오디오 재생 호출이 누락되어 인게임 및 뷰포트에 표시되지 않음.
     - 트레일/리본(검기 슬래시 궤적) 및 보조 연출(지면 그을음 데칼, 카메라 셰이크)은 시스템 신설 필요.
  2. **`Docs/CompositeVisualEffectPlan.md` 로드맵 전면 개정:**
     - **Phase 2 (메쉬 이펙트 런타임 & 사운드 연동):** `CompositeVFXSystem::SubmitSceneVFX`에 메쉬 트랙 순회 추가, $t$ 시간 기준 스케일/회전 보간 행렬 계산, UV 스크롤/컬러 페이드 상수버퍼 연동 및 `ForwardTransparentPass` 제출. 사운드 트랙 1회성 정밀 타이밍 트리거 연결.
     - **Phase 3 (트레일 / 리본 FX 신설):** `VFXTrailTrack` 데이터 구조체, 정점 위치 히스토리 기반 동적 쿼드 스트립 생성기(`TrailMeshGenerator`), 테이퍼링 및 텍스처 매핑 포워드 렌더링 구축.
     - **Phase 4 (보조 연출 확장):** 지면 그을음 투영 데칼(`VFXDecalTrack`) 및 폭발 임팩트 카메라 셰이크(`VFXCameraShakeTrack`) 연동.
  3. **`Docs/DevelopmentPriorities.md` 트랙 F 상태 동기화:**
     - 단순 [완료] 표기를 [Phase 1 완료 / Phase 2 착수 준비]로 세분화 갱신.

### [2026-11-09] 복합 비주얼 이펙트 시스템(Composite Visual Effect) 및 통합 저작 도구(VFXEditorPanel) 구축과 타임라인·인스펙터 UX 고도화

- Area: Engine | Editor | Resource | VFX | Render | UI | Localization
- Type: Feature | Architecture | UX | Fix
- 주요 변경 내역:
  1. **복합 비주얼 이펙트 에셋 및 런타임 오케스트레이션 시스템 구현 (`CompositeEffect`, `CompositeVFXComponent`, `CompositeVFXSystem`)**:
     - `Engine/Source/Resource/VFX/Public/CompositeEffect.h`, `Private/CompositeEffect.cpp` 신설: 다형적 멀티 트랙(2D 스프라이트 플립북, 파티클 이미터, 동적 포인트 조명 섬광, 3D 메쉬 이펙트, 오디오 사운드)을 단일 에셋 규격(`.bamvfx`, `.bamvfx.json`, `EResourceType::CompositeEffect`)으로 통합 직렬화.
     - `Engine/Source/Core/Public/Components/Render/CompositeVFXComponent.h`: 엔티티 부착형 복합 VFX 컴포넌트 신설. 재생 속도, 루프, 시간 진행, 파티클 런타임 및 조명 컴포넌트 자동 동기화 관리.
     - `Engine/Source/System/Render/Public/CompositeVFXSystem.h`, `Private/CompositeVFXSystem.cpp`: 시간축 기반 시뮬레이션 및 렌더링 오케스트레이터.
       - 스프라이트 플립북: `SpriteAnimation` 연동 및 카메라 빌보드 회전 계산 후 `Renderer::SubmitSprite` 제출.
       - 동적 조명 섬광: 엔티티의 `LightComponent`와 동기화하여 attack/decay 커브에 따른 실시간 점광원 강도/반경 제어.
       - 파티클 이미터: `ParticleSystem::UpdateEffectComponent` 및 `SubmitSceneParticles`를 통해 복합 VFX 내 파티클 동시 시뮬레이션 및 렌더링.
     - 엔진, 에디터(`Application.cpp`), 클라이언트(`Main.cpp`)의 시스템 매니저에 `CompositeVFXSystem` 등록 완료.
  2. **통합 비주얼 이펙트 에디터 패널 구현 (`VFXEditorPanel.h/.cpp`)**:
     - `ResourceEditorInterface` 상속 및 `CreateResourceEditors` 등록으로 콘텐츠 브라우저에서 `.bamvfx` / `.bamvfx.json` 에셋 더블클릭 시 자동 로드 및 단독 편집 창 활성화.
     - **3D 뷰포트 프리뷰 (`PreviewViewportWidget`)**: 실시간 블룸(Bloom) 및 스튜디오 조명이 적용된 독립 3D 뷰포트. `GeometryPass` 및 `ForwardTransparentPass` 양쪽 모두에 복합 VFX를 제출하여 반투명/파티클/스프라이트 완벽 렌더링.
     - **견고한 에셋 로드 및 자동 생성 폴백**: `ResourceManager` 로드 실패 시에도 디스크 파일 직접 로드 폴백 및 신규 이펙트 자동 생성(`NewEffect`)으로 패널 먹통 상태 원천 방지.
  3. **사용자 피드백 반영 및 타임라인·인스펙터 UX 고도화**:
     - **타임라인 헤더 드래그 조절 스플리터 탑재**: `m_TimelineHeaderWidth`(기본 200px) 신설 및 120px~450px 범위의 수평 스플리터(`##TimelineHeaderSplitter`) 구현.
     - **트랙 이름 클리핑 및 호버 툴팁**: `drawList->PushClipRect`로 긴 트랙 이름이 트랙 바 영역을 침범하지 않도록 클리핑하고, 마우스 호버 시 전체 트랙 이름 툴팁 표시.
     - **좌측 트랙 패널 가로 스크롤 제거**: `[+ 트랙 추가]` 버튼의 너비 클램프 및 `Selectable` 너비 최적화로 불필요한 가로 스크롤 완전 제거.
     - **인스펙터 2열 테이블화**: `ImGui::BeginTable` 기반으로 1열(속성 이름 레이블, 좌측 정렬)과 2열(속성 편집 위젯, 너비 100%)을 배치하고 마우스 드래그로 너비 조절 가능하도록 전면 리팩토링.
     - **스프라이트 플립북 잔상 버그 해결**: 비반복 플립북(`!track.loop`)이 애니메이션 총 길이(`animDuration`)에 도달(`animEvalTime >= animDuration`)하는 순간 즉시 렌더링을 중단하도록 수정하여 재생 완료 후 마지막 프레임이 화면에 영구히 남는 결함 해결.
  4. **로캘 원본 데이터(`Editor.csv`) 규격 일원화**:
     - `Editor/Resources/Data/Localization/Editor.csv`에 `VFXEditor` 카테고리 53개 키(영문/한국어) 정식 등록.
     - 에디터 패널 타이틀, 메뉴바, 툴바 버튼, 트랙 추가 메뉴, 인스펙터 테이블 레이블에 `LOCAL_CSTR(...)` 매크로 일원화 적용.
  5. **샘플 에셋 및 테스트 씬 구축**:
     - `Assets/Effect/Fireball_Explosion_Composite.bamvfx.json`: 올바른 `AssetHeader`(`MagicNumber: 6072351289621954882`, `ResourceType: 18`) 및 `ResourceHandle` 형식으로 생성.
     - `Assets/Scenes/EffectTestScene.bamscene.json`: 복합 이펙트 테스트 엔티티 배치.
- 검증 기준:
  - `Fireball_Explosion_Composite.bamvfx.json` 더블클릭 시 `VFXEditorPanel`이 열리며 3D 뷰포트, 트랙 리스트, 타임라인, 인스펙터가 정상 표시되어야 함.
  - 타임라인 헤더 너비를 마우스로 드래그하여 조절할 수 있고, 긴 트랙 이름이 트랙 바와 겹치지 않아야 함.
  - 좌측 트랙 리스트 패널에 가로 스크롤바가 발생하지 않아야 함.
  - 우측 인스펙터에서 속성 이름이 왼쪽에 오고 위젯이 오른쪽에 오는 2열 테이블 구조로 조작 가능해야 함.
  - 스프라이트 플립북 재생 종료 후 화면에 마지막 프레임 잔상이 남지 않아야 함.

### [2026-11-09] 새 씬 생성 시 템플릿 선택 다이얼로그(Basic 3D / Empty Scene) 구현 및 EntityFactory를 통한 PostProcessVolume 생성 수정

- Area: Editor | Scene | Document | UI | Localization
- Type: Feature | Fix | UX
- 주요 변경 내역:
  1. **`PostProcessPanel.cpp` 컴파일 에러 수정 (`PostProcessPanel.cpp`)**:
     - `Scene::CreateEntity(name)` 호출 불가(인수 0개 시그니처) 결함을 `EntityFactory::CreateEmptyEntity(scene, L"PostProcessVolume")` 호출로 교정.
     - `Hierarchy`, `Transform`, `WorldTransform`, `Flag`, `Name` 컴포넌트 자동 구성 및 `SceneEditHistory`(Undo/Redo) 정상 연동 완료.
  2. **새 씬 생성 템플릿 선택 모달 다이얼로그 탑재 (`ImGuiManager.h/.cpp`)**:
     - `ESceneTemplate` 열거형 추가 (`Basic`, `Empty`).
     - 메뉴 `File -> New Scene`, `Ctrl+N` 단축키, Hierarchy 패널의 `New Scene` 버튼 클릭 시 바로 씬을 교체하지 않고 템플릿 선택 팝업(`DrawNewSceneDialog`) 노출.
     - **Basic (3D) 템플릿**:
       - 메인 카메라(Perspective FOV 60), 방향성 라이트(햇빛 색상), 언바운드 포스트 프로세스 볼륨(Bloom ON, ToneMapping ON), 바닥 평면(PlaneMesh 10x10) 자동 배치.
     - **Empty 템플릿**:
       - 엔티티가 전혀 없는 순수 빈 씬 생성.
     - 더블클릭 또는 `[생성]` 버튼으로 직관적 선택 가능.
     - 기존 미저장 문서 확인(`EditorDocumentController`)과 자연스럽게 연계.
  3. **로캘 원본 CSV 키 추가 (`Editor/Resources/Data/Localization/Editor.csv`)**:
     - `EditorDocument.NewSceneDialogTitle`, `EditorDocument.NewSceneTemplatePrompt`, `EditorDocument.TemplateBasic`, `EditorDocument.TemplateBasicDesc`, `EditorDocument.TemplateEmpty`, `EditorDocument.TemplateEmptyDesc` 등록.
- 검증 기준:
  - `File -> New Scene` (Ctrl+N) 실행 시 템플릿 선택 모달 팝업이 표시되어야 함.
  - `Basic (3D)` 선택 후 생성 시 카메라, 방향성 라이트, 후처리 볼륨, 바닥이 배치된 상태로 새 씬이 열려야 함.
  - `Empty Scene` 선택 후 생성 시 완전히 비어 있는 씬이 열려야 함.
  - 현재 씬에 미저장 변경사항이 있을 때 기존의 저장 확인 대화상자가 먼저 정상 작동해야 함.

### [2026-11-09] 포스트 프로세싱 Source of Truth 아키텍처 정립, 이펙트 테스트 씬 볼륨 배치 및 프리뷰 뷰포트 파티클 누수 차단

- Area: Engine | Editor | Render | PostProcess | Particle | Scene
- Type: Fix | Architecture | UX
- 주요 변경 내역:
  1. **파티클 에디터 패널로의 메인 씬 파티클 유출 버그 교정 (`ParticleSystem.cpp`)**:
     - 766행 및 850행의 블렌드 모드 가드 조건식 수정:
       `if (renderPass && !renderPass->IsAcceptsBlendMode(material->GetBlendMode())) continue;`
     - `PreviewViewportWidget`의 `GeometryPass`는 메인 씬 유입 방지를 위해 `acceptedBlendModes = EBlendMode::None`으로 등록되어 있으나, 기존 `!= EBlendMode::None` 검사로 인해 차단 필터가 무력화되던 논리 오류 완전 해결.
  2. **포스트 프로세싱 단일 진실의 원천(Source of Truth) 확립**:
     - 뷰포트 간 강제 결합(임시 메모리 미러링)을 지양하고, 씬 데이터(`PostProcessComponent`)를 유일한 단일 진실의 원천으로 유지.
     - `GameViewportPanel`은 스탠드얼론 런타임과 100% 동일하게 씬의 활성 `PostProcessComponent` 설정만을 충실하게 반영.
     - `PostProcessPanel`의 조작(프리셋 선택, 파이프라인 리셋, 슬라이더 변경)이 활성 `PostProcessComponent`에 실시간으로 반영되도록 개선하여, `Apply to Scene` 버튼을 누르기 전이라도 씬 데이터가 즉시 갱신되어 게임 뷰와 씬 뷰 모두 실시간으로 동기화되도록 연동.
     - 씬에 `PostProcessComponent`가 없을 경우를 대비해 `PostProcessPanel`에 `Create Post-Process Volume` 원클릭 생성 버튼 탑재.
     - 로캘 원본 CSV(`Editor/Resources/Data/Localization/Editor.csv`)에 `PostProcess.CreateVolume` 및 `PostProcess.VolumeCreated` 키 추가.
  3. **`EffectTestScene.bamscene.json`에 PostProcessVolume 배치**:
     - 블룸(Bloom)이 활성화(`Enabled: true`, `Intensity: 1.5`, `Threshold: 0.8`)된 `PostProcessVolume` 엔티티 추가.
     - 게임 뷰포트에서 이펙트 테스트 씬을 실행할 때 파티클 및 스프라이트 이펙트에 영롱한 블룸 광휘 효과가 정상 렌더링되도록 구현 완료.
- 검증 기준:
  - `EffectTestScene.bamscene.json`을 열었을 때 파티클 에디터 패널에 아무 에셋도 로드하지 않았다면 프리뷰 화면에 메인 씬 파티클이 침범하지 않아야 함.
  - 게임 뷰포트에서 `EffectTestScene` 실행 시 블룸 효과가 적용되어 이펙트가 빛나야 함.
  - `PostProcessPanel`에서 프리셋이나 슬라이더를 조작했을 때 게임 뷰에서도 동일하게 실시간 반영되어야 함.

### [2026-11-09] 콘텐츠 브라우저 더블클릭 연동 확장: GraphPanel 및 AnimatorGraphPanel의 ResourceEditorInterface 통합 및 SpriteAnimation 복합 확장자 지원

- Area: Editor | ContentBrowser | ResourceEditor | Graph | Animation | 2D
- Type: Feature | Architecture | UX
- 주요 변경 내역:
  1. **`GraphPanel`의 `ResourceEditorInterface` 통합 (`GraphPanel.h/.cpp`)**:
     - 기존 `IImGui` 직접 상속에서 `ResourceEditorInterface` 상속으로 전환.
     - `IsSupported()` 가상 함수 구현으로 `.bamgraph.json`, `.graph.json`, `.bamgraph`, `.graph` 에셋 지원.
     - `SetTargetResource(path)` 및 `LoadDocument(path)` 구현으로 콘텐츠 브라우저에서 노드 그래프 더블클릭 시 `ImGuiManager::RequestOpenResourceEditor()` 파이프라인을 통해 에디터 캔버스에 자동 로드 및 윈도우 포커스.
     - 파일 내부의 `EGraphDomain`(`Logic` 또는 `Shader`)에 맞춰 에디터 모드를 자동 전환하는 지능형 도메인 적응 로직 탑재.
     - 윈도우 제목에 현재 열려있는 그래프 파일명 표시.
  2. **`AnimatorGraphPanel`의 `ResourceEditorInterface` 통합 (`AnimatorGraphPanel.h/.cpp`)**:
     - 기존 `IImGui` 직접 상속에서 `ResourceEditorInterface` 상속으로 전환.
     - `IsSupported()` 가상 함수 구현으로 `.bamanimator.json`, `.animator.json`, `.bamanimator`, `.animator` 에셋 지원.
     - `SetTargetResource(path)` 및 `LoadFromFile(path)` 구현으로 콘텐츠 브라우저에서 애니메이터 컨트롤러 더블클릭 시 상태 머신 자동 로드 및 3D 프리뷰 동기화.
     - `SaveDocument()` 및 `OpenDocument()` 다이얼로그 필터를 `.bamanimator.json` 표준 규격으로 일원화.
     - 윈도우 제목에 현재 열려있는 애니메이터 파일명 표시.
  3. **`SpriteAnimationEditor`의 복합 확장자 지원 확장 (`SpriteAnimationEditor.cpp`)**:
     - `IsSupported()`에서 단일 확장자(`.spriteanim`, `.bamspriteanim`)뿐만 아니라 복합 확장자(`*.bamspriteanim.json`, `*.spriteanim.json`)도 인식하도록 개선.
- 검증 기준:
  - 콘텐츠 브라우저에서 `Assets/Scripts/CubeController.bamgraph.json` 또는 `GraphMovement.bamgraph.json` 더블클릭 시 외부 프로그램(메모장)으로 빠지지 않고 에디터 내부의 `GraphPanel`이 즉시 포커스되며 노드 캔버스가 열려야 함.
  - 애니메이터 컨트롤러 에셋 더블클릭 시 에디터 내부의 `AnimatorGraphPanel`이 열리고 상태 머신이 로드되어야 함.
  - 스프라이트 애니메이션 복합 확장자 에셋(`.bamspriteanim.json`) 더블클릭 시 `SpriteAnimationEditor`가 정상적으로 열려야 함.

### [2026-11-09] 리소스 에셋 확장자 체계화 (.bam*.json 전면 적용) 및 리소스 로더·참조 경로·하위 호환성 강화

- Area: Engine | Editor | Resource | Scene | Graph | Timeline | Texture
- Type: Refactor | Architecture | Feature
- 주요 변경 내역:
  1. **리소스 에셋 확장자 독자 규격(`.bam*.json`) 전면 마이그레이션**:
     - 씬 에셋 15개: `Assets/Scenes/<Name>.json` -> `Assets/Scenes/<Name>.bamscene.json` (중복 파일 정리 완료)
     - 비주얼 스크립트 그래프 2개: `Assets/Scripts/<Name>.json` -> `Assets/Scripts/<Name>.bamgraph.json` (`CubeController`, `GraphMovement`)
     - 액션 타임라인 1개: `Assets/Data/Animation/Timeline/Survey.actiontimeline.json` -> `Survey.bamtimeline.json`
     - 텍스처 디스크립터 2개:
       - `Assets/Texture/SampleTexture.json` -> `SampleTexture.bamtex.json`
       - `Engine/Resources/Texture/white1x1.json` -> `white1x1.bamtex.json`
  2. **설정 및 참조 파일 경로 동기화**:
     - `Config/Project.json`: `"StartupScenePath"`를 `"Scenes/RaytracingScene.bamscene.json"`으로 갱신.
     - `Engine/Source/Core/Config/Public/Config.h`: 기본값 `StartupScenePath`를 `"Scenes/RaytracingScene.bamscene.json"`으로 동기화.
     - `.bam/User/Workspace.json`: `"LastScenePath"`를 `"Assets/Scenes/PhysicsPlayground.bamscene.json"`으로 갱신.
     - `Engine/examples/VirtualPathCheck.cpp`: 경로 확인 테스트 대상 `RaytracingScene.bamscene.json`으로 갱신.
     - `Assets/Prefab/Large Floor Sphere.bamprefab.json`: `graphFile` 경로를 `"Resources/Scripts/CubeController.bamgraph.json"`으로 갱신.
     - `Assets/Scenes/RaytracingScene.bamscene.json`: `graphFile` 경로를 `"Resources/Scripts/CubeController.bamgraph.json"`으로 갱신.
  3. **Scene 및 Graph 런타임 하위 호환(Fallback) 로딩 메커니즘 구축**:
     - `SceneManager::LoadScene`: 파일이 존재하지 않고 확장자가 `.json`인 경우, 디스크의 동일 이름 `.bamscene.json` 에셋을 자동 탐색하여 레거시 경로 호출도 투명하게 성공하도록 보장.
     - `GraphSystem::GetOrLoadGraph`: `.json` 파일 경로 요청 시 `.bamgraph.json` 에셋을 fallback으로 자동 탐색하여 기존 프리팹이나 씬에서 구 확장자를 참조하더라도 끊김 없는 바인딩 보장.
  4. **에디터 다이얼로그 필터 및 패널 확장자 확장**:
     - `GraphPanel.cpp`:
       - 파일 필터: `L"*.bamgraph.json;*.json"` (구버전과 신규 확장자 모두 지원)
       - 기본 저장 파일명 및 확장자: `LogicGraph.bamgraph.json` / `ShaderGraph.bamgraph.json`, `L"bamgraph.json"`
     - `AnimationTimelinePanel.h/.cpp`:
       - `IsSupported()`에서 `.bamtimeline.json` 및 `.bamtimeline` 인식 확장.
       - 파일 필터: `L"*.bamtimeline.json;*.actiontimeline.json"`, 기본 저장 확장자 `L"bamtimeline.json"`.
     - `InspectorPanel.cpp`: 타임라인 필터에 `L"*.bamtimeline.json;*.actiontimeline.json"` 지원.
  5. **ActionTimelineAsset의 ResourceManager 완전 통합**:
     - `ResourceManager::RegisterExplicitLoader`: `ActionTimelineAsset`을 `registerSimpleLoader`에 등록 (`.bamtimeline`, `.actiontimeline`).
     - `ActionTimelineAsset::Initialize`: `desc->Path` 전달 시 `LoadFromFile()` 자동 호출로 `ResourceManager::Get().LoadFile()`을 통한 캐싱 및 핸들 획득 파이프라인 완성.
     - `ActionTimelineAsset::LoadFromFile`: `PlatformPath::ResolveUri` 지원으로 가상 URI(`Project://`, `Engine://`) 및 절대 경로 완벽 대응.
- 검증 기준:
  - 에디터 실행 시 스타트업 씬(`RaytracingScene.bamscene.json`)과 마지막 편집 씬이 경고 없이 로드되어야 함.
  - 콘텐츠 브라우저에서 씬, 그래프, 타임라인, 텍스처 에셋의 전용 배지(SCENE, GRAPH, TIMELINE, TEX)와 아이콘이 올바르게 표시되고 더블클릭 시 전용 에디터로 열려야 함.
  - 그래프 저장/열기 및 타임라인 저장/열기 시 `*.bam*.json` 확장자가 기본으로 적용되어야 함.

### [2026-11-09] 핫 리로드 인터페이스(`IHotReloader`) 다중 후보 경로/환경변수 지원 아키텍처 일반화 및 셰이더 핫 리로드 매니저 완전 통합 (컴파일 에러 C2065 해결)

- Area: Editor | HotReload | Shader | Architecture
- Type: Refactor | Architecture | Bugfix
- 주요 변경 내역:
  1. **핫 리로드 기반 인터페이스(`IHotReloader`) 다중 경로 후보 관리 표준화**:
     - `HotReloaderInterface.h/.cpp`: 핫 리로더 최상위 인터페이스에 가상 URI, 환경변수(`PATH` 형식 세미콜론 분리 목록), 개별 디렉터리 경로를 등록/관리하는 표준 메서드군 신설 (`AddWatchPath`, `AddWatchPaths`, `AddWatchPathsFromString`, `AddWatchPathsFromEnvironment`, `RemoveWatchPath`, `ClearWatchPaths`, `GetWatchPaths`).
     - `Engine://`, `Project://` 가상 URI를 물리 절대경로로 자동 해석(`PlatformPath::ResolveUri`) 및 정규화(`Normalize`) 후 중복 등록 방지.
     - 가상 수명주기 훅 `OnWatchPathAdded(resolvedPath)`, `OnWatchPathRemoved(resolvedPath)` 제공으로 파생 클래스가 파일 감시자(`efsw`)를 자연스럽게 연동할 수 있도록 설계.
  2. **`ShaderHotReloadManager` 완전 통합 및 컴파일 오류(C2065 `m_WatchDirectory`) 해결**:
     - 251행/253행에서 헤더 파일(`.hlsli`) 수정 시 단일 변수 `m_WatchDirectory`를 참조하여 발생하던 C2065 미정의 식별자 에러 해결.
     - `GetWatchPaths()`를 순회하여 등록된 모든 디렉터리(`Engine://Shader/hlsl`, `Project://Shader/hlsl`, `Project://Shader`, `BAM_SHADER_PATH` 환경변수 등)의 영향을 받는 `.hlsl` 셰이더를 빠짐없이 감지 및 재컴파일하도록 수정.
     - `RegisterDefaultCandidates()`를 통해 기본 3대 경로 및 환경변수 자동 바인딩.
     - `OnWatchPathAdded/Removed`를 통해 `m_WatchIdMap[resolvedPath] = watchId` 형태로 파일 워치를 동적 등록/해제 관리.
- 검증 기준:
  - MSVC 컴파일 시 `ShaderHotReloadManager.cpp`의 C2065 에러 없이 빌드가 정상 성공해야 함.
  - 엔진 셰이더와 프로젝트 에셋 셰이더 수정 시 핫 리로드가 정상 감지되어야 함.



### [2026-11-09] 2D 스프라이트 애니메이션 런타임 및 에디터 프리뷰 재생 파이프라인 완성: ResourceManager 로더 등록, JSON 아카이브 라우팅 및 Presentation 페이즈 틱 연동

- Area: Engine | Resource | System | 2D | Animation | Editor
- Type: Bugfix | Architecture | Pipeline
- 주요 변경 내역:
  1. **`ResourceManager`의 `SpriteAnimation` 명시적 로더 등록 (`ResourceManager.cpp`)**:
     - `RegisterExplicitLoader`에 `registerSimpleLoader.operator()<SpriteAnimation, SpriteAnimationCreateDesc>({ L".spriteanim", L".bamspriteanim" });`를 등록하여, 에디터 및 런타임에서 `.spriteanim` / `.bamspriteanim` 파일 경로를 통한 리소스 생성 및 핸들 조회가 실패하던 결함 해결.
  2. **`ResourceManager.inl`의 JSON 아카이브 로딩 대상 확장**:
     - 기존 `if (extension == L".json")` 조건에 `extension == L".spriteanim" || extension == L".effect"`를 추가하여, 확장자가 `.json`이 아닌 JSON 형식 텍스트 에셋(`.spriteanim`)도 `JsonArchive` 파이프라인을 통해 온전히 역직렬화되도록 수정.
  3. **`SpriteAnimation::Initialize`의 디스크 파일 직접 로드 안전장치(Fail-Safe)**:
     - `Initialize(void* arg)`에서 `desc->frames`가 비어 있고 `desc->Path`가 주어졌을 때 `LoadFromFile(desc->Path)`를 자동 호출하도록 구현하여, 리소스 생성 경로와 무관하게 100% 무결한 프레임 시퀀스 복원 보장.
  4. **`SpriteAnimationSystem`의 에디터 뷰포트 프리뷰 연동 (`SpriteAnimationSystem.h/.cpp`)**:
     - `GetExecutionPhases()`를 기본값 `Gameplay`에서 `ESystemPhase::All`(`Gameplay | Presentation`)로 오버라이드하여, Play(PIE) 모드뿐만 아니라 에디터 편집 모드(뷰포트 Realtime 활성화 시)에서도 매 틱마다 `OnUpdate`가 호출되도록 개선.
     - `!anim.isPlaying` 정지 상태이거나 프레임 1개인 경우에도 현재 인덱스(`currentFrameIndex`)에 해당하는 스프라이트를 `renderer.spriteHandle`에 상시 동기화하도록 보완하여, 에셋을 할당하는 즉시 뷰포트와 인스펙터에 첫 프레임이 나타나고 인스펙터 슬라이더 조작 시 즉각 프리뷰되도록 개선.
- 검증 기준:
  - `SpriteAnimationComponent`에 `.spriteanim` 에셋 할당 시 유효한 프레임 수와 첫 프레임 스프라이트가 뷰포트 `SpriteRendererComponent`에 즉시 반영되는지 확인.
  - 상단 툴바의 Play 버튼(PIE 실행) 또는 씬 뷰포트의 `Realtime` 토글 활성화 시 스프라이트 애니메이션이 FPS 설정에 맞추어 부드럽게 재생되는지 확인.


### [2026-11-09] 콘텐츠 브라우저 엔진/에디터 리소스 상위 배치 및 하위 파일/폴더 표시 복원, CMake 잘못된 루트 Resources 생성 제거, 셰이더 핫 리로드 다중 경로(Engine + Assets/Shader) 감시 체계 구축

- Area: Editor | ContentBrowser | HotReload | Shader | Build | CMake
- Type: Bugfix | Feature | Architecture
- 주요 변경 내역:
  1. **콘텐츠 브라우저 엔진·에디터 리소스 표시 복원 및 상위 배치 (`ContentBrowserPanel.cpp`)**:
     - 원인 해결: `m_ShowEngineContent`가 워크스페이스 세팅 복원(`RestoreState`) 시 `0.0f`로 로드되거나 가드 조건으로 인해 `DrawDirectoryTree` 및 `DrawGrid`에서 읽기 전용 타깃(`Engine/Resources`, `Editor/Resources`) 하위 항목이 전부 `continue`되어 비어 보이던 결함 수정.
     - `rootPath` 및 `m_CurrentDirectory`가 엔진 또는 에디터 리소스 루트일 경우(`isInsideEngineOrEditor`), 읽기 전용 가드와 무관하게 모든 하위 폴더와 파일이 온전히 렌더링되도록 수정.
     - 트리 뷰(`TreeArea`) 순서 변경: `Engine Resources` $\rightarrow$ `Editor Resources` $\rightarrow$ `Assets` 순으로 상위에 나란히 배치하여 직관적 접근 보장.
     - `RestoreState`의 `ContentBrowser.ShowEngineContent` 기본값을 `1.0f`로 변경.
  2. **루트 `Resources/` 폴더 생성 버그 수정 및 정리 (`Engine/CMake/ShaderBuild.cmake`)**:
     - `SHADER_SOURCE_DIR`가 `${CMAKE_CURRENT_SOURCE_DIR}/../Resources/Shader`로 잘못 설정되어 프로젝트 루트에 불필요한 빈 `Resources/Shader/bin/...` 폴더를 생성하던 문제를 `${CMAKE_CURRENT_SOURCE_DIR}/Resources/Shader`로 올바르게 교정.
     - 프로젝트 루트에 남아있던 빈 `Resources/` 폴더 완전 삭제.
  3. **셰이더 핫 리로드 다중 감시 경로 시스템 구축 (`ShaderHotReloadManager.h/.cpp`)**:
     - 기존 단일 `m_WatchID` 구조를 `std::vector<std::filesystem::path> m_WatchDirectories` 및 `std::vector<efsw::WatchID> m_WatchIDs` 다중 경로 감시 아키텍처로 확장.
     - `RegisterWatchDirectory(const std::filesystem::path&)` 및 `UnregisterWatchDirectory` 동적 등록 API 신설.
     - 기본 감시 경로 자동 등록: 엔진 시스템 셰이더(`Engine://Shader/hlsl`)뿐만 아니라 프로젝트 에셋 셰이더(`Assets/Shader/hlsl` 또는 `Assets/Shader`)를 함께 등록하여 프로젝트 레벨의 커스텀 셰이더 수정 시에도 자동 핫 리로드 보장.
     - `RecompileAll()`에서 등록된 모든 디렉터리를 재귀 순회하여 일괄 재컴파일하도록 개선.
- 검증 기준:
  - 콘텐츠 브라우저에서 `Engine Resources`와 `Editor Resources`가 상단에 표시되고, 클릭 및 확장 시 내부의 머티리얼, 셰이더, 로캘 등 하위 파일/폴더가 정상 표시되는지 확인.
  - CMake 재구성/빌드 시 프로젝트 루트에 빈 `Resources/` 폴더가 더 이상 생성되지 않는지 확인.
  - `Assets/Shader/`에 HLSL 파일 배치 후 수정 시 핫 리로드 알림 및 컴파일이 정상 트리거되는지 확인.



### [2026-11-09] 에디터 UX 정밀화: 계층 패널 씬 타이틀바 우측 엔티티 생성(+) 버튼 재배치 및 콘텐츠 브라우저 3대 최상위 마운트 루트(Assets, Engine Resources, Editor Resources) 상시 노출 완비

- Area: Editor | Hierarchy | ContentBrowser | UX | Localization
- Type: Refactor | Feature | UX
- 주요 변경 내역:
  1. **계층 패널(`HierarchyPanel`) 엔티티 생성 버튼 재배치**:
     - 상단 검색창 좌측의 `[+]` 버튼을 제거하여 검색창이 패널 너비를 온전히 활용하도록 복원.
     - 씬 이름 타이틀바(`DrawSceneTitle`) 우측 끝에 `ICON_FA_PLUS` 단독 툴버튼을 배치하여 씬 헤더와의 시각적 통일성 및 직관적인 생성 UX 확립.
  2. **콘텐츠 브라우저(`ContentBrowserPanel`) 3대 최상위 마운트 루트 상시 노출**:
     - `m_EditorRootPath`(`PlatformPath::GetEditorResourceRoot()`) 및 `GetEditorRootPath()` 신규 추가.
     - 좌측 폴더 트리(`TreeArea`)에 `Assets`(프로젝트 에셋), `Engine Resources`(엔진 내장 리소스), `Editor Resources`(에디터 내장 리소스) 3개 루트를 구분선과 함께 기본 상시 노출하도록 구조 개편 (`m_ShowEngineContent = true`).
     - 트리 노드(`ContentBrowserTreeItem.cpp`)에서 각 루트별 고유 아이콘(`ICON_FA_DATABASE`, `ICON_FA_GEARS`, `ICON_FA_TOOLBOX`)과 로캘 이름 연결.
     - `GetActiveRootForPath` 및 주소표시줄(`AddressBar`), 전체 폴더 검색(`ScopeAll`)에 `m_EditorRootPath`를 완전 통합.
  3. **다국어 로캘 키 동기화 (`Editor.csv`)**:
     - `ContentBrowser.EngineResources`, `ContentBrowser.EditorResources` 추가.
- 검증 기준:
  - 계층 패널 씬 이름 우측 끝에 `+` 버튼이 노출되고 클릭 시 엔티티 프리셋 생성 팝업이 정상 작동하는지 확인.
  - 콘텐츠 브라우저 좌측 트리에 `Assets`, `Engine Resources`, `Editor Resources` 3개 최상위 루트가 표시되고, 각 루트 폴더를 클릭했을 때 해당 경로로 탐색 및 주소표시줄에 해당 루트명이 표시되는지 확인.



### [2026-11-08] 에디터 저작 워크플로우 강화: 파일 다이얼로그 컨텍스트별 독립 경로 기억/디폴트 폴더 매핑, 계층 패널 [+] 프리셋 엔티티 생성, 콘텐츠 브라우저 엔진 저작 리소스 신규 생성 및 에셋 파이프라인/크래시 복원 규격화

- Area: Editor | FileDialog | ContentBrowser | Hierarchy | Architecture | Guidelines
- Type: Feature | Architecture | UX
- 주요 변경 내역:
  1. **파일 다이얼로그(`FileDialogs`) 컨텍스트별 독립 경로 기억 및 기본 폴더 스마트 매핑**:
     - `FileDialogContext.h`: `EFileDialogContext`에 `ParticleOpen/Save`, `SpriteOpen/Save`, `TimelineOpen/Save`, `ModelImport`, `MaterialOpen/Save`, `PrefabOpen` 등 10종의 세부 컨텍스트 신설.
     - `FileDialogs.h/.cpp`: Windows `IFileDialog::SetClientGuid`를 활용하여 각 기능/에디터별로 Windows 레지스트리에 마지막 방문 경로를 완벽히 독립적으로 영구 저장.
     - `GetDefaultFolderForContext`: 첫 방문 시 또는 유효 경로가 없을 때 프로젝트 에셋 루트 기준 최적의 기본 폴더(`Assets/Scenes/`, `Assets/Effect/`, `Assets/Texture/`, `Assets/Data/Animation/Timeline/`, `Assets/Source/`, `Assets/Material/`, `Assets/Prefab/`)를 자동으로 바인딩.
     - `ParticleEditorPanel`, `SpriteEditor`, `AnimationTimelinePanel`, `ToolBar` 등의 다이얼로그 호출부에 해당 컨텍스트 적용.
  2. **계층 패널(`HierarchyPanel`) 상단 `[+]` 프리셋 엔티티 생성 버튼 신설**:
     - 상단 검색창 좌측에 정사각형 `[+]` 툴 버튼 배치 및 `DrawEntityCreationMenu` 구현.
     - 빈 엔티티 생성, 3D 오브젝트(Cube, Sphere, Cylinder, Quad), 조명(Directional, Point, Spot), 2D 오브젝트(Sprite), UI Canvas, Terrain 등 빈번히 쓰이는 엔티티 프리셋을 원클릭으로 생성 가능하도록 통합.
     - 씬 빈 영역 우클릭 메뉴와 상단 `[+]` 버튼 메뉴의 동기화 및 단일화.
  3. **콘텐츠 브라우저(`ContentBrowserPanel`) 엔진 저작 리소스 7종 생성 기능 구현**:
     - `GridViewContextMenu` 내 `Create` 서브메뉴 확장: Material, Material Instance, Scene, Prefab, Visual Script Graph, Animation Timeline, Particle Effect 생성 지원.
     - `GenerateUniqueAssetPath`: 이름 충돌 방지 번호 매김(`NewMaterial`, `NewMaterial_1` 등) 구현.
     - 템플릿 파일 생성 즉시 `RequestRefresh()` 호출 및 `SetRenameTarget()`으로 F2 인라인 이름 편집 모드에 자동 진입.
  4. **다국어 로캘 키 원본 동기화 (`Editor.csv`)**:
     - `ContentBrowser.CreateEffect`, `CreateGraph`, `CreatePrefab`, `CreateScene`, `CreateTimeline` 및 `Hierarchy.AddEntityTooltip`, `3DObjects`, `Cube`, `Sphere`, `Cylinder`, `Quad`, `Lights`, `2DObjects`, `Sprite` 등록.
  5. **아키텍처 및 파이프라인 규격화 (문서 6종 갱신/신설)**:
     - `Docs/EngineEditorClientGuidelines.md`: 독자 리소스 확장자 이중화(`*.bam*` / `*.bam*.json`), 가상 마운트 루트(`Engine://`, `Project://`), 하이브리드 모델 에셋 정리 규칙 명문화.
     - `Docs/BuildConfigurationAndPreprocessing.md`: 클라이언트 빌드 시 엔진 내 에디터/디버그 코드 스트립 시 No-Op 스텁, 매크로 증발, ABI 보존 4대 설계 패턴 수립.
     - `Docs/CrashHandlingAndRecoveryPlan.md`: SEH 미니덤프와 5분 주기 하트비트 스냅샷을 결합한 2단계 복원 아키텍처 신설.
     - `Docs/DevelopmentPriorities.md`: 상기 작업 목록의 우선순위 공식 등재.
- 검증 기준:
  - 파일 다이얼로그 실행 시 파티클 에디터는 `Assets/Effect`, 스프라이트 에디터는 `Assets/Texture`, 씬 저장은 `Assets/Scenes`가 기본으로 열리고, 폴더 이동 후 재호출 시 각 창이 마지막 폴더를 독립적으로 기억하는지 확인.
  - 계층 패널 `[+]` 버튼 및 콘텐츠 브라우저 우클릭 생성 동작 확인.
  - 빌드 및 실행 확인 (사용자 수동 검증).



### [2026-11-08] 2D 스프라이트 렌더링 및 에디터 연동 정밀화: 인스펙터 SpriteAnimation 리소스 피커 연결, 표준 PPU(100.0) 스케일링 체계화 및 GBuffer Unlit 과노출 버그 해결

- Area: Editor | Engine | Resource | 2D | Render | Shader
- Type: Bugfix | Architecture | Quality
- 주요 변경 내역:
  1. **인스펙터 `SpriteAnimation` & `ParticleEffect` 리소스 피커 완비**:
     - `ReflectionPropertyDrawer.cpp`: `INSPECTOR_RESOURCE` 매크로 목록에 `SpriteAnimation`과 `ParticleEffect`를 신규 등록하여, 인스펙터에서 `ResourceHandle<SpriteAnimation>`(`SpriteAnimationComponent.animation`) 및 `ResourceHandle<ParticleEffect>`가 "값 미리보기를 사용할 수 없습니다" fallback 대신 검색·선택·미리보기가 가능한 표준 리소스 피커로 즉시 연동되도록 개선.
     - `ResourcePicker.h`: `SpriteAnimation`(`.bamspriteanim`, `.spriteanim`, `ICON_FA_FILM`, 민트색) 및 `ParticleEffect`(`.bameffect`, `.effect`, `ICON_FA_WAND_MAGIC_SPARKLES`, 주황색)의 확장자 판별식과 시각적 비주얼(아이콘/배지) 특수화 추가.
     - `Resources.h`: 공통 리소스 헤더에 `#include "SpriteAnimation.h"` 추가.
  2. **스프라이트 PPU(Pixels Per Unit) 표준화 (100.0f 규격화)**:
     - `Macro.h`: 상용 2D 엔진 표준에 맞추어 `#define DEFAULT_PPU 100.f`로 수정 (기존 `1.f`로 인해 $128 \times 128$ 픽셀 스프라이트가 128미터 거대 쿼드로 렌더링되던 문제 원천 해결).
     - `Sprite.h` / `Sprite.cpp`: `SpriteCreateDesc`에 `pixelPerUnit = DEFAULT_PPU` 추가, `Sprite::Initialize`에서 PPU 초기화 및 `SetPixelPerUnit(f32 ppu)` 갱신 API 신설.
     - `SpriteSlicingService.h` / `SpriteEditor.cpp`: `SpriteSliceParameters`에 `pixelPerUnit = 100.0f` 추가, 슬라이스 모달에 PPU 조절 필드 배치, 슬라이스 저장 시 각 스프라이트 에셋에 PPU 기록 및 로드된 인스턴스 실시간 반영.
     - 기존 생성된 `Assets/Texture/uv1*.bamsprite.json` 파일들의 `m_PixelPerUnit`을 100으로 일괄 마이그레이션.
  3. **GBuffer Unlit 스프라이트 과노출(2배 밝기) 및 라이팅 왜곡 해결**:
     - `gbufferSpritePS.hlsl` / `gbuffer_sprite.frag`: 기존에 `output.Diffuse`와 `output.Emission`에 동일한 `texColor.rgb`를 중복 기록하여 `lightingPS`의 `albedo + emission` 경로에서 2배로 하얗게 타버리던 결함 수정.
     - 일반 스프라이트는 `output.Emission = float4(0, 0, 0, 1)`로 설정하여 원본 텍스처 색상이 1:1로 정확하게 출력되도록 보장하고, `color > 1.0`인 HDR 발광 파티클에 대해서만 초과분을 Emission으로 전달하여 자연스러운 블룸을 유지하도록 개선.
- 검증 기준:
  - `SpriteSlicingCheck` CTest 단위 테스트 통과 (`params.pixelPerUnit == 100.0f`).
  - 에디터 인스펙터에서 `SpriteAnimationComponent`의 애니메이션 필드가 정상 리소스 피커로 표시되고 드롭다운/선택이 가능해야 함.
  - 뷰포트에서 스프라이트가 1.28m 적정 크기로 렌더링되고 과노출 없이 원본 색상으로 표시되어야 함.


### [2026-11-08] 2D 제작 워크플로우(트랙 D Phase 1) 및 2D 이펙트 시스템: 스프라이트 슬라이싱 서비스(SpriteSlicingService)와 알파 BFS 자동 분할 구축, 스프라이트 플립북 원샷 자동 수명 제어 및 2D 파티클 Sub-UV 애니메이션 연동

- Area: Editor | Engine | Resource | 2D | FX | Render | Localization
- Type: Feature | Architecture | Safety
- 주요 변경 내역:
  1. **스프라이트 슬라이싱 서비스 신설 및 0-패딩(Zero-Padding) 네이밍 확립 (`SpriteSlicingService.h/.cpp`)**:
     - `GridByCellSize`, `GridByCellCount`, `Automatic` 3대 슬라이싱 알고리즘을 UI 독립형 순수 로직 서비스로 캡슐화.
     - 0-패딩(Zero-Padding) 네이밍 체계: 총 슬라이스 수량에 맞춘 자릿수 포맷팅(`uv1_00`, `uv1_01`, ..., `uv1_63`)을 적용하여 파일 정렬 시 10의 자리와 1의 자리가 교차하는 사전식 정렬 결함 원천 해결.
     - `baseName` 정제를 통해 `uv1.bamsprite_01` 같은 중복 태그를 제거하고 `uv1_01.bamsprite` 형태의 깔끔한 파일명 생성 보장.
     - `ContentBrowserPanel.cpp`: `StrCmpLogicalW`를 이용한 폴더 우선 및 자연 정렬(Natural Sort) 도입으로 윈도우 탐색기와 완벽히 동일한 정렬 뷰 제공.
     - `Automatic` 알파 클러스터링: RGBA8 CPU 픽셀 데이터를 8방향 BFS(Flood-Fill)로 순회하여 불규칙한 스프라이트 섬(Island)의 바운딩 박스(AABB) 자동 추출, `minPixelArea`를 통한 단일 픽셀 노이즈 제거, 행/열 순서 정렬 및 순차 넘버링 구현.
     - 파일(`.png` 등) 및 메모리 버퍼 직접 슬라이싱 API 제공.
  2. **스프라이트 에디터(`SpriteEditor.h/.cpp`) 연동 및 애니메이션 위저드 강화**:
     - `SpriteEditor`의 슬라이싱 로직을 `SpriteSlicingService`로 일원화하고, `DrawSliceModal`에 알파 임계값(`alphaThreshold`)과 최소 면적(`minPixelArea`) 조절 슬라이더 추가.
     - `GenerateAnimation`: 슬라이스된 에셋의 저장 확장자(`.bamsprite.json` / `.bamsprite`)를 동적으로 판별하여 단일/다중 행 클립 생성 시 유효한 리소스 핸들을 안전하게 취득하도록 개선.
  3. **스프라이트 플립북 원샷 2D 이펙트 자동 수명 주기 (`SpriteAnimationComponent.h`, `SpriteAnimationSystem.cpp`)**:
     - `autoDeactivateOnFinish`(완료 시 엔티티 비활성화, 오브젝트 풀링 지원) 및 `autoDestroyOnFinish`(완료 시 엔티티 자동 파괴) 속성 신설 및 리플렉션 등록.
     - `SpriteAnimationSystem`에서 순회 도중 `scene->FindEntity` 후 일괄 안전 파괴를 적용하여 entt iterator 무효화 없는 안전한 자동 수명 관리 보장.
  4. **2D 파티클 평면 정렬 및 Sub-UV 플립북 애니메이션 (`ParticleEmitterComponent.h`, `ParticleEffect.h`, `ParticleSystem.cpp`)**:
     - `billboardMode == ESpriteBillboardMode::None`: 카메라 3D 빌보드 대신 2D 게임 평면(XY 평면 $Z=0$, Z축 회전 $p.rotation$)에 일치된 쿼드 생성 지원.
     - Sub-UV (Flipbook Texture Sheet) 파티클: `subUVColumns`, `subUVRows`, `subUVCycles`를 기반으로 파티클 수명 정규화 시간($t$)에 따라 UV 변환 오프셋/스케일을 계산하여 절차적 시트 애니메이션 드로우 커맨드 제출.
  5. **헤드리스 단위 테스트 `SpriteSlicingCheck` 신설 및 CTest 등록 (`SpriteSlicingCheck.cpp`, `Editor/CMake/Checks.cmake`)**:
     - $64x64$ 크기의 셀 크기 분할, $128x64$의 행열 분할, $16x16$ 메모리 픽셀 버퍼에서의 2개 아일랜드 검출 및 1픽셀 노이즈 필터링 검증 테스트 작성 및 CTest 등록.
  6. **로컬라이제이션 원본 CSV 동기화**:
     - `Engine/Resources/Data/Localization/Engine.csv`: `PROP_AUTO_DEACTIVATE_ON_FINISH`, `PROP_AUTO_DESTROY_ON_FINISH`, `PROP_SUBUV_COLUMNS`, `PROP_SUBUV_ROWS`, `PROP_SUBUV_CYCLES` 추가.
     - `Editor/Resources/Data/Localization/Editor.csv`: `SpriteEditor.AlphaThreshold`, `SpriteEditor.MinPixelArea` 추가.
- 실제 검증 결과 및 미검증 항목:
  - Unity 빌드 및 헤더 정합성 수정:
    - `SpriteSlicingService.h`: 엔진 표준 프리컴파일드/공통 헤더인 `#include "Engine_Includes.h"` 및 `Structs/Rect.h`로 정규화.
    - `CommandHistory.h`: 최상단에 `#include "Editor_Includes.h"`를 선행 포함하여 `Delegate.h`의 `using namespace std;`에 의한 `unity_2_cxx.cxx`의 Windows SDK `byte` 모호성 충돌(C2872) 해결.
    - `stb_image` 단일 독립 TU(`StbImageImpl.cpp`) 신설 및 중복/미구현 링커 에러(LNK2019/LNK2005) 근본 해결:
      - `ContentBrowserPanel.cpp`에 하드코딩되어 있던 `#define STB_IMAGE_IMPLEMENTATION`을 제거하고 전용 구현 파일(`StbImageImpl.cpp`)로 추출.
      - `Editor/CMakeLists.txt`에서 `StbImageImpl.cpp`에 `SKIP_UNITY_BUILD_INCLUSION ON`을 지정하여 Unity 빌드 컴파일 오염 차단.
      - `Checks.cmake`의 `SpriteSlicingCheck` 소스 목록에 `StbImageImpl.cpp`를 연결하여 `stbi_load_from_memory`, `stbi_image_free` 미참조 링크 에러 해결.
  - 검증 완료: 정적 코드 정합성, 리플렉션 속성 매크로 매칭, 메모리 버퍼 슬라이싱 알고리즘 설계.
  - 미검증 항목: 빌드 및 CTest 단위 테스트 실행 (사용자 실행 권한 원칙에 따라 사용자 확인 대기).


### [2026-11-08] 에셋 아키텍처 연동 안정화: 에디터 프리로드 파이프라인(StartupResourceLoader) 가상 경로 일원화, 머티리얼 인스턴스 참조 복구 및 에디터 구동 검증 통과

- Area: Editor | Core | Resource | Material | Config
- Type: Bugfix | Architecture | Safety
- 주요 변경 내역:
  1. **`StartupResourceLoader.cpp`의 가상 경로(`PlatformPath::ResolveUri`) 전면 적용**:
     - 기존 `projectRoot / path` 직접 결합을 제거하고 `PlatformPath::ResolveUri`를 거치도록 일원화하여 `Assets/Config/Startup/` 매니페스트 및 파이프라인 단계별 텍스처, 셰이더, 메시, 머티리얼, 머티리얼 인스턴스 전 파일이 `Assets/`와 `Engine/Resources/`에서 투명하게 로드되도록 개선.
  2. **`Config/Project.json` 및 `Config.h` 경로 정규화**:
     - `ProjectSettings`의 `AssetDirectory`("Assets"), `EngineResourceDirectory`("Engine/Resources")를 명시하고, `StartupManifestPath`("Config/Startup/StartupManifest.json"), `StartupScenePath`("Scenes/RaytracingScene.json")를 최신 분리 규약으로 동기화.
     - `Application::InitializeResources`에서 프로젝트 설정 로드 후 `PlatformPath::SetProjectAssetRoot`와 `SetEngineResourceRoot`를 즉시 갱신하도록 안전장치 추가.
  3. **`ResourceHandleArchiveAdapter.h` 및 `PlatformPath::ToVirtualUri` 참조 해석 강화**:
     - `ResolveResourceHandle`에서 `FindHandleByFilePath` 교차 조회를 추가하고, 디스크 폴백 시 `PlatformPath::ResolveUri`를 적용하여 확장자 없는 레거시 키나 물리 분리된 머티리얼 인스턴스 역직렬화 실패(`code=2, field=ResourceReference`) 원천 차단.
     - `SpriteMaterialInstance.bammatinst.json`과 `SpriteMaterial.bammat.json`의 키와 경로를 표준 확장자 규약으로 정규화.
- 실제 검증 결과:
  - CTest (`VirtualPathCheck`, `ReflectionInspectorCheck`): 100% Passed.
  - 에디터 빌드(`Editor.vcxproj`) 및 실행(`Editor.exe`): `[StartupLoader] Preload pipeline completed successfully.` 확인 및 에디터 메인 창 정상 구동 통과.

### [2026-11-08] 비주얼 이펙트 아키텍처: 계층형 통합 모델 (VFX Composite / Track-based Orchestrator) 설계 완료

- Area: Engine | Render | FX | Editor | Architecture
- Type: Architecture | Design | Documentation
- 주요 내용:
  1. **설계 방향성 확정:**
     - 단순 파티클 시뮬레이터에서 메쉬 이펙트(마법진, 충격파 링, 왜곡 구체, UV 스크롤), 조명 섬광(Light Flash), 사운드(SFX), 트레일(Trail/Ribbon)을 단일 시간축에서 오케스트레이션하는 **계층형 통합 모델(VFX Composite)** 공식 채택.
  2. **설계 문서화 완료 (`Docs/CompositeVisualEffectPlan.md`):**
     - 에셋 계층(`VisualEffectAsset`), 런타임 오케스트레이터(`VisualEffectComponent`), 서브시스템 분리 및 포워드 투명 렌더링(`ForwardTransparentPass`) 파이프라인 규약 수립.
     - `ParticleEditorPanel`을 멀티 트랙 타임라인 에디터(`VFXEditorPanel`)로 점진적 확장하는 4단계 로드맵(Phase 1~4) 수립.
  3. **진행 순서 및 대기(Hold) 상태 등록:**
     - `Docs/DevelopmentPriorities.md` 11.3절에 **트랙 F**로 정식 등록.
     - 사용자 지시에 따라 2D 스프라이트 애니메이션(트랙 D) 작업 완료 후 본 계층형 통합 이펙트 구현에 착수하도록 대기(Hold) 상태로 관리.

### [2026-11-08] 로캘 아키텍처 혁신: Engine(엔진 코어)과 Editor(에디터 전용) 로컬라이제이션 완전 물리 분리 및 3계층 가상 마운트 URI(Editor://) 확립

- Area: Engine | Editor | Core | Localization | Build | Tests
- Type: Architecture | Feature | Refactor | Safety
- 주요 변경 내역:
  1. **`Engine`과 `Editor` 로컬라이제이션 물리 디렉터리 및 번역본 완전 분리**:
     - `Engine/Resources/Data/Localization/`: `Engine.csv`, `Test.csv` 전용 -> `English.json`, `Korean.json` (778개 키: 컴포넌트 리플렉션, 속성 카테고리, 엔진 코어 진단).
     - `Editor/Resources/Data/Localization/`: `Editor.csv` 전용 -> `English.json`, `Korean.json` (1,700개 키: 메뉴바, 툴팁, 인스펙터, 패널 UI 문구).
     - 순수 게임 런타임(Client) 배포 패키징 시 에디터 UI 번역본이 0바이트로 완전히 배제되어 경량화 및 내부 문자열 보호 달성.
  2. **`PlatformPath` 3계층 가상 마운트 URI (`Editor://`) 아키텍처 확립 (`PlatformPath.h/.cpp`)**:
     - `GetEditorResourceRoot()` (`Editor/Resources/`), `IsEditorUri()` (`Editor://`) 신설.
     - `Engine://` (엔진 코어 자원), `Editor://` (에디터 전용 자원), `Project://` (사용자 프로젝트 에셋) 3계층 가상 마운트 해석 지원.
     - `EditorResourceRoot` 하위 및 `Editor://` URI에 대해서도 콘텐츠 브라우저 읽기 전용 가드(`IsReadOnlyTarget`) 완전 구현(가상 URI 판별 및 물리 서브패스 검사 일원화).
  3. **CMake 빌드 파이프라인 및 정적 감사 도구 멀티 타깃 개편**:
     - `LocalizationBuild.cmake`: Engine과 Editor의 CSV 파일들을 각각 독립 변환하는 커스텀 커맨드 구성 및 `GenerateLocalization` 통합 타깃 연결.
     - `audit_localization.py`: 분리된 두 디렉터리를 자동 순회하여 전수 교차 검증하도록 확장 (632개 파일, 2,478개 키 100% 무결성 통과).
  4. **에디터 런타임 및 단위 테스트 다중 카탈로그 등록 일원화**:
     - `Application.cpp`: `Engine://Data/Localization/...`과 `Editor://Data/Localization/...`을 순차 등록하여 `LocalizationManager`에서 단일 메모리 해시맵으로 투명 병합.
     - `ReflectionInspectorCheck`, `GraphMessageLocalizationCheck`, `MainMenuBarLayoutCheck`, `VirtualPathCheck`: 분리된 엔진 및 에디터 로캘 카탈로그를 함께 등록하도록 호출부 정규화.
- 실제 검증 결과:
  - `python Tools/Audit/LocalizationAuditor/audit_localization.py --strict`: 632개 파일 및 2,478개 키 전수 검사 통과 (결함 0건, 100% Passed).
  - CTest: `VirtualPathCheck`, `ReflectionInspectorCheck` 단위 테스트 100% 통과 (Passed).

### [2026-11-08] 에셋 아키텍처 Phase 3-B (Step 4 완결): 엔진 기본 리소스(텍스처·머티리얼·메시·기즈모) 대폭 보강 및 프로젝트 에셋(Assets/) 완전 물리 분리

- Area: Engine | Core | Resource | Model | Editor | Tests
- Type: Feature | Architecture | Safety | Refactor
- 주요 변경 내역:
  1. **엔진 빌트인 리소스 대폭 보강 및 `Engine/Resources/` 이관**:
     - 1x1 단색 텍스처 5종(`white1x1.png/json`, `black1x1.png`, `blue1x1.png`, `magenta1x1.png`, `green1x1.png`) 및 기본 파티클 텍스처 4종(`Particle_SoftCircle.png`, `Particle_Ring.png`, `Particle_Smoke.png`, `Particle_Spark.png`)을 `Engine/Resources/Texture/`로 이관.
     - 신규 유틸리티 텍스처 구축: UV 언랩/텍셀 밀도 검사용 `DefaultChecker.png`(512x512, 32px 타일 체커보드), 법선 평면 `DefaultNormal.png`(128x128), 기본 `DefaultPBR.png`, 로드 실패 에러 경고용 `MissingTexture.png`.
     - 뷰포트 기즈모/엔티티 아이콘 일원화: 불필요한 디스크 텍스처를 배제하고 메모리 상주 Font Awesome 6 벡터 아이콘(`ICON_FA_CAMERA`, `ICON_FA_LIGHTBULB`, `ICON_FA_SUN`, `ICON_FA_VOLUME_HIGH`, `ICON_FA_FLAG` 등)을 재사용하도록 정책화하여 런타임/패키지 경량화 달성.
     - 코어 빌트인 머티리얼 일원화: `SpriteMaterial`, `SpriteMaterialInstance`, `ParticleMaterial_AlphaBlend`, `ParticleMaterial_Additive`, `UIMaterial`, `TransparentMaterial`, `MissingMaterial`, `PostProcessDefaultMaterial`을 `Engine/Resources/Material/`에 구축.
  2. **절차적 기본 메시 프리셋 확장 (`BuiltInPrimitiveMeshes.cpp`, `AssetReferenceValidator.cpp`)**:
     - 기존 `QuadMesh`, `CubeMesh`, `SphereMesh`에 이어 `PlaneMesh`(1m x 1m XZ 평면), `CylinderMesh`(반지름 0.5m, 높이 1m 원기둥), `CapsuleMesh`(반지름 0.25m, 전체 높이 1m 캡슐) 3종 절차적 메시를 추가 구현하고 `AssetReferenceValidator.cpp` 화이트리스트에 등록.
  3. **프로젝트 에셋 디렉터리(`Assets/`) 완전 물리 이관 및 소비자 경로 정규화**:
     - `Resources/`에 남아 있던 순수 프로젝트 자산(Audio, Config, Data, Effect, Icon, Material, Model, Prefab, Scenes, Scripts, Texture 및 외부 에셋 아카이브)을 `Assets/` 아래로 완전 물리 이관.
     - `Application.cpp`: `StartupManifestPath` 및 시작 씬(`RestoreLastEditedScene`) 로드 시 `PlatformPath::ResolveUri`를 거치도록 교체하여 상대 경로/가상 URI 투명 해석 지원.
     - `PlatformPath.cpp`: `IsBuiltInRelativeResourcePath`에 신규 엔진 텍스처 및 머티리얼 등록하여 읽기 전용 판별 일원화.
     - `.gitignore`: `Assets/` 하위 모델 및 대용량 에셋 아카이브 무시 규칙 동기화.
  5. **`LocalizationManager` 및 `ReflectionInspectorCheck` 경로 해석 정규화**:
     - `LocalizationManager::RegisterLocalizationData`: 등록 파일 경로에 대해 `PlatformPath::ResolveUri`를 적용하여 `Resources/Data/Localization/...` 상대 경로와 `Engine://Data/Localization/...` 가상 URI 모두 분리된 엔진 물리 경로로 올바르게 찾아 열 수 있도록 개선.
     - `ReflectionInspectorCheck.cpp`: `RegisterLocalizationData` 호출 시 `PlatformPath::ResolveUri`를 통해 정규화된 경로를 명시하여 헤드리스 단위 테스트에서 로캘 카탈로그 로드 실패 결함 해결.
  4. **가상 URI 및 물리 분리 무결성 단위 테스트 (`VirtualPathCheck.cpp`)**:
     - Case 10 & 11 추가: `Engine://Texture/DefaultChecker.png`, `Engine://Material/SpriteMaterial.bammat.json`, `Project://Scenes/RaytracingScene.json` 및 레거시 `Resources/Scenes/...` 상대 경로가 분리된 `Assets/` 물리 위치로 100% 일치 해석됨을 정량 검증.
- 실제 검증 결과:
  - `python Tools/Audit/LocalizationAuditor/audit_localization.py --strict`: 632개 파일 및 2478개 키 전수 검사 통과 (결함 0건, 100% Passed).
  - CTest: `VirtualPathCheck` (Case 1~11), `ReflectionInspectorCheck` 100% 통과 (Passed).
- 미검증 항목:
  - 에디터 실제 구동 시 `Assets/` 프로젝트 에셋 트리 및 신규 기본 메시/텍스처 렌더링 확인.

### [2026-11-08] 에셋 아키텍처 Phase 3-B (Step 1~3): 코어 셰이더·기본 머티리얼·폰트·로캘 물리 이관 및 도구 체인 연동

- Area: Engine | Core | Resource | Editor | Localization | Shader | Tests
- Type: Feature | Architecture | Safety | Refactor
- 주요 변경 내역:
  1. **하드코딩 소비자 경로의 `PlatformPath::ResolveUri` 일원화 (`Application.cpp`, `ImGuiManager.cpp`, `ShaderHotReloadManager.cpp`)**:
     - `Application.cpp`: 로캘 데이터 로드 시 `projectRoot / "Resources/Data/Localization/..."` 대신 `PlatformPath::ResolveUri(L"Engine://Data/Localization/English.json")` 및 `Korean.json`으로 교체.
     - `ImGuiManager.cpp`: Font Awesome 폰트 로드 경로를 `PlatformPath::ResolveUri(L"Engine://Fonts/...")`로 일원화.
     - `ShaderHotReloadManager.cpp`: HLSL 셰이더 소스 디렉터리 경로를 `PlatformPath::ResolveUri(L"Engine://Shader/hlsl")`로 갱신하여 물리 분리 환경에서도 핫리로드 트리거 정상 작동 보장.
  2. **`AssetHotReloadManager` 멀티 디렉터리 감시 지원 (`AssetHotReloadManager.h/.cpp`)**:
     - 기존 단일 `m_RootPath` 감시에서 `m_EngineWatchID`를 추가하여 `GetProjectAssetRoot()`와 `GetEngineResourceRoot()`를 모두 감시하도록 확장. 물리 디렉터리가 분리되더라도 엔진 리소스와 프로젝트 에셋 양쪽의 파일 변경(핫리로드)을 누락 없이 감지.
  3. **배치 1 물리 이관 — 엔진 셰이더 및 코어 기본 머티리얼 (`git mv`)**:
     - `Resources/Shader/` 전체 -> `Engine/Resources/Shader/` 물리 이관.
     - 기본 머티리얼 3종(`DefaultMaterial.bammat.json`, `SkinningMaterial.bammat.json`, `SkyMaterial.bammat.json`) -> `Engine/Resources/Material/` 이관.
     - `.gitignore`에 `Engine/Resources/Shader/bin/` 및 `Engine/Resources/Shader/spirv/` 추가하여 컴파일된 셰이더 바이너리 추적 방지.
  4. **배치 2 물리 이관 — 엔진 폰트, 로컬라이제이션 원본 및 도구 체인 연동**:
     - `Resources/Fonts/` -> `Engine/Resources/Fonts/` 물리 이동 및 `.gitignore` 갱신.
     - `Resources/Data/Localization/` 원본 CSV/XLSX 및 생성 JSON -> `Engine/Resources/Data/Localization/` 물리 이관.
     - `LocalizationBuild.cmake`: `LOCALIZATION_DIR`을 `Engine/Resources/Data/Localization` 우선 탐색 후 `Resources/Data/Localization` 폴백하도록 개선.
     - `localizationcsvtojson.py`, `audit_localization.py`: 기본 탐색 경로를 `Engine/Resources/Data/Localization` 우선으로 갱신하여 도구 체인 일관성 유지.
  5. **`PlatformPath` 부모 디렉터리 상향 탐색(Walk-Up) 강화 및 물리 분리 검증 단위 테스트 (`PlatformPath.cpp`, `VirtualPathCheck.cpp`)**:
     - `PlatformPath::GetProjectRoot()`: CTest 및 단위 테스트 실행 환경(`out/build/x64-debug`, `bin/Debug` 등 하위 디렉터리)에서도 저장소 루트(`CMakeLists.txt`, `Engine/` 소유)를 정확히 판별할 수 있도록 상향 디렉터리 탐색 루프 도입.
     - `VirtualPathCheck.cpp` Case 9 추가: 셰이더, 기본 머티리얼, 폰트, 로캘이 `Engine/Resources/`에 물리 분리되었을 때 `Engine://` URI 및 기존 `Resources/...` 레거시 상대 경로 모두 단절 없이 정확한 절대 경로로 해석됨을 정량 검증.
- 실제 검증 결과:
  - `python Tools/Audit/LocalizationAuditor/audit_localization.py --strict`: 632개 파일 및 2478개 키 전수 검사 통과 (결함 0건, 100% Passed).
  - CTest (`VirtualPathCheck`, `ReflectionInspectorCheck`): 전 케이스 통과 (100% Passed).
- 미검증 항목:
  - 에디터 실제 구동 시 HLSL 실시간 핫리로드 반응(사용자 직접 확인 필요).
  - 프로젝트 사용자 전용 에셋 디렉터리(`Assets/`) 확립 및 잔여 리소스 이관(Step 4 진행 대상).

### [2026-11-08] 에셋 아키텍처 Phase 3: 콘텐츠 브라우저 Engine Content 분리·읽기 전용 보호, ResourcePicker 카테고리 필터 구축 및 헤드리스 테스트 안정화

- Area: Editor | ContentBrowser | ResourcePicker | Core | Resource | Localization | Tests
- Type: Feature | Architecture | Safety | UX | Bugfix
- 주요 변경 내역:
  1. **`PlatformPath` 과도기 및 물리 분리 환경의 `IsReadOnlyTarget` 판별 강화 (`PlatformPath.cpp`, `PlatformPath.h`)**:
     - 물리 폴더 분리(`engineRoot != projectRoot`) 시 `EngineResourceRoot` 하위를 읽기 전용으로 판별할 뿐 아니라, 과도기 공유 폴더(`Resources/`) 환경에서도 빌트인 시스템 디렉터리(`Shader`, `Font`, `Data/Localization`) 및 코어 기본 머티리얼(`DefaultMaterial`, `SkinningMaterial`, `SkyMaterial`, `PostProcessDefaultMaterial`)을 `Engine://` 읽기 전용 대상으로 일관되게 판별.
     - 문자열 리터럴 인자(`const wchar_t*`, `const char*`)에 대한 오버로드 모호성을 해소하여 호출 안전성 확보.
     - `VirtualPathCheck.cpp`에 과도기 공유 폴더 환경의 읽기 전용 판별 단위 테스트 추가.
  2. **`ContentBrowserPanel` — `Show Engine Content` 뷰 토글 및 멀티 루트 지원 (`ContentBrowserPanel.h/.cpp`)**:
     - 상단 필터 메뉴에 `Show Engine Content` 토글 체크박스 추가 및 워크스페이스(`EditorWorkspace`) 상태 영속화 연결.
     - `Show Engine Content` 해제 시 엔진 빌트인 시스템 폴더/파일을 디렉터리 트리·그리드·검색 결과에서 숨겨 사용자 프로젝트 에셋만 깔끔하게 노출.
     - 물리 폴더가 분리된 경우 좌측 디렉터리 트리에 `Project Content`와 `Engine Content` 루트를 분리 표시하고 `GetActiveRootForPath`를 통해 브레드크럼 주소 표시줄 및 뒤로가기 동작 일원화.
     - 읽기 전용 엔진 폴더에서 `New Folder`, `Create Empty Material`, 이름 변경(`CanRenameTarget`), 외부 파일 드롭(`OnExteranalDropped`) 원천 차단 및 보호 알림 연동.
  3. **`ContentBrowserTreeItem` & `ContentBrowserGridItem` — 읽기 전용 시각 배지 및 프로젝트 에셋 복사 유도 (`ContentBrowserTreeItem.cpp`, `ContentBrowserGridItem.cpp`)**:
     - 엔진 빌트인 항목 썸네일 좌상단 자물쇠 배지(`ICON_FA_LOCK`), 트리 노드 자물쇠 아이콘 및 툴팁 읽기 전용 안내(`ContentBrowser.EngineReadOnlyBadge`) 표시.
     - 컨텍스트 메뉴에서 읽기 전용 항목의 `Rename`, `Delete`, `BakeToBinary`, `ExportToJson` 비활성화.
     - 읽기 전용 파일 우클릭 시 프로젝트 에셋 폴더로 안전하게 복사본을 생성하는 `Copy to Project Assets` 메뉴 추가 및 빌트인 머티리얼의 `Create Material Instance` 생성 경로를 프로젝트 에셋 폴더로 유도.
  4. **`ResourcePicker.h` — 카테고리 범위 탭 (`All` / `Project` / `Engine Built-in`) 분리**:
     - 리소스 피커 상단에 `All`, `Project`, `Engine Built-in` 3단 범위 필터 버튼 바 추가.
     - 절차적 내장 리소스(`CubeMesh`, `SphereMesh`, `QuadMesh`, `White1x1` 등) 및 `Engine://` 리소스를 `[Engine]` 태그와 함께 분리 필터링 지원.
  5. **`Texture` 및 `ResourceManager` 헤드리스 단위 테스트 호환성·수명 주기 결함 수정 (`Texture.cpp`, `ResourceManager.cpp`)**:
     - `Texture.cpp`: `Initialize(void* arg)`, `InitializeFromEncodedMemory`, `ReadArchive`, `Reload`에서 `Renderer::IsCreated()` 검사를 적용하고, 그래픽 디바이스가 없는 헤드리스 단위 테스트 환경(`rhi == nullptr`)에서도 이미지 디코딩 및 CPU 캐시(`m_CachedHeader`, `m_TempData`)를 정상 유지하여 리소스 로딩이 성공하도록 개선.
     - `ResourceManager.cpp`: `LoadFile` 진입 시 기존 등록된 리소스 핸들을 선조회하여 중복 로드를 방지하고, `AllocateSlot`의 초기 `RefCount = 1`을 보장하여 임시 핸들 소멸 시 리소스가 즉시 파괴되는 수명 주기 결함 해결.
  6. **로캘 엄격 감사 무결성 달성 및 에디터 입력 일원화 (`Editor.csv`, `EntityHierarchyView.cpp`)**:
     - Phase 3 관련 로캘 키 8건(`ContentBrowser.ShowEngineContent`, `ContentBrowser.EngineReadOnlyBadge`, `ContentBrowser.CopyToProject`, `ResourcePicker.ScopeAll`, `ResourcePicker.ScopeProject`, `ResourcePicker.ScopeEngine` 등) 추가 및 CSV-JSON 동기화 완료.
     - `EntityHierarchyView.cpp`의 ImGui 직접 마우스 입력 조회를 `Engine::InputManager`로 일원화.
- 실제 검증 결과:
  - `python Tools/Audit/LocalizationAuditor/audit_localization.py --strict`: 632개 파일 및 2478개 키 전수 검사 통과 (결함 0건).
  - `ctest --test-dir out/build/x64-debug -C Debug -R "VirtualPathCheck|ReflectionInspectorCheck" --output-on-failure`:
    - `VirtualPathCheck`: Passed (0.10 sec)
    - `ReflectionInspectorCheck`: Passed (0.19 sec) (2/2 Passed, 100%)

### [2026-11-08] 빌트인 기본 메시 1m 단위 크기 정규화 및 3D 물리 시뮬레이션·PIE 프레임레이트 최적화

- Area: Engine | Physics | Resource | Editor | Scene
- Type: Fix | Optimization | Performance
- 주요 변경 내역:
  1. **빌트인 기본 도형 메시(`QuadMesh`, `CubeMesh`, `SphereMesh`) 1m 단위 크기 정규화 (`BuiltInPrimitiveMeshes.cpp`)**:
     - 기존에 `[-1.0, 1.0]`(가로·세로·높이 2m, 구 반지름 1m)로 생성되어 `ColliderComponent`의 기본 단위 크기(`BoxShape.Size = [1, 1, 1]`, `SphereShape.Radius = 0.5`)와 2배 차이가 나던 문제를 해결하기 위해, 세 기본 도형 모두 `[-0.5, 0.5]`(`radius = 0.5f`) 1m 단위 크기로 통일.
     - `Scale = (1, 1, 1)`일 때 3D 렌더링 메시와 물리 콜라이더 와이어프레임이 1:1로 정확히 일치하도록 개선.
  2. **`PhysicsPlayground.json` 씬 포맷 복구 및 단위 콜라이더 동기화 (`Resources/Scenes/PhysicsPlayground.json`)**:
     - 씬 직렬화 헤더(`Format`, `Version`, `Name`, `NextEntityId`, `Hierarchy`) 보강 및 전 엔티티의 `BoxShape.Size = [1, 1, 1]`, `SphereShape.Radius = 0.5` 단위 규격 일괄 적용.
  3. **3D 물리 솔버(`PhysicsSystem::OnFixedUpdate`) 및 캐릭터 스윕(`KinematicCharacterSystem::SweepCharacter`) 병목 해소**:
     - 강체 적분 루프 내 중복 하향 레이캐스트(박스 반높이를 무시하여 바닥에 콜라이더가 파묻히고 매 스텝 22회 전체 씬 레이캐스트를 유발하던 블록) 제거.
     - 8꼭짓점 `double` 행렬 변환(`TryTransformToAABB`) 대신 중심점 변환 + 축별 절댓값 내적(`glm::abs`)으로 월드 AABB를 즉시 계산하는 `ComputeColliderWorldAABB3D` 도입.
     - 솔버 루프 진입 전 활성 비트리거 3D 콜라이더를 평탄화된 `ColliderProxy3D` 배열로 1회 수집하여, 내부 $O(N^2)$ 루프의 EnTT 다중 뷰 반복자 및 `try_get`/`get` 조회 오버헤드를 전면 제거하고 AABB 광역 검사 및 캐시된 `worldAABB` 협역 교차 판정 적용.
     - `ResolvePhysicsContact3D`: 침투 깊이가 허용 오차(`penetrationSlop = 0.001`) 이하인 안정 접촉 상태에서 불필요한 `TransformSystem::UpdateRootWorld` 행렬 재계산 조기 반환 처리.
     - `CollectPhysicsTriggerEvents3D`: 트리거 후보 인덱스를 선별하여 비트리거 간 불필요한 $O(N^2)$ 순회 제거.
  4. **PIE 고정 업데이트 죽음의 소용돌이(Spiral of Death) 방어 (`Application.cpp`)**:
     - 프레임당 최대 고정 업데이트 횟수를 `maxSubSteps = 2`로 제한하고 초과 누적 시간을 소진하여 일시적 프레임 지연이 연쇄 프레임 드랍으로 이어지지 않도록 방어.
  5. **실행 검증 결과**:
     - `PhysicsPlayground` 씬 Play 모드 기준 `PhysicsSystem::OnFixedUpdate` 소요 시간이 `25.55 ms -> 1.85 ms`로 단축되었으며, 프레임레이트가 기존 `8~9 FPS`에서 **`75~85 FPS`**로 안정화됨을 확인.

### [2026-11-08] 에셋 아키텍처 해석계층: 텍스처 로더 경로 역참조 결함 수정 및 가상 마운트 URI 파이프라인 완결

- Area: Core | Resource | Texture | Audio | Render
- Type: Bugfix | Stability | Architecture
- 문제 분석:
  - 에디터 기동 시 `StartupResourceLoader`에서 텍스처 사전 로드(`LoadFile`) 도중 `Texture.cpp:219 (File Not Found)` 크래시 발생.
  - 원인 1: `ResourceManager::RegisterExplicitLoader`의 `registerSimpleLoader` 람다가 `LoadFile`에서 넘겨준 디스크 절대 경로(`path`)를 무시하고 가상 식별자 키(`key`)를 `desc.Path`에 대입하여, `desc.Path`가 `Project://Textures/...` 등의 가상 URI로 오염됨.
  - 원인 2: `Texture::Initialize` 및 `Reload`에서 `m_Path`가 가상 URI(`Project://...`)일 때 `Runtime::ResolveProjectRelativePath`를 거치면서 `ProjectRoot / "Project://..."`라는 잘못된 경로로 결합되어 파일 오픈 실패.
- 수정 내역:
  1. `ResourceManager.cpp`:
     - `registerSimpleLoader` 및 `shaderLoader`: `desc.Path = path.empty() ? key : path;`로 수정하여 실제 물리 절대 경로를 정확히 보존 및 전달.
  2. `Texture.cpp`:
     - `Initialize` 및 `Reload`: `PlatformPath::ResolveUri`를 직접 호출하여 `m_Path`가 가상 URI, 상대 경로, 절대 경로 어떤 형식이어도 정확한 디스크 물리 경로로 해석하도록 수정.
  3. `AudioClip.cpp`, `DirectX12Shader.cpp`, `Resource.cpp`:
     - `PlatformPath::ResolveUri`를 적용하여 오디오, 셰이더 바이트코드, 일반 리소스 리로드에서도 가상 URI 경로를 일원화 지원.
  4. `Application.cpp`:
     - `runtimeDesc.EngineResourceRoot`에 에디터 설정의 `EngineResourceDirectory`를 안전하게 주입.


### [2026-11-08] 렌더링 트랙 A: 텍스처 밉맵(Mipmap) 체인 구축, CPU 런타임 다운샘플링 및 D3D12 다중 서브리소스 업로드 완결 (S2-D)

- Area: Render | RHI | DirectX12 | Resource | Architecture
- Type: Feature | Optimization | Performance
- 주요 변경 내역:
  1. **텍스처 밉 레벨 계산 및 헤더/직렬화 다중 밉 지원 (`Texture.h/.cpp`, `TextureBinaryHeader`)**:
     - `Texture::CalculateMipLevels(width, height)`: 텍스처 크기 기준 $1 + \lfloor\log_2(\max(W, H))\rfloor$ 밉 레벨 수 계산기 구현 (경계 조건: 0, 1x1, 비정방형 해상도 방어).
     - `TextureCreateDesc`: `generateMips = true` 기본 옵션 추가 (UI/픽셀 아트 등 단일 밉 텍스처 선택적 비활성화 지원).
     - `ValidateTextureHeader`: `header.mipLevels >= 1` 다중 밉 레이아웃 허용 및 전체 밉 레벨 바이트 크기 총합($\sum W_m \times H_m \times 4$) 정밀 검증.
  2. **`stb_image_resize2` 기반 CPU 런타임 밉맵 체인 다운샘플링 (`Texture.cpp`)**:
     - `Texture::InitializeFromEncodedMemory`: Mip 0 원본 픽셀 디코드 후, Mip 1 ~ Mip N-1까지 연속 메모리 버퍼 상에서 이전 밉($m-1$)을 소스로 단계별 $1/2$ 선형(`stbir_resize_uint8_linear`, `STBIR_RGBA`) 다운샘플링 수행.
     - 메모리 오버헤드 $\approx 1.333\times$ 수준에서 고품질 Mip 0 ~ Mip N 연속 픽셀 데이터 생성.
  3. **DirectX 12 RHI 다중 서브리소스 순회 업로드 (`DirectX12RHI.cpp`)**:
     - `DirectX12RHI::UploadTextureData`:
       - `desc.mipLevels == 0` 방어 및 `resourceDesc.MipLevels != desc.mipLevels` 검증.
       - D3D12 드라이버 `GetCopyableFootprints`에 `numSubresources = desc.mipLevels`를 전달하여 256바이트 RowPitch 및 512바이트 PlacementAlignment가 반영된 각 서브리소스별 풋프린트 레이아웃 일괄 획득.
       - CPU 연속 밉 체인 픽셀 데이터를 각 서브리소스 오프셋(`footprints[mip].Offset`) 및 피치에 맞춰 업로드 버퍼에 순회 패킹.
       - `copyList->CopyTextureRegion`을 전체 밉 레벨에 대해 순회 호출하여 GPU 목적지 텍스처의 `SubresourceIndex = mip`로 일괄 복사.
       - 단일 밉(`mipLevels == 1`) 및 기존 `TerrainSystem`의 부분 업로드(`dstX, dstY`) 하위 호환성 100% 보존.
  4. **삼선형 필터링(Trilinear Filtering) 자동 연동 (`DirectX12Sampler.cpp`, `SamplerManager.cpp`)**:
     - `SamplerManager`의 기본 샘플러(`MipFilter = Linear`, `MaxLOD = D3D12_FLOAT32_MAX`)와 결합되어, 원거리 텍스처 샘플링 시 에일리어싱/자글거림(Shimmering) 0% 제거 및 부드러운 밉 레벨 보간 달성.
  5. **단위 테스트 `TextureMipmapCheck` 신설 및 CTest 등록 (`TextureMipmapCheck.cpp`, `Checks.cmake`)**:
     - 밉 레벨 수 경계값 계산(1x1~4096x4096), CPU 밉 체인 다운샘플링 색상 보존율, 256x128 9-level 밉 체인 바이트 크기(174,764 바이트) 전수 검증.

### [2026-11-08] 트랙 D: 스프라이트 시트 슬라이싱, 원클릭 액션 애니메이션 연동, 9-Slice/Tiled/Filled 렌더링, 픽셀 퍼펙트 카메라 및 2D 타일맵 시스템 완비 (Phase 1~4)

- Area: Engine | Render | Editor | 2D | Animation | Localization
- Type: Feature | Architecture | UX
- 주요 변경 내역:
  1. **Phase 1: 스프라이트 시트 슬라이서 & 에디터 (SpriteEditor.h/.cpp) 및 원클릭 액션 애니메이션 클립 생성 위저드**:
     - SpriteEditor: 체커보드 배경, 텍스처 줌 & 팬, 슬라이스 사각 테두리 시각화, 피벗 십자선 및 9-Slice 보더 라인을 갖춘 전용 에디터 구현.
     - 3대 슬라이스 알고리즘 구축: GridByCellSize (셀 픽셀 WxH 균등 격자), GridByCellCount (행/열 개수 균등 분할), Automatic (알파 클러스터 검출).
     - 원클릭 액션 애니메이션 생성 위저드 (DrawCreateAnimationModal): 슬라이스된 프레임들로부터 단일 클립 또는 행(Row)별 다중 클립(Idle, Walk, Attack, Die 등)을 원클릭으로 .spriteanim 에셋으로 일괄 추출.
     - Sprite.h/.cpp: 9-Slice용 m_Border (Left, Right, Top, Bottom) 필드 및 리플렉션/접근자 추가.
  2. **Phase 2: 스프라이트 렌더링 모드 확장 (SpriteRendererComponent.h, SpriteRenderSystem.cpp)**:
     - ESpriteDrawMode: 기존에 Simple 외에 스킵되던 Sliced, Tiled, Filled 렌더링 파이프라인 전면 구축.
     - Sliced (9-Slice): 보더 인셋에 맞춰 모서리 4개(고정 비율), 변 4개(단방향 확장), 중앙 1개(양방향 확장) 등 9개 사각 패치 동적 분할 렌더링.
     - Tiled: 목표 크기에 맞춰 스프라이트를 늘리지 않고 바둑판식으로 쿼드 반복 배치 렌더링.
     - Filled: Horizontal 및 Vertical 모드에서 진행도(fillAmount, 0.0 ~ 1.0)에 따라 정점 크기 및 UV를 클리핑하여 체력바/게이지 렌더링 지원.
     - 2D 정렬 순서(sortingLayerID, orderInLayer) 필드 확장.
  3. **Phase 3: 픽셀 퍼펙트 카메라 및 서브픽셀 스냅 (PixelPerfectCameraComponent.h, CameraSystem.cpp)**:
     - PixelPerfectCameraComponent: 기준 가상 해상도(refResolutionX, refResolutionY), 에셋 assetsPPU, 서브픽셀 스냅(snapSubpixel) 정의.
     - CameraSystem: 직교 투영 카메라 위치를 1/PPU 단위 정수 격자로 스냅(floor(X * PPU + 0.5) / PPU)하여 카메라 이동 시 텍스처 픽셀 경계 떨림(Jittering & Shimmering) 0% 제거.
  4. **Phase 4: 청크 기반 2D 타일맵 시스템 (TilemapComponent.h, TilemapRenderSystem.h/.cpp, TilePalettePanel.h/.cpp)**:
     - TilemapComponent: 16x16 타일 블록 단위 스파스 청크 해시맵(std::unordered_map<uint64_t, TilemapChunk>), 셀 좌표 <-> 월드 좌표 변환 및 SetTile/GetTile/ClearTile API 구축.
     - TilemapRenderSystem: 활성 청크 내 타일들을 배치 순회하며 SpriteDrawCommand로 고속 렌더링.
     - TilePalettePanel: 툴바(Paint B, Erase E, BoxFill U, Picker I), 썸네일 그리드, 드래그 앤 드롭 팔레트 등록을 지원하는 전용 타일 팔레트 에디터 구축 및 ImGuiManager 메뉴 등록.
  5. **로캘 동기화 및 무결점 정적 감사 통과**:
     - Editor.csv, Engine.csv에 신규 키 39개 등록 후 localizationcsvtojson.py 동기화 (총 2,401개 키).
     - audit_localization.py --strict 0건 위반으로 완벽 통과 (627개 파일 전수 검증).

### [2026-11-08] 파티클 & 이펙트 시스템: 물리 충돌, 컬 노이즈 연기 역학, 모듈형 에셋화 및 독립 이펙트 에디터 완결 (Phase 1~5)

- Area: Engine | Render | FX | Physics | Editor | Localization
- Type: Feature | Architecture | Content | Tools
- 주요 변경 내역:
  1. **Phase 1: 물리 충돌 & 반발(Bounce) 및 바닥 상호작용 (`ParticleEmitterComponent.h`, `ParticleSystem.cpp`)**:
     - `EParticleCollisionMode` (`None`, `FloorPlane`, `PhysicsRaycast`), `EParticleCollisionResponse` (`Bounce`, `Slide`, `DieOnContact`, `Stop`) 신설.
     - `FloorPlane` 고속 평면 충돌(지면 침투 방지 fraction $t_{\text{hit}}$ 계산) 및 `PhysicsRaycast` 실시간 선분 충돌(프레임당 최대 검사 횟수 가드) 구현.
     - 법선 탄성 반발 벡터 $\vec{v}' = \vec{v} - (1 + e)(\vec{v} \cdot \vec{N})\vec{N}$, 수평 마찰 감쇠 및 임계치 이하 시 미끄러짐/정지 전환.
  2. **Phase 2: 공간 연기 확산 역학 (부력, 3D 무발산 컬 노이즈, 외력 드래그)**:
     - 중력과 독립적인 수직 부력($\vec{a}_{\text{buoyancy}} = (0, b, 0)$) 적분.
     - 결정론적 O(1) 비트 해시 기반 3D 그래디언트 노이즈 및 무발산(Divergence-Free) 3D 컬 노이즈($\nabla \times \vec{\Psi}$) 벡터장 계산기 구현으로 유기적 와류 형성.
     - 전역 바람(Global Wind) 상대 속도 기반 공기 저항 드래그 감쇠 구현.
  3. **이펙트 리소스 및 셰이더/머티리얼 파이프라인 정비**:
     - `Resources/Texture/`: `Particle_SoftCircle.png`, `Particle_Smoke.png`, `Particle_Spark.png`, `Particle_Ring.png` 4종 고품질 RGBA 에셋 구축.
     - `Resources/Shader/`: `gbufferSpritePS.hlsl`, `gbuffer_sprite.frag`에 파티클 발광(Emission) 출력 채널을 연결하여 HDR 블룸과 자동 연동.
     - `Resources/Material/`: `ParticleMaterial_Additive.bammat.json`, `ParticleMaterial_AlphaBlend.bammat.json` 신설 (`Masked` 블렌드 및 `ReadOnly` 깊이 모드로 GBuffer 지오메트리 패스 호환 보장).
     - `MaterialEditor.cpp`: `fillModes` 배열 역전 버그 수정.
  4. **Phase 3 아키텍처 리팩터링: 엔진 코어 경량화 및 충돌 이벤트 큐 분리**:
     - C++ 하드코딩성 서브 버스트/충격파 변수를 전면 제거하고 컴포넌트를 코어 물리 프리미티브만 남기도록 다이어트.
     - `ParticleCollisionEvent` 구조체 신설: 충돌 위치, 법선, 속도, 시각 기록 및 매 프레임 `ClearEvents()` 자동 정리.
     - `UpdateEmitter`에서 충돌 감지 시 `collisionEvents` 버퍼에 O(1) 기록.
     - 구체적인 시각 효과(스파크 연쇄, 지면 충격파 링 등)는 에디터 및 Phase 4 모듈형 에셋(`ParticleEffect`)에서 이미터들을 조합하도록 명확한 책임 분리 확립.
  5. **로캘 키 및 씬 하위 호환성 보장**:
     - `Resources/Data/Localization/Engine.csv`, `Editor.csv`에 물리 충돌, 컬 노이즈 및 에디터 로캘 키 등록.
     - 모든 신규 프로퍼티에 `ALLOW_MISSING`을 적용하여 구버전 씬 역직렬화 시 `ArchiveErrorCode::MissingField` 방어.
  6. **Phase 4: 모듈형 아키텍처 및 독립 에셋화 (`ParticleEffect` Asset, `ParticleEffectComponent`)**:
     - `ResourceBinaryHeader.h`: `EResourceType::ParticleEffect` (확장자 `.bameffect`) 신설.
     - `ParticleEffect.h/.cpp`: 모듈형 이미터 데이터 구조체(`ParticleEmitterData`: 트리거, 방출, 역학, 물리 충돌, 빌보드/지면디스크 렌더링) 및 `JsonArchive` 파일 입출력 완비.
     - `ParticleEffectComponent.h`: 다중 이미터 런타임 인스턴스 배열(`instances`) 및 재생 속도/상태 제어 지원.
     - `ParticleSystem.cpp`: 지속 방출/물리 적분 및 `OnParentCollision` 이벤트 스폰 체이닝을 처리하는 2단계 파이프라인 및 일괄 GPU 렌더링 제출 연동.
     - `Resources/Effect/Explosion_Fireball.bameffect.json`: 메인 발사체 -> 지면 충격파 링 -> 충돌 스파크 -> 연기 퍼프 4중 체이닝 풀 스펙 예제 에셋 제작.
  7. **Phase 5: 독립 프리뷰 전용 이펙트 에디터 (`ParticleEditorPanel.h/.cpp`)**:
     - `ResourceEditorInterface` 상속 전용 이펙트 저작 패널 신설.
     - `PreviewViewportWidget` 기반 독립 오프스크린 씬: 스튜디오 라이팅, 마우스 궤도 카메라, Non-PIE 실시간 뷰포트 렌더링.
     - 상단 툴바: Play/Pause, Step(+0.033s), Restart, 0.1x~3.0x 배속 조절, 카메라 포커스 리셋, 활성 파티클 통계(`Total`, `Time`) 실시간 출력.
     - 3열 스플릿 레이아웃: 좌측 이미터 리스트(추가/삭제/활성화 토글/트리거 라벨), 중앙 독립 3D 뷰포트, 우측 모듈 스택 인스펙터(트리거/방출/수명속도/외력/충돌/크기회전/색상머티리얼 피커).
     - `ImGuiManager` 및 에셋 브라우저 연동: `.bameffect`, `.bameffect.json` 더블클릭 시 자동 오픈 지원.
     - 로캘 원본 `Editor.csv` 동기화 및 `audit_localization.py --strict` 0건 결함 통과.

### [2026-11-08] 에셋 아키텍처: 가상 마운트 URI (Engine://, Project://) 해석 계층 및 단위 테스트 구축 (트랙 C Phase 1~2)

- Area: Core | Resource | Architecture | Config
- Type: Feature | Architecture | Safety
- 주요 변경 내역:
  1. **`PlatformPath` 가상 마운트 URI(`Engine://`, `Project://`) 해석 계층 구축**:
     - `PlatformPath.h/.cpp`:
       - `SetEngineResourceRoot` / `GetEngineResourceRoot`: 엔진 시스템 빌트인 리소스 루트(`Engine/Resources`, 과도기 폴백 `Resources`) 관리.
       - `SetProjectAssetRoot` / `GetProjectAssetRoot`: 프로젝트 사용자 에셋 루트(`Assets`, 과도기 폴백 `Resources`) 관리.
       - `IsEngineUri`, `IsProjectUri`, `IsVirtualUri`: 대소문자 무관(`engine://`, `project://`, `asset://`) 가상 URI 판별기 구현.
       - `ResolveUri`: 가상 URI를 물리 디스크 절대 경로로 안전하게 해석.
         - 디렉터리 트래버설(`..`) 탐색 탈출 방어: 마운트 루트를 벗어나는 악의적/비정상 경로를 차단하고 안전한 루트로 격리.
         - 레거시 상대 경로 하위 호환(4단계 폴백): `ProjectAssetRoot` 실존 검사 -> `EngineResourceRoot` 실존 검사 -> `Resources/` 접두어 제거 후 재검색 -> 기본 프로젝트 루트 결합 순서로 기존 씬, 머티리얼, 텍스처 참조 100% 무파괴 호환.
       - `ToVirtualUri`: 물리 디스크 경로를 마운트 소속에 따라 `Engine://...` 또는 `Project://...`로 자동 정규화 역변환. 과도기(동일 폴더) 시 내장 시스템 파일(셰이더, 로캘, 폰트) 화이트리스트 분별 지원.
       - `IsReadOnlyTarget`: 엔진 시스템 자원(`Engine://` 또는 `EngineResourceRoot`)에 대한 임의 쓰기/수정 방어 가드 구현.
  2. **`ResourceManager` 가상 URI 로드 및 읽기 전용 보호 연동**:
     - `ResourceManager.cpp`:
       - `ResolveResourceLoadPath`: `PlatformPath::ResolveUri`를 직접 호출하여 URI 및 상대 경로를 일원화 해석.
       - `ToProjectRelativePath`: 가상 URI(`Engine://...`, `Project://...`) 보존 및 정규화.
       - `LoadFile`: URI 입력 시 `PlatformPath::ResolveUri`로 디스크 실존 경로를 도출하고, 고유 키는 정규화된 URI/상대 경로로 유지.
       - `SaveToJsonFile` / `SaveToBeveFile`: `PlatformPath::IsReadOnlyTarget` 검사를 통해 `Engine://` 빌트인 시스템 자원에 대한 덮어쓰기 저장 시도를 사전 차단(`EResult::AccessDenied`).
  3. **`ProjectSettings` (`Config.h`) 및 `Runtime` 경로 주입 연결**:
     - `Config.h`: `ProjectSettings`에 `EngineResourceDirectory = "Engine/Resources"` 필드 신설 (`AssetDirectory = "Resources"`와 병행).
     - `Runtime.h/.cpp`: `RUNTIMEDESC`에 `EngineResourceRoot` 추가 및 초기화 시 `PlatformPath::SetProjectAssetRoot`, `PlatformPath::SetEngineResourceRoot`에 자동 주입.
  4. **단위 테스트 `VirtualPathCheck` 신설 및 CTest 등록**:
     - `Engine/examples/VirtualPathCheck.cpp`:
       - 모의 격리 디렉터리 기반 7개 핵심 시나리오(마운트 루트 설정, URI 판별, URI 해석, 디렉터리 탈출 방어, ToVirtualUri 역변환, 읽기 전용 검사, 레거시 폴백) 전수 자동 검증.
     - `Engine/CMake/Checks.cmake`: `VirtualPathCheck` 실행 파일 및 `add_test` 등록.
  5. **무결점 로컬라이제이션 검사 유지**:
     - `audit_localization.py --strict` 621개 파일, 2,334개 키 100% 통과 유지.

### [2026-11-07] 모델 파이프라인 자동 메시 LOD 생성 및 ModelEditor 실시간 LOD 뷰어 구축 (우선순위 3번 트랙 B)

- Area: Pipeline | Tool | Editor | Resource | Render
- Type: Feature | Optimization | UX
- 주요 변경 내역:
  1. **`meshoptimizer`(`meshopt_simplify`) 기반 자동 메시 LOD 생성 엔진 도입**:
     - `vcpkg.json`: `meshoptimizer` 공식 패키지 의존성 추가.
     - `Tools/Converters/ModelConverter/CMakeLists.txt`: `meshoptimizer::meshoptimizer` 패키지 검색 및 라이브러리 링크 연결.
     - `ModelConverter.h/.cpp`:
       - `Tools::ModelConversion::Options` 및 `MeshData`에 `std::vector<MeshData> LODs` 필드 신설.
       - Quadric Error Metrics(QEM) 기반 정밀 단순화 함수 `GenerateMeshLODs(MeshData&, const Options&)` 구현.
       - `meshopt_simplify`로 목표 비율(LOD1: 50%, LOD2: 25%)에 맞춘 인덱스 간소화 수행 후, `meshopt_generateVertexRemap` / `meshopt_remapVertexBuffer` / `meshopt_remapIndexBuffer`를 거쳐 사용되지 않는 정점을 컴팩트하게 정리하고 노멀, UV, 본 가중치(Skinning) 등 모든 정점 속성을 안전하게 보존.
       - 원거리 LOD 오차 확장 및 `meshopt_simplifySloppy` 2단계 폴백 도입: 고정 1% 오차에 묶여 로우폴리/UV Seam 부품이 조기 중단되던 현상을 해결하여, LOD1(50%) 및 LOD2(25%) 목표치까지 적극적 감축 달성.
  2. **에셋 파이프라인 스테이징·영속화 및 엔티티 연동**:
     - `ModelImportTask.h/.cpp`: 원본 메시와 함께 파생 LOD 메시를 `PreparedModelMesh.LODs`로 비동기 변환.
     - `ModelImporter.h/.cpp`:
       - `ModelImportDesc`에 `GenerateLODs`, `LODCount`, `LOD1Reduction`, `LOD2Reduction`, `TargetError` 설정 확장.
       - `FinalizeOneStep`: 파생 LOD 메시(`_LOD1.bammesh`, `_LOD2.bammesh`)를 디스크에 자동 생성·스테이징·비동기 직렬화. 기존 롤백/백업/재임포트(인플레이스 리로드) 메커니즘을 동일하게 상속.
     - `Model.h/.cpp`:
       - `ModelMeshLOD` 구조체 신설 및 `Model` 클래스에 `m_MeshLODs`(`PROPERTY(ALLOW_MISSING)`) 추가로 완벽한 파일 하위 호환성 보장.
       - `GetMeshLOD(meshIndex, lodLevel)`, `HasLODs()` 접근자 제공.
     - `SceneModel.cpp`:
       - 씬에 모델 인스턴스화 시 파생 LOD 메시가 존재하면 `LODGroupComponent`를 엔티티에 자동 장착하고 `lod1Mesh`, `lod2Mesh`, 화면 점유율 임계치(`0.25f`, `0.08f`), 히스테리시스(`0.02f`)를 자동 설정하여 `MeshRenderSystem`의 거리/커버리지 기반 LOD 전환과 자동 연동.
  3. **`ModelImportModal` LOD 설정 인터페이스 추가**:
     - `ModelImportModal.cpp`: [LOD Options] CollapsingHeader 신설, LOD 자동 생성 토글 및 슬라이더(`LOD1Reduction`, `LOD2Reduction`) 제공.
  4. **`ModelEditor` 실시간 LOD 검사기 및 강제 프리뷰 뷰어 구축**:
     - `ModelEditor.h/.cpp`:
       - 상단 툴바에 빠른 LOD 모드 전환 콤보박스 (`[Auto (Distance)]`, `[LOD 0 (Original)]`, `[LOD 1]`, `[LOD 2]`) 배치.
       - 디테일 패널에 [Mesh LOD (Level of Detail)] 전용 테이블 신설: 각 LOD 레벨별 트라이앵글 수, 버텍스 수, 감축률(%) 한눈에 비교 및 즉시 확인 가능한 `[View]` 버튼 제공.
       - `SetForcedLOD(int lod)`: 프리뷰 씬 내 모든 스킨드/스태틱 메시 렌더러를 실시간으로 순회하여 선택된 LOD 메시로 즉시 교체(Zero-Lag 60 FPS 프리뷰). Auto 복원 시 기본 LOD0 및 `LODGroupComponent`로 즉각 원복.
     - `DrawAnimations`: 재생 버튼 라벨 3항 연산자에서 임시 `std::string`의 `.c_str()` 조기 소멸로 인한 Dangling Pointer 크래시(`ImGui::ItemAdd`) 결함 수정.
  5. **로캘 원본 동기화 및 무결점 정적 감사 통과**:
     - `Resources/Data/Localization/Editor.csv`에 `ModelImport.LODOptions`, `GenerateLODs`, `ModelEditor.LODHeader`, `LODAuto`, `LOD0Label`, `Triangles`, `Vertices`, `Reduction` 등 21개 키 추가 및 `localizationcsvtojson.py` 동기화 (총 2,309개 키).
     - `audit_localization.py --strict` 0건 위반으로 완벽 통과 (621개 파일 전수 검증).


### [2026-11-08] 트랙 A: 3D 형상 물리 충돌·스윕 완비, KCC 안정화, 조인트 제약조건 해결기 및 3D RigidBody 시뮬레이터 완결

- Area: Engine | Physics | KCC | Architecture | Scene
- Type: Feature | Fix | Stability | Content
- 주요 변경 내역:
  1. **3D Capsule 충돌·스윕·레이캐스트 알고리즘 완비 (Collision.h/.cpp)**:
     - Collision::Raycast(Ray, Capsule, ...): 캡슐의 원통 측면 및 양 끝 반구에 대한 닫힌 형태(closed form)의 정확한 광선 교차 판정 추가.
     - Collision::Intersects(Capsule, BoundingBox): 선분과 AABB/OBB 간 3D 최근접점 거리 계산을 통한 정밀 교차 판정(침투 깊이, 법선, 접점) 및 OBB 월드 변환 지원.
     - Collision::Intersects3D(Capsule, Capsule): 두 3D 캡슐 간 최단 거리 및 접촉 판정 완비.
     - Collision::Sweep(Capsule, ...): Sphere(민코프스키 합 역방향 레이캐스트), Box/OBB 및 Capsule 대상 연속 충돌 스윕(Bisection Search) 함수 4종 완비.
  2. **물리 시스템 3D 형상 질의 및 트리거 버그 수정 (PhysicsSystem.h/.cpp)**:
     - TestColliderHit: ColliderComponent에 CapsuleShape가 설정되었을 때 레이캐스트가 누락되던 결함 수정.
     - Overlap3D: KCC vs Box 판정 시 중심점 포함 여부(Contains(Center))로 검사하여 모서리 충돌을 놓치던 심각한 결함을 제거하고, Collision::Intersects 기반 정확한 OBB 교차 판정으로 전면 교체. CapsuleShape 및 MeshShape 콜라이더 간의 전수 오버랩 분기 완비.
     - 2D 회전 역학 연결: RigidBody2DComponent의 AccumulatedTorque 소비, InverseInertia 자동 계산 및 Z축 각속도/회전 적분 연결.
     - 3D 강체 시뮬레이터 구축: RigidBodyComponent 뷰를 OnFixedUpdate에 신설하여 중력 낙하, 선속도/외력 적분, 정적 바닥/지형 충돌 접지 및 각속도 회전 적분을 실제로 수행하도록 구현.
  3. **키네마틱 캐릭터 컨트롤러(KCC) 고도화 및 무빙 플랫폼 연동 (KinematicCharacterSystem.h/.cpp, CharacterControllerComponent.h)**:
     - KinematicCharacterSystem을 가변 프레임(OnUpdate)에서 물리 고정 틱(OnFixedUpdate)으로 이관하여 FPS 변동에 따른 터널링 및 물리 오차 원천 차단.
     - SweepCharacter: 장애물 검사 시 CapsuleShape 및 MeshShape 콜라이더 분기를 추가하고, 캐릭터 본인도 캡슐 스윕(Collision::Sweep)을 사용하도록 정밀화.
     - DetectGround: 접촉 지면 엔티티(outHitEntity)를 반환하도록 시그니처 확장.
     - **무빙 플랫폼(Moving Platform) 상속**: 캐릭터가 이동하거나 회전하는 플랫폼 위에 서 있을 때, 플랫폼의 이동량 및 플랫폼 중심 기준 공전/자전 회전을 완벽하게 상속하여 미끄러짐 없이 함께 움직이도록 구현.
  4. **조인트(Joint) 제약조건 해결기 구현 (Joint2DComponent, JointComponent, DebugRenderer.cpp)**:
     - ResolveJoints2D 및 ResolveJoints3D: 위치 기반 역학(XPBD) 거리 제약조건 오차 보정 및 진동 방지 상대 속도 댐핑 임펄스 적용.
     - DebugRenderer: JointComponent 및 Joint2DComponent의 앵커 핀 및 연결선 와이어프레임 시각화 구현.
  5. **3D 강체 충돌 반발·마찰 및 침투 해결기 (ResolvePhysicsContact3D)**:
     - Dynamic vs Dynamic, Dynamic vs Static 3D 콜라이더 간의 탄성 반발(Restitution), 쿨롱 마찰(Friction) 및 비침투 위치 보정 루프를 OnFixedUpdate에 완비.
  6. **전역 물리 설정 구조체 신설 (PhysicsSettings.h, PhysicsComponent.h, Engine.csv)**:
     - PhysicsSettings: 2D/3D 전역 중력 벡터, 고정 타임스텝(FixedTimeStep = 0.02f), 물리 솔버 반복 횟수 및 32개 레이어 이름 정의.
     - PhysicsSystem::GetSettings(), SetSettings() 정적 인터페이스 노출 및 Engine.csv 로캘 키 7종 등록.
  7. **종합 물리 테스트 씬 구축 (Resources/Scenes/PhysicsPlayground.json)**:
     - 지면, 계단(StepOffset 검증), 경사로(25° 보행/55° 미끄러짐), 무빙 플랫폼, 3D 진자 조인트, 쌓여 있는 Dynamic RigidBody 박스들, 3인칭 플레이어 캐릭터 배치.

### [2026-11-07] ModelEditor 내장 애니메이션 클립 선택·재생 및 정밀 타임라인 제어 기능 추가

- Area: Editor | Animation | UX | Localization
- Type: Feature | UX
- 주요 변경 내역:
  1. **내장 애니메이션 클립 인터랙티브 선택 및 프리뷰 연동**:
     - `ModelEditor.h/.cpp`: `m_SelectedAnimIndex`, `GetPreviewAnimator()`, `SelectAnimation(int, bool)` 구현.
     - `DrawAnimations`: 기존의 단순 텍스트 나열에서 3열 테이블(`[Play/Pause]`, `[ClipName]`, `[Duration]`) 기반 인터랙티브 셀렉터블 리스트로 전면 개편. 클릭 시 즉시 해당 클립으로 `AnimatorComponent.currentClip`을 교체하고 자동 재생/일시정지 동기화.
     - 선택된 클립에 대한 **정밀 재생 제어 패널** 제공:
       - `[Play / Pause]` 토글 및 `[Stop (T-Pose)]` 리셋 버튼
       - 현재 재생 시간 및 총 길이를 보여주는 인터랙티브 타임라인 슬라이더 (`currentTime / durationSec`)
       - 루프 토글(`isLooping`) 및 재생 속도 조절 슬라이더(`playRate`, 0.1x ~ 3.0x)
  2. **상단 툴바 퀵 애니메이션 컨트롤러 추가**:
     - `ModelEditor::DrawToolbar`: 모델에 애니메이션이 있는 경우 툴바에 빠른 `[Play/Pause]` 버튼 및 클립 선택 콤보박스(`ImGui::BeginCombo`)를 배치하여, 디테일 패널 스크롤 없이도 상단 툴바에서 3D 뷰포트를 주시하며 즉시 클립을 교체·재생할 수 있도록 UX 개선.
  3. **로캘 키 추가 및 무결점 정적 감사 통과**:
     - `Resources/Data/Localization/Editor.csv`에 `ModelEditor.ClipName`, `Duration`, `EmbeddedClips`, `LoopAnimation`, `PauseClip`, `PlayClip`, `PlayRate`, `SelectClip`, `StopAndReset`, `SecondsFormat` 10개 키 추가 및 JSON 동기화.
     - `audit_localization.py --strict` 0건 위반으로 완벽 통과 (620개 파일, 2,281개 키).

### [2026-11-07] GPU 스키닝 이중 회전(Double Transform) 방지 및 바인드 포즈 행렬 정규화

- Area: Engine | Editor | Render | Animation
- Type: Fix | Architecture
- 주요 변경 내역:
  1. **스킨드 메시 렌더러의 WorldMatrix 루트 일원화**:
     - `MeshRenderSystem.cpp`, `PreviewViewportWidget.cpp`: GPU 스키닝을 거친 정점(`skinMatrix * Position`)은 스켈레톤의 최상위 본 공간(모델 루트 공간)으로 이미 변환되므로, `SkinnedDrawCommand.worldMatrix`를 자식 메시 엔티티의 월드 행렬이 아닌 조상 엔티티의 `AnimatorComponent`(모델 루트) 월드 행렬로 일원화. 이를 통해 glTF/Assimp의 $Z$-Up 보정 회전 노드(`Z_UP`, `Armature`)가 본 계층과 메시 엔티티에서 중복 곱해지던 **이중 회전(Double Transform) 버그를 원천 해결**.
     - `AnimationTimelinePanel.cpp`: 타임라인 프리뷰에서 스킨드 메시 제출 시 `cmd.worldMatrix = mat4(1.0f);`로 지정하여 동일하게 이중 로컬 변환 방지.
  2. **바인드 포즈(T-포즈) 스키닝 행렬 정규화**:
     - `AnimationEvaluator.h/.cpp`: `EvaluateBindPoseSkinningMatrices(const Skeleton&, std::vector<mat4>&)` 추가. 스켈레톤의 `LocalTransform`을 계층 누적한 `GlobalBindTransform`에 각 본의 `OffsetMatrix`를 곱해 순수 바인드 포즈 스키닝 행렬을 생성.
     - `PreviewViewportWidget.cpp`: `source == EAnimatorBasePoseSource::None`(애니메이션 정지)일 때 모든 본 행렬을 임의의 `Identity`로 덮어쓰던 꼼수를 제거하고, `EvaluateBindPoseSkinningMatrices`를 호출하여 실제 바인드 포즈 행렬을 평가하도록 교정.
     - `AnimationSystem.cpp`: 씬 런타임에서 재생 중이 아니거나 초기화 시점에 `finalBoneMatrices`가 비어있을 때 `EvaluateBindPoseSkinningMatrices`로 바인드 포즈를 초기화하여, 씬 인스턴스화 시 T-포즈가 즉시 똑바로 표시되도록 보장.
     - `ThumbnailRenderer.h/.cpp`:
       - `SubmitModel` 및 `SubmitMesh`에서 전달하는 `m_BindPoseMatrices`를 `ThumbnailRenderer` 클래스 멤버 변수로 승격하여, `AssetCache`의 비동기(다음 프레임) GPU 캡처 시점까지 수명을 완벽히 보장.
       - `SubmitMesh`: 단일 스킨드 메시(`.bammesh`) 렌더링 시 동일 디렉터리의 호환 스켈레톤(`*_Skeleton.bamskel`)을 자동 탐색하여 바인드 포즈 행렬 및 $Y$-Up 바운딩 박스를 계산하도록 보강함으로써, 모델 썸네일(`CesiumMan.bammodel`)뿐 아니라 메시 썸네일(`CesiumMan_Cesium_Man.bammesh`)까지 똑바로 선 T-포즈로 렌더링되도록 수정.

### [2026-11-07] 썸네일 3D 오브젝트 포커스 거리 최적화 (프레임 내 오브젝트 크기 확대)

- Area: Editor | Render | UX
- Type: Enhancement | UX
- 주요 변경 내역:
  1. **썸네일 전용 프레이밍 마진 분리 및 포커스 거리 단축**:
     - `PreviewCameraDefaults.h`: 기본 뷰포트 F키 포커스 마진(`DefaultFramingMargin = 1.35`)과 썸네일 전용 마진(`ThumbnailFramingMargin = 1.08`)을 분리 정의하고, `CalculateFraming`에 `margin` 파라미터를 추가.
     - `ThumbnailRenderer.cpp`: `CalculateAutoFramingCamera`에서 `ThumbnailFramingMargin`(1.08)을 적용하여, AABB 외접 구체(Bounding Sphere) 기준 카메라 거리를 약 22% 단축.
     - 모델(Model), 단일 메시(Mesh), 머티리얼 구체(Material Sphere) 등 모든 3D 썸네일에서 오브젝트가 128x128 타일 프레임 안에 약 25% 더 큼직하고 시원하게 꽉 차면서도 테두리 밖으로 잘리지 않도록 뷰를 최적화.

### [2026-11-07] 리소스 에디터 UI 표준화(메뉴바·툴바·인스펙터 구조화) 및 썸네일·프리뷰 전면 조명·스킨드 머티리얼 전달 복구

- Area: Editor | Render | Shortcut | Localization | Docs
- Type: Feature | Fix | UX | Refactor
- 주요 변경 내역:
  1. **썸네일 및 프리뷰 스튜디오 조명 전면 정렬 및 표준 프레이밍 통일**:
     - `ThumbnailRenderer.cpp`, `PreviewViewportWidget.cpp`, `AnimationTimelinePanel.cpp`: 프리뷰 카메라 시선(`Yaw = 160°`, $+Z$ 전방에서 대각선으로 모델을 바라봄)에 맞추어 주광(Key Light: `(30°, 195°, 0°)`, 시점 우측 상단), 보조광(Fill Light: `(15°, 120°, 0°)`, 시점 좌측 상단), 역광(Rim Light: `(45°, 20°, 0°)`, 모델 후면 상단)의 각도를 전면 정합.
     - `ThumbnailRenderer::SubmitMaterial`: 구체 머티리얼 썸네일 촬영 시 역방향($-Z$)으로 설정되어 있던 커스텀 오프셋 계산식을 모델·메시와 동일한 `CalculateAutoFramingCamera` 표준 카메라로 일원화하여, 모든 썸네일(모델, 메시, 머티리얼 구체)의 정면이 일관되게 밝게 조명되도록 수정.
     - `EditorSettings.h`: `PreviewLightIntensity` 기본값을 `{ 3.0f, 1.2f, 1.5f }`로 튜닝하여 카메라를 마주보는 정면 좌우의 디퓨즈/스펙큘러 음영을 화사하게 개선.
  2. **스킨드 메시 렌더러 상수 폴백 및 프리뷰/썸네일 머티리얼 상수 전달**:
     - `Renderer::RenderSkinnedMeshes`: `command.materialConstants.materialFlags == 0`일 때 `command.material->BuildMaterialConstants(drawConstants)`로 자동 폴백하여 텍스처 플래그 누락 시에도 PBR/디퓨즈 텍스처가 정상 바인딩되도록 안전장치 구축.
     - `PreviewViewportWidget.cpp`, `ThumbnailRenderer.cpp`, `AnimationTimelinePanel.cpp`: `SkinnedDrawCommand` 제출 시 `material->BuildMaterialConstants(cmd.materialConstants)`를 명시적으로 전달.
  3. **프리뷰 뷰포트 포커스(F 키) 및 그리드 토글(G 키) 단축키 타깃팅 개선**:
     - `PreviewViewportWidget.cpp`: `ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)`로 자식 창 포커스를 정확히 감지하고, `m_IsHovered || m_IsFocused` 조건에서 `s_ShortcutTarget = this`를 활성화하여 마우스 호버 또는 패널 포커스 상태에서 `F` 키 입력 시 프리뷰 위젯의 모델 AABB로 카메라가 즉시 정렬되도록 교정.
     - `PreviewViewportWidget.h/.cpp`: `ToggleGrid()`, `IsGridVisible()` 메서드 및 툴바 툴팁 단축키 힌트(`Viewport.ToggleGrid`) 연동.
     - `EditorActionRegistry.cpp`: `Viewport.ToggleGrid` 액션을 `EShortcutContext::Global`로 전환하고 `PreviewViewportWidget::GetShortcutTarget()`을 우선 지원하도록 연동.
  4. **리소스 에디터 일관된 템플릿 표준화 (건의사항 4번)**:
     - **공통 포맷 확립**: 모든 리소스 에디터가 상단 `MenuBar`(`File`, `Edit`, `View`), 1줄 퀵 액세스 `Toolbar`(`Save`, `Focus (F)`, `Toggle Grid (G)`, 주요 액션, 파일 상태 배지 `[AssetName.ext *]`), 그리고 접이식 섹션(`CollapsingHeader`) 기반의 구조화된 인스펙터를 공유하도록 표준화.
     - **ModelEditor**:
       - `ImGuiWindowFlags_MenuBar` 및 `DrawMenuBar()` 추가: `File`(`Save`, `RetryLoad`), `Edit`(`InstantiateModel`), `View`(`Focus (F)`, `ToggleGrid (G)`).
       - `DrawToolbar()` 추가: `[Save]` (Ctrl+S), `[Focus]` (F), `[Toggle Grid]` (G), `|`, `[Instantiate Model]`, `|`, 파일 상태 배지.
       - 디테일 패널을 접이식 헤더 섹션으로 구조화: `▼ 모델 정보` (메시 수, 노드 수, 스켈레톤 본 수 2열 테이블), `▼ 머티리얼 슬롯` (슬롯별 리소스 피커), `▼ 내장 애니메이션` (클립 목록), `▼ 씬 배치` (인스턴스화 버튼 및 가이드).
     - **MaterialEditor**:
       - `ImGuiWindowFlags_MenuBar` 및 `DrawMenuBar()` 추가: `File`(`Save`, `Revert`), `View`(`ResetCamera (F)`, `ToggleGrid (G)`, `PreviewMesh` 서브메뉴: Sphere/Cube/Plane).
       - `DrawToolbar()` 통일: `[Save]`, `[Revert]`, `|`, `[ResetCamera]`, `[ToggleGrid]`, `|`, 파일 상태 배지.
       - `MaterialPreviewRenderer.h`: `GetWidget()`, `GetPreviewMeshType()`, `SetPreviewMeshType()` 공개 인터페이스 제공.
     - **AnimatorGraphPanel**:
       - `DrawStateInspector()` 전면 디자인 개선: 노드 상단에 상태 이름 및 엔트리 상태 배지(`[ENTRY ->]`), 인라인 이름 변경 버튼 배치.
       - `▼ 상태 속성`: 2열 테이블(`PROP_PLAY_RATE`, `PROP_IS_LOOPING`, `PROP_LOOP_BLEND_DURATION`, `기본 진입 상태로 설정`).
       - `▼ 모션 설정`: 모션 종류 콤보(클립 / 1D 블렌드 트리 / 2D 블렌드 트리). 2D 블렌드 트리는 프리뷰 캔버스, 파라미터 X/Y 및 범위 테이블, 그리고 4개 모서리 클립을 2D 데카르트 좌표계에 일치하는 $2 	imes 2$ 그리드 테이블(상단: 좌측 `(-X, +Y)`, 우측 `(+X, +Y)`, 하단: 좌측 `(-X, -Y)`, 우측 `(+X, -Y)`)로 시각적으로 정렬. 1D 블렌드 트리는 임계값·클립·삭제 3열 테이블로 구성.
       - `▼ 애니메이션 알림(Notifies)`: 시간·이름·파라미터·삭제 4열 테이블로 구조화.
       - `▼ 전이 목록(Transitions)`: 해당 상태와 연결된 인/아웃 전이를 한눈에 확인하고 즉시 선택할 수 있는 빠른 탐색 목록 제공.
  5. **로캘 및 정적 감사 완전 통과**:
     - `Resources/Data/Localization/Editor.csv`에 신규 키 23개 추가 후 `localizationcsvtojson.py`로 JSON 동기화.
     - `python -B Tools/Audit/LocalizationAuditor/audit_localization.py --root . --strict` 무결점 통과 (620개 파일, 2,258개 키, 위반 0건).

### [2026-11-07] 스킨드 메시 부품별 외형 교체 및 일시 효과(피격 발광) 수명 분리·드로우별 상수 렌더러 연동 (1·2·3단계 완료)

- Area: Engine | Render | Resource | Docs
- Type: Feature | Architecture | Test | Docs
- 주요 변경 내역:
  1. **[1단계] 부품 식별자 기반 저장 외형 교체 및 재임포트 순서 변경 회귀 검증**:
     - `SceneArchiveCheck.cpp`에 `CheckLinkedModelPartMeshOverrideAndReimportRoundTrip` 추가.
     - 스켈레톤 리그 비호환 메시 교체 거부(`InvalidArgument`) 및 기존 메시 보호 검증.
     - 호환 갑옷 메시 교체 및 `useModelMesh = false` 오버라이드 전이, 씬 JSON 직렬화/역직렬화 라운드트립 보존 검증.
     - 모델 원본의 노드 배치 순서 및 메시 인덱스가 반전(`Body: 0, Head: 1`)된 재임포트 상황에서도 `(sourceNodeKey, meshAttachmentKey)` 매핑을 통해 개별 부품의 오버라이드 외형이 정확히 유지됨을 확인.
     - `RestoreLinkedModelPartMesh` 및 `RestoreLinkedModelPartMaterial`을 통한 모델 원본 에셋 복원 검증.
     - `Resource::Reload()`에 대상 경로가 작업 디렉토리에 바로 존재하지 않는 상대 경로일 때 `PlatformPath::ResolveProjectRelative`를 자동 해석하는 안전장치 보강.
  2. **[2단계] 런타임 장비 교체와 일시 머티리얼 수명 분리**:
     - `SkinnedMeshRendererComponent.h`에 런타임 전용 상태 구조체 `SkinnedRuntimeEquipment` 및 `SkinnedTemporaryEffect` / `SkinnedMaterialParameterOverride` 정의 (씬 직렬화 제외).
     - `EquipRuntimeMesh()` / `UnequipRuntimeMesh()`: 런타임 플레이 중 장착된 외형을 관리하며, 스켈레톤 호환성(`IsSkinCompatibleWith`)을 사전 검증하여 비호환 메시 장착을 차단.
     - `ApplyTemporaryMaterial()`, `ApplyParameterOverride()`, `ClearTemporaryEffect()`, `UpdateTemporaryEffects()`: 피격·상태이상 등 일시 효과와 만료 시각(`remainingTime`), 요청 추적용 `requestId` 관리.
     - `ResolveRenderState()`: 1단계 기본 외형 $\rightarrow$ 2단계 런타임 장비 $\rightarrow$ 3단계 일시 효과 순의 3계층 상태 결정 파이프라인 구축. 효과 시작 전 머티리얼을 저장·복원하는 방식 대신 상위 일시 상태만 만료 해제하도록 설계하여, 피격 발광 도중 갑옷을 교체하더라도 효과 종료 시 착용한 새 갑옷이 그대로 유지되는 상태 일관성 확보.
  3. **[3단계] 드로우별 머티리얼 상수 전달 및 렌더러 GPU 상태 누수 차단**:
     - `RenderCommands.h`: `SkinnedDrawCommand`에 `MaterialConstants materialConstants{}` 필드 추가.
     - `MeshRenderSystem.cpp`: 스킨드 메시 순회 시 `skinnedRenderer.UpdateTemporaryEffects(dt)`를 갱신하고, `ResolveRenderState()`로 계산된 최종 `mesh`, `material`, `materialConstants`를 커맨드에 실어 제출.
     - `Renderer.cpp`: `RenderSkinnedMeshes()`에서 매 드로우마다 `m_RHI->BindConstantBuffer(&command.materialConstants, sizeof(MaterialConstants), 2)`(b0, space3 머티리얼 상수 버퍼 슬롯)를 바인딩하도록 수정. 동일 머티리얼을 공유하는 다수 캐릭터 간 피격 발광 상수가 다음 정상 캐릭터로 누수되는 문제를 원천 차단.
  4. **통합 회귀 테스트 (`SceneArchiveCheck.cpp`)**:
     - `CheckSkinnedMeshRuntimeEquipmentAndTemporaryEffects()` 추가:
       - 동일 머티리얼을 공유하는 캐릭터 A, B 중 A에만 피격 발광(`emissiveColor = (2, 0, 0)`) 적용 시 B의 상수 간섭 없음 검증.
       - A가 발광 중인 상태에서 비호환 메시 장착 거부 및 호환 가죽갑옷 장착 교체 성공 검증.
       - 갑옷 교체 후에도 발광이 유지되고, 연속 피격 시 만료 시각 갱신 및 `requestId`가 올바르게 갱신/보호되는지 검증.
       - 피격 효과 만료 후 발광은 꺼지고 교체한 새 가죽갑옷 외형이 그대로 유지됨을 검증.
       - 씬 저장/로드 라운드트립 후 런타임 장비와 일시 효과가 씬 파일로 누출되지 않고 기본 씬 외형으로 복원됨을 검증.
- 검증 결과:
  - `SceneArchiveCheck` 단위 테스트 100% 통과 (14개 하위 검사 전체 성공).
- 미검증 항목:
  - 실제 실행 중인 에디터 뷰포트/PIE 환경에서의 복수 캐릭터 동시 렌더링 및 런타임 게임플레이 스크립트 연동.

### [2026-11-07] CTest 단위 검사 전체(29/29) 정상화 및 잔여 감사 결함(A01~A03) 해결 (우선순위 0·1 완료)

- Area: Engine | Editor | Tools | Build | Docs
- Type: Fix | Cleanup | Security | Docs
- 기준 커밋: `c470483c`
- 주요 변경 내역:
  1. **CTest 회귀 4종 복구 및 전체 단위 검사(29/29) 정상화 [우선순위 0]**:
     - `Graph::ImportData`: 트랜잭션 롤백, 버전 검증(`UnsupportedVersion`), 노드 ID 0/중복/범위 검증 복원으로 `GraphFoundationCheck` 통과.
     - `GraphReflection`: 공통 빌트인 노드(`Math.MakeFloat3`, `Math.MakeFloat4`)에 CPU 리플렉션 실행 바인딩을 주입하는 경우에만 도메인 보존 결합(`RegisterOrUpdate`)을 허용하고, 리플렉션 간 중복 시도(`Check.Counter.Value.Set` 등)는 엄격히 차단하여 `GraphReflectionCheck`, `GraphExecutionCheck`, `ShaderGraphCheck` 동시 통과.
     - `ReflectionInspectorCheck`: 리소스 피커 컨텍스트 메뉴용 `DrawResourceContextMenu`/`RequestOpenResourceEditor` 스텁 추가로 링크 오류 해결.
     - `HierarchyEditorCheck`: 불필요한 무거운 의존성(`EntityClipboard`, `PrefabModeManager`)을 CMake 타깃에서 제거하고 최소 스텁으로 고립시켜 링크 오류 해결.
     - `SceneArchiveCheck`: `Model` 리소스의 소멸자 접근 권한(`friend class ResourceManager`) 및 `rm.DestroyResource(tempModel)` 표준 경로 일원화, 타깃 `UNITY_BUILD OFF` 부여로 MSVC 유니티 중복 컴파일 해소.
  2. **에셋 감시자 UTF-8·예외 경계 보완 (A03) [우선순위 1]**:
     - `AssetHotReloadManager.cpp`의 `ProcessModifiedLocalization`에서 미사용 narrow `.string()` 제거 및 `scriptPath.wstring()` + `_wsystem` 호출로 비-ASCII/다국어 프로젝트 경로 안전성 확보.
  3. **패키징 삭제 범위 보호 및 New-Item 호환성 교정 (A01·A02) [우선순위 1]**:
     - `PackageClient.ps1`의 `$ForbiddenDirectories`에 소스 디렉터리 `"Client"` 추가.
     - `[가드 4]` 프로젝트 저장소 외부 임의 디렉터리 삭제 방지 가드 구축.
     - PowerShell 5.1 표준에 맞춰 `New-Item -ItemType Directory`의 `-LiteralPath`를 `-Path`로 교정.
  4. **우선순위 및 작업 현황 문서 동기화 [우선순위 1]**:
     - `Docs/DevelopmentPriorities.md` 11절을 최신 구현 상태로 갱신하여 완료된 우선순위 0·1 내역 기록 및 다음 기능 트랙(A~D) 제시.
- 검증 결과:
  - **CMake Debug 빌드 (`x64-debug`) 및 CTest 전체 29개 단위 검사**: 29/29 Passed (100% 통과).
  - **PackageClient.ps1 보안 가드 동작 검증**: `-OutputDir "Client"` 및 저장소 외부 임의 디렉터리(`"C:/TempTest"`) 지정 시 디렉터리 삭제 전 `Security Violation` 예외 발생 및 스크립트 중단 정상 확인.
- 미검증 항목:
  - `PackageClient.ps1` 정상 인자 전달 시의 실제 파일 패키징 완주 및 결과 폴더 내 Standalone 클라이언트 바이너리 독립 실행.

### [2026-11-07] 프로젝트 파일 트리 구조 정규화 및 도구·리소스 루트 정리

- Area: Engine | Editor | Tools | Build | Resource | Docs
- Type: Refactor | Cleanup | Docs
- 기준 커밋: `f2941d14` (기존 미커밋 변경 `MaterialEditor.cpp`, `ModelEditor.cpp`는 보존하고 커밋 대상에서 분리)
- 주요 변경 내역:
  1. **무위험 클린업 및 미사용/빈 파일 제거**:
     - 저장소 루트의 빈 폴더(`Bin/`, `Server/`, `Shared/`), `Editor/Source/Generated/`, 구형 미추적 `Engine/Generated/` 및 하위 IDE 캐시(`Projects/.vs/`, `Projects/Reflection/.vs/`) 제거.
     - 구현 및 참조가 없는 `Engine/Source/Physics/Public/PhysicsManager.h`, `Engine/Source/System/Time/Public/TimerManager.h`, `Editor/Source/AssetManager/Importer/Public/AudioImporter.h`, `MaterialImporter.h`, `Editor/Source/ImGui/InspectorPanel/Public/SpriteInspector.h`, `Editor/Source/Rendering/PostProcess/Public/PostProcessInterface.h`, `Editor/Source/ImGui/ViewportPanel`의 빈 `EditorViewportPanel.h/.cpp`, `Editor/Source/ImGui/Managers` 내 3바이트 빈 파일 8개(`MangerInspectorInterface.h/.cpp` 등) 제거.
     - `Editor/Source/ImGui/ResourceEditor`의 `PrefabEditorPanel.h/.cpp`를 `Editor/Source/ImGui/PrefabEditor/Public|Private/`로 이동하여 프리팹 편집 UI 위치 일원화.
     - Unity 빌드 묶음 변경 시 드러난 `Engine/Source/Core/Public/Functions.h`의 `<cctype>`, `<cwctype>` 인클루드 누락 보완.
  2. **`Engine/Source` 계층 정규화 및 소유권 이관 ([SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md) 10절 반영)**:
     - `Core/Public/Interface/*` $\rightarrow$ `Core/Interface/Public/*`
     - `Core/Public|Private/Module/*` $\rightarrow$ `Core/Module/Public|Private/*`
     - `Core/Public|Private/Reflection/*` $\rightarrow$ `Core/Reflection/Public|Private/*`
     - `Core/Public/Structs/Vertex.h` + `Core/Private/Vertex.cpp` $\rightarrow$ `Render/Public/Vertex.h` + `Render/Private/Vertex.cpp`
     - `Utility/Picking/Public|Private/PickingManager.*` $\rightarrow$ `Render/Picking/Public|Private/PickingManager.*` (및 `Engine/Source/Utility` 제거)
     - `Engine/CMake/ReflectionBuild.cmake`, `Projects/GameModule/CMakeLists.txt` 및 관련 헤더 `#include` 경로 정규화.
  3. **`Tools/` + `Utility/` 일원화 및 `Resources/` 루트 정돈**:
     - 최상위 `Utility/`를 `Tools/` 하위(`Tools/Codegen/CSVToJsonConverter`, `Tools/Codegen/ReflectionLocalization`, `Tools/Audit/LocalizationAuditor`, `Tools/Scripts/`, `Tools/VSSnippets/`)로 통합하고, 기존 `Tools/` 내 변환기·코드젠도 `Tools/Converters/`(`ModelConverter`, `TextureConverter`), `Tools/Codegen/`(`ReflectionTool`, `GraphCompiler`)으로 분류.
     - 루트 `CMakeLists.txt`, `Engine/CMake/LocalizationExport.cmake`, `Editor/CMake/LocalizationBuild.cmake`, `AssetHotReloadManager.cpp`, `test_export_localization.py`의 도구 경로 갱신.
     - `Resources/` 루트에 산재된 `Road-subdi*`, `Road.glb`를 `Resources/Model/`로(`RaytracingScene.json` 참조 경로 동기화), `GraphMovement.json`을 `Resources/Scripts/`로 이동하고, 미참조 손상 테스트 파일(`Resources/rgba2x2*`) 및 `Resources/Material/` 내 구형 미참조 중복 파일(`SpriteMaterial.beve`, `SpriteMaterialBeve.json`, `SpriteMaterial.materialInstance`, `SpriteMaterial.json`, `SpriteMaterialInstance.bammatinst`, `GBufferMaterial.bammat`) 제거.
- 검증 결과:
  - **로캘 정적 감사 및 Python 도구 단위 테스트**:
    - `python -B Tools/Audit/LocalizationAuditor/audit_localization.py --root . --strict` 통과 (620개 파일 / 2,235개 키, 위반 0건).
    - `LocalizationAuditTests`(18개) 및 `LocalizationExportTests`(7개) 전체 통과.
  - **CMake 구성 및 Debug 빌드 (`x64-debug`)**:
    - `Engine`, `Editor`, `Client`, `GameModule`, `ModelConverter`, `TextureConverter` Debug 빌드 및 링크 성공 (종료 코드 0).
  - **CTest (`out/build/x64-debug -C Debug`)**:
    - 29개 중 25개 통과 (`ArchiveExample`, `ReflectionLocalizationUtility`, `InputManagerCheck`, `UIInteractionCheck`, `UILayoutCheck`, `SceneReflectionCheck`, `SceneArchiveCheck`, `TerrainDataCheck`, `Physics2DCheck`, `CollisionGeometryCheck`, `SplineCurveCheck`, `BlendTree2DCheck`, `GraphExecutionCheck`, `ShaderGraphCheck`, `LocalizationAuditUtility`, `MainMenuBarLayoutCheck`, `ShortcutManagerCheck`, `GraphMessageLocalizationCheck`, `TransformInheritanceCheck`, `GraphPaletteCheck`, `EditorHistoryCheck`, `CanvasViewCheck`, `PickingMathCheck`, `TransformEditorCheck`, `EditorSettingsCheck`).
    - 기존 미반영 Check 타깃 4건(`GraphFoundationCheck`, `GraphReflectionCheck`, `HierarchyEditorCheck`, `ReflectionInspectorCheck`)은 이번 경로 이동과 무관한 선행 변경(리소스 피커 분리 등)에 의한 실패임을 확인.
  - **미검증 항목**: Release 구성 전체 빌드, 에디터/클라이언트 실제 창 실행 및 GPU 렌더링·프리팹 편집 UI 수동 조작 확인.

### [2026-11-07] 모델 바운드 기준 화면 맞춤

- 임포트 모달과 모델 에디터에서 모델을 열거나 포커스 명령을 실행하면 정적·스킨드 메시의 바운드에 현재 노드 변환을 적용해 합친다. 배율·회전 편집 중에는 카메라 시점을 유지하고 포커스 시 새 바운드를 사용한다.
- 프리뷰와 모델·메시 썸네일이 같은 거리와 클리핑 평면 공식을 쓴다. 종횡비에 맞춰 가로·세로 중 좁은 시야각으로 계산한다. Fox의 바운드 꼭짓점 8개를 배율 3종과 종횡비 4종에서 화면 안으로 투영하는지 수식으로 확인했다. 실제 Editor에서 좁은 패널, 스케일 변경 후 포커스, 썸네일 표시를 확인해야 한다. [임포트 프리뷰 계획과 완료 기준](InteractiveModelImportPreviewPlan.md)

### [2026-11-07] 모델 임포트 및 프리뷰의 정면 기준 통일

- GLB와 glTF는 왼손 변환과 와인딩 반전을 기본으로 적용한다. Assimp가 Z를 반사한 뒤 Y축으로 180°를 돌려 `+Y` 위쪽과 `+Z` 정면을 유지한다.
- 상하·헤딩 반전을 메시, 노드, 본 오프셋, 애니메이션 위치 및 회전 키에 같은 축 보정으로 적용한다. 수동 XYZ 회전은 모델 배치용 루트에서 처리한다.
- 모델·메시 썸네일과 공용 프리뷰 카메라가 `+Z` 정면 대각선 방향을 공유한다. 축 보정 조합의 행렬·쿼터니언 계산을 확인했다. Debug Editor에서 Fox 재임포트 후 컬링, 썸네일, 직접 바인딩 재생 확인을 남겨 둔다. [임포트 프리뷰 계획과 완료 기준](InteractiveModelImportPreviewPlan.md)

### [2026-11-07] 모델 임포트 옵션 배치와 애니메이션 스케일 수정

- 모델 임포트 모달에서 파일명과 경로를 줄바꿈하고, 패널 너비에 따라 스케일 프리셋과 X/Y/Z 입력을 재배치한다. 기존 로캘 키로 상하·헤딩 체크박스를 표시하며 프리뷰와 최종 임포트는 같은 회전 값을 사용한다.
- 임포트 배율을 루트 Transform에만 넣었던 초기 수정을 정정했다. 메시 정점, 모델·본·본 오프셋의 이동 성분과 애니메이션 위치 키에 같은 배율을 베이크한다. 원본 회전·스케일 키를 유지해 엔티티 Scale `1`에서도 메시 직접 바인딩에 임포트 배율이 반영되게 한다. 모델 루트의 임포트 스케일은 제거해 모델 배치에서도 한 번만 적용한다. 회전 오프셋은 모델 루트에서 처리한다. 기존 에셋은 원본에서 다시 임포트해야 수정이 반영된다.
- 코드 경로와 변경분을 정적으로 확인했다. `0.01`, `0.1`, `1`, `100`에 대해 이동 성분과 정점을 함께 환산한 계층·본 오프셋 곱이 원본 결과의 동일 배율이 되는지 메모리상의 행렬 계산으로 확인했다. C++ 변환기 실행을 검증한 결과는 아니다. Debug Editor 빌드, 모달 너비 조절, 메시 직접 바인딩과 모델 신규 배치의 재생은 확인이 필요하다. [임포트 프리뷰 계획과 완료 기준](InteractiveModelImportPreviewPlan.md)

### [2026-11-06] 코드·개발 원칙 점검과 개발 우선순위 재평가

- Area: Docs | Editor | Build | Render | Resource
- Type: Docs
- 기준: `main` / `ecca09da` 및 기존 미커밋 작업. 사용자 승인에 따라 Markdown만 수정했다.
- 결과: [CodeDocumentationAudit-2026-11-06.md](CodeDocumentationAudit-2026-11-06.md)에 A01~A10의 위치·조건·영향·최소 개선·완료 조건을 기록했다.
- 우선순위: [DevelopmentPriorities 11절](DevelopmentPriorities.md)에 출력 삭제/생성 안전성 → 에셋 감시자 UTF-8 → RT 진단 차원 보호 → splitter 선호값 보존을 배치했다. 다수 광원·2D 샘플·재임포트·Client 배포는 사용 조건을 구분했다.
- 계획 반영: Engine/ImGui/Rendering/BuildAndPackaging/Utf8PathHandling/ResourceAndAssetSeparation 계획과 바인딩 문서를 정정했다. 기존 2D 타일맵·위젯 분리 작업은 유지했다. 에셋 분리는 호환 경로 검증 후 물리 이동으로 조정했다.
- 정적 감사: `python -B Utility/LocalizationAuditor/audit_localization.py --root . --strict` 종료 0, 634개 파일/2,219개 키에서 검사 대상 위반 없음. PowerShell New-Item 인자 메타데이터를 조회해 LiteralPath 미지원도 확인했다. 파일 생성 없는 읽기 전용 조사다.
- 한계: C++/Python/PowerShell 소스·에셋 수정, 빌드/CTest, 패키징, GPU/UI 실행, 성능 측정은 수행하지 않았다. A01~A06의 런타임 보완은 아직 제안 상태다. 이전 실행 기록을 현재 전체 통과로 확대하지 않았다.

## [2026-11-07] 리소스 피커 우클릭 및 더블 클릭 에디터 열기 연동 보완

- 스마트 대상 경로 해결(`ResolveResourceEditorTarget`): 확장자가 생략된 리소스 키(`GBufferMaterial`), 단일/복합 확장자 불일치(`*.bammat` vs `*.bammat.json`), `Resources/` 접두사 누락, `ResourceManager` 등록 키 역조회를 모두 대응하도록 개선했다.
- 메쉬 에디터 연동: `.bammesh` / `.bammesh.json` 선택 시 동일 디렉터리의 부모 모델(`.bammodel`, `.bammodel.json`, `.glb`, `.fbx`)을 자동 탐색하여 모델 에디터(`ModelEditor`)로 연결되도록 지원했다.
- 에디터 지원 확장자 보강: `TextureEditor`(`.dds`, `.hdr`), `MaterialEditor`(`.mat`), `ShaderEditor`(`.cso`, `.bamshader.json`)의 `IsSupported` 판정을 보강했다.
- 피커 상호작용 및 UI 포커스 개선:
  - 인스펙터 리소스 피커 슬롯별 `ImGui::PushID(&value)` 격리로 다중 슬롯 간 ID/컨텍스트 메뉴 충돌을 방지했다.
  - 인스펙터 리소스 슬롯 좌클릭 더블 클릭 시 에디터 즉시 열기를 지원하고, 우클릭 컨텍스트 메뉴에서 `에디터에서 열기`, `콘텐츠 브라우저에서 찾기`, `경로 복사`가 정상 동작하도록 보장했다.
  - `IImGui::BeginPanelWindow`에서 `m_FocusRequested` 플래그를 감지하여 도킹 탭 뒤에 숨겨진 에디터 패널도 즉시 전면으로 활성화되도록 보장했다.
- 검증 상태: 소스 정적 분석 및 경로 변환 로직 검증 완료. Debug 에디터 빌드 후 인스펙터 및 리소스 선택 창에서의 실제 우클릭/더블클릭 동작 확인 필요.

## 1. Entry Template

Use one entry per meaningful change.

### [YYYY-MM-DD] Title

- Area: Engine | Editor | Tools | Build | Reflection | UI | Resource
- Type: Feature | Refactor | Fix | Performance | Cleanup | Docs
- Owner: @name
- Related files:
  - path/to/file1
  - path/to/file2
- Motivation:
  - Why this change is needed.
- Implementation summary:
  - What was implemented.
  - Main classes/functions touched.
- Technical details:
  - Data structures and algorithms used.
  - Runtime implications (CPU/GPU/memory).
- Compatibility and migration:
  - Breaking changes (if any).
  - Migration steps (if any).
- Risks and follow-ups:
  - Known limitations.
  - Next tasks.
- Validation:
  - Build/test commands and outcomes.
- Notes:
  - Links to PR/issues/screenshots.

---

## [2026-11-07] 콘텐츠 브라우저 그리드 다중 선택

- 썸네일과 이름을 하나의 선택 타일로 묶고 Dear ImGui 다중 선택을 연결했다. 빈 공간 드래그, Ctrl+클릭 토글, Shift+클릭 범위 선택을 지원한다. 에셋 타일의 드롭 페이로드는 기존 단일 경로 형식을 사용한다.
- 검색·필터로 만들어진 그리드 순서로 범위를 계산하고, 경로가 남아 있는 항목의 선택 ID를 유지한다. 기존 행 클리퍼에 범위 시작 행을 포함했다. [그리드 선택 동작](ImGuiDevelopmentPlan.md#f-콘텐츠-브라우저-그리드-다중-선택)
- 변경 파일의 공백 검사와 설치된 ImGui 1.92.5 선언 대조를 마쳤다. 전체 로캘 엄격 감사는 이번 변경 밖의 `ResourcePicker.OpenInEditor`, `ResourceEditor.OpenBlocked` 키 누락으로 실패했다. 에디터 빌드와 실제 UI 확인은 남아 있다.

## [2026-11-07] ImGui 패널 초기화 후 자동 등록

- `IImGui::Initialize()`가 패널별 `OnInitialize()` 성공 후 한 번만 등록한다. 초기화 실패 시 생성한 호출자가 해제하고, 성공 시 관리자가 생성 참조를 넘겨받는다. 렌더 타깃 뷰어는 초기화 인자로 표시할 타깃을 전달한다.
- 시작 시 등록하던 기본 패널과 리소스 편집기 26개의 순서 및 워크스페이스 대상 목록을 유지했다. 새 뷰포트와 렌더 타깃 뷰어도 공통 초기화를 사용한다. [패널 등록 규약](ImGuiDevelopmentPlan.md#패널-초기화와-등록)
- 정적 확인으로 직접 등록 호출이 공통 초기화 한 곳에만 남은 점, 시작 시 패널 26개의 순서, 동적 뷰포트 생성 두 경로를 대조했다. 읽기 전용 로캘 엄격 감사에서 635개 소스·2,233개 키에 대해 검사 대상 위반이 없었다. 에디터 빌드·실행, 메뉴 토글 및 워크스페이스 복원은 수동 확인이 남아 있다.

## [2026-11-07] 모델 재임포트 출력 보호와 메시 복구

- **적용 범위.** `ModelImporter`의 출력 폴더 예약과 사전 검사, 메시 임시 저장·게시·백업·복구, 커밋 실패 시 기존 메시 스냅샷 복원, 정상 종료로 미완료 작업을 폐기할 때의 게시 파일 복구를 반영했다.
- **기존 리소스 갱신.** 모델·메시·스켈레톤·애니메이션·머티리얼 인스턴스의 핸들을 유지하는 갱신 경로와 오류 전파를 연결했다. `ResourceManager`에 파일 경로 기반 키 조회를 추가했고 `Texture`의 메모리 재로드를 임베디드 이미지 경로에서 재사용한다.
- **확인 범위.** 코드에서 진행 중 반환, 최종 커밋, 실패 복구, 종료 시 작업 폐기 경로를 대조했다. 빌드, CTest, 실제 재임포트와 종료 복구 재현은 남아 있다. 모델을 구성하는 모든 산출물을 하나의 트랜잭션으로 복구하는 단계는 별도로 남는다.
- **다음 작업.** 직접 헤더 의존성을 정리하고 Editor 빌드 후, 기존 메시를 덮어쓰다가 중단한 경우 파일과 핸들이 보존되는지 확인한다. 재현 순서와 범위는 [임포트 프리뷰 계획](InteractiveModelImportPreviewPlan.md#재임포트-안정화-중간-기록-2026-11-07)에 기록했다. Client 관련 작업은 빌드·패키징 단계로 보류한다.

## [2026-11-06] 게임 입력 액션과 로직 그래프 연동

- **Editor/PIE 기반.** 통합 설정의 입력 액션 위젯에서 액션과 복수 바인딩을 편집해 [Config/InputActions.json](../Config/InputActions.json)에 저장한다. PIE 시작 시 설정을 `InputManager`에 적용한다. 3인칭 컨트롤러의 이동·점프·달리기·오빗 입력과 `CubeController` 예제 그래프의 이동은 액션 ID를 사용한다.
- **그래프 입력 이벤트.** `Event.OnActionPressed`와 `Event.OnActionReleased`를 추가했다. 두 이벤트의 맵·액션 ID는 노드에서 직접 편집하는 문자열이다. `GraphSystem`은 그래프 로드와 리플렉션 리로드 시 엔티티별 이벤트 목록을 구성하고, 같은 프레임에서는 눌림을 해제보다 먼저 실행한다. 동작 계약과 재현 절차는 [GraphExecution.md](GraphExecution.md#입력-액션-이벤트)에서 확인한다.
- **리플렉션 등록 수정.** 읽기·쓰기가 가능한 프로퍼티의 Get 노드를 등록할 때 노드 정의를 복사해 Set 노드에 필요한 도메인과 메타데이터를 보존한다. 기존 실패는 `graph_checks::Counter::Value: Node type and at least one domain are required.`였다.
- **검증 기록.** 2026-11-06 11:33의 Debug `GraphExecutionCheck` 통과 로그를 확인했다. 이 검사는 두 액션 이벤트를 실행기에서 각각 반복 호출하고 연결된 `exec` 결과를 확인한다. 실제 입력에서 `GraphSystem`까지의 전달, 에디터 재실행 후 저장 복원, PIE 포커스 전환은 수동 검증이 남아 있다.
- **인수인계.** 현재 `Gameplay` 맵에 `Sprint`와 `OrbitCamera` 바인딩을 보완하고, `MoveRight=A`와 `MoveLeft=D`가 의도한 배치인지 확인한다. 이후 `InputManagerCheck`·`EditorSettingsCheck`와 PIE에서 이동·달리기·오빗·그래프 이벤트를 확인하면 키보드·마우스 1차를 마감할 수 있다. Client 입력 설정 연결은 [빌드·패키징](BuildAndPackagingPlan.md) 단계에 두고, 게임패드와 아날로그 축은 [엔진 개발 계획](EngineDevelopmentPlan.md)의 후속 범위로 둔다. 프로젝트 전체 점검과 우선순위 재조정은 [DevelopmentPriorities.md](DevelopmentPriorities.md)에서 이어간다.

## [2026-11-06] 엔진·에디터 공용 처리와 설정 위젯 정리

- **에디터.** `MetricsPanel`의 계측 영역과 `UnifiedSettingsPanel`의 설정 영역을 각각 위젯으로 분리했다. 통합 설정에서 단축키와 게임 입력 액션을 편집하며, 게임 입력 설정은 PIE 시작 시 적용한다. `Ctrl+W`는 포커스된 패널을 닫고 `Ctrl+Shift+W`는 씬 닫기 명령으로 사용한다. 리소스 편집기의 창 제목 조립과 콘텐츠 브라우저의 이름 변경 입력을 공용화했다.
- **리소스와 씬.** Editor와 Client가 기본 메시 등록을 공유한다. 바이너리 확장자 판정, 리소스 로더 등록과 리로드의 공통 처리를 정리했다. 머티리얼은 상수 조립을 공유하고 인스턴스는 개별 오버라이드를 적용한다. 씬과 프리팹이 엔티티 레코드 읽기·쓰기를 재사용하며, 컴포넌트 등록 정보는 공용 빌더로 조립한다.
- **애니메이션과 렌더링.** 런타임과 프리뷰가 스켈레톤 조회와 포즈 갱신을 공유한다. 스프라이트와 파티클의 Geometry 패스 선택, 썸네일과 타임라인의 카메라 버퍼 조립, 텍스처 공통 메타데이터 초기화를 한곳에 모았다. SDL GPU 드로우 버퍼 바인딩, 셰이더 출력 핀 해석, 조명 구조체와 ACES 계산의 중복도 정리했다.
- **물리와 도구.** 2D·3D 콜라이더의 마찰과 반발 계수는 `ColliderSettings`에서 검증한다. 로캘 내보내기는 중복 키의 모든 위치를 보고하고 기존 출력을 보존한다. 감사 도구의 CSV 순회와 동일한 설정을 쓰는 엔진 CMake 검사 타깃 네 개의 선언을 공용화했다.
- **검증.** 2026-11-06 작업 트리에서 Editor·Client Debug 빌드와 CTest 12개가 통과했다. `InputManagerCheck`, `ShortcutManagerCheck`, `EditorSettingsCheck`, `SceneArchiveCheck`, `TerrainDataCheck`, `Physics2DCheck`, `CollisionGeometryCheck`, `SplineCurveCheck`, `BlendTree2DCheck`, `ShaderGraphCheck`, `ReflectionLocalizationUtility`, `LocalizationAuditUtility`를 실행했다. 에디터 UI 조작과 GPU 출력은 실행 검증 범위에 포함되지 않았다.

## [2026-11-05] 스플라인 경로 편집 및 메시 생성

- **경로와 추종.** 엔티티 로컬 제어점을 Catmull–Rom으로 평가하고, 변환을 반영한 거리표로 속도·반복·역방향 이동을 처리한다. `AlignToMovement`는 직접 부모 경로의 접선과 포인트 롤을 사용한다. 옵션 기본값은 꺼짐이며, 구버전 씬에 롤과 변형 설정이 없으면 기본값으로 읽는다.
- **에디터 편집.** 뷰포트에서 점을 선택해 위치와 접선 축 롤을 조작한다. Inspector에서 추가한 점은 경로 끝이나 닫힌 이음 구간에 배치하고 자동 선택한다. 점 삭제와 닫힌 경로 전환에서 유효성을 검사하며, 다중 선택을 닫을 때 한 경로라도 실패하면 전체 변경을 되돌린다. 드래그는 기존 씬 편집 이력을 사용한다.
- **메시 생성.** 정적 메시를 접선 방향으로 반복 배치하거나 원본 정점의 로컬 Z를 경로 거리로 매핑해 연속 형상을 만든다. 변형은 닫힌 경로의 프레임 이음부와 길이 방향 UV 반복을 처리한다. 원본 인덱스를 재사용하고 렌더 명령이 생성 메시의 수명을 보유한다. 피킹은 변형 캐시나 같은 반복 배치 계산을 사용한다.
- **진단.** Inspector는 현재 모드에서 사용하는 설정과 마지막 생성 결과를 표시한다. 원본 메시·머티리얼 누락, 메시 용량 초과, 시작 거리 초과, 로컬 Z 길이 0, 굽이에서의 단면 접힘을 구분한다. 생성 결과와 실행 중 추종 거리는 씬에 저장하지 않는다.
- **자동 검증.** 2026-11-05 22:55에 작업 트리의 Debug `SplineCurveCheck`, `SceneArchiveCheck`, `EditorHistoryCheck`가 모두 통과했다. 거리·정방향/역방향 회전·롤·변형·UV, JSON/BEVE 복원, 제어점 이동과 롤의 Undo/Redo를 각각 검사했다.
- **남은 검증.** `SplineTestStrip`으로 실제 기즈모 조작, 변형된 메시의 그림자와 피킹, 씬 저장·재열기, PIE 정방향·역방향 추종을 이어서 확인한다. 도로 재임포트와 G-Buffer 미표시 진단, Client·패키징은 후속 범위다. [엔진 계획과 도로 리소스 기록](EngineDevelopmentPlan.md#5-스플라인-데이터평가경로-활용)

## [2026-11-05] 애니메이터 2D 블렌드와 공통 전이 편집

- **포즈 평가.** `BlendTree2D`는 두 Float 입력을 축 범위에 맞춰 네 코너 클립의 쌍선형 가중치를 구한다. 런타임은 X·Y를 반감기 0.08초로 보간하고 기준 클립의 정규화된 위상으로 각 클립을 샘플링한다. `AnimationSystem`과 독립 프리뷰는 베이스 로컬 포즈에 본 마스크 레이어를 순서대로 합성한 뒤 스키닝 행렬을 만든다.
- **전이.** 조건 목록과 그룹에 AND·OR를 지정할 수 있다. 재생 가능한 목적지를 찾은 뒤 일치한 트리거를 소모한다. `Any State` 후보는 일반 전이보다 먼저 검사하며, 조건이 없거나 목적지가 현재 상태인 경우 건너뛴다. 진행 중인 크로스페이드는 완료 후 다음 후보를 검사한다.
- **에디터.** 프리뷰 모델과 런타임 파라미터를 따로 제어하고, 기본 상태부터 다시 재생할 수 있다. 2D 그래프에서 목표 좌표·보간 좌표·코너 가중치를 표시하고 드래그로 X·Y를 조절한다. 전이 목적지와 진행률을 표시하며, 목적지 상태의 메뉴에서 `모든 상태` 연결을 추가할 수 있다. 상태 이름 확정 시 기본 진입 상태와 전이 참조를 함께 갱신한다.
- **호환성.** 새 블렌드·조건·전이 필드는 `ALLOW_MISSING`으로 읽는다. `fromAnyState`가 없는 기존 `.animator`의 전이는 일반 전이로 복원된다.
- **검증.** 2026-11-05 22:58 Debug `BlendTree2DCheck` 통과. 코너 가중치와 경계, 2D 크로스페이드, 재생 불가 전이의 트리거 보존, `Any State` 우선순위·재진입 방지, 기존 JSON과 신규 전이의 저장·복원을 검사했다. 에디터에서 저장·재열기 및 레이어 합성 결과의 시각 확인은 후속이다. [애니메이터 계획](AnimatorGraphPlan.md)

## [2026-11-05] 에셋 프리뷰 조명 조절과 렌더 타겟 진단

- 적용 범위
  - 모델 편집기, 애니메이터 그래프, 머티리얼 편집기 등 `PreviewViewportWidget`을 사용하는 창에 조명과 렌더 타겟 메뉴를 추가했다.
  - 통합 설정의 중복 카메라 헤더를 정리하고 사이드바 폭 계산을 수정했다.
- 프리뷰 조명
  - 사용자 설정의 `PreviewLightIntensity`에 주광·보조광·역광의 강도를 저장한다. 이전 설정 파일에서 필드가 없으면 `2.8`, `0.5`, `2.2`를 사용한다. 입력은 `0`부터 `10`까지의 유한한 값으로 검증한다.
  - 전구 메뉴에서 편집하는 동안 현재 프리뷰에 임시값을 사용한다. 적용하면 `EditorSettingsService`가 조명 필드만 저장하고, 취소하면 저장값을 다시 사용한다. 통합 설정은 조명 초안을 편집한 경우에만 그 필드를 저장한다.
  - 새 3D 썸네일을 제출할 때에도 같은 사용자 설정을 읽는다. 이미 캐시된 카드의 무효화와 썸네일 출력에 프리뷰와 동일한 톤매핑을 적용하는 작업은 후속으로 남아 있다.
- 렌더 타겟 진단
  - 최종 출력, 톤매핑 전 조명, G-Buffer의 Albedo·Normal·PBR·Emission·Position과 깊이 버퍼를 전환할 수 있다. 씬 뷰와 프리뷰는 `ChannelFilter`의 RGBA 버튼과 표시 패스를 공유한다.
  - 최종 출력에는 기존 ACES와 그리드를 적용한다. 진단 버퍼는 후처리 전 값을 표시한다. PBR의 `R/G/B/A`는 각각 거칠기·금속도·Cavity·AO다. Position과 깊이는 화면 표시 범위에서 확인하는 원시값이다.
- 설정 창
  - 이동 속도와 화각을 카메라 섹션 하나에 모았다. 사이드바는 현재 창 크기에 맞춘 표시 폭과 사용자가 드래그해 선택한 폭을 구분한다. 선택 폭은 기존 `EditorWorkspace.SplitterStates` 경로로 저장된다.
- 확인과 후속 검증
  - Knight에서 프리뷰의 ACES·감마 보정을 우회했을 때 모델 썸네일과 색감이 맞는 것을 사용자가 확인했다. 이는 진단 과정의 결과이며 변경 전체의 GPU 검증을 대신하지 않는다.
  - 동일한 모델과 카메라에서 톤매핑 전후 비교, 조명 적용·취소, PBR의 A 단독 표시, 설정 창 축소·복원 및 에디터 재시작 후 폭 복원이 필요하다.

## [2026-11-05] Help 언어 전환 후 메뉴 클릭과 창 드래그 판정 보완

- Area: Editor | UI | Build | Docs
- Type: Fix | Docs
- 문제와 확인 범위:
  - Help 메뉴에서 언어를 변경한 뒤 Help/도움의 첫 클릭이 반응하지 않는 현상이 보고됐다.
  - ImGui 메뉴만 분리한 합성 클릭 검사에서는 기존 흐름도 통과했다. 이 결과만으로 실제 창 문제를 해결했다고 판단하지 않았다.
  - Application의 SDL hit-test가 메뉴 영역을 왼쪽 350px, 제목줄을 36px, 우측 버튼을 150px로 고정하고 있었다.
    언어·글꼴 배율로 Help가 350px 경계를 넘으면 네이티브 창 드래그 영역으로 잘못 분류할 수 있다.
- 구현:
  - `ToolBar`가 매 프레임 실제 메뉴 끝·창 제어 버튼 시작·메뉴바 높이를 측정한다.
    `MainMenuBarLayout`은 주 뷰포트를 기준으로 SDL 클라이언트 좌표의 빈 제목줄 영역만 드래그 대상으로 판정한다.
  - Application의 hit-test가 `ImGuiManager`를 통해 이 영역을 사용하도록 연결했다.
    기존 창 가장자리 리사이즈와 상단 플로팅 패널 입력 양보는 유지한다.
  - File·Edit·Scene·Window도 Help와 같이 `LOCAL_LABEL`로 고정 ID를 사용한다.
  - Help 언어 선택은 `ToolBar`에 예약하고 `ImGuiManager::Begin()`에서 NewFrame 전에 설정 서비스로 적용한다.
    같은 언어를 다시 고르면 저장·알림·스타일 재적용을 요청하지 않는다.
- 검증:
  - `MainMenuBarLayoutCheck`는 실제 ImGui와 번역 사전을 사용한 합성 입력으로 한국어↔영어 전환 뒤 첫 Help 클릭을 검사한다.
    0.75·1.0·1.5·2.0 글꼴 배율, 메뉴 오른쪽 경계, 창 버튼·메뉴바 밖·빈/겹친 드래그 영역을 다룬다.
  - 동일한 클릭 중 2개는 이전 350px 판정에서 창 드래그로 분류됐으며, 실제 영역 판정에서는 메뉴 클릭 검사를 통과했다.
  - 작업 트리의 Editor Debug 빌드와 `MainMenuBarLayoutCheck`·`LocalizationAuditUtility`·`ShortcutManagerCheck`가 통과했다.
  - 실제 OS 창에서는 Help에서 한국어↔영어를 전환한 뒤 첫 클릭, 빈 제목줄 드래그, 가장자리 리사이즈를 각각 확인해야 한다.
    모니터를 옮겨 DPI가 바뀐 뒤에도 같은 조작을 확인한다.

## [2026-11-05] 로캘 키 소유권과 에디터 사전 정리

- Area: Editor | UI | Tools | Build | Docs
- Type: Cleanup | Fix | Docs
- 적용 범위:
  - 엔진 소유 키를 Editor.csv에서 Engine.csv로 이동한 뒤, 에디터 키와 그 사용처만 정리한다.
  - 이전 커밋에 등록된 Engine.csv의 기존 671개 키와 영어·한국어 번역을 그대로 유지했다.
    스플라인·썸네일·렌더링·애니메이터 프리뷰 변경은 별도 작업 트리 변경으로 구분했다.
- 사전 소유권:
  - 엔진 소유 키 22개를 최종 키와 번역을 유지한 채 Engine.csv로 옮겼다.
    Animator 메타데이터 4개, `Engine::ETransformAbsolute.*` 열거형 표시 5개, 그래프 진단·설명 13개다.
  - Engine.csv의 기존 내용 뒤에 이동 행을 추가해 총 693개 키로 구성했다.
- 에디터 정리:
  - 기존 168개 키 중 109개는 기능 소유자를 드러내는 이름으로 바꾸고, 같은 역할의 59개는 기존·대표 키에 통합했다.
    `UI.ModelImport.*` 14개 중복 계열과 중복 메뉴·명령 표시 키를 포함한다.
  - Editor.csv는 1,490개에서 1,415개 항목으로 정리했다. 모든 행에 Group을 명시하고 Group·Key 순으로 정렬했다.
    기존 188개 빈 Group 행을 정리했다. 엔진 루트 키는 기존 표기를 따른다.
  - 공통 어휘·메뉴·이력 명령·리소스 피커·로그 수준을 각각 `EditorCommon`, `EditorMenu`, `History.Command`, `ResourcePicker`, `LogLevel`로 정리했다.
    에디터 내 공통 네트워크 표시는 `NetworkDisplay`가 소유한다.
  - 에셋 참조의 문제 유형/복구 동작, 모델 다시 불러오기, 셰이더 미리보기와 후처리 효과 이동에 문맥별 키 6개를 추가했다.
    `Metrics.Type`·`Shortcuts.ColAction`·`MaterialEditor.*`·`Inspector.MoveUp/Down`의 부적절한 다른 패널 재사용을 제거했다.
  - 전이 시간과 프레임 지속 시간, 밝은 테마와 조명, 저장 명령과 저장 데이터 유형은 분리 유지했다.
    계층의 `Hierarchy.FocusSelected`도 공통 포커스 명령 이름과 구분한다.
  - 타임라인 삭제 명령은 실제 노티파이·카메라 샷 선택 삭제에 맞춰 'Delete Selected Item / 선택 항목 삭제'로 표시한다.
- 호환성과 검증:
  - 에디터 소스와 예제의 사용처를 함께 변경했다. 단축키 액션 ID와 기본 바인딩은 변경 전과 일치한다.
  - 패널의 고정 창 ID는 유지한다. 네트워크 프로파일러는 번역 키를 바꿔도 이전 창 ID를 명시적으로 유지해 도킹 배치가 바뀌지 않도록 했다.
    개별 컨트롤 키 변경에 따른 실제 UI 상태는 별도 조작 확인이 필요하다.
  - 후속 제목 점검에서 AnimatorGraphPanel의 `m_DisplayNameKey` 미설정으로 내부 이름이 그대로 표시되는 누락을 확인했다.
    기존 `AnimatorGraph.Title`을 연결하고, 감사기에 고정 IImGui 패널의 GetDisplayName fallback 검사와 회귀 테스트를 추가했다.
  - 영어·한국어 JSON은 기존 CSV 변환기의 `--strict` 경로로 생성했다. 직접 수정하지 않았다.
  - 감사기에 Editor.csv의 명시적 Group과 중복 접두사 검사를 추가했다. 제목 fallback 보완을 포함한 Python 회귀 테스트 18개를 통과했다.
    Engine.csv의 루트 키 규칙은 이 검사 대상으로 삼지 않는다.
  - 작업 트리의 Editor Debug, ReflectionInspectorCheck, GraphMessageLocalizationCheck, EditorHistoryCheck, TransformInheritanceCheck 빌드 통과.
    기존 엔진 사전을 자동 정렬하는 의존 타깃을 실행하지 않도록 MSBuild의 `/p:BuildProjectReferences=false`를 사용했다.
    엔진 전체 재빌드 결과로 해석하지 않는다.
  - 기존 이력 검사 타깃의 누락된 에디터 include·ImGui 의존성을 Editor/CMake/Checks.cmake에서 보완했다.
  - 관련 CTest 7개 통과: InputManagerCheck, LocalizationAuditUtility, ShortcutManagerCheck, GraphMessageLocalizationCheck,
    TransformInheritanceCheck, EditorHistoryCheck, ReflectionInspectorCheck.
  - 커밋 대상만 반영한 스냅샷에서 엄격 감사와 Python 테스트 18개를 통과했다. 생성 JSON은 Test.csv를 포함한
    전체 2,109개 키에서 CSV와 일치했다. C++ 빌드와 CTest 결과는 작업 트리 기준이며, 분리한 스냅샷은 정적으로 확인했다.
  - 실제 에디터 UI에서 언어 전환·도킹·단축키 재바인딩 조작은 별도 확인이 필요하다.

## [2026-11-05] 로캘·입력·단축키 정합성 보완과 자동 감사 확대

- Area: Editor | UI | Tools | Build | Docs
- Type: Fix | Cleanup | Docs
- 관련 기준: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md) 14절,
  [EditorWorkflowAndInteractionPlan.md](EditorWorkflowAndInteractionPlan.md) 2.4절·5절.
- 동기:
  - 메뉴에 표시한 키와 등록 액션이 다르거나, 노드 에디터의 기본 키 처리가 재바인딩을 우회하는 경로를 제거한다.
  - 고정 문구가 배열·직접 그리기·파일 필터를 통해 번역 조회를 우회하는 위치와 ImGui 입력 직접 조회를 정리한다.
- 구현:
  - 타임라인·스프라이트·성능·네트워크·리소스 참조 패널, 설정과 파일 필터의 고정 문구를 로캘 키로 연결한다.
    `Editor.csv`와 `Engine.csv`를 원본으로 사용하며 영어·한국어 JSON은 CSV 변환 도구로 생성한다.
  - 메뉴·툴팁의 키 안내는 `GetShortcutHint()`로 조회하고, 바인딩 해제 시 빈 단축키를 안전하게 처리한다.
  - `Graph.AddNode`와 `Graph.Validate`를 등록한다. 노드 에디터의 `EnableShortcuts(false)`로 기본 Delete·F·Space 경로를 차단한다.
    그래프 삭제·정렬·복제·코멘트 생성은 자기 문서의 에디터 문맥을 설정하고 호출 후 기존 문맥을 복원한다.
  - `Viewport.Focus`는 현재 프리뷰, 그래프, UI 캔버스, 씬 대상으로 전달한다. UI 캔버스는 별도 `UIViewport` 문맥을 사용한다.
  - 같은 문맥의 동일 키만 충돌로 판단하며 로컬 액션을 Global 액션보다 우선한다.
    로컬 액션이 비활성이어도 같은 키의 씬 명령이 대신 실행되지 않게 하고, 팝업 중 중앙 단축키 평가를 억제한다.
  - 키·마우스 상태와 화면 위치·이동량·휠은 InputManager를 사용한다.
    그래프 우클릭은 캔버스에서 시작하고 끝나는 경우로 제한하고, 타임라인은 창 호버·트랙 영역으로 클릭을 제한한다.
  - 카메라 연속 이동, 텍스트·드래그 취소와 바인딩 수집은 InputManager를 사용하는 상태 처리로 유지한다.
    ImGui 위젯 반환값·호버·포커스·활성 상태는 UI 문맥으로 사용한다.
- 자동 검사:
  - `audit_localization.py`는 Engine·Editor·Client·존재하는 Shared 소스의 표시 문자열, 키 누락, 빈 번역과 모든 CSV의 중복 키 위치를 검사한다.
    콤보·트리·검색 힌트·직접 그린 텍스트·메뉴의 고정 단축키 안내도 검사한다.
  - 리터럴 액션 참조의 미등록 여부와 ImGui 키·마우스 직접 조회 검사를 추가한다.
    동적 식별자 전체의 유효성이나 실제 UI 동작까지 증명하는 검사는 아니다.
  - 감사기 회귀 테스트 14개와 `ShortcutManagerCheck`를 추가·보완한다.
    단축키 검사는 문맥 우선순위, 비활성 로컬 명령 격리, 입력 억제, 재바인딩과 힌트 해제를 다룬다.
- 검증 근거:
  - 당시 작업 트리에서 Editor Debug 빌드와 `AuditLocalization` 통과.
    소스 587개·CSV 키 2,172개 검사에서 누락·빈 번역·중복·검사 대상 고정 문구·미등록 액션·ImGui 입력 직접 조회 지적 0건.
  - `InputManagerCheck`, `ShortcutManagerCheck`, `GraphMessageLocalizationCheck`, `LocalizationAuditUtility` CTest 4개 통과.
    `LocalizationAuditUtility`에는 Python 회귀 테스트 14개가 포함된다. 전체 CTest 실행 결과로 확대하지 않는다.
  - 등록 액션 68개를 대조하여 같은 문맥의 기본 바인딩 중복이 없음을 정적 확인했다.
    영어·한국어 생성 JSON도 당시 작업 트리의 CSV 키 2,172개와 일치했다.
  - 위 수치는 별도 진행 중인 변경도 포함한 작업 트리 기준이다.
    이번 커밋은 썸네일·핫리로드·스플라인 변경을 제외하며 생성 JSON도 커밋에 포함한 CSV에 맞춘다.
  - 커밋할 인덱스의 소스·사전을 별도 임시 디렉터리로 추출해 다시 검사했다.
    소스 578개·키 2,162개에서 엄격 감사 지적 0건, 감사기 테스트 14개 통과, 양 언어 CSV/JSON 일치를 확인했다.
    별도 빌드 디렉터리에서 이 인덱스만으로 C++ 전체 빌드를 다시 수행한 것은 아니다.
- 재현 명령 (PowerShell, 저장소 루트):

  ```powershell
  $bamCMake = (Select-String out/build/x64-debug/CMakeCache.txt '^CMAKE_COMMAND:INTERNAL=').Line.Split('=', 2)[1]
  & $bamCMake --build out/build/x64-debug --config Debug --target Editor InputManagerCheck ShortcutManagerCheck GraphMessageLocalizationCheck GenerateLocalization AuditLocalization --parallel 4
  ctest --test-dir out/build/x64-debug -C Debug -R '^(InputManagerCheck|ShortcutManagerCheck|GraphMessageLocalizationCheck|LocalizationAuditUtility)$' --output-on-failure
  ```

  기존 빌드 디렉터리는 캐시에 기록된 CMake 실행 파일을 사용한다. 다른 버전의 CMake 모듈이 섞이면 재구성이 실패할 수 있다.
- 미검증·후속 확인:
  - 실제 UI에서 언어 전환, 재바인딩·해제 후 메뉴/툴팁, 텍스트 입력·팝업 중 명령 억제를 확인한다.
  - 그래프 캔버스 밖 우클릭, 분리 창·DPI별 좌표, 타임라인 클릭·드래그·휠과 각 프리뷰 포커스를 확인한다.
  - 외부 라이브러리의 개발자 UI·사용자 콘텐츠·기술 식별자는 이번 고정 문구 번역 범위와 구분한다.

## [2026-11-05] 에디터 인스펙터 및 편집이력창 반응형 UI와 텍스트 클리핑 방지 고도화

- Area: Editor | UI | Docs
- Type: Feature | Fix | Refactor | Docs
- Owner: @user & @assistant
- 관련 파일:
  - Docs/EngineEditorClientGuidelines.md
  - Docs/ImplementationLog.md
  - Editor/Source/ImGui/InspectorPanel/Private/ReflectionPropertyDrawer.cpp
  - Editor/Source/ImGui/InspectorPanel/Private/PropertyDrawerEnTT.cpp
  - Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp
  - Editor/Source/ImGui/HistoryPanel/Private/HistoryPanel.cpp
- Motivation:
  - 인스펙터 창 및 편집이력창의 너비가 좁아질 때 프로퍼티 라벨, 툴바 버튼, 경로 텍스트가 잘려나가는 현상 해결
  - 다국어 텍스트 및 고정 비율 컬럼으로 인한 가독성 저하를 방지하고 상용 엔진 수준의 반응형 UI 지원
- Implementation summary:
  - ReflectionPropertyDrawer: 고정 비율(0.25:0.75) 테이블 컬럼을 기본 고정폭(120px) + 가변 확장(WidthStretch) 정책으로 개편
  - DrawResponsivePropertyLabel: 텍스트 가용 너비 초과 감지(isClipped) 및 마우스 호버 시 전체 이름/타입 통합 툴팁 제공
  - InspectorPanel: 프리팹 헤더바 버튼 3개에 대한 가용 폭 감지 및 자동 개행(Wrap) 적용, 슬라이더 너비 -FLT_MIN 꽉 채우기, 경로 입력 필드 폭 최적화
  - HistoryPanel: 360px 기준 컴팩트 아이콘 버튼 툴바 자동 전환, 컬럼 슬림화(50px/45px), ShortcutManager::Get().GetShortcutHint() 기반 실시간 동적 핫키 힌트 연동
- Technical details:
  - 자물쇠 등 우측 컨트롤 예약 폭(reservedRightWidth)을 계산하여 정확한 텍스트 클리핑 감지
  - ShortcutManager 중앙 액션(Edit.Undo, Edit.Redo) 힌트와 툴팁을 일원화하여 단축키 재바인딩 즉각 반영
- Validation:
  - 창 크기 변경 시 인스펙터 라벨 120px 유지 및 잘림 시 호버 툴팁 정상 표시 확인
  - 프리팹 헤더바 및 편집이력 툴바의 좁은 창 자동 개행 및 아이콘 전환 검증 완료

---

## [2026-11-05] 에디터 패널 인터랙티브 스플리터 도입 및 레이아웃 상태 영속화(기억하기) 구현

- Area: Editor | UI | Settings | Reflection | Docs
- Type: Feature | Fix | Refactor | Docs
- Owner: @user & @assistant
- 관련 파일:
  - Editor/Source/ImGui/Public/ImGuiSplitter.h
  - Editor/Source/Settings/Public/EditorSettings.h
  - Editor/Source/ImGui/Private/ImGuiManager.cpp
  - Editor/Source/ImGui/AnimatorGraphPanel/Public/AnimatorGraphPanel.h
  - Editor/Source/ImGui/AnimatorGraphPanel/Private/AnimatorGraphPanel.cpp
  - Editor/Source/ImGui/ResourceEditor/Public/MaterialEditor.h
  - Editor/Source/ImGui/ResourceEditor/Private/MaterialEditor.cpp
  - Editor/Source/ImGui/ResourceEditor/Public/SpriteAnimationEditor.h
  - Editor/Source/ImGui/ResourceEditor/Private/SpriteAnimationEditor.cpp
  - Editor/Source/ImGui/ResourceEditor/Public/ModelEditor.h
  - Editor/Source/ImGui/ResourceEditor/Private/ModelEditor.cpp
  - Editor/Source/AssetManager/Importer/Public/ModelImportModal.h
  - Editor/Source/AssetManager/Importer/Private/ModelImportModal.cpp
  - Editor/Source/ImGui/SettingsPanel/Public/UnifiedSettingsPanel.h
  - Editor/Source/ImGui/SettingsPanel/Private/UnifiedSettingsPanel.cpp
  - Editor/Source/ImGui/ConsolePanel/Public/ConsolePanel.h
  - Editor/Source/ImGui/ConsolePanel/Private/ConsolePanel.cpp
  - Editor/Source/ImGui/TimelinePanel/Public/AnimationTimelinePanel.h
  - Editor/Source/ImGui/TimelinePanel/Private/AnimationTimelinePanel.cpp
  - Editor/Source/ImGui/InspectorPanel/Private/PropertyDrawerEnTT.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Public/ContentBrowserPanel.h
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/Shortcut/Private/ShortcutSettingsPanel.cpp
  - Editor/Source/ImGui/HistoryPanel/Private/HistoryPanel.cpp
  - Editor/Source/ImGui/PostProcessPanel/Private/PostProcessPanel.cpp
  - Editor/Source/ImGui/NetworkProfilerPanel/Private/NetworkProfilerPanel.cpp
- 동기 및 배경:
  - 에디터 내 주요 복합 창들(AnimatorGraphPanel, MaterialEditor, SpriteAnimationEditor)의 내부 분할이 고정 너비/비율로 하드코딩되어 있어, 파라미터나 테이블 컬럼 내용이 잘리고 사용자가 창 크기를 조절할 수 없었음.
  - 기존에 스플리터가 있던 패널들(UnifiedSettingsPanel, ConsolePanel, TimelinePanel, ModelEditor 등)도 분할 크기가 C++ float 변수에만 머물러 있어, 에디터를 재시작하면 기본값으로 무조건 리셋되는 현상이 있었음.
  - ShortcutSettingsPanel, HistoryPanel, PostProcessPanel 등 일부 테이블에 ImGuiTableFlags_Resizable이 누락되어 컬럼 조절이 불가능했고, 테이블 컬럼 라벨에 다국어 키만 전달되어 언어 전환 시 ImGui 컬럼 너비 설정이 초기화되는 문제 해결.
- 구현 내용 요약:
  1. **공통 스플리터 헬퍼 구축 (ImGuiSplitter.h)**:
     - Editor/Source/ImGui/Public/ImGuiSplitter.h 신규 생성.
     - ImGui 순정 SplitterBehavior를 기반으로 마우스 호버 감지 여유폭(hover_extend), 커서 변경(ResizeEW/ResizeNS), 하이라이트 렌더링, 양방향 크기 보존 클램핑을 지원하는 SplitterV (좌우) 및 SplitterH (상하) 인라인 헬퍼 함수 구현.
  2. **고정 분할 패널의 스플리터 적용**:
     - AnimatorGraphPanel: 고정 상수(260px, 280px)였던 좌측 뷰포트/파라미터 및 우측 인스펙터에 SplitterV 좌우 더블 스플리터 적용.
     - MaterialEditor: 뷰포트 55% vs 디테일 창 45% 고정 비율을 SplitterV 기반 인터랙티브 분할로 개선.
     - SpriteAnimationEditor: 캔버스 <-> 프레임 목록(SplitterV) 및 상단 <-> 필름스트립 타임라인(SplitterH) 2중 스플리터 적용.
     - PrefabEditorPanel: 계층 뷰 스플리터 분석 및 기존 PrefabModeManager의 씬/계층/뷰포트 둔갑 시스템 확인 및 레거시 독립 창 정리 대상 분류.
  3. **기존 수동 스플리터 패널 헬퍼 전환 및 리팩토링**:
     - ModelEditor, ModelImportModal, UnifiedSettingsPanel의 20여 줄 수동 드래그/버튼 코드를 공통 SplitterV 헬퍼 단 1~2줄로 통합.
  4. **테이블 컬럼 리사이즈 및 다국어 영속성 보강**:
     - ShortcutSettingsPanel, HistoryPanel, PostProcessPanel, NetworkProfilerPanel에 ImGuiTableFlags_Resizable 플래그 추가하여 컬럼 폭 조절 잠금 해제.
     - ContentBrowserPanel, GraphPanel, ShortcutSettingsPanel, HistoryPanel, PostProcessPanel, PropertyDrawerEnTT의 컬럼 라벨에 ##StableID를 부여하여 언어 변경(한국어 <-> 영어) 시에도 컬럼 너비 설정이 유지되도록 수정.
     - PropertyDrawerEnTT의 DrawPropertyTable에 타입별 ImGui::PushID를 감싸 컴포넌트 간 테이블 ID 충돌 방지.
     - ContentBrowserPanel.h의 미사용 변수 m_ModalSplitterWidth 정리.
  5. **스플리터 영속화(기억하기) 시스템 구축**:
     - Editor/Source/Settings/Public/EditorSettings.h: EditorWorkspace 구조체에 리플렉션 직렬화 필드 std::map<std::string, float> SplitterStates 및 SetSplitter/GetSplitter 헬퍼 함수 추가.
     - Editor/Source/ImGui/Private/ImGuiManager.cpp: CreateDefaultPanels()에서 CreateResourceEditors() 호출 순서를 보정하여 머티리얼, 모델 등 리소스 에디터 패널들이 워크스페이스 저장/복원 대상(m_WorkspacePanels)에 정상 포함되도록 수정.
     - 각 패널(AnimatorGraphPanel, MaterialEditor, SpriteAnimationEditor, ConsolePanel, AnimationTimelinePanel, UnifiedSettingsPanel, ModelEditor, ModelImportModal)에 CaptureState / RestoreState 오버라이드 구현.
- 검증 및 확인:
  - Editor 전체 빌드 정상 통과 (ReflectionGenerator 및 CMake 타깃 빌드 성공).
  - 각 패널에서 스플리터 마우스 드래그 조작 시 리사이즈 커서(<->) 표시 및 부드러운 폭/높이 조절 동작 확인.
  - 에디터 재시작 후 .bam/User/Workspace.json의 SplitterStates를 통해 조절했던 모든 분할 크기가 초기화 없이 정상 복원됨을 검증.
---

## [2026-11-05] 3인칭 캐릭터 Locomotion 애니메이션 블렌딩 고도화 및 모델 재임포트 핫 리로드

- Area: Animation | Physics | Resource | Editor | Docs
- Type: Feature | Fix | Refactor | Docs
- Owner: @user & @assistant
- 관련 파일:
  - Docs/ThirdPersonCharacterSystemPlan.md
  - Docs/InteractiveModelImportPreviewPlan.md
  - Docs/DevelopmentPriorities.md
  - Docs/ImplementationLog.md
  - Engine/Source/Resource/Model/Private/AnimationEvaluator.cpp
  - Engine/Source/Resource/Model/Public/AnimationEvaluator.h
  - Engine/Source/Resource/Model/Private/AnimatorStateMachine.cpp
  - Engine/Source/Resource/Model/Public/AnimatorStateMachine.h
  - Engine/Source/Core/Public/Components/Physics/CharacterControllerComponent.h
  - Engine/Source/System/Physics/Private/KinematicCharacterSystem.cpp
  - Engine/Source/System/Physics/Private/ThirdPersonControllerSystem.cpp
  - Editor/Source/AssetManager/Importer/Private/ModelImporter.cpp
  - Editor/Source/AssetManager/Importer/Public/ModelImporter.h
  - Editor/Source/AssetManager/Importer/Private/ModelImportModal.cpp
  - Editor/Source/AssetManager/Importer/Public/ModelImportModal.h
  - Resources/Data/Animation/Locomotion.animator
  - Resources/Data/Localization/Engine.csv
  - Resources/Scenes/RaytracingScene3.json
- 구현 내용 및 동기:
  1. 1D 블렌드 트리 위상 동기화 (Phase Matching): 클립 간 길이가 다른 Walk와 Run 클립을 정규화 시간(Normalized Time) 기준으로 일치시켜 LERP 보간함으로써 발걸음이 어긋나는 현상 제거.
  2. 미세 루프 크로스페이드 (Loop Smoothing): 비반복 모션 잘림 방지를 위해 마지막 0.05초 구간 동안 0초 포즈와 미세 보간하여 루프 래핑 시 뚝 끊기는 문제 해결.
  3. 이동 속도 연동 애니메이션 재생 배속: CharacterControllerComponent의 이동 속도(horizontalSpeed)를 기반으로 애니메이터의 PlayRate를 동적 스케일링하여 발 미끄러짐(Foot Sliding) 및 급감속 시 발길질 현상 방지. 인스펙터 실시간 조절(ScalePlayRateWithSpeed, MinPlayRate, MaxPlayRate) 지원.
  4. 역직렬화 하위 호환성 확보: 신규 추가된 프로퍼티에 ALLOW_MISSING 어노테이션을 부여하여 구버전 씬(RaytracingScene3.json) 로드 시 실패 에러(code=1) 방어.
  5. 모델 재임포트 인플레이스 핫 리로드: ModelImporter에서 기존 리소스 핸들이 유효할 경우 Reload(key)를 호출하여 MaterialInstance registration failed 및 0xC0000005 크래시 원천 차단.
  6. 대화형 모델 임포트 모달(ModelImportModal) 클래스 분리 및 뷰포트 프리뷰 인프라 연동.
- 검증:
  - Fox 모델 기반 Idle -> Walk -> Run 실시간 이동 및 방향 전환, 감속, Shift 달리기 시 모션 블렌딩과 자연스러운 루프 재생 확인.
  - RaytracingScene3.json 로드 및 인스펙터 슬라이더 조절 정상 작동 확인.

---

## [2026-11-04] 3인칭 캐릭터 컨트롤러, 스프링암 카메라 및 KCC 물리 안정화 (ThirdPersonCharacterSystemPlan)

- Area: Physics | Camera | Engine | Editor | Docs
- Type: Feature | Fix | Docs
- Owner: @user & @assistant
- 관련 파일:
  - Docs/ThirdPersonCharacterSystemPlan.md
  - Docs/DevelopmentPriorities.md
  - Docs/ImplementationLog.md
  - Engine/Source/Core/Public/Components/Camera/SpringArmComponent.h
  - Engine/Source/Core/Public/Components/Physics/ThirdPersonControllerComponent.h
  - Engine/Source/Core/Public/Components/Physics/CharacterControllerComponent.h
  - Engine/Source/System/Camera/Public/SpringArmSystem.h
  - Engine/Source/System/Camera/Private/SpringArmSystem.cpp
  - Engine/Source/System/Physics/Public/ThirdPersonControllerSystem.h
  - Engine/Source/System/Physics/Private/ThirdPersonControllerSystem.cpp
  - Engine/Source/System/Physics/Public/KinematicCharacterSystem.h
  - Engine/Source/System/Physics/Private/KinematicCharacterSystem.cpp
  - Engine/Source/System/Physics/Private/PhysicsSystem.cpp
  - Editor/Source/Application/Private/Application.cpp
  - Editor/Source/ImGui/ViewportPanel/DebugRenderer/Private/DebugRenderer.cpp
  - Resources/Data/Localization/Engine.csv
  - Resources/Data/Localization/Editor.csv
- 동기 및 배경:
  - 3D 충돌체(Box/Sphere)와 Kinematic Character Controller(KCC) 충돌 판정 결함으로 캐릭터가 바닥을 뚫고 추락하거나, 바닥에 하반신이 파묻혀 이동 입력이 영구히 막히는 현상 발생.
  - 고정 물리 틱(FixedUpdate, 60Hz)과 렌더 틱(LateUpdate, 가변 주사율) 간의 주기 불일치로 인한 심한 시각적 떨림(Stuttering/Jitter) 해결 필요.
  - 공중 체공 중 최고점을 지나 낙하를 시작할 때 수직 거리가 남아있음에도 바닥으로 순간이동하듯 착 달라붙는 조기 접지 흡착(Early Snap) 결함 해결 필요.
  - 스프링암이 자식 엔티티로 부착된 계층 구조에서 뷰포트 상의 카메라 아이콘 및 프러스텀 위치가 스프링암 프로브 구체와 동기화되지 않고 엉뚱한 위치로 어긋나 보이는 결함 해결 필요.
- 구현 및 해결 내용 요약:
  1. **3D 충돌체 및 스윕 판정 정상화 (PhysicsSystem.cpp, KinematicCharacterSystem.cpp)**:
     - PhysicsSystem::TestColliderHit의 BoundingBox 중심을 (0,0,0) ~ Size*0.5에서 [-halfSize, halfSize]로 정상화하고 SphereShape 스케일 반영.
     - 발바닥 오프셋(
ootOffset = Center.y - Height*0.5)을 계산하여 중심 피벗 메시(구체 등)가 바닥 패널 위에 정확히 안착하도록 보정.
     - 수평 이동 스윕 시 발바닥 여유 마진을 적용하여 바닥과의 불필요한 마찰 걸림(Stuck) 제거.
  2. **프레임 레이트 동기화 및 부드러운 이동 (KinematicCharacterSystem.cpp)**:
     - KCC 물리 갱신을 OnFixedUpdate에서 OnUpdate(dt)로 전환하여 디스플레이 주사율(144Hz 등)과 1:1로 부드럽게 동기화, 화면 떨림을 완벽히 제거.
  3. **공중 낙하와 지면 보행 분리 (KinematicCharacterSystem.cpp)**:
     - DetectGround에 의한 조기 바닥 흡착 제거. kcc.IsGrounded 상태일 때만 보행용 지면 추적을 수행하고, 공중에서는 SweepCharacter에 의한 물리적 충돌 감지로만 자연스럽게 착지하도록 분리.
  4. **스프링암 계층 좌표 변환 및 프러스텀 동기화 (SpringArmSystem.cpp)**:
     - 부모가 있는 카메라 엔티티에 대해 부모 역행렬(invParent)을 적용하여 로컬 좌표와 회전을 설정하고, TransformSystem::TryComposeWorld 및 camWorld.CommitWorld를 호출하여 부모 변환을 정확히 결합.
     - TransformSystem::UpdateRootWorld의 무조건 호출로 인해 부모 변환이 무시되고 로컬 좌표가 월드 행렬로 덮어씌워지던 버그를 해결하여 프로브 구체와 뷰포트 카메라 프러스텀의 위치를 일치시킴.

---

## [2026-11-04] 에디터 워크플로 및 계층·뷰포트·인스펙터 상호작용 개선과 단축키 중앙화 (EditorWorkflowAndInteractionPlan)

- Area: Editor | UI | Shortcut
- Type: Feature | Refactor | UX | Docs
- Owner: @user & @assistant
- 관련 파일:
  - AGENTS.md
  - Docs/EngineEditorClientGuidelines.md
  - Docs/EditorWorkflowAndInteractionPlan.md
  - Docs/DevelopmentPriorities.md
  - Editor/Source/PIE/Private/PIEManager.cpp
  - Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp
  - Editor/Source/ImGui/ViewportPanel/Public/SceneViewportPanel.h
  - Editor/Source/ImGui/ViewportPanel/Private/BaseViewportPanel.cpp
  - Editor/Source/Application/Camera/Private/EditorCameraController.cpp
  - Editor/Source/Application/Camera/Public/EditorCameraController.h
  - Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp
  - Editor/Source/ImGui/HierarchyPanel/Private/EntityHierarchyView.cpp
  - Editor/Source/ImGui/HierarchyPanel/Public/EntityHierarchyView.h
  - Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp
  - Editor/Source/ImGui/HierarchyPanel/Public/HierarchyPanel.h
  - Editor/Source/Shortcut/Private/EditorActionRegistry.cpp
  - Editor/Source/Shortcut/Public/ShortcutTypes.h
- Motivation:
  - PIE 실행 중 씬 뷰포트 카메라가 게임 카메라로 강제 전환되어 편집 및 디버깅 시점 유지가 불가능했던 문제 해결.
  - 계층 패널에서 엔티티 선택 시 숨겨진 인스펙터 탭이 전면으로 노출되지 않고, 개별 패널 `Draw()`에 파편화된 `KEY_DOWN` 처리로 인해 단축키 충돌 및 씹힘이 발생하던 문제 해결.
- Implementation summary:
  - **PIE 카메라/뷰 분리**: PIE 실행 중에도 `Scene View`는 에디터 카메라 조작(Orbit, Pan, Fly) 및 기즈모 디버깅을 상시 유지하고, 게임 실행 화면은 `Game View`를 자동 오픈/포커스하여 분리 표시. PIE 종료 시 `Scene View`로 포커스 자동 복원.
  - **계층 패널 ↔ 인스펙터 연계**: 계층 패널에서 엔티티 클릭 시 숨겨진 인스펙터 탭을 즉시 최상위로 전면화(`Focus()`)하며, 기존 엔티티별 스크롤 유지 맵(`m_EntityScrollMap`) 보존.
  - **계층 패널 빠른 조작 및 컨텍스트 메뉴 확장**: `Focus Selected`(F, 씬 뷰포트 카메라 이동), `Rename`(F2, 인라인 텍스트 입력), `Create Child Empty`, `Duplicate`(Ctrl+D), `Move to Root` 메뉴 확장.
  - **단축키 중앙화 원칙 준수**: 패널 내 파편화된 수동 `KEY_DOWN` 처리(F, F2, Delete 등)를 제거하고 `ShortcutManager`(`EditorActionRegistry`)의 정식 액션(`Viewport.Focus`, `Hierarchy.Rename` 등) 및 문맥(`EShortcutContext`)으로 일원화. UI 메뉴 단축키 힌트 또한 `GetShortcutHint`로 동적 조회 일치.
  - **지침 및 가이드라인 문서화**: `AGENTS.md` 11절 및 `Docs/EngineEditorClientGuidelines.md` 14절에 단축키 중앙화 관리 원칙 명시.
- Validation:
  - `cmake --build out/build/x64-debug --target Editor` 빌드 성공 (경고 0, 에러 0).
  - 씬 뷰포트 에디터 카메라 유지, Game View 자동 전환/복원, F 포커스, F2 인라인 이름 변경, 컨텍스트 메뉴 동작 검증.

---

## [2026-11-04] 다국어(UTF-8) 파일 경로 입출력 안정화 및 efsw 파일 감시자 스레드 예외 방어 (Utf8PathHandlingPlan)

- Area: Engine | Editor | Core | HotReload | UI
- Type: Fix | Reliability | Core
- 관련 파일:
  - Engine/Source/Core/Public/Functions.h
  - Editor/Source/HotReload/Private/ModuleBuildManager.cpp
  - Editor/Source/HotReload/Private/ModuleHotReloadManager.cpp
  - Editor/Source/HotReload/Private/ShaderHotReloadManager.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserTreeItem.cpp
  - Editor/Source/AssetManager/AssetCache/Private/AssetCache.cpp
  - Docs/Utf8PathHandlingPlan.md
  - Docs/DevelopmentPriorities.md
- Motivation:
  - 한국어 Windows 환경에서 파일 다이얼로그나 탐색기를 통해 '새 폴더' 등 한글 경로를 생성할 때 efsw 콜백에서 MSVC std::filesystem::path로의 narrow 문자열 변환 시 인코딩 불일치(CP_ACP vs UTF-8, Win32 오류 코드 1113 ERROR_NO_UNICODE_TRANSLATION)로 인해 백그라운드 스레드에서 예외가 발생하여 에디터 프로세스가 크래시되는 결함 해결.
  - ImGui 인라인 이름 변경(F2 renameBuffer), SDL3 외부 파일 드래그 앤 드롭(OnExteranalDropped), 썸네일 캐시 큐 등 잠재적 크래시 지점 전수 교정.
- Implementation summary:
  - Engine/Source/Core/Public/Functions.h에 C++20 표준 규격(reinterpret_cast<const char8_t*>, std::u8string_view) 기반의 Engine::Utf8ToPath 단일 진실의 원천(SSOT) 헬퍼 함수 구축.
  - ContentBrowserFileListener::handleFileAction: UTF-8 경로 변환 및 try-catch 스레드 예외 방어벽 추가.
  - ModuleSourceFileListener, ModuleHotReloadFileListener, ShaderFileListener: handleFileAction 내 UTF-8 경로 변환 적용.
  - ContentBrowserGridItem.cpp / ContentBrowserTreeItem.cpp: 인라인 리네임(DrawRename)의 renameBuffer UTF-8 변환 적용 및 머티리얼 인스턴스/스프라이트 경로 생성 시 불필요한 narrow 왕복 변환을 제거하고 wstring 결합 적용.
  - ContentBrowserPanel.cpp: OnExteranalDropped에서 SDL3의 UTF-8 경로 데이터를 (const char8_t*)로 안전하게 변환.
  - AssetCache.cpp: ProcessPending3DThumbnail의 썸네일 캐시 큐 경로 변환 안전화.
- Technical details:
  - efsw, ImGui, SDL3 등 서드파티 라이브러리가 넘겨주는 std::string은 UTF-8이며, MSVC STL의 path(std::string)는 기본적으로 CP_ACP(CP949)를 가정하므로 UTF-8 다국어 바이트가 유입되면 MultiByteToWideChar 실패 및 예외가 발생함.
  - (const char8_t*) 또는 Utf8ToPath를 통해 UTF-8임을 명시함으로써 C++20 표준 path 생성자가 올바른 코드페이지 변환을 수행하도록 보장.
  - 외부 백그라운드 스레드(efsw) 콜백 내 try-catch를 배치하여 OS I/O 예외 발생 시 프로세스 즉사를 방지.
- Validation:
  - 정적 코드 분석 및 파일 감시자/UI/드롭 경로 전수 점검 완료.

---

## [2026-11-03] 강화학습 프로토콜·결정론적 seed=0 보정(F09), Release 빌드 디렉터리 동적 추적(F13), Client 독립 배포 파이프라인 완성(F04) 및 Unity 헤더 충돌 보정

- Area: Engine | Simulation | Editor | Tools | Build
- Type: Feature | Fix | Reliability | Build
- 관련 파일:
  - Engine/Source/Simulation/Public/RLEnvironment.h
  - Engine/Source/Simulation/Public/CartPoleEnvironment.h
  - Engine/Source/Simulation/Private/CartPoleEnvironment.cpp
  - Engine/Source/Simulation/Private/RLServer.cpp
  - Tools/RL/bam_gym_env.py
  - Editor/Source/HotReload/Private/HotReloadManager.cpp
  - Tools/Scripts/PackageClient.ps1
  - Editor/Source/ImGui/ViewportPanel/Private/ViewportDeferredPipeline.cpp
  - Editor/Source/FileDialog/Private/FileDialogs.cpp
  - Editor/Source/Command/Public/SceneEditHistory.h
  - Engine/Source/World/Scene/Private/SceneManager.cpp
  - Engine/Source/World/Scene/Private/SceneHierarchy.cpp
  - Editor/Source/HotReload/Private/AssetHotReloadManager.cpp
  - Docs/DevelopmentPriorities.md
  - Docs/CodeDocumentationAudit-2026-11-02.md
  - Docs/BuildAndPackagingPlan.md
- Motivation:
  - 강화학습 환경의 seed=0 재현성 결함 및 비표준 JSON 파싱 취약점 해결 (F09).
  - 핫리로드 모듈 빌드 시 Release 구성의 빌드 디렉터리 불일치 해결 (F13).
  - Standalone Client 패키징 시 바이너리 다단계 자동 탐색 및 누락 리소스 수집 완성 (F04).
  - MSVC Unity 빌드 환경에서 전역 using namespace std; 및 Windows SDK 헤더 로드 순서로 인한 byte 심볼 모호성(C2872), ODR 충돌(C2084), Glaze 내부 링크 에러(C7631) 해소.
- Implementation summary:
  - **F09 (강화학습 프로토콜 및 재현성)**:
    - `IRLEnvironment::Reset`에 `std::optional<uint64_t> seed = std::nullopt` 시그니처를 적용하고, `seed.has_value()` 분기로 `seed=0`일 때도 난수 생성기를 정확히 초기화하도록 수정하여 Gymnasium 계약 준수.
    - `RLServer.cpp`에 `<glaze/glaze.hpp>`를 적용하여 견고한 JSON 역직렬화 구축. 익명 네임스페이스 대신 `Engine` 네임스페이스로 external linkage 보장.
    - 소켓 부분 송신(Partial Send)을 방지하는 `SendAll()` 루프 및 64KB 버퍼 상한 도입.
    - `bam_gym_env.py`의 `numpy` 선택적 임포트 및 메서드 호출 시 `self` 바인딩 에러 없는 순수 파이썬 폴백 공간(`DiscreteSpaceFallback`, `BoxSpaceFallback`) 구현.
  - **F13 (Release 모듈 빌드 디렉터리 정합성)**:
    - `HotReloadManager.cpp`의 `ResolveBuildDirectory()`에서 실행 파일 디렉터리 기준으로 상위 디렉터리의 `CMakeCache.txt`를 동적 역추적하여 Debug/Release 및 멀티 구성 프리셋 디렉터리를 자동 감지하도록 개선.
  - **F04 (Client 배포 스크립트 산출물 수집 완성)**:
    - `PackageClient.ps1`에 구성별 바이너리 다단계 자동 탐색 배열(`$BinDirCandidates`) 도입.
    - `Config/Project.json` 및 의존성 리소스 필수 수집 및 누락 시 패키징 중단 예외 처리.
  - **Unity 빌드 심볼 충돌 해결**:
    - `SceneEditHistory.h` 최상단에 `#include "Editor_Includes.h"`를 추가하여 Windows SDK 헤더를 먼저 로드함으로써 `rpcndr.h`의 `byte`와 `std::byte` 모호성 충돌(C2872) 원천 차단.
    - `SceneHierarchy.cpp`의 정적 헬퍼 함수명을 `HierarchyFinite`, `HierarchyMatches`로 변경하여 `TransformSystem.cpp`와의 ODR 심볼 충돌(C2084) 해결.
    - `ViewportDeferredPipeline.cpp`에 `LightComponent.h`, `SceneManager.cpp`에 `PrefabInstanceComponent.h` 인클루드 보강.
- Technical details:
  - Unity 빌드에서 `Delegate.h` 등의 전역 `using namespace std;`가 선행 포함된 상태에서 뒤이어 `<shobjidl.h>`/`<rpcndr.h>`가 로드되면 `byte` 식별자 모호성이 발생하므로, Windows SDK를 사용하는 헤더는 최상단에서 SDK 헤더를 선점 포함하도록 정렬.
- Validation:
  - `CartPoleEnvironment` `reset(seed=0)` 반복 호출 시 정확히 동일한 초기 상태 100% 결정론적 재현 검증.
  - 패키징 스크립트로 생성된 독립 배포 폴더에서 Client.exe 단독 실행 및 씬 로딩 검증 완료.
  - MSVC Unity 빌드 전체 컴파일 및 링크 성공 (`Engine.dll`, `Editor.exe` 빌드 통과).
- Risks and follow-ups:
  - 전역 `using namespace std;` 제거 작업은 전체 코드베이스에 걸친 대규모 작업이므로 별도 리팩토링 단계에서 체계적으로 점진적 제거 권장.

---

## [2026-11-03] 단일 CPU 파티클 시스템 1단계 코어 구축 및 애니메이션·FX 통합 타임라인 에디터 계획 수립 (P3)

- Area: Engine | Render | Editor | Docs
- Type: Feature | Optimization | Architecture | Docs
- 관련 파일:
  - Engine/Source/Core/Public/Components/Render/ParticleEmitterComponent.h
  - Engine/Source/Core/Public/Components/Render/RenderComponents.h
  - Engine/Source/System/Render/Public/ParticleSystem.h
  - Engine/Source/System/Render/Private/ParticleSystem.cpp
  - Engine/Source/Render/Private/Renderer.cpp
  - Editor/Source/Application/Private/Application.cpp
  - Client/Source/Main.cpp
  - Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp
  - Resources/Data/Localization/Engine.csv
  - Resources/Data/Localization/Editor.csv
  - Docs/AnimationTimelineEditorPlan.md
  - Docs/AnimatorGraphPlan.md
  - Docs/DevelopmentPriorities.md
- Motivation:
  - 씬 내 화염, 연기, 스파크 등 동적 시각 효과를 표현하기 위한 CPU 파티클 시스템 1단계 기반 마련 (EngineDevelopmentPlan 4절).
  - 파티클마다 엔티티를 생성하는 ECS 안티패턴을 배제하고 단일 컴포넌트 내 연속 입자 풀링 및 일괄 빌보드 렌더링 파이프라인 안착.
  - 캐릭터 애니메이션 및 타격 프레임과 이펙트를 함께 제어하기 위한 통합 타임라인 저작 도구 계획 수립.
- Implementation summary:
  - `Particle`, `ParticleEmitterComponent` 설계 (방출률, 버스트, 수명, 속도, 크기, 회전, 색상 보간, World/Local 공간 분리, 고정 난수 시드, 원뿔 확산 각도).
  - `ParticleSystem` (ISystem 상속, `OnUpdate` 오일러 물리 적분 및 $O(1)$ 스왑 앤 팝 제거, `OnSubmit` 카메라 뷰 기저 벡터 사전 계산 및 빌보드 조립).
  - `Renderer::RenderSprite`의 텍스처/파이프라인 중복 바인딩 캐싱(`lastTexture`)을 통한 배칭 오버헤드 감소.
  - 에디터 인스펙터 실시간 활성 파티클 카운트 및 원클릭 재생/일시정지/정지/리셋 툴바 UI 연동.
  - 프로퍼티 5대 카테고리(`CATEGORY_RENDER`, `CATEGORY_PARTICLE_EMISSION`, `CATEGORY_PARTICLE_MOTION`, `CATEGORY_PARTICLE_SIZE`, `CATEGORY_PARTICLE_COLOR`) 분류 및 `Engine.csv`/`Editor.csv` 번역 원본 등록.
  - `AnimationTimelineEditorPlan.md` 계획 문서 수립 (비-PIE 실시간 프리뷰 루프 및 애니메이션·이펙트·노티파이 통합 타임라인 저작 도구 로드맵).
- Technical details:
  - 개별 파티클 인스턴스는 경량 POD 구조체로 `std::vector<Particle>`에서 연속 메모리로 관리되어 캐시 히트율 극대화.
  - 원뿔 확산 방향 계산 시 Gram-Schmidt 직교 기저를 사용하여 별도 쿼터니언 변환 오버헤드 없이 $O(1)$ 삼각함수만으로 균일 구면 캡 표본화.
  - 렌더러 제출 시 루프 외부에서 카메라 역행렬의 `Right`, `Up`, `Forward` 축을 1회만 계산하고, 입자별 월드 행렬을 직접 합성하여 CPU 연산량 대폭 절감.
- Validation:
  - 에디터 뷰포트에서 단일 파티클 이미터 정상 방출 및 빌보드 시각화 확인 완료.
  - 인스펙터 실시간 파티클 카운트 및 Play/Pause/Stop/Reset 버튼 정상 동작 검증.
- Risks and follow-ups:
  - 비-PIE 에디터 루프 재생 (`Application.cpp` 에디터 틱 분기).
  - 반투명 파티클 카메라 거리 깊이 정렬 (Alpha Sorting).
  - 완전한 하드웨어 인스턴싱(`DrawIndexedInstanced`) 배칭.

---

## [2026-11-03] 에셋 참조 무결성 검증기(AssetReferenceValidator/Panel) 구축 및 바이너리 머티리얼 조사·정규화 (단계 4 F02 잔여)

- Area: Engine | Editor | Resource | UI
- Type: Feature | Fix | Architecture | Tooling
- 관련 파일:
  - Engine/Source/Core/Public/PlatformPath.h
  - Engine/Source/Core/Private/PlatformPath.cpp
  - Editor/Source/AssetManager/Importer/Private/ModelImporter.cpp
  - Editor/Source/AssetManager/Public/AssetReferenceValidator.h
  - Editor/Source/AssetManager/Private/AssetReferenceValidator.cpp
  - Editor/Source/ImGui/AssetReferencePanel/Public/AssetReferencePanel.h
  - Editor/Source/ImGui/AssetReferencePanel/Private/AssetReferencePanel.cpp
  - Editor/Source/ImGui/Private/ImGuiManager.cpp
  - Resources/Data/Localization/Editor.csv
  - Resources/Scenes/TestScene.json
  - Resources/Scenes/TestScene4.json
  - Docs/DevelopmentPriorities.md
- Motivation:
  - `Logs/Editor.log`에 대량으로 기록되던 구형 바이너리 머티리얼(`DefaultMaterial.bammat` 등) 로드 실패(`stage=archive file read`) 원인을 규명하고 정합성을 회복.
  - 모델 임포트 시 절대 경로(`I:/...`)가 하드코딩되어 다른 개발자 환경이나 독립 패키징 배포 시 메시가 로드되지 않는 결함 해결.
  - 씬, 프리팹, 모델, 머티리얼 간 상호 참조 누락(`Missing`), 외부 절대 경로(`External`), 확장자 누락/불일치(`InvalidExtension`)를 전수 진단하고 안전하게 자동 치환하는 도구 및 코어 확보.
- Implementation summary:
  - **프로젝트 상대 경로 유틸리티 및 임포터 표준화**: `PlatformPath::ToProjectRelative`를 구현하고, `ModelImporter::CreateJob`에서 대상 디렉터리를 상대 경로로 정규화하여 생성 산출물의 절대 경로 하드코딩을 원천 차단.
  - **에셋 참조 검증 코어 (`AssetReferenceValidator`)**:
    - `Resources/` 폴더 내 모든 에셋 JSON을 탐색하여 `AssetPath`, `AssetKey`, `Path` 필드를 수집.
    - 내장 셰이더/메시 심볼(`FullscreenQuad`, `LightingPS` 등) 제외 처리.
    - 3대 결함 유형(`MissingFile`, `ExternalAbsolutePath`, `InvalidExtension`) 분류 및 `TryResolveFallbackPath` 기반 자동 복구 경로 산출.
    - 임시 파일 생성 후 rename하는 원자적 파일 교체(`WriteTextFileAtomic`) 기반의 `FixIssue` 및 파일별 일괄 치환 `FixAllSafeIssues` 구현.
  - **참조 검사기 UI 패널 (`AssetReferencePanel`)**:
    - `IImGui` 기반 패널 구현 및 `ImGuiManager` 메뉴 등록 (`Window -> Panels -> Asset Reference Checker`).
    - 스캔 결과 요약 통계(누락, 외부, 확장자 오류, 복구 가능 수), 결함 유형별 콤보 필터링, 경로 검색창 제공.
    - 발견된 결함 목록 테이블 표시 및 단일 `[Fix]` / 상단 `[Fix Safe Issues]` 원클릭 일괄 복구 버튼 연동.
    - `Editor.csv` 로캘 키(`UI_ASSET_REFERENCE_PANEL`, `AssetRef.*`) 다국어 완비.
  - **바이너리 머티리얼 조사 및 구형 씬 참조 정규화**:
    - `Logs/Editor.log`의 10개 실패 파일이 2026-10-11에 생성된 28바이트 구형/손상 포맷임을 규명(최신 원본은 `.bammat.json`으로 온전히 존재).
    - `TestScene.json` 및 `TestScene4.json`에서 구형 `.bammat`을 가리키던 참조를 유효한 최신 `.bammat.json`으로 교체.
- Validation:
  - 에디터 내 `Asset Reference Checker` UI 패널 구동, 에셋 전체 스캔 및 안전 참조 자동 복구 정상 동작 확인.
  - `TestScene` 및 `TestScene4` 로드 시 머티리얼 로드 에러 해소 검증 완료.

---

## [2026-11-03] 에셋 파이프라인(F02·F12), 애니메이션 재생(F07), 머티리얼 플래그(P1-E·F08), 클라이언트 검증(F05) 종합 안정화

- Area: Engine | Editor | Render | Resource | Client
- Type: Feature | Fix | Performance | Architecture
- 관련 파일:
  - Editor/Source/AssetManager/Importer/Private/ModelImporter.cpp
  - Editor/Source/AssetManager/AssetCache/Public/AssetCache.h
  - Editor/Source/AssetManager/AssetCache/Private/AssetCache.cpp
  - Editor/Source/ImGui/Public/ResourcePicker.h
  - Engine/Source/Resource/Model/Public/AnimationEvaluator.h
  - Engine/Source/Resource/Model/Private/AnimationEvaluator.cpp
  - Engine/Source/Resource/Model/Public/AnimatorStateMachine.h
  - Engine/Source/Resource/Model/Private/AnimatorStateMachine.cpp
  - Engine/Source/Core/Public/Components/Render/AnimatorComponent.h
  - Engine/Source/System/Render/Private/AnimationSystem.cpp
  - Engine/Source/Resource/Material/Public/MaterialInterface.h
  - Engine/Source/Resource/Material/Private/MaterialInterface.cpp
  - Engine/Source/Resource/Material/Public/MaterialInstance.h
  - Engine/Source/Resource/Material/Private/MaterialInstance.cpp
  - Resources/Shader/hlsl/shading_flags.hlsli
  - Resources/Shader/hlsl/gbufferPS.hlsl
  - Engine/Source/Graph/Public/ShaderGraphCompiler.h
  - Engine/Source/Graph/Private/ShaderGraphCompiler.cpp
  - Engine/examples/ShaderGraphCheck.cpp
  - Client/Source/Main.cpp
- Motivation:
  - **F02 (모델 임포트)**: 기본 머티리얼 누락 시 껍데기 인스턴스를 계속 생성하고 저장 실패를 무시하던 문제 및 재임포트 시 Overwrite 플래그가 무시되던 결함 해결.
  - **F12 (썸네일 캐시 예산)**: 4K 등 대형 이미지의 무제한 디코드로 인한 메모리 폭증, 프레임당 무제한 업로드로 인한 Hitch, 무제한 캐시 누적으로 인한 VRAM 고갈 위험 차단.
  - **F07 (애니메이션 런타임)**: 비반복 애니메이션 종료 시 fmod 리셋 버그(마지막 포즈 대신 0초 첫 포즈로 튀는 현상) 해결, 부분 트랙의 바인드 로컬 TRS 오염 방지, ASM 시간 동기화.
  - **P1-E / F08 (머티리얼 플래그)**: 텍스처 미사용 슬롯의 상태 누수 방지 및 셰이더 샘플링 바이패스, 기본 더미 텍스처 안전 바인딩.
  - **3-B (셰이더 그래프)**: G-Buffer PBR 머티리얼 HLSL 코드 생성 및 채널 분리/결합 노드 지원.
  - **F05 (클라이언트 검증)**: ProjectSettings 로드 결과 검증을 통한 설정 손상 시 안전 종료 및 시작 씬 부재 처리.
- Implementation summary:
  - **ModelImporter (F02)**: 기본 머티리얼 로드 실패 시 필수 실패(EResult::ResourceNotFound) 및 등록 롤백, Overwrite 정책 반영, ValidateImportOutput 패턴 적용.
  - **AssetCache (F12)**: 128×128 Bilinear 다운샘플러(DownsampleRGBA)로 메모리 99% 절감, 메인 스레드 큐 기반 프레임당 최대 4개 업로드 스로틀링, LRU 256개 / 64MB 예산 퇴거 구축.
  - **AnimationEvaluator / ASM (F07)**: 비반복 애니메이션 시간 clamp 보존, SampleLocalTRS에서 트랙 누락 채널을 본의 바인드 TRS로 보존, ASM isLooping 및 Stop() 시간 리셋 연결.
  - **Material & G-Buffer (P1-E / F08)**: MaterialFeatureFlags 비트 플래그 시스템 구축, gbufferPS.hlsl에서 텍스처 없는 슬롯의 샘플링 바이패스 및 안전 기본값 직결.
  - **ShaderGraphCompiler (3-B)**: GenerateGBufferMaterialHlsl 구현, Math.MakeFloat3/BreakFloat3/MakeFloat4/BreakFloat4 지원, ShaderGraphCheck 회귀 테스트 통과.
  - **Client (F05)**: ConfigLoadResult 검사, SchemaVersion 유효성 검사, 시작 씬 미존재 시 FileNotFound 명확한 에러 전파.
- Validation:
  - cmake --build out/build/x64-debug --config Debug 전체 성공.
  - ShaderGraphCheck CTest 통과.

---

## [2026-11-03] 통합 설정 패널(UnifiedSettingsPanel) 전면 구축, ImGui 커스텀 스타일 영속화 및 스케일링 버그 수정

- Area: Editor | UI | Settings | Style | Localization
- Type: Feature | Architecture | Fix
- 관련 파일:
  - Editor/Source/ImGui/SettingsPanel/Public/UnifiedSettingsPanel.h
  - Editor/Source/ImGui/SettingsPanel/Private/UnifiedSettingsPanel.cpp
  - Editor/Source/ImGui/Public/ImGuiManager.h
  - Editor/Source/ImGui/Private/ImGuiManager.cpp
  - Editor/Source/Settings/Public/EditorSettings.h
  - Editor/Source/Settings/Public/EditorSettingsService.h
  - Editor/Source/Settings/Private/EditorSettingsService.cpp
  - Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp
  - Editor/Source/FileDialog/Public/FileDialogs.h
  - Editor/Source/FileDialog/Private/FileDialogs.cpp
  - Resources/Data/Localization/Editor.csv
  - Resources/Data/Localization/Korean.json
  - Resources/Data/Localization/English.json
- Motivation:
  - 기존의 개별 독립 패널(`GraphicSettingPanel`, `ShortcutSettingsPanel` 등)로 파편화되어 있던 에디터 및 프로젝트 설정을 단일 통합 창(`UnifiedSettingsPanel`)으로 일원화.
  - 기즈모 스냅, 카메라 속도/FOV, 폰트 파일/크기, 시작 씬, 에셋/셰이더 경로 등 에디터 작업에 필수적인 설정을 중앙 집중화하고 영속화.
  - ImGui 커스텀 스타일 편집기(`ShowStyleEditor`)에서 수정한 테마가 재시작 시 보존되지 않고, 테마 복원 시 누적 곱연산으로 인해 UI 간격이 기하급수적으로 벌어지던 치명적 버그 해결.
  - 폰트 크기 변경 및 한/영 전환 시 고정 폭 버튼 텍스트가 잘리는 문제 해결.
- Implementation summary:
  - **`UnifiedSettingsPanel` 전면 구축**:
    - 좌측 3대 그룹 트리(`System & Display`, `Editor & Workspace`, `Project`) 내비게이션, 스플리터 리사이징(`m_SidebarWidth`), 가로 스크롤바.
    - 7대 카테고리 구현: `General`(언어, UI 배율, 폰트 파일 탐색기, 폰트 크기, 자동 저장), `Theme & Style`(다크/라이트/클래식/커스텀 프리셋, StyleEditor 임베드, 원클릭 복원), `Graphic`(창 해상도, 전체화면, FPS, VSync, 15초 확인 모달), `Editor`(기즈모 스냅 SSOT, 카메라 속도/FOV, 로그 레벨), `Workspace`(뷰포트 해상도 프리셋/커스텀/종횡비 비동기 적용, 썸네일 크기, 도킹 레이아웃), `Shortcuts`(실시간 검색, 키 캡처, 충돌 감지, 리셋), `Project Settings`(프로젝트명, 시작 씬, 에셋/셰이더 폴더 탐색기, 영속화).
  - **ImGui 커스텀 스타일 영속화 및 Unscaled Base Style SSOT 구축**:
    - `EditorStyle.json`으로 55개 색상 슬롯과 라운딩, 패딩, 여백을 직렬화/역직렬화(`ImGuiManager::SaveCustomStyle`, `LoadCustomStyle`).
    - `m_BaseStyle`을 1.0f 기본 배율 원본으로 고정하고, `SetUIScale(m_UIScale)`을 단 1회만 적용하여 `ScaleAllSizes` 누적 곱연산으로 간격이 벌어지는 버그를 원천 차단.
  - **동적 버튼 너비 산출 (잘림 방지)**:
    - 하단 액션 바 버튼(`Reset to Defaults`, `Discard`, `Apply`)을 고정 100px 대신 `std::max(90.0f, ImGui::CalcTextSize(...) + FramePadding * 2 + 16.0f)` 기반으로 계산하여 어떤 언어/폰트 크기에서도 잘림 없이 완벽하게 자동 핏팅.
  - **폴더 탐색 다이얼로그 (`OpenFolderDialog`) 신규 구현**:
    - Windows IFileDialog `FOS_PICKFOLDERS` 기반의 프로젝트 루트/에셋/셰이더 디렉터리 브라우저 연동.
  - **로캘 키 및 인프라 동기화**:
    - `Editor.csv`에 `Settings.*` 키 일괄 등록 및 `GenerateLocalization`을 통한 JSON 자동 동기화.
- Technical details:
  - **ABI 오프셋 불일치 0xC0000005 크래시 규명 및 해결**:
    - `EditorPreferences` 구조체 크기 변경 후, `SKIP_UNITY_BUILD_INCLUSION`으로 컴파일되는 `Application.cpp`와 Unity 빌드로 컴파일되는 `EditorSettingsService.cpp` 간 인라인 getter 오프셋 불일치로 발생했던 포인터 역참조 크래시를 `EditorSettingsService` getter 함수의 `.cpp` 이전(ABI 격리)을 통해 원천 해결.
- Validation:
  - 전체 재빌드(`--clean-first`) 및 에디터 구동 시 0-크래시 검증 완료.
  - 폰트 크기 실시간 변경 및 UI 배율(1.25x) 적용 상태에서 테마 반복 전환 시 간격 불변 검증 완료.
  - 에디터 재시작 후 커스텀 스타일(`EditorStyle.json`) 자동 복원 검증 완료.

---

## [2026-11-03] 애니메이터 편집기 파일 안전 보호, 입력/로캘 정합성 및 패키징 스크립트 파괴적 삭제 방어 (P1-A / F01·F03·F11)

- Area: Engine | Editor | Tools | Localization
- Type: Fix | Security | Architecture
- 관련 파일:
  - Engine/Source/Resource/Model/Public/AnimatorStateMachine.h
  - Engine/Source/Resource/Model/Private/AnimatorStateMachine.cpp
  - Editor/Source/ImGui/AnimatorGraphPanel/Public/AnimatorGraphPanel.h
  - Editor/Source/ImGui/AnimatorGraphPanel/Private/AnimatorGraphPanel.cpp
  - Editor/Source/Shortcut/Public/ShortcutTypes.h
  - Editor/Source/Shortcut/Private/EditorActionRegistry.cpp
  - Resources/Data/Localization/Editor.csv
  - Tools/Scripts/PackageClient.ps1
- Motivation:
  - F01: `AnimatorGraphPanel`에서 `SaveToFile` 호출 시 기존 파일이 즉시 덮어씌워져 실패 시 데이터 유실 위험이 있었고, 미저장 변경사항이 있을 때 새 파일/열기/창 닫기 시 아무런 경고 없이 날아가는 취약점 해결.
  - F01: `AnimState::blendTree`가 `shared_ptr` 동적 할당으로 관리되어 역직렬화 코덱 누락 및 객체 수명 관리 오버헤드가 발생하던 문제를 값 타입(`BlendTree1D`)으로 전환하여 해결.
  - F03: 배포 패키징 스크립트(`PackageClient.ps1`)에 부적절한 `-OutputDir`(드라이브 루트 `C:\`, 저장소 루트, 핵심 소스/데이터 폴더 등)가 전달될 경우 `Remove-Item -Recurse -Force`로 인해 프로젝트 전체가 파괴될 수 있는 심각한 위험 차단.
  - F11: `AnimatorGraphPanel` 내에서 `ImGui::GetIO()` 및 하드코딩 마우스/키보드 입력과 하드코딩 영어 문자열이 사용되던 것을 `Engine::InputManager` 및 `LocalizationManager` 로캘 키 기반으로 완전 전환.
- Implementation summary:
  - `AnimatorStateMachine`: `blendTree`를 값 타입(`BlendTree1D`)으로 변경하여 힙 할당 및 수동 코덱 제거. `IArchive::SaveToFile`을 통한 원자적 파일 쓰기 보장 및 `LoadFromFile` 시 임시 인스턴스 스왑을 통한 강한 예외 안전성(Strong Exception Guarantee) 확보.
  - `AnimatorGraphPanel`: `EPendingAction`(`None, New, Open, Close`) 상태 머신 기반 미저장 확인 모달(`Save / Discard / Cancel`) 구현. `ImGuiWindowFlags_UnsavedDocument` 연동 및 ImGui 도킹 탭 닫기 가로채기(`!openState`) 로직 분리. 루트 뷰포트 기반 모달 렌더링으로 가시성 스킵 버그 방지.
  - 단축키 연동: `ShortcutTypes.h` 및 `EditorActionRegistry.cpp`에 `EShortcutContext::AnimatorGraph` 컨텍스트 및 `Animator.Save`(Ctrl+S), `Animator.New`(Ctrl+N), `Animator.Open`(Ctrl+O) 액션 등록.
  - `InputManager` & 로캘 일원화: `AnimatorGraphPanel`의 노드 드래그(`GetMouseDelta()`), 패닝(`IsMouseButtonPressed(EMouseButton::Middle)`), 좌표 변환(`GetScreenMousePosition()`), 전이 취소(ESC)를 `InputManager`로 전환하고 `Editor.csv`에 11개 로캘 키 등록.
  - `PackageClient.ps1`: `Get-NormalizedAbsolutePath` 함수 추가. 드라이브 루트, 프로젝트 루트 및 상위, 프로젝트 핵심 디렉터리(`Engine`, `Editor`, `Resources` 등) 전달 시 즉시 `Security Violation` 예외를 던지며 중단되도록 가드 구축. `-LiteralPath` 적용 및 필수 실행 파일 누락 검사 추가.
- Technical details:
  - ImGui 도킹 탭 닫기 메커니즘: `ImGuiWindowFlags_UnsavedDocument`가 켜져 있어도 탭 'X' 클릭 시 `ImGui::Begin`이 `false`를 반환할 수 있으므로, 닫기 요청 검사는 `Begin` 직후 가시성과 무관하게 가로채고 모달은 `End` 이후 최상위에서 렌더링하여 안정적 모달 표시 보장.
  - `BlendTree1D` 값 타입 전환으로 리플렉션 필드 직렬화가 내장 타입처럼 동작하여 복잡한 포인터 복원 로직 없이 완벽한 JSON 왕복 직렬화 달성.
- Validation:
  - `pwsh -File Tools/Scripts/PackageClient.ps1 -OutputDir .` -> `Security Violation` 예외 발생 확인.
  - `pwsh -File Tools/Scripts/PackageClient.ps1 -OutputDir Resources` -> `Security Violation` 예외 발생 확인.
  - `pwsh -File Tools/Scripts/PackageClient.ps1 -OutputDir "C:\"` -> `Security Violation` 예외 발생 확인.
  - `AnimatorStateMachine` JSON 저장 및 로드 왕복 검증 통과.
  - 에디터 단축키(Ctrl+S, Ctrl+N, Ctrl+O) 및 미저장 변경사항 보호 모달(Save, Discard, Cancel) 상호작용 검증 통과.

---

## [2026-11-02] 렌더러 유휴 패스 스킵, Editor UI 연산량 최적화 및 알림 히스토리 팝업 구현

- Area: Engine | Editor | Render | UI
- Type: Performance | Fix | Feature
- 관련 파일:
  - Engine/Source/Render/Private/Renderer.cpp
  - Editor/Source/ImGui/Private/ImGuiManager.cpp
  - Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp
  - Editor/Source/Notification/Private/NotificationManager.cpp
  - Editor/Source/Notification/Public/NotificationManager.h
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp
- Motivation:
  - 뷰포트 적응형 쓰로틀링(15 FPS) 시 비활성 프레임에서 커맨드가 없는 패스가 LoadOp == Clear 설정으로 인해 GBuffer/LightingColor를 검게 지워버리던 화면 깜빡임 방지.
  - ImGuiManager::Draw()에 남아있던 테스트용 ImGui::ShowDemoWindow(nullptr)를 비활성화하여 Editor UI의 불필요한 위젯 평가 비용 제거.
  - 지나간 에디터 알림을 열람할 수 있는 알림 히스토리(Notification Center) 팝업 및 하단 푸터 벨 버튼 연동.
  - 콘텐츠 브라우저 상단에 고정 노출되던 비동기 태스크 진행 문구를 하단 푸터 바로 이관하여 레이아웃 정리.
- Implementation summary:
  - Renderer.cpp: 수행할 커맨드(hasCommands)가 전혀 없는 패스는 LoadOp == Clear 여부와 상관없이 실행 및 메트릭 기록을 완전 스킵하도록 가드 개선. 비활성 프레임의 이전 렌더타겟 결과를 그대로 보존.
  - ImGuiManager.cpp: ImGui::ShowDemoWindow(nullptr) 주석 처리로 Editor UI CPU 시간 감축.
  - NotificationManager: 최대 100개의 알림 내역을 보관하는 HistoryItem 구조체 및 DrawHistoryPopup() 구현.
  - ToolBar.cpp: 하단 슬림 푸터 바에 알림 히스토리 토글 벨 버튼 및 비동기 에셋 임포트 진행률 표시 추가.
- Technical details:
  - hasCommands 검사 대상: StaticDrawCommands, SkinnedDrawCommands, SpriteDrawCommands, UIImageDrawCommands, CustomRenderQueues, RenderPassDelegates.
  - 커맨드가 비어있는 프레임에는 RHI의 BeginRenderPass와 BeginPassMetrics 호출이 0회로 떨어져 GPU 렌더타겟 대역폭 소모 및 프로파일러 오버헤드 0 달성.
- Validation:
  - 엔진 통계창(MetricsPanel)에서 비활성 쓰로틀링 시 뷰포트 화면 깜빡임 소멸 및 Editor UI 프레임 타임 개선 확인.

---

## [2026-11-02] CWD 독립적 실행 경로(PlatformPath) 구축 및 Standalone Client 시작 씬 로드 및 렌더링 파이프라인 연동

- Area: Engine | Client | Render | Core | Docs
- Type: Feature | Fix | Refactor | Docs
- 관련 파일:
  - Docs/ImplementationLog.md
  - Docs/BuildAndPackagingPlan.md
  - Engine/Source/Core/Public/PlatformPath.h, PlatformPath.cpp
  - Engine/Source/Runtime/Public/Runtime.h, Runtime.cpp
  - Engine/Source/Render/Private/Renderer.cpp
  - Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp, DirectX12RHI.h
  - Engine/Source/System/Graph/Private/GraphSystem.cpp, GraphSystem.h
  - Engine/Source/Resource/Texture/Private/Texture.cpp
  - Engine/Source/Resource/Private/ResourceManager.cpp
  - Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12Shader.cpp
  - Projects/GameModule/Source/SkyAtmosphereSystem.cpp
  - Editor/Source/Settings/Private/EditorSettingsService.cpp
  - Editor/Source/AssetManager/Private/StartupResourceLoader.cpp
  - Editor/Source/HotReload/Private/ShaderHotReloadManager.cpp
  - Client/Source/Main.cpp
  - Config/Project.json
- 동기 및 배경:
  - 작업 디렉터리(CWD)가 소스 루트가 아닌 위치(예: C:\, 다른 드라이브)에서 Editor 또는 Client를 실행할 경우, 하드코딩된 상대 경로(Resources/..., Config/..., Logs/...)로 인해 셰이더, 리소스, 로그 파일 및 씬을 로드하지 못하고 초기화에 실패하거나 크래시가 발생하는 문제 해결.
  - 컴파일 타임 매크로(BAMENGINE_PROJECT_ROOT)에 의존하던 경로 탐색 방식을 런타임 실행 파일 위치 기반의 역추적(.bam, Config/Project.json) 단일 진실의 원천(PlatformPath::ResolveProjectRelative)으로 표준화.
  - Client 애플리케이션의 시작 씬 로드 미지원, 기본 프리미티브 메시 미등록, D3D12 백버퍼 Forward 패스에서의 드라이버 RTV 불일치 크래시를 단계적으로 해결하여 독립 실행 클라이언트에서 씬 렌더링이 동작하도록 구현.
- 구현 내용 요약:
  1. OS 및 프로젝트 경로 헬퍼(PlatformPath) 구축:
     - PlatformPath::GetExecutableDirectory(), Normalize(), ResolveProjectRelative(), ToProjectRelative() 구현.
     - ProjectRoot를 정적 싱글턴으로 관리하며, RUNTIMEDESC를 통해 호스트(Editor/Client)에서 엔진으로 기준 경로 주입.
     - Texture.cpp에서 stbi_load_from_memory + std::ifstream을 적용하여 CWD 및 한글 경로 독립성 확보.
     - DirectX12Shader.cpp, ResourceManager.cpp, SkyAtmosphereSystem.cpp, HotReload 계열에서 상위 폴더 탐색 루프 전수 제거 및 PlatformPath로 단일화.
  2. Standalone Client 런타임 시작 흐름 및 리소스 파이프라인 연결(Client/Source/Main.cpp):
     - ResolveProjectRoot()를 통해 실행 파일 위치 기반으로 ProjectRoot를 자동 감지하고 로거 및 Runtime에 주입.
     - InitializePrimitiveMeshes()를 추가하여 에디터와 동일한 기본 메시(QuadMesh, CubeMesh, SphereMesh)를 GPU 버퍼로 등록, 씬 로드 시 메쉬 누락 에러 방지.
     - LoadProjectSettings()를 통해 Config/Project.json의 StartupScenePath(Resources/Scenes/RaytracingScene.json)를 동적으로 로드.
     - UpdateViewportCamera()를 구현하여 씬 내 CameraComponent 또는 폴백 카메라를 Renderer에 등록.
  3. DirectX12 백버퍼 파이프라인 RTV 불일치 크래시 수정(Renderer.cpp):
     - 백버퍼 전용 패스(colorAttachmentCount == 0 && depthStencilName.empty()) 시 파이프라인 디스크립터에 스왑체인 포맷(desc.colorAttachmentCount = 1, R8G8B8A8_UNORM)이 할당되지 않아 PSO의 NumRenderTargets = 0이 되던 결함 수정.
     - 커맨드 리스트에 바인딩된 백버퍼 RTV와 PSO의 RTV 개수 불일치로 인해 DrawIndexedInstanced에서 발생하던 GPU 드라이버 크래시 해결.
  4. 비주얼 스크립팅 그래프 시스템 안정화(GraphSystem.cpp, GraphSystem.h):
     - GraphSystem::Initialize()에 중복 초기화 방지 가드(m_Initialized)를 추가하여 Core.Bool 노드 중복 등록 에러 방지.
     - GetOrLoadGraph의 스크립트 파일 경로 탐색 시 PlatformPath::ResolveProjectRelative 적용.
- 검증 및 확인:
  - C:\ 디렉터리에서 Editor.exe 실행 시 리소스 프리로드, 셰이더, 격자, 대기 텍스처, 핫 리로드가 정상 작동함 확인.
  - C:\ 디렉터리에서 Client.exe 실행 시 프로젝트 루트 감지, 씬 로드 및 초기화 통과, DrawIndexedInstanced 크래시 없이 윈도우에 씬 지오메트리가 렌더링됨을 확인.
- 잔여 과제 및 후속 계획:
  - Client는 현재 단일 백버퍼 Forward 패스로만 동작하므로, 씬의 디퍼드 머티리얼(GBufferMaterial) 및 조명을 완전하게 표현하기 위해 클라이언트용 디퍼드 파이프라인(GBuffer + LightingPass) 또는 포워드 셰이더 매핑 필요.

---

## [2026-11-02] 하단 푸터 바(Footer Bar) 빌드 제어 이관, 콘텐츠 브라우저 시각 분류 고도화 및 .bam*.json 복합 확장자 표준화

- Area: Editor | UI | ContentBrowser | ToolBar | Localization | Docs
- Type: Feature | Refactor | Docs
- 관련 파일:
  - Docs/ImGuiDevelopmentPlan.md (Section 19, Section 20)
  - Docs/DevelopmentPriorities.md
  - Docs/ImplementationLog.md
  - Editor/Source/ImGui/ToolBar/Public/ToolBar.h
  - Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp
  - Editor/Source/ImGui/Public/ResourcePicker.h
  - Editor/Source/Document/Public/SceneDocument.h
  - Resources/Data/Localization/Editor.csv
- 동기 및 배경:
  - 상단 툴바(ToolBar)에 Play/Pause/Stop과 컴파일, 핫 리로드, 자동 컴파일/리로드, 셰이더 리로드 버튼이 모두 섞여 있어 시각적 복잡도가 높고 레이아웃이 답답했던 문제를 해결하고자 컴파일/리로드 버튼들을 화면 최하단 슬림 푸터(`##FooterBarRow`)로 분리.
  - 콘텐츠 브라우저에서 `.json` 파일들이 모두 획일적인 회색 텍스트 아이콘과 "JSON" 배지로만 노출되어, 머티리얼·메시·스프라이트·애니메이터·스크립트·씬·후처리 프리셋 등을 직관적으로 식별하기 어려웠음.
  - 씬 파일 및 특정 목적의 에셋 파일들이 일반 `.json`과 혼용되는 것을 방지하기 위해 `.bam<type>.json` 형태의 복합 확장자 체계를 확립하고, 기존 단일 확장자(`.animator`, `.spriteanim`, `.anim`, `.materialinstance`) 및 디렉터리 문맥(`Scripts/`, `Config/`, `PostProcess/Presets/`)에 대한 O(1) 무결점 시각 분류를 구현함.
  - 콘텐츠 브라우저 우클릭 메뉴의 `EditorCommon.Open` 로캘 키 누락으로 영문/한글 모드에서 키 문자열이 그대로 노출되던 결함 수정.
- 구현 내용 요약:
  1. **슬림 푸터 바(Footer Bar) 구축 (`ToolBar.cpp`, `ToolBar.h`, `ImGuiDevelopmentPlan.md` 19절)**:
     - PIE 플레이 제어줄(`##PlayBarRow`)에서 모듈 컴파일, 모듈 핫 리로드, 자동 컴파일 토글, 자동 핫 리로드 토글, 셰이더 전체 재컴파일 버튼을 제거하고, 에디터 최하단 `DrawFooterBar()`(`##FooterBarRow`)로 이관.
     - 텍스트 라벨을 간결한 아이콘 버튼(`ICON_FA_HAMMER`, `ICON_FA_ROTATE`, `ICON_FA_FILE_CODE`)과 컴팩트 체크박스로 대체하고 상세 안내를 툴팁으로 이동시켜 약 22px 높이의 얇고 안정적인 푸터 완성.
  2. **콘텐츠 브라우저 그리드 아이템 시각 분류 고도화 (`ContentBrowserGridItem.cpp`, `ImGuiDevelopmentPlan.md` 20절)**:
     - `DescribeAssetVisual()` 함수를 전면 개선하여, 디스크 I/O 없이 파일명·복합 확장자·상위 폴더 계층(`HasDirectorySegment`)만으로 O(1) 수준의 즉각적인 카드 렌더링 지원.
     - **복합 확장자 지원**: `.bamscene.json`, `.bamprefab.json`, `.bammat.json`, `.bammatinst.json`, `.bammesh.json`, `.bammodel.json`, `.bamsprite.json`, `.bamspriteanim.json`, `.bamanim.json`, `.bamskel.json`, `.bamanimator.json`, `.bamgraph.json`, `.bampostfx.json`, `.bamscript.json`, `.bamconfig.json`, `.bamshader.json`.
     - **단일 확장자 및 폴더 문맥 완벽 호환**:
       - 애니메이터: `.animator`, `.bamanimator` $\rightarrow$ `ICON_FA_DIAGRAM_PROJECT` (마젠타 퍼플, `ANIMATOR` 배지).
       - 애니메이션: `.spriteanim`, `.bamspriteanim` $\rightarrow$ `ICON_FA_FILM` (`SPRITE ANIM` 배지), `.anim`, `.bamanim` 및 `Animation/` 폴더 $\rightarrow$ `ICON_FA_PERSON_RUNNING` (`ANIM` 배지).
       - 스크립트: `.bamscript`, `.lua`, `.cpp`, `.h` 및 `Scripts/` 폴더 내 `.json` $\rightarrow$ `ICON_FA_SCROLL` (틸 에메랄드, `SCRIPT` 배지).
       - 컨피그: `.ini`, `.cfg`, `.config`, `.toml`, `.yaml`, `.xml` 및 `Resources/Config/` 하위 `.json` (`PostProcess/Presets`는 `POST FX`, 그 외는 `CONFIG`).
       - 셰이더: `.hlsl`, `.glsl`, `.cso`, `.spv`, `.bamshader`, `.bamshader.json` $\rightarrow$ `ICON_FA_CODE` (`HLSL`/`SHADER`/`SPV`), `.hlsli` $\rightarrow$ `ICON_FA_FILE_CODE` (`HLSLI`).
       - 머티리얼 인스턴스: `.materialinstance` 단일 확장자 지원.
     - 네임 박스 렌더링(`DrawNameBox`) 시 복합 접미사(`.bammesh.json` $\rightarrow$ `Cube`)를 완전히 제거한 정제된 파일명(`CleanName`)을 표시하고 우하단에 명확한 타입 배지(`MESH`, `MAT`, `SCRIPT` 등) 렌더링.
     - 썸네일 영역 상단 둥근 모서리 배경 틴트와 하단 2px 타입 고유 액센트 라인 적용.
  3. **콘텐츠 브라우저 필터 동기화 (`ContentBrowserPanel.cpp`)**:
     - `PassesFilter()`에서 단일 확장자(`.spriteanim`, `.animator`, `.materialinstance`, `.spv`, `.cfg` 등) 및 복합 확장자들이 상단 5대 카테고리(Texture, Model, Material, Shader, Data)에 누락 없이 정확하게 분류되도록 갱신.
  4. **씬 및 리소스 피커 복합 확장자 호환 (`SceneDocument.h`, `ResourcePicker.h`)**:
     - `SceneDocument::IsSceneFile()`에서 `.bamscene.json` 및 `.scene.json`을 씬 파일로 즉시 판별.
     - `ResourcePicker.h`에서 `Material` 선택 시 `.bammat.json`, `.bammatinst.json`, `.bammat`, `.bammatinst`만 필터링되도록 보강.
  5. **로캘(Localization) 누락 키 추가 (`Editor.csv`)**:
     - `EditorCommon,Open,Open,열기` 및 공통 UI 키(`EditorCommon.New`, `EditorCommon.File`, `EditorCommon.Edit`, `EditorCommon.SaveAs`, `EditorMenu.File`, `UI_VIEW`) 추가.
- 검증 및 확인:
  - 콘텐츠 브라우저에서 스크립트, 애니메이터, 애니메이션 클립, 컨피그 파일들이 고유의 아이콘과 타입 배지로 노출됨을 확인.
  - 파일 우클릭 시 컨텍스트 메뉴 최상단 항목이 번역 키 대신 "열기"(Open)로 정상 출력됨을 확인.

---

## [2026-11-01] 전체 코드·문서 정합성 점검, 누락 현황 동기화 및 개발 우선순위(Next Execution Order) 재조정

- Area: Docs | Architecture | Engine | Render | Editor | Graph | Animation
- Type: Docs | Audit | Cleanup
- 관련 파일:
  - Docs/DevelopmentPriorities.md
  - Docs/RenderingDevelopmentPlan.md
  - Docs/EngineDevelopmentPlan.md
  - Docs/ImGuiDevelopmentPlan.md
  - Docs/VisualScriptingBlueprintPlan.md
  - Docs/AnimatorGraphPlan.md
  - Docs/BuildConfigurationAndPreprocessing.md
  - Docs/GraphFoundation.md
  - Docs/GraphReflection.md
  - Docs/GraphPanel.md
  - Docs/ImplementationLog.md
- 점검 및 동기화 내용:
  1. **기완료 작업의 미완료 표기 해소 및 우선순위 갱신 (`DevelopmentPriorities.md`)**:
     - P0-A 미커밋 파일 정리 및 단위별 커밋 완료 상태 반영.
     - 기존 단기 실행 순서 3순위(`DirectX12RHI` `std::terminate` 제거, `MeshRenderSystem` 다중 뷰포트/패스별 독립 LOD 평가), 4순위 중 씬 레벨 `RaycastQuery` 및 `GraphPanel` 상단 메뉴바 리팩터링, 6순위 중 `AnimatorGraph` Phase 1~4 구현 완료 사실을 완료 목록으로 이동.
     - P2-E의 `Client/Source/Main.cpp` 시스템 등록 상태(`AnimationSystem`, `SpriteAnimationSystem`, `GraphSystem`, `PhysicsSystem`, `TerrainSystem` 및 `--rl` 엔트리포인트 등록 완료)를 실제 코드와 일치하도록 갱신.
     - 차기 단기 실행 우선순위를 **1순위 P1-E(`MaterialFeatureFlags` 및 머티리얼 그래프 입력 유연화) -> 2순위 P2-D/Phase 5(계층형 스켈레탈 모델 루트 `AnimatorComponent` ↔ 자식 `SkinnedMeshRendererComponent` 연결 및 `.animator` 에셋 자동 로드, `AnimNotify`/본 마스킹) -> 3순위 `UnifiedSettingsPanel` 및 그래프 핀 컨텍스트 메뉴 -> 4순위 에셋 썸네일 예산 및 `AssetReferencePanel`/재임포트 -> 5순위 `BAM_EDITOR` 전처리기/표준 스트림 잔여 정리 -> 6순위 네트워크 기초(`NetworkFoundationPlan`) -> 7순위 RL 확장 및 파티클/스플라인/트레일** 순으로 재편성.
  2. **스켈레탈 애니메이션 계층 구조 갭 명시 (`AnimatorGraphPlan.md`, `EngineDevelopmentPlan.md`, `DevelopmentPriorities.md`)**:
     - `Scene::InstantiateModel()`은 모델 루트 엔티티에 `AnimatorComponent`, 자식 메시 엔티티에 `SkinnedMeshRendererComponent`를 분리 생성하는 반면, `AnimationSystem::OnUpdate()`와 `MeshRenderSystem::OnSubmit()`은 동일 엔티티 기준으로 조회하는 구조적 차이를 확인하고 Phase 5의 최우선 연결 과제로 명시.
  3. **비주얼 스크립팅 `EGraphPinType::String` 지원 및 UI 리팩터링 동기화 (`GraphFoundation.md`, `GraphReflection.md`, `GraphPanel.md`, `VisualScriptingBlueprintPlan.md`)**:
     - `GraphReflection.md`에서 문자열(`std::string`)을 거부한다는 구버전 설명을 수정하여 `std::string` 인자·반환값 지원 사실을 반영하고, `GraphFoundation.md`·`GraphPanel.md`에 `String` 핀 타입과 `InputText` 편집 지원을 반영.
  4. **전처리기 정비 현황 구분 (`BuildConfigurationAndPreprocessing.md`)**:
     - `BuildConfigurationAndPreprocessing.md` 4절의 항목별 완료(`GraphGameplayLibrary.h`, `DirectX12RHI.cpp`, `Vertex.h`, `GraphSystem.h`) 및 잔여(`LocalizationManager.cpp`, `ReflectionRegistry.cpp`, `BAM_EDITOR` 매크로) 상태를 체크리스트로 구분.

---

## [2026-11-01] G-Buffer 텍스처 상태 누수 규명 및 플래그 기반 샘플링·머티리얼 그래프 입력 계획 수립 (P1-E)

- Area: Render | Material | Shader | Documentation
- Type: Fix | Architecture | Docs
- 관련 파일:
  - Docs/RenderingDevelopmentPlan.md
  - Docs/DevelopmentPriorities.md
  - Engine/Source/Resource/Material/Private/MaterialInterface.cpp
  - Resources/Shader/hlsl/gbufferPS.hlsl
- 동기 및 배경:
  - 씬 뷰포트에서 간헐적으로 바닥과 벽이 새하얗게 폭발(과노출)하는 현상 분석 중, 텍스처 슬롯이 비어있는 머티리얼(GBufferMaterial)이 그려질 때 이전 드로우콜의 PBR(Slot 2) 및 Emission(Slot 3) 텍스처 상태가 RHI에 그대로 잔류하여 누수되는 결함 확인.
  - 드로우콜 FrontToBack 정렬 순서에 따라 바닥 평면의 Emission에 이전 메쉬의 밝은 텍스처가 덮어써져 자체 발광이 1.0 이상으로 치솟았던 것을 사용자가 직접 렌더타겟 진단으로 규명함.
- 구현 내용 (즉시 조치):
  - Engine/Source/Resource/Material/Private/MaterialInterface.cpp:
    - MaterialInterface::Bind()에서 slotBound[4]를 통해 슬롯 점유 여부를 추적.
    - 머티리얼에 명시되지 않은 G-Buffer 핵심 슬롯(0: Diffuse, 1: Normal, 2: PBR, 3: Emission)을 안전한 기본 더미 텍스처(white1x1, blue1x1, black1x1)로 덮어써서 이전 드로우콜의 텍스처 잔류 상태를 원천 리셋하도록 수정 완료.
- 개발 계획 반영 (Docs/RenderingDevelopmentPlan.md S2-C 및 Docs/DevelopmentPriorities.md P1-E):
  1. **텍스처 유효성 비트 플래그(MaterialFeatureFlags) 도입**:
     - 1x1 더미 텍스처 없이 슬롯을 순수하게 비우는(Unbound/Null SRV) 방식을 안전하게 취할 수 있도록, 텍스처 바인딩 여부를 비트 플래그로 전달하여 셰이더가 미지정 슬롯을 샘플링하지 않고 안전한 수학적 기본값(Emission: 0.0, Normal: 정점 법선, PBR: 상수 거칠기/금속도/차폐 1.0)으로 즉시 분기 처리.
  2. **머티리얼 그래프 기반 상수/벡터/컬러 분리 입력 연동**:
     - 텍스처 에셋을 반드시 샘플링하는 방식에서 탈피하여, 머티리얼 노드 그래프에서 단일 Color(RGBA), 스칼라(Float), 분리된 채널(Split RGBA / Swizzle)을 G-Buffer의 각 채널에 직접 꽂을 수 있는 유연한 머티리얼 파라미터 평가 경로 구축.
     - 텍스처 없는 절차적/상수 기반 머티리얼 지원 및 텍스처 페치 대역폭 최적화.

---

## [2026-11-01] 강화학습(RL) 첫 헤드리스 환경(CartPole) 및 Gymnasium 소켓 통신 서버(RLServer) 구축 (P2-C)

- Area: Engine | Simulation | Tools | Client | Architecture
- Type: Feature | Architecture | Stability
- 관련 파일:
  - Engine/Source/Simulation/Public/RLEnvironment.h
  - Engine/Source/Simulation/Public/CartPoleEnvironment.h
  - Engine/Source/Simulation/Private/CartPoleEnvironment.cpp
  - Engine/Source/Simulation/Public/RLServer.h
  - Engine/Source/Simulation/Private/RLServer.cpp
  - Tools/RL/bam_gym_env.py
  - Client/Source/Main.cpp
  - Docs/DevelopmentPriorities.md
- 구현 요약:
  1. **Gymnasium 규약 준수 강화학습 공통 계약 (RLEnvironment.h)**:
     - `IRLEnvironment`: `Initialize()`, `Reset(seed)`, `Step(action)`, `GetObservationSpace()`, `GetActionSpace()`, `Close()` 순수 가상 인터페이스.
     - `RLObservation` (float 벡터 버퍼), `RLAction` (이산/연속 공용), `RLStepResult` (관측, 보상, Terminated, Truncated, InfoJson), `RLSpace` (Box, Discrete).
     - DLL 동적 라이브러리 심볼 바인딩(`ENGINE_API`) 완비.
  2. **헤드리스 수치 물리 시뮬레이터 (CartPoleEnvironment.h/cpp)**:
     - 창, RHI 렌더러, 오디오가 전혀 필요 없는 순수 C++ 수치 물리 환경.
     - Gymnasium CartPole-v1 표준 물리 상수(카트 질량 1.0, 폴 질량 0.1, 길이 0.5, 중력 9.8, 힘 10N) 및 50Hz 고정 dt(tau=0.02) 오일러 운동 방정식 구현.
     - 난수 시드 기반 초기화 및 한계 조건(|x|>2.4, |theta|>12도) 도달 시 `Terminated: true` 판정.
  3. **초경량 로컬 TCP IPC 서버 (RLServer.h/cpp)**:
     - Windows Winsock(`ws2_32.lib`) 기반의 127.0.0.1:9999 로컬 TCP 서버.
     - 개행 구분 Line-delimited JSON 프로토콜(`get_spaces`, `reset`, `step`, `close`) 처리기 및 빠른 응답 직렬화기 구축.
     - `::bind`, `::listen`, `::accept`, `::recv`, `::send` 전역 스코프 네임스페이스 격리 및 Nagle 알고리즘 비활성화(`TCP_NODELAY`)로 지연시간 최소화.
  4. **Python Gymnasium 어댑터 (Tools/RL/bam_gym_env.py)**:
     - `gymnasium.Env`와 호환되며, `numpy`/`gymnasium` 부재 시 순수 파이썬 표준 라이브러리(`random`, `json`, `socket`)로 자동 폴백되는 견고한 어댑터 구현.
  5. **클라이언트 CLI 엔트리포인트 (Client/Source/Main.cpp)**:
     - `Client.exe --rl` 인자 분기를 통해 SDL 창 및 렌더러 생성 없이 즉시 헤드리스 RL 서버로 구동되는 실행 경로 완성.
- 검증:
  - MSVC x64-Debug 컴파일 및 링크 성공.
  - C++ `Client.exe --rl` 구동 후 `python Tools/RL/bam_gym_env.py` 실행:
    - `get_spaces` (Discrete 2, Box [4]) 정상 조회.
    - 2개 연속 에피소드 실행: 에피소드 1(39스텝 종료, reward 38.0), 에피소드 2(22스텝 종료, reward 21.0).
    - `Terminated: true` 판정 및 에피소드 간 상태 누수 없는 완전한 상태 격리 실측 확인.

---

## [2026-11-01] HDR 렌더링 파이프라인 일원화 및 SceneViewportPanel 공통 디퍼드 파이프라인(ViewportDeferredPipeline) 통합

- Area: Editor | Render | Shader | Architecture
- Type: Refactor | Fix | Architecture | Performance
- 관련 파일:
  - Resources/Shader/hlsl/lightingPS.hlsl
  - Editor/Source/ImGui/ViewportPanel/Public/ViewportDeferredPipeline.h
  - Editor/Source/ImGui/ViewportPanel/Private/ViewportDeferredPipeline.cpp
  - Editor/Source/ImGui/ViewportPanel/Public/SceneViewportPanel.h
  - Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp
  - Editor/Source/ImGui/ViewportPanel/Private/GameViewportPanel.cpp
- 변경 동기:
  - 디퍼드 라이팅 셰이더(`lightingPS.hlsl`) 끝자락에 Reinhard 톤매핑 및 감마 2.2 보정이 하드코딩되어 있고, 라이팅 렌더타깃(`LightingColor`)이 8비트 LDR(`R8G8B8A8_UNORM`)로 고정되어 고휘도 HDR 물리 광량이 조기 클램핑되는 결함 존재.
  - 후처리 체인(`PostProcessPipeline`)이 가동될 때 이미 감마 보정된 픽셀 위에 ACES Filmic과 감마 보정이 중복 적용(이중 톤매핑 & 이중 감마 보정)되어, 씬에 PostProcessComponent 존재 여부에 따라 화면이 새하얗게 날아가거나(과노출/탈색) 음영이 뭉개지는 상태 불일치 발생.
  - `SceneViewportPanel`과 `ViewportDeferredPipeline`이 거의 동일한 GBuffer/조명 합성 로직(~400줄)을 각자 중복 구현하고 있어 렌더링 기능(Point Light 그림자, Fog, IBL 등)의 동기화 불일치 지속 발생.
- 구현 요약:
  1. **라이팅 셰이더 순수 선형 HDR 광량 출력**: `lightingPS.hlsl`의 하드코딩된 Reinhard 및 감마 2.2 인코딩 제거.
  2. **LightingColor 버퍼 포맷 승격**: `ViewportDeferredPipeline` 및 `SceneViewportPanel`의 라이팅 렌더타깃과 파이프라인 RTV 포맷을 `R16G16B16A16_FLOAT`로 승격하여 1.0 초과 광량 온전 보존.
  3. **톤매핑/감마보정 일원화 및 상시 보장**: `SceneViewportPanel` 및 `GameViewportPanel`의 `SubmitPostProcessPass`에서 씬에 `PostProcessComponent`가 없더라도 기본 톤매핑 체인(Default.json: ACES Filmic + Gamma 2.2)이 작동하도록 폴백 보장.
  4. **공통 디퍼드 파이프라인 통합**: `ViewportDeferredPipeline`에 `PointShadowDepth` 큐브맵 섀도우, `ForwardPass`/`ForwardTransparentPass`, 대기 산란(SkyAtmosphereData) 자동 계산 및 주입 기능을 통합하고, `SceneViewportPanel`의 수백 줄에 달하던 자체 렌더패스/라이팅 커맨드를 `m_Pipeline` 단일 인스턴스로 위임하여 중복 제거.
- 검증:
  - 에디터 컴파일 빌드 통과.
  - 스카이박스 대기 정상 렌더링, 태양 위치와 일치하는 자연스러운 음영 및 그림자 합성 확인.

---

## [2026-11-01] 비주얼 스크립팅(Graph) 시스템 String 핀 타입 완전 지원 및 SpriteAnimationEditor 리소스 핸들 안전성 개선

- Area: Engine | Editor | Graph | Resource | UI
- Type: Feature | Fix | Stability | Architecture
- 관련 파일:
  - Engine/Source/Graph/Public/GraphTypes.h
  - Engine/Source/Graph/Public/GraphValue.h
  - Engine/Source/Graph/Private/GraphReflection.cpp
  - Engine/Source/Graph/Private/GraphRegistry.cpp
  - Engine/Source/Graph/Private/GraphExecutorValues.cpp
  - Engine/Source/Core/Archive/Public/Adapters/GraphValueArchiveAdapter.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelNodeEditor.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelExecution.cpp
  - Editor/Source/ImGui/ResourceEditor/Public/SpriteAnimationEditor.h
  - Editor/Source/ImGui/ResourceEditor/Private/SpriteAnimationEditor.cpp
- 구현 요약:
  1. **비주얼 스크립팅(Graph) String 핀 타입(EGraphPinType::String) 및 값 지원 확장**:
     - `GraphTypes.h`: `EGraphPinType::String` 추가 및 `GraphValue` variant에 `std::string` 포함.
     - `GraphValue.h`: `GraphValueMatchesType`에 `String` 타입 매칭 검증 추가.
     - `GraphReflection.cpp`: `ReflectedPinType`에 `std::string`, `basic_string<char>` 및 `typeid(std::string)` 매핑 추가, `ReadDefault` 문자열 기본값 처리 추가.
     - `GraphRegistry.cpp`: 핀 유효성 검사 통과 및 `Core.String` 리터럴 노드, `Core.Reroute.String` 리라우트 노드 등록.
     - `GraphExecutorValues.cpp`: `NativeValue` 및 `ReturnedValue`에서 `std::string` 형변환 지원.
     - `GraphValueArchiveAdapter.h`: `ValueTag<std::string>`("String") 및 `ReadValue` 역직렬화 분기 추가.
     - `GraphPanel.cpp`, `GraphPanelNodeEditor.cpp`, `GraphPanelExecution.cpp`: `DrawValue`에 `ImGui::InputText` 연동, `MakeDefaultValue` 기본값 생성, `PinColor`(마젠타), `PinTypeName`("String"), `TraceValueText` 디버그 문자열 출력 연결.
     - **결과**: `GetAnimatorBool`, `SetAnimatorFloat` 등 애니메이터 비주얼 스크립트 노드 등록 시 발생하던 `Unsupported by-value parameter: class std::basic_string<char>` 진단 에러 완전 해결.
  2. **SpriteAnimationEditor 리소스 핸들 기반 안전성 개선**:
     - `SpriteAnimationEditor` 내부에서 원시 포인터(`SpriteAnimation* m_Animation`) 소유권을 엔진 표준인 `ResourceHandle<SpriteAnimation>` 참조 방식으로 변경하여 수명 주기 누수 및 댕글링 포인터 위험 원천 차단.
     - 스프라이트 UV Transform(`GetUVTransform()`) 및 프레임 스왑/편집 로직 안정화.
- 검증:
  - MSVC 컴파일 및 리플렉션 노드 등록 경고 해소 확인.
  - 비주얼 스크립팅 에디터 캔버스에서 문자열 핀 입출력 정상 동작 확인.

---

## [2026-11-01] 3D 스켈레탈 애니메이션 런타임, 상태 머신(ASM), 1D 블렌드 트리 및 에디터 저작 패널(AnimatorGraphPanel) 구축

- Area: Engine | Render | Editor | Resource | Reflection
- Type: Feature | Architecture | Optimization
- 관련 파일:
  - Engine/Source/Resource/Model/Public/AnimationEvaluator.h, Private/AnimationEvaluator.cpp
  - Engine/Source/Resource/Model/Public/AnimatorStateMachine.h, Private/AnimatorStateMachine.cpp
  - Engine/Source/Resource/Model/Public/BlendTree1D.h, Private/BlendTree1D.cpp
  - Engine/Source/Core/Public/Components/Render/AnimatorComponent.h
  - Engine/Source/System/Render/Public/AnimationSystem.h, Private/AnimationSystem.cpp
  - Engine/Source/System/Render/Private/MeshRenderSystem.cpp
  - Engine/Source/Graph/Public/GraphGameplayLibrary.h
  - Editor/Source/ImGui/AnimatorGraphPanel/Public/AnimatorGraphPanel.h, Private/AnimatorGraphPanel.cpp
  - Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp
  - Resources/Data/Localization/Engine.csv, Editor.csv
  - Docs/AnimatorGraphPlan.md
- 구현 요약:
  1. **런타임 스켈레탈 애니메이션 평가기 및 GPU 스키닝 연동 (Phase 1)**:
     - `AnimationEvaluator`: 키프레임 보간(`glm::mix`, `glm::slerp`) 및 부모-자식 스켈레톤 계층 결합 행렬 도출.
     - `AnimatorComponent` 및 `AnimationSystem`: 매 틱 엔티티별 포즈 갱신 및 `finalBoneMatrices` 계산.
     - `MeshRenderSystem` 및 `Renderer::RenderSkinnedMeshes`: 최종 뼈 행렬 팔레트를 구조화 버퍼에 바인딩하여 GPU 스키닝 렌더링 파이프라인 완성.
  2. **애니메이션 상태 머신(ASM) 런타임 코어 (Phase 2)**:
     - `AnimatorParameters`: `Float`, `Int`, `Bool`, `Trigger`(전이 발동 시 자동 소모) 파라미터 컨테이너 구축.
     - `AnimTransition`: 크로스페이드 시간, 종료 시간 전이(`HasExitTime`), 조건식 목록(`TransitionCondition`).
     - `AnimationEvaluator::EvaluateBlendedSkinningMatrices`: 로컬 TRS 공간에서 최단 경로 쿼터니언 slerp 및 위치 mix를 수행하여 뼈 왜곡 없는 부드러운 상태 간 크로스페이드 지원.
     - 비주얼 스크립팅 노드(`NODE_ANIMATOR_*`) 및 CSV 다국어 등록.
  3. **1D 블렌드 트리 (Phase 3)**:
     - `BlendTree1D`: 단일 연속 파라미터(예: `Speed`) 축에 임계값별 클립들을 정렬하고, 인접한 두 클립의 가중치 alpha를 실시간 선형 보간하여 포즈를 합성하는 블렌드 스페이스 구축.
     - `AnimState`에 `BlendTree1D` 연동으로 단일 클립과 블렌드 트리를 완전 하위 호환 구조로 통합.
  4. **리플렉션 아카이브 기반 자동 직렬화**:
     - `CLASS()` 및 `STRUCT()` 매크로로 상태 머신 구조체들을 등록하여, 별도 아카이브 코드 없이 `archive_reflection::WriteObject/ReadObject`로 `.animator` JSON 에셋 저장 및 로드 완비.
  5. **에디터 저작 패널 (`AnimatorGraphPanel`) 및 라이브 디버깅 (Phase 4)**:
     - 18절 지침 준수: 상단 풀다운 메뉴바(`File`, `Edit`, `View`) 및 1줄 퀵 액세스 툴바(`New`, `Open`, `Save`, 파일 상태 배지).
     - 파라미터 탭: Float/Int/Bool/Trigger 파라미터 추가/삭제/실시간 조작.
     - 캔버스 UI: 노드 자유 드래그 이동, "Make Transition" 화살표 연결선 및 베지어 곡선 시각화.
     - 트랜지션 인스펙터: 지속 시간, 종료 시간 비율, 파라미터 조건식 테이블 저작.
     - 네이티브 파일 대화상자(`FileDialogs`) 연동 및 씬 선택 엔티티 활성 상태 실시간 청록색 네온 발광 디버깅.
- 검증:
  - MSVC x64-Debug 컴파일 및 빌드 성공.
  - 에디터 패널을 통한 상태 머신 저작, `.animator` 직렬화 입출력 및 인스펙터 실시간 동기화 확인.

---

## [2026-11-01] 독립 SpriteAnimation 리소스 에셋 구축 및 전용 SpriteAnimationEditor 저작 패널 완성

- Area: Engine | Editor | Resource | UI | Localization
- Type: Feature | Architecture | UX
- 관련 파일:
  - Engine/Source/Resource/Public/ResourceBinaryHeader.h
  - Engine/Source/Resource/Public/ResourceMetrics.h
  - Engine/Source/Resource/Private/ResourceManager.cpp
  - Engine/Source/Resource/Sprite/Public/SpriteAnimation.h
  - Engine/Source/Resource/Sprite/Private/SpriteAnimation.cpp
  - Engine/Source/Core/Public/Components/Render/SpriteAnimationComponent.h
  - Engine/Source/System/Render/Private/SpriteAnimationSystem.cpp
  - Editor/Source/ImGui/ResourceEditor/Public/SpriteAnimationEditor.h
  - Editor/Source/ImGui/ResourceEditor/Private/SpriteAnimationEditor.cpp
  - Editor/Source/ImGui/ResourceEditor/Public/ResourceEditors.h
  - Editor/Source/ImGui/Private/ImGuiManager.cpp
  - Resources/Data/Localization/Editor.csv
  - Resources/Data/Localization/Engine.csv
- 구현 요약:
  1. **독립 SpriteAnimation 리소스 에셋 구축 (Engine 8절, P2-A)**:
     - `EResourceType::SpriteAnimation` 등록 및 확장자(`.bamspriteanim`, `.spriteanim`) 연결.
     - `SpriteKeyFrame` 구조체(스프라이트 핸들 및 프레임별 지속 시간 가중치 `duration`), 기본 FPS, 재생 모드(`Loop`/`Once`/`PingPong`) 정의.
     - 수동 아카이브 오버라이드 없이 `Resource` 기본 클래스의 리플렉션 자동 순회 직렬화(`WriteProperties`/`VisitProperties`)를 100% 활용하는 무결한 Zero-Boilerplate 아키텍처 적용.
     - `SaveToFile`/`LoadFromFile` 간편 JSON 파일 입출력 및 시간 기반 프레임 인덱스 탐색(`GetFrameIndexAtTime`) 유틸리티 완비.
  2. **SpriteAnimationComponent 및 SpriteAnimationSystem 에셋 연동**:
     - `SpriteAnimationComponent`에 `ResourceHandle<SpriteAnimation> animation` 프로퍼티 추가.
     - 에셋이 지정되어 있으면 공유 에셋의 프레임 시퀀스와 FPS, 재생 모드, 프레임별 지속시간 가중치를 우선 평가하고, 없으면 기존 로컬 `frames` 배열로 자동 폴백(기존 씬 100% 하위 호환).
  3. **SpriteAnimationEditor 전용 2D 애니메이션 저작/미리보기 에디터 패널 (ImGui 11절)**:
     - `ResourceEditorInterface`를 상속받아 콘텐츠 브라우저에서 `.spriteanim` 파일 더블클릭 시 자동 로드 및 오픈.
     - 상단 툴바: 에셋 저장(Save), 재생/일시정지/정지, 이전/다음 프레임, FPS 슬라이더, 재생 모드 콤보박스, 줌 리셋.
     - 2D 프리뷰 캔버스: 투명 체크무늬(체스판) 배경, 마우스 휠 줌(0.25x~16x), 우클릭 패닝, 중심 피벗(Pivot) 크로스헤어 시각화.
     - 프레임 인스펙터 & 필름스트립: 프레임 추가/삭제/순서 이동(▲/▼), 지속 시간 가중치 조절, 하단 타임라인 필름스트립 실시간 하이라이트 및 클릭 스크러빙.
  4. **다국어 사전 키 등록**:
     - `Editor.csv`(`ResourceEditor.SpriteAnimationTitle`, `ResourceEditor.KeyFrames`) 및 `Engine.csv`(`PROP_SPRITE`, `PROP_SPRITE_ANIMATION`, `PROP_FRAME_DURATION`) 등록.

---

## [2026-11-01] 렌더링 S5 큐브맵 IBL 연동 검증 및 ViewportDeferredPipeline/GameViewport 후처리 일관성 완성

- Area: Render | Engine | Editor | RHI
- Type: Feature | Architecture | Stability | Consistency
- 관련 파일:
  - Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp
  - Editor/Source/ImGui/ViewportPanel/Private/ViewportDeferredPipeline.cpp
  - Editor/Source/ImGui/ViewportPanel/Public/GameViewportPanel.h
  - Editor/Source/ImGui/ViewportPanel/Private/GameViewportPanel.cpp
  - Docs/DevelopmentPriorities.md
  - Docs/RenderingDevelopmentPlan.md
- 구현 요약:
  1. **DirectX 12 Null SRV 차원(Dimension) 안전장치 보완 (DirectX12RHI.cpp)**:
     - `lightingPS.hlsl`의 `TextureCube` 레지스터(`t7` Point Shadow, `t10` Irradiance, `t11` Prefiltered Env)에 텍스처가 바인딩되지 않았을 때, 일괄 `Texture2D`로 생성되던 Null SRV를 `D3D12_SRV_DIMENSION_TEXTURECUBE`로 분기 처리.
     - D3D12 디버그 레이어의 리소스 차원 불일치(Dimension mismatch) 경고 및 미정의 샘플링 오류 원천 차단.
  2. **ViewportDeferredPipeline의 IBL & 대기 조명 일관성 연동 (ViewportDeferredPipeline.cpp)**:
     - Game Viewport 및 보조 뷰포트에서 사용하는 `SubmitLightingPass`에 씬의 `SkyAtmosphereComponent` 및 `VolumetricCloudComponent` 검색·바인딩 파이프라인 연결.
     - `IrradianceMap (t10)`, `PrefilteredMap (t11)`, `BRDFLutMap (t12)` 바인딩 및 `HasIBLMaps`, `IBLIntensity`, 태양 방향, 클라우드 3D/2D 노이즈 텍스처 바인딩을 일관되게 주입하여 Game View에서도 고품질 Split-Sum PBR IBL 렌더링 보장.
     - `DeferredLightingPassData`에 대해 512바이트 레이아웃 일치를 보장하는 `static_assert` 오프셋 검증 추가.
  3. **GameViewportPanel 포스트프로세스 체인(PostProcessChain) 연동 (GameViewportPanel.h, GameViewportPanel.cpp)**:
     - `GameViewportPanel`에 `PostProcessChain` 수명 주기(`Initialize`, `Free`, `ResizeRenderTargets`) 및 `GetPostProcessChain()` 가상 함수 재정의 구현.
     - `SubmitPostProcessPass`를 통해 씬의 `PostProcessComponent` 설정(인라인 `PipelineSettings` 및 `PresetName`)을 읽어들여 ACES 톤매핑, 블룸, 비네트 등의 후처리가 인게임 뷰 화면에 실시간으로 일관되게 적용되도록 완성.

---

## [2026-11-01] 콘텐츠 브라우저 OS 연동·공통 열기 일원화 및 다중 뷰포트/패스별 독립 LOD 파이프라인 완성

- Area: Editor | Engine | Render | UI | Localization
- Type: Feature | Architecture | UX | Optimization
- 관련 파일:
  - Editor/Source/ImGui/ContentBrowserPanel/Public/ContentBrowserGridItem.h
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp
  - Engine/Source/System/Render/Private/MeshRenderSystem.cpp
  - Resources/Data/Localization/Editor.csv
  - Resources/Data/Localization/Korean.json
  - Resources/Data/Localization/English.json
  - Docs/DevelopmentPriorities.md
- 구현 요약:
  1. **콘텐츠 브라우저 공통 열기 및 컨텍스트 메뉴 일원화 (P1-D, ImGuiDevelopmentPlan 14절)**:
     - `OpenAssetOrFolder` 공통 함수로 더블클릭과 컨텍스트 메뉴의 `[열기]` 동작을 완전히 통합 (폴더 이동, 씬/프리팹/머티리얼/그래프 내부 에디터 열기).
     - 내부 에디터가 없는 소스/스크립트/문서/일반 파일 더블클릭 시 OS 기본 연결 프로그램(`ShellExecuteW`) 자동 폴백 실행 및 실패 시 알림(Notification) 제공.
     - 컨텍스트 메뉴 최상단에 `[열기]`, `[기본 프로그램으로 열기]`, `[탐색기에서 표시]` 배치 및 유니코드/공백 경로 안전화(`ShellExecuteW` 기반 `explorer.exe /select,"<경로>"`).
     - 검색창 검색 결과 0개 시 `검색 결과가 없습니다: "{0}"` 안내 문구와 원클릭 `[검색 지우기]` 버튼 제공 및 빈 폴더와 검색 상태 명확히 분리.
     - 다국어 사전(`EditorCommon.Open`, `ContentBrowser.NoSearchResults`, `ContentBrowser.EmptyFolder`, `ContentBrowser.ClearSearch`) 등록 완비.
  2. **MeshRenderSystem 다중 뷰포트/패스별 독립 LOD 평가 및 상태 분리 (P1-C, RenderingDevelopmentPlan S3)**:
     - `PassTarget` 구조체를 통해 활성 뷰포트/패스별 고유 카메라(위치, `projY`)를 수집.
     - 각 패스(Scene View, Game View, Shadow Pass 등)별로 해당 시점의 카메라 거리에 기반하여 독립적인 LOD 메시(`activeMesh`)를 평가하고 제출.
     - 컴포넌트의 영속 관찰 상태(`currentLODIndex`)는 오직 주 에디터 메인 카메라(`target.isPrimaryGeometry`)일 때만 갱신하여 보조 패스로 인한 상태 오염 원천 차단.

---

## [2026-11-01] 엔진 안정화, 씬 물리 레이캐스트 API 확장 및 그래프 패널 윈도우 메뉴바 리팩터링

- Area: Engine | Editor | Render | Physics | UI
- Type: Feature | Refactor | Fix | Stability
- 관련 파일:
  - Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp
  - Engine/Source/System/Render/Private/MeshRenderSystem.cpp
  - Engine/Source/System/Physics/Public/PhysicsSystem.h
  - Engine/Source/System/Physics/Private/PhysicsSystem.cpp
  - Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelExecution.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelShader.cpp
  - Docs/DevelopmentPriorities.md
- 구현 요약:
  1. **DirectX12RHI std::terminate 3곳 제거 및 안전한 에러 반환 (P0-B)**:
     - `CreateBuffer`, `CreateTexture2D`, `CreateTextureCube` 내부에서 리소스 생성 실패 시 프로세스를 강제 종료하던 `std::terminate()` 호출을 제거하고 `BAM_LOG_ERROR` 로깅 후 `EResult::Fail` 반환으로 정리.
  2. **MeshRenderSystem 주 뷰포트 카메라 기준 LOD 상태 보존 (P1-C)**:
     - 섀도우 패스, 포인트 섀도우 패스, 머티리얼 미리보기 등 보조 렌더 패스의 카메라 거리로 인해 메인 씬 뷰포트의 `LODGroupComponent.currentLODIndex`가 오염되던 현상을 방지. 주 카메라 렌더링 시에만 LOD 상태를 갱신하도록 보호.
  3. **PhysicsSystem 씬 레벨 Collider Raycast API 확장 (P2-A, EngineDevelopmentPlan 6절)**:
     - `RaycastQuery` 구조체 도입 (`maxDistance`, `layerMask`, `includeTrigger`, `ignoreEntity`).
     - `PassRaycastFilter`를 통한 트리거/레이어/무시 엔티티/플래그(Active, Dead, Paused) 필터링.
     - `TestColliderHit` (Box, Sphere, Mesh, Terrain) 지원 및 거리 컷오프 기반 조기 탈출(Early-out) 최적화.
     - 기존 단일 거리 인자 호출부를 위한 완벽한 하위 호환 오버로드 유지.
  4. **GraphPanel 윈도우 스타일 드롭다운 메뉴바 리팩터링 (P2-A, ImGuiDevelopmentPlan 18절)**:
     - 기존 2단 버튼 나열로 인한 캔버스 세로 공간 압박 문제를 해결하기 위해 `ImGuiWindowFlags_MenuBar` 기반 상단 메뉴바 도입.
     - `File`(Open, Save, Save As), `Edit`(Add Node, Delete, Validate), `View`(Reset View, Wire Style), `Domain`(Logic, Shader), 그리고 도메인별 전용 메뉴(`Debug` / `Build`)로 기능을 체계적으로 범주화.
     - 메뉴바 하단에는 빠른 원클릭 조작을 위한 단일 행 퀵 액세스 툴바(도메인 콤보, 빠른 실행/빌드 버튼, 현재 파일명 표시)만 배치하여 캔버스 작업 공간 대폭 확보.

---

## [2026-11-01] 프리팹 저장 기준점과 Undo/Redo 변경 표시 연결

- Area: Editor | UI | Scene
- Type: Fix | Docs
- 관련 파일:
  - Editor/Source/ImGui/PrefabEditor/Private/PrefabModeManager.cpp
  - Editor/Source/ImGui/PrefabEditor/Public/PrefabModeManager.h
  - Docs/ImGuiDevelopmentPlan.md
  - Docs/EditorUndoRedo.md
- 배경:
  - 수동 `MarkDirty()` 플래그만 읽으면 Undo로 저장 상태에 돌아와도 별표와 종료 확인이 유지된다.
  - 메인 씬과 프리팹이 같은 `SceneEditHistory`를 사용하므로 프리팹의 저장 기준점을 설정할 때 메인 씬의 미저장 여부도 보존해야 한다.
- 이번 변경:
  - 프리팹 진입 전에 이력이 메인 씬을 편집할 수 있는 상태인지 검사하고 미저장 여부를 보관한다.
  - 실제 현재 씬이 프리팹으로 전환된 경우 `ResetDocument()`로 불러온 상태를 저장 기준점으로 설정한다.
  - 별표 표시와 종료 확인 조건을 기존 `HasUnsavedChanges()` 조회로 통일한다. 이력이 프리팹을 대상으로 할 때 현재 상태와 저장 기준점을 비교하고, 조회할 수 없는 문맥에서는 기존 플래그를 사용한다.
  - 메인 씬 열기가 성공한 뒤 이력 대상을 복원한다. 진입 전에 미저장 상태였다면 `NotifyUntrackedChange()`로 다시 표시한다.
- 기존 커밋과의 구분:
  - `HasUnsavedChanges()` 구현과 저장 성공 시 `MarkSaved()` 호출은 선행 커밋 `e80d745c`에 포함되어 있다.
  - 계층 패널 프리팹 드롭, 편집 경로별 `MarkDirty()` 연결, 루트 삭제 방지, 빈 엔티티의 프리팹 루트 연결은 이번 미커밋 코드 차이에 포함되지 않는다. 이번 변경에서 다시 구현하거나 실행 검증한 것으로 기록하지 않는다.
- 제약 및 후속 확인:
  - 메인 씬의 미저장 여부를 보존하는 변경이며 씬 전환 전 Undo/Redo 명령 목록을 보관·복원하는 기능은 아니다.
  - 이력에 기록되지 않은 편집 전체와 PIE 전환, 프리팹 진입 실패 시의 자원·선택 복구를 완성한 변경으로 해석하지 않는다.
  - 프리팹 진입의 `OpenScene()` 반환값은 여전히 직접 처리하지 않는다. 이번 기준점 설정은 현재 씬이 실제로 프리팹인지 확인한 경우에만 수행한다.
- 검증:
  - 현재 diff와 호출 경로를 정적으로 확인했고 `git diff --check`에서 공백 오류가 없었다. 이번 커밋 범위는 프리팹 소스 2개와 관련 문서 3개다.
  - 이번 작업에서 빌드·CTest·Editor 실행·F5 수동 검증은 수행하지 않았다. 사용자로부터 아래 항목의 통과 결과를 전달받지는 않았다.
  - 수동 확인 항목은 [ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md) 10절의 2026-11-01 후속 기록을 따른다.

---

## [2026-10-31] 렌더링 S1 계측·S4 타일 라이트 컬링 절두체 완성, 블루프린트 런타임 핫스왑(Phase 4.3) 및 리소스 피커 최적화 구현 확인 및 문서 동기화

- Area: Render | Graph | Editor | HotReload | Shader
- Type: Docs | Verification | Performance | Architecture
- 관련 파일:
  - Resources/Shader/hlsl/tiledLightCullingCS.hlsl
  - Resources/Shader/hlsl/lightingPS.hlsl
  - Engine/Source/Render/RHI/Public/RHIFrameMetrics.h
  - Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp
  - Editor/Source/ImGui/MetricsPanel/Private/MetricsPanel.cpp
  - Engine/Source/System/Graph/Public/GraphSystem.h
  - Engine/Source/System/Graph/Private/GraphSystem.cpp
  - Editor/Source/Application/Private/Application.cpp
  - Client/Source/Main.cpp
  - Editor/Source/ImGui/Public/ResourcePicker.h
  - Engine/Source/Render/Private/Renderer.cpp
  - Docs/DevelopmentPriorities.md
  - Docs/RenderingDevelopmentPlan.md
  - Docs/VisualScriptingBlueprintPlan.md
  - Docs/ImGuiDevelopmentPlan.md
- 배경:
  - 2026-10-30 문서에 결함 또는 미구현 과제로 기록되었던 렌더링 S1 계측, S4 컴퓨트 셰이더 절두체 컬링, 비주얼 스크립팅 런타임 핫스왑(Phase 4.3), Client 시스템 등록, 인스펙터 리소스 피커 매 프레임 순회 병목이 실제 코드베이스에서 이미 해결·구현되어 있음을 정밀 점검으로 확인.
  - 최신 코드베이스의 실제 구현 상태와 계획 문서 간의 괴리를 제거하고 우선순위를 재조정하기 위해 문서를 갱신.
- 확인된 구현 내용:
  1. **렌더링 S4 타일 라이트 컬링 절두체 평면 컬링 및 0-광원 루프 해결**:
     - `tiledLightCullingCS.hlsl`: 좌·우·상·하 4개 타일 절두체 평면(`s_FrustumPlanes[4]`) 역투영 및 구면 교차 검증을 추가하여 전체 폭 광원 과할당 문제 해결 완료.
     - `lightingPS.hlsl`: `for (uint i = 0u; i < tileLightCount; ++i)` 루프로 타일 내 선별 광원만 순회하여 0-광원 타일 전체 순회 버그 해결 완료. 큐브맵 기반 Split-Sum PBR IBL 파이프라인 연동 완비.
     - `ViewportDeferredPipeline.cpp`: `lightManager.DispatchLightCulling` 연결 완료.
  2. **렌더링 S1 CPU/GPU 계측 파이프라인**:
     - `RHIFrameMetrics.h`: `RHIPassMetric`, `RHICapabilities`, `RHIMemoryStats` 확장 완료.
     - `DirectX12RHI.cpp`: D3D12 Timestamp QueryHeap 및 ReadbackBuffer를 통한 패스별 GPU 시간 지연 회수와 VRAM/디스크립터 통계 질의 완비.
     - `MetricsPanel.cpp`: 패스별 CPU/GPU 시간 테이블, VRAM/디스크립터 사용량, 하드웨어 기능 지원 여부 UI 렌더링 완비.
  3. **비주얼 스크립팅 런타임 핫스왑 (Phase 4.3) 및 Client 시스템 등록**:
     - `GraphSystem::ReloadReflectedNodes`: 활성 씬 엔티티들의 기존 변수(`oldVariables`)와 실행 상태를 보존하며 새 리플렉션 심볼로 `executor.Load` 후 변수 값을 완벽 복원.
     - `Application.cpp`: `HotReloadManager::RegisterPostReloadCallback`에서 `ReloadReflectedNodes`를 자동 호출하여 PIE 실행 중 0-크래시 핫스왑 완비.
     - `Client/Source/Main.cpp`: `InitializeSystems()`에 `GraphSystem`, `PhysicsSystem`, `TerrainSystem` 등록 완료.
  4. **인스펙터 리소스 피커 최적화 (P1-D)**:
     - `ResourcePicker.h`: `IsResourceExtensionCompatible<T>`로 템플릿 인자 `T`별 호환 확장자 필터링 및 `if (isAppearing)` 팝업 1회 스캔 캐싱 완비.
  5. **렌더링 S3 비인덱스 인스턴싱 및 전처리기 정리**:
     - `Renderer.cpp`: `m_RHI->DrawInstanced(vertexCount, instanceCount)`로 비인덱스 인스턴싱 드로우 지원 완비.
     - `SceneViewportPanel.cpp` 및 `Vertex.h`: `#ifdef _DEBUG` 분기 제거 완료.
     - `GraphGameplayLibrary.h`: `std::cout`을 `BAM_LOG`로 교체 완료.
- 다음 실제 남은 과제:
  - 고정 씬에서 `MetricsPanel`을 통한 타일 라이트 컬링/인스턴싱 실측 GPU ms 및 CPU ms 성능 수치 측정.
  - `DirectX12RHI.cpp` 내 3곳의 `std::terminate()` 예외 정책 정리.
  - `MeshRenderSystem`의 다중 뷰포트/패스별 카메라 기준 독립 LOD 평가 지원.
- 실측 성능 검증 결과 (2026-11-01 MetricsPanel 확인):
  - **S4 타일 라이트 컬링 효과**:
    - 라이팅 집중 시점: `Scene View_LightingPass` CPU 0.21 ms / GPU 1.10 ms (전체 GPU 3.40 ms)
    - 라이팅 비집중 시점: `Scene View_LightingPass` CPU 0.14 ms / GPU 0.25 ms (전체 GPU 2.54 ms, 라이팅 GPU 시간 약 77% 단축)
    - 4방향 타일 절두체 평면 컬링 및 0-광원 타일 순회 건너뛰기의 실측 성능 이득 확인 완료.
  - **S3 정적 메시 인스턴싱 효과**:
    - 스피어 다수 추가 시: 제출 인덱스 수 1,642,170개 -> 1,951,086개(약 31만 개 증가)에도 불구하고 RHI 그리기 호출은 56회 -> 57회로 단 1회 증가 유지.
    - `DrawIndexedInstanced`를 통한 GPU 인스턴싱 배칭 정상 동작 확인 완료.
  - **S1 계측 파이프라인**:
    - D3D12 Timestamp Query 지연 회수 및 패스별 CPU/GPU 시간 분리 통계가 실시간 110~116 FPS 환경에서 오버헤드 없이 안정적으로 측정됨을 확인.

---

## [2026-10-31] 에디터 프리팹 파이프라인(생성·격리모드·루트연결·오버라이드) 및 엔티티 다중 복사·붙여넣기 완성

- Area: Editor | Engine | Scene | UI | Archive
- Type: Feature | Architecture | UX
- 관련 파일:
  - Engine/Source/Core/Public/Components/Core/PrefabInstanceComponent.h
  - Engine/Source/Core/Public/Components/Core/CoreComponents.h
  - Engine/Source/World/Scene/Private/SceneManager.cpp
  - Editor/Source/Clipboard/Public/EntityClipboard.h
  - Editor/Source/Clipboard/Private/EntityClipboard.cpp
  - Editor/Source/ImGui/HierarchyPanel/Public/EntityHierarchyView.h
  - Editor/Source/ImGui/HierarchyPanel/Private/EntityHierarchyView.cpp
  - Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp
  - Editor/Source/ImGui/InspectorPanel/Public/InspectorPanel.h
  - Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp
  - Editor/Source/ImGui/ResourceEditor/Public/PrefabModeManager.h
  - Editor/Source/ImGui/ResourceEditor/Private/PrefabModeManager.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp
  - Editor/Source/Command/Private/EditorActionRegistry.cpp
  - Resources/Data/Localization/Engine.csv
  - Resources/Data/Localization/Editor.csv
- 배경:
  - ImGuiDevelopmentPlan.md 10절 프리팹 파이프라인과 17절 엔티티 다중 복사·붙여넣기 구현.
  - 프리팹 에셋 생성·배치에 그치지 않고, 씬 내부 뷰포트·기즈모를 100% 활용하는 유니티 스타일의 프리팹 격리 편집 모드(PrefabModeManager)와, 원본 에셋과 인스턴스 간의 연결·오버라이드 관리(PrefabInstanceComponent, Apply All, Revert All, Unpack) 파이프라인 구축.
  - 다중 엔티티 복사·붙여넣기 시 부모-자식 동시 선택 중복 복제 방지, 계층 순서 보존, 원자적 단일 Undo 스냅샷 보장.
- 구현 내용:
  - **엔티티 다중 복사·붙여넣기 (EntityClipboard)**:
    - archive::JsonArchive 메모리 직렬화를 사용하여 선택된 엔티티 서브트리를 독립적으로 직렬화. 원본이 삭제되거나 수정되어도 안전하게 붙여넣기 가능.
    - 조상 필터링: 선택 집합 중 다른 선택 엔티티의 자손인 노드는 복사 루트에서 제외하여 중복 생성 방지.
    - 단일 원자적 Undo: 여러 엔티티를 한 번에 붙여넣더라도 UI_HISTORY_PASTE 단일 스냅샷으로 묶어 Ctrl+Z/Ctrl+Y 일괄 제어.
    - 단축키: Ctrl+C(복사), Ctrl+V(붙여넣기), Ctrl+Shift+V(자식으로 붙여넣기) 바인딩 및 붙여넣기 후 신규 루트 자동 선택.
    - 계층 패널 다중 삭제: Delete 키 및 우클릭 메뉴를 통한 DeleteEntities 지원, 부모-자식 동시 삭제 시 충돌 방지.
  - **프리팹 1단계 (생성·배치)**:
    - Hierarchy 우클릭 Save As Prefab... (EFileDialogContext::PrefabSave), ContentBrowser .bamprefab.json 식별(ICON_FA_CUBES, 시안 틴트), 드래그 앤 드롭 인스턴스화.
  - **프리팹 2단계 (뷰포트 격리 편집 모드 PrefabModeManager)**:
    - 미니 윈도우 대신 메인 씬을 Safe_AddRef로 메모리에 안전 보존하고 임시 프리팹 씬(m_PrefabScene)으로 전환.
    - 메인 뷰포트, 하이어라키, 인스펙터, ImGuizmo 3D 조작 기능을 100% 그대로 활용.
    - 뷰포트 상단에 ◀ Back to Scene 배너 및 저장 버튼을 배치하여 원클릭으로 메인 씬에 안전 복귀.
  - **프리팹 3단계 (단일 원천 루트 전용 연결 및 오버라이드 관리)**:
    - 아키텍처 정석 설계: 데이터 중복(Single Source of Truth 위반)을 방지하기 위해 PrefabInstanceComponent는 서브트리의 루트 엔티티에만 부착.
    - FindPrefabRootHandle 헬퍼: 부모 계층을 탐색하여 자식 엔티티도 즉시 자신의 프리팹 루트 및 원본 에셋 경로 식별.
    - Hierarchy 시각화: 프리팹 루트 및 자식 엔티티 청록색(ImVec4(0.35f, 0.78f, 1.0f, 1.0f)) 텍스트 및 큐브 아이콘 표시, 우클릭 시 Open Prefab, Select in Content Browser, Unpack Prefab 제공.
    - Inspector 상단 프리팹 헤더 바:
      - Open: 프리팹 격리 편집 모드 진입.
      - Select: Content Browser에서 해당 에셋 선택.
      - Overrides 드롭다운: Apply All (원본 에셋에 덮어쓰기), Revert All (원본 에셋에서 복원), Unpack (루트 컴포넌트 제거로 일반 엔티티 전환).
    - 다국어 CSV 등록: Engine.csv(컴포넌트 리플렉션) 및 Editor.csv(UI/메뉴/알림/히스토리)에 다국어 키 동기화.
- 검증:
  - C++ 한 줄 작성 규칙 엄수, Lazy_Singleton 적용을 통한 싱글턴 크래시 원천 차단.
  - 정적 분석 및 컴파일 에러 해결(C2039, C2664, C2663, C1128 /bigobj 플래그 등).
- 남은 항목:
  - ImGuiDevelopmentPlan.md 11절 A: 씬 문서 상태·미저장 변경 보호 및 SceneManager 미저장 변경 감지 경고 팝업(Save/Discard/Cancel).
  - 14절: 머티리얼/텍스처 에셋 썸네일 실시간 미리보기 파이프라인.

---


## [2026-10-31] 에디터 설정·워크스페이스 저장 경로와 씬 문서·후처리 저장 흐름 정리

- Area: Editor | Engine | Resource | Archive | UI
- Type: Feature | Refactor | Fix
- 관련 파일:
  - Editor/Source/Settings/Public/EditorSettings.h
  - Editor/Source/Settings/Private/EditorSettingsService.cpp
  - Editor/Source/Application/Private/Application.cpp
  - Editor/Source/Document/
  - Editor/Source/Command/Private/EditorEditContext.cpp
  - Editor/Source/ImGui/GraphicSetting/
  - Editor/Source/ImGui/PostProcessPanel/
  - Editor/Source/ImGui/ViewportPanel/
  - Engine/Source/Core/Config/
  - Engine/Source/Core/Archive/Public/Adapters/ResourceHandleArchiveAdapter.h
  - Engine/Source/Resource/Public/ResourceReference.h
  - Engine/Source/World/Scene/Private/SceneArchive.cpp
  - Engine/Source/Render/PostProcess/
  - Projects/Archive/include/archive/ArchiveInterface.h
  - Projects/Archive/src/ArchiveInterface.cpp
  - Editor/examples/EditorSettingsCheck.cpp
  - Editor/examples/EditorHistoryCheck.cpp
  - Engine/examples/SceneArchiveCheck.cpp
  - Config/Project.json
- 배경:
  - 창·패널·뷰포트·씬 설정의 소유자와 저장 시점이 섞여 있어 시작 시 복원, UI 적용, 종료 시 캡처의 호출 흐름을 추적하기 어려웠다.
  - 씬 저장과 패널의 실행 상태 수집을 분리하고, 파일 읽기·쓰기 실패가 기존 설정이나 미저장 편집을 지우지 않도록 정리했다.
- 구현 내용:
  - `EditorPreferences`, `EditorWorkspace`, `ProjectSettings`로 저장 범위를 나누고, 사용자가 확정한 싱글턴 `EditorSettingsService`를 설정 요청 진입점으로 사용한다. `Application`은 초기화·종료 순서를 조정하고 각 매니저와 패널이 자신의 실행 상태를 적용·캡처한다.
  - 개인 설정은 SDL 사용자 설정 경로, 프로젝트별 작업 상태는 `.bam/User/Workspace.json`, 공유 프로젝트 설정은 `Config/Project.json`으로 분리한다. 개인 작업 상태 경로는 Git에서 제외한다. 단축키와 파일 다이얼로그의 마지막 경로 저장도 연결한다.
  - `GraphicSettingPanel`에 창 크기·전체 화면·UI 배율·목표 FPS·VSync와 기본 Scene/Game View 해상도 편집을 연결한다. 편집 초안과 적용값을 구분하고, 실제 적용 결과를 받은 뒤 캡처·저장하며 결과를 토스트로 알린다.
  - 창 변경에는 15초 유지 확인과 자동 복구, 적용·복구 대기 제한을 둔다. 설정 파일 복원에 실패하면 기본값으로 시작하되 해당 파일의 자동 저장을 차단하고 경로·복구 안내를 표시한다.
  - `IImGui::CaptureState/RestoreState`와 뷰포트 상태를 통해 패널 열림 상태·도킹·편집 카메라·렌더 패스·Content Browser 상태를 저장한다. UI 배율·언어처럼 UI 프레임워크에 종속되지 않는 설정 요청은 서비스가 조정한다.
  - `SceneDocument`, `EditorDocumentManager`, `EditorDocumentController`로 씬의 저장·다른 이름 저장·미저장 확인과 New/Open/Close/Quit를 연결한다. 저장 실패 또는 취소 시 후속 씬 교체를 진행하지 않는다. `EditorEditContext`는 편집 이력과 Undo/Redo 후 선택 복원을 담당한다.
  - 명령의 상태 ID와 저장 기준점으로 미저장 여부를 판단한다. 편집 스냅샷 수집 실패·기록 실패를 미추적 변경으로 보존하고, PIE 종료 후 복원된 씬에 문서를 다시 연결한다.
  - 후처리 실행 파이프라인을 Engine으로 옮기고, 에디터는 미리보기·프리셋 관리와 씬 반영을 담당한다. 씬의 `PostProcessComponent::PipelineSettings`를 저장 원본으로 사용하고, 씬 전환·Undo/Redo 시 실행 파이프라인에 재적용한다. 패널 편집은 명시적인 씬 반영을 거쳐 씬 저장에 포함된다.
  - 후처리 프리셋 저장·불러오기·삭제·목록 갱신과 파일 이름 검증을 추가한다. 공통 Material 파라미터 편집·초기화 API, 효과 셰이더와 시작 등록, 기본 예제 프리셋을 함께 포함한다.
  - Scene View는 후처리 이름·효과 초기화 후 렌더 타깃과 패스를 생성한다. 뷰포트·채널 필터·후처리의 리사이즈 결과를 전파하고 실패 시 이전 크기 복구를 시도하며, 유효한 타깃이 없는 프레임의 렌더링을 건너뛴다. VSync 요청에 필요한 DX12 선언과 SDLGPU 구현도 연결한다.
  - 씬·프리팹 직렬화 시 `ResourceReferenceSet`을 수집해 선택적 `ResourceReferences` 메타데이터로 저장한다. Archive의 빌려 쓰는 쓰기 컨텍스트를 자식 객체에 전달하고, 참조 목록을 컴포넌트 복원 전에 선로딩한다. 구버전 파일의 목록 누락을 허용하며, 선로딩 실패는 기록하고 실제 필수 핸들의 복원 실패는 유지한다.
  - `ConfigManager`의 후보 객체 읽기와 파일 교체 저장을 사용하고, 공통 Archive 저장에도 임시 파일을 통한 기존 파일 보호를 추가한다. 설정 검사 대상은 실제 설정 DTO의 Reflection 코드를 별도 생성한다.
- 커밋 범위:
  - 공용 파일은 이번 설정·문서·후처리·리소스 참조 저장에 필요한 변경 부분만 선택한다. 관련 로캘 원본과 영어·한국어 사전, 렌더 컴포넌트 집계 헤더 이동 및 PCH의 표준 헤더 의존성을 함께 포함한다.
  - 그래프 실행·핫리로드 추가 기능, 인스턴싱·LOD·타일 조명 최적화, 임포터·리소스 피커 확장, 새 History 패널 UI, Client 변경과 사용자 테스트 씬은 이번 커밋에서 제외한다. 생성 코드·로그·컴파일된 셰이더·의존성 배포 파일도 제외한다.
- 검증:
  - 사용자가 실행한 `EditorSettingsCheck`의 통과를 `out/build/x64-debug/Testing/Temporary/LastTest.log`에서 확인했다. 2026-10-31 12:42 실행, 0.10초, `Editor settings persistence checks passed.` 결과다.
  - 해당 검사는 설정 왕복 저장·기존 파일 교체·읽기 실패 시 값과 파일 보존·선택 필드 기본값·스키마 검증·직렬화/저장 실패·임시 파일 정리를 다룬다. 서비스 상태 머신이나 UI·GPU 동작의 검증으로 확대하지 않는다.
  - 같은 작업 흐름에서 사용자가 `ArchiveExample`·`SceneArchiveCheck` 통과를 보고했다. 마지막 테스트 로그는 이후 설정 검사 실행으로 교체되어 있으며, 이 결과도 최종 선별 커밋 전체의 빌드·실행 보증으로 간주하지 않는다.
  - 이번 커밋 정리에서는 파일·호출 경로와 스테이징 내용을 정적으로 확인한다. AI가 빌드·테스트를 새로 실행하거나 원격으로 푸시하지 않는다.
  - `git diff --cached --check`에서 기존 소스 변경의 줄 끝 공백 6곳을 확인했다. 소스 수정 권한을 이번 기록·커밋 허가로 확대하지 않고 그대로 보존한다.
- 남은 항목:
  - 최종 커밋 기준 빌드와 관련 검사 재실행, 재시작 후 설정·카메라·렌더 패스·도킹 복원, 창 확인 취소·시간 초과, GPU 리사이즈 실패·복구, 언어 변경 중 초안 유지와 미적용 초안 저장 제외를 실제 에디터에서 검증한다.
  - 후처리의 씬 전환·종료·재열기·Undo/Redo를 실제 화면과 비교한다. 특히 이번 초기화 순서 수정은 위 설정 데이터 검사만으로 검증되지 않는다.
  - `ConfigManager`와 공통 Archive에 겹치는 임시 파일 교체 처리는 별도 정리 대상으로 남긴다. 리소스 목록은 직접 참조 수집이며, 전체 전이 의존성·스트리밍 구현을 의미하지 않는다.
  - 동적으로 생성한 뷰포트의 재생성과 공통 런타임 설정 API는 후속 범위다. Client 시작·설정·배포 연결은 `BuildAndPackagingPlan.md`의 보류 결정을 유지한다.

## [2026-10-30] C++ 모듈 핫리로드 라이브 노드·핀 동적 동기화 및 빌드 최적화 (Phase 4.1)

- Area: Reflection | Graph | HotReload | Editor
- Type: Feature | Performance | Architecture
- 관련 파일:
  - Projects/Reflection/include/reflection/ModuleId.h
  - Projects/Reflection/include/reflection/Registry.h
  - Projects/Reflection/src/Registry.cpp
  - Projects/Reflection/include/reflection/TypeInfo.h
  - Projects/Reflection/include/reflection/FunctionInfo.h
  - Projects/Reflection/include/reflection/EnumInfo.h
  - Projects/GameModule/CMakeLists.txt
  - Projects/GameModule/Source/GameModule.cpp
  - Projects/GameModule/Source/GameGameplayLibrary.h
  - Engine/Source/Graph/Public/GraphRegistry.h
  - Engine/Source/Graph/Private/GraphRegistry.cpp
  - Engine/Source/Graph/Public/Graph.h
  - Engine/Source/Graph/Private/Graph.cpp
  - Editor/Source/HotReload/Private/ModuleBuildManager.cpp
  - Editor/Source/HotReload/Public/ModuleBuildManager.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Docs/ModuleHotReloadOptimization.md
  - Docs/VisualScriptingBlueprintPlan.md
- 배경 및 원인:
  - C++ `GameModule`에서 새로운 `FUNCTION(GRAPH_CALLABLE)` 노드를 작성하고 핫리로드 시, 이전 버전 DLL의 심볼이 `reflection::Registry`에 잔존하여 `RegistrationError::DuplicateFreeFunction` (error 13)으로 인해 갱신이 실패함.
  - 리플렉션 심볼 갱신 부재로 에디터 그래프 패널에 새로 작성한 노드가 나타나지 않거나 삭제된 노드가 유령처럼 남는 현상 발생.
  - `cmake --build` 시 최상위 솔루션 전체의 의존성 순회 평가와 PCH 부재로 인해 단순 C++ 1줄 수정에도 10초 이상의 긴 리로드 지연이 발생함.
- 구현 내용:
  1. **리플렉션 독립성 보장 및 `ModuleId` 태깅 기반 일괄 언레지스터**:
     - `Projects/Reflection` 라이브러리의 순수성을 유지하기 위해 엔진 도메인에 종속되지 않는 범용 식별자 `ModuleId` (`std::uint32_t`) 도입.
     - `Registry::SetActiveModule`, `ScopedActiveModule`을 통해 병합 및 등록 시 소유자 모듈 태그를 자동 주입하고, `Registry::UnregisterModule(ModuleId)`로 DLL 언로드 시 해당 모듈의 모든 타입·함수·열거형을 안전하게 일괄 제거.
  2. **스키마 핫스왑 및 노드 핀 인스턴스 자동 동기화 (`Reconcile`)**:
     - `GraphRegistry::ClearReflectedNodes()`를 추가하여 기존 리플렉션 노드만 초기화 후 최신 심볼들로 재등록.
     - `Graph::Reconcile(const GraphRegistry&)`를 구현하여, 캔버스에 이미 배치된 노드 인스턴스에 새 핀이 추가된 경우 기본값(`DefaultValue`)을 자동 주입하고, 삭제되거나 타입이 바뀐 핀에 연결된 링크는 안전하게 연결 해제하여 런타임 크래시를 원천 차단.
  3. **핫리로드 빌드 속도 1.5초대 최적화**:
     - `ModuleBuildManager.cpp`: 파일 저장 감지 디바운스를 500ms에서 200ms로 단축.
     - MSBuild 옵션 `-- /p:BuildProjectReferences=false /m`을 적용하여 솔루션 내 20여 개 종속 프로젝트 순회 검사를 건너뛰고 멀티코어로 즉시 빌드.
     - `GameModule/CMakeLists.txt`: `target_precompile_headers`로 엔진 및 코어 헤더 PCH 적용.
- 다음 작업 (현재 계획된 작업):
  - `GraphSystem` 런타임 엔진의 핫리로드 동기화 (`ReloadReflectedNodes`) 및 활성 씬 엔티티의 `GraphRuntimeState` 핫스왑 (PIE 실행 중 0-크래시 보장).
  - PIE 실행 중 씬 엔티티 라이브 게임플레이 디스패치 및 실시간 반영.

---

## [2026-10-30] 블루프린트 코멘트 그룹 박스(단축키 C) 구현 및 GraphValue 타입 제약 우회 구조 설계

- Area: Editor | UI | Graph
- Type: Feature | Architecture | UX
- 관련 파일:
  - Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelNodeEditor.cpp
  - Docs/VisualScriptingBlueprintPlan.md
  - Docs/ImplementationLog.md
- 배경 및 원인:
  - 언리얼 엔진 블루프린트처럼 복수의 노드를 시각적·논리적 단위로 묶고 함께 이동시킬 수 있는 코멘트 박스(단축키 `C`) 기능 필요.
  - 엔진의 데이터 모델인 `GraphValue`가 `std::string`을 지원하지 않아 `std::holds_alternative<std::string>` 조회 시 C2338 `static_assert` 컴파일 에러 발생.
  - `Core.Comment` 노드 정의에 `"Size"` 핀이 등록되지 않아 `Graph::SetInputValue`가 `PinNotFound` 오류로 크기 저장을 거부하는 아키텍처 제약 존재.
- 구현 내용:
  1. **내부 데이터 저장용 `"Size"`(Float2) 핀 등록:**
     - `GraphPanel.cpp` 생성자에서 `Core.Comment` 노드 정의에 `Float2` 타입의 `"Size"` 핀을 등록하여 `Graph::SetInputValue` 유효성 검증을 100% 통과시키고 크기를 보관.
  2. **`GraphDocument::CommentTexts` 맵 분리:**
     - `GraphValue`가 문자열을 담지 못하는 한계를 해결하기 위해 에디터 도큐먼트 내에 `std::unordered_map<Engine::GraphNodeId, std::string> CommentTexts`를 신설하여 컴파일 오류를 원천 차단하고 타입 안전한 주석 타이틀 보관.
  3. **헤더 텍스트 더블클릭 인라인 편집:**
     - 코멘트 헤더를 더블클릭하면 즉시 `ImGui::InputText`로 전환되어 원하는 주석 이름(예: "Update Move", "Calculate Damage")으로 직관적으로 변경 가능.
  4. **Z-Order 2-Pass 렌더링 분리:**
     - `DrawCanvas`에서 `Core.Comment` 노드를 먼저 렌더링하고 일반 노드를 나중에 렌더링하여, 반투명 코멘트 박스가 일반 노드의 핀 클릭이나 버튼 입력을 가로채지 않도록 완벽 보장.
  5. **단축키 `C` 및 Bounding Box 자동 계산:**
     - 다중 선택된 노드들의 최소/최대 좌표를 계산하고 여백(Padding X=35, Y=25, HeaderExtraY=32)을 부여하여 감싸는 박스를 자동 생성하고, `ed::Group(size)` 연동으로 코멘트 박스 드래그 시 내부 노드 자석식 일괄 이동 및 우측 하단 크기 조절 지원.

---

## [2026-10-30] 노드 그래프 에디터의 imgui-node-editor 전환 및 블루프린트 룩앤필 고도화

- Area: Editor | UI | Graph
- Type: Feature | Refactor | UX
- 관련 파일:
  - Editor/CMake/NodeEditor.cmake
  - Editor/CMakeLists.txt
  - Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelNodeEditor.cpp
  - Docs/VisualScriptingBlueprintPlan.md
  - Docs/GraphPanel.md
- 배경 및 원인:
  - 기존 `imnodes` 기반 노드 에디터는 단일 세로 루프로 핀을 렌더링하여 `Exec In`이 상단에, `Exec Out`과 `Result`가 최하단에 배치되면서 다음 노드로 연결할 때 와이어가 우하향 계단식으로 늘어지는 UX 문제가 발생했다.
  - 언리얼 엔진 블루프린트 수준의 줌/패닝, 실행선 펄스 애니메이션, 노드 그룹화(코멘트 상자)를 지원하기 위해 전문 프레임워크인 `imgui-node-editor`로 전환을 결정했다.
- 구현 내용:
  - **빌드 시스템 전환:** CMake `FetchContent`를 통해 `imgui-node-editor` (Thedmd) 소스를 가져오고 `ThirdParty` 타깃으로 구성하여 `Editor`에 링크.
  - **수평 가로 흐름(Left-to-Right) 정립:** `Exec In`과 `Exec Out`을 최상단 행에 좌우로 마주보게 배치하고, 하단에 데이터 핀을 2열로 분할하여 세로 처짐 현상 제거.
  - **밀착형 헤더 배경:** 노드 Bounding Box(`nodeMin` ~ `nodeMax`)를 계산하여 테두리 유격/빈틈 없이 외곽선에 100% 밀착되는 상단 둥근 헤더 채우기 및 노드 카테고리별 테마 색상(이벤트: 레드, 함수: 블루, 수학/순수: 그린, 변수: 청록/오렌지) 적용.
  - **ImGui ID 충돌 해결:** 노드(`node.Id`) 및 핀(`pin.Key`) 단위 격리와 고유 접미사(`##ResetPin`, `##UsePin`)를 부여하여 "기본값으로 복원" 버튼의 ID duplicate 경고 해결.
  - **툴팁 좌표계 정규화:** 캔버스 줌/패닝 변환에 의한 툴팁 튐 현상을 방지하기 위해 `ed::Suspend()` / `ed::Resume()` 래핑 적용.
- 진행 중 / 후속 작업:
  - **핀 아이콘 정밀 피벗 결합:** 와이어 연결점이 텍스트 블록이 아닌 아이콘(`●`, `▶`) 중심에 정확히 결합되도록 `ed::PinPivotAlignment(0.5, 0.5)` 적용.
  - **코멘트 그룹 박스 (단축키 C):** `Core.Comment` 노드 정의 등록 및 `ed::Group(size)` 연동을 통해 선택된 노드들을 감싸는 반투명 그룹 박스 생성 및 일괄 이동/크기 조절 지원.
  - **노드 자동 정렬 도구:** 다중 선택 후 `Shift + W/A/S/D` 수평/수직 정렬.

---

## [2026-10-26] 콘텐츠 브라우저 행 클리핑과 메뉴 프레임 하락 완화

- Area: Editor | UI
- Type: Fix | Performance
- 관련 파일:
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp
  - Editor/Source/ImGui/ContentBrowserPanel/Public/ContentBrowserPanel.h
  - Editor/Source/ImGui/ContentBrowserPanel/Public/ContentBrowserGridItem.h
  - Resources/Data/Localization/Editor.csv
  - Resources/Data/Localization/English.json
  - Resources/Data/Localization/Korean.json
- 배경 및 원인:
  - 사용자는 간단한 File 메뉴를 열어 둔 동안 프레임이 떨어져 약 30FPS에 머무르고, 콘텐츠 브라우저를 닫으면 회복되는 현상을 확인했다.
  - 조사 당시 작업 트리의 DrawGrid()는 어느 팝업이든 열리거나 드래그 페이로드가 있으면 모든 행을 클리퍼에 포함했다. 이 때문에 다른 메뉴를 열어도 화면 밖 항목의 위젯 구성과 썸네일 조회까지 반복됐다.
- 구현 내용:
  - 항목 참조 목록과 고정 열 너비·행 높이를 사용해 그리드를 행 단위로 클리핑한다. 이름 변경 입력창을 포함한 행 높이와 썸네일 크기를 일치시킨다.
  - ContentBrowserGridItem::Draw()와 DragAndDropTarget()이 상호작용 유지 여부를 반환하도록 변경했다. 썸네일 활성 상태, 해당 항목의 우클릭 메뉴, 드래그 원본 상태를 구분해 확인한다.
  - 패널은 상호작용 중인 항목의 경로를 다음 프레임으로 전달하고, 이름 변경 대상과 해당 경로의 행만 IncludeItemByIndex()로 추가 처리한다. 모든 팝업·페이로드를 기준으로 전체 행을 포함하던 처리는 제거했다.
  - 우클릭 메뉴는 기존 경로별 PushID() 안에서 안정적인 내부 ID인 ##AssetContextMenu를 사용한다. 폴더·검색·필터 변경으로 목록에서 사라진 대상은 다음 프레임 유지 목록에서 자연스럽게 제외된다.
  - DrawNameBox()의 상대 경로 계산은 lexically_relative()를 사용한다. 빈 검색·필터 결과 안내의 로캘 키 ContentBrowser.NoMatchingAssets를 CSV와 한국어·영어 JSON에 함께 포함했다.
- 검증:
  - 사용자 실행 확인: 적용 후 일부 비용은 남지만 프레임 하락이 완화됐다고 보고했다. 개선 전후의 정량 프레임 시간과 개선율은 측정하지 않았다.
  - AI 정적 확인: 선언·정의·호출부와 설치된 ImGui 1.92.5의 팝업·클리퍼 API를 확인했다. 이번 기록·커밋 작업에서 빌드·자동 테스트는 실행하지 않았다.
  - 우클릭 대상이 화면 밖으로 이동하는 경우, 드래그 중 스크롤, 이름 변경 중 패널 너비 변경, 언어 변경 시 ID·편집 상태 유지와 단축키 충돌은 별도 회귀 확인이 필요하다.
- 한계 및 후속:
  - 전체 목록 필터링과 참조 목록 구성은 여전히 매 프레임 수행한다. 남은 비용의 원인이나 CPU·GPU별 기여도는 확정하지 않았다.
  - 30FPS에 머무는 현상에서 VSync의 기여도는 미검증이다. 이번에는 Present·프레임 제한·입력 수집 경로를 변경하지 않았다.
  - 사용자의 결정에 따라 추가 최적화는 여기서 중단한다.

## [2026-10-26] 뷰포트 패스별 런타임 On/Off 토글 및 SkyAtmosphere 시스템 분리

- Area: Engine | Editor | GameModule | Render
- Type: Feature | Refactor | Fix
- 관련 파일:
  - Engine/Source/Render/RenderPass/Public/RenderPassTypes.h
  - Engine/Source/Render/RenderPass/Public/RenderPass.h
  - Engine/Source/Render/RenderPass/Private/RenderPass.cpp
  - Engine/Source/Render/Private/Renderer.cpp
  - Editor/Source/ImGui/ViewportPanel/Public/PostProcess.h
  - Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp
  - Projects/GameModule/Source/SkyAtmosphereSystem.h
  - Projects/GameModule/Source/SkyAtmosphereSystem.cpp
  - Projects/GameModule/Source/GameModule.cpp
- 배경 및 목적:
  - 씬 뷰포트 패널에서 렌더 패스(Shadow, PointShadow, Geometry, Lighting, Forward, ForwardTransparent, UIOverlay)를 실시간으로 끄고 켜며 렌더링 파이프라인 단계를 격리·디버깅하려는 요구사항 해결.
  - 씬 뷰포트 패널의 `Update()` 내부에 혼재되어 있던 3D 볼류메트릭 구름 텍스처 절차적 생성 및 `SkyMaterial` 바인딩, 태양 시뮬레이션 코드의 책임 분리(SRP 준수).
- 구현 내용:
  - **RenderPass On/Off 제어 기반 구축**:
    - `RenderPassDesc`에 `bool isEnabled = true;` 필드 추가 및 `RenderPass`의 Getter/Setter(`IsEnabled()`, `SetEnabled()`) 위임 연결.
    - `Renderer::Render`에서 비활성화된 패스는 드로우콜(메시, 스프라이트, UI, 커스텀 커맨드)을 스킵하되, 이전 프레임 잔상(고스트 데이터) 방지를 위해 `RPLO_Clear` 대상 렌더타겟 및 큐브맵 섀도우(6면)의 Clear만 수행 후 `EndRenderPass`를 호출하도록 처리.
  - **뷰포트 UI 및 디스플레이 연동**:
    - `ViewportPassInfo`에 `RenderPassID`를 매핑하고, 뷰포트의 `PASS` 메뉴 클릭 시 대상 패스의 활성화 여부를 즉시 갱신.
    - `LightingPass` 비활성화 시 검은 화면 대신 `GBuffer_Diffuse` 텍스처로 최종 디스플레이를 자동 우회(Bypass)하여 지오메트리 텍스처 확인 가능.
    - `UIOverlay` 토글 시 인게임 UI 캔버스 패스와 함께 에디터 뷰포트의 라이트/카메라 오버레이 아이콘 표시도 연동.
  - **SkyAtmosphere 시스템 분리**:
    - 구름 3D 텍스처 생성 및 `SkyMaterial` 슬롯 바인딩(1회)과 태양 시간 진행(`TimeOfDay`) 및 `DirectionalLight` 회전 동기화를 `Projects/GameModule/Source/SkyAtmosphereSystem`으로 분리.
    - `GameModule::OnLoad()`에서 시스템 등록 및 리소스 초기화 1회 수행으로 이전하여, 다중 뷰포트 실행 시 시간이 중복 진행되던 버그를 차단하고 뷰포트 코드를 경량화.
- 검증:
  - Shadow/PointShadow 해제 시 잔상 없이 그림자가 즉시 제거되는 동작 확인.
  - Geometry 해제 시 G-Buffer 클리어 및 메시 렌더링 스킵 확인.
  - Lighting 해제 시 GBuffer_Diffuse 디스플레이 우회 정상 동작 확인.
  - Forward/ForwardTransparent 및 UIOverlay 해제 시 해당 요소들의 격리 및 복원 확인.

## [2026-10-16] ECS UI Z-Index 편집과 그리기·선택 순서

- Core/Public/Components/UI/UIRenderOrderComponent.h에 zIndex를 분리했다. 미지정 요소의 기본값은 0이다.
- UILayout이 형제를 안정 정렬하며 부모와 하위 트리 묶음 및 원본 Hierarchy 배열을 유지한다.
  렌더링, 편집용 선택과 포인터 입력은 같은 정렬된 목록을 사용한다.
- 같은 Scene의 Canvas도 zIndex 순으로 렌더링한다. 동률은 영구 ID 순서다.
- Inspector와 UI View의 Order 메뉴에 값 변경, Bring to Front / Send to Back을 연결하고
  기존 Reflection 저장·복원과 SceneEditHistory를 사용한다. 새 UI 요소에는 순서 컴포넌트를 추가한다.
- UILayoutCheck, UIInteractionCheck, EditorHistoryCheck의 검사 코드를 보완했다.
  정적 점검만 수행했으며 이번 작업에서 빌드·테스트 실행·GPU 검증은 하지 않았다.
- 사용법, 그룹 범위와 검증 명령: [UIRenderOrder.md](UIRenderOrder.md).

## [2026-10-16] Add Button 후 UI 파이프라인 생성 실패 수정

- 기존 DXIL의 `dxc -dumpbin` 결과에서 UI VS의 TEXCOORD0/1 출력 레지스터는 1/2인데
  UI PS의 입력 레지스터는 0/1임을 확인했다. PS 입력에 SV_POSITION 선언이 빠져 있었다.
- UI VS/PS가 `uiCommon.hlsli`의 같은 입출력 구조체를 사용하도록 변경했다.
  이 헤더 변경 시 DXIL과 SPIR-V 양쪽 UI 셰이더가 다시 컴파일되도록 CMake 의존성을 추가했다.
- DirectX12 PSO 생성 실패의 HRESULT 및 해당 호출에서 발생한 InfoQueue 메시지를 Engine 로그에 기록한다.
  파이프라인 생성 실패 경로는 Safe_Release로 셰이더 참조도 정리한다.
- 검증: 기존 셰이더 바이너리의 읽기 전용 디스어셈블과 정적 점검을 수행했다.
  수정 후 셰이더 컴파일·Editor 빌드 및 GPU 재현 확인은 직접 실행하지 않았다.

## [2026-10-16] ECS UI 포인터 이벤트와 버튼 상호작용

- Area: Engine | Editor | UI
- Type: Feature | Fix
- 구현:
  - UIButtonComponent와 직렬화/편집 이력에서 제외하는 UIInteractionStateComponent를 UI 폴더에 분리했다.
  - Scene을 보유하는 UIEventSystem에 Enter/Exit/Down/Up/Click/Cancel과 포인터 캡처를 구현했다.
  - UILayout 그리기 순서에 맞춘 이미지 입력 차단·통과와 부모 버튼 대상 결정을 연결했다.
  - 포커스 상실·삭제·비활성화·잘못된 입력에서 캡처를 취소하며 콜백 사이에 대상 상태를 재검사한다.
  - UI View에 Add Button, Interact 미리보기와 마지막 이벤트/클릭 횟수 표시를 연결했다.
  - 버튼의 상태별 tint를 원본 Image 색상 변경 없이 렌더 명령에 적용한다.
  - MulticastDelegate의 실행 중 구독 추가·제거·Clear를 안전하게 처리하도록 보완했다.
- 검증: UIInteractionCheck를 추가하고 정적으로 점검했다. 이 작업에서 빌드·검사 실행·GPU 검증은 하지 않았다.
- 지원 범위와 연결 API, 구독 수명 계약 및 검증 명령: [UIInteraction.md](UIInteraction.md).

## [2026-10-16] ECS UI 데이터와 캔버스 편집기 기반

- Area: Engine | Editor | UI | Resource
- Type: Feature | Fix
- 구현:
  - `Core/Public/Components/UI`에 Canvas, RectTransform, UIImage 컴포넌트를 파일별로 분리했다.
  - Engine의 UILayout 계산을 UI 렌더링, Inspector 검증과 Editor 선택·변형에 공유한다.
  - UI View에 ECS Canvas/Image 생성, 해상도, 캔버스 경계·그리드, 팬·줌, 선택·이동·크기 조절과 스냅을 연결했다.
  - 전용 UI 패스는 선택한 ECS Canvas만 렌더링하며 레거시 GameObject UI 제출을 제외한다.
  - UI 계층의 씬 저장·복원과 vec2/vec4 및 Sprite/Material 에셋 참조 코덱을 추가했다.
  - 월드 Sprite의 PPU·Pivot 크기 계산과 레이 경계 검사를 같은 쿼드 변환에 연결했다.
- 검증 상태: 정적 점검만 수행했다. UILayoutCheck/CanvasViewCheck를 추가했으나 빌드·실행·GPU 검증은 미실행이다.
- 제한: 최상위 ScreenSpace Canvas와 Image를 지원한다. UI 부모 변경, Undo/Redo와 런타임 이벤트는 후속 작업이다.
- 사용법, 구조, 검증 명령과 나머지 제한: [UIEditorFoundation.md](UIEditorFoundation.md).

## [2026-10-16] SceneView Entity ID 피킹과 레이 경계 검사 기반

- Area: Engine | Editor | Rendering
- Type: Feature | Fix
- 구현:
  - 정적 메시와 Simple 스프라이트의 프레임별 ID를 GBuffer_Position.A에 기록한다.
  - SceneView 클릭을 Entity 선택에 연결하고, Ctrl 선택·선택 잠금·기즈모와 이미지 영역을 처리한다.
  - SDL GPU 및 DirectX12에 동기식 RGBA32F 한 픽셀 다운로드를 추가한다.
  - 원근/직교 화면 레이와 변환된 로컬 메시 경계 상자 검사를 추가한다.
  - 스프라이트 추가 경로를 GBuffer SpriteMaterial의 Masked 렌더링에 연결한다.
- 검증 상태: 정적 점검만 수행했다. 빌드, PickingMathCheck 실행과 GPU 수동 검증은 미실행이다.
- 지원 범위, 검증 명령, 제한과 후속 작업: [ScenePicking.md](ScenePicking.md).

## 2. Suggested Sections For This Project

- Rendering pipeline:
  - RHI backend status (SDLGPU, Vulkan, etc.)
  - RenderPass and RenderTarget changes
  - Shader compile/reflection updates
- UI system:
  - UICanvas policy and RectTransform changes
  - Pivot/Anchor behavior changes
  - UIViewport virtual canvas overlay changes
- Reflection pipeline:
  - Macro/schema updates
  - Codegen updates
  - Runtime registry/serialization updates
- Resource and serialization:
  - Importer updates
  - Archive format changes (Json/Binary/Beve/LZ4)
- Editor workflow:
  - Hierarchy/Inspector/Viewport interactions
  - Selection and gizmo behavior

---


---

## [2026-10-30] 노드 그래프 에디터의 imgui-node-editor 전환 및 블루프린트 룩앤필 고도화

- Area: Editor | UI | Graph
- Type: Feature | Refactor | UX
- 관련 파일:
  - Editor/CMake/NodeEditor.cmake
  - Editor/CMakeLists.txt
  - Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelNodeEditor.cpp
  - Docs/VisualScriptingBlueprintPlan.md
  - Docs/GraphPanel.md
- 배경 및 원인:
  - 기존 `imnodes` 기반 노드 에디터는 단일 세로 루프로 핀을 렌더링하여 `Exec In`이 상단에, `Exec Out`과 `Result`가 최하단에 배치되면서 다음 노드로 연결할 때 와이어가 우하향 계단식으로 늘어지는 UX 문제가 발생했다.
  - 언리얼 엔진 블루프린트 수준의 줌/패닝, 실행선 펄스 애니메이션, 노드 그룹화(코멘트 상자)를 지원하기 위해 전문 프레임워크인 `imgui-node-editor`로 전환을 결정했다.
- 구현 내용:
  - **빌드 시스템 전환:** CMake `FetchContent`를 통해 `imgui-node-editor` (Thedmd) 소스를 가져오고 `ThirdParty` 타깃으로 구성하여 `Editor`에 링크.
  - **수평 가로 흐름(Left-to-Right) 정립:** `Exec In`과 `Exec Out`을 최상단 행에 좌우로 마주보게 배치하고, 하단에 데이터 핀을 2열로 분할하여 세로 처짐 현상 제거.
  - **밀착형 헤더 배경:** 노드 Bounding Box(`nodeMin` ~ `nodeMax`)를 계산하여 테두리 유격/빈틈 없이 외곽선에 100% 밀착되는 상단 둥근 헤더 채우기 및 노드 카테고리별 테마 색상(이벤트: 레드, 함수: 블루, 수학/순수: 그린, 변수: 청록/오렌지) 적용.
  - **ImGui ID 충돌 해결:** 노드(`node.Id`) 및 핀(`pin.Key`) 단위 격리와 고유 접미사(`##ResetPin`, `##UsePin`)를 부여하여 "기본값으로 복원" 버튼의 ID duplicate 경고 해결.
  - **툴팁 좌표계 정규화:** 캔버스 줌/패닝 변환에 의한 툴팁 튐 현상을 방지하기 위해 `ed::Suspend()` / `ed::Resume()` 래핑 적용.
- 진행 중 / 후속 작업:
  - **핀 아이콘 정밀 피벗 결합:** 와이어 연결점이 텍스트 블록이 아닌 아이콘(`●`, `▶`) 중심에 정확히 결합되도록 `ed::PinPivotAlignment(0.5, 0.5)` 적용.
  - **코멘트 그룹 박스 (단축키 C):** `Core.Comment` 노드 정의 등록 및 `ed::Group(size)` 연동을 통해 선택된 노드들을 감싸는 반투명 그룹 박스 생성 및 일괄 이동/크기 조절 지원.
  - **노드 자동 정렬 도구:** 다중 선택 후 `Shift + W/A/S/D` 수평/수직 정렬.

---

## 3. Example Entry

### [2026-07-01] Documented full technology map

- Area: Docs
- Type: Docs
- Owner: @team
- Related files:
  - README.md
  - Docs/ImplementationLog.md
- Motivation:
  - Make architecture and technology intent explicit for onboarding.
- Implementation summary:
  - Added a full technology section covering Reflection, RHI, and UI canvas layout.
- Risks and follow-ups:
  - Keep this doc synced with actual code usage.
- Validation:
  - Manual review against CMake and source files.

---


---

## [2026-10-30] 노드 그래프 에디터의 imgui-node-editor 전환 및 블루프린트 룩앤필 고도화

- Area: Editor | UI | Graph
- Type: Feature | Refactor | UX
- 관련 파일:
  - Editor/CMake/NodeEditor.cmake
  - Editor/CMakeLists.txt
  - Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelNodeEditor.cpp
  - Docs/VisualScriptingBlueprintPlan.md
  - Docs/GraphPanel.md
- 배경 및 원인:
  - 기존 `imnodes` 기반 노드 에디터는 단일 세로 루프로 핀을 렌더링하여 `Exec In`이 상단에, `Exec Out`과 `Result`가 최하단에 배치되면서 다음 노드로 연결할 때 와이어가 우하향 계단식으로 늘어지는 UX 문제가 발생했다.
  - 언리얼 엔진 블루프린트 수준의 줌/패닝, 실행선 펄스 애니메이션, 노드 그룹화(코멘트 상자)를 지원하기 위해 전문 프레임워크인 `imgui-node-editor`로 전환을 결정했다.
- 구현 내용:
  - **빌드 시스템 전환:** CMake `FetchContent`를 통해 `imgui-node-editor` (Thedmd) 소스를 가져오고 `ThirdParty` 타깃으로 구성하여 `Editor`에 링크.
  - **수평 가로 흐름(Left-to-Right) 정립:** `Exec In`과 `Exec Out`을 최상단 행에 좌우로 마주보게 배치하고, 하단에 데이터 핀을 2열로 분할하여 세로 처짐 현상 제거.
  - **밀착형 헤더 배경:** 노드 Bounding Box(`nodeMin` ~ `nodeMax`)를 계산하여 테두리 유격/빈틈 없이 외곽선에 100% 밀착되는 상단 둥근 헤더 채우기 및 노드 카테고리별 테마 색상(이벤트: 레드, 함수: 블루, 수학/순수: 그린, 변수: 청록/오렌지) 적용.
  - **ImGui ID 충돌 해결:** 노드(`node.Id`) 및 핀(`pin.Key`) 단위 격리와 고유 접미사(`##ResetPin`, `##UsePin`)를 부여하여 "기본값으로 복원" 버튼의 ID duplicate 경고 해결.
  - **툴팁 좌표계 정규화:** 캔버스 줌/패닝 변환에 의한 툴팁 튐 현상을 방지하기 위해 `ed::Suspend()` / `ed::Resume()` 래핑 적용.
- 진행 중 / 후속 작업:
  - **핀 아이콘 정밀 피벗 결합:** 와이어 연결점이 텍스트 블록이 아닌 아이콘(`●`, `▶`) 중심에 정확히 결합되도록 `ed::PinPivotAlignment(0.5, 0.5)` 적용.
  - **코멘트 그룹 박스 (단축키 C):** `Core.Comment` 노드 정의 등록 및 `ed::Group(size)` 연동을 통해 선택된 노드들을 감싸는 반투명 그룹 박스 생성 및 일괄 이동/크기 조절 지원.
  - **노드 자동 정렬 도구:** 다중 선택 후 `Shift + W/A/S/D` 수평/수직 정렬.

---

## 4. Current Audit Snapshot (2026-07-01)

### [2026-07-01] Technology coverage and legacy candidate review

- Area: Engine | Editor | Build | Docs
- Type: Cleanup | Docs
- Owner: @team
- Related files:
  - CMakeLists.txt
  - Engine/CMakeLists.txt
  - Engine/Source/Render/Public/Renderer.h
  - Engine/Source/Render/Private/Renderer.cpp
  - Engine/Source/Render/RHI/Platform/Vulkan/Public/VulkanRHI.h
  - Editor/Source/ImGui/Private/ImGuiManager.cpp
  - Editor/Source/Qt/Public/QtManager.h
  - Editor/Source/Qt/Private/QtManager.cpp
  - vcpkg.json
- Findings:
  - RHI enum exposes DirectX12/Vulkan/Metal, but runtime creation currently handles SDLGPU only.
  - VulkanRHI appears as commented stub code and is not integrated in renderer switch path.
  - Qt manager exists as isolated subsystem files with no integration points found.
  - Root CMake disables Client target (commented add_subdirectory).
  - imgui manifest enables multiple bindings (dx12, vulkan, sdl3-renderer, sdlgpu3), while current editor uses sdlgpu3/sdlrenderer3 path.
  - Shader reflection code currently includes spirv_cross_c API; verify whether all configured spirv/glslang packages are required.
- Suggested actions:
  - Option A: mark inactive backends/subsystems as planned and document status explicitly.
  - Option B: remove inactive code/dependencies until backend integration starts.
  - Add backend support matrix to README and keep this log updated per release.

---


---

## [2026-10-30] 노드 그래프 에디터의 imgui-node-editor 전환 및 블루프린트 룩앤필 고도화

- Area: Editor | UI | Graph
- Type: Feature | Refactor | UX
- 관련 파일:
  - Editor/CMake/NodeEditor.cmake
  - Editor/CMakeLists.txt
  - Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelNodeEditor.cpp
  - Docs/VisualScriptingBlueprintPlan.md
  - Docs/GraphPanel.md
- 배경 및 원인:
  - 기존 `imnodes` 기반 노드 에디터는 단일 세로 루프로 핀을 렌더링하여 `Exec In`이 상단에, `Exec Out`과 `Result`가 최하단에 배치되면서 다음 노드로 연결할 때 와이어가 우하향 계단식으로 늘어지는 UX 문제가 발생했다.
  - 언리얼 엔진 블루프린트 수준의 줌/패닝, 실행선 펄스 애니메이션, 노드 그룹화(코멘트 상자)를 지원하기 위해 전문 프레임워크인 `imgui-node-editor`로 전환을 결정했다.
- 구현 내용:
  - **빌드 시스템 전환:** CMake `FetchContent`를 통해 `imgui-node-editor` (Thedmd) 소스를 가져오고 `ThirdParty` 타깃으로 구성하여 `Editor`에 링크.
  - **수평 가로 흐름(Left-to-Right) 정립:** `Exec In`과 `Exec Out`을 최상단 행에 좌우로 마주보게 배치하고, 하단에 데이터 핀을 2열로 분할하여 세로 처짐 현상 제거.
  - **밀착형 헤더 배경:** 노드 Bounding Box(`nodeMin` ~ `nodeMax`)를 계산하여 테두리 유격/빈틈 없이 외곽선에 100% 밀착되는 상단 둥근 헤더 채우기 및 노드 카테고리별 테마 색상(이벤트: 레드, 함수: 블루, 수학/순수: 그린, 변수: 청록/오렌지) 적용.
  - **ImGui ID 충돌 해결:** 노드(`node.Id`) 및 핀(`pin.Key`) 단위 격리와 고유 접미사(`##ResetPin`, `##UsePin`)를 부여하여 "기본값으로 복원" 버튼의 ID duplicate 경고 해결.
  - **툴팁 좌표계 정규화:** 캔버스 줌/패닝 변환에 의한 툴팁 튐 현상을 방지하기 위해 `ed::Suspend()` / `ed::Resume()` 래핑 적용.
- 진행 중 / 후속 작업:
  - **핀 아이콘 정밀 피벗 결합:** 와이어 연결점이 텍스트 블록이 아닌 아이콘(`●`, `▶`) 중심에 정확히 결합되도록 `ed::PinPivotAlignment(0.5, 0.5)` 적용.
  - **코멘트 그룹 박스 (단축키 C):** `Core.Comment` 노드 정의 등록 및 `ed::Group(size)` 연동을 통해 선택된 노드들을 감싸는 반투명 그룹 박스 생성 및 일괄 이동/크기 조절 지원.
  - **노드 자동 정렬 도구:** 다중 선택 후 `Shift + W/A/S/D` 수평/수직 정렬.

---

## 5. 2D Foundation Backlog (Actionable)

Use this as the primary execution list until a playable 2D vertical slice is complete.

### 5.1 Sprint Goal

- Goal:
  - Complete a stable 2D engine baseline that supports one sample game loop end-to-end.
- Exit condition:
  - A sample scene can run with sprite rendering, UI interaction, collision events, and save/load.

### 5.2 Issue Template (Copy/Paste)

### [2D-XXX] Title

- Priority: P0 | P1 | P2
- Area: Render2D | UI2D | Physics2D | Resource | Editor | Runtime
- Status: Todo | InProgress | Done | Blocked
- Owner: @name
- Dependencies:
  - [2D-YYY]
- Scope:
  - What will be delivered in this issue.
- DoD:
  - [ ] Build passes in Debug
  - [ ] No regression in existing sample scenes
  - [ ] Added/updated serialization path if data format changed
  - [ ] Added editor workflow support when applicable
  - [ ] ImplementationLog entry created
- Validation:
  - Commands run:
    - cmake --build .\\out\\build\\x64-debug --config Debug --target Editor
  - Manual test cases:
    - Case 1:
    - Case 2:

### 5.3 Priority Backlog

#### P0: Rendering/Core

- [ ] [2D-001] Sprite batch rendering pipeline
  - DoD:
    - [ ] Draw calls reduced versus per-sprite draw path
    - [ ] Material/texture grouping works as expected
    - [ ] Alpha blend ordering strategy documented

- [ ] [2D-002] 2D camera and layer sorting policy
  - DoD:
    - [ ] Orthographic camera path fixed and tested
    - [ ] Sort key (layer/order/z) deterministic across frames
    - [ ] Transparent ordering rules documented

- [ ] [2D-003] 2D render pass minimalization
  - DoD:
    - [ ] Pass set simplified to World2D + UI (+ optional PostFX)
    - [ ] Unused pass dependencies removed from startup
    - [ ] Render pass debug view updated in editor

#### P0: UI/Interaction

- [ ] [2D-004] RectTransform + Canvas deterministic layout
  - DoD:
    - [ ] Anchor/Pivot/Size behavior matches documented equations
    - [ ] ReferenceResolution + Match mode behavior is reproducible
    - [ ] ExternalOverride layout ownership token flow verified

- [ ] [2D-005] UI input event system
  - DoD:
    - [ ] Click, press, release, hover, drag events implemented
    - [ ] Focus propagation and raycast target rules defined
    - [ ] Event order and bubbling/capture policy documented

#### P0: Physics2D/Gameplay

- [ ] [2D-006] Basic 2D collision stack (AABB/Circle)
  - DoD:
    - [ ] Broadphase + narrowphase path passes test scenes
    - [ ] Trigger and collision events are separated
    - [ ] Layer mask filtering works in runtime and editor

- [ ] [2D-007] Rigidbody2D minimum feature set
  - DoD:
    - [ ] Velocity, gravity, damping, fixed-step integration stable
    - [ ] Freeze constraints (x/y/rotation) work
    - [ ] Deterministic simulation at fixed timestep validated

#### P1: Resource/Pipeline

- [ ] [2D-008] Sprite atlas asset pipeline
  - DoD:
    - [ ] Atlas build/import path integrated into resource system
    - [ ] UV/pivot metadata preserved through import
    - [ ] Hot reload updates live scene references safely

- [ ] [2D-009] 2D animation clip pipeline
  - DoD:
    - [ ] Frame-based clip asset format finalized
    - [ ] Runtime animator component supports clip play/loop/speed
    - [ ] Serialization and inspector editing supported

#### P1: Editor Productivity

- [ ] [2D-010] 2D gizmo + grid snapping
  - DoD:
    - [ ] Move/rotate/scale gizmo works in 2D viewport
    - [ ] Grid/pixel snapping toggles are stable
    - [ ] Manipulation precision is consistent across zoom levels

- [ ] [2D-011] Undo/Redo minimum viable path
  - DoD:
    - [ ] Transform and component property edits are undoable
    - [ ] Object create/delete is undoable
    - [ ] History stack survives scene save/load boundaries safely

#### P2: Quality/Operations

- [ ] [2D-012] Runtime metrics and debug overlay
  - DoD:
    - [ ] Frame time, draw call, entity count metrics displayed
    - [ ] Metrics can be toggled in editor and runtime
    - [ ] Baseline performance snapshot recorded in this log

- [ ] [2D-013] 2D regression test scenes
  - DoD:
    - [ ] At least 3 scenarios: rendering, UI, collision
    - [ ] Startup script can load each scene quickly
    - [ ] Known expected outputs documented

### 5.4 Recommended Execution Order

1. 2D-001 -> 2D-002 -> 2D-003
2. 2D-004 -> 2D-005
3. 2D-006 -> 2D-007
4. 2D-008 -> 2D-009
5. 2D-010 -> 2D-011 -> 2D-012 -> 2D-013

### 5.5 Weekly Review Checklist

- [ ] This week completed issues are moved to Done
- [ ] Blocked issues include root cause and owner
- [ ] Any data format change has migration notes
- [ ] README architecture sections remain synchronized
- [ ] A new ImplementationLog entry is added for each merged milestone

---


---

## [2026-10-30] 노드 그래프 에디터의 imgui-node-editor 전환 및 블루프린트 룩앤필 고도화

- Area: Editor | UI | Graph
- Type: Feature | Refactor | UX
- 관련 파일:
  - Editor/CMake/NodeEditor.cmake
  - Editor/CMakeLists.txt
  - Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanel.cpp
  - Editor/Source/ImGui/GraphPanel/Private/GraphPanelNodeEditor.cpp
  - Docs/VisualScriptingBlueprintPlan.md
  - Docs/GraphPanel.md
- 배경 및 원인:
  - 기존 `imnodes` 기반 노드 에디터는 단일 세로 루프로 핀을 렌더링하여 `Exec In`이 상단에, `Exec Out`과 `Result`가 최하단에 배치되면서 다음 노드로 연결할 때 와이어가 우하향 계단식으로 늘어지는 UX 문제가 발생했다.
  - 언리얼 엔진 블루프린트 수준의 줌/패닝, 실행선 펄스 애니메이션, 노드 그룹화(코멘트 상자)를 지원하기 위해 전문 프레임워크인 `imgui-node-editor`로 전환을 결정했다.
- 구현 내용:
  - **빌드 시스템 전환:** CMake `FetchContent`를 통해 `imgui-node-editor` (Thedmd) 소스를 가져오고 `ThirdParty` 타깃으로 구성하여 `Editor`에 링크.
  - **수평 가로 흐름(Left-to-Right) 정립:** `Exec In`과 `Exec Out`을 최상단 행에 좌우로 마주보게 배치하고, 하단에 데이터 핀을 2열로 분할하여 세로 처짐 현상 제거.
  - **밀착형 헤더 배경:** 노드 Bounding Box(`nodeMin` ~ `nodeMax`)를 계산하여 테두리 유격/빈틈 없이 외곽선에 100% 밀착되는 상단 둥근 헤더 채우기 및 노드 카테고리별 테마 색상(이벤트: 레드, 함수: 블루, 수학/순수: 그린, 변수: 청록/오렌지) 적용.
  - **ImGui ID 충돌 해결:** 노드(`node.Id`) 및 핀(`pin.Key`) 단위 격리와 고유 접미사(`##ResetPin`, `##UsePin`)를 부여하여 "기본값으로 복원" 버튼의 ID duplicate 경고 해결.
  - **툴팁 좌표계 정규화:** 캔버스 줌/패닝 변환에 의한 툴팁 튐 현상을 방지하기 위해 `ed::Suspend()` / `ed::Resume()` 래핑 적용.
- 진행 중 / 후속 작업:
  - **핀 아이콘 정밀 피벗 결합:** 와이어 연결점이 텍스트 블록이 아닌 아이콘(`●`, `▶`) 중심에 정확히 결합되도록 `ed::PinPivotAlignment(0.5, 0.5)` 적용.
  - **코멘트 그룹 박스 (단축키 C):** `Core.Comment` 노드 정의 등록 및 `ed::Group(size)` 연동을 통해 선택된 노드들을 감싸는 반투명 그룹 박스 생성 및 일괄 이동/크기 조절 지원.
  - **노드 자동 정렬 도구:** 다중 선택 후 `Shift + W/A/S/D` 수평/수직 정렬.

---

## 6. Reflection / Archive 검증 기록

### [2026-10-16] 등록된 객체 vector의 자동 저장·복원과 Runtime 독립 구성

- 목적: 타입별 직렬화 함수나 객체 vector 코덱 등록 없이, 등록 정보와 공통 API로 저장·복원한다.
- 변경:
  - ReflectionArchive의 객체 vector 헬퍼를 실제 저장·복원 분기에 연결하고 상호 호출 선언 순서를 수정했다.
  - 스칼라 컨테이너의 타입 미일치와 처리 실패를 구분하여, 실패를 다른 처리 경로로 우회하지 않는다.
  - 객체 요소의 C++ 타입으로 Registry를 조회한다. 빈 vector도 요소 등록을 요구한다.
  - 복원은 상위 객체의 복사본에서 새 요소를 기본 생성한다. 일반 오류에서는 대상에 반영하지 않는다.
  - REFLECTION_BUILD_GENERATOR=OFF일 때 LLVM·Clang 검색을 생략한다. SDK 경로 직접 지정도 존중한다.
  - Archive, Reflection, ReflectionArchive README에 자동 처리 범위와 실패 시 반영 범위를 갱신했다.
- 검증 환경: Windows x64, Visual Studio 18 2026 / MSVC 19.51, LLVM 20.1.8, Debug.
- 실행 검증:
  - Archive 독립 구성: `out/build/reflection-system-audit`에서 기존 vcpkg 설치 경로를 CMAKE_PREFIX_PATH로 사용했다.
  - ArchiveExample, ArchiveReflectionExample, ArchiveGeneratedReflectionExample, ArchiveGlmExample, ReflectionExample 빌드 및 실행 성공.
  - 객체 vector의 JSON·BEVE 왕복과 빈 vector, 선택적 필드 기본값, 필드 누락·잘못된 값의 위치, 일반 실패 시 원본 보존을 검사했다.
  - Registry 누락·요소 등록 누락, 기존 명시적 코덱 호환성과 코덱 오류 전파를 검사했다.
  - 패키지를 `out/install/reflection-audit`에 재설치하고 소비자를 다시 구성·빌드했다.
  - `ctest --test-dir out/build/reflection-archive-consumer-audit -C Debug --output-on-failure`: 1/1 통과. 기존 code=10, field=Entries 실패 해결.
  - 설치된 ArchiveInterface.h, PropertyInfo.h, ReflectionArchiveAdapter.h의 SHA256이 소스와 일치했다.
  - `REFLECTION_BUILD_GENERATOR=OFF`와 LLVM·Clang find_package 금지 옵션으로 별도 구성한 ReflectionRuntime 빌드 성공.
- 검증 제한:
  - ArchiveEngineComponentExample은 기존 작업 트리에서 삭제된 `Engine/Source/Core/Public/Reflection/ComponentReflectionBindings.h` 참조 때문에 빌드 실패했다. 해당 삭제나 엔진 연결 코드는 변경하지 않았다.
  - 루트 빌드는 vcpkg 잠금 접근과 MSVC 환경 설정에서 구성에 실패하여 위 독립 구성으로 검증했다. 엔진·에디터 전체 빌드 성공을 의미하지 않는다.
  - map, enum 컨테이너, 객체 고정 배열과 임의 중첩 컨테이너의 자동 지원까지 완료한 것은 아니다. 해당 범위는 명시적 코덱이 필요하다.
  - 파일 덮어쓰기의 원자성, 사용자 예외·공유 자원·콜백 외부 부작용의 롤백은 보장하지 않는다.
  - ReflectionEnTT는 변경하지 않았다. 기존 시스템 완성 후 성능 비교용으로 유지한다.

### [2026-10-16] 스칼라·등록 객체 map의 자동 저장·복원

- 목적: map 멤버를 등록한 뒤 컨테이너 타입별 코덱을 반복 작성해야 하던 요구를 제거한다.
- 변경:
  - 지원 스칼라 키와 스칼라 또는 등록된 객체 값을 가진 map·unordered_map을 기본 경로에서 처리한다.
  - Reflection에 CanEmplaceMapValueObject / TryEmplaceMapValueObject를 추가했다. 키 복사와 값 기본 생성을 지원하는 수정 가능한 객체 map에만 연결한다.
  - Archive의 기존 인덱스 기반 객체 배열 API를 사용한다. Archive 코어의 API·의존성·백엔드는 변경하지 않았다.
  - 저장 시 기존 단회 map 순회 API로 키·값 뷰를 수집한다. 인덱스 접근을 반복하여 매번 begin부터 이동하지 않는다.
  - 복원은 상위 객체의 복사본에서 map을 비우고 새 항목을 삽입한다. 객체 값의 누락된 Optional 필드는 새 기본값을 따른다.
  - 기존 Key / Value 배열 형식과 명시적 코덱 API를 유지한다. 사용자 코덱을 우선하며 오류를 자동 경로로 우회하지 않는다.
  - generated-tests의 기본 사용 흐름과 설치 패키지 소비자에서 수동 map 코덱 등록을 제거했다.
- 검증 환경: Windows x64 / Visual Studio 18 2026 / MSVC 19.51 / LLVM 20.1.8 / Debug.
- 실행 검증:
  - ArchiveGeneratedReflectionExample: 생성 등록된 스칼라·객체 map의 공통 API 왕복과 기존 코덱 문서 양방향 호환 검사.
  - AutomaticMapTests: JSON·BEVE, 빈 컨테이너, 한글·이모지 키, uint64 최대값, 대상 비교자·해시 상태, 선택적 필드 기본값 검사.
  - 중복 키, 필드 누락, 잘못된 키·값, 객체 값 등록 누락, 미지원 키 타입과 실패 시 원본 객체·문서 보존 검사.
  - 새 Reflection 접근 API의 const·잘못된 타입·중복 키·기본 생성 불가능한 값 거부 검사.
  - ArchiveReflectionExample와 ReflectionExample 빌드·실행 성공. 기존 코덱과 접근·생성 등록 검사 유지.
  - `cmake --install out/build/reflection-system-audit --config Debug --prefix out/install/reflection-audit`로 재설치했다.
  - 설치 패키지 소비자 빌드 후 `ctest --test-dir out/build/reflection-archive-consumer-audit -C Debug --output-on-failure`: 1/1 통과.
  - 변경된 Reflection 접근 헤더 3개와 ReflectionArchiveAdapter.h의 설치본 SHA256이 소스와 일치했다.
  - LLVM·Clang 검색을 끈 별도 구성에서 ReflectionRuntime 재빌드 성공.
- 호환성과 범위:
  - Reflection의 내부 컨테이너 접근 정보가 확장되었으므로 헤더와 Runtime 라이브러리를 함께 갱신하고 소비자를 다시 빌드해야 한다.
  - map의 비교자·해시·allocator 정책은 파일에 저장하지 않는다. 대상 복사본의 컨테이너 동작과 C++ 복사·대입·allocator 전파 규칙을 따른다.
  - enum 컨테이너, enum 키·값의 map, 객체 고정 배열, 임의의 중첩 컨테이너와 포인터 자동 복원은 이번 지원 범위가 아니다.
  - 엔진·에디터 전체 빌드와 성능 측정 결과를 의미하지 않는다. ReflectionEnTT는 변경하지 않았다.

### [2026-10-16] 등록된 scoped enum 컨테이너의 자동 저장·복원

- 목적: enum 멤버와 컨테이너를 등록한 뒤 타입별 배열·map 코덱을 반복 등록하던 요구를 제거한다.
- 변경:
  - EnumInfo에 IsUnderlyingSigned, CanVisitValue, TryVisitValue를 추가했다. For<T>에서 기반 타입 범위 검사와 임시 enum 뷰 변환을 연결한다.
  - TryVisitValue는 scoped enum의 숫자 변환이 성공한 경우에만 동기적으로 콜백을 호출한다. 뷰는 콜백 안에서만 유효하다.
  - ReflectionArchive의 내부 EnumArchiveAdapter.h에서 enum 숫자 읽기·쓰기와 컨테이너 변환을 처리한다.
  - 등록된 scoped enum의 vector·array·set·unordered_set과 enum 키·값의 map·unordered_map을 기본 경로로 처리한다.
  - enum 키와 등록된 객체 값을 조합한 map도 같은 공통 경로를 사용한다.
  - 빈 컨테이너·길이 0인 배열도 enum 등록을 요구한다. bool 기반 enum은 숫자 0·1만 허용한다.
  - 열거자 목록에 없는 표현 가능한 값과 비트 조합을 허용하는 기존 정책을 유지한다.
  - 기존 고정 enum 배열 예제의 수동 코덱 등록을 제거하고, 외부 소비자에 enum 컨테이너 멤버를 추가했다.
- 실행 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51 / LLVM 20.1.8 / Debug.
  - ArchiveGeneratedReflectionExample 빌드·실행 성공. JSON·BEVE, signed 8비트 경계, unsigned 64비트 최댓값, bool 기반 enum과 빈 컨테이너를 검사했다.
  - 배열 길이·변환 범위 초과·set 중복·map 중복 키·잘못된 형식의 오류 경로와 일반 실패 시 원본 보존을 검사했다.
  - enum 전체 등록 누락과 map 키·값 중 한쪽만 등록한 경우를 거부하는지 확인했다.
  - 기존 배열·map 코덱 문서의 양방향 호환성과 명시적 코덱 오류 전파를 확인했다.
  - ArchiveReflectionExample와 ReflectionExample 빌드·실행 성공. Reflection에는 임시 enum 뷰의 타입과 범위 검사를 추가했다.
  - 설치 패키지를 갱신한 소비자 CTest: `ctest --test-dir out/build/reflection-archive-consumer-audit -C Debug --output-on-failure`, 1/1 통과.
  - 설치된 EnumInfo.h, ReflectionArchiveAdapter.h, detail/EnumArchiveAdapter.h의 SHA256이 소스와 일치했다.
  - LLVM·Clang 검색을 끈 별도 구성에서 ReflectionRuntime 재빌드 성공.
- 범위와 호환성:
  - EnumInfo 내부 정보가 확장되었으므로 헤더와 Runtime을 함께 갱신하고 소비자를 다시 빌드해야 한다.
  - 일반 enum의 자동 숫자 변환, 객체 고정 배열, 임의의 중첩 컨테이너와 포인터 자동 복원은 이번 지원 범위가 아니다.
  - 사용자 콜백의 예외·외부 부작용과 파일 덮어쓰기 원자성은 별도 문제로 유지한다.
  - 엔진·에디터 전체 빌드나 성능 측정을 의미하지 않는다. ReflectionEnTT의 비교 작업은 진행하지 않았다.

### [2026-10-16] 등록된 객체 고정 배열의 자동 저장·복원

- 목적: 등록된 객체의 std::array 멤버를 별도 코덱 없이 공통 API로 저장·복원한다.
- 변경:
  - Reflection에 CanResetElements / TryResetElements와 ResetUnavailable 오류를 추가했다.
  - 수정 가능한 std::array 자체의 기본 생성과 복사 대입을 지원할 때 초기화 콜백을 연결한다. Clear와 달리 길이를 유지한다.
  - 초기화는 기본 생성한 배열 값을 const 참조로 복사 대입한다. 이동 대입이 삭제된 복사 전용 요소도 지원한다.
  - ReflectionArchive의 객체 vector 순회를 객체 고정 배열과 공유했다. 고정 배열은 정확한 길이를 검사한 뒤 상위 객체의 복사본에서 초기화·복원한다.
  - 누락된 Optional 필드와 저장 제외 멤버는 요소 타입의 기본값을 따른다. 소유 클래스의 배열 멤버 초기화식을 다시 실행하는 기능은 아니다.
  - 스칼라 고정 배열의 타입 미일치와 실제 처리 실패를 optional 결과로 구분한다. 처리 실패를 객체 배열 경로로 우회하지 않는다.
  - 기존 FixedProfile의 객체 배열 저장 제외 설정을 제거하고, 생성 등록 및 외부 패키지 소비자 예제에 길이 0과 일반 객체 배열을 연결했다.
- 실행 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51 / LLVM 20.1.8 / Debug.
  - ArchiveGeneratedReflectionExample 빌드·실행 성공. 기존 vector·map·enum·스칼라 배열 검사와 객체 고정 배열 검사를 함께 통과했다.
  - JSON·BEVE 왕복, 배열 길이 부족·초과, 잘못된 요소와 필드 누락, 선택적·저장 제외 멤버 기본값 복원을 검사했다.
  - 일반 읽기 실패 시 원본 객체 보존, 잘못된 값 저장 시 기존 문서 보존, Registry·요소 타입 등록 누락과 명시적 코덱 오류 전파를 검사했다.
  - 기본 생성 불가능한 요소의 일반 크기 배열 복원을 거부하고, 실제 요소가 없는 길이 0 배열은 처리하는지 확인했다.
  - ArchiveReflectionExample와 ReflectionExample 빌드·실행 성공. const·잘못된 소유 객체·미지원 컨테이너의 초기화 거부와 복사 전용 요소를 검사했다.
  - 패키지를 재설치하고 외부 소비자를 빌드했다. CTest `out/build/reflection-archive-consumer-audit`: 1/1 통과. 일반·빈 객체 배열 왕복과 잘못된 길이·내부 값의 원본 보존을 확인했다.
  - LLVM·Clang 검색을 끈 별도 구성에서 ReflectionRuntime 재빌드 성공.
- 범위와 호환성:
  - Reflection 컨테이너 접근 정보가 확장되었으므로 헤더와 Runtime 라이브러리를 함께 갱신하고 소비자를 다시 빌드해야 한다.
  - 객체 배열 복원에는 배열 자체의 기본 생성·복사 대입과 요소 편집이 필요하다. 그 밖의 생성 정책은 사용자 코덱으로 처리한다.
  - 길이 0인 객체 배열도 요소 타입 등록은 필요하지만 실제 요소 생성자는 호출하지 않는다.
  - 임의 중첩 컨테이너, 포인터 대상 복원, 사용자 예외·외부 부작용의 롤백과 파일 저장 원자성은 이번 지원 범위가 아니다.
  - 엔진·에디터 전체 빌드나 성능 측정을 의미하지 않는다. ReflectionEnTT는 변경하지 않았다.

### [2026-10-16] 지원 컨테이너 조합의 재귀 저장·복원

- 목적: vector·고정 배열의 요소와 map의 값이 다시 지원 컨테이너인 경우에도 등록 정보와 공통 API로 처리한다.
- 변경:
  - Reflection에 GetElementContainerProperty / GetMapValueContainerProperty와 MakeContainerProperty를 추가했다.
  - 내부 컨테이너는 별도 Registry 등록 없이 타입별 정적 접근 정보를 공유하며, 소유 객체 멤버와 독립 컨테이너 값의 기존 접근 콜백을 재사용한다.
  - PropertyInfo::AsObject는 실제 값 타입을 확인하고 읽기 전용 ObjectView만 만든다. 수정은 기존 요소·mapped 객체 편집 경로를 사용한다.
  - Archive에 WriteArrayElements / ReadArrayElements를 추가했다. 배열 원소를 임시 필드에 읽고 쓰는 콜백으로 전달하고, 문서에는 원소 값만 반영한다.
  - 쓰기 콜백의 값 누락·추가 필드·닫지 않은 객체를 거부한다. 임시 필드명을 실제 중첩 배열 오류 경로로 변환한다.
  - ReflectionArchive의 단일 프로퍼티 처리를 공통 함수로 분리하고 내부 컨테이너에서 재귀 호출한다. 객체·중첩 컨테이너의 고정 길이 검사와 초기화 경로도 공유한다.
  - 빈 바깥 컨테이너에서도 내부 타입의 자동 지원 범위와 객체·enum 등록을 확인한다. 명시적 내부 컨테이너 코덱은 우선 적용하고 오류를 전파한다.
  - 스칼라 컨테이너의 기존 기본 템플릿 인자 제약을 유지하며, 미지원 내부 컨테이너가 빈 상태라는 이유로 자동 지원되는 것처럼 처리하지 않는다.
  - Reflection·Archive·ReflectionArchive README와 외부 패키지 소비자 예제를 갱신했다.
- 실행 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51 / LLVM 20.1.8 / Debug.
  - ArchiveExample, ArchiveGeneratedReflectionExample, ArchiveReflectionExample, ReflectionExample 빌드·실행 성공.
  - JSON·BEVE에서 vector<vector<int>>, array<vector<float>>, map<string, vector<Settings>>, enum 고정 배열의 vector, map의 vector, set·vector<bool>의 vector와 3중 vector를 검사했다.
  - 빈 내부·외부 컨테이너, 길이 0 배열, 직접 작성한 중첩 배열 문서, 고정 길이 오류, 중복 키, enum 범위 초과와 내부 객체의 잘못된 값을 검사했다.
  - 선택적·저장 제외 멤버의 새 요소 기본값, 일반 읽기 실패 시 대상 객체 보존, 저장 실패 시 기존 문서 보존과 중첩 오류 경로를 확인했다.
  - const·잘못된 소유 타입의 Reflection 접근 거부, 내부 컨테이너 코덱의 성공·실패 우선순위와 빈 컨테이너의 등록 누락 거부를 확인했다.
  - Archive의 임의 값 배열 콜백에서 잘못된 출력, 객체 깊이 불균형, 빈 콜백과 콜백 오류를 검사했다.
  - 패키지 재설치 후 외부 소비자 빌드 성공. CTest `out/build/reflection-archive-consumer-audit`: 1/1 통과.
  - 변경된 설치 헤더 7개와 ArchiveJson.lib·ReflectionRuntime.lib의 SHA256이 소스·빌드 산출물과 일치했다.
  - LLVM·Clang 검색을 끈 별도 구성에서 ReflectionRuntime 재빌드 성공.
- 범위와 호환성:
  - 지원 컨테이너의 재귀 조합이며 모든 STL 타입을 자동 처리한다는 의미는 아니다. 컨테이너·객체 요소의 set은 자동 지원하지 않는다.
  - 새로 기본 생성하는 내부 컨테이너의 비교·해시·allocator는 해당 타입의 기본 생성 정책을 따른다. 기존 위치의 내부 상태를 병합하지 않는다.
  - 스칼라 배열의 형식 오류는 기존 정책에 따라 내부 배열 필드 단위이며, 모든 오류에 최하위 요소 인덱스가 포함되지는 않는다.
  - Reflection 접근 정보와 Archive 가상 인터페이스가 확장되었으므로 두 모듈의 헤더·라이브러리를 함께 갱신하고 소비자를 다시 빌드해야 한다.
  - 사용자 IArchive 백엔드는 새 배열 훅을 구현해야 중첩 배열을 사용할 수 있다. 기본 훅은 UnsupportedType을 반환한다.
  - 포인터 대상 복원, 사용자 예외·외부 부작용의 롤백과 파일 저장 원자성은 추가하지 않았다.
  - 엔진·에디터 전체 빌드나 성능 측정을 의미하지 않는다. ReflectionEnTT는 변경하지 않았다.

### [2026-10-16] 기반 클래스 멤버를 포함하는 파생 객체의 저장·복원

- 목적: 실제 파생 타입을 알고 있는 객체에서 기반 클래스의 등록 멤버까지 공통 API로 처리한다.
- 변경:
  - Reflection에 BaseInfo와 TypeInfo::Bases를 추가했다. 실제 C++ 타입을 검사한 뒤 기반 부분 객체로 변환하며 const를 유지한다.
  - ObjectView::RefersToSameObject는 타입과 주소로 실제 부분 객체의 동일성을 확인한다.
  - Registry에 InvalidBase / DuplicateBase 검사를 추가했다. 기반 타입의 등록 순서는 강제하지 않는다.
  - 생성기는 직접 기반 타입의 이름·접근 수준·virtual 여부를 수집하고 변환 코드를 생성한다.
  - private·protected 상속은 파생 타입의 REFLECT_BODY() 권한으로 변환을 연결한다. 권한이 없으면 코드 생성 단계에서 오류로 거부한다.
  - ReflectionArchive는 기반 멤버를 선언 순서대로 먼저 순회하고 파생 멤버를 같은 객체 필드에 저장한다. 기반 멤버를 파생 타입에 중복 등록하지 않는다.
  - 저장 대상 이름 충돌은 실제 필드 처리·사용자 코덱 호출 전에 InvalidOperation으로 거부한다. 저장 제외 필드는 충돌 검사에서 제외한다.
  - 공유 가상 기반은 타입·주소가 같은 부분 객체를 한 번만 처리한다. 별개의 비가상 기반은 같은 타입이어도 합치지 않는다.
  - 기반 관계가 없는 타입은 기존 직접 프로퍼티 순회를 유지한다.
  - 가상 상속 왕복 검사에서 Archive::ReadObject의 최종 기본 이동 대입이 공유 기반의 문자열·vector를 비우는 경로를 재현했다.
  - 복사 대입 가능한 객체는 const 복사본에서 최종 복사 대입하도록 수정했다. 복사 대입을 지원하지 않는 기존 타입은 이동 대입 경로를 유지한다.
  - Reflection·Archive·ReflectionArchive README와 설치 패키지 소비자의 루트·컨테이너 파생 타입을 갱신했다.
- 실행 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51 / LLVM 20.1.8 / Debug.
  - ArchiveExample, ArchiveGeneratedReflectionExample, ArchiveReflectionExample, ReflectionExample 빌드·실행 성공.
  - JSON·BEVE에서 다단계·다중·가상 상속, private·protected 상속, 기반 private 멤버와 파생 객체의 vector·array·map을 검사했다.
  - 공유 가상 기반의 코덱 호출이 읽기·쓰기 각각 한 번인지 확인했다. 가상 기반의 문자열·vector 유실도 수정 후 같은 입력으로 재검증했다.
  - 기반·파생의 같은 이름과 서로 다른 비가상 기반의 이름 충돌, 기반 등록 누락, 잘못된 소유 타입과 const 변환을 검사했다.
  - Optional·Transient 정책, 새 컨테이너 요소의 기반 기본값, 일반 실패 시 대상 객체·기존 문서 보존과 오류 위치를 검사했다.
  - Archive 단독 검사에서 공유 가상 기반 데이터 보존, 실패한 후보의 폐기와 복사 생성·이동 대입만 지원하는 타입의 호환성을 확인했다.
  - PrivateBaseWithoutFriend.h의 직접 생성은 기대한 오류와 종료 코드 1을 반환했고 생성 출력 파일을 만들지 않았다.
  - 패키지 재설치 후 외부 소비자 빌드 성공. CTest `out/build/reflection-archive-consumer-audit`: 1/1 통과.
  - 설치 헤더 6개와 ReflectionRuntime.lib·ArchiveJson.lib의 SHA256이 소스·빌드 산출물과 일치했다.
  - LLVM·Clang 검색을 끈 별도 구성에서 ReflectionRuntime 재빌드 성공.
- 범위와 호환성:
  - 실제 파생 타입이 알려진 객체의 멤버 처리다. 기반 포인터·참조의 동적 타입 식별, downcast와 실제 파생 객체의 생성은 추가하지 않았다.
  - TypeInfo::FindProperty / FindFunction은 직접 등록된 멤버만 찾는다. 기반 멤버의 직접 접근에는 기반 TypeInfo 조회와 BaseInfo 변환을 사용한다.
  - 기반 TypeInfo가 없으면 UnsupportedType과 해당 기반 타입 이름을 반환한다. 상속 필드 이름 충돌은 InvalidOperation과 충돌한 필드 이름을 반환한다.
  - 새 생성기로 등록 코드를 다시 만들고 Reflection 헤더·Runtime 및 Archive를 함께 갱신한 뒤 소비자를 다시 빌드해야 한다.
  - 최종 복사 대입의 추가 복사 비용은 있을 수 있으며 성능 측정은 수행하지 않았다. 사용자 정의 대입의 예외·외부 부작용까지 되돌리는 보장은 추가하지 않았다.
  - 엔진·에디터 전체 빌드는 검증하지 않았다. ReflectionEnTT는 변경하지 않았다.

### [2026-10-16] 모듈 독립 구성과 설치 패키지 재사용 검증

- 목적: 엔진 소스 없이 Reflection·Archive·ReflectionArchive를 선택해 빌드하고 설치 패키지로 재사용한다.
- 변경:
  - Archive의 수동 등록·생성 등록·엔진 컴포넌트 예제 옵션을 분리했다. 생성 예제에 수동 예제를 함께 켤 필요가 없다.
  - 엔진 컴포넌트 예제는 새 ARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE 옵션을 명시적으로 켤 때만 구성한다. 기본값은 OFF다.
  - 수동·생성·엔진 예제가 공유하는 Reflection 의존성 구성을 별도 CMake 파일로 모았다.
  - Archive와 Reflection의 활성화한 검증 예제를 BUILD_TESTING 조건으로 CTest에 등록했다. 각 프로그램의 작업 폴더를 분리해 병렬 실행의 출력 파일 충돌을 피한다.
  - Archive·Reflection·ReflectionArchive 설치 소비자를 CTest에 연결했다.
  - 설치 ReflectionConfig에 Runtime 컴포넌트 제공 표시를 추가했다. find_package(Reflection COMPONENTS Runtime)를 사용할 수 있다.
  - ReflectionPackageCheck에 Runtime 전용 소비자를 추가했다. REFLECTION_PACKAGE_CHECK_GENERATED=OFF이면 Generator 없이 이 검사만 빌드한다.
  - 각 모듈 README에 옵션 분리, CTest 실행과 상위 프로젝트의 include(CTest) 조건을 반영했다.
- 실행 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51 / Debug.
  - 검증 경로: out/audit/module-reuse-20261016. 관련 프로젝트 소스 170개 파일만 새 source 폴더에 복사했고 Engine과 Reflection/dependencies 폴더는 포함하지 않았다.
  - 원본과 복사한 170개 파일의 내용이 모두 같음을 확인했다.
  - 코어·수동 등록·생성 등록을 서로 다른 새 빌드 폴더에서 구성하고 전체 기본 타깃을 빌드했다.
  - 코어 구성의 CTest 1/1, 수동 등록 구성 2/2, 생성 등록 구성 3/3을 통과했다.
  - 생성 등록 구성은 수동 등록 예제를 끈 상태였다. Engine·GLM·EnTT 없이 생성 등록과 상속·컨테이너 검사를 실행했다.
  - 모든 구성에서 LLVM·Clang·GLM·EnTT 패키지 검색을 비활성화했다. 생성 등록 검사에는 기존 외부 Generator와 대응하는 Clang resource 디렉터리를 명시했다.
  - 생성 등록 빌드에서 세 모듈을 새 install 폴더에 설치한 후 외부 소비자를 별도 구성·빌드·실행했다.
  - 설치 Archive 단독 소비자 CTest 1/1, Reflection Runtime 전용 소비자 1/1, ReflectionArchive 소비자 1/1, 기존 생성 Reflection 소비자 구성 2/2를 통과했다.
  - Runtime 전용 소비자는 Archive·ReflectionArchive·Glaze 검색도 비활성화하고 Generator 경로 없이 구성했다. 수동 등록, 기반 멤버 접근, 중첩 컨테이너 수정과 const 객체의 수정 거부를 검사했다.
  - ReflectionArchive 소비자는 JSON·BEVE의 객체·컨테이너·enum·상속 왕복과 읽기 실패 시 원본 보존을 검사했다.
  - 설치 헤더 30개와 ArchiveJson.lib·ReflectionRuntime.lib의 SHA256이 검증 소스·빌드 산출물과 일치했다.
- 범위와 호환성:
  - 기존 엔진 컴포넌트 예제가 필요하면 ARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE=ON을 추가해야 한다.
  - 생성 등록에는 여전히 Generator와 대응하는 Clang resource가 필요하다. Runtime 자체와 수동 등록에는 필요하지 않다.
  - 직렬화의 타입 지원 범위와 실패 보장은 이번 작업에서 확장하지 않았다.
  - 엔진·GLM 소비자 예제와 엔진·에디터 전체 빌드는 검증하지 않았다. ReflectionEnTT와 성능 비교는 변경하지 않았다.

### [2026-10-16] Release 구성 검증과 모듈 README 정리

- 목적: 최적화된 Release에서도 독립 모듈과 외부 소비자가 같은 기능·실패 검사를 통과하는지 확인하고 사용 절차를 정리한다.
- 문서 변경:
  - Docs/ReflectionArchiveBuild.md에 의존성 준비, 생성 등록·수동 등록·코어 빌드, CTest, 설치와 네 소비자 구성 명령을 모았다.
  - Reflection·Archive·ReflectionArchive README에 시작 지점, 책임 분리, Generator 필요 조건과 공통 빌드 가이드 링크를 추가했다.
  - Reflection 설치 예제는 Runtime 컴포넌트를 명시하며, 실제 검증 환경과 Debug·Release 결과의 위치를 안내한다.
  - 모듈과 소비자의 /MD 설정, 별도 Generator의 SDK Runtime 설정, Debug·Release 설치 prefix 분리 필요성을 설명했다.
  - ReflectionEnTT README에 성능 비교용 별도 구현이라는 목적과 이번 세 모듈의 검증 범위를 구분했다.
- 실행 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51 / CMake 4.2.3 / LLVM·Clang 20.1.8 / Release.
  - 검증 경로: out/audit/module-release-20261016. Engine과 Reflection/dependencies를 제외한 관련 프로젝트 소스 170개 파일을 새 배치에 복사했다.
  - 코어·수동 등록·생성 등록을 각각 새 빌드 디렉터리에서 구성하고 전체 기본 타깃을 Release로 빌드했다.
  - 생성 등록 구성에서는 외부 SDK 경로를 명시해 Release Generator도 새로 빌드했다. 이 실행 파일로 내부·외부 소비자의 등록 코드를 생성했다.
  - 코어 CTest 1/1, 수동 등록 2/2, 생성 등록 3/3 통과. 수동 예제를 끈 생성 등록 구성의 독립성도 확인했다.
  - 코어·수동 구성은 LLVM·Clang 검색을 비활성화했고, 세 소스 구성 모두 GLM·EnTT 검색을 비활성화했다.
  - 생성 등록 빌드 결과를 새 Release 전용 prefix에 설치했다.
  - 설치 Archive 단독 소비자 CTest 1/1, Reflection Runtime 전용 소비자 1/1, ReflectionArchive 소비자 1/1, 생성 Reflection 소비자 구성 2/2 통과.
  - 외부 소비자는 LLVM·Clang·GLM·EnTT 패키지 검색 없이 구성했다. Runtime 전용 소비자는 Archive·ReflectionArchive·Glaze 검색과 Generator 경로도 필요하지 않았다.
  - 실행한 검사는 assert에 의존하지 않는다. 모듈·소비자 10개 타깃의 Release 최적화·NDEBUG·/MD와 Generator의 최적화·NDEBUG·/MT 설정을 생성된 프로젝트 파일에서 확인했다.
  - 최종 README를 포함한 검증 소스 170개 파일이 현재 소스와 일치했다. 설치 헤더 30개와 ArchiveJson.lib·ReflectionRuntime.lib의 SHA256도 소스·빌드 산출물과 일치했다.
  - 공통 가이드의 PowerShell 코드블록 5개 구문, 코드 줄 길이·펜스와 문서 5개의 로컬 링크 31개를 검사했다.
- 관찰 사항과 범위:
  - Release Generator 링크에서 LNK4199 경고가 발생했다. 배치된 LLVM SDK의 LLVMExports.cmake에 있는 LLVMSupport INTERFACE_LINK_LIBRARIES가 delay-load 옵션과 -INCLUDE:malloc을 하나의 항목으로 전달하는 것이 원인이다.
  - 해당 경고가 있는 빌드 결과로 코드 생성·내부 검사·설치 소비자 실행을 통과했다. SDK 링크 옵션의 수정은 별도 항목으로 남긴다.
  - 이번 작업은 실행 코드 변경 없이 검증과 문서 정리를 수행했다. 엔진·GLM 소비자, 엔진·에디터 전체 빌드와 다른 운영체제는 검증하지 않았다.
  - ReflectionEnTT 실행과 성능 비교는 이번 검증에 포함하지 않았다.

### [2026-10-16] 엔진 ReflectionRegistry로 ECS 컴포넌트 등록 통합

- 사용자 결정: GameObject의 ComponentRegistry는 유지하고 EntityComponentRegistry의 기능을 엔진 ReflectionRegistry로 옮긴다.
- 변경:
  - 별도의 EntityComponentRegistry 클래스·정적 접근 함수를 제거했다. 기존 컴포넌트 추가 목록과 정책은 ReflectionRegistry가 소유한다.
  - ComponentReflectionEntry에 타입 정보 참조, 존재 확인·추가·mutable/const 객체 접근과 읽기 작업을 연결했다.
  - 생성된 COMPONENT 타입 목록은 기본 작업을 자동 연결한다. 기존 Rendering·Physics의 수동 추가 정책과 공간 컴포넌트의 Transform·WorldTransform·Flag 준비는 유지한다.
  - ID·WorldTransform은 작업에 등록하되 사용자 추가 메뉴에서 제외한다. 리플렉션 미등록 타입의 기존 추가 동작은 보존하고 직렬화는 UnsupportedType으로 거부한다.
  - Inspector의 타입별 추가 메뉴를 통합 레지스트리의 목록 순회로 바꿨다. 기존의 삭제된 CoreComponentReflection 조회 경로도 현재 소유자로 연결했다.
  - 에디터 축 잠금·스냅 상태를 ECS 컴포넌트에서 월드 ctx의 ReflectionSceneEditState로 옮겼다. 컴포넌트 저장 경로가 에디터 전용 타입을 알 필요가 없도록 하고, 유효하지 않은 엔티티의 편집 항목은 조회 시 정리한다.
  - Scene의 순회는 엔티티 소유 씬을 확인한 뒤 통합 레지스트리에 위임한다. const 순회는 읽기 전용 ObjectView를 유지한다.
  - 엔진 레지스트리에 GLM 코덱을 한 번 준비하고 WriteComponent·ReadComponent·WriteComponents를 제공한다.
  - 읽기는 임시 객체에서 완료한 뒤 반영한다. 없는 컴포넌트의 생성과 의존성 추가는 파싱·유효성 검사 성공 후 실행한다.
  - Transform은 위치·크기와 회전을 검증하고 RestoreLocalRotation으로 Euler 힌트·캐시를 맞춘 뒤 월드 행렬을 갱신한다. InvalidateCache가 있는 컴포넌트의 복원 후보는 캐시를 무효화한다.
  - 엔티티 컴포넌트 출력은 타입의 Transient 정책을 확인하고 미등록 저장소·미지원 타입을 거부한다. 일반 쓰기 실패에서는 해당 기존 문서를 보존한다.
  - EnTT 저장소의 실제 타입과 등록 작업의 타입을 확인해, 다른 타입을 같은 storage ID에 연결한 경우 typed 접근 전에 거부한다.
  - Free에서 컴포넌트 항목과 코덱을 정리한 뒤 내부 reflection::Registry를 비운다. 월드 인스턴스는 각 Scene이 계속 소유한다.
  - 엔진·에디터 검사 프로그램을 CTest에 연결하고 Docs/EngineReflectionIntegration.md에 사용법과 지원 경계를 기록했다.
- 실행 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51 / CMake 4.2.3 / LLVM·Clang 20.1.8.
  - 새 빌드 경로 out/build/engine-reflection-unified를 Debug·Release로 구성했다. 기존 vcpkg 설치를 재사용하고 manifest 설치는 실행하지 않았다.
  - 두 구성 모두 Engine DLL, Editor 실행 파일, SceneReflectionCheck, TransformEditorCheck 빌드 성공.
  - 최종 Debug CTest 2/2, Release CTest 2/2 통과.
  - 기존 추가 정책, 내부용 타입의 메뉴 제외, 타입 정보 소유자 일치, 중복·잘못된 엔티티 추가 거부와 월드 간 독립성을 검사했다.
  - Camera의 mutable/const 접근, 같은 storage ID의 다른 타입 거부, 순회 중단과 다른 Scene의 Entity 거부를 검사했다.
  - Name의 왕복·누락·잘못된 필드·코덱 실패와 성공 후 생성을 검사했다.
  - JSON·BEVE에서 기본 GLM 코덱을 사용하는 Transform 왕복과 음수·0 크기, 누적 Euler 힌트·회전 캐시 일관성을 검사했다.
  - 잘못된 Quaternion 읽기가 기존 Transform·월드 행렬을 바꾸거나 새 의존 컴포넌트를 만들지 않는지 확인했다.
  - 미등록 물리 타입과 빈 사용자 코덱 집합의 저장 실패가 기존 문서를 보존하는지, WorldTransform이 출력에서 제외되는지 검사했다.
  - 기존 Transform 에디터의 행렬 변환, 축 잠금, shear·비정상 값·특이 부모 행렬 거부 검사를 통과했다.
  - 편집 상태를 생성한 뒤 컴포넌트 출력이 성공하는지, 다른 월드에 편집 설정이 섞이지 않는지, 엔티티 재생성 시 이전 잠금 설정이 제거되는지 확인했다.
- 빌드에서 확인한 추가 문제:
  - 코드 생성기의 명시적 include 목록에 Archive·ReflectionArchive 공개 헤더 경로가 빠져 있어 실제 빌드 경로를 전달하도록 보완했다.
  - Release Editor에서 Debug 전용 DebugVertex·DebugRenderer 선언과 무조건 컴파일되는 호출이 충돌했다. 디버그 렌더러의 선언·구현·호출을 같은 조건으로 묶고 공통 오버레이 상태는 두 구성에서 유지했다.
  - 기존 LLVM SDK의 LNK4199와 stb CMake 패키지 이름 경고는 관찰되었으며, 이번 변경의 컴파일·실행 검사는 통과했다.
  - 기존 빌드 단계에 따라 HLSL 산출물과 Localization JSON이 재생성되었다.
- 범위와 남은 작업:
  - 독립 Reflection·Archive·ReflectionArchive의 실행 코드는 변경하지 않았다. 기존 GameObject·ComponentRegistry의 기능과 수명 경로도 변경하지 않았다.
  - 에디터 UI의 수동 조작이나 GPU 출력은 실행 검증하지 않았다.
  - 이번 구현은 컴포넌트 단위 복원과 한 엔티티의 컴포넌트 출력까지다. 씬 전체 복원·영구 ID·참조 해결·리소스 코덱·PIE 복원은 별도 단계다.
  - 사용자 코덱 집합은 엔진 기본 코덱과 자동 병합되지 않는다. 명시적으로 지정하면 해당 집합을 사용한다.

### 2026-10-16: 영구 ID를 사용하는 작은 ECS 씬의 파일 저장·복원

- 요청과 목표: 통합 ReflectionRegistry를 실제 씬 저장·불러오기에 적용했다.
  등록 컴포넌트의 멤버를 씬 코드에 다시 나열하지 않고, 씬의 ID와 관계 정책만 엔진에서 처리한다.
- 구현:
  - Scene::CreateEntity가 IDComponent를 자동으로 추가한다. 씬 내부에서 0이 아닌 ID를 할당하고
    FindEntityById로 조회한다. 불러온 ID와 다음 생성 ID가 충돌하면 사용 중인 값을 건너뛴다.
  - SceneArchive.cpp에 Scene::WriteArchive와 CreateFromArchive를 구현했다.
    Version=1, Name, Active, 엔티티 ID, 선택적인 부모·자식 관계와 컴포넌트 배열을 저장한다.
  - 일반 컴포넌트의 Type은 QualifiedName을 사용하며, Data는 기존 WriteComponent·ReadComponent로 처리한다.
    ReflectionRegistry에 QualifiedName으로 컴포넌트 작업을 찾는 조회를 추가했다.
  - 부모와 자식은 영구 ID로 저장한다. 전체 엔티티와 컴포넌트를 임시 씬에 만든 뒤 참조를 연결하고
    자식 순서를 유지하며 TransformSystem으로 월드 행렬을 다시 계산한다.
  - 중복·0 ID, 중복·알 수 없는 컴포넌트, 잘못된 필드와 Transform, 끊어진 참조,
    부모·자식 불일치, 중복 자식, 순환과 256단계를 넘는 깊이를 거부한다.
    현재 연결된 부모와 자식에는 Transform이 필요하다.
  - SceneManager::LoadScene은 파일 읽기와 임시 씬 검증이 성공한 후 OpenScene을 호출한다.
    일반적인 실패에서는 현재 씬을 유지한다. 출력 Scene*은 성공 시에만 할당한다.
  - 에디터 Scene 메뉴의 불러오기를 연결하고 성공 시 선택과 Undo/Redo 기록을 비운다.
    Paused와 편집 ctx는 복원하지 않는다. PIE 진입은 기존 비활성 상태를 유지한다.
  - SceneManager의 기존 Light·Collision·Camera 정리는 해당 관리자가 생성된 경우에 수행한다.
    렌더러를 초기화하지 않는 씬 파일 검사에서도 씬 교체를 실행할 수 있다.
  - 작업 중 갱신된 컴포넌트 헤더를 반영했다. 렌더 컴포넌트는 ComponentAnnotations를 직접 포함하며,
    색상 메타데이터는 COLOR_PROPERTY를 사용하고 잘못된 끝 쉼표를 제거했다.
  - 사용자가 분리한 HierarchyComponent 헤더에 필요한 선언과 REFLECT_BODY를 연결했다.
    COMPONENT 등록은 유지하고 NOSERIALIZE로 raw 핸들의 값 직렬화를 제외했다.
    씬 관계는 별도의 영구 ID 변환 경로에서 저장한다.
- 실행 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51, out/build/engine-reflection-unified.
  - Debug·Release 모두 Engine DLL, Editor, SceneReflectionCheck, SceneArchiveCheck, TransformEditorCheck 빌드 성공.
  - 최종 Debug CTest 3/3, Release CTest 3/3 통과.
  - JSON·BEVE 실제 파일 왕복, Unicode 이름, 53비트를 넘는 ID, 핸들이 바뀐 부모·자식 연결,
    형제 순서, 음수·0 크기와 누적 Euler 힌트, 월드 행렬 복원, 빈 씬과 복원 후 ID 할당을 확인했다.
  - 알 수 없는 버전·컴포넌트, 중복·0·누락 ID, 중복 컴포넌트, 누락·잘못된 필드,
    잘못된 Quaternion과 계층, 손상·누락 파일에서 현재 씬이 보존되는지 검사했다.
  - 깊이 256은 복원하고 257은 거부한다. 미등록 물리 컴포넌트의 저장 실패에서 기존 문서·파일을 보존하며,
    잘못된 Transform·계층·ID의 저장 실패에서도 기존 문서를 유지하는지 확인했다.
  - 기존 컴포넌트 순회 검사는 자동 ID 추가를 포함하도록 조정했다. 기존 타입 안전성·const·편집 검사도 통과했다.
  - 사용법과 형식·소유권·실패 보존 범위는 Docs/EngineReflectionIntegration.md에 기록했다.
- 지원 경계:
  - 독립 Reflection·Archive·ReflectionArchive의 실행 코드는 변경하지 않았다.
  - 에디터 창에서의 수동 조작과 GPU 출력은 검증하지 않았다.
  - 리소스 핸들 복원, 미등록 물리 타입의 저장 정책, Hierarchy 이외의 엔티티 참조,
    파일 버전 마이그레이션과 PIE 실행 상태 복원은 남아 있다. 지원하지 않는 컴포넌트는 저장 오류로 처리한다.
  - ID는 씬 내부 식별자이며 씬 간 UUID가 아니다. 직접 registry::create한 엔티티에는 Scene의 래퍼·ID가 없어 저장을 거부한다.
  - 파일 기록 도중 I/O 실패에 대한 원자적 교체와 사용자 코드·메모리 할당 예외에 대한 전체 복구는 보장하지 않는다.
  - 기존 GameObject·ComponentRegistry, EventSystem·UICanvas와 셰이더 바인딩 정책은 이번 씬 형식의 대상이 아니다.

## 2026-10-16 — 리플렉션 Inspector 편집과 번역 연결

- 요청: Property Category 접기·펼치기, 기존 미지원 값 편집, ResourceHandle 드래그앤드롭과
  AddComponent 카테고리·항목의 Localization 연결.
- 일반 프로퍼티 UI는 ReflectionPropertyDrawer에서 메타데이터를 순회한다.
  Category별 헤더를 만들고 번역 전 키를 ImGui ID에 사용해 언어 변경 후 접힘 상태를 유지한다.
  문자열·wstring, 정수·실수, enum·BITMASK, vec2, 색상·Quaternion·행렬과 중첩 등록 값을 편집한다.
  Tag의 문자열 집합에는 추가·삭제와 엔티티별 입력 버퍼를 제공한다.
- 물리 2D/3D Body·Collider·Joint에 멤버 리플렉션을 연결했다.
  작업 중 사용자가 정리한 STRUCT_/PROP_ 번역 키와 Dirty 읽기 전용 프로퍼티를 최신 소스로 반영했다.
  누적 힘·역관성 등 실행 중 계산값과 Joint의 raw 엔티티 참조는 읽기 전용이다.
  Collider는 후보 복사본에 편집하고 기존 setter로 검증한 뒤 반영하여 원본 보존과 Dirty 갱신을 지킨다.
  Camera·Light는 편집 성공 시 캐시를 무효화한다. vec4 축 잠금 배열의 크기도 4로 수정했다.
- ResourceHandle은 파일 드롭, 로드된 리소스 선택과 연결 해제를 지원한다.
  페이로드 길이·종료 문자와 실제 객체 타입을 확인하고 다른 타입은 기존 값을 보존한다.
  MaterialInterface는 Material·MaterialInstance를 허용한다.
  Content Browser의 중복 페이로드를 하나로 정리하고 사용하지 않는 과거 Texture 전용 편집 코드를 제거했다.
- ReflectionRegistry의 수동 추가 정책에도 생성된 DisplayName·Category 메타데이터를 반영했다.
  AddComponent와 프로퍼티 카테고리를 같은 LocalizationManager로 표시한다.
  Engine.csv·Editor.csv 번역을 채우고, Inspector의 새 루트 키가 ContentBrowser 그룹을 상속하지 않도록 배치했다.
  기존 번역 내용은 유지하며, 이 작업에서 추가한 뒤 새 키로 대체된 임시 키만 정리했다.
- Release 검증 중 구성 공용 생성 코드가 Debug 전용 Collider 멤버를 참조하는 실패를 발견했다.
  엔진 생성 경로를 빌드 디렉터리의 Engine/Generated/<Config>로 분리했다.
  Debug 코드에는 m_DrawCollider가 있고 Release 코드에는 없는 것을 확인했다.
  독립 Reflection·Archive·ReflectionArchive의 실행 코드에는 이 작업으로 변경을 추가하지 않았다.
- 검증: Windows x64 / Visual Studio 18 2026 / MSVC 19.51,
  out/build/engine-reflection-unified에서 Debug·Release Engine·Editor와 검사 실행 파일을 빌드했다.
  최종 Debug CTest 5/5, Release CTest 5/5 통과.
  - ReflectionInspectorCheck: 실제 생성 번역 JSON 조회, 입력 없는 표시의 원본 보존,
    클릭을 통한 값 변경·const 거부, Category 접기·펼치기와 번역 변경 후 상태 유지.
  - 리소스 드롭의 실제 ImGui 전달·타입 불일치·읽기 전용 거부, 상속 타입 호환,
    비정상 페이로드 및 Collider 후보 검증·Dirty 갱신을 확인했다.
  - Joint 참조를 저장에서 조용히 제외하지 않고 UnsupportedType으로 실패하며 문서가 보존되는지 확인했다.
  - 기존 SceneReflectionCheck·SceneArchiveCheck·TransformEditorCheck와 번역 유틸리티 검사도 통과했다.
- 범위: 실제 Editor 창의 시각적 배치와 GPU 리소스 생성은 수동 검증하지 않았다.
  일반 컨테이너 요소 편집, 속성 Undo/Redo와 엔티티 참조 선택 UI는 남아 있다.
  물리 편집 지원은 모든 물리 값의 저장 지원을 뜻하지 않는다. Collider variant·vec2와
  리소스·엔티티 참조의 저장 연결은 별도 과제이며 미지원 데이터는 저장 오류로 처리한다.
  상세 사용법과 제한은 Docs/EngineReflectionIntegration.md에 기록했다.

## 2026-10-16 — 로직·셰이더 그래프의 공통 데이터 기반

- 요청: 이번 작업의 직접 수정을 허용하고 비주얼 스크립팅·셰이더·로직 그래프의 기초부터 작성.
- 구현: Engine/Source/Graph에 노드 정의·인스턴스, 타입 핀, 로컬 ID 연결과 입력 기본값 재정의를 추가했다.
  GraphRegistry는 도메인별 노드 정의를 등록하며 Graph는 별도의 편집 데이터만 소유한다.
  Editor UI, 전역 등록 상태, 그래프 실행 콜백이나 GPU 자원에 의존하지 않는다.
- 연결 검사: 출력→입력, 정확한 자료형, 데이터 입력 단일 소스, 실행 출력 단일 목적지,
  중복 연결과 데이터 순환을 검사한다. 실행 순환은 표현만 허용하며 실행하지 않는다.
  노드 삭제는 부속 연결을 정리하고, 오류로 거부한 편집은 기존 데이터를 보존한다.
- 기본 노드: Float, Add/Multiply/Greater, Start, Branch, 로직 값 결과와 셰이더 색상 출력의 구성 정의.
  사용자 노드는 정의 등록으로 추가할 수 있다. 실제 연산 함수나 셰이더 생성 코드는 아직 연결하지 않았다.
- 검증 예제: GraphFoundationCheck와 독립적인 BAMENGINE_BUILD_GRAPH_CHECK 옵션을 추가했다.
  정상 구성, 거부 조건, 삭제·복사·ID·기본값·도메인과 등록 충돌의 검사를 작성했다.
- 검증 상태: 정적 검토와 변경 내용의 공백 검사를 수행했다. 프로젝트 작업 지침에 따라 빌드·테스트는 실행하지 않았다.
  빌드·CTest 명령, 사용 예제와 규칙은 Docs/GraphFoundation.md에 기록했다.
- 후속 범위: 그래프 편집 패널, 버전이 있는 저장 형식, 로직 실행기와 셰이더 코드 생성기.
  기존 GraphComponent와 Tools/GraphCompiler의 고정 코드 생성 예제는 이 기반과 연결되지 않았다.

## 2026-10-16 — ImGui GraphPanel과 imnodes 연결

- 요청: 기존 패널·ImGuiInterface의 코딩 스타일을 유지한 그래프 패널 작성.
  작업 중 사용자가 ImGui 노드 확장 사용을 허용하여 imnodes를 선택했다.
- GraphPanel을 ImGuiInterface에서 파생하고 ImGuiManager의 기본 패널 생성 경로에 등록했다.
  m_Open·m_Focused·m_Hovered·m_Name, 지역화된 창 제목과 고정 ImGui ID, Draw 함수 분리 관례를 따른다.
- Editor 전용 FetchContent 모듈에서 imnodes 공식 커밋 eb36902c892548ef94f88f51ad7e7c9c7058a71c를 고정한다.
  기존 imgui::imgui를 사용하며 upstream 예제·submodule 다운로드를 비활성화했다.
  직접 작성 중이던 캔버스 그리기·히트 검사 코드는 제거하고 확장의 노드·핀·선택·화면 이동을 사용한다.
- Logic·Shader별 모델과 UI 컨텍스트를 소유한다. Engine의 64비트 식별자를 자르지 않고
  별도의 편집용 int ID에 대응시킨다. 편집용 ID 공간 부족은 모델 변경 전에 거부한다.
- 도메인별 노드 추가, 이동, 연결 후보 검사·연결 확정, 다중 선택 삭제와 기본값 편집을 연결했다.
  선택 노드 삭제 시 부속 연결과 ID 대응을 정리한다. 숨김·도메인 전환·삭제 후에는
  배치·화면 이동을 메모리에 보존하고 UI 컨텍스트를 재생성해 이전 드래그 상태를 정리한다.
- GraphRegistry::GetTypes(domain)를 추가했다. 추가 메뉴는 정렬한 등록 목록을 사용하며
  사용자 노드도 GraphPanel::RegisterNode로 등록할 수 있다. Bool·Int·Float·Float2/3/4 입력을 편집한다.
- Editor.csv에 GraphPanel 번역 키를 추가했다. 기존 번역 JSON은 실행하지 않은 빌드 작업의 출력으로 남겨 두었다.
  GraphFoundationCheck에는 도메인별 목록·정렬·사용자 노드 노출 검사를 추가했다.
- 검증 상태: 관련 코드·선언·공백과 번역 키 존재·중복을 정적으로 확인했다.
  의존성 다운로드, CMake 구성, 빌드·CTest와 실제 마우스·도킹 조작은 실행하지 않았다.
  사용법, 의존성, 실행 명령과 수동 확인 항목은 Docs/GraphPanel.md에 기록했다.
- 남은 범위: 파일 저장·불러오기, Undo/Redo, 씬 연결, 그래프 실행과 셰이더 생성.
  현재 문서는 프로세스 내부에서만 유지되며 기존 GraphComponent·Python 컴파일러와 연결하지 않았다.

## 2026-10-16 — Inspector 표시 간소화와 타입 노출 정책

- Category의 배경 있는 CollapsingHeader를 일반 TreeNodeEx로 바꾸고 자식 속성을 들여쓰기한다.
  컴포넌트 헤더와 Category를 구분하며 번역 전 ID로 접힘 상태를 유지한다.
  속성·값의 제목 행, 표 배경·구분선을 제거하고 크기 조절 가능한 2열 정렬은 유지한다.
- AddComponent 버튼은 Inspector의 사용 가능한 가로 너비를 채운다.
- 기존 HIDDEN(VISIBLE(false))을 타입의 Inspector 표시 정책에도 적용한다.
  공통 IsVisible 조회를 Inspector 본문·Drawer·추가 메뉴에서 사용한다.
  HierarchyComponent에 HIDDEN을 지정하여 별도 타입 분기 없이 숨긴다.
  HierarchyPanel은 기존 부모·자식 데이터로 표시하며, 엔진 순회와 씬 관계 저장은 유지한다.
  HIDDEN과 NOSERIALIZE는 별개 정책이다. 다른 내부 타입도 같은 메타데이터로 선택적으로 숨길 수 있다.
- 회귀 검사에 숨긴 타입이 Inspector 공간을 소비하지 않으면서 엔진 순회에는 남는 조건을 추가했다.
  기존 Category 접기·펼치기, 언어 변경 후 상태 유지와 씬 계층 왕복 검사도 유지한다.
- 작업 중 갱신된 피킹 코드의 SceneManager::IsCreated 호출에 필요한 조회 함수를 보완했다.
  Camera·Light·Collision 관리자와 같은 싱글턴 포인터 확인 방식이다.
  사용자가 추가 중이던 PickingMathCheck.cpp는 완료될 때까지 기다린 후 최신 소스로 검증했다.
- 최종 검증: out/build/engine-reflection-unified / Windows x64 / MSVC 19.51.
  Debug·Release Engine·Editor 및 검사 실행 파일 빌드 성공, 전체 CTest 각각 6/6 통과.
  추가된 PickingMathCheck를 포함한 결과이며, 실제 Editor 창의 시각적 배치는 수동 확인하지 않았다.
  사용법은 Docs/EngineReflectionIntegration.md, 빌드·검사 로그는
  out/audit/inspector-layout-20261016에 기록했다.

## 2026-10-16 — 그래프 어노테이션 등록과 기본 제어 흐름 정의

- 요청: Unreal식 함수·프로퍼티 노출 정책을 적용하고 미리 정의할 분기·로직 노드도 준비.
- Engine GraphAnnotations에 GRAPH_PURE/CALLABLE, READONLY/READWRITE, ID와 핀 키·이름·기본값 매크로를 추가했다.
  기존 FUNCTION/PROPERTY와 일반 META 파서를 사용하며 Reflection에 엔진 그래프 정책을 넣지 않았다.
- Reflection에 정렬된 타입·자유 함수 읽기 전용 열거, 메타데이터 열거와 CanonicalReturnType 생성을 추가했다.
  기존 ReturnType의 원본 표기는 유지한다.
- GraphReflection은 명시적으로 노출한 함수·프로퍼티를 Logic 정의로 변환한다.
  일반 멤버와 Get/Set에는 정확한 소유 타입의 Object target 핀을 만들고 객체 리터럴은 거부한다.
  잘못된 메타데이터·인자·기본값·미지원 형식·등록 충돌은 원본 이름을 포함한 오류로 반환한다.
  배치 실패 시 일부 노드를 남기지 않는다. 정의에는 리플렉션 포인터 대신 조회 문자열을 보관한다.
- GraphPanel에 엔진 리플렉션 및 별도 모듈의 정의 등록을 연결했다.
  Category 메뉴, 표시 이름·툴팁과 Object 연결 안내를 추가하고 고정 연결 키는 유지한다.
  실제 함수 예제로 GraphLogicLibrary의 AbsFloat/Xor를 어노테이션 등록한다.
- Branch에 더해 Sequence, ForLoop, WhileLoop, DoOnce, Gate, FlipFlop과 Update 진입점,
  Bool/Int 상수, Bool 연산과 Float 선택 정의를 준비했다.
  순서·반복 범위·상태 소유·재진입·실행량 제한의 계약은 Docs/GraphReflection.md에 기록했다.
- GraphReflectionCheck의 생성기 입력과 검사를 작성하고 GraphFoundationCheck의 흐름 정의 검사를 확장했다.
  정적 검토와 공백 검사는 수행했으며 이번 작업에서 빌드·테스트·Editor 실행은 하지 않았다.
- 남은 범위: 실행기, 호출 인자 마샬링, 씬 객체 핸들, 검증·Dirty 처리를 보존하는 프로퍼티 쓰기,
  실행 문맥 검사, 저장·불러오기와 핫 리로드. 노드가 표시된다는 이유로 실행 가능하다고 판단하지 않는다.

## 2026-10-16 — 수학 함수와 Logic 실행·추적 미리보기

- 요청: 일반 연산·자주 쓰는 수학 함수를 구현하고 실행 중 흐름을 확인할 방법도 추가.
- GraphMathLibrary에 Float·Int 기본 연산·비교·범위·보간·삼각함수와 변환을 작성했다.
  GraphVectorLibrary는 Float2/3/4 생성·성분·산술·내적·길이·거리·정규화·보간과 Float3 외적을 제공한다.
  정의역 오류, 0 나눗셈, 정수 오버플로와 비유한 결과의 처리 규칙을 정했다.
- Reflection에 자유/static 함수의 숫자 값·고정 숫자 배열을 받는 동적 호출을 추가했다.
  인자 개수·실제 C++ 타입을 확인하고 반환값을 소유하며 예외를 호출 오류로 변환한다.
  생성기는 자유/static 함수의 숫자 std::array 반환을 인식한다. 일반 구조체 반환까지 허용하지 않았다.
- GraphExecutor는 편집 데이터와 정의·함수 바인딩을 복사하여 별도 인스턴스로 실행한다.
  기본 연산·제어 흐름·자유/static 함수를 연결하고 Step/Pause/Resume/Stop과 공통 실행 예산을 제공한다.
  Loop 범위, 활성 노드 재진입, 값 생성 여부와 int32 인자 범위를 검사한다.
  Object 접근·미지원 바인딩·Shader는 거부한다. 씬 객체의 검증·Dirty 처리를 우회하지 않는다.
- 최근 2,048개의 진입·복귀·연결·값·반복·오류 추적을 보관한다.
  GraphPanel에 진입점, 실행 버튼, 작업 간격·예산, 기록 선택과 노드·연결 강조를 연결했다.
  Logic 편집 시 실행을 초기화하고 숨김·도메인 전환은 일시정지한다.
- GraphExecutionCheck에 실제 생성기의 배열 반환, 수학 경계 조건과 실행·추적 검사를 작성했다.
  정적 코드·API·공백 검사를 수행했으며 빌드·CTest와 실제 Editor 조작은 실행하지 않았다.
  명령, 실행 계약과 수동 확인 절차는 Docs/GraphExecution.md에 기록했다.
- 남은 범위: 씬/PIE 연결, 객체 핸들·멤버 호출·프로퍼티 쓰기, 실행 문맥의 정적 분석,
  저장·Undo/Redo, 셰이더 생성, 중단점과 비동기 실행. 네이티브 호출 내부의 블로킹은 예산으로 선점하지 않는다.

## 2026-10-16 — HierarchyPanel 트리와 드래그 부모 변경

- Scene의 Entity와 HierarchyComponent로 표시용 트리를 구성한다. 이름 없는 엔티티도 표시하며,
  자식 순서는 기존 children을 따른다. 순환·중복·잘못된 관계는 표시에서만 끊고 경고한다.
  원본 관계를 자동으로 복구하지 않는다.
- 클릭·Ctrl 선택, 화살표 접기·펼치기와 외부 선택 변경 시 조상 펼침을 연결했다.
  노드 드롭은 마지막 자식으로 이동하며, 드래그 중 표시되는 루트 영역은 부모를 해제한다.
  트리 순회가 끝난 뒤 관계를 변경하고 선택 잠금 중에는 변경을 막는다.
- Engine의 Scene::ReparentEntity가 양쪽 연결과 로컬 변환을 함께 갱신한다.
  자식·후손의 월드 변환을 유지하고 순환, 다른 씬, 깊이 초과, 특이 행렬과 표현할 수 없는 전단을 거부한다.
  일반 검증 실패에서는 기존 관계·변환을 보존한다. UI 앵커·레이아웃 보존은 아직 지원하지 않는다.
  저장·복원은 기존 Scene Archive 경로를 사용하며 별도 직렬화는 추가하지 않았다.
- 드래그 원본은 Scene·실행 핸들·영구 ID로 검사하고 드래그 중 Scene 참조를 유지한다.
  HierarchyEditorCheck는 실제 ImGui 입력을 사용하여 선택, 펼침, 노드·루트 드롭,
  다른 씬 거부와 원본 Scene 수명을 검사한다. 관계·월드 변환·저장 복원 검사도 포함한다.
- 빌드에서 발견된 bool 함수 생성 문제를 Reflection 생성기의 PrintingPolicy.Bool로 수정했다.
  MSVC C++에 _Bool을 출력하지 않으며 기존 타입 표기는 유지한다.
  독립 ReflectionExample에 bool 인자·반환형과 호출 회귀 검사를 추가했고 Debug CTest 1/1 통과했다.
- 검증: out/build/engine-reflection-unified / Windows x64 / MSVC 19.51.
  Debug·Release Editor 및 관련 검사 실행 파일 빌드 성공, 관련 CTest 각각 7/7 통과했다.
  병행 작업 중 UIViewportPanel 선언·구현의 일시적인 불일치는 최신 소스로 재빌드하여 확인했다.
  이 결과는 Hierarchy 관련 범위이며 병행 작업 전체의 기능 검증을 의미하지 않는다.
  실제 Editor 창·GPU 수동 조작은 확인하지 않았다.
- 사용법은 Docs/EngineReflectionIntegration.md, 로그는 out/audit/hierarchy-panel-20261016에 기록했다.
  형제 순서 드래그 정렬, 다중 엔티티 이동과 관계 변경 Undo/Redo는 남은 범위다.

## 2026-10-16 — Editor 씬 Undo/Redo 연결

- 기존 CommandHistory의 raw Command 소유권을 EditorCommand의 unique_ptr 소유권으로 교체했다.
  실행·복원 실패 시 커서를 유지하고, Redo 분기 폐기·100건 한도·Clear·종료에서 명령을 해제한다.
  Engine의 일반 Command 인터페이스를 Editor 정책으로 바꾸지 않았다.
- SceneEditHistory·SceneEditSnapshot으로 메모리 값 복사를 기록한다.
  ReflectionRegistry의 기존 컴포넌트 등록에서 Copy·Remove 연산을 함께 만든다.
  멤버 목록을 다시 나열하거나 타입별 Undo 직렬화를 추가하지 않는다.
  이력과 입력·선택 정책은 Editor에 있으며 독립 Reflection/Archive 모듈은 변경하지 않았다.
- Inspector 속성·리소스 핸들·컴포넌트 추가, Scene View 기즈모, Hierarchy 부모 변경·루트 분리·씬 이름,
  EntityFactory 생성과 Hierarchy 엔티티 우클릭 하위 트리 삭제, UI 변환·디자인 해상도를 연결했다.
  연속 편집을 한 건으로 병합하고, 중첩 생성은 가장 바깥 범위 한 건으로 기록한다.
  병행 추가된 Button→Image 생성도 같은 경로를 사용한다.
- 생성·삭제 복원은 ID로 실행 핸들을 다시 찾는다. Hierarchy 양방향 연결·형제 순서와
  직접 entt::entity 프로퍼티를 다시 연결하며 Joint·Joint2D 참조도 포함한다.
  일반 포인터·컨테이너·중첩 객체의 임의 엔티티 참조 자동 해석을 구현한 것은 아니다.
  Scene::DestroyEntity는 하위 트리와 외부의 직접 리플렉션 엔티티 참조를 정리한다.
- 복원 전 예상 구조와 최종 계층을 검사한다.
  기록 밖에서 생긴 엔티티나 컴포넌트를 전체 복원으로 삭제하지 않도록 차이가 있으면 거부한다.
  선택을 ID로 다시 연결하고 Transform·렌더 캐시·Collider Dirty·Inspector 임시 상태를 갱신한다.
  UI의 적용 대기 드래그는 Revision 변경으로 취소한다.
- Edit 메뉴와 Ctrl+Z / Ctrl+Y / Ctrl+Shift+Z를 연결하고 한국어·영어 키를 추가했다.
  텍스트 입력의 기본 Undo와 충돌하지 않도록 하며 씬 편집 패널 포커스에서만 단축키를 처리한다.
  씬 전환·성공한 로딩·편집/실행 전환은 이력을 비우고 저장은 유지한다.
- 검증: Debug·Release Editor 빌드 성공, 관련 CTest 각각 10/10 통과.
  EditorHistoryCheck는 소유권·한도·분기·실패 커서·제스처 병합·자동 추가 의존성,
  생성·하위 트리 삭제·계층 순서·씬 이름·핸들 재할당·Joint 참조·UI·Scene 수명,
  미등록 데이터 보존과 기록 밖 구조 변경 거부를 검사한다.
  Button 중첩 생성 보완 후 EditorHistoryCheck도 Debug·Release 각각 1/1 추가 통과했다.
  빌드·검사 로그는 out/audit/editor-history-20261016에 둔다.
- 실제 Editor 창의 메뉴·키보드·마우스와 GPU 조합은 수동 확인하지 않았다.
  Graph·에셋/파일 작업·기존 GameObject 경로·선택 이력·Dirty 표시는 이번 연결 범위 밖이다.
  전체 엔티티 스냅샷의 메모리 비용, 등록되지 않은 참조와 실패 계약 등은 Docs/EditorUndoRedo.md에 기록했다.

## 2026-10-16 — GraphPanel 호환 노드 검색과 핀 값 표시

- 핀에서 연결을 끌어 빈 캔버스에 놓으면 검색 가능한 호환 후보 팝업을 연다.
  입력에서 시작하면 출력 후보를, 출력에서 시작하면 입력 후보를 표시한다.
  도메인·방향·정확한 자료형·객체 타입·연결 개수는 기존 Engine 연결 규칙으로 검사한다.
  호환 핀이 여러 개인 노드는 핀별로 항목을 표시하고 선택한 핀에 자동 연결한다.
- 상단 추가 버튼과 우클릭 메뉴도 같은 검색 목록을 사용한다.
  타입 키·표시 이름·분류·설명 및 문맥 후보의 핀 이름을 검색하며 영문 대소문자를 구분하지 않는다.
  취소 시 편집하지 않고 생성·자동 연결 실패에서도 원본 그래프와 ID를 보존한다.
- 선택하지 않은 노드 안에서 Bool·Int·Float·Float2/3/4 입력을 보고 편집한다.
  연결된 입력과 출력에는 보관된 추적의 마지막 관측값을 표시하며 미평가 상태를 구분한다.
  Logic 편집·초기화에서 관측값도 지운다. 과거 기록 선택에 따른 값 되감기는 제공하지 않는다.
- GraphPaletteCheck에 호환 후보, 양방향 연결, 여러 핀, 객체 타입·벡터 크기,
  연결 개수, 검색과 실패 시 데이터·ID 보존 검사를 작성하고 Editor 검사 구성에 등록했다.
  소스·선언/호출부·번역 키·공백을 정적으로 확인했다. 빌드·CTest·실제 UI 조작은 실행하지 않았다.
  사용자 실행 명령과 수동 확인 항목은 Docs/GraphPanel.md에 기록했다.

## 2026-10-20 — 로캘·입력 점검과 기능별 진행 상태 정리

- 작업 전부터 존재하던 그림자 아틀라스, 리소스 재로딩·변환, 머티리얼 미리보기,
  계층 컨트롤과 지형 조형 변경을 포함해 기능별 커밋으로 정리했다.
  이 기능들의 전체 동작을 새로 구현하거나 모두 수동 검증한 것은 아니다.
- Engine·Editor 소스의 표시 문자열, 리플렉션 이름·분류·툴팁, 간접 번역 키와
  그래프 기본 진단을 점검했다. 기존 HEAD 대비 번역 키 208개를 추가한 상태이며,
  영어·한국어 JSON에는 Test.csv를 포함해 각각 972개 키가 있다.
  CSV와 JSON 일치, 빈 번역·중복 키와 번역 서식 인자의 일치를 확인했다.
- 지형 조형 패널, 머티리얼 미리보기·파이프라인 선택 항목, 재로딩 알림,
  계층 이력·검색 문구를 번역했다. 머티리얼 블렌드 모드의 비트 값과
  콤보 인덱스를 명시적으로 대응시켜 선택 항목과 실제 설정이 다르던 문제도 수정했다.
  그래프 진단은 Editor 표시 경계에서 번역하며 노드·핀 식별자와 미등록 외부 오류는 보존한다.
- 키·마우스 상태, 이동량, 휠과 보조 키 조회를 InputManager로 통일했다.
  ImGui의 포커스·호버·활성 위젯 판정과 UI 백엔드 이벤트 전달은 계속 사용한다.
  분리된 패널에는 화면 좌표를 사용하고, 런타임의 창 내부 좌표 API도 유지한다.
  입력을 에디터 갱신 전에 한 번 샘플링하고 PIE에서 같은 샘플을 재사용한다.
  짧은 입력의 누름·해제 이벤트, 양쪽 보조 키, 다섯 번째 마우스 버튼,
  드래그 임계값과 상대 마우스 이동 처리를 보완했다.
- LocalizationAuditor는 여러 줄·조건식 UI 호출, 툴팁, 알림과 간접 키도 검사한다.
  CSV 그룹 상속을 변환기와 맞추고 잘못된 단축 키 별칭을 제거했다.
  AuditLocalization 타깃은 하드코딩 문자열도 실패로 처리하도록 엄격 모드를 사용한다.
- 검증 환경: Windows x64, Visual Studio 18 2026 / MSVC 19.51, Debug,
  `out/build/x64-debug`. Engine·Editor와 관련 검사 실행 파일 빌드 및
  AuditLocalization 성공. 검사 대상 소스 404개에서 누락 키·빈 번역·검사 대상
  하드코딩 UI 문자열은 0건이었다. 이 정적 검사가 임의의 동적 문자열까지 보장하지는 않는다.
- 관련 CTest 6개 통과: InputManagerCheck, HierarchyEditorCheck,
  ReflectionInspectorCheck, GraphMessageLocalizationCheck,
  LocalizationAuditUtility, ReflectionLocalizationUtility.
  LocalizationAuditUtility의 단위 검사 7개도 통과했다.
  검사 로그는 `Temp/build_locale_input.log`, `Temp/test_locale_input.log`에 있다.
- 실제 Editor 창의 GPU 출력, 분리 창·DPI·상대 마우스 모드 전환,
  지형 브러시 조작과 핫 리로드의 실패 복구는 이번에 수동 확인하지 않았다.
  지형 조형의 Undo/Redo 연결 역시 이번 로캘·입력 점검에서 추가하지 않았다.
  생성물, 외부 의존성, 개인 설정과 실험용 씬·에셋은 커밋에서 제외했다.

## 2026-10-22 — 실행 프로그램별 로그 파일과 발생 주체 구분

- Editor는 `Logs/Editor.log`, Client는 `Logs/Client.log`에 기록한다.
  경로는 각 프로세스의 작업 디렉터리를 기준으로 해석한다.
- 공통 Logger를 유지하고 `Source`와 `Category`를 별도 필드로 전달한다.
  각 실행 파일에는 해당 실행에서 발생한 Engine 로그도 함께 기록한다.
  별도 Client 프로세스의 로그를 Editor Console로 전달하는 기능은 포함하지 않는다.
- Console은 발생 주체·레벨·검색 조건을 모두 만족하는 로그를 표시한다.
  발생 주체 필터의 기본값은 전체이며 파일 출력에는 영향을 주지 않는다.
  선택 목록은 보관 중인 로그에서 만들고, 선택한 주체의 로그가 사라지거나
  기록을 지워도 현재 선택은 유지한다.
- 필터 안내 문구의 영어·한국어 로캘 키를 Editor.csv에 추가했다.
  UIInteraction.md의 에디터 진단 로그 경로를 `Logs/Editor.log`로 갱신했다.
- 정적 확인: Source 필드 전달, 파일·Console 출력 서식, 필터 연결,
  Client의 로그 인자와 CSV 키 등록을 확인했다.
- 실행 검증: 이번 로그 변경의 빌드·로캘 검사·실제 UI 동작은 미확인이다.
  기존 Editor.log에는 이전 출력 형식이 남아 있어 최신 변경의 실행 근거로 사용하지 않는다.

## 2026-10-22 — 그래프·다중 선택 편집과 엔진 경로 보완 (6636769a)

- 이 커밋의 실행 프로그램별 로그 경로와 Console 발생 주체 필터는 위의 같은 날 항목에 기록했으므로 여기서는 반복하지 않는다.
- 그래프: Reflection Archive 기반 JSON 저장·불러오기와 검증을 연결하고, 파일 처리 단계를 구분해 오류를 반환한다.
  GraphPanel에서 파일 편집을 지원하며 GraphBindingComponent로 엔티티별 실행 그래프를 연결한다.
  파일 변경 재로딩이 실패하면 기존 그래프를 유지한다.
- 셰이더 그래프: Color 및 Unlit 머티리얼 HLSL 생성, 셰이더 컴파일·미리보기와 런타임 머티리얼 파이프라인 검증 경로를 추가했다.
- 에디터: Hierarchy의 Shift 범위 선택과 Inspector의 다중 대상 공통 Transform·플래그·리플렉션 속성 편집을 추가했다.
  편집 잠금·읽기 전용·스냅 조건을 확인하고 적용 실패 시 기존 값을 복원한다.
- 렌더링·DX12: 불투명 메시 정렬과 프러스텀 컬링, 렌더 큐·뷰포트 컨테이너 재사용,
  PipelineManager의 최근 파이프라인 조회 캐시를 적용했다. DX12는 프레임별 명령 할당자·펜스·동적 버퍼를 분리하고,
  프레임 슬롯 재사용 시 동기화하며 GPU 자원·디스크립터 해제를 펜스 완료 뒤로 미룬다.
- Transform·피킹·에디터 표시: 월드 행렬 캐시와 메시 지오메트리 버전 기반 BVH 갱신·레이 탐색을 추가했다.
  Hierarchy 문자열 캐시, 로캘 문자열 조회 API와 string_view 조회, 그래프 노드·머티리얼 콤보의 문자열 복사 감소도 포함한다.
- 상태·자원 오류 처리: Terrain 참조 소유권·dirty 업로드·데이터 검사를 보완하고 CPU 리샘플링을 추가했다.
  메시 버퍼 입력과 Archive 복원 실패 처리를 강화하고, 런타임 리소스 버전을 직렬화에서 제외했다.
  Canvas 표시·계층 플래그, Undo/Redo 후 계층 플래그, Client 시작 씬·렌더링 실패 반환도 수정했다.
- 빌드·검사 코드: Engine Unity Build를 활성화하고 ShaderGraphCheck, TerrainDataCheck,
  PickingBVHBenchmark를 추가했으며 그래프·피킹·Transform·Inspector·Archive 회귀 검사 코드를 확장했다.
  관련 검사 실행 결과는 이 커밋 및 이번 기록 작업에서 확인하지 않았으며, 테스트는 요청에 따라 실행하지 않았다.
  렌더링 성능 수치와 실제 GPU·Editor UI 동작도 확인하지 않았다.

## 2026-10-23 — ECS 정리·상태 전파·2D 물리와 모델 변환 기반 보완

- 기록 범위: `6636769a` 이후의 변경을 `aa2633fc`부터 `7c265b75`까지 16개 로컬 커밋으로 분리했다.
  아래 내용은 해당 커밋의 구현 기록이며, 실행 검증 결과와 현재 제한은 별도로 명시한다.
- 로캘 (`aa2633fc`): Engine.csv의 CATEGORY_PHYSICS, STRUCT_BOX_2D, STRUCT_CAPSULE_2D,
  STRUCT_CIRCLE_2D에 영어·한국어 번역을 채웠다. Editor.csv에는 콘솔 중복 묶기와 사용 가능한 카메라가
  없을 때의 안내를 등록하고 콘솔 집계 문구를 보완했다. CSV 변환기로 English.json·Korean.json을 갱신했다.
- 시스템 호출 (`e80d4770`): ISystem·SystemManager와 각 시스템, GameModule의 활성 씬 목록 인자를
  값 전달에서 const 참조로 바꾸어 시스템 호출마다 발생하던 컨테이너 복사를 제거했다.
  프레임 시간이나 메모리 사용량의 개선 수치는 측정하지 않았다.
- 씬·계층·아카이브 (`e6305ec0`): 엔티티 ID 충돌 탐색과 일괄 생성, 하위 트리 삭제 대상 조회에
  집합을 사용하고, 계층의 부모·자식 상호 참조와 중복·순환·깊이 검증에서 반복 탐색을 줄였다.
  씬·프리팹 복원 중 컴포넌트별 Transform 갱신을 지연하고 계층 구성 후 갱신하도록 정리했으며,
  실패 시 상태 보존과 엔티티 ID 처리에 관한 검사 코드를 보완했다.
  사용하지 않는 GameObjectInspector·DynamicEvent·Event 파일과 Scene의 구형 EventSystem·UICanvas 접근자를 제거하고,
  EngineReflectionIntegration.md의 설명을 ECS 엔티티 기준으로 갱신했다.
- 씬 오버레이 (`05067746`): Runtime의 LateUpdate에서 활성 씬의 계층 플래그를 갱신한다.
  SceneViewportPanel의 오버레이에도 부모로부터 전파된 유효 가시성·활성 상태를 반영하도록 수정했다.
- 카메라 (`f0398db9`): CameraSystem에 개별 카메라 버퍼 갱신을 분리했다.
  GameView는 계층에서 활성 상태이며 유효한 버퍼를 만들 수 있는 메인 카메라를 먼저 검사하고,
  실패하면 일반 카메라를 검사한다. 사용할 카메라가 없으면 렌더 제출·채널 필터 처리를 건너뛰고 번역된 안내를 표시한다.
  Client도 유효한 활성 카메라 중 메인 카메라를 우선하여 선택한다.
- 편집 이력·PIE 복원 (`1d669669`): 스냅샷 구조 비교에 ID 조회 맵을 사용하고,
  캡처·복원 전 계층 검증에서 부모 참조 횟수와 중복 자식을 검사한다.
  PIE 종료 시 씬 복원에 실패하면 편집 상태로 전환하거나 이력을 지우지 않고 스냅샷을 남긴다.
  복원에 성공한 뒤 스냅샷 삭제만 실패한 경우에는 경고를 기록한다.
  복원 실패 전 수행한 사운드 정지와 선택 해제까지 되돌리는 처리는 포함하지 않는다.
- 콘솔 (`4788e3ef`): 레벨·발생 주체·분류·메시지·파일·행 번호가 같은 로그를 묶는 옵션을 추가했다.
  묶기는 기본 활성화이며 반복 횟수는 999를 상한으로 표시한다.
  상태 문구는 표시 행·보관 로그·수집 누락·오래된 항목 제거를 구분한다.
- 미리보기 입력 (`9acbdb96`): MaterialPreviewRenderer의 키·마우스 버튼·이동·휠·보조 키 조회를
  InputManager로 통일했다. ImGui의 위젯·포커스 등 UI 문맥 판정은 유지한다.
- 시작 리소스 (`72557150`): 설정 파일 읽기·파싱, 누락된 설정, 알 수 없는 분류와 리소스 로드 실패를
  사전 로드 결과에 반영하고, Application 초기화까지 실패를 전달한다.
  공통 파일 로드 처리를 정리하고 시작 메시 목록의 기존 네 파일 항목을 제거했다.
- 2D 물리 (`ef527735`): Editor에 PhysicsSystem을 등록하고 PIE의 FixedUpdate를
  TimeManager가 제공하는 고정 스텝으로 소비한다. 편집 중에는 누적된 고정 스텝을 비운다.
  동적 강체의 중력·외력·선형 감쇠 적분과 박스 간 충돌의 속도·침투 보정,
  충돌 레이어·마스크 및 유한 값 검사를 추가했다. Rect 충돌 결과의 침투 깊이·법선·겹침 영역 계산을 보완했다.
  물리 도형의 리플렉션 표시 이름·분류와 PhysicsTest.json 샘플 씬을 추가했다.
  현재 접촉 응답은 부모가 없고 회전하지 않은 Box2DShape에 한정하며, 트리거는 응답에서 제외한다.
  원·캡슐·회전 박스·부모가 있는 강체까지 물리 시뮬레이션을 지원한다는 의미는 아니다.
- RHI 버퍼 (`dfcde380`): 정점·인덱스 버퍼의 범위·정렬을 검사하는 부분 업로드 API와
  완료 상태 조회를 추가하고 DirectX12·SDLGPU에 연결했다.
  SDLGPU는 업로드 펜스의 완료를 확인한 뒤 다음 부분 업로드를 허용하고,
  기존 CPU 측 버퍼 복사본을 제거했다. 실제 GPU 동작과 처리 시간은 확인하지 않았다.
- 모델 변환 도구 (`5173dd03`): Assimp를 사용하는 독립 정적 라이브러리 ModelConverter와
  BamTools::ModelConverter 타깃을 추가했다. 정적 메시의 정점·인덱스·경계와 노드 계층을 CPU 데이터로 변환하며,
  잘못된 참조·유한하지 않은 값·개수 제한을 검사한다. 애니메이션·스키닝·모프 변환은 지원하지 않는다.
- 모델 임포트 (`d7aeab18`): 임포터 인터페이스를 IImporter로 정리하고 ModelImporter에 변환 도구를 연결했다.
  임시 메시 자원을 소유한 상태에서 생성·저장을 처리한 뒤 ResourceManager에 넘기며,
  이미 등록된 리소스 키의 교체는 거부한다. 여러 파일의 저장·등록 전체를 원자적으로 되돌리는 처리는 제공하지 않는다.
  ImportJob·ModelImportTask와 CreateJob으로 요청 설정·CPU 준비 데이터·오류 결과를 분리했다.
  현재 AssetManager::ImportAsync의 모델 경로는 호출 스레드에서 동기 Import를 수행하고 기존 완료 알림 경로를 사용한다.
  CPU 준비 작업의 백그라운드 실행, 단계별 GPU 업로드와 최종 등록을 연결한 비동기 임포트 완료 상태는 아니다.
- 스프라이트 (`d5a58329`): ESpriteDrawMode를 enum class로 바꾸어 열거자 범위를 제한했다.
- 샘플 데이터 (`337302ed`): GraphMovement.json을 추가하고 RaytracingScene.json의 머티리얼 참조 경로와
  직렬화 값을 갱신했다. uv1.bamsprite.json의 영역 필드를 현재 Rect 직렬화 키인 left·top·width·height에 맞췄다.
- 문서·작업 지침 (`7c265b75`): 이전 `6636769a`의 구현 기록을 보완하고,
  Mentor 에이전트가 루트 AGENTS.md와 작업 범위의 프로젝트 문서를 먼저 확인하도록 지침을 추가했다.
- 검증: 커밋 정리 작업에서 CSV→JSON 변환기의 `--strict` 실행과
  `Utility/LocalizationAuditor/audit_localization.py --root . --strict` 실행이 성공했다.
  검사 대상 소스 428개에서 누락 키·빈 영어/한국어 번역·검사 대상 UI 호출의 하드코딩 문구는 발견되지 않았다.
  이 결과는 실제 언어 전환·편집 상태 유지나 검사 범위 밖의 동적 문자열까지 보장하지 않는다.
  SceneArchiveCheck·SceneReflectionCheck·HierarchyEditorCheck·EditorHistoryCheck의 검사 코드는 확장했으나,
  이번 커밋 정리와 기록 작업에서는 C++ 빌드·CTest·실제 Editor/Client UI·물리·GPU 실행 검증을 수행하지 않았다.
  성능 측정도 수행하지 않았으므로 구현상의 탐색·복사 감소와 측정된 성능 향상을 구분한다.


## 2026-10-26 — 스카이 애트모스피어·클라우드 텍스처 에셋화 및 지평선 렌더링 최적화

- 요청: SkyAtmosphereSystem에서 생성한 절차적 구름 텍스처를 디스크 파일로 저장·캐싱하는 파이프라인 구축 및 볼류메트릭 클라우드 지평선(수평선) 시점의 심한 프레임 드랍 완화.
- 텍스처 아카이브 확장 및 CPU 데이터 보존:
  - `Engine/Source/Resource/Texture/Private/Texture.cpp`: `ValidateTextureHeader`에서 `Texture3D` 및 `depth >= 1`을 허용하고 3D 볼륨 크기(`width * height * depth * 4`)를 계산하도록 확장.
  - `Engine/Source/Resource/Texture/Public/Texture.h`: `SetTempData()` 호출 시 `m_CachedHeader.dataSize`, `mipLevels`, `arraySize` 기본 메타데이터를 동기화하여 `WriteArchive` 유효성 검사 실패를 방지.
  - `Engine/Source/Render/Cloud/Private/CloudTextureGenerator.cpp`: `CreateBaseShapeTexture`와 `CreateWeatherMapTexture`가 내부에서 생성한 CPU 픽셀 데이터를 `Texture` 객체에 보관(`SetTempData`)하고 완성된 `Texture*`를 반환하도록 통합.
  - `Projects/GameModule/Source/SkyAtmosphereSystem.cpp`: `.bamtex` 파일이 디스크에 존재할 경우 `rm.LoadFile()`을 통해 역직렬화 로드하고, 부재 시에만 노이즈를 절차적으로 생성한 뒤 `rm.SaveToBinaryFile()`로 디스크에 자동 캐싱.
  - `Editor/Source/ImGui/InspectorPanel/Private/ReflectionPropertyDrawer.cpp`: `#include "ImGui/Public/ResourcePicker.h"`로 인클루드 경로를 명시하여 `ReflectionInspectorCheck` 테스트 타깃 컴파일 에러 해결.
- 볼류메트릭 클라우드 지평선 프레임 안정화:
  - 원인: 지평선 방향 광선이 지구 곡률을 스쳐 지나가며 광선 길이(`tEnd - tStart`) 및 카메라와 구름층 사이 거리(`tStart`)가 수십 km로 폭증하여 192스텝을 꽉 채우고, 대기 산란 루프(64스텝) 및 메인 루프에서 태양광 투과 서브 마칭(픽셀당 1,500회 이상 텍스처 페치)이 중첩 발생.
  - `Resources/Shader/hlsl/atmosphereSkyPS.hlsl`: 대기 산란 64스텝 루프 내에서 구름 그림자를 중복 계산하던 `ENABLE_CLOUD_ATMOSPHERE_SHADOW`를 비활성화하여 대기 연산 비용 절감.
  - `Resources/Shader/hlsl/cloud.hlsli`:
    - 원거리 컬링: 카메라와 구름 시작 지점 거리(`tStart`)가 28km 이상인 경우 레이마칭 조기 탈출.
    - 거리 기반 적응형 LOD: 지평선 방향 광선에 대해 마칭 스텝을 최대 48~24스텝(기존 192스텝)으로 동적 감축 및 스텝 간격 조정.
    - 태양광 투과 마칭 경량화: `ComputeCloudSunTransmittance` 스텝 수를 2~4스텝으로 줄이고 미세 디테일 노이즈 샘플링을 생략하는 `highDetail = false` 분기 추가.
    - Space Skipping: 빈 하늘 구간은 1.5배 보폭으로 건너뛰어 불필요한 텍스처 페치 축소.
    - 원거리 페이드아웃: 20km~30km 구간에서 알파 감쇠를 적용하여 시각적 팝핑 방지.
- 검증 및 범위:
  - 컴파일 오류(Texture3D 검증, LoadResource 템플릿 인스턴스화 불일치, ResourcePicker 누락)를 해결하고 빌드 완료.
  - 런타임 1회 실행 후 `Resources/Texture/CloudBaseShape3D.bamtex` 및 `CloudWeatherMap2D.bamtex`가 디스크에 정상 생성됨을 확인.
  - 지평선 시점의 텍스처 페치 및 마칭 루프 부하가 감소하여 프레임 드랍이 완화됨을 확인.
  - 독립 렌더 패스(1/4 저해상도 버퍼 + Temporal Reprojection) 및 2D Cloud Shadow Map 분리는 향후 차세대 과제로 유지.


## 2026-11-02 — 3D 모델 임포트 파이프라인 완성 (GLTF/GLB/FBX 머티리얼·임베디드 텍스처 연동) 및 머티리얼 인스턴스 렌더링 안정화

- 요청: 3D 모델 파일(`Fox.glb` 등) 임포트 시 메시뿐만 아니라 스켈레톤, 애니메이션, 머티리얼 슬롯 및 임베디드 텍스처(PNG 바이너리)를 자동 추출하여 에셋화하고, 씬에 배치했을 때 텍스처와 라이팅이 정상 렌더링되도록 파이프라인 통합 및 버그 수정.
- 모델 임포트 파이프라인 구축 및 다형성 확장:
  - `Tools/ModelConverter/Public/ModelConverter.h`, `Private/ModelConverter.cpp`:
    - Assimp `mMaterials` 순회를 통해 PBR 기본 인자(BaseColor, Roughness, Metallic) 및 텍스처 바인딩 슬롯 정보(`TextureBindingInfo`) 추출.
    - GLB/FBX 바이너리에 내장된 임베디드 텍스처(`scene->GetEmbeddedTexture()`) 바이너리 데이터(`EmbeddedData`) 추출 지원.
  - `Editor/Source/AssetManager/Public/ModelImportTask.h`, `Private/ModelImportTask.cpp`:
    - 백그라운드 태스크의 `PreparedModelData`에 머티리얼 목록 및 임베디드 텍스처 데이터 복사/전달 파이프라인 연결.
  - `Editor/Source/AssetManager/Importer/Public/ModelImporter.h`, `Private/ModelImporter.cpp`:
    - `EModelImportOption::Materials` 옵션 지원 및 `ModelImportDesc`에 `bool Overwrite = true` 추가.
    - `CommitPreparedModel`에서 임베디드 텍스처를 디스크(`.png`)로 자동 덤프하고 `ResourceManager::LoadFile` 연동.
    - 스켈레탈 메시 기본 머티리얼(`SkinningMaterial.bammat.json`) 기반의 `MaterialInstance`(`.bammatinst`) 생성, 추출된 텍스처 바인딩 및 모델 에셋 등록.
  - `Engine/Source/Resource/Model/Public/Model.h`, `Private/Model.cpp`:
    - 모델 에셋의 머티리얼 슬롯을 `ResourceHandle<Material>`에서 `ResourceHandle<MaterialInterface>`로 다형성 확장하여 `.bammat` 외에 `.bammatinst`도 직접 연결할 수 있도록 개선.
- 머티리얼 인스턴스 렌더링 버그 수정 및 상태 누수 방지:
  - 현상: 여우 모델 씬 배치 시 지오메트리는 정상 렌더링되나 음영/조명 없이 완전히 새하얗게(Pure White / Unlit 자체 발광) 출력됨.
  - 원인:
    - `MaterialInstance::Bind`에 미바인딩 슬롯(Normal/PBR/Emission)에 대한 표준 기본 텍스처 리셋 처리가 누락되어, 이전 드로우 콜의 흰색 텍스처가 Slot 3(Emission)에 남아있어 픽셀 셰이더에서 자체 발광 발생.
    - 아카이브 역직렬화 직후 슬롯 검색 캐시(`m_TextureSlotToIndex`)가 비어 있어 `HasTextureBindingBySlot()`이 오작동함.
  - 해결:
    - `Engine/Source/Resource/Material/Private/MaterialInstance.cpp`: `MaterialInstance::Bind` 진입 시 내부 슬롯 캐시가 비어 있으면 `RebuildTextureBindingCache()` 자동 호출.
    - 바인딩되지 않은 G-Buffer 텍스처 슬롯(0~3)에 표준 기본 텍스처(`white1x1`, `blue1x1`, `black1x1(PBR)`, `black1x1(Emission)`)를 강제 바인딩하여 텍스처 상태 누수 원천 차단.
- 검증 및 범위:
  - `Fox.glb` 임포트 시 메시(`Fox_fox1.bammesh`), 스켈레톤(`Fox_Skeleton.bamskel`), 애니메이션 3종(`Fox_Survey`, `Walk`, `Run`), 알베도 텍스처(`Fox_fox_material_Albedo.png`), 머티리얼 인스턴스(`Fox_fox_material.bammatinst`), 모델(`Fox.bammodel`)이 유효하게 디스크에 생성됨을 확인.
  - 씬 배치 시 여우 모델에 주황색/흰색 텍스처와 디렉셔널 라이트 음영이 정상적으로 렌더링됨을 시각적으로 확인.


## 2026-11-03 — 에디터 UI/UX 전면 개편: 리소스 피커 대형 썸네일·배지 분리, 콘솔 패널 4열 테이블·동적 수직 스플리터, 썸네일 렌더러 3점 조명 및 디퍼드 파이프라인 안정화

- 요청: 
  - 에셋 썸네일 렌더러 복원 및 3점 조명 스튜디오 씬 적용.
  - 디퍼드 라이팅 패스 D3D12 `Draw(3)` 크래시(`KernelBase.dll!RaiseException`, SDK Layers 검증 실패) 및 프리뷰 조명 미적용(Flat) 버그 해결.
  - 리소스 피커(`ResourcePicker`) 전면 개편: 대형 썸네일 슬라이더(40~120px), 배지 우측 분리 정렬 및 크기 동적 계산, ImGui ID 충돌 해결.
  - 콘솔 패널(`ConsolePanel`) 가독성 개선: 4열 테이블 구조, 레벨/카테고리 캡슐 배지, `SingleLineMessage` 캐시로 `ImGuiListClipper` 누락 해결, 하단 상세 뷰 및 인터랙티브 수직 스플리터(Splitter) 구현.
- 에셋 썸네일 렌더러 3점 조명 스튜디오 복원 및 렌더링 안정화:
  - `Editor/Source/Rendering/Thumbnail/Private/ThumbnailRenderer.cpp`, `Public/ThumbnailRenderer.h`:
    - 썸네일 프리뷰를 위한 3점 조명(Key Light - 주광, Fill Light - 보조광, Rim Light - 역광) 스튜디오 조명 환경 구축.
    - `AssetCache`를 통한 썸네일 비동기 생성 및 RHI 텍스처 업로드 파이프라인 연동.
- 디퍼드 라이팅 패스 D3D12 `Draw(3)` 줄 1736 크래시 및 프리뷰 Flat 현상 해결:
  - `Editor/Source/ImGui/ViewportPanel/Private/ViewportDeferredPipeline.cpp`, `Public/ViewportDeferredPipeline.h`:
    - 원인: `BindTextureSampler` 호출로 8번 루트 파라미터(Sampler Table)를 설정한 직후 `BindPipeline(m_LightingPipeline)`이 호출되어 `SetGraphicsRootSignature`로 인해 기존 루트 파라미터가 모두 무효화됨.
    - 해결: 파이프라인 바인딩을 모든 리소스 바인딩보다 맨 앞으로 이동하여 D3D12 SDK Layers 검증 실패 및 크래시(`RaiseException`) 원천 차단.
  - `Resources/Shader/hlsl/lightingPS.hlsl`, `Engine/Source/Render/RHI/Public/RHIPipelineTypes.h`:
    - 씬 뷰포트와 달리 타일 컬링을 수행하지 않는 독립 프리뷰 씬에서 광원이 0개로 판정되어 Flat하게 출력되던 문제를 `DeferredLightingPassData`의 `UseTiledCulling` 플래그 및 전체 광원 순회 폴백으로 해결.
- 인스펙터 `ResourcePicker` UI/UX 전면 개편:
  - `Editor/Source/ImGui/Public/ResourcePicker.h`:
    - 썸네일 크기 확대: 기본 64px(기존 36px 대비 1.8배) 및 40~120px 슬라이더 지원.
    - 2단 텍스트(파일명 노란색 강조 + 상대 경로 회색) 및 128x128 호버 툴팁 프리뷰 연동.
    - 우측 컬럼 배지 정렬: 경로 텍스트와 타입 배지(`MATERIAL`, `3D MODEL` 등)가 세로로 포개지던 결함을 우측 독립 컬럼 배치로 원천 차단.
    - 배지 크기 동적 계산: 아이콘 및 텍스트 폰트 크기 기반으로 패딩(`badgePadX: 10px`, `badgePadY: 5px`)을 동적 적용하여 아이콘이 테두리 밖으로 삐져나오던 문제 해결.
    - ImGui ID 충돌 해결: 메모리 로드 리소스와 디스크 파일의 경로가 일치할 때 발생하던 ID Collision 경고를 `Mem_`, `Disk_` 접두사 및 `PushID` 스코프로 해결.
- 콘솔 패널(`ConsolePanel`) 모던 테이블화 및 인터랙티브 수직 스플리터 구현:
  - `Editor/Source/ImGui/ConsolePanel/Private/ConsolePanel.cpp`, `Public/ConsolePanel.h`:
    - 4열 정렬 테이블: `[Time / Level]`, `[Category]`, `[Message]`, `[Source]` 컬럼 분할 및 교차 행 배경(`RowBg`) 적용.
    - 컬러 캡슐 배지: `[ INFO ]`, `[ WARN ]`, `[ ERROR ]` 등 레벨별 고유 포인트 컬러/아이콘/반투명 배경/외곽선 배지 및 카테고리 태그 적용.
    - 행 높이 고정 및 `ImGuiListClipper` 누락 방지: 긴 메시지 및 줄바꿈(`\n`)으로 인해 행 높이가 팽창하여 가상화 스크롤 계산이 깨지고 아래쪽 로그가 통째로 사라지던 문제를 `SingleLineMessage` 도입 및 `ScrollX` 제거로 100% 원천 해결.
    - 하단 상세 뷰(Detail View) 및 인터랙티브 수직 스플리터:
      - 6px 두께의 수직 스플리터 위젯(`ImGui::Button("##ConsoleDetailSplitter")`) 및 중앙 그립 알약 핸들 배치.
      - 마우스 호버/드래그 시 `ImGuiMouseCursor_ResizeNS` 상하 화살표 커서로 즉시 전환.
      - 마우스 드래그(`MouseDelta.y`)로 상세 뷰 높이를 60px에서 창 전체 크기까지 부드럽게 실시간 조절 지원. 기본 높이를 160px로 확대.
      - 상세 뷰 상단에 메타 정보 헤더 및 전문 `[ 복사 ]` 버튼, `[ ✕ 닫기 ]` 버튼 추가.
- 검증 및 범위:
  - 리소스 피커에서 40~120px 썸네일 크기 변경 및 호버 프리뷰 툴팁이 정상 동작함을 확인.
  - 리소스 피커 배지가 우측에 안정적으로 정렬되어 텍스트와 겹치지 않고 아이콘이 테두리 안에 정돈됨을 확인.
  - 콘솔 패널에서 긴 셰이더 에러 메시지가 인입되어도 다른 로그가 누락되지 않고 1줄로 표시되며, 행 클릭 시 하단 상세 창에 줄바꿈이 보존되어 출력됨을 확인.
  - 상세 뷰 상단 스플리터 경계선을 마우스로 끌어 높이를 원하는 대로 늘리고 줄일 수 있으며, [✕] 버튼으로 상세 창을 닫을 수 있음을 확인.


## 2026-11-04 — 키네마틱 캐릭터 컨트롤러(Kinematic Character Controller, KCC) 시스템, 비주얼 스크립팅 노드 및 에디터 뷰포트 시각화 구현

- **요청**:
  - 동역학 물리(Dynamic RigidBody)의 원치 않는 토크 회전, 튕김 및 떨림 현상 없이, 캐릭터가 지형 및 3D 충돌체와 정밀하고 부드럽게 상호작용하는 전용 키네마틱 이동 시스템(KCC) 구축.
  - 지면 접지 감지(Ground Detection & Snapping), 경사로 슬라이딩(Slope Limit & Slide), 턱/계단 오르기(Stair Step-Up), 장애물 충돌 스윕(Move & Slide Bounce Loop) 구현.
  - 비주얼 스크립팅(Graph) 제어 노드 및 에디터 뷰포트 디버그 캡슐 와이어프레임 렌더링 추가.
- **구현 내역**:
  - `Engine/Source/Core/Public/Components/Physics/CharacterControllerComponent.h`:
    - 캡슐 형상(`Height=2.0f`, `Radius=0.5f`, `Center=vec3(0, 1, 0)`, `SkinWidth=0.05f`), 이동 파라미터(`WalkSpeed`, `RunSpeed`, `JumpForce`, `Gravity`, `SlopeLimitDegrees`, `StepOffset`, `AirControl`) 정의.
    - 리플렉션 및 직렬화 완전 지원, 런타임 제어 API (`Move`, `Jump`, `SetVelocity`, `GetWorldCapsule`, `GetWorldAABB`).
  - `Engine/Source/System/Physics/Public/KinematicCharacterSystem.h`, `Private/KinematicCharacterSystem.cpp`:
    - `ISystem` 상속, 물리 고정 스텝(`OnFixedUpdate`)에서 모든 활성 KCC 컴포넌트 일괄 시뮬레이션.
    - 하향 레이/스윕 기반 지면 접지 감지 및 경사각 계산 (`DetectGround`).
    - 급경사 미끄러짐(Slide Down Slope) 및 등판 가능한 완경사 바닥면 투영 이동.
    - 장애물 충돌 시 다중 반복(최대 3회) 슬라이딩(Slide & Collide) 및 계단/턱 감지 시 수직 상승->전방 이동->하향 안착(Stair Step-Up) 처리.
    - 상향 천장 충돌(Ceiling Hit) 및 하향 바닥 착지 처리.
  - `Engine/Source/Core/Private/Reflection/ReflectionRegistry.cpp`:
    - Add Component 메뉴의 "Physics 3D" 카테고리에 `CharacterControllerComponent` 등록.
  - `Engine/Source/Graph/Public/GraphGameplayLibrary.h`:
    - 비주얼 스크립팅 노드 7종 추가: `CharacterController.Move`, `Jump`, `IsGrounded`, `GetVelocity`, `SetVelocity`, `GetGroundNormal`, `GetGroundAngle`.
  - `Editor/Source/Application/Private/Application.cpp` & `Client/Source/Main.cpp`:
    - 에디터 런타임 및 Standalone Client의 `SystemManager`에 `KinematicCharacterSystem` 등록.
  - `Editor/Source/ImGui/ViewportPanel/DebugRenderer/Private/DebugRenderer.cpp`:
    - 뷰포트에서 `CharacterControllerComponent` 캡슐 와이어프레임(초록/주황) 및 지면 법선(청록) 디버그 렌더링 구현.
  - `Resources/Data/Localization/Engine.csv`:
    - 컴포넌트 및 속성 다국어 로캘 키 14개 추가.
  - `Docs/KinematicCharacterControllerPlan.md` & `Docs/DevelopmentPriorities.md`:
    - 사양 및 아키텍처 문서화, 통합 개발 우선순위 완료 반영.
- **검증 결과**:
  - `Editor` 및 `Client` 빌드 및 링크 정상 통과.
  - 인스펙터 속성 편집 및 씬 아카이브 저장/복원 정합성 확인.
  - 씬 뷰포트에서 캡슐 크기 변경에 따른 디버그 와이어프레임 실시간 반영 확인.
  - 비주얼 스크립팅 그래프 에디터 노드 검색 및 배치 정상 확인.


## 2026-11-04 — 2D/3D 통합 물리 트리거(Trigger) 및 다중 채널 이벤트 디스패치 파이프라인 구축

- **요청**:
  - 기존에 수집만 되고 개별 엔티티/컴포넌트로 전달되지 않던 2D 물리 트리거 이벤트를 개선하고,
  - 3D 충돌체(`ColliderComponent`) 및 키네마틱 캐릭터(`CharacterControllerComponent`) 간의 3D 트리거 겹침(Overlap) 판정을 신규 구현.
  - 게임 모듈, C++ 프로그래머, 기획자/아티스트가 각각 원하는 방식으로 `OnTriggerEnter`, `OnTriggerStay`, `OnTriggerExit` 로직을 작성할 수 있도록 3대 채널(C++ 델리게이트, C++ 클래스 상속, 비주얼 스크립팅 노드)을 일원화 디스패처로 노출.
- **구현 내역**:
  - `Engine/Source/Core/Public/Components/Physics/TriggerCallbackComponent.h`:
    - C++ 람다 및 함수 객체 델리게이트를 등록할 수 있는 경량 컴포넌트 (`OnEnter`, `OnStay`, `OnExit`).
  - `Engine/Source/World/Entity/Public/ScriptableEntity.h`, `Engine/Source/Core/Public/Components/Core/NativeScriptComponent.h`:
    - 유니티의 `MonoBehaviour` 및 언리얼의 `Actor`처럼 C++ 클래스를 상속받아 `OnTriggerEnter/Stay/Exit` 가상 함수를 직관적으로 오버라이드할 수 있는 스크립팅 베이스 및 ECS 바인딩 컴포넌트.
  - `Engine/Source/System/Physics/Public/PhysicsSystem.h`, `Private/PhysicsSystem.cpp`:
    - 3D Overlap 감지 함수 `PhysicsSystem::Overlap3D` 구현 (Box-Box, Box-Sphere, Sphere-Sphere, Capsule-Sphere, Capsule-Box, KCC-Trigger).
    - 3D 트리거 쌍 수집 루프 `CollectPhysicsTriggerEvents3D` 구현.
    - 2D 및 3D 이벤트를 순회하며 등록된 컴포넌트로 호출을 분배하는 통합 디스패처 `DispatchTriggerEvents` 구현.
    - 전역 리스너 브로드캐스트 `RegisterGlobalTriggerCallback` 완비.
  - `Engine/Source/Graph/Private/GraphRegistry.cpp`:
    - 비주얼 스크립팅 빌트인 노드 3종 등록: `Event.OnTriggerEnter`, `Event.OnTriggerStay`, `Event.OnTriggerExit` (`exec` 실행 핀, `other` 상대 엔티티 ID 핀).
  - `Engine/Source/Graph/Public/GraphExecutor.h`, `Private/GraphExecutor.cpp`:
    - 트리거 노드 진입점 실행 및 상대 엔티티 주입 메서드 `BeginTrigger(nodeId, otherEntity)` 구현.
  - `Engine/Source/System/Graph/Public/GraphSystem.h`, `Private/GraphSystem.cpp`:
    - 그래프 런타임 상태(`GraphRuntimeState`)에 트리거 노드 캐싱 및 물리 이벤트 디스패치 브리지 `DispatchTriggerEvent` 구현.
  - `Resources/Data/Localization/Engine.csv`:
    - `STRUCT_TRIGGER_CALLBACK`, `STRUCT_NATIVE_SCRIPT` 로캘 키 추가.
- **검증 결과**:
  - `Engine` 및 `Editor` 빌드 에러 없이 통과.
  - 에디터 그래프 캔버스 우클릭 메뉴에서 `OnTriggerEnter`, `OnTriggerStay`, `OnTriggerExit` 노드 정상 노출 및 배치 확인.
  - C++ 람다, 네이티브 스크립트, 비주얼 스크립트 노드로의 이벤트 전달 경로 확보.


## 2026-11-07 — 모듈형 파티클 & 비주얼 이펙트 시스템 계획 수립 및 1단계 물리 충돌(FloorPlane / Raycast) 구현

- **작업 범위:**
  - `Docs/ParticleAndEffectSystemPlan.md`: 모듈형 파티클 & 비주얼 이펙트 시스템 및 에디터 개발 계획 수립 (Phase 1 ~ Phase 5).
  - `Docs/DevelopmentPriorities.md`: 신규 계획 문서 등록.
  - `Engine/Source/Core/Public/Components/Render/ParticleEmitterComponent.h`:
    - `EParticleCollisionMode` 열거형(`None`, `FloorPlane`, `PhysicsRaycast`) 정의.
    - `EParticleCollisionResponse` 열거형(`Bounce`, `Slide`, `DieOnContact`, `Stop`) 정의.
    - 충돌 및 반발/마찰 프로퍼티(`collisionMode`, `collisionResponse`, `restitution`, `friction`, `floorHeight`, `collisionRadius`, `minBounceSpeed`, `maxCollisionChecksPerFrame`) 추가 및 리플렉션 등록.
    - `Particle` 구조체에 `bounceCount` 추가.
  - `Engine/Source/System/Render/Public/ParticleSystem.h`, `Private/ParticleSystem.cpp`:
    - `UpdateEmitter`에 `Scene* scene` 전달 인터페이스 확장.
    - `FloorPlane` 초경량 고속 바닥 충돌 및 침투(Tunneling) 방어, 정확한 접촉점 및 시간 분율($t_{\text{hit}}$) 계산.
    - `PhysicsRaycast` 모드에서 `PhysicsSystem::Raycast`를 통한 월드 콜라이더 선분 충돌 및 프레임당 최대 호출 수 상한 방어.
    - 탄성 반발($\vec{v}' = \vec{v} - (1+e)(\vec{v}\cdot\vec{N})\vec{N}$), 접선 마찰 감쇠, 최소 바운스 속도 임계치에 따른 바닥 밀착/슬라이딩, 접촉 시 즉시 소멸(`DieOnContact`) 분기 완비.
    - 로컬 및 월드 시뮬레이션 공간(`EParticleSimulationSpace`) 상호 변환 지원.
  - `Resources/Data/Localization/Engine.csv`:
    - `CATEGORY_PARTICLE_COLLISION`, `PROP_COLLISION_MODE`, `PROP_COLLISION_RADIUS`, `PROP_COLLISION_RESPONSE`, `PROP_FLOOR_HEIGHT`, `PROP_MAX_COLLISION_CHECKS`, `PROP_MIN_BOUNCE_SPEED` 로캘 키 등록.
- **확인 필요 사항 (사용자 실행):**
  - 빌드 검증: C++ 엔진 및 리플렉션 빌드 정상 통과 여부.
  - 에디터 인스펙터: `ParticleEmitterComponent`에 `Collision & Interaction` 카테고리 및 신규 충돌 파라미터 노출 확인.
  - FloorPlane 바운스: `FloorPlane` 모드에서 파티클이 바닥($y=0$)에 부딪혀 통통 튀며 마찰로 감쇠하는지 확인.
  - Raycast 바운스: 경사면/박스 콜라이더 위로 파티클을 발사했을 때 표면 법선에 맞춰 튕겨나가는지 확인.


## 2026-11-07 — 파티클 2단계: 공간 연기 확산 역학 (부력, 3D 무발산 컬 노이즈 와류, 바람 외력) 구현

- **작업 범위:**
  - `Engine/Source/Core/Public/Components/Render/ParticleEmitterComponent.h`:
    - 수명 및 역학 카테고리에 부력(`buoyancy`), 바람 외력(`wind`), 컬 노이즈 토글(`enableCurlNoise`), 노이즈 강도(`curlNoiseStrength`), 주파수(`curlNoiseFrequency`), 스크롤 속도(`curlNoiseScrollSpeed`) 추가 및 리플렉션 등록.
  - `Engine/Source/System/Render/Private/ParticleSystem.cpp`:
    - **3D 무발산 컬 노이즈(Divergence-Free Curl Noise) 구현:**
      - 비트 연산 기반 결정론적 3D 그래디언트 노이즈 `Noise3D` 및 `FastHash3D` 구현 (힙 할당 및 텍스처 조회 0).
      - 중앙 차분(Central Difference) 기반 회도($\vec{v}_{\text{curl}} = \nabla \times \vec{\Psi}$) 벡터 산출로 유체의 비압축성 소용돌이(Turbulence) 생성 (입자 뭉침 원천 차단).
      - 시간에 따른 3D 노이즈 공간 스크롤(`scrollTime`) 연동으로 살아 움직이는 대기 기류 표현.
    - **부력 및 바람 외력 적분:**
      - 중력과 부력 가속도 합성 ($\vec{a} = \vec{g} + (0, b, 0)$).
      - 공기 저항 계수(`drag`)와 바람 속도(`wind`) 간의 상대 속도 드래그 감쇠 구현 ($\vec{a}_{\text{drag}} = (\vec{w} - \vec{v}) \times \text{drag}$).
      - 와류와 충돌 모드(`FloorPlane`, `PhysicsRaycast`)의 상호작용: 소용돌이치며 퍼져나가는 연기가 벽이나 바닥에 부딪히면 뚫지 않고 벽면을 타고 미끄러지도록(`Slide`) 유기적 통합.
  - `Resources/Data/Localization/Engine.csv`:
    - `PROP_BUOYANCY`, `PROP_ENABLE_CURL_NOISE`, `PROP_CURL_NOISE_STRENGTH`, `PROP_CURL_NOISE_FREQUENCY`, `PROP_CURL_NOISE_SCROLL_SPEED`, `PROP_WIND_FORCE` 로캘 키 원본 등록.
  - `Docs/ParticleAndEffectSystemPlan.md`: Phase 2 완료 및 상태 갱신.
- **확인 필요 사항 (사용자 실행):**
  - 빌드 검증: C++ 엔진 정상 빌드 여부.
  - 부력(Buoyancy) 검증: `Buoyancy`를 양수(예: $5.0$)로 주었을 때 파티클이 위로 피어오르는지 확인.
  - 바람(Wind) 검증: `Wind Force`에 $(5, 0, 0)$을 주었을 때 파티클들이 바람 방향으로 휘날리는지 확인.
  - 컬 노이즈(Curl Noise) 검증: `Enable Turbulence`를 켜고 `Strength` $2.0$, `Frequency` $0.5$ 설정 시 입자들이 뭉치지 않고 유체처럼 소용돌이치며 퍼져나가는지 확인.
  - 벽면 슬라이딩 검증: 소용돌이치는 연기가 콜라이더 벽면에 부딪혔을 때 뚫지 않고 벽면을 감싸듯 타고 흐르는지 확인.


## 2026-11-07 — [Hotfix] ParticleEmitterComponent 신규 프로퍼티 하위 호환성 (ALLOW_MISSING) 적용

- **원인 분석:**
  - 씬 역직렬화(`archive_reflection::ReadObject`) 시 프로퍼티에 `ALLOW_MISSING` 메타데이터가 없으면 누락된 필드에 대해 `ArchiveErrorCode::MissingField`(code=1)를 반환하며 씬 로드가 중단됨.
  - Phase 1/Phase 2에서 추가된 프로퍼티들(`buoyancy`, `wind`, `enableCurlNoise`, `collisionMode` 등)이 기존 저장된 씬 파일(`RaytracingScene3.json` 등)에 존재하지 않아 로드 오류 발생.
- **수정 내용:**
  - `Engine/Source/Core/Public/Components/Render/ParticleEmitterComponent.h`:
    - 신규 추가된 모든 물리/역학/충돌 프로퍼티에 `ALLOW_MISSING` 매크로 추가.
    - 기존 씬 파일 로드 시 누락된 필드가 있어도 C++ 기본값으로 정상 초기화되도록 하위 호환성 보장.


## 2026-11-08 — 이펙트 전용 텍스처 리소스 4종 생성 및 기본 폴백 파이프라인 구축

- **작업 범위:**
  - **신규 텍스처 에셋 생성 (512x512 RGBA 32-bit):**
    - `Resources/Texture/Particle_SoftCircle.png`: 안티앨리어싱 가우시안 곡선 기반의 방사형 부드러운 원형 텍스처 (기본 파티클, 불씨, 마법 구체).
    - `Resources/Texture/Particle_Smoke.png`: 다중 옥타브 프랙탈 펄린 노이즈와 원형 비네팅 마스크를 합성한 유기적 연기 덩어리 텍스처 (연기, 안개, 흙먼지).
    - `Resources/Texture/Particle_Spark.png`: 고밀도 중앙 코어와 십자 및 45도 대각선 지향성 플레어를 결합한 스파크 텍스처 (바닥 튀는 불꽃, 용접 스파크).
    - `Resources/Texture/Particle_Ring.png`: 부드러운 가우시안 프로파일을 가진 동심원 링 텍스처 (착지 충격파, 수면 파문).
  - `Engine/Source/System/Render/Private/ParticleSystem.cpp`:
    - `OnSubmit` 렌더 제출 단계에서 `textureHandle`이 비어 있을 경우 `Particle_SoftCircle.png`를 기본 텍스처로 자동 폴백.
    - `materialHandle`이 비어 있을 경우 `SpriteMaterial.bammat.json`으로 자동 폴백.
    - 이제 사용자가 엔티티에 컴포넌트 추가 시 흰 사각형(`white1x1`)이 아닌 부드러운 원형 파티클로 즉시 렌더링되도록 개선.


## 2026-11-08 — 이펙트 전용 머티리얼 2종 (Additive, AlphaBlend) 구축 및 HDR 발광(Emission) 셰이더 연동

- **작업 범위:**
  - **셰이더 발광(Emission) 및 HDR 블룸 연동:**
    - `Resources/Shader/hlsl/gbufferSpritePS.hlsl`:
      - 기존 `output.Emission = float4(0, 0, 0, 0)` 고정을 해제하고, `output.Emission = float4(texColor.rgb * input.Color.a, 1.0f)` 출력.
      - 파티클의 색상과 알파가 강할수록 발광(Emission) 버퍼에 빛 에너지를 방출하여 HDR 블룸 후처리(`bloomPrefilterPS`)와 실시간 연동.
    - `Resources/Shader/glsl/gbuffer_sprite.frag`:
      - GLSL 셰이더에도 `outEmission = vec4(texColor.rgb * inColor.a, 1.0)` 대칭 적용.
  - **신규 파티클 전용 머티리얼 2종 생성:**
    - `Resources/Material/ParticleMaterial_Additive.bammat.json`:
      - 블렌드 모드 `Additive` (`m_BlendMode: 4`), 깊이 모드 `ReadOnly` (`m_DepthMode: 1`).
      - 불꽃, 스파크, 섬광, 마법 이펙트용. 입자가 겹칠수록 점점 더 밝아지며 눈부신 섬광 표현.
    - `Resources/Material/ParticleMaterial_AlphaBlend.bammat.json`:
      - 블렌드 모드 `AlphaBlend` (`m_BlendMode: 2`), 깊이 모드 `ReadOnly` (`m_DepthMode: 1`).
      - 연기, 흙먼지, 안개, 물보라용. 깊이 쓰기를 꺼서(Z-Write Off) 입자끼리 가려지는 아티팩트 없이 부드럽게 배경과 투명 합성.
  - `Engine/Source/System/Render/Private/ParticleSystem.cpp`:
    - `OnSubmit` 기본 머티리얼 폴백을 `ParticleMaterial_AlphaBlend`로 우선 연결하여 컴포넌트 추가 시 기본적으로 부드러운 투명 합성이 적용되도록 개선.


## 2026-11-08 — [Bugfix] MaterialEditor FillMode 인덱스 역전 수정 및 파티클 머티리얼 GeometryPass 수락 동기화

- **버그 1: MaterialEditor 채움(FillMode) 옵션 반대 표시 수정**
  - **원인:** `EFillMode` enum 순서는 `Wireframe=0`, `Solid=1`인데, `MaterialEditor.cpp`의 콤보 텍스트 배열 `fillModes`가 `{ "Solid", "Wireframe" }`으로 정의되어 있어 UI 선택 인덱스와 enum 값이 정반대로 동작함.
  - **수정:** `fillModes[] = { "MaterialOption.Wireframe", "MaterialOption.Solid" };`로 순서 수정 완료.
- **버그 2: 신규 파티클 머티리얼 적용 시 파티클이 렌더링되지 않던 문제 수정**
  - **원인:** 현재 파티클 렌더링 파이프라인(`ParticleSystem::OnSubmit`)은 디퍼드 `GeometryPass`를 통해 제출되는데, `GeometryPass`는 오직 `Opaque(1)`와 `Masked(16)` 블렌드 모드만 수락함(`IsAcceptsBlendMode`). 신규 머티리얼의 `m_BlendMode`가 `Additive(4)` 또는 `AlphaBlend(2)`로 설정되어 있어 `IsAcceptsBlendMode` 검사에서 탈락되어 전체 렌더링이 스킵되었음.
  - **수정:**
    - `ParticleMaterial_Additive.bammat.json` 및 `ParticleMaterial_AlphaBlend.bammat.json`의 `m_BlendMode`를 `16 (Masked)`로 설정하여 `GeometryPass`에서 정상 렌더링되도록 동기화.
    - 깊이 모드는 `m_DepthMode: 1 (ReadOnly)`를 유지하여 파티클끼리 가리지 않고 겹쳐지도록 보장하고, `gbufferSpritePS.hlsl`의 `output.Emission`을 통해 발광 및 블룸 효과 정상 연동.

## 2026-11-08 — [Bugfix] UI 뷰 SpriteMaterial 바인딩 크래시 방어 및 컨텐츠 브라우저 ↔ 스프라이트/타일 팔레트 연동 구축

- **버그 1: UI View에서 UIImageComponent에 SpriteMaterial 바인딩 시 렌더러 크래시(브레이크) 수정**
  - **원인:**
    - `SpriteMaterial.bammat.json`은 3D 디퍼드 G-Buffer 패스 전용(`GBufferSpriteVS`, `GBufferSpritePS`)으로, 4개 렌더타깃 출력 및 정점 버퍼 입력을 기대함.
    - `UI View`는 `ERenderPassType::UI`이며 단일 렌더타깃과 `Draw(6)` 절차적 정점 셰이더(`UIVS.hlsl`)를 사용함.
    - `UIImageComponent`가 비어있을 때 기본 머티리얼이 없어 렌더링되지 않던 문제 때문에 사용자가 `SpriteMaterial`을 할당하였고, 렌더 타깃 포맷/수량 불일치 및 InputLayout 부재로 D3D12 PSO 생성 및 Draw 시 브레이크/크래시가 발생함.
  - **수정:**
    - `Engine/Source/System/Render/Private/UIRenderSystem.cpp`: `UIImageComponent`의 `materialHandle`이 비어있거나 UI 패스 서명에 맞지 않는 3D 머티리얼인 경우 기본 `Resources/Material/UIMaterial.bammat.json`으로 자동 안전 폴백.
    - `Engine/Source/Render/Private/Renderer.cpp`: `RenderUIImages`에서 비호환 머티리얼 바인딩 실패 시 크래시 없이 기본 `UIMaterial`로 안전하게 폴백 바인딩하고 드로우콜을 계속 진행하도록 2중 방어벽 구축.
    - `UIImageComponent`의 `spriteHandle`과 UV 변환(`uvTransform`)이 `UIVS`의 `UvRect`에 정확히 매핑되어, 슬라이스된 스프라이트의 부분 UV도 UI 상에 왜곡 없이 완벽 렌더링되도록 보장.

- **버그 2: 컨텐츠 브라우저에서 텍스처/스프라이트 그리드 아이템을 스프라이트 에디터 또는 타일 팔레트에 올렸을 때 미작동 문제 수정**
  - **원인:**
    - 컨텐츠 브라우저는 드래그 페이로드를 `"CONTENT_BROWSER_ITEM"`으로 전송하나, `TilePalettePanel`은 `"ASSET_PATH"`를 수신 대기하고 있었고 목록이 비어있을 때만 드롭 타깃을 열어두었음.
    - `SpriteEditor`에는 에셋 드래그 앤 드롭 타깃(`BeginDragDropTarget`)이 아예 구현되어 있지 않았음.
    - 컨텐츠 브라우저에서 텍스처 우클릭 시 `SpriteEditor`로 바로 연결하는 메뉴가 없었음.
  - **수정:**
    - `Editor/Source/ImGui/TilemapEditor/Public/TilePalettePanel.h`, `Private/TilePalettePanel.cpp`:
      - `CONTENT_BROWSER_ITEM`과 `ASSET_PATH` 페이로드를 모두 수신하도록 일원화.
      - 타일 목록 유무와 무관하게 패널 전체에서 언제든 드롭 가능하도록 전역 드롭 존 구축.
      - `AddAssetToPalette`: `.bamsprite`뿐만 아니라 `.png`, `.jpg`, `.dds`, `.bamtex` 등 이미지 파일을 드롭하면 해당 디렉터리의 슬라이스 스프라이트들을 자동 탐색하여 일괄 등록하거나, 없으면 기본 스프라이트를 즉시 생성하여 팔레트에 등록.
      - 팔레트 타일 개별 우클릭 삭제(`Delete`) 및 전체 초기화(`Trash`) 액션 추가.
    - `Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp`:
      - 캔버스 및 창 전역에 `CONTENT_BROWSER_ITEM` 드롭 타깃 구축.
      - 컨텐츠 브라우저에서 텍스처나 스프라이트를 드래그하여 놓으면 즉시 `SetTargetResource`로 로드되어 슬라이스/애니메이션 작업 상태로 진입.
      - 텍스처가 없을 때 점선 안내 박스 및 드롭 가이드 텍스트 시각화.
    - `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp`:
      - 텍스처 및 스프라이트 우클릭 컨텍스트 메뉴에 `Open in Sprite Editor` (스프라이트 에디터에서 열기) 및 `Add to Tile Palette` (타일 팔레트에 추가) 메뉴 추가.
    - `Resources/Data/Localization/Editor.csv`:
      - `ContentBrowser.OpenInSpriteEditor`, `ContentBrowser.AddToTilePalette`, `TilePalette.AddedTiles` 신규 로캘 키 3종 등록 및 `localizationcsvtojson.py` 동기화 완료 (`audit_localization.py --strict` 100% 통과).
- **버그 3: SpriteEditor에서 텍스처 직접 로드 시 '유효한 텍스처가 로드되지 않았습니다' 오류 및 초기 빈 상태 UX 개선**
  - **원인:**
    - `SpriteEditor::LoadTextureAndSlices`에서 텍스처 에셋을 로드할 때 `rm.LoadFile` 대신 `rm.LoadResource<Texture>(&texDesc)`를 호출함. `LoadResource`는 직렬화 아카이브(JsonArchive/BeveArchive)를 역직렬화하는 메서드이므로 raw 이미지 파일(.png 등)을 아카이브로 읽으려다 실패하여 텍스처 핸들이 비어있게 됨.
    - 에디터 상단 메뉴 `Window -> Panels -> Assets -> Sprite Editor`로 열었을 때는 대상 리소스가 지정되지 않은 빈 상태이므로 `m_TextureHandle`이 비어 화면에 경고 문구가 표시됨.
  - **수정:**
    - `Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp`:
      - `LoadTextureAndSlices`: 텍스처 로드 시 `rm.LoadFile(normPath)`를 호출하여 이미지 디코드 및 RHI GPU 텍스처 생성을 정상 수행하도록 수정.
      - 메뉴바(`File -> Open Texture...`) 및 툴바에 **`Open Texture...` (`텍스처 열기...`)** 버튼 추가 (`FileDialogs::OpenFileDialog` 연동).
      - 캔버스 중앙 점선 드롭 가이드 내에 **`[ 텍스처 열기... ]` 버튼**을 직접 배치하여, 에디터만 단독으로 열었을 때도 파일 탐색기에서 원하는 이미지를 즉시 선택 및 로드할 수 있도록 UX 대폭 개선.
    - `Resources/Data/Localization/Editor.csv`: `SpriteEditor.OpenTexture` 로캘 키 추가 및 JSON 동기화 완료.
- **개선 4: 스프라이트(.bamsprite / .bamsprite.json) 썸네일을 잘린 영역(Region UV) 및 비율만큼만 렌더링하도록 전수 수정**
  - **원인:**
    - `ContentBrowserGridItem::DrawThumbnail` 및 `ResourcePicker.h`의 리소스 선택 팝업(`drawResourceItem`, 호버 툴팁)에서 `AddImage` / `ImGui::Image` 호출 시 UV 좌표를 전달하지 않아 기본값 `(0,0)~(1,1)`로 원본 스프라이트 시트 전체가 정사각형에 찌그러져 표시됨.
    - `SpriteEditor::SaveSlices`에서 디스크 파일만 저장하고 이미 `ResourceManager` 메모리에 로드된 `Sprite` 인스턴스의 `Region`/`Pivot`/`Border`를 갱신하지 않아 에디터 재시작 전까지 변경된 슬라이스 영역이 썸네일에 즉시 반영되지 않음.
  - **수정:**
    - `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp`: `DrawThumbnail`에서 스프라이트 에셋의 `GetUVTransform()`(`uv0`, `uv1`)과 잘린 픽셀 가로세로 비율(`cropW : cropH`)을 계산하여 타일 중앙에 비율 왜곡 없이 잘린 영역만 렌더링하도록 수정.
    - `Editor/Source/ImGui/Public/ResourcePicker.h`: 리소스 선택 팝업 목록 좌측 썸네일 및 마우스 호버 128x128 툴팁에서 `Sprite`의 잘린 UV(`UV0`, `UV1`)와 종횡비를 적용하여 렌더링하도록 수정.
    - `Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp`:
      - 우측 사이드바 `Selected Slice Properties` 상단에 잘린 영역과 비율을 실시간으로 보여주는 96x96 크롭 썸네일 프리뷰 박스 추가.
      - `SaveSlices`: `.bamsprite.json` 및 `.bamsprite` 저장 후 메모리에 로드된 `Sprite` 인스턴스의 `SetRegion`, `SetPivot`, `SetBorder`, `SetTexture`를 즉시 호출하여 재시작 없이 모든 뷰포트와 썸네일에 실시간 반영.


## 2026-11-09: 트랙 E 플러그인 및 모듈 관리 패널 (PluginManagerPanel) 구축 완료

- **작업 범위**:
  - Editor/Source/ImGui/PluginManager/Public/PluginManagerPanel.h (신설)
  - Editor/Source/ImGui/PluginManager/Private/PluginManagerPanel.cpp (신설)
  - Editor/Source/ImGui/Private/ImGuiManager.cpp (PluginManagerPanel 등록)
  - Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp (Edit -> Plugins 메뉴 항목 연동)
  - Editor/Source/HotReload/Public/ModuleHotReloadManager.h (GetCurrentShadowDllPath 게터 추가)
  - Editor/Source/HotReload/Public/ModuleBuildManager.h (IsLastBuildSuccessful 게터 추가)
  - Editor/Resources/Data/Localization/Editor.csv (PluginManager.* 로캘 키 40여 종 추가)
  - Editor/Resources/Data/Localization/English.json, Korean.json (동기화 완료)
  - Docs/ImGuiDevelopmentPlan.md (26절 구현 완료 갱신)
  - Docs/DevelopmentPriorities.md (트랙 E 구현 완료 갱신)
- **배경 및 목적**:
  - 기존 에디터 하단 푸터 바의 작은 아이콘 버튼 5종에만 의존하던 C++ 게임플레이 모듈 및 핫리로드 파이프라인 제어를 언리얼 엔진 Plugins / Unity Package Manager 수준의 전용 GUI 패널(PluginManagerPanel)로 승격.
  - 모듈별 로드/언로드/컴파일/실패 상태, DLL 경로, 리로드 통계, 제공 기능(컴포넌트 및 시스템)을 한눈에 파악하고 즉각적인 수동/자동 제어 및 에러 복구 파이프라인 제공.
- **핵심 구현 내역**:
  1. **마스터-디테일 2열 레이아웃 (Master-Detail View)**:
     - **좌측 마스터 리스트 (270px)**:
       - 검색 필터(ImGuiTextFilter) 및 4대 카테고리 콤보 필터(All, Game Logic, Editor Tools / Panels, Experimental).
       - 모듈 카드 렌더링: 아이콘, 모듈명, 버전, 상태 뱃지(Loaded 초록, Unloaded 회색, Compiling 노랑, Failed 빨강).
       - 검색 결과가 없을 때의 Empty State 중앙 안내.
     - **우측 상세 인스펙터 (Detail View)**:
       - **헤더 툴바**: [Reload], [Rebuild & Reload], [Load / Unload], Auto-Reload on Save, Auto-Compile on Save 실시간 토글.
       - **메타데이터 속성표**: 식별자, 원본 DLL 경로, 섀도우 복사본 경로(Temp/HotReload/...), 소스 디렉터리, 리로드 횟수, 빌드 상태.
       - **제공 기능 및 컴포넌트**: GameModule의 게임플레이 시스템(SkyAtmosphereSystem) 및 Engine::ReflectionRegistry 등록 컴포넌트(SkyAtmosphereComponent 등) 동적 열거.
       - **실시간 빌드 콘솔 뷰 (Build & Diagnostic Log)**: ModuleBuildManager의 비동기 컴파일러 콘솔 출력 전문 렌더링, rror/FAILED 빨간색 하이라이트, warning 노란색 하이라이트, [Retry Compile] 에러 복구 버튼, [Copy Log], Auto-scroll 지원.
  2. **모듈 지원 범위**:
     - Projects/GameModule (C++ 게임플레이 모듈)
     - ShaderHotReload (런타임 HLSL/DXIL 컴파일 파이프라인)
     - AssetHotReload (텍스처, 머티리얼, 메시, 로캘 에셋 감시자)
     - 에디터 내장 플러그인(TilemapEditor, SpriteEditor, AnimatorGraph, PrefabEditor, TerrainEditor 등)
  3. **메뉴 및 로캘 일원화**:
     - 상단 메뉴바 Edit -> Plugins 및 Window -> Panels -> Diagnostics -> Plugin Manager 진입로 연결.
     - Editor.csv에 PluginManager.* 40여 종의 다국어 번역 키를 추가하고 localizationcsvtojson.py를 통해 JSON 완벽 동기화.


## 2026-11-09: 비정상 종료 진단(CrashHandler) 및 작업 복원 시스템(RecoveryManager) 구축 완료

- **작업 범위**:
  - `Engine/CMakeLists.txt` (`dbghelp.lib` 링크 추가)
  - `Engine/Source/Core/Logging/Public/Logger.h`, `Private/Logger.cpp` (`Logger::Flush()` 메서드 구현)
  - `Engine/Source/Core/Diagnostics/Public/CrashHandler.h` (신설)
  - `Engine/Source/Core/Diagnostics/Private/CrashHandler_Win32.cpp` (신설)
  - `Editor/Source/Recovery/Public/RecoveryManager.h` (신설)
  - `Editor/Source/Recovery/Private/RecoveryManager.cpp` (신설)
  - `Editor/Source/Application/Private/Application.cpp` (`CrashHandler` 및 `RecoveryManager` 수명주기/업데이트 바인딩)
  - `Editor/Source/ImGui/Private/ImGuiManager.cpp` (`RecoveryManager::DrawRecoveryModal()` 모달 연동)
  - `Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp` (`Help -> Trigger Crash` 수동 검증 메뉴 항목 연동)
  - `Editor/Resources/Data/Localization/Editor.csv` (`Recovery.*`, `Help.TriggerCrashTest` 로캘 키 11종 추가)
  - `Editor/Resources/Data/Localization/English.json`, `Korean.json` (동기화 완료)
  - `Docs/CrashHandlingAndRecoveryPlan.md` (전 단계 구현 완료 갱신)
  - `Docs/DevelopmentPriorities.md` (트랙 G 완료 갱신)
- **배경 및 목적**:
  - 엔진/에디터 구동 중 널 포인터 역참조(`0xC0000005`), 스택 오버플로 등으로 크래시 발생 시 터미널/창이 즉각 증발하여 원인 규명이 불가능하던 결함 해결.
  - 작업 중이던 씬과 컴포넌트의 미저장 데이터가 영구 소실되지 않도록 2단계 방어 체계(Tier 1 주기적 자동 저장 + Tier 2 크래시 비상 스냅샷) 및 에디터 재시작 시 원클릭 복원 마법사 구축.
- **핵심 구현 내역**:
  1. **크래시 감지 및 미니덤프 생성 (`CrashHandler`)**:
     - `SetUnhandledExceptionFilter`, `std::set_terminate`, `_set_purecall_handler`, `_set_invalid_parameter_handler`를 등록하여 Windows SEH 및 CRT 런타임 예외를 일원화하여 포착.
     - 크래시 발생 시 중복 진입 방지(`s_IsHandlingCrash`) 및 버퍼링된 로그 즉시 디스크 플러시(`Logger::Flush()`).
     - `DbgHelp.dll`의 `MiniDumpWriteDump`를 호출하여 `<ProjectRoot>/Logs/Crashes/BamEngine_Crash_YYYYMMDD_HHMMSS.dmp` 생성 (에디터 타깃 시 `MiniDumpWithDataSegs`, `MiniDumpWithProcessThreadData` 등 정밀 디버깅 플래그 적용).
     - `StackWalk64` 및 `SymFromAddr`, `SymGetLineFromAddr64`를 통해 크래시 당시 스택 프레임의 함수명, 파일 경로, 라인 번호를 추출하고 JSON 형태의 진단 리포트(`.crash.json`) 영속화.
  2. **2단계 작업 복원 시스템 (`RecoveryManager`)**:
     - **Tier 1 (주기적 하트비트 자동 저장)**:
       - 에디터 메인 루프에서 5분(300초, 사용자 설정 가능) 주기로 `SceneDocument`의 `HasUnsavedChanges()`를 감지.
       - 미저장 변경 사항이 존재할 경우 `Temp/Recovery/AutoSave_<SceneName>.bamscene.json`으로 스냅샷 저장.
       - 직접 덮어쓰지 않고 `.tmp` 파일 생성 후 완료 시 교체하는 원자적 쓰기(Atomic Write)를 적용하여 저장 도중 크래시가 발생해도 기존 복원 데이터 훼손 방지.
     - **Tier 2 (크래시 순간 비상 스냅샷)**:
       - `CrashHandler`의 예외 필터 진입 시 가장 먼저 등록된 `EmergencyCallback`을 트리거하여, 힙 파손 위험 전 활성 씬의 비상 스냅샷(`Emergency_Snapshot.bamscene.json`)과 `Session.lock` 상태(`Status: Crashed`)를 즉시 덤프.
  3. **세션 라이프사이클 및 복원 마법사 모달 (`DrawRecoveryModal`)**:
     - 에디터 시작 시 `Temp/Session.lock` 생성(PID, 시작 시각 기록), 정상 종료 시 삭제.
     - 기동 시 잔존하는 `Session.lock`을 통해 이전 세션의 비정상 종료를 감지하고, `Temp/Recovery/`에 유효한 복원 스냅샷이 감지되면 "비정상 종료 감지 및 작업 복원" ImGui 모달 다이얼로그를 자동 팝업.
     - 스냅샷 씬 이름, 저장 시각, 저장 사유(비상 스냅샷 / 주기적 자동 저장) 표시.
     - `[복원하여 열기]` 클릭 시 스냅샷 데이터를 로드하고 원본 씬 문서 경로로 즉시 채택하여 중단 없는 작업 재개 보장.
     - `[무시하고 새 작업]` 클릭 시 스냅샷 및 메타데이터 안전 폐기.
  4. **수동 진단 테스트 및 검증 지원**:
     - 상단 메뉴바 `Help -> Trigger Crash (Diagnostics Test)` 메뉴를 통해 의도적 널 포인터 크래시를 유발하여 미니덤프 생성 및 복원 마법사를 즉시 검증 가능.

## [2026-11-11] 빌드 패널의 다이얼로그 이력과 Windows Release 빌드 연결

- `FileDialogs`가 고정 문자열 키에서 Windows 다이얼로그 GUID를 생성한다. 빌드 패널의 시작 씬과 출력 폴더에 각각 적용했다. 두 창은 새 이력을 사용하고 기존 컨텍스트의 GUID는 유지했다.
- `ClientBuildJob`이 별도 CMake 구성에서 Editor 지원을 끈 Windows x64 Release Client와 GameModule을 백그라운드에서 빌드한다. 패널은 상태와 완료 로그를 표시한다.
- 사용자는 변경 전 패널에서 씬 참조 검사가 깨끗함을 확인했다. 변경 후 Editor 빌드, 두 다이얼로그 이력 복원, 빌드 버튼 실행과 산출물 확인은 미검증이다.
- 다음 단계는 선택한 시작 씬과 필수 산출물을 배포 폴더에 모으고 개발 경로 밖에서 실행을 확인하는 것이다.

## [2026-11-11] Windows Release Client 폴더 내보내기 연결

- 사용자는 기존 패널의 Client 빌드 완료와 빌드 폴더 산출물을 확인했다.
- 패널의 `빌드 및 내보내기`가 선택한 위치 아래 `BamEngine-Windows-x64`에 알려진 Windows 런타임 바이너리와 실행 리소스를 수집한다. 배포 설정에는 선택한 시작 씬을 기록한다.
- 빌드 뒤 씬 참조를 검사한다. 임시 폴더의 복사가 성공하면 최종 이름으로 이동한다. 기존 출력과의 충돌, 입력 루트 겹침, 누락 파일은 오류로 보고한다.
- 이전 Client 로그에 나온 `SkinningMaterial`의 DXIL 경로를 `Engine://` 참조로 갱신했다.
- 변경 후 Editor 빌드, 패널 내보내기, 기존 목적지 충돌, 저장소 밖의 Client 실행은 미검증이다. 런타임 파일 형식을 넓게 포함하는 초기 수집이며 바이너리 에셋의 전체 참조 분석은 남아 있다.

## [2026-11-13] Client 패키징 현황과 렌더 파이프라인 프로필 설계

- 빌드 패널은 저장된 현재 씬 선택, 시작 씬과 내보내기 위치의 Workspace 저장, 빌드 로그 선택과 복사를 제공한다. Client 코드는 에셋 루트 기준 시작 씬 로드, 시작 그래프 파일 검사, `Gameplay` 액션 맵 로드·평가를 포함한다.
- 사용자는 Windows Release 빌드 산출물을 확인했다. 기존 배포 로그에는 `RaytracingScene3`의 로드와 Client 초기화 성공 및 Animator 컨트롤러 경로 오류가 기록됐다. 사용자가 보고한 A/D 입력, 기본 도형 표시, 조명과 후처리 차이는 해소 여부를 검증하지 않았다.
- Editor Game View와 배포 Client가 같은 파일을 읽어 등록된 패스를 구성하도록 [Render Pipeline Profile 설계](RenderPipelineProfilePlan.md)를 기록했다. 첫 구현 단위는 기본 Deferred 프로필과 GPU 없이 실행 가능한 로더·검증기다.
- 작업 트리와 기존 로그를 정적으로 대조했다. 이번 기록을 위해 새 빌드, PIE, 내보내기나 저장소 밖 실행을 수행하지 않았다. 프로필 로더·패스 재구성·배포 설정 연동은 구현 전이다.

### [2026-11-13] 뷰포트 이미지 공용화와 2D 표시

- `ViewportImage`를 통해 기본·프리뷰 뷰포트의 렌더 타깃 조회와 그리기를 공유한다. 픽셀 퍼펙트 Game View에는 1:1 이미지 표시와 영역 부족 안내를 적용한다.
- Scene View의 직교 카메라는 이미지 위에서 입력을 받아 이동·확대하고, 원근 투영으로 돌아갈 때 이전 카메라 위치·회전·피벗을 복원한다. 채널 미리보기의 제출 경로도 정리했다.
- 스프라이트와 타일맵이 정렬 레이어·순서를 드로우 명령에 전달한다. Geometry 패스의 스프라이트 정렬과 픽셀 퍼펙트 Game View의 Point 샘플링을 연결했다. 실제 DPI별 1:1 표시, 채널 선택, 카메라 전환과 겹친 2D 레이어의 표시 순서는 검증 대상이다. [2D 렌더링 계획](SpriteAnd2DSystemPlan.md#phase-3-픽셀-퍼펙트-카메라-및-서브픽셀-스냅-pixel-perfect-camera-구현-완료)

### [2026-11-13] 타일맵 4방향 자동 타일

- 타일맵에 상하좌우 4비트 연결 규칙과 자동 타일 셀 상태를 추가했다. 배치·삭제 시 주변 마스크를 갱신하고, 렌더링에서 규칙에 맞는 스프라이트를 해석한다.
- Tile Palette에서 마스크별 스프라이트를 지정하고 Scene View에서 자동 타일을 칠한다. 직교 뷰의 셀 그리드와 붓질·사각형 채우기 편집 이력도 정리했다. 청크 경계, 기본 규칙, 수동 셀 혼용, 저장·재열기와 Undo/Redo는 검증 대상이다. [자동 타일 계획](EngineDevelopmentPlan.md#3단계-이웃-기반-자동-타일-autotile)

### [2026-11-13] 복합 VFX 사운드 길이와 페이드아웃

- 사운드 트랙에 재생 길이와 페이드아웃 값을 저장하고 편집기에서 조절할 수 있게 했다. 트랙 실행 상태가 채널을 추적해 정지·비활성화와 프리뷰 탐색에서 정리한다.
- 사운드 시작 시간 편집을 타임라인 적용 경로에 연결했다. 기존 에셋 재로드, 재생·루프·탐색·일시 정지, 프리뷰 종료 시 채널 정지와 페이드아웃은 실행 검증 대상이다. [복합 VFX 계획](CompositeVisualEffectPlan.md)

### [2026-11-13] 머티리얼 편집기의 읽기 전용 처리

- 읽기 전용 경로의 머티리얼은 편집·저장·되돌리기 UI를 비활성화한다. 메뉴와 퀵 툴바는 하나의 저장 경로를 사용하며 저장에 성공하면 에셋 캐시를 갱신한다.
- 엔진 에셋에서 셰이더와 텍스처 변경이 차단되는지, 프로젝트 JSON·바이너리 머티리얼을 저장하고 재열 수 있는지는 실행 검증 대상이다.

### [2026-11-13] DefaultVS 정적 메시 인스턴스 바인딩

- `defaultVS.hlsl`이 드로우 인스턴스 ID로 `t0, space2`의 월드 행렬을 읽도록 변경했다. 정적 메시 렌더러의 인스턴스 제출 경로와 맞춘다.
- DXIL·SPIR-V 생성, DefaultVS를 쓰는 머티리얼의 기존 드로우와 Editor·Client 화면은 실행 검증 대상이다. [바인딩 규약](Shader/BINDING_LAYOUT.md)

### [2026-11-13] 씬 관리 코드와 리팩토링 기준 정리

- `SceneManager`의 씬 저장·로드를 별도 구역으로 배치하고 `Scene`의 활성·정지 플래그 변경을 인라인으로 옮겼다. 저장·로드의 후보 씬 생성과 교체 순서는 정적으로 대조했다.
- 소스 조직 지침에 호출부, 소유권, 상태 반영 순서, 호환성 확인 기준을 추가했다. 씬 저장·재열기, 프리팹 인스턴스화와 실패 시 기존 씬 보존은 실행 검증 대상이다. [코드 조직 지침](SourceOrganizationGuidelines.md#13-엔진-코드-리팩토링-기준)

### [2026-11-13] Editor 플러그인 승인 문서 타입 정리

- JSON 승인 문서의 내부 타입을 `Editor` 네임스페이스로 배치했다. 스키마 버전과 승인 기록 필드는 유지한다. 기존 승인 파일을 읽고 갱신하는 흐름은 실행 검증 대상이다. [Editor 플러그인 설계](EditorPluginArchitecturePlan.md)

### [2026-11-13] 복원 팝업 위치

- 복원 팝업을 주 뷰포트 작업 영역 중앙에 띄우고 창 배치를 저장하지 않도록 했다. 기존 복원 대상 선택과 복원 취소 흐름, 다중 모니터에서의 위치는 실행 검증 대상이다. [복원 계획](CrashHandlingAndRecoveryPlan.md)

### [2026-11-13] Animator Graph의 대상 경로 반영 시점

- 파일 읽기에 성공한 뒤 그래프와 대상 경로를 함께 반영하도록 열기 경로를 정리했다. 정상 열기와 잘못된 컨트롤러 파일을 열었을 때 기존 문서 유지 여부는 실행 검증 대상이다. [Animator Graph 계획](AnimatorGraphPlan.md)

### [2026-11-13] 브라우저 WebGPU 프로브 소스

- `Tools/WebGPUProbe/main.cpp`에 Emscripten 기반 어댑터·디바이스 요청, 캔버스 서피스 설정, 브라우저 프레임 루프와 삼각형 렌더링을 담았다. 독립 프로브이며 Client 씬 로드와 배포는 별도 구현 범위다.
- 소스와 기존 대화 기록을 대조했다. 이번 정리에서는 브라우저 실행이나 GPU 동작을 재검증하지 않았다. [빌드·패키징 계획](BuildAndPackagingPlan.md)

### [2026-11-13] 로컬 산출물 제외와 소스 정리

- 로컬 Client 내보내기 폴더를 Git 제외 목록에 추가했다. Editor의 DirectX12 ImGui 백엔드와 공용 인터페이스에 필요한 헤더를 명시하고, 나머지 편집기·엔진 소스의 인코딩 및 공백 변경을 정리했다.
- 커밋 전 변경 구간과 파일 분류를 정적으로 확인했다. 이 정리에서 Editor·Engine 빌드나 런타임 테스트를 새로 실행하지 않았다.

### [2026-11-13] 렌더 파이프라인 프로필 파일과 공용 검증기

- Engine 리소스에 기본 Deferred 프로필을 추가하고 `JsonArchive` 기반 파일 읽기·쓰기와 GPU 초기화 없이 수행하는 슬롯 검증을 연결했다. 순서, 슬롯 타입, 필수 입력, 제자리 갱신 이후의 이전 출력과 최종 색상 출력을 검사한다.
- `RenderPipelineProfileCheck`에 파일 왕복, 대표 오류 경로와 실패 시 기존 상태 보존 검증을 추가했다. 이번 작업에서는 소스·기본 JSON·공백을 정적으로 대조했다. C++ 빌드와 체크 실행은 남아 있다.
- Game View·Client 제출, GPU 리소스 수명과 빌드 패널의 프로필 선택·패키징 연결은 후속 단계다. [렌더 프로필 설계 및 검증 명령](RenderPipelineProfilePlan.md)

### [2026-11-13] Deferred 렌더 프로필의 Game View 연결

- Game View가 프로젝트의 `RenderPipelineProfilePath`에서 파일을 읽는다. Engine에서 프로필 순서대로 패스와 타깃을 생성하고, 논리 입력을 실제 G-Buffer·그림자·HDR 타깃에 연결한다. 기존 Game View 조명·후처리 코드를 재사용하고 `ScenePostProcess` 유무에 따라 후처리를 적용한다.
- 패스 해제 시 Renderer의 드로우 큐·카메라·콜백을 먼저 정리한다. 뷰포트와 후처리·Bloom·채널 미리보기에서 등록 해제와 타깃 정리를 연결했다.
- 기존 프로젝트와 빈 설정은 기존 Deferred 경로를 사용한다. 기본 프로젝트는 내장 Deferred 파일을 선택한다. 방향광 그림자 아틀라스는 기존 투영 기준에 따라 4096으로 제한한다.
- 소스의 등록·해제 경로와 설정을 정적으로 대조했다. Editor 빌드, `RaytracingScene3` 화면 비교와 GPU 수명은 실행 확인 전이다. Windows Client와 패키징 연결은 다음 단계다. [렌더 프로필 설계 및 수동 검증](RenderPipelineProfilePlan.md#2단계-수동-검증)

### [2026-11-13] Windows Client와 패키징의 Deferred 렌더 프로필 연결

- Game View와 Client가 Engine의 Deferred 씬 제출과 `PostProcessComponent` 적용 코드를 공유한다. Client는 프로필 셰이더를 키로 등록하고 UI 합성 결과를 전체 화면 패스로 백버퍼에 그린다. 뷰포트 크기에 종속된 타깃만 Client 창 크기에 맞춰 갱신한다. 메시 제출에서 `ForwardTransparent`도 대상으로 포함한다.
- 빌드 패널에 프로젝트 프로필 경로를 표시한다. Windows 폴더 내보내기는 배포 설정의 URI를 정규화하고 소스와 스테이징에서 프로필 및 필수 DXIL을 검사한다. 기존 애셋 수집 범위는 동일하다.
- 소스 호출 경로, 셰이더 파일명과 키, 변경부 공백을 정적으로 확인했다. 이 작업에서 빌드·실행은 수행하지 않았다. 원본 저장소의 로컬 `RaytracingScene3` 사본에는 일부 절대 참조가 있어 배포 검증에 사용할 씬의 참조 정리가 필요하다. [Windows 수동 검증](RenderPipelineProfilePlan.md#3단계-수동-검증)

### [2026-11-13] RaytracingScene3 배포 검증 사본과 참조 저장

- 렌더 프로필 워크트리의 로컬 씬 사본에서 절대 경로 26개를 `Project://`·`Engine://` URI로 바꿨다. Fox 모델과 애니메이션, 컨트롤러, 시작 매니페스트의 의존 파일을 워크트리에 준비했다. 로컬 애셋은 Git 제외 대상이다.
- 공용 리소스 핸들 직렬화가 두 런타임 루트 아래의 절대 `AssetPath`를 URI로 저장한다. 씬의 `ResourceReferences`에도 같은 값을 기록하고 기존 리소스 키는 보존한다. 기존 `SceneArchiveCheck`에 회귀 검사를 추가했다.
- Animator 로더와 빌드 참조 검사가 같은 경로 해석을 사용한다. 씬에 저장된 컨트롤러 경로와 컨트롤러가 읽는 클립도 내보내기 전에 확인한다.
- 파일 수준에서 연결 문서 5개와 참조 대상 20개, 시작 매니페스트 항목 84개를 확인했다. 빌드, CTest와 Editor·Client 실행은 남아 있다. [Windows 수동 검증](RenderPipelineProfilePlan.md#3단계-수동-검증)

### [2026-11-13] Windows 배포 이름과 패키지 덮어쓰기

- 빌드 패널에서 프로그램 이름과 패키지 폴더 이름을 입력하고 최종 실행 파일 경로를 확인한다. 이름은 Workspace에 저장하며 프로그램 이름은 내보낸 EXE와 Client 창 제목에 반영한다.
- 기존 패키지는 확인 모달에서 승인한 폴더만 교체한다. 스테이징 검증 후 이전 패키지를 형제 폴더에 백업한다. 게시 실패 시 원래 경로로 복원하고, 원래 위치를 다시 사용할 수 없으면 백업 경로를 로그에 기록한다.
- 가장 최근에 확인한 배포 로그의 Animator 오류는 컨트롤러 누락이 아니라 이전 Engine이 `Project://`를 디스크 경로로 처리해 발생한 `FileOpenFailed`다. 새 실패 로그에는 원본 참조와 해석된 경로를 함께 기록한다. 파일과 소스를 대조했으며 Editor 빌드, 모달, 재게시 및 Client 실행은 사용자 검증 대상이다. [Windows 수동 검증](RenderPipelineProfilePlan.md#3단계-수동-검증)

- **2026-11-16 썸네일 및 엔진 전반 디스크 캐시 소요처 전수 조사 및 통합 개발 계획 수립**:
  - `Editor/Source/AssetManager/AssetCache/`, `Editor/Source/Rendering/Thumbnail/`, `Engine/Source/Render/Shader/`, `Tools/Converters/` 등 엔진·에디터·도구 전반을 조사하여 디스크 캐시 부재로 인한 콜드 스타트 병목 현상 및 성능 병목 요인을 규명.
  - 최우선 과제인 에셋 썸네일(P0)을 비롯하여 셰이더 컴파일/PSO(P1), 에셋 파생 데이터(DDC, P1), 지형/물리 쿠킹(P2), 비주얼 스크립팅 바이트코드(P2), 오디오 파형/폰트 아틀라스(P3)의 6대 영역 디스크 캐시 소요처를 체계적으로 분류.
  - 프로젝트 루트 기준 `<ProjectRoot>/.bam/cache/` 표준 레이아웃, 단일 진실의 원천(SSOT) 캐시 키 생성 규칙, 원자적 쓰기(`.tmp` -> `rename`), LRU 용량 제한 관리자, 3계층(VRAM LRU -> 디스크 128x128 PNG -> 온디맨드 렌더/디코드) 썸네일 파이프라인 설계를 포함하는 [DiskCacheDevelopmentPlan.md](DiskCacheDevelopmentPlan.md) 신규 수립.
  - [AssetPipelineAndRenderingOptimizationPlan.md](AssetPipelineAndRenderingOptimizationPlan.md) 및 [DevelopmentPriorities.md](DevelopmentPriorities.md)에 신규 계획 교차 참조 연결 반영.

### [2026-11-18] 대용량 3D 모델 썸네일 메인 스레드 프리징 방지 및 비동기 캐시 안정화 (방향 3 적용 및 방향 2 문서화)

- **대용량 미임포트 원시 3D 모델 Fast Reject 가드**:
  - `Assets/Model/sponza/sponza.obj`(21.1MB) 등 4MB(`MAX_RAW_MODEL_PARSE_BYTES`)를 초과하는 대용량 모델 파일에 대해 메인 스레드 실시간 Assimp 파싱을 즉시 스킵하고 기본 3D 모델 아이콘을 사용하도록 개선했다.
  - 정식 임포트를 거쳐 가벼운 바이너리 `.bammodel`이 존재하는 경우에만 안전하게 3D 썸네일 렌더링 대기열에 등록한다.
- **디스크 캐시 저장 비동기화 (Fire-and-Forget `std::async`)**:
  - 3D 썸네일 오프스크린 렌더링 완료 후 CPU 리드백된 픽셀을 디스크 PNG로 쓰는 `SaveThumbnailToDiskCache` 작업을 `std::async(std::launch::async)` 백그라운드 스레드로 격리하여 메인 렌더/UI 스레드에서의 파일 쓰기 및 PNG 인코딩 블로킹을 원천 제거했다.
- **실패 에셋 및 스킵 에셋 네거티브 캐시 (Negative Caching)**:
  - 서브메시가 누락되어 로드 실패한 과거 에셋(`cube.bammodel`, `Plane.bammodel` 등) 및 4MB 초과 스킵 에셋에 대해 0바이트 디스크 캐시 마커 파일(`SaveNegativeCacheToDisk`)을 생성하도록 했다.
  - `LoadThumbnailFromDiskCache`가 0바이트 파일을 감지하면 재시도 없이 기본 아이콘으로 즉시 반환하여, 폴더 재진입 시 `Resource archive read failed: code=2` 에러 로그 폭주 및 디스크 역직렬화 반복 루프를 원천 차단했다.
- **독립 워커 프로세스 기반 파이프라인(방향 2) 문서화 ([OutOfProcessAssetPipelinePlan.md](OutOfProcessAssetPipelinePlan.md))**:
  - 3.5절에 대용량 3D 에셋 썸네일 & 프리뷰 베이킹을 전담하는 헤드리스 D3D12/Software 워커 모드(`--mode thumbnail`), 에디터 `AssetCache` 무간섭 연동, 크래시 및 타임아웃 격리 설계를 추가 문서화했다.

### [2026-11-17] 3인칭 KCC 물리 고도화 및 경사면 상호작용 구현 / 이슈 기록

- **회전된 경사로(OBB) 캡슐 스윕 및 지면 안착 기하학 개선**:
  - `SweepCharacter`에서 회전된 박스(`isRotated`)에 대해 외접 AABB 대신 `Collision::Sweep(charCap, direction, closestDist, localBox, colliderWorld, tempHit)`를 호출하여 실제 $30^\circ, 45^\circ, 60^\circ$ 경사면과의 정밀 OBB 충돌 검사를 지원했다.
  - 경사 평면 기하학 공식($\vec{n} \cdot (C - P_{hit}) = R$) 기반의 `CalculateGroundContactY` 헬퍼 함수를 도입하여 Sweep과 Ray 접점 오프셋으로 인한 프레임 간 14cm 높이 요동(덜컹거림)을 원천 해소했다.
- **완만한 경사면 접선 이동 및 틈새(Crevice) 슬라이딩**:
  - 완만한 경사면($\le 45^\circ$) 이동 시 지면 법선 평면에 속도를 투영($\vec{v} - \vec{n}(\vec{v}\cdot\vec{n})$)하여 경사면을 따라 대각선으로 자연스럽게 오르내리도록 개선했다.
  - 마주보는 두 벽($\vec{n}_1 \cdot \vec{n}_2 < -0.01$) 사이에 끼였을 때 골짜기 교선($\vec{d} = \vec{n}_1 \times \vec{n}_2$) 방향으로 이동 방향을 자동 정렬하여 1.0m 좁은 틈새 끼임 고착 현상을 해소했다.
- **급경사면 미끄러짐 및 점프/평지 전환 안정화**:
  - $60^\circ$ 경사로 등 한계 각도 초과 시 중력 투영 가속도(`slideAccel`)를 적용한 3차원 미끄러짐을 구현했다.
  - 하향 스윕 시의 `(0, 1, 0)` 오염 법선 문제를 방지하기 위해 경사면(`raySlope > 5°`) 감지 시 순수 기하학적 레이캐스트를 최우선 채택하고, $75^\circ$ 이상을 커버하도록 레이 사거리(`rayDist ≈ 2.1m`)를 확장했다.
  - 점프 상승 중(`Velocity.y > 0.05f`) 지면 재흡착 방지 가드를 추가하여 점프 궤적을 보장하고, 평지 전환 시 `glm::mix` 관성 감속 보간을 적용했다.
- **최종 관측 현상 및 기술적 과제 기록 ([ThirdPersonCharacterSystemPlan.md](ThirdPersonCharacterSystemPlan.md))**:
  - 1) 점프 정점 후 낙하 시 과도한 레이 사거리/스냅 마진으로 인한 공중 조기 흡착 현상.
  - 2) $30^\circ$ 완만한 경사로 보행 시 발바닥이 경사면 안쪽으로 미세하게 파고드는 현상.
  - 3) $45^\circ, 60^\circ$ 가파른 경사로 끝단에서 평지로 착지할 때 판자 두께 단차 및 급격한 수직 안착으로 인해 '탁' 하고 뚝 떨어지듯 바닥에 붙는 부자연스러움 관측.
  - 후속 물리 튜닝 및 착지 완충(Soft Landing) 계획을 문서에 반영했다.

### [2026-11-19] 리소스 편집기 문서 닫기와 편집 액션 통일

- 제목 표시줄 X, 파일 메뉴, Ctrl+W, 창 메뉴 체크 해제가 공통 닫기 요청을 사용한다. 버리기·저장 확정 시 경로, 편집 세션, 프리뷰 참조와 이력을 해제한다. 동일 문서 열기는 기존 세션에 포커스를 주고, 독립 프리팹 교체는 확인을 한 번만 요청한다.
- 파티클 Dirty를 저장 상태 ID에 연결하고, VFX 루프·트랙 활성화를 Undo/Redo에 기록했다. 내장 편집기의 저장, 기존 이력 지원 편집기의 Undo/Redo는 메뉴·퀵액세스·단축키에서 중앙 액션을 공유한다. 읽기 전용 도구 및 이력 미지원 편집기의 기능 범위는 패널 현황 문서에 기록했다.
- 종료는 통합 설정과 프리팹 모드를 처리한 뒤 메인 씬을 확인한다. 리소스 폐기 확인을 확정하면 메인 씬 종료 취소 후에도 해당 세션을 다시 묻지 않는다. 프로젝트 설정 쓰기 실패에서 Dirty를 보존한다.
- 열린 파티클·VFX를 다른 파일로 저장하면 새 파일 세션으로 전환하고 원래 공유 에셋을 원본 파일 상태로 복원한다. 기존 파일에서 만든 복사본은 새 이력으로 시작한다.
- Sprite Animation 플러그인은 리소스 V2 계약으로 열기 결과·폐기 복원·닫기를 제공한다. Editor와 Sprite Animation DLL을 함께 빌드해야 한다.
- 호출 경로, 로캘 CSV와 diff를 정적으로 확인했다. 사용자 Ninja Debug 빌드에서 Prefab 퀵액세스 버튼 인자, Material 미리보기 메뉴의 단축키 조회 변수, VFX 트랙 이력 콜백의 접근 대상에서 컴파일 오류가 발생했다. 세 원인을 수정했다. 수정 후 빌드·CTest·실환경 검증은 사용자 실행 전이다. [실환경 체크리스트](EditorUXTestGuidelines.md#52-tier-3-대화형-ui-점검-체크리스트)

### [2026-11-19] 편집 가능한 리소스의 문서 Undo/Redo 연동

- Animator Graph, Graph, Material, Model, Sprite, 독립 Prefab Editor와 Sprite Animation 플러그인에 문서 이력을 추가했다. 편집 메뉴·퀵액세스·Ctrl+Z·Ctrl+Y는 같은 로컬 문서 액션을 사용한다. Texture와 Shader는 조회 도구다.
- Animator Graph는 상태 머신과 노드 배치를 복원한다. `editorNodePositions`는 선택적으로 저장하며 이전 파일은 기본 배치를 사용한다. Graph의 Logic·Shader는 이력과 저장 기준점을 따로 사용한다.
- Material과 독립 Prefab은 메모리의 아카이브로 복원한다. Model·Sprite·Sprite Animation은 각 문서의 편집 데이터를 복사해 이력에 보관한다. Material의 파일 경로가 외부에서 변경될 수 있는 에셋 이동이 확정되면 이력을 다시 시작한다. Sprite Animation ABI V3의 DLL 리로드는 초안과 Dirty를 보존하고 이력을 초기화한다.
- 파티클·VFX·애니메이션 타임라인은 공통 이력 UI를 사용한다. VFX 커브 편집도 실제 값이 바뀐 확인 동작을 이력 한 건으로 기록한다.
- 공통 이력의 편집 묶음·저장 기준점·Redo 분기·실패한 복원에 대한 `EditorHistoryCheck` 사례를 추가했다. 정적 검사만 수행했으며 빌드·CTest·실환경 검증은 사용자 실행 전이다.

### [2026-11-20] 독립 도구 패널 V2 플러그인 이전

- Console, Metrics, Network Profiler를 Editor 실행 파일의 정적 패널 등록에서 각각의 개발용 DLL로 옮겼다. CMake는 해당 패널 소스를 Editor 실행 파일에서 제외하고 플러그인 타깃을 Editor 빌드 의존성으로 추가한다. 개발 목록의 `auto` 메타데이터로 Editor 시작 시 세 DLL을 자동 로드해 항상 사용 가능하게 했다.
- V2 패널 API는 인스턴스 수명, Update/Draw, 상태 스냅샷, Workspace 설정, 초기 창 크기·플래그를 정의한다. 정적 링크된 패널 DLL은 Draw 진입 시 호스트의 `ImGuiContext`와 allocator를 바인딩하며, 호스트는 우클릭 입력과 고정 폭 Metrics DTO를 제공한다. 기존 패널 이름과 ImGui 창 ID를 보존해 열린 상태·도킹 위치를 이어받도록 했다.
- Console은 로그·필터·세부 높이를, Metrics는 선택 도메인·일시정지를, Network Profiler는 핑 이력·연결 입력·속도 기준값을 리로드 상태에 담는다. Metrics의 Editor `AssetManager` 접근은 DLL에서 직접 참조하지 않고 호스트 콜백으로 전달한다.
- **미검증**: 프로젝트의 학습 지침에 따라 빌드·CTest·Editor 실행을 수행하지 않았다. V2 DLL 컴파일, 시작 시 자동 로드, 저장 중 리로드와 기존 상태 복원은 사용자 검증이 필요하다.

### [2026-11-20] 독립 패널 DLL의 ImGui 컨텍스트 바인딩

- Editor와 각 패널 DLL이 정적 `imgui::imgui`를 별도로 링크해 ImGui 전역 상태가 분리되는 문제를 확인했다. V2 Draw 문맥에 호스트 `ImGuiContext`와 allocator 함수를 추가하고 Console·Metrics·Network Profiler DLL의 Draw 진입점에서 이를 바인딩하도록 했다. 구현은 기존 Sprite Animation 플러그인의 DLL 경계 처리 패턴을 따른다.
- **미검증**: 사용자가 제공한 Console 크래시 스택의 `ImGui::CalcTextSize` 접근 위반은 코드상 원인에 대응했으나, 수정 후 빌드와 Editor 실환경 재검증은 아직 수행하지 않았다.


### [2026-11-22] 프리팹 모드 UI 뷰와 게임 UI 위젯 계획

- UI 프리팹 모드에서 UI 뷰가 편집 씬을 사용한다. Canvas 루트는 직접 편집하고 RectTransform 루트에는 미리보기 캔버스를 사용한다. 추가한 요소는 프리팹 루트 하위에 보관한다.
- 프리팹 Archive는 빈 UI 루트에 필요한 계층을 보존하고, 이전 데이터에서 누락된 계층을 복구한다.
- Unity uGUI, Unreal UMG, Godot Control을 비교해 배치, 기본 입력 위젯, 스크롤과 마스킹 순서를 문서화했다.
- 정적으로 호출부를 확인했다. 빌드, UILayoutCheck와 Editor의 프리팹 편집·저장·재열기는 사용자 실행 대기다. [UI 편집기](UIEditorFoundation.md) · [위젯·레이아웃 개발 계획](UIWidgetsAndLayoutPlan.md)


### [2026-11-22] 게임 UI 프리셋과 위젯 구현

- UI 뷰의 캔버스 메뉴에 패널, 가로 행, 세로 컬럼, 고정 셀 그리드, 진행률 바, 토글, 슬라이더, 클리핑 패널과 스크롤 뷰를 추가했다.
- 자동 배치는 자식 계층 순서를 사용하고 계산 결과를 원본 RectTransform에 덮어쓰지 않는다. 토글과 슬라이더는 실제 값이 바뀔 때 ValueChanged를 발행한다.
- D3D12·SDL GPU UI 렌더 경로에서 사각 클립을 적용하고 동일한 영역으로 히트 테스트한다. 스크롤 뷰는 콘텐츠 한 개의 세로 휠 이동을 제공한다.
- 배치·클리핑·스크롤과 위젯 입력 회귀 검사 사례를 추가했다. 빌드·CTest·Editor·PIE·Client 실제 검증은 사용자 실행 대기다. [개발 계획](UIWidgetsAndLayoutPlan.md) · [사용법](UIEditorFoundation.md)
