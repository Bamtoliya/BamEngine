# 통합 개발 우선순위와 완료 기준

- 작성일: 2026-10-24 (최종 갱신: 2026-11-06)
- 상태: 2026-11-05 단계 1(F02), 단계 2(F05, F06, F07, F08), 단계 3(P1-E/3-A, 3-B), 단계 4(F12), 빌드/배포(F13, F04) 및 강화학습(F09) 구현 반영, Standalone Client 독립 실행 및 Unity 빌드 정합성 검증 완료, 단일 CPU 파티클 시스템 1단계 코어(Engine 4절) 구축, AnimationTimelineEditorPlan(에디터 저작 도구 및 런타임 액션 타임라인 재생 시스템 Phase 1~5) 구축 완료, 네트워크 기초 파이프라인 및 키네마틱 캐릭터 컨트롤러 구축 완료, 3인칭 캐릭터 Locomotion 1D 블렌드 트리 위상 동기화(Phase Matching) 및 루프 크로스페이드·속도 연동 재생 배속 완성, 모델 재임포트 인플레이스 핫 리로드 및 대화형 3D 임포트 모달(ModelImportModal) 구축 완료.
- 최신 근거: [2026-11-06 코드·원칙·계획 점검](CodeDocumentationAudit-2026-11-06.md). **현재 작업 선택은 11절을 우선한다.** 위 상태와 10절의 완료 표시는 당시 기록이며 A01~A10의 현행 잔여 사항을 해소한 뜻이 아니다.
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
- [BuildAndPackagingPlan.md](BuildAndPackagingPlan.md): Client 경로·설정 기반 시작 씬은 일부 반영. 시작 매니페스트·실패 정책·Build/Cook/Stage/Package 및 배포 검증은 미완료.
- [ReflectionArchiveGuidelines.md](ReflectionArchiveGuidelines.md): 독립 모듈과 엔진 연결·저장/복원 정확성.
- [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md): 공유 바인딩 규약. 실제 셰이더·백엔드와 대조한다.
- [ImplementationLog.md](ImplementationLog.md): 작업별 구현·검증 근거. 계획의 체크 표시로 대체하지 않는다.
- [VisualScriptingBlueprintPlan.md](VisualScriptingBlueprintPlan.md): 비주얼 스크립팅과 블루프린트 개발 계획 및 런타임 게임플레이 연동.
- [ModuleHotReloadOptimization.md](ModuleHotReloadOptimization.md): C++ 모듈 핫리로드 및 빌드 시간 단축 계획.
- [AnimatorGraphPlan.md](AnimatorGraphPlan.md): 애니메이터 그래프, 애니메이션 상태 머신(ASM) 및 블렌드 트리 개발 계획.
- [NetworkFoundationPlan.md](NetworkFoundationPlan.md): 소켓 전송 계층, 패킷 직렬화, 세션 및 ECS 리플리케이션·RPC 개발 계획.
- [AnimationTimelineEditorPlan.md](AnimationTimelineEditorPlan.md): 애니메이션·이펙트·노티파이 통합 타임라인 저작 도구 및 비-PIE 실시간 프리뷰 계획.
- [KinematicCharacterControllerPlan.md](KinematicCharacterControllerPlan.md): 키네마틱 캐릭터 컨트롤러(KCC), 지면 감지, 경사로·계단 오르기 및 슬라이딩 물리 계획.
- [ThirdPersonCharacterSystemPlan.md](ThirdPersonCharacterSystemPlan.md): 3인칭 캐릭터(KCC) 이동, 스프링암 카메라 제어, 3D 지면 충돌 안정화 및 애니메이션 연동 계획.
- [InteractiveModelImportPreviewPlan.md](InteractiveModelImportPreviewPlan.md): 대화형 3D 모델 임포트 프리뷰 시스템, 실시간 축/스케일 변환 및 뷰포트 일원화 계획.
- [Utf8PathHandlingPlan.md](Utf8PathHandlingPlan.md): UTF-8 파일 경로, 다국어('새 폴더' 등) 입출력 및 비동기 파일 감시자(efsw) 크래시 방어 계획.
- [EditorWorkflowAndInteractionPlan.md](EditorWorkflowAndInteractionPlan.md): PIE 에디터 시점 분리, 계층/인스펙터 전면화 연계, F/F2 상호작용 및 단축키(ShortcutManager) 일원화 계획.

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

### P0-A. 현재 변경과 재현 기준 확인 (정리 및 단위별 커밋 완료)

- **2026-11-01 현황 (완료)**:
  - 기존 작업 트리에 섞여 있던 약 240여 개 미커밋 변경과 임시 부산물(`ApplicationSettings.cpp` 0바이트 파일, `test_graph.gen.cpp`, `default_*.cso`, `archive-example/`, 대용량 LLVM 의존성 등)을 정리하고 `.gitignore` 제외 및 기능 단위별 커밋·푸시를 완료했다.
  - 위 기록은 당시의 정리 결과다. 2026-11-02 점검 후에는 문서 변경이 작업 트리에 있으며, 과거의 깨끗한 상태를 현재 상태로 확대하지 않는다.
- **조치 기준**:
  - 신규 작업 시에도 임시 부산물 유입을 방지하고, 마지막 확인한 빌드·실행 상태와 새 변경을 구분한다.
- 완료 기준: 임시 부산물이 제거되고, 새 수정으로 발생한 문제와 기존 문제를 구분할 기준이 있다. (달성)

### P0-B. 표준 헤더의 PCH 중앙화 및 전처리기 구성 정리

- **2026-11-01 현황**:
  - `Projects/GameModule/CMakeLists.txt`에 PCH(`target_precompile_headers`)가 적용되어 모듈 핫리로드 빌드 시간이 단축되었다.
  - `Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp`의 `m_DebugRenderer` 및 `Engine/Source/Core/Public/Structs/Vertex.h`의 `DebugVertex`, `Engine/Source/System/Graph/Public/GraphSystem.h`의 디버그 실행기 조회에서 `#ifdef _DEBUG` 분기가 제거되어 Release 에디터에서도 디버그 드로우와 그래프 디버깅이 상시 정상 제공된다 (반영 완료).
  - `Engine/Source/Graph/Public/GraphGameplayLibrary.h`의 `std::cout` 사용이 제거되고 `BAM_LOG` 로깅 시스템으로 전환 완료되었다.
  - `Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp` 내 3곳의 `OutputDebugStringA` + `std::terminate()` 호출이 안전한 에러 로깅(`BAM_LOG_ERROR`) 및 `EResult::Fail` 반환으로 교체 완료되었다 (2026-11-01 반영 완료).
  - **2026-11-02 추가 정적 확인**:
    - `BAMENGINE_WITH_EDITOR` 옵션과 Engine의 `PUBLIC BAM_EDITOR=1` 정의, GraphSystem 디버그 훅 및 ResourceManager 스냅샷 가드가 있다.
    - `LocalizationManager.cpp` 및 `ReflectionRegistry.cpp`의 과거 표준 스트림 출력 위치는 `BAM_LOG`로 전환되어 있다.
  - **남은 항목**:
    - Editor 포함 구성의 Client도 매크로를 전달받는 점을 고려한 별도 Editor OFF 빌드·ABI 검증.
    - `BAM_SHIPPING` 정책·정의와 최종 제거 범위, 기존 단언/디버그 코드의 후속 정리.
- **조치 기준**:
  - [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md) 8절 및 [BuildConfigurationAndPreprocessing.md](BuildConfigurationAndPreprocessing.md) 4절에 따라 타깃 매크로 정의와 표준 스트림 정리를 마무리한다.
- 완료 기준: 영향 타깃에서 필요한 정의가 제공되고 `std::terminate` 및 표준 스트림 직접 호출이 제거된다.

### P0-C. 소유 위치가 명확한 파일부터 정리

- **2026-10-30 현황 (코드 반영됨)**:
  - 컴포넌트를 `Engine/Source/Core/Public/Components/<Domain>/`의 개별 헤더로 분리했다. 도메인별 묶음 헤더도 해당 하위 경로에 남아 있다. 묶음 헤더 전체 제거로 해석하지 않는다(2026-11-02 대조).
  - `Entity.h/.cpp`를 `Engine/Source/World/Entity/Public|Private/`로 이동하고, 모델 인스턴스화 로직을 `Engine/Source/World/Scene/Private/SceneModel.cpp`로 분리 완료.
- 완료 기준: 참조·생성 입력·관련 검사와 문서 경로를 추적할 수 있고 기존 동작을 보존한다.

### P0-F. 다국어(UTF-8) 파일 경로 및 외부 비동기 파일 감시자 안정화 (Utf8PathHandlingPlan)

- **2026-11-04 현황 (구현 및 안정화 완료)**:
  - 열거한 파일 감시자와 UI 경로에 UTF-8 변환을 반영했다. 2026-11-06 점검에서는 AssetHotReloadManager의 narrow 재변환·예외 경계가 남아 있어 모든 감시자 차단 완료로 보지 않는다(A03).
  - Engine::Utf8ToPath(Functions.h) 단일 진실의 원천 구축.
  - ContentBrowserFileListener, ModuleSourceFileListener, ModuleHotReloadFileListener, ShaderFileListener의 handleFileAction 경로 생성 전수 교정 및 try-catch 예외 방어벽 구축.
  - Content Browser 인라인 리네임(DrawRename)의 renameBuffer, 외부 드롭(OnExteranalDropped), AssetCache 3D 썸네일 경로 변환의 UTF-8 안전화 및 Wide String 결합(wstring) 적용 완료.
- 완료 기준: 한글/다국어 파일명 및 경로 생성, 수정, 리네임, 드래그앤드롭과 에셋 감시 콜백이 정상 동작한다. 기존 네 리스너의 기록과 A03 잔여 검증을 구분한다.

