# 통합 개발 우선순위와 완료 기준

- 작성일: 2026-10-24 (최종 갱신: 2026-10-30)
- 상태: 계획 및 2026-10-30 작업 트리 정적 점검 반영 / 빌드·실행·GPU 성능 검증은 별도 수행 기준을 따른다.
- 공통 기준: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md)
- 분류와 이관: [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md), [BuildConfigurationAndPreprocessing.md](BuildConfigurationAndPreprocessing.md)

## 1. 문서의 역할

Engine·Render·Editor·Client 계획에서 무엇을 먼저 연결하고 무엇을 보류할지 결정하는 진입 문서다.
기능의 상세 계약을 복사하지 않고 원래 계획의 절과 연결한다.
원문에 있는 최종 목표를 삭제하거나 게임·강화학습·2D/3D 아바타·제작 도구 중 하나를 제외하지 않는다.

기준 문서와 소유 범위는 다음과 같다.

- [EngineDevelopmentPlan.md](EngineDevelopmentPlan.md): 실행 기능·시간·수명·게임·학습·아바타·런타임 설정 계약.
- [RenderingDevelopmentPlan.md](RenderingDevelopmentPlan.md): RHI·제출·가시성·기하·조명·GPU 자원·성능 기준.
- [ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md): 편집·미리보기·문서 보호·탐색·리소스 선택과 제작 흐름.
- [BuildAndPackagingPlan.md](BuildAndPackagingPlan.md): Client 시작·경로·Build/Cook/Stage/Package. 현재 구현 보류.
- [ReflectionArchiveGuidelines.md](ReflectionArchiveGuidelines.md): 독립 모듈과 엔진 연결·저장/복원 정확성.
- [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md): 공유 바인딩 규약. 실제 셰이더·백엔드와 대조한다.
- [ImplementationLog.md](ImplementationLog.md): 작업별 구현·검증 근거. 계획의 체크 표시로 대체하지 않는다.
- [VisualScriptingBlueprintPlan.md](VisualScriptingBlueprintPlan.md): 비주얼 스크립팅과 블루프린트 개발 계획 및 런타임 게임플레이 연동.
- [ModuleHotReloadOptimization.md](ModuleHotReloadOptimization.md): C++ 모듈 핫리로드 및 빌드 시간 단축 계획.

전체 작업 선택은 이 문서, 기능 내부의 구현 순서·실패 정책은 상세 계획을 따른다.
새 사용자 결정이 우선하며 충돌이 생기면 이 문서와 해당 계획을 함께 갱신한다.

## 2. 우선순위와 상태의 해석

- P0: 현재 변경의 기준 상태와 작은 정리. 다음 기능의 원인을 추적할 수 있게 한다.
- P1: 편집 데이터 보호·공통 적용 경로·계측·렌더 계약처럼 여러 작업이 공유하는 기반.
- P2: 기존 기반을 실제 작은 실행 예제로 연결하고 결과물별 첫 동작을 만든다.
- P3: 검증된 기본 기능을 콘텐츠 제작·규모·표현으로 확장한다.
- P4: 요구·측정·장치 조건이 확인된 후 선택하는 큰 기능과 연구.

이는 일정이나 중요도 점수가 아니며 모든 P1 작업이 끝나야 모든 P2 작업을 시작한다는 뜻이 아니다.
선택한 작업의 실제 선행 조건만 만족하면 독립 경로를 진행할 수 있다.
확인된 데이터 손상·충돌·기존 기능 실패는 이 순서보다 먼저 수정한다.

상태는 계획 / 코드 존재·정적 확인 / 실행 검증 / 성능 확인 / 보류를 구분한다.
아래 완료 기준은 수행해야 할 조건이며 이번 문서에서 통과시킨 결과가 아니다.
날짜가 있는 기존 검증 기록도 현재 작업 트리 전체의 통과로 확대하지 않는다.

## 3. P0: 기준 상태와 작은 구조 정리

### P0-A. 현재 변경과 재현 기준 확인 (최우선 정리 대상)

- **2026-10-30 현황**:
  - 작업 트리에 컴포넌트 폴더 분리(P0-C), 문서 보호(P1-A), 설정 패널(P1-B), 비주얼 스크립팅 및 모듈 핫리로드(P2-A), 렌더링 인스턴싱·LOD·타일 라이트 컬링·포스트프로세스(P2-B) 등 약 240여 개 파일이 미커밋 상태로 함께 섞여 있다.
  - 추적 대상이 아닌 빈 파일·임시 부산물(`Editor/Source/Application/Private/ApplicationSettings.cpp` 0바이트 파일, `Tools/GraphCompiler/test_graph.gen.cpp`, `Resources/Shader/default_*.cso`, `archive-example/`)이 작업 트리에 남아 있다.
