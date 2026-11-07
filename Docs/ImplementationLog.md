# BamEngine Implementation Log

This document tracks implementation details, architectural decisions, and technical debt.

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
     - 발바닥 오프셋(ootOffset = Center.y - Height*0.5)을 계산하여 중심 피벗 메시(구체 등)가 바닥 패널 위에 정확히 안착하도록 보정.
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