전체 폴더 재배치·싱글턴 전면 교체·엔진 다중 DLL 분리는 P0의 완료 조건이 아니다.

## 4. P1: 안전한 제작과 측정 가능한 공통 기반

### P1-C. CPU/GPU 계측, 바인딩 계약 동기화 및 렌더링 S3·S4 안정화

- **2026-10-31 구현 현황**:
  - **S1 (계측 및 기능 지원 조회 — 구현 완료)**:
    - `Engine/Source/Render/RHI/Public/RHIFrameMetrics.h`에 패스별 CPU/GPU 실행 시간(`RHIPassMetric`), 하드웨어 지원(`RHICapabilities`), VRAM/디스크립터 통계(`RHIMemoryStats`) 구조 확장을 완료했다.
    - `DirectX12RHI.cpp`에서 D3D12 Timestamp QueryHeap 및 ReadbackBuffer를 통한 패스별 GPU 시간 지연 회수(`m_LastGPUTimestampResults`)와 `QueryMemoryStats()` 구현을 완료했다.
    - `Editor/Source/ImGui/MetricsPanel/Private/MetricsPanel.cpp`에 패스별 CPU/GPU 시간 테이블, VRAM/디스크립터 사용량, 하드웨어 기능 지원 여부 UI 렌더링을 연결했다.
  - **S2 (바인딩 계약)**:
    - [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)를 실제 `DirectX12RHI` 루트 시그니처(`space0`~`space3`, Root Parameter `[0]`~`[10]`) 기준으로 갱신 완료했다.
    - `SDLGPURHI`의 `DrawIndexedInstanced`(`NotImplemented`), `BindInstanceBuffer`, Compute 인터페이스는 미지원 상태로 안전 폴백 정책을 유지한다.
  - **기구현 S3·S4의 정적 결함 해결 및 보존 (반영 완료)**:
    1. `Resources/Shader/hlsl/tiledLightCullingCS.hlsl`: 좌·우·상·하 4개 타일 절두체 평면(`s_FrustumPlanes[4]`) 역투영 및 구면 교차 검증을 추가하여 화면 전체 폭 광원 과할당 문제를 해결했다.
    2. `Resources/Shader/hlsl/lightingPS.hlsl`: `for (uint i = 0u; i < tileLightCount; ++i)` 루프로 타일 내 선별 광원만 순회하여 `tileLightCount == 0`일 때 전체 광원을 순회하던 버그를 해결했다.
    3. `Editor/Source/ImGui/ViewportPanel/Private/ViewportDeferredPipeline.cpp`: `lightManager.DispatchLightCulling` 호출을 연결하여 타일 라이트 컬링이 정상 실행되도록 일관성을 확보했다.
    4. `Engine/Source/Render/Private/Renderer.cpp`: `batchMesh->GetIndexBuffer()` 유무에 따라 비인덱스 메시(`indexCount == 0`)도 `m_RHI->DrawInstanced(vertexCount, instanceCount)`로 인스턴싱 드로우하도록 처리 완료했다.
    5. `Engine/Source/System/Render/Private/MeshRenderSystem.cpp`: `PassTarget` 구조체를 통해 다중 뷰포트/패스별 고유 카메라 거리 및 투영 행렬 기준으로 독립적인 LOD 메시를 평가·제출하며, 주 뷰포트(`isPrimaryGeometry`) 렌더링 시에만 `LODGroupComponent::currentLODIndex` 영속 상태를 갱신하여 보조 패스로 인한 LOD 상태 오염을 원천 차단했다.
  - **기존 성능 관찰 기록 (2026-11-01 MetricsPanel 확인)**:
    - 타일 라이트 컬링: 라이팅 비집중 영역 응시 시 GPU 시간 1.10 ms -> 0.25 ms (77% 단축).
    - 정적 메시 인스턴싱: 인덱스 31만 개 추가에도 드로우콜 56 -> 57회로 유지 (배칭 검증 완료).
- 관찰의 한계: 위 수치는 기존 기록이며 GPU·해상도·동일 카메라·기준 경로·반복 통계가 첨부된 최신 통제 비교로 간주하지 않는다. 이번에 재측정하지 않았다.
- 완료 기준: S3·S4의 정확성과 미지원·용량 초과 동작을 검사하고, 같은 장면·장치·설정의 기준 경로와 비용/화질을 비교한다. 코드 반영·기존 관찰·추가 검증을 구분한다.

### P1-A. 편집 데이터와 저장 실패 보호

- **2026-10-31 현황**:
  - `Editor/Source/Document/`(`IEditorDocument`, `SceneDocument`, `EditorDocumentManager`, `EditorDocumentController`)와 `SceneEditHistory`(`HasUnsavedChanges`, `MarkSaved`, `CanFinalizeEditsFor`)가 구현되어 `ToolBar`의 New/Load/Save/Close 및 앱 종료 흐름에 연결되었다.
  - Undo/Redo 명령 스택 및 저장 기준점 시각화 창(`HistoryPanel`)이 구현되어 이력 표시와 Undo/Redo가 연결되었다.
- **2026-11-02 최우선 잔여**: 씬에 있는 보호를 AnimatorGraphPanel의 저장/로드·New/Open/종료에도 연결한다(F01). 같은 패널을 수정할 때 로캘·InputManager 기준(F11)을 함께 적용한다.
- 완료 기준: 취소가 씬 교체를 막고, 저장 실패 후 초안·기존 파일·변경 표시가 보존되며, `HistoryPanel`에서 현재 Undo/Redo 위치와 저장 기준점을 확인할 수 있다.

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
  - **통합 설정 패널(`UnifiedSettingsPanel`) 전면 구축 완료 (2026-11-03)**: 개별 패널(`GraphicSettingPanel`, `ShortcutSettingsPanel`)을 통합하여 좌측 3대 그룹 트리 내비게이션(`General`, `Theme & Style`, `Graphic`, `Editor`, `Workspace`, `Shortcuts`, `Project Settings`)과 우측 상세 뷰로 관리하는 통합 설정 패널 일원화 완료. ImGui 커스텀 스타일 영속화(`EditorStyle.json`) 및 Unscaled SSOT 아키텍처, 버튼 동적 너비 자동 산출, 다이얼로그 폴더 브라우저 연동 완료 ([ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md) 3절).
  - EngineDevelopmentPlan.md 19절의 공통 런타임 설정 API.
  - 동적 생성 뷰포트의 재생성.
  - Client의 경로·설정 기반 시작 씬은 일부 반영되었다. 시작 매니페스트·실패 정책·배포는 BuildAndPackagingPlan.md의 잔여 범위다.
- 완료 기준: 코드 구현·자동 검증·실제 UI 검증을 구분하고 실행 검증 결과를 기록한다.

### P1-D. 기본 에셋 판정·열기와 작업량 제한

- **2026-11-01 현황**:
  - `ContentBrowserPanel` 그리드 행 단위 클리핑(`ImGuiListClipper`)과 `ResourceReference` 도입 완료.
  - `Editor/Source/ImGui/Public/ResourcePicker.h` 최적화 완료:
    - `IsResourceExtensionCompatible<T>`를 통해 템플릿 인자 `T`(`Texture`, `Material`, `Model`, `AudioClip`, `Shader` 등)에 매핑된 호환 확장자 파일만 선별 표시.
    - `if (isAppearing)` 플래그를 통해 팝업 진입 시 1회만 디스크를 스캔하고 `s_CachedDiskFiles`에 캐싱하여 매 프레임 재귀 I/O 순회 병목을 원천 차단.
  - `ContentBrowserGridItem` 및 `ContentBrowserPanel` 편의 기능 완성 (2026-11-01 반영 완료):
    - `OpenAssetOrFolder` 공통 함수를 통해 더블클릭과 컨텍스트 메뉴의 `열기` 동작 통합 (내부 에디터 미지원 파일은 OS 기본 프로그램 폴백).
    - 컨텍스트 메뉴 상단 `열기`, `기본 프로그램으로 열기`, `탐색기에서 표시(Show in Explorer)` 유니코드 안전화(`ShellExecuteW`).
    - 검색창 검색 범위 토글(`현재 폴더` vs `전체 리소스 폴더`), 검색 결과 0개 시 명확한 안내 및 `검색 지우기` 버튼 연동.
  - **남은 항목**:
    1. 에셋 썸네일 축소 생성 및 캐시 예산 제한(14-D).
    2. 피커의 CWD 의존 스캔과 Shader 복합 확장자 누락, 모델 임포트의 머티리얼 저장 실패/재임포트 처리([점검 F02·F06·F12](CodeDocumentationAudit-2026-11-02.md)).
- 완료 기준: JSON/바이너리의 같은 콘텐츠 타입을 구분하고, 리소스 피커가 프레임 드랍 없이 해당 핸들 타입(`T`)과 호환되는 에셋만 디스크에서 즉각 선별·표시하며, 미로드 에셋 선택·검색 범위 전환·손상/미지원 처리가 동작한다.

### P1-E. 머티리얼 플래그 기반 텍스처 샘플링 분기 및 그래프 입력 유연화

- **2026-11-01 현황 및 배경**:
  - **즉시 조치 반영 완료**: 머티리얼 바인딩 시 미지정 슬롯의 텍스처 상태 누수(이전 드로우콜 텍스처 잔류로 인한 PBR/Emission 오염 및 화면 폭발/순백색 자체 발광 버그)를 막기 위해, `MaterialInterface::Bind()` 및 `MaterialInstance::Bind()`에서 슬롯 0~3 점유 여부를 추적하고 미지정 슬롯을 안전한 기본 더미 텍스처(`white1x1`, `blue1x1`, `black1x1`)로 리셋하도록 조치 완료했다. 또한 머티리얼 인스턴스의 역직렬화 직후 슬롯 캐시(`m_TextureSlotToIndex`)가 비어 있는 경우 자동 재구축하도록 방어했다.
  - **후속 아키텍처 목표**: 1x1 더미 텍스처 바인딩 및 무조건 샘플링 방식에서 탈피하여 플래그 기반 분기와 머티리얼 그래프 기반의 유연한 입력 구조를 확보한다.