- **조치 기준**:
  - 불필요한 임시/빈 파일을 먼저 정리하고, 기존 미커밋 변경과 마지막 확인한 빌드·실행 상태를 구분한다.
  - 사용자 수행 빌드·관련 검사에서 실패가 있으면 해당 변경 범위부터 해결한다.
- 완료 기준: 임시 부산물이 제거되고, 새 수정으로 발생한 문제와 기존 문제를 구분할 기준이 있다.

### P0-B. 표준 헤더의 PCH 중앙화 및 전처리기 구성 정리

- **2026-10-30 현황**:
  - `Projects/GameModule/CMakeLists.txt`에 PCH(`target_precompile_headers`)가 적용되어 모듈 핫리로드 빌드 시간이 단축되었다.
  - 반면 [BuildConfigurationAndPreprocessing.md](BuildConfigurationAndPreprocessing.md) 4절의 정리 대상은 아직 미적용 상태다:
    - `BAM_EDITOR` / `BAM_SHIPPING` 타깃 매크로 미정의.
    - `Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp`의 `m_DebugRenderer` 및 `Engine/Source/Core/Public/Vertex.h`의 `DebugVertex`가 여전히 `#ifdef _DEBUG`에 묶여 있어 Release 에디터에서 콜라이더 디버그 표시가 누락됨.
    - `Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp`의 `OutputDebugStringA` + `std::terminate()`, `Engine/Source/Graph/Public/GraphGameplayLibrary.h`의 `std::cout` 사용이 남아 있음.
- **조치 기준**:
  - [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md) 8절 및 [BuildConfigurationAndPreprocessing.md](BuildConfigurationAndPreprocessing.md) 4절에 따라 PCH 공급과 빌드 구성 매크로를 정리한다.
- 완료 기준: 영향 타깃에서 필요한 정의가 제공되고, Release 에디터에서 디버그 드로우가 누락되지 않으며 `std::terminate`/`std::cout` 직접 호출이 제거된다.

### P0-C. 소유 위치가 명확한 파일부터 정리

- **2026-10-30 현황 (코드 반영됨)**:
  - 기존 `Engine/Source/Core/Public/`의 묶음 컴포넌트 헤더(`AudioComponents.h`, `CoreComponents.h`, `PhysicsComponent.h`, `RenderComponents.h`, `UIComponents.h`)를 제거하고 `Engine/Source/Core/Public/Components/<Domain>/` 도메인별 개별 헤더로 분리 완료.
  - `Entity.h/.cpp`를 `Engine/Source/World/Entity/Public|Private/`로 이동하고, 모델 인스턴스화 로직을 `Engine/Source/World/Scene/Private/SceneModel.cpp`로 분리 완료.
- 완료 기준: 참조·생성 입력·관련 검사와 문서 경로를 추적할 수 있고 기존 동작을 보존한다.

전체 폴더 재배치·싱글턴 전면 교체·엔진 다중 DLL 분리는 P0의 완료 조건이 아니다.

## 4. P1: 안전한 제작과 측정 가능한 공통 기반 (우선순위 재조정 반영)

> **우선순위 재조정 사유 (2026-10-30)**:
> P2-B(렌더링 S3 인스턴싱/LOD, S4 타일 라이트 컬링, S5 반구형 Sky IBL) 코드가 P1-C(S1 계측, S2 바인딩 계약)보다 먼저 작성되었으나, 정적 점검에서 **S4 컴퓨트 셰이더의 XY 타일 절두체 판정 누락 및 0-광원 폴백 버그, S3 다중 뷰포트 LOD 상태 변이 및 비인덱스 메시 인스턴싱 누락, S2 문서·백엔드 불일치**가 확인되었다. 따라서 신규 기능 확장보다 **P1-C(렌더링 계약 동기화·기구현 S3/S4 결함 수정·S1 계측 도입)**를 P1 최상위로 끌어올려 먼저 안정화한다.

### P1-C. [최우선 격상] CPU/GPU 계측, 바인딩 계약 동기화 및 기구현 S3·S4 결함 수정

