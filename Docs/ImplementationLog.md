# BamEngine Implementation Log

This document tracks implementation details, architectural decisions, and technical debt.

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