- **주요 계획**:
  1. **텍스처 유효성 비트 플래그(`MaterialFeatureFlags`) 도입**:
     - 머티리얼별 텍스처 바인딩 여부를 비트 플래그로 G-Buffer 셰이더(`gbufferPS.hlsl`)에 공급.
     - 슬롯이 비어있으면 샘플링을 바이패스하고 안전한 수학적 기본값(Emission: 0.0, Normal: 정점 법선, PBR: 상수 거칠기/금속도/차폐 1.0)으로 즉시 분기.
     - 슬롯을 더미 텍스처로 채우지 않고 순수하게 비우는(Unbound/Null SRV) 방식을 취하더라도 상태 오염 없는 안전한 렌더링 보장 및 텍스처 페치 대역폭 절감.
  2. **머티리얼 그래프 기반 상수/벡터/컬러 분리 입력 연동**:
     - 텍스처 에셋을 반드시 샘플링하는 구조 대신, 머티리얼 그래프에서 특정 변수나 단일 Color(RGBA), 스칼라 파라미터(Float), 분리된 채널(Split RGBA / Swizzle)을 G-Buffer의 각 속성에 직접 공급할 수 있는 평가 경로 구축.
     - Albedo에 단색(Color Uniform) 직결, Roughness/Metallic에 스칼라 슬라이더 직결, Emission에 HDR Color 직결 등 텍스처 없는 절차적/상수 기반 머티리얼 작성을 완전하게 지원.
- 완료 기준: 텍스처가 없는 머티리얼에서 이전 드로우콜의 텍스처 오염이나 하얗게 새는 현상이 발생하지 않고, 머티리얼 그래프에서 연결된 상수 컬러 및 채널 분리 데이터가 G-Buffer와 최종 라이팅에 정확히 반영된다.

### P1-F. 대화형 3D 모델 임포트 프리뷰 시스템 및 에디터 뷰포트 일원화

- **2026-11-04 현황 및 구현 반영**:
  - 상세 계획: [InteractiveModelImportPreviewPlan.md](InteractiveModelImportPreviewPlan.md) 및 [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md) 16절(3D 뷰포트 일원화), 17절(가변 스플리터).
  - **Phase 1 (뷰포트 및 변환 인프라 구축 — 완료)**:
    - `EditorCameraController`: `Alt+LMB`(피벗 중심 Orbit), `Alt+MMB`/`MMB`(Pan), `Alt+RMB`/휠(Zoom) 구현 및 뷰포트 호버 시에만 휠 스크롤 소비(`allowScroll`)로 모달/UI 스크롤 간섭 방지.
    - `PreviewViewportWidget`: 포스트프로세스 체인(`ExecuteChain`) 이후 디스플레이 RT에 바닥 그리드(`m_Grid.SubmitGrid`) 렌더링 순서 교정, 정지 시 본 행렬 `Identity` 초기화로 원본 T-포즈 보장, `InvisibleButton`과 오버레이 툴바 Z-order 분리로 툴바 버튼 클릭 복구.
    - `ModelEditor`: 프로퍼티 창에 `PreviewViewportWidget` 및 수평 가변 스플리터(`Splitter`) 장착 완료.
    - `ModelConverter` & `ModelImporter`: `UniformScale`, `RotationOffset` 파라미터 확장 및 Assimp 루트 노드 변환 연동. `Resources/Data/Localization/Editor.csv`에 `ModelImport.*` 로캘 키 추가.
    - `ContentBrowserPanel`: 외부 드래그 앤 드롭 시 모델 확장자(`.glb`, `.gltf`, `.fbx`, `.obj`) 감지 모달 트리거 및 기본 팝업(`DrawModelImportModal`) 연동.
  - **후속 진행 (Phase 2~4)**:
    - 모달 내 `PreviewViewportWidget` 임베딩 및 임시 프리뷰 씬 엔티티 트랜스폼 실시간 동기화 (슬라이더 조작 시 무지연 60 FPS 회전/스케일 피드백 제공).
    - 가변 스플리터 적용 및 외부 에셋 썸네일 베이킹/프리셋 시스템 확장.
- 완료 기준: 모달 오픈 시 3D 모델이 좌측 뷰포트에 즉각 표시되고, 우측 슬라이더와 원클릭 버튼("Pitch 180" 등) 조작 시 모델의 방향/스케일이 실시간 반영되며, 최종 임포트 결과물이 모달 세팅과 100% 일치한다.

### P1-G. 에디터 워크플로·상호작용 개선 및 단축키 일원화 (EditorWorkflowAndInteractionPlan)

- **2026-11-05 추가 정리 (코드·자동 검증 완료, 실제 UI 회귀 확인 대기)**:
  - 로캘 조회·InputManager·등록 액션·동적 단축키 힌트를 보완하고, 그래프 라이브러리 기본 단축키 우회와 비활성 로컬 명령의 Global 실행을 차단했다.
  - Editor Debug 빌드·엄격 감사·관련 CTest 4개를 통과했다. 전체 CTest 및 실제 UI 검증 완료를 뜻하지 않는다.
  - 재바인딩·언어 전환·분리 창/DPI·캔버스 밖 우클릭의 실제 조작 확인을 남긴다.
    범위와 근거는 [ImplementationLog.md](ImplementationLog.md)의 2026-11-05 로캘·입력·단축키 기록과
    [EditorWorkflowAndInteractionPlan.md](EditorWorkflowAndInteractionPlan.md) 5절을 따른다.
- **2026-11-04 현황 (구현 및 검증 완료)**:
  - **PIE 에디터 시점 분리**: PIE 실행 중에도 `Scene View`는 에디터 카메라 조작(Orbit, Pan, Fly) 및 기즈모 디버깅을 상시 유지하고, 게임 실행 화면은 `Game View`를 자동 오픈/포커스하여 분리 표시. PIE 종료 시 `Scene View`로 포커스 자동 복원.
  - **계층 패널 ↔ 인스펙터 연계**: 계층 패널에서 엔티티 클릭 시 숨겨진 인스펙터 탭을 즉시 최상위로 전면화(`Focus()`)하며, 기존 엔티티별 스크롤 유지 맵(`m_EntityScrollMap`) 보존.
  - **계층 패널 빠른 조작 및 컨텍스트 메뉴 확장**: `Focus Selected`(F, 씬 뷰포트 카메라 이동), `Rename`(F2, 인라인 텍스트 입력), `Create Child Empty`, `Duplicate`(Ctrl+D), `Move to Root` 메뉴 확장.
  - **단축키 중앙화 원칙 준수**: 패널 내 파편화된 수동 `KEY_DOWN` 처리(F, F2, Delete 등)를 제거하고 `ShortcutManager`(`EditorActionRegistry`)의 정식 액션(`Viewport.Focus`, `Hierarchy.Rename` 등) 및 문맥(`EShortcutContext`)으로 일원화. UI 메뉴 단축키 힌트 또한 `GetShortcutHint`로 동적 조회 일치.
- 완료 기준: PIE 중 씬 뷰 에디터 카메라가 유지되고, 계층 패널 클릭 시 인스펙터 탭이 전면 노출되며, F/F2 단축키가 포커스 상태와 무관하게 일관성 있게 작동함. (달성)

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
  - **Phase 4.3 런타임 핫스왑 및 Client 시스템 등록 완성 (2026-10-31 반영 완료)**:
    - `GraphSystem::ReloadReflectedNodes`: 모듈 핫리로드 시 활성 씬 엔티티들의 기존 변수(`oldVariables`)를 보존하고 새 리플렉션 심볼 및 그래프 템플릿으로 `GraphRuntimeState`를 핫스왑.
    - `Application.cpp`: `HotReloadManager::RegisterPostReloadCallback`에서 `GraphSystem->ReloadReflectedNodes(activeScenes)`를 자동 호출하도록 연결하여 PIE 실행 중 0-크래시 핫스왑 보장.
    - `Client/Source/Main.cpp`: `InitializeSystems()`에 `GraphSystem`, `PhysicsSystem`, `TerrainSystem` 등록 완료.
  - **엔진·프리팹 기본 연결 및 파이프라인 (2026-10-31 구현 완료)**:
    - **프리팹 1단계 (생성·배치)**: 계층 패널 `Save As Prefab...`, 콘텐츠 브라우저 큐브 식별, 드래그 앤 드롭 인스턴스화 및 Undo/Redo.
    - **프리팹 2단계 (격리 편집 모드)**: `PrefabModeManager` 뷰포트 격리 편집 모드(메인 씬 보존, 메인 뷰포트·기즈모 100% 활용, 상단 복귀/저장 배너).
    - **프리팹 3단계 (원본 연결 및 오버라이드 관리)**: 단일 원천 루트 전용 `PrefabInstanceComponent`, 계층 패널 청록색 강조 및 컨텍스트 메뉴(Open/Select/Unpack), 인스펙터 상단 프리팹 헤더 바 및 Overrides 팝업(Apply All, Revert All, Unpack).
    - **엔티티 다중 복사·붙여넣기 (17절 완료)**: `EntityClipboard` 메모리 직렬화, 조상 필터링, 원래 계층/형제 순서 보존, 원자적 단일 Undo(`History.Command.PasteEntities`), 단축키(`Ctrl+C`/`Ctrl+V`/`Ctrl+Shift+V`) 및 다중 삭제(`DeleteEntities`, `Delete` 키).
    - **그래프 패널 상단 도구 모음 메뉴바 리팩터링 (2026-11-01 구현 완료)**:
      - 기존 2단 버튼 나열 방식을 제거하고 `ImGuiWindowFlags_MenuBar` 기반 상단 메뉴바(`File`, `Edit`, `View`, `Graph Mode`, `Debug/Build`)와 1줄 퀵 액세스 툴바로 재설계하여 캔버스 작업 공간 대폭 확보.
    - **씬 레벨 독립 Collider Raycast API (2026-11-01 구현 완료)**:
      - `PhysicsSystem::Raycast`/`RaycastAll`에 `RaycastQuery`(거리, 레이어마스크, 트리거 포함 여부, 무시 엔티티) 필터링, 활성 플래그 검사, 거리 컷오프 기반 조기 탈출(Early-out) 최적화 및 하위 호환 오버로드 완비.
    - **독립 SpriteAnimation 에셋 및 전용 에디터 구축 (2026-11-01 구현 완료)**:
      - `SpriteAnimation` 리소스 에셋 클래스 및 JSON 직렬화/역직렬화 구축.
      - `SpriteAnimationComponent` 및 `SpriteAnimationSystem` 에셋 우선/로컬 폴백 파이프라인 연동.
      - `SpriteAnimationEditor` 전용 저작 패널(프리뷰 캔버스, 줌/팬, 피벗 크로스헤어, 툴바, 프레임 관리, 하단 필름스트립 타임라인, `ResourceHandle<SpriteAnimation>` 안전 참조) 및 콘텐츠 브라우저 더블클릭 연동 완성.
    - **비주얼 스크립팅 `String` 핀 타입 완전 지원 (2026-11-01 구현 완료)**:
      - `EGraphPinType::String` 추가, `GraphValue` `std::string` 지원, 리플렉션 함수 `std::string` 인자·반환값 자동 매핑, `Core.String` 리터럴 및 `Core.Reroute.String` 노드, `GraphValueArchiveAdapter` 직렬화, `GraphPanel` `ImGui::InputText` 편집 및 마젠타 색상 핀 연동 완료 (`SetAnimatorFloat` 등 문자열 파라미터 노드 완벽 지원).