- **2026-10-30 현황**:
  - **S1 (계측 및 지원 조회)**: `RHIFrameMetrics`는 여전히 드로우콜·정점/인덱스 수 4개 항목만 집계하며, 패스별 CPU 제출 시간·GPU Timestamp Query·메모리/디스크립터 사용량·기능 지원 질의 API가 미구현 상태다.
  - **S2 (바인딩 계약)**: [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)를 실제 `DirectX12RHI` 루트 시그니처(`space0`~`space3`, Root Parameter `[0]`~`[10]`) 기준으로 갱신 완료했으나, `SDLGPURHI`는 `DrawIndexedInstanced`(`NotImplemented`), `BindInstanceBuffer`, `BindComputeStorageBuffer`, `Dispatch`, `ResourceBarrierUAV`가 미구현 상태다.
  - **기구현 S3·S4의 즉시 수정 대상 결함**:
    1. `Resources/Shader/hlsl/tiledLightCullingCS.hlsl`: 타일별 깊이(`[minTileZ, maxTileZ]`)만 비교하고 **타일 상·하·좌·우 XY 절두체 평면 컬링이 누락**된 문제 및 View-space Z 부호(`viewZ > 0.0f`) 전제 점검.
    2. `Resources/Shader/hlsl/lightingPS.hlsl`: `tileLightCount == 0`일 때 타일 컬링 미사용으로 오판하여 전체 `numLights`를 순회하는 폴백 버그 수정(상수 버퍼에 명시적 타일 컬링 활성화 플래그 분리).
    3. `Editor/Source/ImGui/ViewportPanel/Private/ViewportDeferredPipeline.cpp`의 타일 컬링/버퍼 바인딩 누락 및 `SceneViewportPanel::SubmitLightingPass`의 중복 `BindConstantBuffer(..., 3)` 호출 제거.
    4. `Engine/Source/Render/Private/Renderer.cpp`의 비인덱스 메시(`indexCount == 0`) 인스턴싱 드로우 누락 및 `MeshRenderSystem::OnSubmit`의 단일 카메라 기준 `currentLODIndex` 변이 보완.
- 완료 기준: S3·S4의 정적 결함이 해결되고, S1 패스별 CPU/GPU 계측을 통해 고정 씬에서 단일 드로우/전체 광원 순회 대비 실제 비용 변화를 수치로 확인할 수 있으며, 백엔드 미지원 경로가 안전하게 처리된다.

### P1-A. 편집 데이터와 저장 실패 보호

- **2026-10-30 현황**:
  - `Editor/Source/Document/`(`IEditorDocument`, `SceneDocument`, `EditorDocumentManager`, `EditorDocumentController`)와 `SceneEditHistory`(`HasUnsavedChanges`, `MarkSaved`, `CanFinalizeEditsFor`)가 구현되어 `ToolBar`의 New/Load/Save/Close 및 앱 종료 흐름에 연결되었다.
  - **남은 항목**: [ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md) 11절 A의 **Undo/Redo 명령 스택 및 저장 기준점 시각화 창(`HistoryPanel`)** 미구현(자동 백업·복구 UI는 다음 단계 유지).
- 완료 기준: 취소가 씬 교체를 막고, 저장 실패 후 초안·기존 파일·변경 표시가 보존되며, `HistoryPanel`에서 현재Undo/Redo 위치와 저장 기준점을 확인할 수 있다.

### P1-B. 설정 적용 경로와 기존 저장/복원 연결

- **2026-10-31 구현 확인**:
  - EditorPreferences·EditorWorkspace·ProjectSettings의 저장 범위와 EditorSettingsService 호출 경로가 분리되어 있다.
  - GraphicSettingPanel에 창 크기·전체화면·UI 배율·목표 FPS·VSync와 기본 Scene/Game View의 해상도 편집이 연결되어 있다.
  - 뷰포트별 초안을 유지하며 실제 리사이즈 결과 확인 후 해당 뷰포트를 캡처하고 저장한다.
  - 창 변경의 15초 확인·자동 복구, 적용 실패와 저장 실패의 구분, 저장 차단 안내와 토스트가 연결되어 있다.
  - 파일 메뉴의 설정 진입점과 기존 창 메뉴가 같은 패널을 사용한다.
  - Scene View의 후처리 초기화는 이름·효과 초기화 후 렌더 타깃·패스를 생성하는 순서로 정리되었다.
- **자동 검증**:
  - EditorSettingsCheck가 2026-10-31 12:42 통과했다.
  - 설정 데이터의 왕복 저장·기존 파일 교체·읽기 실패 시 값 보존·저장 실패·임시 파일 정리를 검사한다.
