# 통합 개발 우선순위와 완료 기준

- 작성일: 2026-10-24 (최종 갱신: 2026-11-23)
- 상태: 2026-11-05~11-07 우선순위 0~1(CrashHandler·Recovery, A01~A10 후속) 및 2026-11-08~11-15 리소스/에셋 경로 분리, 2D 확장(Phase 1~5), 파티클 물리·CompositeVFX(Phase 1~4), 3줄 리소스 편집기·공용 UI·PluginHost, Render Pipeline Profile 및 독립 클라이언트 패키징/종료 수명주기 안정화, 2026-11-19~11-22 머티리얼 에디터/셰이더 그래프·KCC·신규 6대 엔진 시스템(스프린트 C)에 이어, 2026-11-23 스프린트 D(카메라 시퀀서·RL 벡터화·`.bampak` 2-팩 패키징·노드/기즈모), 그룹 1(카메라 트랙 UI·독립 Client `.bampak` 부트스트랩·NavGrid 베이크/오버레이), 그룹 A(1~3: 비주얼 스크립팅 확장·RL 커리큘럼/SaveGame·`BAM_EDITOR`/`BAM_SHIPPING` 스텁/스트립), 그룹 B-4(`ResolveResourceHandle` 리팩터링) 및 렌더링·스트리밍 개선(`57fafba5`~`a7547fb7`)이 `main`(`c18c8ea2`)에 반영되었다.
- 최신 근거: [MaterialShaderGraphIntegrationPlan.md](MaterialShaderGraphIntegrationPlan.md), [MaterialGraphSynchronizationAndStageDesign.md](MaterialGraphSynchronizationAndStageDesign.md), [MaterialPrimaryPassTypeDesign.md](MaterialPrimaryPassTypeDesign.md), [BuildConfigurationAndPreprocessing.md](BuildConfigurationAndPreprocessing.md), [CodeRefactoringPlan.md](CodeRefactoringPlan.md), 최신 커밋선(`c2058594` $\to$ `c5f714e6` $\to$ `40c91851` $\to$ `c18c8ea2`), [ImplementationLog.md](ImplementationLog.md) 2026-11-23 항목. **현재 작업 선택은 11절을 우선한다.** 위 상태와 10절의 과거 완료 표시는 당시 기록이며 미검증 빌드/실행 항목(x64-release·독립 Shipping Client·`RLSimulationCheck` 실패 해소·Emscripten 툴체인·실 크래시·수동 비교)을 해소한 뜻이 아니다.
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
- [ParticleAndEffectSystemPlan.md](ParticleAndEffectSystemPlan.md): 모듈형 파티클 & 비주얼 이펙트 시스템, 물리 충돌/기류 상호작용 및 독립 에디터 계획.
- [CompositeVisualEffectPlan.md](CompositeVisualEffectPlan.md): 계층형 통합 비주얼 이펙트 시스템(VFX Composite), 메쉬 이펙트/트레일/라이트 섬광 및 트랙 기반 오케스트레이터 계획.
- [SpriteAnd2DSystemPlan.md](SpriteAnd2DSystemPlan.md): 스프라이트 시트 슬라이싱, 9-Slice/Filled 렌더링, 픽셀 퍼펙트 카메라, 2D 타일맵 시스템 및 애니메이션 자동 연동 계획.
- [CrashHandlingAndRecoveryPlan.md](CrashHandlingAndRecoveryPlan.md): 비정상 종료 진단(SEH/미니덤프, 크래시 리포트) 및 에디터 하트비트 자동 저장/작업 복원 계획.
- [ResourceAndAssetSeparationPlan.md](ResourceAndAssetSeparationPlan.md): 빌트인 리소스(`Engine/Resources`)와 프로젝트 애셋(`Assets`) 분리 및 가상 마운트 URI(`Engine://`, `Project://`) 계획.
- [EditorPluginArchitecturePlan.md](EditorPluginArchitecturePlan.md): Builtin·Custom·개발용 에디터 DLL 플러그인 발견, 승인, 핫리로드 및 런타임 모듈 확장 계획.
- [RenderPipelineProfilePlan.md](RenderPipelineProfilePlan.md): Game View와 Client의 공용 Deferred 렌더 파이프라인 프로필 검증 및 실행 계획.
- [EditorUXTestGuidelines.md](EditorUXTestGuidelines.md): 에디터 사용성(UX) 테스트 3계층, 환경 매트릭스, 6대 영역별 Pass/Fail 기준 및 미검증 UI 회귀 체크리스트.
- [ShaderToolchainEvaluation.md](ShaderToolchainEvaluation.md): 셰이더 다중 백엔드(WebGPU/Vulkan/D3D12/Metal) 컴파일 툴체인 비교 및 Slang 도입 타당성 검토.
- [OutOfProcessAssetPipelinePlan.md](OutOfProcessAssetPipelinePlan.md): 독립 워커 프로세스 기반 대용량 에셋 임포트 파이프라인, Job Object 수명주기 및 60FPS 무간섭 격리 계획.
- [AssetPipelineAndRenderingOptimizationPlan.md](AssetPipelineAndRenderingOptimizationPlan.md): 에셋 파이프라인 및 렌더링 최적화 조치 계획서 (와인딩 반전, 프리뷰 카메라 동결, 콘텐츠 브라우저 스터터링, 스폰자 드로우콜 병목).
- [DiskCacheDevelopmentPlan.md](DiskCacheDevelopmentPlan.md): 썸네일 디스크 캐시 및 엔진 전반(셰이더, DDC, 지형/물리) 디스크 영속성 통합 개발 계획.
- [MaterialShaderGraphIntegrationPlan.md](MaterialShaderGraphIntegrationPlan.md): 머티리얼-셰이더 그래프 통합, 원클릭 진입 및 실시간 3D 프리뷰(PBR Live Hot-Reload) 개발 계획.
- [ResourceEditorAsyncOpenPlan.md](ResourceEditorAsyncOpenPlan.md): 리소스 편집기 창 우선 표시, 백그라운드 문서 준비, 패널 공유 프레임 예산과 요청 수명 계획.
- [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md): 지형 데이터 저장·하이트맵 입출력·스플랫 레이어·팔레트·머티리얼 그래프 지형 도메인·지형 대응 내비게이션 계획.

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
  - **2026-11-23 추가 정적 정비 (`2489e74e`)**:
    - `Engine/CMakeLists.txt`에 `BAMENGINE_WITH_EDITOR=OFF` 시 `PUBLIC BAM_EDITOR=0` 명시 전파를 추가했다.
    - `SystemManager`, `AnimationSystem`, `GraphSystem`, `Runtime`, `SceneManager`의 `#ifdef BAM_EDITOR`를 `#if BAM_EDITOR`로 전환하고, `BAM_EDITOR=0`용 인라인 No-Op 스텁·`nullptr` 반환 계약 및 클래스 멤버 ABI 불변성을 정비했다(`ResourceManager.h/.cpp`는 동시 작업 회피를 위해 후속 이관 항목으로 유지).
    - `Logger.h/.cpp`에 `BAM_SHIPPING` 시 `Trace`/`Info` 컴파일 타임 제거·`std::source_location{}` 비노출·`stderr`/위치 생략, `!BAM_EDITOR` 콘솔 큐 비활성을 적용하고, `GraphExecutionCheck` 계약 검사를 추가했다.
  - **남은 항목**:
    - `ResourceManager.h/.cpp` `#ifdef BAM_EDITOR` 후속 이관.
    - 별도 `BAMENGINE_WITH_EDITOR=OFF` 및 `BAMENGINE_SHIPPING=ON` 구성의 Engine·Client·프로젝트 모듈 실빌드/ABI 검증.
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
- **2026-11-17 반영 (빌드·실행 시나리오 사용자 확인 완료)**: AnimatorGraphPanel 내부 New/Open/Close 확인은 이미 있다. 앱 종료 경로는 씬 문서만 확인하고 있었으므로, `ImGuiManager::RequestQuit`에 모든 리소스 편집기의 미저장 변경 확인 모달을 씬 확인보다 먼저 추가했다([ImplementationLog.md](ImplementationLog.md) 2026-11-17 항목). `GraphPanel`의 dirty 추적 부재는 잔여다.
- **2026-11-17 GraphPanel 반영 (CTest 38/38, 열기 확인 사용자 확인)**: `Engine::Graph` 수정 카운터와 `GraphDocument::SavedRevision` 비교로 GraphPanel 미저장 판정을 연결해 창 제목 `*`·종료 확인·다른 에셋 열기 차단에 포함시켰다. 메뉴와 Ctrl+O 열기가 같은 확인(`RequestOpenDocument`)을 거친다. 잔여: 그래프 노드 위치·주석 텍스트 미저장(저장 형식 확장 필요), 콘텐츠 브라우저 경로는 모달 없이 알림 차단(편집기 공통).
- **2026-11-17 그래프 저장 범위 확장 (사용자 확인)**: 노드 위치·주석 제목을 `GraphNode` 메타데이터로 저장하고, 2026-10-31 이전 파일이 `Variables` 필수 필드 때문에 열리지 않던 결함과 주석 그래프의 실행·셰이더 컴파일 실패를 해결했다([ImplementationLog.md](ImplementationLog.md) 2026-11-17 항목). (2026-11-18) `ImGuiManager::OpenPendingResourceEditor`에 `RunWithDiscardConfirmation`을 연결해 콘텐츠 브라우저 경로의 알림 차단을 폐지하고 대화형 확인 모달로 일원화 완료.
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
    1. 썸네일 표시가 끝난 뒤에도 지속되는 콘텐츠 브라우저 프레임 저하 계측. 축소 생성과 캐시 예산은 10.6절에 기록되어 있으며, [ImGui 계획 14-D](ImGuiDevelopmentPlan.md#콘텐츠-브라우저-썸네일-드로우콜-최적화)에서 ImGui 명령 분리와 아틀라스 필요성을 판단한다.
    2. 피커의 CWD 의존 스캔과 Shader 복합 확장자 누락, 모델 임포트의 머티리얼 저장 실패/재임포트 처리([점검 F02·F06·F12](CodeDocumentationAudit-2026-11-02.md)).
- 완료 기준: JSON/바이너리의 같은 콘텐츠 타입을 구분하고, 리소스 피커가 프레임 드랍 없이 해당 핸들 타입(`T`)과 호환되는 에셋만 디스크에서 즉각 선별·표시하며, 미로드 에셋 선택·검색 범위 전환·손상/미지원 처리가 동작한다.

### P1-E. 머티리얼 플래그 기반 텍스처 샘플링 분기 및 그래프 입력 유연화 [완료: 2026-11-22]

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
- **2026-11-22 구현 및 검증 완료**:
  - Shader Graph 범용 `Shader.TextureSample` 노드 멀티슬롯(`0..7`) 바인딩 지원 및 채널 분리(`rgba`/`rgb`/`r`/`g`/`b`/`a`) 출력 핀 추가 완료.
  - `MaterialInterface::SyncTextureBindings` 연동으로 그래프 노드의 텍스처 슬롯/경로와 머티리얼 인터페이스 실시간 동기화 완료 (`c58c2980`, `633b5e58`).
  - `MaterialInterface` / `MaterialInstance` 상수 버퍼 블롭 버전 캐싱(`m_ConstantsBlob`, `m_ConstantsBlobVersion`) 및 단일 순회 무할당(Zero-allocation) 파라미터 해석 구축 (`9b17e906`).
  - `Renderer` 드로우 패스 PSO-First 정렬로 파이프라인/루트 시그니처/섀도 바인딩 중복 전환 제거.
  - `ShaderGraphCheck` 단위 테스트 통과 완료.
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
  - **S3**: `InstanceData`, `LODGroupComponent`, `MeshRenderSystem` 다중 뷰포트/패스별 카메라 기준 화면 덮개 비율 + 히스테리시스 독립 LOD 선택(및 주 뷰포트 전용 상태 보존), `Renderer::RenderMeshes` 인스턴스 배칭(`DrawIndexedInstanced` 및 비인덱스 `DrawInstanced` 지원) 반영. **(2026-11-18 고도화)** 그림자 패스(`ShadowDepthPass`) 메시 우선 정렬 및 동일 메시 다중 머티리얼 단일 인스턴스 배치 병합 최적화, 1024개 분할 플러시 가드 추가, 메모리 레이아웃(80B/112B)·여인수 기저 노멀 역전치 외적 공식·배칭 시뮬레이션·LOD 히스테리시스 진동 억제 전용 단위 테스트 `MeshInstancingCheck` CTest 등록 완료.
  - **S4**: `tiledLightCullingCS.hlsl` 4방향 절두체 평면 컬링, `DirectX12Pipeline` Compute PSO, `DirectX12RHI::Dispatch`/`ResourceBarrierUAV`, `LightManager::DispatchLightCulling`, `lightingPS.hlsl` 선별 광원 루프 순회 반영 및 라이팅 GPU 시간 77% 단축(1.10 ms -> 0.25 ms) 실측 검증 완료.
  - **S5 및 후처리/HDR 파이프라인 일원화**:
    - `lightingPS.hlsl`의 반구형 Sky IBL 및 Split-Sum PBR 큐브맵 IBL(`gLightingData.HasIBLMaps`, `gIrradianceMap`, `gPrefilteredEnvMap`, `gBRDFLut`), 접촉 그림자, `ShadowStrength`와 `PostProcessPipeline`(`BloomEffect`, `PostProcessMaterialEffect` 기반 8종 효과) 및 `PostProcessPanel` 반영됨.
    - **(2026-11-18 고도화)**: Karis 2013 중요도 샘플링 기반 256x256 `R16G16_FLOAT` Split-Sum BRDF LUT 절차적 자동 생성 및 캐싱 파이프라인(`IBLManager`) 구축. 사용자 LUT 미지정 시 엔진 기본 LUT 자동 주입 및 `PrefilteredMap` 단독 지정 시 폴백 지원. 큐브맵 부재 씬에서도 `SkyAtmosphereComponent`의 `ZenithColor`, `SkyIntensity` 기반 반구형 물리 IBL 연동. 직사광 완전 차폐 시 칠흑 현상(Pitch Black)을 방지하고 알베도를 보존하는 `ShadowAmbientFloor` 앰비언트 하한 밸런싱 도입 및 `IBLShadingCheck` 단위 검증 통과.
    - **HDR 파이프라인 일원화**: `lightingPS.hlsl`의 내장 Reinhard/감마 보정 제거, `LightingColor` 버퍼 포맷 `R16G16B16A16_FLOAT` 승격을 통한 1.0 초과 물리 광량 보존, `PostProcessPipeline`의 ACES Filmic 톤매핑 + 감마 보정 상시 가동(PostProcessComponent 부재 시에도 Default ACES 자동 폴백)을 통해 실행/씬별 과노출 및 탈색 불일치 근본 해결.
    - **공통 디퍼드 파이프라인(`ViewportDeferredPipeline`) 통합**: `SceneViewportPanel`의 수백 줄에 달하던 자체 GBuffer/Shadow/PointShadow/Lighting/Forward 렌더패스 및 커맨드를 `ViewportDeferredPipeline`으로 단일화하여 Scene View, Game View, Material Preview 전반의 렌더링 일관성(스카이 대기, IBL, Directional/Point 그림자 100% 동등성) 확보.
  - **남은 항목**:
    1. ~~타일당 최대 광원 용량 초과(Overflow) 시 대체 동작 및 다중 광원/이동 카메라 경계 조건의 추가 검증 기록~~ -> **(2026-11-18 해소 완료)**: 4방향 절두체 평면/구면 교차 정합성, 255/256개 경계값 패킹, 256개 초과 오버플로 시 전역 순회 폴백의 Ground Truth 100% 조도 일치성, 1,024개 광원 스트레스 및 Directional/Sky 광원 전역 포함 규칙을 포괄 검증하는 전용 단위 테스트 `TiledLightingCheck` CTest 구축 완료.
- 완료 기준: 작은/큰 씬에서 고정 비용과 이득을 S1 계측 수치로 구분하고, 미지원·용량 초과의 정확한 대체 동작과 IBL/그림자 품질을 확인한다. (S3·S4·S5 단위 검증 전체 완결)

- [Render S6 1~4단계](RenderingDevelopmentPlan.md#s6-래스터-반투명-재질과-수면) **[1~4단계 구현, Editor 및 Client 검증 완료: 2026-11-18]** — 1단계 정적 반투명 메시 거리 정렬, 2단계 씬 컬러 복사 및 유리 굴절, 3단계 평평한 수면 셰이더 및 깊이 페이드, 4단계 세분화 격자 메시(`WaterGridMesh`, 64x64, 4,225 정점)와 4중 거스트너 파도 셰이더(`waveVS.hlsl`, `wavePS.hlsl`, `WaveMaterial.bammat.json`, 도함수 기반 분석적 법선/탄젠트 동기화, 파도 마루 Whitecap 거품, Depth Fade 굴절) 파이프라인을 구축하고, 스케일 확장 시 텍스처 스트레칭을 방지하는 월드 공간 XZ 정렬(World-Aligned UV)을 적용했다. Editor Game View와 독립 배포 Client(`Client.exe`) 환경에서 동일 카메라·해상도로 반투명/파도 렌더링 동등성을 검증 완료했다.

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

### P2-E. Client 시작·배포 — Windows 내장 패키징·실행 분리 및 `B01~B03`·`B05` 반영 / 실행 검증 미완료

- **2026-11-14 정적 반영**:
  - Windows 호스트(`Client/Source/Main.cpp`)와 플랫폼 중립 실행 계층 `ClientGameRuntime`(`Client/Source/Execution/Private/ClientGameRuntime.cpp`, 무조건 등록 시스템 19종 + `!UsesDeferredProfile()` 조건부 `LightSystem`), `ClientRenderPipeline`(Deferred 프로필 지원)이 분리되었다.
  - `Editor/Source/Build/Private/ClientPackage.cpp`가 에디터 내장 Windows Release 패키징 공식 경로(`Assets/`·`Engine/Resources/` 분리, `.bam-client-package`, `StartupResourceLoader::CollectPipelineFiles()` 검증, 스테이징·원자적 게시·`[Package inventory]`, `SDL_CleanupTLS()`)로 연결되었다.
  - `Client/Web/` 정적 프리뷰, `Tools/WebGPUProbe`, `Engine/CMakeLists.txt` Emscripten 경계 및 호스트 Tint WGSL sidecar 초안에 이어, `StartupResourceLoader`↔`Shader` WGSL 경로 연결(**B01**), `ShaderImporter`·`ShaderHotReloadManager` 경로 정규화(**B02**), `ClientGameRuntime`·`ClientRenderPipeline`·`Engine/CMakeLists.txt` Emscripten `NetworkSystem`/FMOD Wasm/RHI 셰이더 경계 정리(**B03**), 독립 Client `CrashHandler` 연결·활성 씬/진단 문구 분기 및 `PackageClient.ps1` 현행화(**B05**)까지 반영되었고 `B01~B03` 기준 `x64-debug` CTest 36/36 및 `PackageClient.ps1` 경로 차단 가드 4건(`.`, `Client`, `Assets`, `Utility`) 검증이 확인되었다. 단, Emscripten/WebGPU·Windows `x64-release`·독립 `Client.exe` 실 크래시 덤프 생성 검증은 미수행 상태다.

- **2026-11-13 정적 반영**: Engine Resource로 옮긴 공용 `StartupResourceLoader`를 Editor와 Client가 사용한다. Client는 시작 매니페스트를 씬 로드 전에 처리하며 빈 `StartupScenePath`는 실패한다. `.bam-client-package`가 표시된 실행에서 `Config/Project.json`이 없으면 실패하고 개발 실행은 기본값 경로를 유지한다. 패키저는 Windows D3D12용 시작 파이프라인의 설정·직접 참조 파일을 검증해 staging 및 인벤토리에 반영한다. 빌드·패키지 실행 검증은 수행하지 않았다.
- **2026-11-01 현황**:
  - `Tools/Scripts/PackageClient.ps1` 기본 복사 스크립트 초안이 추가되었고 당시 `Client/Source/Main.cpp`에 `LoadGameModule()`, `--rl` 헤드리스 강화학습 서버, `InitializeSystems()` 기본 12종 등록이 반영되었다(2026-11-14 작업 트리에서는 `ClientGameRuntime::InitializeSystems()`로 이관되어 무조건 19종 + `!UsesDeferredProfile()` 조건부 `LightSystem`을 등록한다).
  - **2026-11-02 정적 갱신**: 실행 파일 기반 루트 탐색·Runtime 경로 주입·기본 메시 등록·설정 로드·`StartupScenePath` 선택·카메라 등록이 있다.
  - **2026-11-14 기준 잔여**: `B01~B05` 정적·스크립트 단절은 모두 코드·스크립트 반영(`B01~B03` CTest 36/36 및 `PackageClient.ps1` 경로 차단 가드 4건 확인)으로 해소되었고, Emscripten/Tint/WebGPU 실 툴체인 빌드·실행 검증, `B05` 실 크래시 덤프 생성·정상 스테이징 실행 확인, `RenderPipelineProfilePlan` 2·3단계(Game View vs 외부 패키지 비교, `SDL_CleanupTLS()` 누수 해소) 및 `x64-release`·패키지 외부 독립 실행 검증이 남아 있다.
  - 남은 배포 범위는 `ClientPackage.cpp`의 `StartupResourceLoader::CollectPipelineFiles()` 기반 직접 참조 수집을 동적 참조 분석·선택적 프루닝·다중 플랫폼(Emscripten/Wasm) 대상으로 확장하고, `PackageClient.ps1`(`B05` 현행화 완료)의 정상 스테이징 및 패키지 외부 독립 실행 검증을 마치는 것이다(이전 전체 스캔 구조 기준은 [점검 F04~F05](CodeDocumentationAudit-2026-11-02.md)).
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
- [Render S6 2~4단계](RenderingDevelopmentPlan.md#s6-래스터-반투명-재질과-수면) **[계획]** — 기본 반투명 합성 확인 후 콘텐츠 요구에 따라 얇은 굴절 유리, 평평한 물, 메시가 움직이는 파도를 차례로 검증한다.
- Render M3: 기능 지원·자원 계약·계측을 갖춘 뒤 DXR 그림자→반사를 독립 경로로 진행한다.
- Render M4: IBL 합성 기준과 광선 조회 기반 이후 제한된 공간의 프로브 GI를 검증한다. **[1~4단계 코드 반영·화면 확인 (2026-11-23), 성능·메모리 측정 대기]** [상세](RenderingDevelopmentPlan.md#m4-프로브-기반-동적-gi)
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
- Render S1/S2는 P1, S3/S4/S5와 S6 1단계는 P2, S6 2~4단계와 M1~M4는 조건부 P3, M5·L 계열은 P4다.
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
  - ASM `isLooping` 제어, `Stop()` 시간 리셋 및 `controllerPath` 상대 경로 정규화 완료. 단, 인스펙터 찾아보기는 2026-11-18까지 절대 경로를 저장했다. 현재 `controllerPath`와 `ActionTimelineComponent::timelinePath`는 경로 피커(`DrawInspectorAssetPathPicker`)로 고르며 가상 URI로 저장한다([구현 기록](ImplementationLog.md)). `ActionTimelineComponent`의 `timelineAsset` 핸들을 `NOSERIALIZE` 런타임 캐시로 전환하고 인스펙터를 `timelinePath`로 일원화하여 이중 참조를 해소했다.
- **기존 Client의 실패 보고 — P2-E / F05 일부 [정책 반영 / 실행 검증 대기]**:
  - `ConfigManager::Load` 반환 결과와 `SchemaVersion`을 검사한다. 개발 실행은 설정 파일 부재 시 경고·기본값을 사용하고 파싱 실패 시 종료한다.
  - A08 정책에 따라 패키지 마커가 있으면 `Project.json` 누락으로 실패한다. 빈 `StartupScenePath`도 `InvalidArgument`로 실패하며 실행 검증은 남아 있다.

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
  - `AssetReferenceValidator` 정적 검증 코어 구현: `Resources/` 내 전체 씬/프리팹/머티리얼/모델 JSON 대상 누락(`MissingFile`), 외부 절대 경로(`ExternalAbsolutePath`), 확장자 오류(`InvalidExtension`) 전수 검출 및 구조 기반 자동 복구(`FixIssue`, `FixAllSafeIssues`) 지원. 기록 전 문서 잠금·원문 검증·백업을 사용한다.
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
- **텍스처 밉맵 체인 및 GPU 블록 압축 파이프라인 — P3 / S2-D ([RenderingDevelopmentPlan.md](RenderingDevelopmentPlan.md)) [1·2·3단계 완결: 2026-11-18]**:
  - 1단계(런타임): `stb_image_resize2` 기반 CPU 밉맵 체인 생성 및 RHI 다중 서브리소스 순회 업로드 (2026-11-08).
  - 2단계(블록 압축 및 컨테이너): `RHITextureTypes.h` 헬퍼 함수, `DirectX12RHI::UploadTextureData`의 4x4 블록 압축 포맷(BC1~BC7) 서브리소스 업로드 지원, 고속 인메모리 `DDSLoader` 파서 구축, `Texture` 및 `ResourceManager`에 `L".dds"` 확장자 등록, `TextureMipmapCheck` 9레벨 Mipmap BC7 검증 완결 (2026-11-18).
  - 3단계(오프라인 베이킹): `TextureImporter`에서 `kQuickTextureSanityMode` 제거, 노멀 맵(`BC5_RG_UNORM`) 및 베이스 컬러(`BC7_RGBA_UNORM_SRGB`) 스마트 기본값 감지, 에디터 임포트 시 `.bamtex` + 표준 `.dds` 동시 베이킹, `DirectXTexCompressor` sRGB 포맷 정밀 동기화, `ModelImporter` 내장/외부 텍스처 자동 베이킹 연계(`BakeTexturesToDDS`), `AssetCache`의 DDS 및 원본 3D 모델(FBX/GLTF/OBJ) 썸네일 지원, `.bamtex` 핫리로드 연동 및 `CMakePresets.json` CTest 프리셋 완결 (2026-11-18).
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

## 11. 현재 작업 트리 기준 우선순위 재평가 (2026-11-14 갱신)

근거는 [2026-11-14 점검 B01~B06](CodeDocumentationAudit-2026-11-14.md), [2026-11-06 점검 A01~A10](CodeDocumentationAudit-2026-11-06.md) 및 2026-11-07~14 반영 현황(트랙 A~J 및 현재 미커밋 4갈래)이다. 10절의 과거 완료 기록보다 이 절의 잔여 범위와 작업 순서를 우선한다.

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

- **다수 광원 지원 확장 — A06 본체 / F10 [해소 완료: 2026-11-15]**: `LightManager`의 씬 광원 상한을 1024개(`MAX_LIGHTS`)로 확장하고, `tiledLightCullingCS.hlsl`의 실제 누적 카운트(`s_TileLightCount`) 보존 및 `lightingPS.hlsl`의 256개 초과 타일 안전 전체 순회 폴백(`useTiled = false; loopCount = numLights;`)을 적용하여 조도 누락을 해소했다(`x64-debug` 빌드 및 CTest 36/36 통과 확인).
- **모델 재임포트** — 기존 머티리얼 보존, 원본 추적, 생성물 하위 폴더 분류를 반영하고 도끼·활·Fox의 사용자 확인 결과를 기록했다. 모델 전용 폴더와 원본 보관 자동화, 기존 에셋 이관은 후속이다. `Overwrite`·`SaveToFile` 조합과 중간 저장·부모 머티리얼 실패의 복구는 별도로 검증한다. [모델 임포트 계획](InteractiveModelImportPreviewPlan.md#phase-6-하이브리드-모델-에셋-정리-파이프라인-hybrid-source-to-asset-pipeline)
- **클라이언트 빌드 시 에디터·디버그 코드 스트립 및 안전 실행 보장 (No-Op 스텁화 / ABI 보호)**: 엔진 내 에디터 및 디버깅 전용 함수를 식별하고, 클라이언트 DLL/바이너리 빌드 시 전처리기로 완전히 제거(Strip)하되 링크 오류 및 널 크래시를 방지하기 위해 No-Op 인라인 스텁 및 안전 매크로를 적용한다. 코어 데이터 구조의 ABI 불변성과 호출부 널 안전성을 보장한다. (참조: [BuildConfigurationAndPreprocessing.md](BuildConfigurationAndPreprocessing.md) 3.1절)
- **비정상 종료 진단(크래시 리포터) 및 에디터 작업 복원 시스템 (Crash Recovery)**: Windows SEH 및 미니덤프(`MiniDumpWriteDump`) 연동, 블랙박스 로그 플러시, 구조화된 크래시 리포트(`.crash.json`), 5분 주기 하트비트 자동 저장(Heartbeat Auto-Save) 및 차기 실행 시 복원 마법사 구축. (참조: [CrashHandlingAndRecoveryPlan.md](CrashHandlingAndRecoveryPlan.md))
- **2D 제작 예제**: 로그에 남은 구형 sprite/texture 로드 실패의 현재 재현 여부를 먼저 확인한다. 현행 로더 문제와 오래된 샘플 데이터 문제를 분리한 뒤 Engine 14절 L의 Simple/Filled 또는 기본 TileMap 중 필요한 작은 예제로 진행한다.
- **기하 와인딩 CW 표준화 — [AssetPipelineAndRenderingOptimizationPlan.md](AssetPipelineAndRenderingOptimizationPlan.md) 조치 1 [완료: 2026-11-16]**: 내장 메시·머티리얼 JSON·`MaterialInstance` FrontFace 상속·RHI 기본값을 CW로 통일하고 `BuiltInMeshWindingCheck`를 신설했다(`x64-debug` CTest 38/38 및 렌더링 실행 확인). `SplineTestStrip` 확인, 기본 머티리얼(`DefaultMaterial`) 재설계, 조치 2~4는 잔여다.
- **에셋 썸네일 및 통합 디스크 캐시 인프라 구축 — [DiskCacheDevelopmentPlan.md](DiskCacheDevelopmentPlan.md) [Phase 1~4 구현 및 검증 완료: 2026-11-17]**: 에디터 재시작 시 콘텐츠 브라우저 3D 렌더링 및 2D 4K 이미지 재디코딩으로 인한 스터터링(프레임 드랍)을 제거하기 위해 128x128 썸네일 디스크 캐시(`.bam/cache/thumbnails/`)를 최우선(P0) 구축하고(Phase 1), 셰이더 컴파일 바이너리 타임스탬프 캐시(`dxc.exe` 스킵) 및 D3D12 `ID3D12PipelineLibrary` 기반 PSO 디스크 캐시(`.bam/cache/shaders/pipeline.library`, `pipeline.names`)를 완성했으며(Phase 2), 텍스처(DirectXTex BCn 압축 스킵) 및 모델(Assimp/LOD 파싱 스킵) 에셋 파생 데이터 캐시(`.bam/cache/ddc/`, Phase 3)를 구축했다. 이어 지형 그리드 메시 스트림 캐시(`.bam/cache/terrain/`, Phase 4), 모델/텍스처 DDC 공통 유틸리티(`DdcCommon.h`), 프로젝트 루트 오인식 방지 인프라 및 에디터 환경 설정 내 통합 캐시 모니터링/일괄 비우기 UI를 완성하여 전체 디스크 캐시 라이프사이클을 완결했다.

### 11.3 안정화 다음 기능 선택 (차기 5개 트랙)

- **트랙 A (렌더링 품질 — S2-D) [완료: 2026-11-08]**: 텍스처 밉맵(Mipmap) 체인 생성 및 RHI 다중 서브리소스 업로드 (`stb_image_resize2` 기반 CPU 밉맵 체인 생성, `DirectX12RHI` 다중 서브리소스 업로드, 삼선형 필터링 연동 및 `TextureMipmapCheck` 단위 테스트 완결).
- **트랙 B (모델 파이프라인 — InteractiveModelImport Phase 5) [완료: 2026-11-07]**: `meshoptimizer`(`meshopt_simplify`) 기반 자동 메시 LOD 생성 파이프라인 및 `ModelEditor` 실시간 LOD 검사기 완결.
- **트랙 C (에셋 아키텍처 — ResourceAndAssetSeparation Phase 1~3-B) [완료: 2026-11-08]**: 가상 마운트 URI(`Engine://`, `Editor://`, `Project://`) 해석 계층 구축, 빌트인 리소스 보강, 에디터 로캘 분리 및 `Assets/` 물리 폴더 완전 이관 완결. (※ Phase 4 배포 패키징 연동은 2026-11-08 사용자 결정에 따라 핵심 기능 구현 완료 후 최종 배포 단계로 최대한 뒤로 보류)
- **트랙 D (2D 제작 워크플로 — [SpriteAnd2DSystemPlan.md](SpriteAnd2DSystemPlan.md)) [구현 완료: 2026-11-15]**:
  - Phase 1 [완료]: 스프라이트 에디터(`SpriteEditor`) 인터랙티브 시트 3대 슬라이서(Grid by Cell Size, Cell Count, Auto Alpha Connected Components) 및 원클릭 액션 애니메이션 클립(`.spriteanim`) 일괄 생성 위저드 완성.
  - Phase 2: 스프라이트 렌더링 모드(Simple, Sliced 9-Slice, Tiled, Filled)와 2D 정렬 필드가 있다. 정렬 필드의 렌더 제출 연결은 Phase 5에서 다룬다.
  - Phase 3 [완료]: 픽셀 퍼펙트 카메라(`PixelPerfectCameraComponent`), 서브픽셀 카메라 스냅(`snapSubpixel`), 가상 레퍼런스 해상도 기준 정수 줌 배율 자동 계산 및 OrthographicSize 완벽 동기화, `GameViewportPanel` 연동 및 `PixelPerfectCameraCheck` 단위 테스트 완성.
  - Phase 4 [완료]: 2D 타일맵 시스템(`TilemapComponent`), 16x16 청크 분할 및 직렬화/복원, `TilemapRenderSystem` 및 머티리얼/메시 fallback, `TilePalettePanel`(B, E, U, I 브러시 툴바), `SceneViewportPanel` 인터랙티브 실시간 타일 페인팅 및 `TilemapCheck` 단위 테스트 완성.
  - [Phase 5](SpriteAnd2DSystemPlan.md#phase-5-스프라이트-아틀라스-및-드로우콜-배칭) [구현 및 검증 완료: 2026-11-15]: MaxRects 패킹, 런타임 스프라이트 인스턴싱 드로우콜 배칭(SpriteInstanceData, RenderSprite 루프, DrawIndexedInstanced 1회 제출), Unlit 머티리얼·Fallback 연결, 패키징 에셋 검증 및 독립 클라이언트 정상 종료 검증 완료 (CTest 36/36 통과).
- **트랙 E (모듈 및 플러그인 GUI 제어 파이프라인 — PluginManagerPanel, [ImGuiDevelopmentPlan 26절](ImGuiDevelopmentPlan.md#26-플러그인-및-모듈-관리-패널pluginmanagerpanel-구현-완료)) [구현 완료]**:
  - Phase 1 [완료]: `PluginManagerPanel` 에디터 패널 신설 (`Editor/Source/ImGui/PluginManager/Public/PluginManagerPanel.h`, `Private/PluginManagerPanel.cpp`).
  - Phase 2 [완료]: 마스터-디테일 2열 레이아웃(좌측 270px 목록 + 필터 + 상태 뱃지, 우측 상세 인스펙터 및 제어 툴바) 구축.
  - Phase 3 [완료]: `HotReloadManager` 4대 엔진 매니저(`ModuleHotReloadManager`, `ModuleBuildManager`, `ShaderHotReloadManager`, `AssetHotReloadManager`) 및 에디터 패널 플러그인 연동 (`Reload`, `Rebuild & Reload`, `Load / Unload`, `Auto-Reload`).
  - Phase 4 [완료]: 비동기 컴파일 콘솔 전문 스트리밍, 에러/경고 하이라이트 및 `[Retry Compile]` 에러 복구 파이프라인, 메뉴바(`Edit -> Plugins`, `Window -> Panels -> Diagnostics -> Plugin Manager`) 및 로캘 키 40여 종 연동 완료.
  - 공용 패널과 Builtin·Custom 수동 활성화, V2 패널 템플릿 생성은 코드에 반영했다. 생성 패널의 독립 빌드·설치형 활성화는 [실행 확인](EditorPluginArchitecturePlan.md#사용자-실행-검증)이 필요하다. DLL이 없는 사용자 소스 패키지의 관리자 내 구성·빌드·게시·로드는 [별도 계획](EditorPluginArchitecturePlan.md#사용자-소스-패키지-빌드로드-계획)이며 미구현이다. Client 작업은 빌드·패키징 단계에서 재개한다.
- **트랙 F (계층형 통합 비주얼 이펙트 시스템 — [CompositeVisualEffectPlan.md](CompositeVisualEffectPlan.md)) [Phase 1~4 완료 / Phase 5 Step 1~3·블렌드 패스 교정·Air Mode 반영 / Step 4 통합·검증 잔여: 2026-11-20]**:
  - 배경: 기존 파티클(입자) 중심 시스템에서 스프라이트 플립북, 메쉬 이펙트(마법진, 충격파 링, UV 스크롤), 순간 조명 섬광, 사운드, 트레일, 데칼, 카메라 셰이크를 타임라인 시간축으로 오케스트레이션하는 계층형 통합 아키텍처 채택.
  - Phase 1 [완료]: `CompositeEffect` 에셋(JSON 아카이브), `CompositeVFXComponent` 및 `CompositeVFXSystem` 런타임 오케스트레이터, 스프라이트 플립북·파티클·라이트 섬광 연동, `VFXEditorPanel` 저작 도구 구축 완료.
  - Phase 2 [완료]: 메쉬 이펙트(`VFXMeshTrack`) 런타임 렌더링 제출(`ForwardTransparentPass`), 스케일/회전 보간 및 UV 스크롤/컬러 페이드 연동(`SpriteDrawCommand` / `SpriteUBO`), 사운드 트랙(`VFXSoundTrack`) 1회성 정밀 타이밍 트리거 연결 완료.
  - Phase 3 [완료]: 트레일 / 리본(`VFXTrailTrack`) 정점 히스토리 수집과 테이퍼링. 2026-11-17 동적 정점 버퍼 기반 연속 Triangle Strip으로 전환(곡선 이음새 해소), 정렬 모드(A-B 축/카메라 정면) 추가. SDL GPU 대체 경로는 미검증.
  - Phase 4 [완료]: 지면 투영 데칼(`VFXDecalTrack`) 알파 페이드아웃 투영 및 다축 사인파 감쇠 카메라 셰이크(`VFXCameraShakeTrack`), `VFXEditorPanel` 트랙 인스펙터 편집 전면 지원 완료.
  - 후보 B 실환경 검증 & 안정화 [완료: 2026-11-15]: `acceptsSceneSubmissions` 가드를 통한 메인 씬 스카이박스 누출 차단, 타임라인 앞뒤 여백(-0.25s/+1.0s) 및 10배 줌아웃(0.1x)·패닝 확장, 트레일 검증용 프리뷰 모션(원형 궤도) 토글, D3D12 `MISMATCHINGCLEARVALUE` 예외 중단 해제 및 RTV null 가드 크래시 해결, 3단계 배경 프리셋(스튜디오 18% 회색/암실/주간) 및 스튜디오 조명 배율 연동, 차콜 PBR 바닥 슬래브(18% 반사율 매트 머티리얼 인스턴스) 적용으로 바닥 자체 블룸 제거 및 플래시 반사 극대화 완료.
  - Phase 5 [진행 중 / Step 1~3·패스 교정·Air Mode 반영, Step 4 통합·검증 잔여: 2026-11-20]: Step 1(2026-11-16) 3D 기즈모(`GizmoHelper`), 환경 장애물 4종·바람/중력 외력, 48m `PlaneMesh`, 15cm Depth Fade·프레넬 림 코드 반영(기즈모·외력·Depth Fade 실환경 실행 기록 미비). Step 2(2026-11-17) `FloatCurve` (2D 3차 베지어 에르미트 스플라인), Mesh/Light 커브 샘플링, `EffectTimelineView` 인라인 커브 프리뷰 완료. Step 3(2026-11-19) `CurveEditorModal` 2D 베지어 커브 팝업 모달 및 `VFXEditorPanel` 연동 완료. 2026-11-19 추가 반영: `CompositeVFXSystem` Geometry/ForwardTransparent 블렌드 필터 교정(허공 흙갈색 탁화 원인 해소, 프리뷰·씬 확인 완료), `VFXEditorPanel::ToggleAirMode()` 공중 프리뷰 토글, `ActionTimelineSystem`/`AnimationTimelinePanel`의 `SpawnCompositeVFX` 본 소켓 스폰·추적(`SocketAttachmentComponent`)·지속형 해제(`ActionVFXHoldComponent`)·`ActionTimelineEvents` 기본 구현. 잔여(Step 4 및 [AnimationPipelineIntegrationPlan.md](AnimationPipelineIntegrationPlan.md) 2~5절): 편집 씬 격리, 컴포넌트 제거 시 Animator 복원, 프리뷰↔런타임 Notify/소켓 평가 순서 단일화, `m_ModelPath` 가상 URI 정규화, PIE·독립 Client 대표 액션 통합 검증, 선택적 순수 Additive(`One/One`) 분리 검토.
- **트랙 G (비정상 종료 진단 및 에디터 작업 복원 시스템 — [CrashHandlingAndRecoveryPlan.md](CrashHandlingAndRecoveryPlan.md)) [Phase 1~4 구현 및 실환경 검증 완료: 2026-11-15]**:
  - Phase 1 [완료]: Windows SEH / CRT 예외 가로채기 핸들러(`CrashHandler`), `MiniDumpWriteDump` 기반 `.dmp` 미니덤프 생성, 크래시 순간 `Logger::Flush()` 강제 디스크 동기화 파이프라인 구축.
  - Phase 2 [완료]: `StackWalk64` / `SymFromAddr` 기반 심볼릭 콜스택 추출 및 타임스탬프 기반 구조화된 `.crash.json` 진단 리포트 자동 생성.
  - Phase 3 [완료]: Tier 1 하트비트 주기적 자동 저장(`RecoveryManager`), 씬 dirty 상태 감지 시 5분 주기 `.tmp` 임시 파일 안전 교체(원자적 쓰기) 스냅샷 생성.
  - Phase 4 [실환경 검증 완료: 2026-11-15]: `Temp/Session.lock` 세션 라이프사이클 관리, 에디터 시작 시 비정상 종료 감지 및 작업 복원 마법사 모달(`DrawRecoveryModal`) 연동, `Help -> Trigger Crash (Diagnostics Test)` 메뉴를 통한 의도적 크래시 및 미니덤프/진단 리포트/비상 스냅샷 생성, `Escape` 방어 및 무손실 씬 복원 검증 완료.
  - 저장 경로 일원화 [완료: 2026-11-15]: 개발 환경과 배포 패키지 환경을 모두 만족하도록 `PlatformPath::GetProjectRoot()` 기준으로 `Logs/`(로그·크래시 덤프)와 `Temp/`(세션 락·복구 스냅샷) 경로 일원화 및 `"Host": "Editor"` 컨텍스트 등록 완료.

- **트랙 H — 에디터 공용 UI·3줄 리소스 편집기·콘텐츠 브라우저 통합 [정적 완료 / UI·GPU 미검증]**
  - `ResourceEditorInterface` 기반 3줄 공통 헤더(12개 리소스 편집기 전환), `EditorUI` 공통 검색 바·모달 버튼·반응형 툴바·테이블 확대, `ContentBrowser`의 `Assets/`/`Engine/Resources/` 분리 및 읽기 전용 보호·썸네일 재사용·다중 드롭 반영 완료.
- **트랙 I — Render Pipeline Profile·Windows 내장 패키징·Client 실행 분리 [정적 반영 / 미커밋 포함 / 실행·누수 미검증]**
  - `Deferred.bamrenderpipeline.json`, `RenderPipelineProfile`, `ClientRenderPipeline`, `Editor/Source/Build/Private/ClientPackage.cpp`, `StartupResourceLoader` Engine 공용화, `ClientGameRuntime` 분리(무조건 19종 + 조건부 `LightSystem`), 빌드 워커 `SDL_CleanupTLS()`, 독립 Client `CrashHandler` 연결·진단 문구 분기 및 `PackageClient.ps1` 현행화(**B05** 해소) 반영 완료. 실 크래시 덤프 생성·패키지 외부 실행/누수 검증은 잔여 범위다.
- **트랙 J — WebGPU 씬 프리뷰 및 Emscripten/WGSL 이식 경계 [`B01~B03` 코드 반영 완료 / 툴체인 실행 미검증]**
  - `Client/Web/` 정적 프리뷰, `Tools/WebGPUProbe`, `Engine/CMakeLists.txt` `EMSCRIPTEN` 경계, 호스트 `Tint` WGSL sidecar, `ThirdParty/FMOD` 2.03.15 갱신과 함께 `B01`(`StartupResourceLoader`↔`Shader` WGSL 경로 및 `SceneArchiveCheck` 라운드트립)·`B02`(`ShaderImporter`/`ShaderHotReloadManager` 경로 정규화)·`B03`(`NetworkSystem` Emscripten 가드, FMOD Wasm 링크, `ClientRenderPipeline` RHI 경로) 반영이 완료되었고 `x64-debug` 빌드 및 CTest 36/36 통과를 확인했다. Emscripten SDK/`Tint` 실 툴체인 빌드·실행 검증은 미수행이다.

### 11.4 현행 작업 트리 정리 및 단기 후속 순서 (`B01~B05`)

1. **작업 트리 정리 완료 (`B04`) 및 Windows 기준선 후속 검증**: 미커밋 변경분의 5개 논리 커밋 분리(`1a798d3d`, `5083be3c`, `e5cd4e4b`, `c426a4c2`, `13f1c821`) 및 `x64-debug` 멀티코어 빌드·CTest 36/36 통과(2026-11-14)는 완료했다. `x64-release` 빌드 및 `RenderPipelineProfilePlan` 2·3단계 수동 비교·FMOD 오디오 재생 확인이 남아 있다.
2. **WebGPU/Emscripten 이식 선결 해소 (`B01` → `B02` → `B03` 코드 반영 완료 / 툴체인 검증 대기)**: `StartupResourceLoader`·`SceneArchiveCheck` WGSL 경로 연결 및 직렬화 검증(`B01`), `ShaderImporter`·`ShaderHotReloadManager` 경로 정규화(`B02`), `Systems.h`/`ClientGameRuntime`/`Engine/CMakeLists.txt`/`ClientRenderPipeline`의 Emscripten `NetworkSystem`/FMOD Wasm/RHI 셰이더 경계 정리(`B03`)를 반영하고 Windows `x64-debug` 빌드·CTest 36/36 통과를 확인했다. 다음 단계로 호스트 `ReflectionGenerator` + `Tint` 기반 `Tools/WebGPUProbe` 및 Emscripten 정적 빌드·실행을 검증한다.
3. **안정성·후속 기능 마무리 (`B05`·`A06 본체`·`2D Phase 5` 반영 및 패키지·종료 수명주기 안정화 완료)**: `Client` `CrashHandler` 연결 및 `PackageClient.ps1` 현행화(`B05`), 타일 컬링 257개 초과 안전 전체 순회 폴백 및 `MAX_LIGHTS` 1024 확장(`A06 본체`), 2D Phase 5 스프라이트 인스턴싱 드로우콜 배칭 및 Unlit 머티리얼·Fallback 연결, 패키징 에셋 검증 픽스, `Runtime::Free` 종료 수명주기 역전 및 `PickingManager` 지연 소멸 크래시 해결을 완료했다(`x64-debug` CTest 36/36 및 독립 클라이언트 정상 종료 확인).

렌더링 작업을 선택할 때는 1번의 Game View와 Windows 배포 Client 비교를 먼저 한다. 래스터 표현을 확장한다면 [Render S6](RenderingDevelopmentPlan.md#s6-래스터-반투명-재질과-수면)의 반투명 합성부터 시작한다. 이 작업의 선행 조건은 기존 래스터 경로에서 충족하며 DXR(M3)과 패스 트레이싱(L4)은 각 계획의 도입 조건을 따른다.

### 11.5 상태·검증 기록 규칙

- 2026-11-07 기준 CMake Debug 빌드 및 CTest 전체 29개(29/29 Passed) 통과 확인 완료; 2026-11-14 `spirv_cross_*` 순서 수정 후 `x64-debug` CTest 36/36 통과, `B01~B03` 코드 반영 후 CTest 36/36 통과, `B05` 가드 4건 검증 완료, 2026-11-15 `A06 본체` 타일 컬링 초과 폴백 및 `MAX_LIGHTS 1024` 반영 후 `x64-debug` 빌드 및 CTest 36/36 통과 확인 완료; 2026-11-15 2D Phase 5 스프라이트 인스턴싱 드로우콜 배칭 및 Unlit 머티리얼·Fallback, 패키징 에셋 검증 픽스, 런타임 종료 수명주기 크래시 해결 후 `x64-debug` CTest 36/36 통과 및 독립 패키지 클라이언트 정상 종료 확인 완료; 2026-11-15 에디터 비정상 종료 진단(CrashHandler/미니덤프/`.crash.json`), 2단계 작업 복원(RecoveryManager/세션 락/복원 모달/무손실 씬 복원) 실환경 검증 및 프로젝트 루트 기준 경로 일원화 완료; 2026-11-15 복합 비주얼 이펙트(후보 B) 뷰포트 스카이박스 누출 차단, 타임라인 0.1x 줌아웃/패닝, 프리뷰 모션, D3D12 클리어 예외 해결, 3단계 배경 프리셋 및 차콜 PBR 바닥 슬래브(블룸 포화 제거 & 플래시 반사 극대화) 실환경 검증 완료(`x64-release`·실 오디오/UI/GPU 수동 비교·Emscripten/Tint 실 툴체인 및 Phase 5 환경 고도화는 미검증).
- 2026-11-19 `x64-ninja-debug`(Ninja 단일 구성) 모두 빌드 후 CTest 44개 전체 통과를 사용자가 확인했다.
- 2026-11-20 `fcda4887`~`c2058594` 커밋(프리팹 Revert All 안전성 강화, D3D12 파이프라인 생성 안정화, 머티리얼 에디터 완전 통합/3분할 구축, 설계 문서화 2건) 및 `SceneArchiveCheck` 핫픽스(`AddSpatialEntity`를 통한 트랜스폼 컴포넌트 보장) 후 `x64-ninja-debug` 모두 빌드 및 CTest 44개 전체 통과(`44/44 Passed`)를 사용자가 확인했다.
- 2026-11-22 병렬 기능 스프린트(`a388afbe`~`61f4594f`: 셰이더 그래프 핀/노드 확장, 게임플레이 컴포넌트/노드, `BAM_SHIPPING` 가드·`RuntimeSystemManifest`, 프리팹 내부 `entt::entity` 리맵/계약, ECS 게임 UI 확장, 애니메이션 IK/Root Motion/PIE 동기화, 2D GPU 배칭·아틀라스·`SpatialBroadphase`·씬 `Sweep`/`OverlapAll`, `AsyncResourceOpenService`·DCC 프리셋 영속화, `Plugin://` 가상 URI·CMake 소스 빌드/게시, 프로젝트 로캘 카탈로그 및 UI 텍스트 폰트·번역 토글) 및 회귀 수정(`dbdaa188`, `e0eebb53`) 후 `x64-ninja-debug` 전체 빌드 및 전체 CTest 통과를 사용자가 확인했다. (`x64-release`, `BAMENGINE_WITH_EDITOR=OFF` 독립 Client, PIE·실환경 UI/GPU 수동 검증은 후속 검증 범위.)
- 2026-11-22 티어 1 스프린트 A & B, 후보 그룹 A & B-2, 신규 6대 핵심 엔진 시스템(스프린트 C) 전체 구현 및 단위 테스트(12개 CTest 타깃 전원 통과)를 사용자가 확인했다:
  - `UIInteractionCheck` (ECS UI 런타임 렌더링, 열린 드롭다운 팝업 HitTest/배경 차단, TextInput 런타임 키/텍스트/IME 라우팅)
  - `ShaderGraphCheck` (범용 `Shader.TextureSample` 0..7 슬롯, 채널 분리, `SyncTextureBindings`, B-2 상수 버퍼 블롭 캐싱·무할당 해석 및 렌더러 PSO 우선 정렬)
  - `VirtualPathCheck` (`ModelImportModal` 60FPS 실시간 프리뷰 트랜스폼 갱신 및 Phase 6 `ModelSubfolderHelper` 서브폴더 구성)
  - `ActionTimelinePlaybackCheck` (`GameplayCombatSystem` 공격 스윙 중복 피격 방지/넉백 및 `ActionTimelineSystem` `"HitboxOn"`/`"HitboxOff"` 노티파이 연동)
  - `BlendTree2DCheck` (`AnimationIKComponent` Foot Ground IK 지면 레이캐스트 접지·골반 보정 및 비주얼 스크립팅 IK 노드)
  - `KinematicCharacterCheck` (KCC 체공 낙하 조기 스냅 억제, 30도 경사로 법선 분리 오프셋, 경사→평지 연착륙 `IsSoftLanding` 클램핑)
  - `PakArchiveCheck` (`.bampak` 아카이브 포맷, 결정적 `PakWriter`, 무결성 검증 `PakReader`, `PakMountManager`)
  - `SaveGameCheck` (`SaveGameManager` 원자적 슬롯 저장, 메타데이터 조회, 체크포인트 캡처/복원)
  - `NavigationAICheck` (`NavGrid` 8방향 A* 코너 컷 방지·직선화 스무딩, `NavAgentComponent`, `AIPerceptionComponent`, `NavigationSystem`)
  - `AudioMixerCheck` (`AudioMixer` 5대 버스 유효 게인·페이드·3D 거리 감쇠·FMOD 채널 그룹 동기화)
  - `NetworkRpcCheck` (`NetworkRpc` Server/Client/Multicast RPC, Dead Reckoning 스냅샷 외삽, 지연/손실 시뮬레이터)
  - `SceneStreamingCheck` (`ScenePersistenceComponent` `DontDestroyOnLoad`, Additive 서브씬 스트리밍, `AsyncResourceLoader`)
- 2026-11-23 스프린트 D · 그룹 1 · 그룹 A(1~3) · 그룹 B-4 (`2b2ffbf9`..`c18c8ea2`) 및 렌더링·리소스 스트리밍(`57fafba5`..`a7547fb7`) 구현과 단위 체크 확장까지 반영되었으나, **사용자 실행에서 `RLSimulationCheck` 실패(5초)가 보고되어 상세 로그 확인 및 수정이 필요**하며 `GraphReflectionCheck`, `GraphExecutionCheck`, `SceneArchiveCheck`/`SceneReflectionCheck`, `BAMENGINE_WITH_EDITOR=OFF` + `BAMENGINE_SHIPPING=ON` 독립 Client 빌드 및 UI/PIE 수동 확인은 사용자 실행 검증 대기 상태다([ImplementationLog](ImplementationLog.md) 2026-11-23 항목).
- 기준 커밋, 미커밋 범위, 구성, 검사 이름과 실제 실행 개수, 실패 시나리오, UI/GPU 미검증을 기록한다.
- 소스 전면 이동·싱글턴 전면 교체·모든 문서의 장기 목표 구현을 완료 조건으로 추가하지 않는다.

### 11.6 차기 우선순위 개발 후보 로드맵 (2026-11-23 갱신)

2026-11-22~11-23 후속 집중 스프린트를 통해 다음 영역들이 `main`에 구현·병합되었다:
1. **티어 1 스프린트 A & B**: ECS UI 위젯 런타임 렌더링·프리셋 및 열린 드롭다운 팝업 HitTest/배경 차단·TextInput 런타임 입력/IME(`UIInteractionCheck`), Shader Graph 범용 `TextureSample` 0..7 슬롯·채널 분리·`SyncTextureBindings`(`ShaderGraphCheck`), `ModelImportModal` 60FPS 실시간 프리뷰 트랜스폼 및 Phase 6 서브폴더 구성(`VirtualPathCheck`), `GameplayCombatSystem` 중복 피격 방지/넉백 및 `ActionTimelineSystem` 노티파이 연동(`ActionTimelinePlaybackCheck`), `AnimationIKComponent` Foot Ground IK 접지·골반 보정 및 비주얼 스크립팅 노드(`BlendTree2DCheck`).
2. **후보 그룹 A & B-2**: `AsyncResourceOpenService` 4대 리소스 편집기(Material, Texture, VFX, Sprite) 전면 확산(`3fdfff6f`), `GameplayCombatSystem` 런타임 매니페스트 등록 및 뷰포트 디버그 기즈모(`e3e00bd0`), KCC 체공 낙하 조기 스냅 억제·30도 경사로 법선 분리 오프셋·경사→평지 연착륙 클램핑(`KinematicCharacterCheck`), B-2 머티리얼 상수 버퍼 블롭 버전 캐싱·단일 순회 무할당 파라미터 해석 및 렌더러 PSO 우선 정렬(`9b17e906`, `ShaderGraphCheck`).
3. **신규 6대 핵심 엔진 시스템 (스프린트 C)**: `.bampak` 리소스 팩 아카이브 포맷·결정적 `PakWriter`·무결성 `PakReader`·`PakMountManager`(`PakArchiveCheck`), `SaveGameManager` 원자적 슬롯 저장(`.tmp` + `MoveFileExW`)·메타데이터·체크포인트 캡처/복원(`SaveGameCheck`), `NavGrid` 8방향 A* 코너 컷 방지·직선화 스무딩·`NavAgentComponent`·`AIPerceptionComponent`·`NavigationSystem`(`NavigationAICheck`), `AudioMixer` 5대 버스 유효 게인·페이드·3D 거리 감쇠·FMOD 채널 그룹 동기화(`AudioMixerCheck`), `NetworkRpc` Server/Client/Multicast RPC·Dead Reckoning 외삽·지연/손실 시뮬레이터(`NetworkRpcCheck`), `ScenePersistenceComponent` `DontDestroyOnLoad`·Additive 서브씬 스트리밍·`AsyncResourceLoader`(`SceneStreamingCheck`).
4. **스프린트 D · 그룹 1 · 그룹 A(1~3) · 그룹 B-4 (`2b2ffbf9`..`c18c8ea2`, 실빌드/테스트 검증 대기 포함)**:
   - 시네마틱 카메라 컷/스플라인 시퀀서 트랙 및 `AnimationTimelinePanel` 저작 UI(`2b2ffbf9`, `06487b72`), `.bampak` 2-팩 패키징 및 독립 `ClientGameRuntime` 자동 마운트·오디오 믹서/비동기 로더 부트(`b2419fbd`, `ffdc7b09`), `NavGrid` 원클릭 베이크·셀 오버레이(`f0d67432`, `d81847c1`, `40c91851`).
   - 비주얼 스크립팅 노드 확장 총 25종(`3263d3a5` 15종 + `97c69d29` 10종), `SceneRLEnvironment`·`VectorizedRLEnvironment`·`RLServer` `VEC_*` 프로토콜 + 동적 장애물 커리큘럼·다중 웨이포인트·`SaveGame` 스냅샷(`e66c3e76`, `8f9f3154` — **`RLSimulationCheck` 실패(5초) 원인 확인 필요**).
   - `BAM_EDITOR=0/1`·No-Op 스텁·`BAM_SHIPPING` 로깅 스트립(`2489e74e`, `BuildConfigurationAndPreprocessing.md`), `ResolveResourceHandle` 리팩터링 파일럿 1단계(`c18c8ea2`, `CodeRefactoringPlan.md`).
   - 차기 후속 우선순위: (1) `RLSimulationCheck` 실패(5초) 로그 확인 및 수정·미검증 CTest(`GraphReflectionCheck`, `GraphExecutionCheck`, `SceneArchiveCheck`, `SceneReflectionCheck`, `BAMENGINE_WITH_EDITOR=OFF` + `BAMENGINE_SHIPPING=ON` Client) 검증, (2) 그룹 B-5 에디터 UX 일관성 점검(`SettingsPanel`/`BuildProfilePanel`/`AnimationTimelinePanel`), (3) `ResourceManager.h/.cpp` `#if BAM_EDITOR` 후속 이관.

2026-11-22 사용자 선택으로 Render M3의 DXR 방향광 그림자 1차를 진행했다. 구현 범위와 남은 검증은 [RenderingDevelopmentPlan](RenderingDevelopmentPlan.md#m3-dxr-그림자--반사)을 따른다.

2026-11-21 Windows 제작·배포 후속에서는 인스펙터의 공격 챌린지 구성, 그래프의 타임라인 재생·Notify 수신, 빌드 프로필의 추가 씬·에셋 목록을 코드에 반영했다. 기존 Q/R 샘플의 사용자 확인과 이번 변경의 실행 상태는 [구현 기록](ImplementationLog.md)에서 구분한다.

2026-11-20 기준 커밋선(`fcda4887` $\to$ `1cd11857` $\to$ `11abec04` $\to$ `c2058594`)에 머티리얼 에디터 3-Pane 레이아웃 전면 개편, 셰이더 그래프 직접 소유권 전환, 프리팹 Revert All 외부 참조 검증 및 신규 심층 설계 2건이 반영되었다. 이후 `SceneArchiveCheck` 핫픽스를 거쳐 `x64-ninja-debug` 빌드 및 CTest 전체(44개) 통과를 확인했다([ImplementationLog](ImplementationLog.md) 2026-11-20 항목). 머티리얼 시스템 고도화 및 기존 작업 연속성에 맞춰 차기 개발 후보군을 5개 영역으로 공식 재편성한다.

1. **후보 1 (머티리얼 / 셰이더 파이프라인): 그래프 파라미터 동기화 및 VS/PS 기능 마감 [구현 완료: 2026-11-22]**
   - **기준 문서**: [MaterialGraphSynchronizationAndStageDesign.md](MaterialGraphSynchronizationAndStageDesign.md), [MaterialShaderGraphIntegrationPlan.md](MaterialShaderGraphIntegrationPlan.md)
   - **목표**: 상용 엔진(UE/Unity) 수준의 0초 실시간 파라미터 튜닝 피드백 및 정점 변위(WPO)/Custom UV를 지원하는 통합 셰이더 컴파일 파이프라인 구축.
   - **세부 내용**:
     - 머티리얼 인스펙터 슬라이더/컬러 조절 시 셰이더 재컴파일 없이 GPU 상수 버퍼(`MaterialConstants : register(b0, space3)`) 메모리만 즉시 갱신하는 60fps 무지연 Fast-Path 구현.
     - 셰이더 그래프 마스터 노드 핀 ↔ 머티리얼 파라미터 맵 간 양방향 실시간 동기화.
     - **범용 동적 버퍼 레이아웃(Dynamic ConstantBuffer Layout)**: 48바이트 정적 PBR 구조체(`baseColor`, `roughness` 등) 하드코딩 종속을 탈피하고, 그래프의 임의 커스텀 파라미터(디졸브, UV 스케일 등)를 수집하여 HLSL `cbuffer` 동적 생성 및 원시 바이트 블롭(`std::vector<uint8_t>`) 기반 런타임 저장소로 고도화하는 아키텍처 수립.
     - `Shader.MaterialOutput` 단일 마스터 노드에 정점 입력 핀(WPO, Normal, Custom UV)과 픽셀 입력 핀(PBR)을 함께 배치하고, `ShaderGraphCompiler`에서 `GenerateGBufferVS`와 `GenerateGBufferPS`를 동시 분할 생성.
   - **관련 파일**: `MaterialEditor.cpp`, `MaterialEditorCanvas.cpp`, `MaterialEditorShader.cpp`, `ShaderGraphCompiler.cpp`, `MaterialInterface.cpp`, `MaterialConstants.h`.
   - **구현 반영:** 동적 cbuffer·GraphVariable 노출·WPO VS/PS 생성, `Custom UV0/1` 연산/폴백, 범용 `Shader.TextureSample` 노드 멀티슬롯(`0..7`) 바인딩, 채널 분리(`rgba`/`rgb`/`r`/`g`/`b`/`a`), `MaterialInterface::SyncTextureBindings`, B-2 상수 버퍼 블롭 캐싱(`m_ConstantsBlob`) 및 무할당 파라미터 해석, `Renderer` PSO 우선 정렬까지 전면 반영 완료되어 `ShaderGraphCheck` 단위 테스트를 통과했다.

2. **후보 2 (렌더 패스 정합성): 머티리얼 기본 렌더 패스(Primary Pass Type) 및 Fallback 정합성 [구현 완료: 2026-11-20]**
   - **기준 문서**: [MaterialPrimaryPassTypeDesign.md](MaterialPrimaryPassTypeDesign.md)
   - **목표**: 머티리얼 기본 셰이더의 암묵적 GBuffer 고정 구조를 해소하고, `ForwardTransparent`(반투명), `UI`, `PostProcess` 등 주 렌더 패스를 머티리얼이 직접 결정할 수 있도록 확장.
   - **세부 내용**:
     - `MaterialInterface`에 `m_PrimaryPassType` 속성 및 리플렉션 등록.
     - `GetPixelShader(passType)`에서 규격 불일치 타 패스 요청 시 렌더러 기본 셰이더로 안전하게 Fallback하도록 조회 로직 개선 (반투명 재질 타깃 불일치 드로우콜 스킵 버그 근본 해결).
     - 머티리얼 에디터 [파이프라인 설정] 헤더에 기본 패스 타입 선택 콤보 및 `BlendMode` 권장 프리셋 연동.
   - **관련 파일**: `MaterialInterface.h/.cpp`, `MaterialEditor.cpp`, `MeshRenderSystem.cpp`, `Renderer.cpp`.

3. **후보 3 (물리 / 3인칭 캐릭터): KCC 지면 착지 및 경사로 3대 물리 상호작용 잔여 결함 해결 [구현 완료: 2026-11-22]**
   - **기준 문서**: [ThirdPersonCharacterSystemPlan.md](ThirdPersonCharacterSystemPlan.md) 4절
   - **목표**: `SimpleTests` 씬 실환경에서 확인된 물리 상호작용 및 지면 스냅 결함을 안정화하여 자연스러운 3인칭 보행 조작감 완성.
   - **세부 내용**:
     - 체공 낙하(`!IsGrounded && Velocity.y < 0`) 중 `snapMargin` 최소화 및 캡슐 하단 스윕 기반 2단계 착지 확정을 통한 공중 1m 조기 순간이동 흡착 제거.
     - 30도 경사로 보행 시 접촉 표면 법선 방향 `SkinWidth` 최소 분리 오프셋 보정 및 스윕 피부 마진 강화로 메시 파고듦 제거.
     - 45도/60도 급경사 끝단 $\to$ 평지 진입 시 전환 프레임 수직 스냅 속도 제한(Vertical Snap Clamping) 및 법선 보간을 통한 단차 '탁' 충격 완화(Soft Landing).
   - **관련 파일**: `Engine/Source/Physics/Private/KinematicCharacterSystem.cpp`, `Engine/Source/Physics/Private/Collision.cpp`, `Engine/examples/KinematicCharacterCheck.cpp`.
   - **구현 반영:** 3대 물리 결함 수정 및 연착륙 슬라이드 감속 예산 공제 로직을 반영하여 `KinematicCharacterCheck` 단위 테스트를 통과했다 (`68213696`, `5b3d775b`, `671d6ae6`).

4. **후보 4 (비주얼 이펙트 / 액션 파이프라인): Composite VFX Phase 5 Step 4 & 액션 타임라인-소켓 VFX 통합 완성 (기존 후보 B)**
   - **기준 문서**: [CompositeVisualEffectPlan.md](CompositeVisualEffectPlan.md), [AnimationTimelineEditorPlan.md](AnimationTimelineEditorPlan.md), [AnimationPipelineIntegrationPlan.md](AnimationPipelineIntegrationPlan.md)
   - **목표**: 이미 기본 구현된 본 소켓 VFX 스폰·블렌드 패스 교정·공중 프리뷰를 토대로, 편집 씬 오염·Animator 미복원·프리뷰/런타임 불일치·절대 경로 참조 결함을 해소하고 대표 액션(입력 → 클립 → 무기 소켓 VFX/사운드 → 판정 1회 → 이동 복귀)의 PIE·독립 Client 통합 검증을 완료한다.
   - **기구현 현황 (2026-11-19 반영, 중복 구현 제외)**:
     - Phase 5 Step 3 `CurveEditorModal` 2D 커브 팝업 모달 완료.
     - `ActionTimelineAsset`/`ActionTimelineSystem`/`AnimationTimelinePanel`의 `SpawnCompositeVFX` 본 소켓 스폰·`SocketAttachmentComponent` 실시간 추적·지속형 `ActionVFXHoldComponent` 해제·`ActionTimelineEvents` 기본 구현 완료.
     - `CompositeVFXSystem` Geometry/ForwardTransparent 블렌드 필터 교정(허공 흙갈색 탁화 해소), `forwardParticlePS.hlsl` 정면 알파 완화(`0.90`), `VFXEditorPanel::ToggleAirMode()` 공중 프리뷰 토글 완료(9.9절 가상 환경광 보정은 2026-11-19 정정으로 철회).
     - **2026-11-22 전투 연동 반영:** `GameplayCombatSystem` 단일 공격 내 중복 피격 방지(피격자 해시셋 캐싱), 넉백 벡터 계산, 상호작용 질의 및 `ActionTimelineSystem` `"HitboxOn"` / `"HitboxOff"` 노티파이 창 수명 연동을 완료하고 `ActionTimelinePlaybackCheck` 단위 테스트를 통과했다 (`24288084`, `952b112a`).
   - **현재 진행 상태 (2026-11-20)**: 1~3단계 코드와 회귀 검사 사례를 반영했다. 빌드·CTest·로캘 생성·PIE·독립 Client 검증은 실행하지 않았으며, 4단계 수동 통합 검증이 남아 있다. 세부 체크리스트와 명령은 [AnimationPipelineIntegrationPlan.md](AnimationPipelineIntegrationPlan.md) 5·7절을 따른다.
     - 1단계: Gameplay 페이즈 분리, 인스펙터 재생의 독립 프리뷰 라우팅, Animator 소유권 기반 복원과 타임라인 컴포넌트 제거 훅을 반영했다.
     - 2단계: 프리뷰·런타임이 공용 시간/Notify 교차 판정 helper를 사용하고, 포즈 갱신 뒤 Notify를 검사하도록 정렬했다. `ActionTimelinePlaybackCheck`를 추가했지만 실행하지 않았다.
     - 3단계: 모델 경로 저장 시 가상 URI 정규화, 클립·스켈레톤·소켓·범위 진단, 기본 스프라이트·파티클 머티리얼의 셰이더 URI 정규화를 반영했다.
     - 4단계: 패널·편집 씬 격리·복원 수명·Notify 경계·PIE·독립 Windows Release Client 수동 확인이 남아 있다. 순수 Additive(`One/One`) 분리와 `ParticleSystem`/`CompositeVFXSystem` 패스 수집 중복 정리는 선택적 후속 검토이며 이번 구현 범위에 포함하지 않았다.
   - **관련 파일**: `Engine/Source/VFX/Private/CompositeVFXSystem.cpp`, `Engine/Source/Animation/Timeline/`, `Engine/Source/Render/RHI/Public/RHIBlendTypes.h`, `Editor/Source/ImGui/TimelinePanel/Private/AnimationTimelinePanel.cpp`, `Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp`, `Editor/Source/Application/Private/Application.cpp`, `Editor/Source/AssetManager/Private/AssetReferenceValidator.cpp`, `Editor/Source/ImGui/ResourceEditor/Private/VFXEditorPanel.cpp`.

5. **후보 5 (에셋 파이프라인 격리): `BamAssetWorker` Phase 3 대용량 모델/텍스처 임포트 독립 프로세스 위임 및 푸터 바 진행률 연동 (기존 후보 C)**
   - **기준 문서**: [OutOfProcessAssetPipelinePlan.md](OutOfProcessAssetPipelinePlan.md)
   - **목표**: 대용량 모델(수백 MB FBX/GLTF) 및 텍스처(BC7 4K) 임포트 시 에디터 메인 스레드 60FPS 무간섭 유지 및 OOM/크래시 100% 완전 격리.
   - **세부 내용**:
     - `Tools/AssetWorker/`: `BamAssetWorker.exe` 파이프 데몬 프로토콜에 `IMPORT_MODEL`(Assimp + meshopt LOD) 및 `IMPORT_TEXTURE`(DirectXTex BCn 압축) 명령 확장 및 실시간 진행률(`PROGRESS`) 응답 구현.
     - `Editor/Source/AssetManager/Worker/AssetWorkerHost.cpp`: 에셋 임포트 비동기 위임 및 하단 푸터 바(`FooterBar`) 실시간 진행률 프로그레스 바 / 취소(`[X]`) 버튼 연동.
   - **관련 파일**: `Tools/AssetWorker/main.cpp`, `AssetWorkerHost.h/.cpp`, `AssetManager.cpp`, `ToolBar.cpp`.
   - **2026-11-20 코드 반영**: `AssetWorkerSnapshotCheck`가 모델 snapshot을 `ReadModelSnapshot`으로 실제 소비하며, 독립 이미지 임포트와 모델 임포트의 내장/외부 텍스처 베이킹을 `IMPORT_TEXTURE`로 위임한다. `.bamtex`/`.dds`는 staging 후 함께 교체하고, 텍스처 진행률·취소·워커 사용 불가 시 인프로세스 폴백 및 슬롯별 압축 설정을 연결했다. 모델은 텍스처 작업 완료 후 커밋하며, 베이킹 실패·취소는 기존처럼 원본 텍스처를 사용한다. `AssetWorkerTextureCheck`가 PNG → BC7 및 실패 산출물 부재를 검사하도록 추가됐다.
   - **미검증/잔여**: 저장소 지침에 따라 AI는 빌드·테스트를 실행하지 않았다. 사용자 실행 대기: `cmake --build --preset x64-ninja-debug --target Editor BamAssetWorker AssetWorkerSnapshotCheck`, `ctest --test-dir out/build/x64-ninja-debug -R "^AssetWorker(Model|Texture)Check$" --output-on-failure`. 모델의 내장/외부 텍스처 임포트, 출력 보존, 취소·폴백, 에디터 커밋 및 대형 에셋 프레임 실측은 실행 검증이 남아 있다.

### 11.7 2026-11-23 사용자 요청 반영 작업 순서

사용자가 요청한 지형 확장·그래프 바인딩 피커·이름 변경 실패·액션 3초 지연·디스토션·시퀀서 필요성을 점검하고 다음 순서로 재정립했다. 11.6의 P0 잔여(`RLSimulationCheck` 실패, 미실행 CTest)는 계속 우선한다.

| 순서 | 작업 | 선행 | 완료 기준 | 상태 |
|---|---|---|---|---|
| 1-A | 콘텐츠 브라우저 이동·이름 변경 차단 사유 표시 | 없음 | 차단 시 모달에 구체적 원인(미저장 씬·편집기·프리팹 모드 등)과 대상 이름이 표시된다 | 코드 반영 / 빌드·실행 미검증 ([구현 기록](ImplementationLog.md)) |
| 1-B | 액션 타임라인 종료 정책(`EndPolicy`), 종료 이벤트, 새 타임라인 기본값 | 없음 | 클립 없는 타임라인이 마지막 트랙 종료 시점에 끝나고 그래프가 종료 사유를 받는다 | 코드 반영 / 빌드·실행 미검증 ([AnimationTimelineEditorPlan.md](AnimationTimelineEditorPlan.md) 12절) |
| 1-C | `GraphBindingComponent::graphFile` 로직 그래프 경로 피커 | 없음 | 로직 도메인 그래프만 표시되고 이동·이름 변경 후 참조가 유지된다 | 코드 반영 / 빌드·실행 미검증 ([구현 기록](ImplementationLog.md)) |
| T1~T4 | 지형 저장·레이어·그래프 도메인·내비게이션 | [TerrainSystemExpansionPlan.md](TerrainSystemExpansionPlan.md) 4절 | 같은 문서 4절 | T1(a~d)·T2(a~e)·T3(a~c)·T4 코드 반영 / 빌드·실행 미검증 |
| V1 | 디스토션 1단계: `ForwardTransparent` 그래프에 `Shader.ScreenUV`·`Shader.SceneColor` 노드 | 없음 | 강도 0에서 왜곡이 없고 화면 밖 UV는 왜곡 전 배경을 쓴다 | 코드 반영 / 빌드·실행 미검증 |
| 보류 | 씬 단위 시퀀서 | 컷신 요구 확인 | — | 보류(P4) |

- **이름 변경 실패 원인(정적 분석)**: 참조 스캔과 이동 가능 판정이 분리되어 있다. 참조가 0건이어도 `ImGuiManager::PrepareAssetFileChanges`가 미저장 리소스 편집기·미저장 씬·프리팹 모드·PIE·씬 로딩에서 거부하며, 모달은 원인을 구분하지 않았다. 확인 후 `ResourceManager::RelocateFileResources`가 모델 임포트 원본·셰이더 바이너리 경로를 가진 로드 리소스 때문에 거부할 수도 있다.
- **3초 지연 원인(정적 분석)**: `ContentBrowserPanel::CreateEmptyTimeline`이 새 타임라인을 3초로 만들고, 클립을 지정할 때만 길이가 바뀐다. 종료 조건은 에셋 길이뿐이며 `ActionTimeline.Play`는 재생 중 재실행을 거부한다. 1-B에서 `Fixed`/`ClipLength`/`LastTrackEnd` 정책과 지속형 노티파이의 해제 규칙(경과·타임라인 종료·Stop/중단 시 히트박스 Off와 VFX Hold 해제)을 명시한다.
- **디스토션**: 불투명 패스 후 `SceneColorCopy`가 이미 있어(`glassPS`) 그래프 노드 추가로 첫 결과를 낸다. 반투명끼리는 서로의 왜곡을 보지 못하는 제약이 있으며, 다수 중첩·파티클 대응이 필요하면 별도 디스토션 버퍼 후처리를 검토한다.
- **시퀀서**: `ActionTimeline`이 이미 엔티티 단위 카메라 컷·스플라인 트랙을 가진다. 여러 엔티티 바인딩 컷신 요구가 생기기 전에는 새 시스템을 만들지 않고, 필요 시 ActionTimeline에 엔티티 바인딩 트랙을 확장하는 방향을 먼저 검토한다.

#### 11.7.1 검증 대기 목록 (2026-11-24)

코드는 반영했지만 AI는 빌드·실행하지 않았다. 아래 항목을 사용자가 확인한 뒤 각 행을 지우고, 결과를 [ImplementationLog.md](ImplementationLog.md)의 해당 기록에 남긴다. 실패하면 출력과 로그를 붙여 해당 작업으로 돌아간다.

**공통 빌드·검사**

```powershell
cmake --build --preset x64-ninja-debug --target GenerateLocalization Editor Client ActionTimelinePlaybackCheck AssetReferenceCheck TerrainDataCheck ShaderGraphCheck NavigationAICheck PakArchiveCheck SceneArchiveCheck RLSimulationCheck VFXBlendAndPassCheck VirtualPathCheck RaytracingSkinnedPolicyCheck
ctest --test-dir out/build/x64-ninja-debug -R "^(ActionTimelinePlaybackCheck|AssetReferenceCheck|TerrainDataCheck|ShaderGraphCheck|NavigationAICheck|PakArchiveCheck|SceneArchiveCheck|VFXBlendAndPassCheck|VirtualPathCheck|RaytracingSkinnedPolicyCheck)$" --output-on-failure
ctest --test-dir out/build/x64-ninja-debug -R "^RLSimulationCheck$" --output-on-failure -V
```

| 작업 | 확인 항목 | 통과 기준 | 상태 |
|---|---|---|---|
| 1-A | 씬을 수정해 저장하지 않은 채 참조 없는 에셋 이름 변경 | 모달에 미저장 문서 사유와 씬 파일명이 표시된다 | 대기 |
| 1-A | 머티리얼 편집기를 미저장 상태로 두고 이름 변경 | 모달에 편집기 이름이 표시된다 | 대기 |
| 1-A | 모두 저장한 뒤 이름 변경 | 모달 없이 바로 바뀐다 | 대기 |
| 1-A | 한국어·영어 전환 | 차단 사유 문구가 두 언어로 표시된다 | 대기 |
| 1-B | `ActionTimelinePlaybackCheck` | 통과(정책별 길이, `Completed`·`Stopped`·`Interrupted`, 히트박스 창 닫힘, 정책 직렬화) | 빌드 오류 수정 후 재실행 대기 |
| 1-B | **사용자 보고: 어느 정책에서도 1초에 Completed** | `TestScene`에서 공격 시 콘솔 `ActionTimeline` 로그의 `Session N ended: Completed at`이 약 0.05초(현재 `Test.bamtimeline.json`), 경로가 `Test.bamtimeline.json`이다. 1초 근처면 로그 전체를 보고한다 | 원인 미확정 |
| 1-B | 패널에서 종료 시점을 고정 길이로 바꿔 저장 | 타임라인 핫 리로드 알림이 뜨고, 다음 공격이 0.4초에 끝난다(에디터 재시작 없이) | 대기 |
| 1-B | 종료 시점 콤보 변경·Ctrl+Z | 파란 종료선과 `x초에 종료` 표시가 따라 바뀌고 되돌아간다 | 대기 |
| 1-B | 그래프 `On Action Timeline Ended` → `Print` | 끝까지 재생 시 `reason` 0, Stop 시 1 | 대기 |
| 1-B | 기존 타임라인(`BaselineVerification_Hit` 등) | 이전과 같은 길이(약 1.07초)에서 끝난다 | 대기 |
| 1-C | Graph Binding 피커 목록 | `Assets/Scripts`의 로직 그래프만 있고 `Assets/Shaders/Graphs/NewMaterial_2`는 없다 | 대기 |
| 1-C | 셰이더 그래프를 피커에 끌어다 놓기 | 거부된다 | 대기 |
| 1-C | 그래프 선택 후 PIE | 고른 그래프가 실행된다 | 대기 |
| 1-C | 콘텐츠 브라우저에서 그래프 이름 변경 | 피커 표시와 PIE 실행이 유지된다. `AssetReferenceCheck` 통과 | 대기 |
| 1-C | Ctrl+Z | 이전 그래프로 돌아간다 | 대기 |
| T1-a | `TerrainDataCheck` | 통과(한글 경로 왕복, R16 오차, 손상·잘린 파일 거부, 기본 경로 이름) | 대기 |
| T1-a | 지형을 스컬프트·페인트하고 씬 저장 | 씬 옆에 `<씬>_Terrain_<ID>.bamterrain`이 생기고 씬 JSON의 `DataPath`가 그 파일을 가리킨다 | 대기 |
| T1-a | 에디터 재시작 후 씬 열기 | 높이와 스플랫이 저장 시점과 같다 | 대기 |
| T1-a | 이전 씬(`DataPath` 없음) 열기 | 오류 없이 열리고 기존 기본 형태가 보인다 | 대기 |
| T1-a | 독립 Client 실행 | 같은 지형이 보인다(패키징에 `.bamterrain` 포함 여부도 확인) | 대기 |
| T1-a | 스컬프트 → Ctrl+Z → 저장 → 재시작 | 되돌린 상태가 저장되어 있다 | 대기 |
| T1-b | `TerrainDataCheck`의 `CheckUnsavedTerrainPreservation` | 통과 | 대기 |
| T1-b | 지형을 스컬프트하고 **저장하지 않은 채** PIE 시작 → 종료 | 스컬프트한 높이가 그대로 남는다(마지막 저장 상태나 기본 형태로 돌아가지 않는다) | 대기 |
| T1-b | 저장한 적 없는 새 씬의 지형으로 위 항목 반복 | 같다 | 대기 |
| T1-b | 스컬프트 후 자동 저장 주기를 기다린 뒤 에디터 강제 종료 → 재시작 → 복구 | 복구한 지형이 스컬프트한 상태다. `Temp/`(복구 폴더)에 `AutoSave_<씬>_Terrain_<ID>.bamterrain`이 있다 | 대기 |
| T1-b | 복구 후 씬 저장 | 원래 `.bamterrain`(씬 옆)에 쓰이고, 씬 JSON의 `DataPath`가 복구 폴더를 가리키지 않는다. `RecoveryDataPath`는 빈 값이다 | 대기 |
| T1-b | 원본 파일 보호 | 자동 저장만 일어나고 씬을 저장하지 않았을 때 씬 옆 `.bamterrain`의 수정 시각이 바뀌지 않는다 | 대기 |
| T1-c | `TerrainDataCheck`의 `CheckHeightmapImportExport` | 통과 | 대기 |
| T1-c | 지형 편집기 → 하이트맵 내보내기(PNG) | 16비트 그레이스케일 PNG가 생기고 외부 이미지 도구(예: GIMP)에서 16비트로 열린다. 완료 알림이 뜬다 | 대기 |
| T1-c | 내보낸 PNG를 다시 가져오기 | 지형 모양이 같고 경사면에 8비트 계단이 보이지 않는다 | 대기 |
| T1-c | RAW(`.r16`)로 내보내기·가져오기 | 위와 같다. 파일 크기 = (해상도+1)² × 2바이트 | 대기 |
| T1-c | 한글 폴더·파일 이름으로 내보내기·가져오기 | 성공한다 | 대기 |
| T1-c | 기존 8비트 PNG 하이트맵 가져오기 | 이전처럼 가져와진다 | 대기 |
| T1-c | 가져오기·초기화 후 Ctrl+Z / Ctrl+Y | 이전 지형으로 돌아가고 다시 적용된다. 편집 메뉴에 "하이트맵 가져오기"·"지형 초기화"가 보인다 | 대기 |
| T1-c | 이미지가 아닌 파일 가져오기 | 경고 알림이 뜨고 지형이 바뀌지 않으며 이력에 남지 않는다 | 대기 |
| T1-d | 새 지형 엔티티 만들기(계층 → 지형) | 인스펙터의 머티리얼이 `TerrainMaterial`(엔진)이고 지형이 바로 보인다 | 대기 |
| T1-d | `Assets/Material/TerrainMaterial.bammat.json`을 임시로 다른 폴더에 옮긴 뒤 기존 지형 씬 열기 | 지형이 엔진 기본 머티리얼로 보이고, 머티리얼 로드 오류가 매 프레임 반복되지 않는다. 확인 후 파일을 되돌린다 | 대기 |
| T1-d | 기존 씬(Assets 머티리얼 참조) 열기 | 이전과 같은 모양이다 | 대기 |
| T1-d | Client 일반 파일 패키지 만들기 | 패키지 `Assets` 아래에 씬의 `.bamterrain`과 `Engine/Resources/Material/TerrainMaterial.bammat.json`이 있고, 실행하면 지형이 저장한 모양이다 | 대기 |
| T1-d | Client pak 패키지 | 지형 밖 기존 한계(pak 리소스 로더 없음). 확인만 하고 결과를 기록한다 | 참고 |
| T2-a | `TerrainDataCheck`의 `CheckLayersAndAttributes` | 통과(해상도 상한, v2 왕복, v1 호환, 이동·회전 지형 페인트 위치·정규화, 속성 페인트, 해상도 변경) | 대기 |
| T2-a | T1에서 저장한 지형 씬(`.bamterrain` v1) 열기 | 높이·스플랫이 그대로 보인다. 다시 저장하면 파일이 v2가 되고(크기 증가: 정점당 5바이트) 다시 열어도 같다 | 대기 |
| T2-a | 인스펙터에서 지형 해상도 입력 | 1024보다 크게 넣을 수 없다. 1024로 바꾸면 지형이 다시 만들어지고 높이가 리샘플링된다 | 대기 |
| T2-a | 해상도를 바꾼 뒤 저장·재시작 | 바뀐 해상도와 높이가 유지된다 | 대기 |
| T2-b | 지형 편집기 열기 → 지형 선택 | `브러시: 꺼짐` 버튼과 `조형`·`레이어 페인트`·`속성 페인트` 도구 버튼이 보이고, 선택한 도구가 강조된다 | 대기 |
| T2-b | 브러시 켜기 → 조형 도구로 드래그 | 이전과 같이 높이가 바뀐다(초록 링). Ctrl+Z 메뉴 이름이 "조각" | 대기 |
| T2-b | 레이어 페인트 → 레이어 2 선택 → 드래그 | 칠한 곳이 레이어 2(암석) 색으로 바뀐다(파란 링). 놓을 때 이력 1건 "레이어 페인트", Ctrl+Z로 되돌아간다 | 대기 |
| T2-b | 지형을 이동·회전한 뒤 페인트 | 링 중심(커서 아래)에 칠해진다 | 대기 |
| T2-b | 레이어 5~8 선택 | (T2-e에서 대체) 경고 없이 칠한 색이 보인다 | T2-e 참고 |
| T2-b | 속성 페인트(이동 불가) 드래그 | 화면 변화는 없고(주황 링) 이력 1건 "속성 페인트"가 남는다. 저장 후 `.bamterrain` 수정 시각이 바뀐다 | 대기 |
| T2-b | 뷰포트에 포커스를 두고 숫자 1~8 | 페인트 도구에서만 팔레트 선택이 바뀐다. 조형 도구이거나 브러시가 꺼져 있으면 다른 동작에 쓰이거나 무시된다 | 대기 |
| T2-b | `[`·`]` 누르고 있기 | 브러시가 켜져 있을 때 반경이 1~100 사이에서 연속으로 줄고 는다 | 대기 |
| T2-b | 단축키 설정 창 | "지형 편집기" 분류에 10개 동작이 있고, 재바인딩하면 팔레트·반경 툴팁의 키 표시가 바뀐다. 기존 동작과 충돌 경고가 없다 | 대기 |
| T2-b | 텍스트 입력 중(검색창 등) 숫자 입력 | 레이어 선택이 바뀌지 않는다 | 대기 |
| T2-b | 한국어·영어 전환 | 새 문구가 두 언어로 보인다 | 대기 |
| T2-c | 속성 페인트 도구로 이동 불가·물 칠하기 | 칠한 정점에 빨강·파랑 점이 보이고, 커서를 지형 밖으로 옮겨도 유지된다. 지우기로 칠하면 점이 사라진다. 다른 도구로 바꾸면 점이 숨는다 | 대기 |
| T2-c | 지형을 이동·회전한 뒤 속성 점 | 점이 지형 표면 위치에 붙어 있다 | 대기 |
| T2-c | 해상도 1024 지형에서 속성 페인트 | 프레임 시간 측정(점 표시 비용). 느리면 보고 | 측정 필요 |
| T2-c | 이전 씬(`Layers` 없음) 열기 | 오류 없이 열리고 목록이 비어 있다 | 대기 |
| T2-d | `TerrainDataCheck`의 `CheckTerrainLayerAsset` | 통과(한글 파일명 저장 왕복, 키 유지, 표시 이름, 확장자 판정) | 대기 |
| T2-d | 콘텐츠 브라우저 우클릭 → 만들기 → 지형 레이어 | `NewTerrainLayer.bamterrainlayer.json`이 생기고 이름 바꾸기 상태가 된다. 초록 레이어 아이콘·"LAYER" 배지 | 대기 |
| T2-d | 그 파일 더블클릭 | 지형 레이어 편집기가 열리고 알베도 텍스처·반복 길이·이동 가능·이동 비용·표면 종류를 편집할 수 있다 | 대기 |
| T2-d | 값 변경 → Ctrl+Z / Ctrl+Y → Ctrl+S | 되돌리기·다시 실행이 되고, 저장하면 미저장 표시가 사라지며 파일에 반영된다 | 대기 |
| T2-d | 미저장 상태로 편집기 닫기 | 저장·버리기 확인이 뜬다 | 대기 |
| T2-d | 지형 컴포넌트 → 레이어 목록 `+` → 칸의 에셋 피커 | 지형 레이어 에셋만 목록에 보이고 고를 수 있다. 끌어다 놓기도 된다 | 대기 |
| T2-d | 레이어 에셋 이름을 "잔디"로 바꾸고 칸에 지정 | 지형 편집기 팔레트 칸이 "잔디"로 보이고, 선택 시 에셋 설정 요약이 보인다. 에셋이 없는 칸은 "레이어 N"과 지정 안내 | 대기 |
| T2-d | 두 지형이 같은 레이어 에셋을 참조한 상태에서 에셋의 이동 비용을 바꿔 저장 | 핫 리로드 알림이 뜨고 두 지형 팔레트 요약이 모두 바뀐다(에디터 재시작 없이) | 대기 |
| T2-d | 씬 저장·재시작 | 지형 칸의 에셋 참조가 유지된다 | 대기 |
| T2-d | 레이어 에셋을 콘텐츠 브라우저에서 이동·이름 변경 | 지형 칸의 참조가 끊기지 않는다(기존 리소스 핸들 경로). 끊기면 보고 | 대기 |
| T2-d | 한국어·영어 전환 | 새 문구(만들기 메뉴, 편집기 제목·안내, 알림)가 두 언어로 보인다 | 대기 |
| T2-e | 셰이더 빌드 | `terrainPS.hlsl` 컴파일 경고·오류가 없다(특히 분기 안 텍스처 읽기 경고) | 대기 |
| T2-e | 레이어 에셋이 없는 기존 지형 씬 열기 | 이전과 같은 색(잔디·암석·흙)과 무늬 크기로 보인다. 절벽은 암석 색 | 대기 |
| T2-e | 레이어 5~8을 칠하기 | 칠한 곳에 해당 칸 기본 색(파랑·노랑·보라·흰색 계열)이 바로 보인다. 저장·재시작 후에도 같다 | 대기 |
| T2-e | 레이어 에셋에 알베도 텍스처 지정 후 저장 | 그 레이어를 칠한 곳에 텍스처가 보이고, 같은 에셋을 쓰는 다른 지형에도 바로 반영된다 | 대기 |
| T2-e | 레이어 에셋의 반복 길이를 1m·20m로 바꿔 저장 | 무늬 크기가 바뀐다 | 대기 |
| T2-e | 두 레이어 경계 | 경계에서 텍스처가 반짝이거나 줄무늬가 생기지 않는다(`SampleGrad`) | 대기 |
| T2-e | 지형을 이동·회전 | 레이어 무늬는 월드 XZ 기준이라 지형을 옮기면 무늬가 미끄러진다(이전과 같은 동작, 결함 아님) | 참고 |
| T2-e | 그림자·피킹·TAA 켠 상태 | 지형 그림자·선택·모션 벡터가 이전과 같다 | 대기 |
| T2-e | Vulkan·SDL GPU 백엔드(있으면) | 지형이 그려지는지 확인. 지형 상수 256바이트를 받지 못하면 보고 | 미확인 |
| T2-e | 프레임 시간 | 레이어 8개를 고르게 칠한 넓은 지형에서 G-Buffer 패스 시간을 칠하기 전과 비교 | 측정 필요 |
| T3-a | 셰이더 빌드 | `terrainVS`·`terrainShadowDepthVS`·`motionVectorTerrainVS`·`terrainPS` 컴파일 오류·경고가 없다 | 대기 |
| T3-a | 지형이 있는 씬 | 높이·레이어 색이 T2-e와 같다(바인딩 이동 전후 차이 없음) | 대기 |
| T3-a | 지형 그림자 | 지형이 그림자를 드리우고 받는다(`terrainShadowDepthVS`가 새 슬롯의 높이맵을 읽음). 평평한 판 모양 그림자면 실패 | 대기 |
| T3-a | TAA 켜고 지형 이동 | 잔상이 없다(`motionVectorTerrainVS`) | 대기 |
| T3-a | 스컬프트·페인트 직후 | 높이·색이 바로 바뀐다(업로드한 텍스처가 새 슬롯에 묶임) | 대기 |
| T3-a | 그래픽스 디버거(PIX 등, 가능하면) | 지형 드로우에서 `t0`~`t7, space1`에 지형 텍스처가 없고, `t8`~`t10, space3`에 높이맵·스플랫 두 장이 있다 | 선택 |
| T3-a | Vulkan·SDL GPU 백엔드(있으면) | 지형이 그려지는지 확인 | 미확인 |
| T3-b1 | `ShaderGraphCheck` | 통과(지형 도메인 사례 포함) | 대기 |
| T3-b1 | 셰이더 빌드 | `terrainLayers.hlsli`를 포함한 `terrainPS.hlsl`이 경고·오류 없이 컴파일된다 | 대기 |
| T3-b1 | 지형이 있는 씬 | T3-a와 같은 모양이다(`terrainPS`를 공용 헤더로 다시 썼지만 동작은 같아야 함). 레이어 0만 칠한 곳, 여러 레이어 경계, 절벽 | 대기 |
| T3-b1 | 머티리얼 편집기의 그래프 노드 목록 | "지형 레이어 가중치/샘플/합성", "지형 경사", "지형 높이" 노드가 보인다. 일반(G-Buffer) 머티리얼에 연결하고 컴파일하면 "지형 노드는 지형 머티리얼에서만…" 오류가 뜬다 | 대기 |
| T3-b2 | 엔진 `TerrainMaterial` 인스펙터 | "머티리얼 도메인"이 "지형"으로 보인다. 다른 머티리얼은 "표면" | 대기 |
| T3-b2 | `TerrainMaterial`을 프로젝트로 복제 → 셰이더 그래프 연결 → 지형 노드(레이어 합성 → baseColor) 컴파일 | 컴파일 성공. 생성된 PS가 `terrainLayers.hlsli`를 포함해 DXC로 컴파일된다(오류 시 메시지 기록) | 대기 |
| T3-b2 | 그 머티리얼을 지형 컴포넌트에 지정 | 지형 높이·레이어가 기존과 같게 보인다. 그림자도 그대로 | 대기 |
| T3-b2 | 그래프에서 `Terrain.LayerSample(1)` → baseColor로 바꿔 실시간 반영 | 지형 전체가 레이어 1 색으로 바뀐다(에디터 재시작 없이) | 대기 |
| T3-b2 | `Terrain.Slope`를 roughness나 baseColor 혼합에 연결 | 경사에 따라 결과가 바뀐다 | 대기 |
| T3-b2 | 지형 머티리얼의 주 패스를 ForwardTransparent로 바꾸고 컴파일 | "지형 머티리얼은 지오메트리 패스를 써야 합니다" 오류 | 대기 |
| T3-b2 | 일반 머티리얼에서 그래프 컴파일 | 이전과 같이 VS·PS를 만든다(회귀 없음) | 대기 |
| T3-b2 | 지형 머티리얼 미리보기(구) | 지형 모양이 보이지 않는 것은 알려진 한계(실제 지형에서 확인) | 참고 |
| T3-c | `ShaderGraphCheck` 기본 지형 그래프 사례 | 통과 | 대기 |
| T3-c | 지형 도메인 머티리얼에서 "셰이더 그래프 만들기" | 새 그래프에 레이어 합성·레이어 샘플(1)·경사·smoothstep·빼기·lerp 두 개·상수 세 개가 연결된 상태로 열린다. 노드가 겹치지 않게 놓인다 | 대기 |
| T3-c | 그 그래프를 컴파일해 지형에 지정 | 엔진 기본 지형 머티리얼과 나란히 비교해 색·절벽 전환·거칠기(반사 하이라이트)가 같다 | 대기 |
| T3-c | 표면 머티리얼에서 "셰이더 그래프 만들기" | 이전처럼 출력 노드 하나만 있다 | 대기 |
| CTest 수정 | `ShaderGraphCheck`·`NavigationAICheck` 재실행 | 둘 다 통과한다(위 표의 T3-b1·T3-c 사례와 디스토션·지형 내비게이션 사례 포함). `NavigationAICheck`는 경로 우회·`.bamnavgrid` 저장 왕복까지 이어서 확인한다. 또 실패하면 `LastTest.log`의 마지막 메시지를 기록한다 | 대기 |
| T3-c | T3 완료 기준 | 그래프 노드 값(예: smoothstep 0.55→0.3)을 바꾸면 지형 절벽 범위가 바로 바뀌고, 그림자·클릭 선택이 이전과 같다 | 대기 |
| T4 | `NavigationAICheck` | 통과(기존 테스트 + 지형 바운드·급경사·`Blocked` 우회 경로·한글 경로 `.bamnavgrid` 저장/로드 왕복) | 대기 |
| T4 | 지형(급경사 + `Blocked` 페인트) 씬에서 뷰포트 NavGrid 베이크 버튼 클릭 | 한국어 툴팁·알림이 표시되고, 오버레이가 지형 높이를 따라 그려지며 경사/Blocked 영역이 적색(장애물)으로 표시된다 | 대기 |
| T4 | 베이크 후 씬 저장 → 에디터 재시작/씬 재로드 | `<씬이름>_NavGrid.bamnavgrid`가 생성되고, 재베이크 없이 오버레이와 `NavAgent` 우회 경로가 유지된다 | 대기 |
| V1 | `ShaderGraphCheck` (11절 7종) | 통과(강도 0 무왜곡, 왜곡 오프셋, 미연결 폴백, 화면 밖 배경 폴백, 채널 분리, GBuffer/VS 거부) | 대기 |
| V1 | `ForwardTransparent` 머티리얼 그래프에서 `Shader.ScreenUV` + `Shader.SceneColor` 연결 | 강도 0에서 배경 왜곡이 없고, UV 오프셋 시 왜곡되며 화면 밖 UV에서 가장자리 늘어짐(Streak) 없이 원본 배경을 사용한다 | 대기 |
| B-5 | `GenerateLocalization` 및 한/영 언어 전환 | `SettingsPanel`(오디오 믹서), `UnifiedSettingsPanel`(검색창), `BuildProfilePanel`(BamPak 요약·빈 빌드 로그 중앙 정렬), `AnimationTimelinePanel`(카메라 컷/스플라인 인스펙터·트랙 추가/복제) 문구가 두 언어로 깨짐 없이 표시된다 | 대기 |
| Pak | `PakArchiveCheck` (`TestLoaderPakFallbackAndTerrain`) & `SceneArchiveCheck` | 통과(가상 URI·물리 경로 `ExistsOrOnDisk`, 팩 우선 + 디스크 폴백, JSON `LoadArchiveWithFallback`, 디스크 원본 삭제 후 `.bamterrain` v2 pak 로드, 변조 팩 디스크 폴백 차단) | 대기 |
| Pak | Client pak 패키징본 실행 (`Assets/` 원본 제거 상태) | 씬·텍스처·머티리얼·셰이더·`.bamterrain`이 `.bampak`에서 정상 로드되어 렌더링된다 | 대기 |
| A-1 | `BAMENGINE_WITH_EDITOR=ON`/`OFF` 빌드 | `ResourceManager::GetDebugSnapshot()` `#if BAM_EDITOR` 스텁 전환 후 양 구성에서 Engine·Editor·Client 빌드·링크 성공 | 대기 |
| A-2 | IBL 큐브맵 할당 씬(`RenderingFeatureLab`) 첫 프레임 | 이미 `992de2bd`에 해소 반영됨. `DirectX12RHI` 패스 내 업로드 거부 로그 없이 IBL 반사/조명 정상 출력 확인 | 대기 |
| A-3 | `.bampak` 마운트 상태에서 AnimatorController·Graph JSON·Localization JSON·AsyncResourceLoader 로드 | 디스크 원본 없이 pak에서 정상 로드되고, 미마운트 시 기존 디스크 폴백이 동작한다 | 대기 |
| A-4 | `VFXBlendAndPassCheck` 및 `PureAdditive` 머티리얼 | 통과(`One/One` vs `SrcAlpha/One` 보존, `VFXPassHelper` 필터 일치) 및 머티리얼 에디터 드롭다운 한/영 표시 | 대기 |
| B-1 | `VirtualPathCheck` 및 프로젝트 A↔B 전환·독립 모듈 핫리로드 | `ResolveModules`/`ResolvePrimaryRuntimeModule` 통과, 기존 루트 `GameModule.dll` 하위 호환, 외부 프로젝트 `Source/` 수정 시 자동 빌드·리로드 및 Client Export DLL 로드 확인 | 대기 |
| B-2 | `raytracedShadowMaskRTCS.hlsl` 컴파일·`RaytracingSkinnedPolicyCheck` 및 `RenderingFeatureLab` | 셰이더 컴파일·`SortSkinnedByDistance` 통과, `Masked` 재질 DXR 그림자 알파 컷아웃 및 스킨드 32개 초과 시 근접 캐릭터 우선 유지 | 대기 |

**커밋 전 주의:** 1-A는 커밋 `cda1467f`로 main에 있다. 1-B·1-C·T1(a~d)·T2(a~e)·T3(a~c)·T4·V1·B-5·Pak 로더 연결은 미커밋이다. 새 파일 `Engine/Source/Terrain/{Public,Private}/TerrainDataFile.*`·`TerrainLayerAsset.*`, `Editor/Source/ImGui/ResourceEditor/{Public,Private}/TerrainLayerEditor.*`, `Engine/Resources/Material/TerrainMaterial.bammat.json`, `Engine/Resources/Shader/hlsl/terrainLayers.hlsli`도 커밋 대상이다. 메인 워크트리에는 MultiProject/ProjectHub 변경과 `Engine.csv`의 타 작업 행이 혼재되어 있으므로 커밋 시 기능 단위로 선별 스테이징한다.