- 완료 기준: PIE 중 모듈 핫리로드와 그래프 실행이 안전하게 유지되고, 입력으로 작은 씬을 조작하며 화면 표시 여부와 무관한 충돌 질의·두 인스턴스의 독립 상태를 확인한다.

### P2-B. 기본 렌더 확장 (S3·S4·S5 검증 및 완성)

- **2026-11-01 현황**:
  - **S3**: `InstanceData`, `LODGroupComponent`, `MeshRenderSystem` 다중 뷰포트/패스별 카메라 기준 화면 덮개 비율 + 히스테리시스 독립 LOD 선택(및 주 뷰포트 전용 상태 보존), `Renderer::RenderMeshes` 인스턴스 배칭(`DrawIndexedInstanced` 및 비인덱스 `DrawInstanced` 지원) 반영 및 배칭 드로우콜 유지(56 -> 57회) 실측 검증 완료.
  - **S4**: `tiledLightCullingCS.hlsl` 4방향 절두체 평면 컬링, `DirectX12Pipeline` Compute PSO, `DirectX12RHI::Dispatch`/`ResourceBarrierUAV`, `LightManager::DispatchLightCulling`, `lightingPS.hlsl` 선별 광원 루프 순회 반영 및 라이팅 GPU 시간 77% 단축(1.10 ms -> 0.25 ms) 실측 검증 완료.
  - **S5 및 후처리/HDR 파이프라인 일원화**:
    - `lightingPS.hlsl`의 반구형 Sky IBL 및 Split-Sum PBR 큐브맵 IBL(`gLightingData.HasIBLMaps`, `gIrradianceMap`, `gPrefilteredEnvMap`, `gBRDFLut`), 접촉 그림자, `ShadowStrength`와 `PostProcessPipeline`(`BloomEffect`, `PostProcessMaterialEffect` 기반 8종 효과) 및 `PostProcessPanel` 반영됨.
    - **HDR 파이프라인 일원화**: `lightingPS.hlsl`의 내장 Reinhard/감마 보정 제거, `LightingColor` 버퍼 포맷 `R16G16B16A16_FLOAT` 승격을 통한 1.0 초과 물리 광량 보존, `PostProcessPipeline`의 ACES Filmic 톤매핑 + 감마 보정 상시 가동(PostProcessComponent 부재 시에도 Default ACES 자동 폴백)을 통해 실행/씬별 과노출 및 탈색 불일치 근본 해결.
    - **공통 디퍼드 파이프라인(`ViewportDeferredPipeline`) 통합**: `SceneViewportPanel`의 수백 줄에 달하던 자체 GBuffer/Shadow/PointShadow/Lighting/Forward 렌더패스 및 커맨드를 `ViewportDeferredPipeline`으로 단일화하여 Scene View, Game View, Material Preview 전반의 렌더링 일관성(스카이 대기, IBL, Directional/Point 그림자 100% 동등성) 확보.
  - **남은 항목**:
    1. 타일당 최대 광원 용량 초과(Overflow) 시 대체 동작 및 다중 광원/이동 카메라 경계 조건의 추가 검증 기록.
- 완료 기준: 작은/큰 씬에서 고정 비용과 이득을 S1 계측 수치로 구분하고, 미지원·용량 초과의 정확한 대체 동작과 IBL/그림자 품질을 확인한다.

### P2-C. 강화학습의 첫 환경

- **2026-11-01 현황**:
  - **Gymnasium 규약 준수 공통 인터페이스 (`RLEnvironment.h`)**: `IRLEnvironment`, `RLObservation`, `RLAction`(이산/연속), `RLStepResult`(관측, 보상, terminated, truncated, info), `RLSpace`(Box, Discrete) 인터페이스 구축.
  - **헤드리스 물리 시뮬레이션 환경 (`CartPoleEnvironment.h/cpp`)**: 창·Renderer·오디오 없는 순수 수치 오일러 적분(50Hz, tau=0.02) 기반 카트폴 역학 모델 구축, 난수 시드 기반 초기화 및 각도/위치 한계값에 따른 Terminated 판정 구현.
  - **로컬 IPC 통신 서버 (`RLServer.h/cpp`)**: Windows Winsock 기반의 TCP 소켓 서버(기본 포트 9999) 구축, 개행 구분 JSON 프로토콜(`get_spaces`, `reset`, `step`, `close`) 처리 완비.
  - **Python Gymnasium 어댑터 (`Tools/RL/bam_gym_env.py`)**: numpy를 사용하는 어댑터와 과거 2개 에피소드 실행 기록이 있다. 표준 라이브러리만의 폴백은 구현되어 있지 않으며 Gymnasium 미설치 분기의 sample 메서드는 보완 대상이다(2026-11-02 정적 대조).
  - **클라이언트 CLI 엔트리포인트 (`Client/Source/Main.cpp`)**: `--rl` 명령행 인자를 통한 독립 헤드리스 서버 구동 연결.
  - **남은 항목**: seed=0 재현·프로토콜 파싱/송신·폴백(F09), 씬 기반 물리 컴포넌트(RigidBody/Joint) 연동 환경, 다중 환경 병렬화(Vectorization), ONNX Runtime 정책 추론 연결(Engine 15절 F).
- 완료 기준: 반복 reset·동일 seed/행동의 재현·실패 요청 처리를 검사하고 환경 상태·관측 버퍼가 섞이지 않음을 확인한다. 과거 2개 에피소드 관찰만으로 전체 계약 검증을 완료 처리하지 않는다.

### P2-D. 아바타·제작 도구의 첫 재생

- **2026-11-01 현황**:
  - **3D 스켈레탈 애니메이션 런타임 & GPU 스키닝 파이프라인 (Phase 1)**: `AnimationEvaluator`(키프레임 mix/slerp, 본 계층 결합, TRS 포즈 크로스페이드), `AnimatorComponent`, `AnimationSystem` 매 틱 갱신 및 `MeshRenderSystem`/`Renderer` 구조화 버퍼(`finalBoneMatrices`) GPU 스키닝 연동 완성.
  - **애니메이션 상태 머신(ASM) 코어 (Phase 2)**: `AnimatorParameters`(Float/Int/Bool/Trigger), `AnimTransition`(크로스페이드 시간, HasExitTime, ExitTime, 조건식 목록), `AnimState`, `AnimatorStateMachine`, `AnimatorRuntimeState` 인스턴스 격리 및 비주얼 스크립팅 노드(`NODE_ANIMATOR_*`) 구축.
  - **1D 블렌드 트리 (Phase 3)**: `BlendTree1D`(속도/파라미터 기반 인접 클립 선형 가중치 보간) 및 `AnimState` 블렌드 트리 연동 완성.
  - **리플렉션 아카이브 직렬화**: `CLASS()`/`STRUCT()` 매크로 등록을 통한 `archive_reflection::WriteObject/ReadObject` 기반 `.animator` JSON 에셋 저장/불러오기 지원.
  - **에디터 시각 저작 패널 (`AnimatorGraphPanel`, Phase 4)**: 18절 지침 풀다운 메뉴바 및 1줄 퀵툴바, 파라미터 독(+추가/삭제/실시간 조작), 캔버스 노드 자유 드래그, "Make Transition" 화살표 연결선, 트랜지션 인스펙터, `FileDialogs` 연동 및 씬 선택 엔티티 활성 상태 청록색 실시간 하이라이트 디버깅 완성.
  - **남은 항목**:
    1. **계층 연결 이후의 정확성과 데이터 보호**: 하위 Skeleton 검색·부모 Animator 조회·`controllerPath` 자동 로드 진입점은 이미 있다(2026-11-02 정적 확인). 상대 경로, 저장/로드 실패, 비반복 종료·부분 트랙·ASM 제어는 [점검 F01·F06·F07](CodeDocumentationAudit-2026-11-02.md)의 잔여 문제다.
    2. **고급 게임플레이 연동 (Phase 5)**: 애니메이션 프레임 이벤트(`AnimNotify`), 상·하체 분리 재생을 위한 레이어드 애니메이션 및 본 마스킹, 패널 내 독립 3D 프리뷰 뷰포트.
    3. **아바타 및 2D 변형 확장**: 2D 변형 메시/파라미터 평가(스프라이트 애니메이션과 독립 경로), 표정/모프·VRM 등 필요한 16·17절 기반 확장.