- **실행 검증 대기**:
  - 재실행 후 설정·뷰포트 해상도·편집 카메라·렌더 패스·도킹 배치 복원.
  - 창 변경 취소·시간 초과, 실제 GPU 리사이즈 실패와 복구.
  - 언어 변경 시 선택·초안 유지와 적용하지 않은 초안의 저장 제외.
- **별도 후속 범위**:
  - EngineDevelopmentPlan.md 19절의 공통 런타임 설정 API.
  - 동적 생성 뷰포트의 재생성.
  - Client 시작·설정·배포 연결은 BuildAndPackagingPlan.md의 보류 결정을 따른다.
- 완료 기준: 코드 구현·자동 검증·실제 UI 검증을 구분하고 실행 검증 결과를 기록한다.

### P1-D. 기본 에셋 판정·열기와 작업량 제한

- **2026-10-30 현황**:
  - `ContentBrowserPanel` 그리드 행 단위 클리핑(`ImGuiListClipper`)과 `ResourceReference` 도입 완료.
  - `ResourcePicker.h`에 텍스트 필터 및 디스크 상의 미로드 에셋 선택 로직이 추가되었으나, **팝업(`ResourceChoices`)이 열려 있는 동안 매 프레임 `recursive_directory_iterator("Resources")`로 모든 디스크 파일을 동기 순회하여 심각한 프레임 드랍이 발생하고 대상 핸들 타입과 무관한 파일까지 노출**되는 결함 확인.
  - **개선 계획 및 남은 항목**:
    1. `Editor/Source/ImGui/Public/ResourcePicker.h` 디스크 스캔 최적화 및 핸들 타입별 필터링:
       - **핸들 타입 기반 확장자 필터링**: `ResourceHandle<T>`의 `T`에 매핑된 호환 확장자(예: `Texture` -> `.png`, `.jpg`, `.dds` / `Material` -> `.bammat`, `.bammat.json` / `Model` -> `.fbx`, `.obj` / `Audio` -> `.wav`, `.mp3` 등)에 해당하는 파일만 디스크 후보로 수집.
       - **팝업 진입 시 1회 스캔 및 캐싱**: 매 프레임 재귀 탐색을 방지하고, 팝업이 처음 열리는 시점(`IsWindowAppearing` 또는 팝업 오픈 상태 플래그)에 1회만 후보 목록을 캐싱하여 렌더링 프레임 저하 방지.
       - **AssetCache/메타데이터 색인 연계**: 향후 `AssetCache` 색인 목록을 직접 조회하여 디스크 I/O 없이 인스펙터 리소스 피커 즉각 렌더링.
    2. `ContentBrowserPanel`의 검색 범위 선택(`현재 폴더` vs `전체 리소스 폴더`, 14-E), 외부 프로그램 열기 메뉴(14-A), 썸네일 축소 생성 및 캐시 예산 제한(14-D).
- 완료 기준: JSON/바이너리의 같은 콘텐츠 타입을 구분하고, 리소스 피커가 프레임 드랍 없이 해당 핸들 타입(`T`)과 호환되는 에셋만 디스크에서 즉각 선별·표시하며, 미로드 에셋 선택·검색 범위 전환·손상/미지원 처리가 동작한다.

## 5. P2: 작은 결과물과 기본 실행 경로

### P2-A. 게임 실행의 연결 및 비주얼 스크립팅 마감

- **2026-10-31 구현 현황**:
  - [VisualScriptingBlueprintPlan.md](VisualScriptingBlueprintPlan.md) Phase 1(`GraphSystem` 라이프사이클·틱 실행), Phase 2(`GraphActorLibrary`, `GraphGameplayLibrary`), Phase 3(`GraphVariable` 및 변수 Get/Set), Phase 4.1(`ModuleId` 언레지스터, `Graph::Reconcile`, 200ms 디바운스 + `/p:BuildProjectReferences=false` + PCH 기반 1.5초대 핫리로드) 완료.
  - **Phase 4.2 UI/UX 완성**:
    - **노드 레이아웃 고도화**: `SetCursorPosX` 오염 제거, 2열 컬럼 Group 분할 레이아웃, `then ▶`/`result` 등 Out 핀 우측 테두리 100% 밀착 정렬.
    - **와이어 자동 교체 (Replace Connection)**: 연결된 입력/실행 핀에 새 와이어 꽂을 시 기존 링크 자동 해제 및 교체.
    - **Electronic Nodes 하이브리드 배선**: Exec 실행선은 반듯한 직선 + 두께 2.8px, Data선은 부드러운 곡선 + 두께 1.8px 차별화, 툴바 실시간 3단계 토글(`All Curved` / `Electronic` / `All Straight`).
    - **리루트(Reroute/Knot) 노드 완성**: 더블클릭 마우스 위치 정확 생성, 네모 박스 제거 및 점 하나로 드래그 이동, 점 중심(`dotCenter`) 와이어 피벗 결합, 단독 생성 허브 및 1:N 다중 분기 지원.
    - **입력 일원화 및 내비게이션**: 마우스 휠(미들클릭) 드래그 캔버스 패닝(`NavigateButtonIndex = 2`), 지침 14절 준수(`InputManager` 일원화).
    - **정렬 및 그룹화**: `Shift+W/S/A/D/H/V` 자동 정렬/분배, 코멘트 그룹 박스(`C`), 노드·와이어 우클릭 메뉴.
  - **남은 항목**:
    1. **Phase 4.3 (`GraphSystem` 런타임 핫스왑)**: PIE 실행 중 `GameModule.dll` 리로드 시 활성 엔티티들의 `GraphRuntimeState` 및 `GraphExecutor`를 재바인딩하여 크래시 없이 새 C++ 노드 로직을 즉시 반영.
    2. **`Client/Source/Main.cpp` 시스템 등록 누락 수정**: `Editor`와 달리 `Client`의 `InitializeSystems()`에 누락되어 있는 `GraphSystem`, `PhysicsSystem`, `TerrainSystem` 등록.
    3. **엔진·프리팹 기본 연결**: `PickingManager` 가시성에 의존하지 않는 씬 레벨 Collider Raycast API([EngineDevelopmentPlan.md](EngineDevelopmentPlan.md) 8절), 독립 Sprite Flipbook 재생기(6절), 에디터 초기 Prefab 생성·배치·Undo/Redo UI([ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md) 10절 1단계).
    4. *참고*: `Tools/GraphCompiler/GraphCompiler.py`(Phase 6)는 구형 `GraphComponent`/`GameObject` 대상의 57줄 POC 수준이므로 P3/P4 패키징 단계로 이관한다.
- 완료 기준: PIE 중 모듈 핫리로드와 그래프 실행이 안전하게 유지되고, 입력으로 작은 씬을 조작하며 화면 표시 여부와 무관한 충돌 질의·두 인스턴스의 독립 상태를 확인한다.

### P2-B. 기본 렌더 확장 (S3·S4·S5 검증 및 완성)

- **2026-10-30 현황**:
  - **S3**: `InstanceData`, `LODGroupComponent`, `MeshRenderSystem` 화면 덮개 비율 + 히스테리시스 LOD 선택, `Renderer::RenderMeshes` 인스턴스 배칭, `gbufferVS.hlsl`/`shadowDepthVS.hlsl`의 `StructuredBuffer<InstanceData> g_Instances : register(t0, space2)` 반영됨.
  - **S4**: `ShaderBuild.cmake`의 `*CS.hlsl`(`cs_6_0`) 빌드, `DirectX12Pipeline` Compute PSO, `DirectX12RHI::Dispatch`/`ResourceBarrierUAV`, `LightManager::DispatchLightCulling` 및 `tiledLightCullingCS.hlsl` 반영됨.
  - **S5 및 후처리**: `lightingPS.hlsl`의 반구형 Sky IBL(`LIGHT_TYPE_SKY` + `EnvBRDFApprox`), 접촉 그림자, `ShadowStrength`와 `PostProcessPipeline`(`BloomEffect`, `PostProcessMaterialEffect` 기반 Vignette/ChromaticAberration/FilmGrain/LensDistortion/ColorAdjustments/Sharpen/Tonemapping) 및 `PostProcessPanel` 반영됨.
  - **남은 항목**:
    1. **P1-C에서 격상된 S3·S4 결함 수정 및 S1 계측 검증 완료** (최우선 선행).
    2. **S5 환경맵 텍스처 기반 IBL 확장**: 현재의 반구형 색상 근사(`SkyLight`)를 넘어 큐브맵 기반 Diffuse Irradiance / Roughness별 Prefiltered Specular / 2D BRDF LUT 텍스처 파이프라인 연결 및 그림자 경계 안정성 측정.
- 완료 기준: 작은/큰 씬에서 고정 비용과 이득을 S1 계측 수치로 구분하고, 미지원·용량 초과의 정확한 대체 동작과 IBL/그림자 품질을 확인한다.

### P2-C. 강화학습의 첫 환경