- 완료 기준: 같은 에셋의 두 실행 인스턴스와 Editor 미리보기가 상태를 공유하지 않고 저장 결과를 다시 재생한다. 코어 구현과 과거 기록은 있으나 전체 경계 조건·저장 보호·PIE/Client 동등성은 추가 검증 대상이다.

### P2-E. Client 시작·배포 — 시작 경로 일부 반영 / 배포 미완료

- **2026-11-01 현황**:
  - `Tools/Scripts/PackageClient.ps1` 기본 복사 스크립트 초안이 추가되었고 `Client/Source/Main.cpp`에 `LoadGameModule()`, `--rl` 헤드리스 강화학습 서버 엔트리포인트, 그리고 `InitializeSystems()`의 필수 시스템(`TransformSystem`, `CameraSystem`, `LightSystem`, `MeshRenderSystem`, `SpriteRenderSystem`, `SpriteAnimationSystem`, `AnimationSystem`, `UIRenderSystem`, `AudioSystem`, `TerrainSystem`, `PhysicsSystem`, `GraphSystem`) 등록이 모두 완료되었다.
  - **2026-11-02 정적 갱신**: 실행 파일 기반 루트 탐색·Runtime 경로 주입·기본 메시 등록·설정 로드·`StartupScenePath` 선택·카메라 등록이 있다. 빈 시작 씬에서만 NewScene2로 폴백한다.
  - `StartupManifestPath`와 `AssetDirectory` 적용, 설정 실패·스키마 검사, 공통 기본 리소스 확보, 디퍼드/후처리 연결은 미완료다.
  - 패키징 초안은 출력 경로 삭제 보호·구성별 바이너리 위치·Config/Model/Scripts 수집·필수 누락 실패 처리가 필요하다([점검 F03~F05](CodeDocumentationAudit-2026-11-02.md)).
- 과거 보류 대상 중 이미 반영된 시작 경로는 현재 사실로 기록한다. 남은 배포 구현을 이 문서 갱신만으로 자동 재개하지 않는다.
- 후속 진행 시 설정 실패 계약·시작 리소스/씬→Build/Cook→Stage/Package 순서로 남은 범위를 검증한다.
- 관련 기능의 Client 실행이 현재 고정 시작 씬에서 확인된 것과 저장소 밖 배포 검증을 구분한다.
- 완료 기준: Editor·개발 경로·현재 작업 디렉터리에 의존하지 않는 배포 폴더에서 설정한 프로젝트가 실행된다.

보류된 작업을 이 문서 작성이나 우선순위 지정만으로 자동 재개하지 않는다.
그 보류가 독립적인 Engine·Editor·시뮬레이션 작업 전체를 막는 조건도 아니다.

## 6. P3: 콘텐츠 제작과 규모 확장

선택한 기능의 기본 실행·수명·저장·계측이 검증된 후 필요한 항목을 확장한다.