- Engine 계획 15절: 수치 관측의 단일 환경, 외부 제어 스텝·reset·seed·관측/행동·종료/중단 계약.
- 필요한 물리·시간 상태만 먼저 격리하고 창·Renderer·오디오 없는 구성을 작은 예제로 검증한다.
- 로컬 Python 요청/응답과 첫 Gymnasium 어댑터를 연결한다. 게임 기능 전체나 고급 렌더링 완료를 기다리지 않는다.
- 완료 기준: 반복 reset과 같은 조건의 스텝을 검사하고 한 환경의 상태·관측 버퍼가 다음 에피소드에 섞이지 않는다.

### P2-D. 아바타·제작 도구의 첫 재생

- 3D는 Engine 계획 9절의 클립 평가·골격·스키닝을 먼저 연결하고, 표정/모프·매핑 등 필요한 16절 기반으로 확장한다.
- 2D는 작은 변형 메시·파라미터 평가·마스크/합성을 독립 경로로 진행한다. 3D 전체 완료를 요구하지 않는다.
- 17절의 제작 작업은 우선 임포트 데이터 검사·작은 수정·Undo/Redo·안전한 저장과 변환 결과 확인부터 시작한다.
- 완료 기준: 같은 에셋의 두 실행 인스턴스와 Editor 미리보기가 상태를 공유하지 않고 저장 결과를 다시 재생한다.

### P2-E. Client 시작·배포 — 현재 보류

- **2026-10-30 현황**:
  - `Tools/Scripts/PackageClient.ps1` 기본 복사 스크립트 초안이 추가되었고 `Client/Source/Main.cpp`에 `LoadGameModule()` 호출이 추가되었다.
  - 그러나 `Client/Source/Main.cpp`는 여전히 `Resources/Scenes/NewScene2.json`을 하드코딩하고 `StartupResourceLoader`를 거치지 않으며, `InitializeSystems()`에서 `GraphSystem`·`PhysicsSystem`·`TerrainSystem` 등록이 누락되어 있다(시스템 등록 누락은 P2-A에서 먼저 해소).
- BuildAndPackagingPlan의 사용자 결정에 따라 StartupScenePath 연결과 배포 구현은 보류 상태를 유지한다.
- 사용자가 해당 작업을 재개하면 실행 경로→시작 리소스/씬→Build/Cook→Stage/Package 순서로 진행한다.
- 관련 기능의 Client 실행이 현재 고정 시작 씬에서 확인된 것과 저장소 밖 배포 검증을 구분한다.
- 완료 기준: Editor·개발 경로·현재 작업 디렉터리에 의존하지 않는 배포 폴더에서 설정한 프로젝트가 실행된다.

보류된 작업을 이 문서 작성이나 우선순위 지정만으로 자동 재개하지 않는다.
그 보류가 독립적인 Engine·Editor·시뮬레이션 작업 전체를 막는 조건도 아니다.

## 6. P3: 콘텐츠 제작과 규모 확장

선택한 기능의 기본 실행·수명·저장·계측이 검증된 후 필요한 항목을 확장한다.

- Engine 4·5·7절: CPU 파티클→다중 이미터·모듈, 스플라인 경로, 독립 트레일. 서로 불필요한 선행 조건으로 묶지 않는다.
- Engine 9·10절: 두 클립 블렌딩·소켓·상태 제어. 포즈 그래프·IK·리타게팅은 해당 평가 기반 이후 추가한다.
- Engine 11절: 실제 요구에 맞는 텍스트 스크립트 언어/VM 선택·엔진 바인딩·배포. 네이티브 모듈과 지원 범위를 구분한다.
- ImGui 10·11·14절: 독립 프리팹 편집, 참조 색인·누락 검사, 임포트 옵션·재임포트, 검색·썸네일·미리보기와 비용 제한.
- Engine 14절: 해당 예제에 필요한 캐릭터 이동·게임 저장·카메라·오디오·UI 위젯·타일맵·비동기 로딩을 작은 흐름으로 연결한다.
- Render M1: Compute 기반과 S3 이후 GPU 가시 명령·간접 그리기→Hi-Z를 CPU 기준 경로와 비교한다.
- Render M2: 바인딩 병목·명령 구성 제약이 측정될 때 Bindless를 추가한다.
- Render M3: 기능 지원·자원 계약·계측을 갖춘 뒤 DXR 그림자→반사를 독립 경로로 진행한다.
- Render M4: IBL 합성 기준과 광선 조회 기반 이후 제한된 공간의 프로브 GI를 검증한다.
- Engine 15절: 프로세스별 환경→영상 센서·평가·랜덤화·필요한 정책 추론. 같은 프로세스의 병렬화는 상태 격리 이후다.
- Engine 16·17절: VRM 지원 항목·외부 추적·표정/립싱크·방송 출력, 2D 파츠/변형과 뼈·웨이트·모프 제작을 단계적으로 연결한다.

기본 기능이 있다는 이유로 성능·품질·파일 호환까지 완료 처리하지 않는다.
기능별 정상 예제와 실패/초기화/대량 입력의 결과를 상세 계획 기준으로 남긴다.

## 7. P4: 요구와 측정 이후 선택할 큰 확장

- Render M5·L1~L5: Meshlet/Mesh Shader, 가상화 지오메트리, VSM, ReSTIR, 기준 패스 트레이싱과 추가 연구 후보.
- GPU 파티클·비동기 Compute·고급 스트리밍은 실제 데이터 규모와 비용이 요구할 때 진행한다.
- 중첩 프리팹·Variant·복잡한 포즈 그래프·상태 이전 핫 리로드는 안정된 식별·평가·복구 계약 이후다.
- 내비게이션·게임 AI·씬 스트리밍·멀티플레이는 대상 게임의 요구와 기존 구현을 다시 확인한 뒤 별도 범위를 정한다.
- 학습의 대규모 병렬화·다중 에이전트·관절/모션 최적화, 아바타의 고급 추적·제작 연산·장시간 출력은 첫 예제 이후다.

P4는 삭제된 목표나 구현 불필요 판정이 아니다. 선행 조건·콘텐츠·측정이 갖춰지면 우선순위를 올릴 수 있다.

## 8. 잘못된 선행 조건을 만들지 않는 기준

- GPU 파티클은 Compute·버퍼·동기화·측정이 필요하지만 CPU 파티클에 GPU 실행 전체를 요구하지 않는다.
- GPU Driven과 DXR은 별도 경로다. Bindless·Mesh Shader를 DXR의 필수 선행 조건으로 두지 않는다.
- 2D 애니메이션 상태 제어는 Sprite 재생기로 시작할 수 있다. 골격 스키닝 전체가 공통 필수 조건은 아니다.
- 학습의 수치 관측은 고급 그래픽·내비게이션·텍스트 VM 전체를 필요로 하지 않는다.
- 2D 아바타와 3D 아바타, 아바타 제작과 강화학습은 공유 기반만 연결하고 모든 단계를 서로 대기시키지 않는다.
- 런타임 기능보다 전용 편집 창 전체를 먼저 만들지 않는다. 기존 Inspector·뷰포트·명령 이력을 재사용한다.
- 전체 폴더 정리·공통 모듈의 다중 DLL화·자동 Render Graph를 모든 신규 기능의 필수 조건으로 만들지 않는다.

## 9. 원래 계획과의 대응

- Engine 3·13절은 전 단계의 시간·수명·저장·검증 기준이다. 12절은 게임 기능 내부 순서, 18절은 결과물별 흐름이다.
- Engine 4~11절은 P2/P3에서 필요한 실행 기능을 선택한다. 14절은 대상 게임의 요구에 따라 P2~P4로 나눈다.
- Engine 15~17절은 P2의 첫 환경/재생/제작에서 P3/P4로 확장한다. 19절은 P1의 공통 설정 경로다.
- Render S1/S2는 P1, S3/S4/S5는 P2, M1~M4는 조건부 P3, M5·L 계열은 P4다.
- ImGui 3~8절·11절 A·14절 기본 판정/예산은 P1, 10절 초기 배치와 검색/선택은 P2, 고급 제작은 P3/P4다.
- BuildAndPackagingPlan은 P2-E의 보류·재개 조건을 우선하고, 재개 후 그 문서의 1~4단계를 따른다.

각 계획의 상세 기능 목록과 완료 기준은 원문에서 유지한다. 우선순위 분류만으로 기존 검증 상태를 바꾸지 않는다.

## 10. 조사 시점의 문서·코드 차이와 재조정된 단기 실행 순서 (2026-10-30)

2026-10-30 작업 트리 정적 점검을 통해 다음 사실과 괴리를 확인했다.