- Engine 4·5·7절: CPU 파티클→다중 이미터·모듈, 스플라인 경로, 독립 트레일. 서로 불필요한 선행 조건으로 묶지 않는다.
- Engine 9·10절 및 [AnimatorGraphPlan.md](AnimatorGraphPlan.md): 기존 스켈레탈 평가·ASM·1D 블렌드·저작 UI·계층 연결의 정확성과 저장 보호를 검증한 뒤, Phase 5(AnimNotify, 레이어/본 마스크), [AnimationTimelineEditorPlan.md](AnimationTimelineEditorPlan.md)의 애니메이션·이펙트·노티파이 통합 타임라인 저작 도구 및 런타임 재생 시스템(Phase 1~5 완료), 2D 블렌드·소켓·IK·리타게팅을 확장한다.
- Engine 11절: 실제 요구에 맞는 텍스트 스크립트 언어/VM 선택·엔진 바인딩·배포. 네이티브 모듈과 지원 범위를 구분한다.
- ImGui 10·11·14절: 독립 프리팹 편집, 참조 색인·누락 검사, 임포트 옵션·재임포트, 검색·썸네일·미리보기와 비용 제한.
- 빌트인 리소스와 사용자 애셋 분리 보관 파이프라인 ([ResourceAndAssetSeparationPlan.md](ResourceAndAssetSeparationPlan.md)): `Engine/Resources/` 불변 시스템 자원과 `Assets/` 가변 사용자 콘텐츠의 물리 분리, 가상 마운트 URI(`Engine://`, `Project://`) 해석 계층, 에디터 콘텐츠 브라우저 읽기 전용 보호 및 패키징 시 경량 배포 연동을 구축한다.
- 모델 임포터 자동 메쉬 LOD 생성 파이프라인 ([InteractiveModelImportPreviewPlan.md](InteractiveModelImportPreviewPlan.md) Phase 5): `meshoptimizer`(`meshopt_simplify`) 라이브러리를 도입하여 QEM 기반 정점/UV 경계 보존 메쉬 간소화 수행, `_LOD1.bammesh` 자동 디스크 베이킹 및 `LODGroupComponent` 화면 커버리지 임계값 자동 구성을 연결한다.
- 텍스처 밉맵(Mipmap) 체인 구축 및 GPU 블록 압축(BC7/DDS) ([RenderingDevelopmentPlan.md](RenderingDevelopmentPlan.md) S2-D): CPU `stb_image_resize2` 기반 Mip 0 ~ Mip N 런타임 다운샘플링 체인 생성 및 RHI 다중 서브리소스 업로드(`UploadTextureData`)를 지원하고, 에셋 임포트 시 `DirectXTex`(`texconv`) 오프라인 BC7/DDS 베이킹 파이프라인을 구축하여 원거리 텍스처 앨리어싱 해소 및 VRAM 대역폭을 최적화한다.
- Engine 14절: 해당 예제에 필요한 캐릭터 이동·게임 저장·카메라·오디오·UI 위젯·타일맵·비동기 로딩을 작은 흐름으로 연결한다. 2D 제작은 [스프라이트 그리기 모드와 2D 타일맵](EngineDevelopmentPlan.md#l-스프라이트-그리기-모드와-2d-타일맵)의 그리기 모드와 기본 타일 배치를 독립적으로 검증하고, 배치와 저장 이후 이웃 규칙을 연결한다.
- Engine 14절 M 및 [NetworkFoundationPlan.md](NetworkFoundationPlan.md): 소켓 전송 계층(`INetworkTransport`)과 바이너리 패킷 프로토콜, 세션 라이프사이클(Connect/Disconnect/Ping) 기초를 구축하고, ECS 엔티티 리플리케이션(`NetworkIdentity`, `NetworkTransform`)과 RPC로 확장한다. 멀티플레이 전체를 강화학습이나 V-Tuber의 필수 선행 조건으로 묶지 않는다.
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
- 내비게이션·게임 AI·씬 스트리밍은 대상 게임의 요구에 따라 진행하고, 멀티플레이어 고급 확장(클라이언트 예측, 랙 보상, 대역폭 최적화)은 기본 네트워크 세션 및 리플리케이션 검증 이후 단계로 확장한다 ([NetworkFoundationPlan.md](NetworkFoundationPlan.md) Phase 5).
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
- BuildAndPackagingPlan은 P2-E의 구현 반영·잔여/보류 범위를 구분하고 후속 작업에서 1~4단계의 미충족 기준을 따른다.

각 계획의 상세 기능 목록과 완료 기준은 원문에서 유지한다. 우선순위 분류만으로 기존 검증 상태를 바꾸지 않는다.

## 10. 조사 시점의 문서·코드 차이와 재조정된 단기 실행 순서 (2026-11-02 갱신)

아래 목록은 기존 2026-11-01 기록과 이후 반영 내역을 포함한다. 2026-11-02 점검 범위·추가 결함·검증 한계는 [점검 문서](CodeDocumentationAudit-2026-11-02.md)를 따른다.

- **기존 구현 기록과 현재 코드 반영 영역**:
  - **렌더링 S1·S3·S4·S5 및 HDR/공통 디퍼드 파이프라인 통합**:
    - `RHIFrameMetrics` 및 `DirectX12RHI` Timestamp QueryHeap 연동, `MetricsPanel`의 기존 GPU 시간·드로우콜 관찰 기록. 최신 통제 비교 완료와 구분한다.
    - `MeshRenderSystem` 다중 뷰포트/패스별(`PassTarget`) 독립 카메라 기준 LOD 평가 및 주 카메라 전용 상태 보존 완료.
    - `lightingPS.hlsl` 선형 HDR 출력, `LightingColor` `R16G16B16A16_FLOAT` 승격, ACES 톤매핑 일원화 및 `ViewportDeferredPipeline` 공통 통합 완료.
    - `MaterialInterface::Bind()`의 미지정 G-Buffer 슬롯 기본 텍스처 리셋 코드. 기본 텍스처 미로드/바인딩 실패까지 보호하는 것은 아니다(F08).
  - **비주얼 스크립팅 & 그래프 에디터 (P2-A)**:
    - `GraphSystem::ReloadReflectedNodes` 런타임 변수 보존 핫스왑 및 `Application.cpp` 콜백 연동 완료.
    - `EGraphPinType::String` 문자열 핀 타입(`Core.String`, `Core.Reroute.String`, 리플렉션 `std::string` 매핑, 아카이브 직렬화, `GraphPanel` `InputText`) 완전 지원 완료.
    - `GraphPanel` 상단 윈도우 풀다운 메뉴바(`File`, `Edit`, `View`, `Mode`, `Debug/Build`) 및 1줄 퀵 액세스 툴바 리팩터링 완료.
  - **물리 질의 및 2D/3D 애니메이션 파이프라인 (P2-A & P2-D)**:
    - `PhysicsSystem::Raycast`/`RaycastAll`의 `RaycastQuery`(거리, 레이어마스크, 트리거, 무시 엔티티, 조기 탈출) 확장 및 블루프린트 노드 노출 완료.
    - 독립 `SpriteAnimation` 에셋(`.spriteanim`), `SpriteAnimationComponent`/`System`, 전용 `SpriteAnimationEditor`(`ResourceHandle` 기반 안전 참조) 완성.
    - 3D 평가기·ASM·1D 블렌드·AnimationSystem·저작 패널·블루프린트 노드의 기본 구현. Phase별 전체 완료와 구분하고 F01/F06/F07/F11을 마무리한다.
  - **강화학습 첫 헤드리스 환경 (P2-C)**:
    - RL 환경·CartPole·loopback 서버·`Client --rl`·Python 어댑터의 기본 연결과 과거 실행 기록. seed·프로토콜·폴백은 F09의 잔여 범위다.
  - **클라이언트 시스템 등록 및 엔진 안정화 (P0-B & P1-D)**:
    - `Client/Source/Main.cpp`의 `InitializeSystems()`에 `AnimationSystem`, `SpriteAnimationSystem`, `GraphSystem`, `PhysicsSystem`, `TerrainSystem` 등 전체 필수 시스템 등록 완료.
    - `DirectX12RHI.cpp` 내 3곳의 `std::terminate()` 제거 및 `EResult::Fail` 반환 처리 완료.
    - `ContentBrowser` 공통 열기(`OpenAssetOrFolder`), 외부 앱 실행(`ShellExecuteW`), 검색 범위 토글 및 `ResourcePicker.h` 1회 스캔 캐싱 완료.
    - **하단 푸터 바(`Footer Bar`) 분리 (P1-B)**: 상단 PIE 바에서 C++ 모듈 컴파일·핫리로드·자동컴파일·셰이더리로드 버튼을 에디터 최하단 슬림 푸터(`##FooterBarRow`)로 이관하고 툴팁 기반 간결한 UI로 정리 완료 ([ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md) 19절).
    - **콘텐츠 브라우저 시각 분류 고도화 및 복합 확장자 표준화 (P1-D)**: O(1) 파일명·복합 확장자(`.bam*.json`)·폴더 문맥 기반 고유 아이콘/색상/배지 표시, 단일 확장자(`.animator`, `.spriteanim`, `.anim`, `.materialinstance`) 및 스크립트/컨피그/셰이더 분류, 네임박스 정제명(`CleanName`) 표시, `EditorCommon.Open` 로캘 반영 완료 ([ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md) 20절).
  - **단일 CPU 파티클 시스템 1단계 및 애니메이션·FX 타임라인 계획 수립 (P3)**:
    - `Particle`, `ParticleEmitterComponent`(방출률, 버스트, 수명, 속도, 크기, 회전, 색상 보간, World/Local 공간, 고정 시드, 원뿔 확산) 및 `ParticleSystem` 구축 완료.
    - `Renderer::RenderSprite` 텍스처 상태 중복 바인딩 캐싱(`lastTexture`) 최적화 및 인스펙터 실시간 카운트/재생 제어 UI 연동 완료.
    - [AnimationTimelineEditorPlan.md](AnimationTimelineEditorPlan.md) 수립: 비-PIE 실시간 프리뷰 루프 및 애니메이션·이펙트·노티파이 통합 타임라인 저작 도구 로드맵 수립.

### 10.1 우선순위 조정의 기준

결함의 심각도와 실제 작업 순서를 구분한다. 데이터 손실·파괴 위험은 먼저 막고,
사용 중인 편집·임포트·재생 경로의 정확성을 확보한 다음 기능을 확장한다.
사용 빈도가 낮거나 보류된 경로의 전체 구현을 다른 모든 기능의 선행 조건으로 만들지 않는다.

- `MaterialFeatureFlags`는 **기능 확장 중 첫 순위**를 유지하되, 저장 보호·기본 리소스 정확성보다 먼저 진행하지 않는다.
- 이미 반영된 계층 Animator 연결·컨트롤러 자동 로드·매크로·로거 전환은 새 구현 목록에서 제외한다. 남은 오류와 실행 검증만 진행한다.
- 썸네일 예산과 참조 검증을 통합 설정 창·편의 메뉴 확장보다 앞에 둔다. 성능 문제의 실제 크기는 측정으로 판단한다.
- F03의 삭제 범위 보호와 F04의 전체 패키징 구축을 분리한다. 배포 전체는 조건부 경로로 유지한다.
- 아래 단계는 기본 선택 순서다. 서로 독립적인 경로를 하나의 전역 대기열로 묶지 않는다.

### 10.2 단계 0 — 수정 전 기준과 기존 오류 확인

- 변경할 기능마다 커밋·구성·재현 입력·기존 검사 결과를 먼저 기록한다. 코드 수정 전 영향을 받는 타깃과 검사만 확인한다.
- `SceneArchiveCheck`의 과거 실패 목록은 현재 재현 여부를 확인한다. 과거 실패 파일만으로 현재 실패를 확정하지 않는다.
- `Logs/Editor.log`의 바이너리 머티리얼 읽기 오류를 현재 샘플로 좁힌다. 현재도 재현되면 관련 에셋·포맷 문제를 해당 리소스 작업보다 먼저 해결한다.
- **완료 기준**: 현재 재현되는 실패와 과거 로그를 구분할 수 있다. 작업마다 관련 검사를 수행하며, 매 단계에서 무관한 전체 검사를 반복하지 않는다.
- 이 문서 작성에서 빌드·검사·재현을 실행한 것은 아니다.

### 10.3 단계 1 — 편집 데이터와 출력 보호 [당시 구현 기록 / 삭제 보호 재개]

1. **AnimatorGraphPanel 저장 보호 — P1-A / F01·F11 [완료]**
   - 원자적 파일 저장/로드 및 실패 결과 전파(Step 1), New/Open/종료 미저장 보호 모달(Step 2), InputManager 및 로캘 일원화(Step 3) 완료 (2026-11-02).
   - **후속 대기 과제 (목록 등록)**: GraphPanel과의 캔버스 UI 토대 일원화(`imgui-node-editor` 기반 캔버스 마이그레이션)는 데이터 안전성 확보 후 고도화 스프린트에서 착수.
2. **패키징 삭제 범위의 최소 보호 — P2-E / F03 [부분 반영 / A01·A02 잔여]**
   - 입력/출력 절대 경로 정규화, 프로젝트 저장소 루트(`$Root`)·드라이브 루트·핵심 디렉터리(`Engine`, `Editor`, `Resources` 등) 삭제 차단 가드 구축.
   - `-LiteralPath` 적용 및 필수 바이너리 누락 시 패키징 중단 예외 처리 완료.
   - **검증 완료**: `.`, `Resources`, `C:\` 전달 시 삭제 수행 전 `Security Violation` 예외 정상 차단 확인.
3. **모델 임포트 실패 전달·재임포트 일관성 — P1-D / F02 일부 [완료]**
   - 부모 기본 머티리얼(`SkinningMaterial.bammat.json`/`DefaultMaterial.bammat.json`) 로드 실패 시 필수 실패(`EResult::ResourceNotFound`) 및 등록 롤백 처리 완료 (2026-11-03).
   - `settings.Overwrite` 플래그를 임베디드 텍스처 디스크 기록에 반영하여 재임포트 일관성 확보 완료.
   - `ValidateImportOutput` 패턴 및 `SaveToBinaryFile` 반환값 검증을 머티리얼에도 적용하여 파일 저장 실패 은폐 원천 차단 완료.
   - **완료 기준 달성**: 머티리얼 저장 실패 시 임포트 전체가 성공으로 표시되지 않고 즉시 실패를 전파하며, 재임포트 결과가 Overwrite 정책과 일치한다.

각 작업은 독립된 작은 변경으로 검토한다. 2026-11-06 현재 출력 삭제 보호와 패키징 생성 실패(A01·A02)가 남아 단계 1 전체 완료로 보지 않는다.

### 10.4 단계 2 — 기존 리소스·재생·시작 경로의 정확성 [완료]

- **에셋 경로·기본 슬롯 — P1-D/P1-E / F06 일부·F08 [완료]**:
  - `ResourcePicker.h`의 CWD 독립 에셋 루트 경로(`PlatformPath::ResolveProjectRelative`) 적용 및 `.bamshader.json` 확장자 필터 보완 완료 (2026-11-03).
  - `MaterialInterface`/`MaterialInstance`에서 기본 슬롯 안전 바인딩 및 텍스처 상태 누수 원천 차단 완료.
- **애니메이션 기본 동작 — P2-D / F06 일부·F07 [완료]**:
  - 비반복 애니메이션 종료 시 clamp 적용으로 마지막 포즈 정상 보존 (`fmod(duration, duration) == 0` 리셋 버그 해결).
  - 부분 트랙 누락 채널에서 0/identity 대신 본의 원래 바인드 로컬 TRS를 분해하여 보존하도록 `SampleLocalTRS` 보완.
  - ASM `isLooping` 제어, `Stop()` 시간 리셋 및 `controllerPath` 상대 경로 정규화 완료.
- **기존 Client의 실패 보고 — P2-E / F05 일부 [완료]**:
  - `ConfigManager::Load` 반환 결과를 검사한다. 현행은 설정 파일 부재 시 경고·기본값 성공, 파싱 실패 시 실패 반환이다. 배포의 누락 정책은 A08에 따라 별도 확정한다.
  - `SchemaVersion` 유효성(0 검사) 및 최대 지원 버전 초과 검사 추가, 시작 씬 부재 시 명확한 `FileNotFound` 반환 연결 완료.

### 10.5 단계 3 — 머티리얼 기능 확장 (P1-E / 렌더링 S2-C) [완료]

- **3-A: MaterialFeatureFlags와 기본값 분기 [완료]**:
  - 머티리얼 슬롯 점유 여부를 비트 플래그로 G-Buffer 셰이더(`gbufferPS.hlsl`, `shading_flags.hlsli`)에 공급하고, 텍스처 없는 슬롯은 샘플링을 바이패스하여 텍스처 상태 누수 원천 차단 및 대역폭 절감 달성 (2026-11-03).
- **3-B: 그래프 상수·벡터·채널 분리 입력 [완료]**:
  - `ShaderGraphCompiler`에 G-Buffer PBR 머티리얼 HLSL 코드 생성기(`GenerateGBufferMaterialHlsl`) 구현 및 `Shader.MaterialOutput` 노드 연동 완료.
  - `Math.MakeFloat3`, `Math.BreakFloat3`, `Math.MakeFloat4`, `Math.BreakFloat4` 채널 분리/결합 노드 지원.
  - `ShaderGraphCheck` 회귀 테스트 작성 및 CTest 통과 완료.

### 10.6 단계 4 — 제작 안정성과 작업량 제한 (P1-D / P3 준비)

- **썸네일 축소·캐시/업로드 예산 — F12 [완료]**:
  - `LoadImageThumbnail`에서 원본 디코드 후 최대 128×128로 축소해 보관·업로드 데이터를 제한한다. 원본 디코드 피크 감소율을 뜻하지 않는다(A07).
  - 메인 스레드 대기열에서 프레임당 최대 4개 업로드를 처리한다. hitch 제거 여부는 프레임 시간 측정으로 확인한다.
  - LRU 기반 최대 256개 항목 / 최대 64MB VRAM 바이트 예산 퇴거(Eviction) 메커니즘 구축 완료.
- **참조 검증·재임포트 완성 및 바이너리 머티리얼 조사 — F02 잔여 [완료]**:
  - `PlatformPath::ToProjectRelative` 유틸리티 추가 및 `ModelImporter::CreateJob`에서 에셋 출력 경로의 절대 경로 하드코딩 방지 완료.
  - `AssetReferenceValidator` 정적 검증 코어 구현: `Resources/` 내 전체 씬/프리팹/머티리얼/모델 JSON 대상 누락(`MissingFile`), 외부 절대 경로(`ExternalAbsolutePath`), 확장자 오류(`InvalidExtension`) 전수 검출 및 원자적 자동 복구(`FixIssue`, `FixAllSafeIssues`) 지원.
  - `AssetReferencePanel` 에디터 UI 구축 및 `ImGuiManager` 메뉴 등록 (`Window -> Panels -> Asset Reference Checker`): 누락/외부/확장자별 필터링, 경로 검색, 안전 항목 원클릭 일괄 복구 연동 완료.
  - 구형 28바이트 바이너리 머티리얼 및 확장자 오류 파일 조사 완료, `TestScene.json` 및 `TestScene4.json` 내 구형 `.bammat` 참조를 최신 `.bammat.json` 표준 규약으로 정규화 완료.
- **완료 기준 달성**: 에셋 탐색의 메모리·대기열·업로드가 설정한 상한 안에서 관리되고, 누락 참조 식별 및 안전 복구 정책이 에디터 UI와 코어로 확보되었다.

### 10.7 단계 5 — 제작 도구와 콘텐츠 확장

- **애니메이션 기능 확장 — P3 / Phase 5 [코어 및 런타임 완료]**:
  - **애니메이션 노티파이 (AnimNotify)**: `AnimNotify` 및 `AnimNotifyEvent` 데이터 모델 구축, `AnimState::notifies` 직렬화(`ALLOW_MISSING` 하위 호환성 확보), 루프 래핑 구간 포함 타임라인 검출 및 `AnimatorComponent::notifyCallbacks` 디스패치 파이프라인 완성.
  - **레이어드 애니메이션 & 본 마스킹 (BoneMask)**: `BoneMask` 스켈레톤 계층 가중치 빌더, `AnimationEvaluator::EvaluateMaskedSkinningMatrices` LERP/SLERP 합성 엔진 구현, `AnimatorComponent::layers` 다중 레이어 구조 및 비주얼 스크립팅 노드(`Animator.SetLayerWeight/GetLayerWeight/StopLayer`) 등록 완료.
  - **리플렉션 제네릭 컨테이너 에디터**: `ReflectionPropertyDrawer`에서 `std::vector<T>`를 지원하여 인스펙터에서 `[+]` 요소 추가, 트리 노드 전개, 구조체 하위 프로퍼티 편집, `[-]` 삭제 완비.
  - **런타임 검증 완료**: `Fox` 모델(베이스: `Fox_Walk`, 상체 레이어: `Fox_Survey`, 마스크 본: `b_Spine01_02`)을 통해 상·하체 분리 블렌딩 및 가중치 실시간 조절 동작 검증 완료 (2026-11-03).
  - **남은 목표 및 후속 과제**:
    1. *애니메이터 프리뷰와 저장 검증*: 독립 프리뷰의 레이어 합성, 2D 사각 블렌드와 `Any State`는 구현됐다. `BlendTree2DCheck` 통과를 확인했으며, 실제 모델과 저장·재열기 검증이 남았다. [애니메이터 계획](AnimatorGraphPlan.md)
    2. *2D 블렌드 확장*: 임의 샘플 배치와 삼각망 보간.
    3. *상태 머신 캔버스 UI 토대 일원화*: `imgui-node-editor` 기반으로 마이그레이션하여 줌/패닝 및 정렬 일관성 확보.
- 통합 설정 창과 그래프 핀/와이어 편의 메뉴는 기존 저장·초안·입력 동작을 보존하면서 진행한다. 이미 있는 패널/서비스를 재사용한다.
- 단일 CPU 파티클·스플라인·트레일은 각각 필요한 실행·저장·렌더 제출 기반으로 진행한다. 전체 네트워크나 배포 완료를 기다리지 않는다.
- 네트워크는 `NetworkFoundationPlan`의 Phase 1~5(소켓 전송 추상화, 바이너리 패킷 프로토콜, 세션/3-Way 핸드셰이크, ECS NetworkSystem 트랜스폼 동기화 및 에디터 NetworkProfilerPanel) 구축을 2026-11-03 완료하였다. 이후 필요 시 고급 RPC 및 클라이언트 예측 연구로 확장한다.
- 키네마틱 캐릭터 컨트롤러는 `KinematicCharacterControllerPlan`의 계획에 따라 ECS 기반 `CharacterControllerComponent` 및 `KinematicCharacterSystem`을 통해 지면 감지, 경사로 슬라이딩, 턱/계단 오르기(Step-Up), 점프 및 중력 시뮬레이션, 비주얼 스크립팅 노드 7종 및 에디터 뷰포트 디버그 캡슐 렌더링을 구축 완료하였다 (2026-11-03 완료).
- 2D/3D 통합 물리 트리거 & 다중 채널 디스패치 파이프라인(`TriggerCallbackComponent`, `ScriptableEntity`/`NativeScriptComponent`, `Event.OnTriggerEnter/Stay/Exit` 비주얼 스크립트 노드 및 `PhysicsSystem::Overlap3D`)을 구축 완료하였다 (2026-11-04 완료).
- 3인칭 캐릭터 시스템(ThirdPersonCharacterSystemPlan Phase 3) 및 애니메이션 블렌딩 고도화:
  - 1D 블렌드 트리(Walk ~ Run) 내 Normalized Time 기반 위상 동기화(Phase Matching) 및 루프 끝-시작 0.05초 크로스페이드 스무딩 구현.
  - 이동 속도 연동 재생 배속(`ScalePlayRateWithSpeed`, `MinPlayRate`, `MaxPlayRate`) 적용으로 발 미끄러짐 방지 및 히스테리시스 전이 조건 안정화.
  - 리플렉션 역직렬화 하위 호환성(`ALLOW_MISSING`) 확보 및 Fox 모델 실시간 이동 검증 완료 (2026-11-05 완료).
- **모델 임포트 시 자동 메쉬 LOD 생성 파이프라인 — P3 / Phase 5 ([InteractiveModelImportPreviewPlan.md](InteractiveModelImportPreviewPlan.md))**:
  - `meshoptimizer`(`meshopt_simplify`) 기반 QEM 메쉬 간소화 알고리즘 도입.
  - 임포트 옵션(`GenerateLODs`, `LODCount`, `LODReductionRates`)에 따라 `Model_Mesh_LOD1.bammesh` 파생 에셋 자동 영속화.
  - 엔티티 인스턴스화 시 `LODGroupComponent` 자동 장착 및 기존 `MeshRenderSystem`의 다중 뷰포트/패스별 화면 커버리지 렌더링에 자동 연동.
- **텍스처 밉맵 체인 및 GPU 블록 압축 파이프라인 — P3 / S2-D ([RenderingDevelopmentPlan.md](RenderingDevelopmentPlan.md))**:
  - 현재 `Texture.cpp`의 단일 밉(Mip 0) 하드코딩 및 `DirectX12RHI::UploadTextureData`의 서브리소스 1개 제약을 해소.
  - 런타임: `stb_image_resize2` 기반 CPU 밉맵 체인 생성 및 RHI 다중 서브리소스 순회 업로드.
  - 오프라인/임포트: `DirectXTex`(`texconv`)를 활용한 BC7/BC5 블록 압축 및 밉맵 내장 `.dds` 에셋 베이킹 파이프라인 구축.
- **빌트인 리소스와 사용자 애셋 분리 보관 파이프라인 — P3 / 에셋 아키텍처 ([ResourceAndAssetSeparationPlan.md](ResourceAndAssetSeparationPlan.md))**:
  - `Engine/Resources/` 불변 시스템 자원(기본 셰이더, 프리미티브 메시, 기본 텍스처, 엔진 로캘)과 `Assets/` 가변 사용자 콘텐츠(씬, 임포트 모델, 텍스처, 오디오, 머티리얼)의 물리 디렉터리 분리.
  - `ResourceManager` 및 `PlatformPath`에 가상 마운트 URI(`Engine://`, `Project://`) 해석 계층 구축 및 레거시 상대 경로 하위 호환 매핑.
  - 에디터 `ContentBrowserPanel`의 `Show Engine Content` 뷰 토글 옵션 및 빌트인 자원 읽기 전용 보호 적용.
  - `PackageClient.ps1` 연동: 에디터 전용 리소스 및 미사용 원본 테스트 파일 제외, 런타임 필수 엔진 자원과 프로젝트 에셋 분리 수집.

### 10.8 사용 조건에 따라 먼저 처리할 경로

- **RL을 다시 학습/비교에 사용할 때 — F09 [완료]**:
  - `IRLEnvironment`/`CartPoleEnvironment`의 `seed=0` 재현성(std::optional<uint64_t>)을 수정하여 `reset(seed=0)`의 동일 상태 초기화 보장 (2026-11-03).
  - `RLServer.cpp`에 Glaze JSON 파서, 소켓 부분 전송 방지 `SendAll` 루프, 64KB 버퍼 상한 가드 구축.
  - `bam_gym_env.py` 순수 표준 라이브러리 폴백 및 `sample()` 메서드 `self` 바인딩 에러 수정 완료.
- **배포 작업 — F04·F05 [과거 실행 기록 / A01·A02·A08 잔여]**:
  - `PackageClient.ps1`의 멀티 구성 프리셋(`bin/$Config`, `bin` 등) `$BinDirCandidates` 다단계 자동 탐색 구축.
  - `$Root/Config/Project.json` 필수 설정 및 `Resources` 하위 핵심 에셋 온전한 수집 자동화 및 Standalone Client 독립 실행 검증 완료.
- **Release 에디터의 모듈 컴파일 — F13 [완료]**:
  - `HotReloadManager.cpp`의 `ResolveBuildDirectory()`에 에디터 실행 파일(`Editor.exe`) 기준 `CMakeCache.txt` 런타임 역추적 및 Debug/Release 프리셋 경로 동적 분기 구축 완료.
- **타일 용량에 접근하는 광원 장면을 사용할 때 — F10**:
  오버플로 계측·정확한 대체 동작을 먼저 구현하고 255/256/257개 경계를 확인한다. 용량 초과가 현재 재현되면 렌더링 신규 기능보다 우선한다.

모든 단계의 완료는 **코드 반영 → 관련 검사 → 정상/실패 실행 → 기록**으로 구분한다.
한 F 항목을 여러 단계로 나눴다면 남은 범위를 명시하고 전체를 조기에 완료 처리하지 않는다.

코드 직접 수정·빌드·테스트 권한은 사용자 요청과 AGENTS.md를 따르며 이 문서가 실행 허가를 대신하지 않는다.

## 11. 현재 작업 트리 기준 우선순위 재평가 (2026-11-07 갱신)

근거는 [2026-11-06 점검 A01~A10](CodeDocumentationAudit-2026-11-06.md) 및 2026-11-07 우선순위 0~1 구현이다. 10절의 과거 완료 기록보다 이 절의 잔여 범위와 작업 순서를 우선한다.

### 11.1 선행 안정화 및 결함 해결 내역 [우선순위 0·1 완료]

1. **CTest 회귀 4종 복구 및 전체 단위 검사(29/29) 정상화 [완료] (2026-11-07)**:
   - `Graph::ImportData`: 트랜잭션 롤백, 버전 검증(`UnsupportedVersion`), 노드 ID 0/중복/범위 검증 복원으로 `GraphFoundationCheck` 통과.
   - `GraphReflection`: 공통 빌트인 노드(`Math.MakeFloat3`, `Math.MakeFloat4`)에 CPU 리플렉션 실행 바인딩을 주입하는 경우에만 도메인 보존 결합(`RegisterOrUpdate`)을 허용하고, 리플렉션 간 중복 시도(`Check.Counter.Value.Set` 등)는 엄격히 차단하여 `GraphReflectionCheck`, `GraphExecutionCheck`, `ShaderGraphCheck` 동시 통과.
   - `ReflectionInspectorCheck`: 리소스 피커 컨텍스트 메뉴용 `DrawResourceContextMenu`/`RequestOpenResourceEditor` 스텁 추가로 링크 오류 해결.
   - `HierarchyEditorCheck`: 불필요한 무거운 의존성(`EntityClipboard`, `PrefabModeManager`)을 CMake 타깃에서 제거하고 최소 스텁으로 고립시켜 링크 오류 해결.
   - `SceneArchiveCheck`: `Model` 리소스의 소멸자 접근 권한(`friend class ResourceManager`) 및 `rm.DestroyResource(tempModel)` 표준 경로 일원화, 타깃 `UNITY_BUILD OFF` 부여로 중복 컴파일 해소.
2. **에셋 감시자 UTF-8·예외 경계 — A03 / P0-F [완료] (2026-11-07)**:
   - `TextureFileListener` 콜백의 `Utf8ToPath` 및 `try-catch` 경계 적용 완료.
   - `ProcessModifiedLocalization`의 미사용 narrow `.string()` 제거 및 `scriptPath.wstring()` + `_wsystem` 호출로 비-ASCII/다국어 프로젝트 경로 안전성 확보 완료.
3. **RT 진단의 입력 차원 보호 — A04 / P1-E [완료] (2026-11-06)**:
   - `ChannelFilter::CanSampleAsTexture2D` 플래그 검사(2D, 1-Array, Non-MSAA, Sampler), `BaseViewportPanel`의 미지원 큐브맵(`PointShadowDepth`) 2D 오버레이 차단 및 안내 문구 표시 완료.
4. **모델 편집·임포트 splitter 저장값 보존 — A05 / P1-B [완료] (2026-11-06)**:
   - `ModelEditor`, `ModelImportModal`, `ConsolePanel`에서 `Editor::DrawSidebarLayout` 및 드래그 상태 조건 분기를 통해 표시 클램프와 저장 선호 폭/높이 분리 완료.
5. **타일 컬링 디스패치 범위 — S4 / A06 후속 [완료] (2026-11-06)**:
   - `LightManager::DispatchLightCulling`에서 버퍼 할당 크기가 아닌 실제 G-Buffer 위치 텍스처 크기(`rtWidth`, `rtHeight`) 기준으로 디스패치 그룹 수 계산 일치 완료.
6. **패키징 삭제 범위 보호 및 New-Item 수정 — A01·A02 / F03 [완료] (2026-11-07)**:
   - `PackageClient.ps1`의 ``에 소스 디렉터리 `"Client"` 추가.
   - `[가드 4]` 프로젝트 저장소 외부 임의 디렉터리 삭제 방지 가드 구축.
   - PowerShell 표준에 맞춰 `New-Item -ItemType Directory`의 `-LiteralPath`를 `-Path`로 교정 완료.
7. **에셋 압축률 왜곡 원인 제거 — A07 [완료] (2026-11-07)**:
   - 비정상 92바이트 PNG 파일(`Resources/rgba2x2*`) 제거 완료.

### 11.2 사용 조건에 따라 앞당길 작업

- **다수 광원 지원 확장 — A06 본체 / F10**: 현행 씬 광원 업로드 상한은 32개다. 이 상한을 확장할 때 CPU 버퍼 용량·셰이더 순회 범위와 타일당 256개 초과 계측·폴백을 함께 검증한다.
- **재임포트 작업**: Overwrite/SaveToFile 조합, 기존 핸들 유지, 중간 파일 저장/부모 머티리얼 실패를 확인한다. 정적 검토만으로 다중 파일 트랜잭션 완료를 선언하지 않는다.
- **2D 제작 예제**: 로그에 남은 구형 sprite/texture 로드 실패의 현재 재현 여부를 먼저 확인한다. 현행 로더 문제와 오래된 샘플 데이터 문제를 분리한 뒤 Engine 14절 L의 Simple/Filled 또는 기본 TileMap 중 필요한 작은 예제로 진행한다.

### 11.3 안정화 다음 기능 선택 (차기 4개 트랙)

- **트랙 A (렌더링 품질 — S2-D)**: 텍스처 밉맵(Mipmap) 체인 생성 및 RHI 다중 서브리소스 업로드 (`stb_image_resize2` 기반 CPU 밉맵 생성 및 다중 서브리소스 순회 업로드).
- **트랙 B (모델 파이프라인 — InteractiveModelImport Phase 5)**: `meshoptimizer`(`meshopt_simplify`) 기반 자동 메시 LOD 생성 파이프라인.
- **트랙 C (에셋 아키텍처 — ResourceAndAssetSeparation Phase 1~2)**: 가상 마운트 URI(`Engine://`, `Project://`) 경로 해석 계층 및 단위 테스트 구축 (물리 폴더 이동 전 선행).
- **트랙 D (2D 제작 워크플로 — Engine 14절 L)**: 스프라이트 Fill / 9-Slice 또는 기본 2D 타일맵.

### 11.4 상태·검증 기록 규칙

- 2026-11-07 기준 CMake Debug 빌드 및 CTest 전체 29개 테스트(29/29 Passed) 100% 통과 확인 완료.
- 기준 커밋, 미커밋 범위, 구성, 검사 이름과 실제 실행 개수, 실패 시나리오, UI/GPU 미검증을 기록한다.
- 소스 전면 이동·싱글턴 전면 교체·모든 문서의 장기 목표 구현을 완료 조건으로 추가하지 않는다.