- **코드에 반영되었으나 결함·미완성 지점이 확인된 영역**:
  - 렌더링 S3(`InstanceData`, `LODGroupComponent`, `MeshRenderSystem`, `Renderer::RenderMeshes`)와 S4(`tiledLightCullingCS.hlsl`, `LightManager::DispatchLightCulling`)가 S1(계측)·S2(바인딩 문서 동기화)보다 먼저 작성되었으나, 타일 XY 절두체 컬링 누락·0-광원 폴백 버그·비인덱스 메시 인스턴싱 누락·단일 카메라 LOD 변이가 존재한다.
  - 비주얼 스크립팅(`GraphSystem`, `GraphPanel`의 `imgui-node-editor` 전환, 코멘트 박스 `C`, `ModuleId` 기반 핫리로드 및 `Graph::Reconcile`)이 구현되었으나, PIE 실행 중 `GraphSystem`의 활성 실행기 핫스왑(Phase 4.3)과 `Client`의 시스템 등록(`GraphSystem`, `PhysicsSystem`, `TerrainSystem`)이 빠져 있다.
  - 문서 보호(`EditorDocumentController`, `SceneEditHistory`)와 그래픽 설정(`EditorSettingsService`, `GraphicSettingPanel`)이 연결되었으나, `HistoryPanel` UI, 뷰포트 내부 해상도 설정 연동, `ToolBar`의 `UI_SETTINGS` 메뉴 연결, `ResourcePicker`의 미로드 에셋·검색·썸네일 지원이 남아 있다.

### 재조정된 단기 실행 우선순위 (Next Execution Order)

새로운 P2/P3 기능을 추가로 벌리기 전에 아래 1~5순위 순서로 현재 작업 트리의 변경을 안정화하고 마감한다.

1. **[1순위 / P0-A] 작업 트리 불필요 부산물 정리 및 변경 범위 분리**
   - 0바이트 빈 파일(`Editor/Source/Application/Private/ApplicationSettings.cpp`), 레거시 POC 생성물(`Tools/GraphCompiler/test_graph.gen.cpp`), 루트 셰이더 컴파일 부산물(`Resources/Shader/default_*.cso`) 등 정리.
2. **[2순위 / P1-C & P2-B 안정화] 기구현 렌더링 S3·S4 결함 수정 및 S2 백엔드 계약 방어**
   - `tiledLightCullingCS.hlsl` 타일 상·하·좌·우 절두체 평면 컬링 추가 및 View-space Z 부호 검증.
   - `lightingPS.hlsl`의 `tileLightCount == 0` 전체 광원 루프 폴백 버그 수정 및 `ViewportDeferredPipeline` 바인딩 일관성 확보, `SceneViewportPanel` 중복 CBV 바인딩 제거.
   - `Renderer::RenderMeshes` 비인덱스 인스턴싱 처리 및 `MeshRenderSystem` 다중 뷰포트/패스 LOD 제출 기준 보완.
3. **[3순위 / P1-C] 렌더링 S1 CPU/GPU 계측(`RHIFrameMetrics` 확장) 구현 및 S3·S4 효과 검증**
   - 패스별 CPU 제출 시간, D3D12 Timestamp Query 기반 GPU 시간, 디스크립터·메모리 통계 및 기능 지원 질의를 `RHIFrameMetrics`와 `MetricsPanel`에 연결하여 S3(인스턴싱/LOD)·S4(타일 라이트 컬링)의 전후 성능을 수치로 검증.
4. **[4순위 / P2-A & P0-B] 런타임 스크립팅 핫스왑(Phase 4.3), `Client` 시스템 등록, 전처리기 정리**
   - `GraphSystem`에 모듈 핫리로드 시 활성 엔티티 `GraphRuntimeState` 재바인딩 연결.
   - `Client/Source/Main.cpp`의 `InitializeSystems()`에 `GraphSystem`, `PhysicsSystem`, `TerrainSystem` 등록.
   - [BuildConfigurationAndPreprocessing.md](BuildConfigurationAndPreprocessing.md) 4절의 `#ifdef _DEBUG` 제거(`SceneViewportPanel`, `Vertex.h`), `BAM_EDITOR` 도입, `std::terminate`/`std::cout` 정리.
5. **[5순위 / P1-A·P1-B·P1-D] 에디터 제작 기반 잔여 UI 마감**
   - `GraphicSettingPanel` 뷰포트 해상도 제어 및 `ToolBar` 설정 메뉴 연결(P1-B, 완료).
   - `ResourcePicker` 검색·핸들 타입 필터링 및 팝업 캐싱(프레임 드랍 방지)·미로드 에셋 선택 및 `ContentBrowserPanel` 검색 범위 전환(P1-D).
   - Undo/Redo `HistoryPanel` 시각화 창 추가(P1-A, 완료) 및 블루프린트 Reroute/정렬 단축키(P2-A Phase 4.2 잔여, 완료).

코드 직접 수정·빌드·테스트 권한은 사용자 요청과 AGENTS.md를 따르며 이 문서가 실행 허가를 대신하지 않는다.
