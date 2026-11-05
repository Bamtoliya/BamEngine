# 에디터 워크플로 및 계층·뷰포트·인스펙터 상호작용 개선 개발 계획

## 1. 개요 및 배경

에디터 작업 흐름(Workflow)의 직관성과 조작 연속성을 높이기 위해, 다음 4가지 핵심 영역의 상호작용 및 단축키 처리 구조를 전면 개선한다.

1. **PIE(Play In Editor) 뷰 및 카메라 분리**:
   - 기존: PIE 시작 시 씬 뷰포트가 게임 런타임 카메라로 강제 전환되어 편집 시점을 잃고 디버깅 중 자유로운 카메라 이동이 제한됨.
   - 개선: PIE 중에도 씬 뷰(`Scene View`)는 항상 에디터 카메라(Orbit/Pan/Free-look)를 유지하여 실시간 씬 디버깅을 지원하고, 게임 화면은 `Game View`를 자동으로 활성화/포커스하여 렌더링하도록 분리. PIE 종료 시 `Scene View`로 포커스 자동 복원.

2. **계층 패널 ↔ 인스펙터 연계 강화**:
   - 기존: 계층 패널(`HierarchyPanel`)에서 엔티티를 클릭해도 다른 도킹 탭(TerrainEditor, PostProcess 등) 뒤에 가려진 인스펙터가 자동으로 전면 노출되지 않음.
   - 개선: 계층 패널에서 엔티티 클릭 시 인스펙터 창을 전면(맨 위 탭)으로 올리고 활성화(`Open()` & `Focus()`).
   - 스크롤 보존: 엔티티 전환 시 인스펙터 스크롤 위치를 유지/복원하는 기존 스크롤 맵(`m_EntityScrollMap`) 보존.

3. **계층 패널 컨텍스트 메뉴 및 빠른 조작(F/F2) 확장**:
   - 컨텍스트 메뉴 확장:
     - 씬 뷰에서 포커스 (Focus Selected, F)
     - 이름 변경 (Rename, F2)
     - 빈 자식 엔티티 추가 (Create Child Empty)
     - 복제 (Duplicate, Ctrl+D)
     - 루트로 이동 / 부모 해제 (Move to Root / Unparent)
   - 빠른 조작:
     - 계층 패널에서 엔티티 선택 후 `F` 키 입력 시 씬 뷰포트 카메라가 해당 엔티티로 포커스 이동.
     - `F2` 키 입력 시 트리 노드에서 인라인 텍스트 입력창 활성화.

4. **단축키 중앙화 원칙 준수 (ShortcutManager 일원화)**:
   - 기존: 각 패널의 `Draw()` 내부에서 `KEY_DOWN(EKeyCode::F2)`, `KEY_DOWN(EKeyCode::Delete)` 등으로 파편화되어 포커스 상태에 따라 단축키가 씹히거나 충돌 발생.
   - 개선: 모든 단축키를 `ShortcutManager`(`EditorActionRegistry`)의 고유 액션으로 등록하고 적절한 `EShortcutContext` 부여. UI 메뉴/툴팁의 단축키 힌트 또한 `ShortcutManager::Get().GetShortcutHint(...)`로 일원화.

---

## 2. 세부 설계 및 변경 사양

### 2.1 PIE 카메라/뷰 분리
- **`SceneViewportPanel`**:
  - `IsCameraInputAllowed()`에서 `!isPIE` 검사를 제거하여 PIE 실행 중에도 에디터 카메라 조작 허용.
  - PIE 중에도 `DrawImViewGuizmo` 및 `DrawCameraOverlay` 정상 렌더링 유지.
  - 더 이상 필요 없는 `FindActiveSceneCameraBuffer()` 헬퍼 제거.
- **`PIEManager`**:
  - `StartPlay()`: `Game View` 패널을 조회하여 `SetOpen(true)` 및 `Focus()` 호출.
  - `Stop()`: `Scene View` 패널을 조회하여 `SetOpen(true)` 및 `Focus()` 호출.
  - *패널 명칭 식별자 주의*: 에디터 패널 식별자는 `L"Scene View"` 및 `L"Game View"`임.

### 2.2 뷰포트 카메라 Focus 로직 개선
- **`EditorCameraController`**:
  - `FocusEntity()`에서 `distance >= 15.0f` 조건문 제거 (15유닛 이내의 오브젝트에 대해서도 정상 거리 계산 및 카메라 이동 수행).
  - 2D 오쏘그래픽 카메라에 대한 바운딩 박스 오쏘그래픽 사이즈 자동 조절 지원.
  - 위치 갱신 후 `RefreshMatrices()` 무조건 호출.

### 2.3 계층 패널 ↔ 인스펙터 포커스 연계
- **`BaseViewportPanel` / `InspectorPanel`**:
  - `m_FocusRequested` 플래그 및 `Focus()` API 지원 (`ImGui::SetNextWindowFocus()` 및 `ImGui::SetWindowFocus()` 탭 전면화).
- **`HierarchyPanel`**:
  - 엔티티 클릭 선택 처리 시 `InspectorPanel`을 찾아 `Open()` 및 `Focus()` 호출.
  - 기존 엔티티별 스크롤 기억 맵(`m_EntityScrollMap`)은 훼손하지 않음.

### 2.4 단축키 및 컨텍스트 메뉴 일원화
- **`EditorActionRegistry`**:
  - `Viewport.Focus`: `EShortcutContext::Global`. 현재 프리뷰·그래프·UI 캔버스 문맥에 우선 전달하고, 씬 대상으로는 활성 뷰포트 또는 `L"Scene View"`를 사용한다.
  - `Hierarchy.Rename`: `EShortcutContext::Global`, 기본 단축키 `F2`. 계층 패널의 `TriggerSelectedEntityRename()`을 호출하며 콘텐츠 브라우저는 로컬 `Edit.Rename`으로 분리한다.
- **`EntityHierarchyView`**:
  - `FocusEntity`, `CreateChild`, `Duplicate`, `Unparent`, `StartRename` 액션 바인딩.
  - 인라인 Rename 시 `ImGui::InputText` 제공 및 포커스 부여 (`ImGui::SetKeyboardFocusHere()`).
  - 단축키 힌트 표시는 `sm.GetShortcutHint(...)`를 통해 동적으로 조회.
- **`HierarchyPanel`**:
  - `Draw()` 및 이벤트 루프 내의 수동 `KEY_DOWN` 처리(F2, Delete 등) 제거 및 `ShortcutManager` 위임.

---

## 3. 관련 파일 및 영향 범위

| 파일 경로 | 주요 변경 내용 |
| :--- | :--- |
| `AGENTS.md` | 11절 "에디터 단축키의 ShortcutManager 등록 원칙" 추가 |
| `Docs/EngineEditorClientGuidelines.md` | 14절 에디터 단축키 중앙 관리 및 로캘 원칙 보강 |
| `Editor/Source/PIE/Private/PIEManager.cpp` | PIE 시작 시 Game View 포커스, 정지 시 Scene View 복원 |
| `Editor/Source/ImGui/ViewportPanel/Private/SceneViewportPanel.cpp` | PIE 중 에디터 카메라 유지 및 기즈모 활성화 |
| `Editor/Source/Application/Camera/Private/EditorCameraController.cpp` | `FocusEntity()` 15유닛 거리 제한 제거, 오쏘그래픽 지원 |
| `Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp` | `Focus()` 요청 시 도킹 탭 전면 전환 처리 |
| `Editor/Source/ImGui/HierarchyPanel/Private/EntityHierarchyView.cpp` | 컨텍스트 메뉴 확장, 인라인 Rename UI, 단축키 힌트 일원화 |
| `Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp` | 수동 `KEY_DOWN` 제거, 인스펙터 자동 전면화, 트리거 핸들러 |
| `Editor/Source/Shortcut/Private/EditorActionRegistry.cpp` | `Viewport.Focus` (Global), `Hierarchy.Rename` (F2) 등록 |

---

## 4. 검증 결과 및 테스트 시나리오

1. **PIE 카메라 동작**:
   - PIE(F5) 실행 시 `Game View` 탭이 자동으로 전면으로 뜨며 게임 실행이 표시됨.
   - `Scene View` 탭으로 전환했을 때 여전히 에디터 카메라로 자유로운 시점 이동(Orbit, Pan, Fly) 및 오브젝트 기즈모 디버깅이 가능함.
   - PIE 정지(Shift+F5) 시 `Scene View` 탭이 자동으로 전면으로 복원됨.
2. **계층 패널 포커스 및 인스펙터 연계**:
   - 다른 탭(Terrain, PostProcess 등) 뒤에 인스펙터가 숨겨진 상태에서 계층 패널의 엔티티를 클릭하면 인스펙터가 즉시 최상위 탭으로 노출됨.
   - 이전에 스크롤을 내려둔 컴포넌트 위치가 유지됨.
3. **F 키 포커스**:
   - 계층 패널에서 엔티티를 클릭하고 바로 `F` 키를 누르면 `Scene View`의 에디터 카메라가 해당 엔티티로 즉시 포커스 이동함.
4. **F2 키 인라인 리네임**:
   - 계층 패널에서 엔티티 선택 후 `F2`를 누르면 해당 노드에 인라인 텍스트 입력창이 열리고 엔터/포커스 아웃 시 이름이 즉시 반영됨 (Undo/Redo 지원).
5. **컨텍스트 메뉴**:
   - 계층 패널 엔티티 우클릭 시 Focus(F), Rename(F2), Create Child Empty, Duplicate(Ctrl+D), Move to Root 메뉴가 단축키 힌트와 함께 정상 작동함.


## 5. 2026-11-05 로캘·입력·단축키 보완 상태

이번 보완은 코드 반영과 자동 검증까지 수행했다. 4절의 조작 시나리오를 이번 변경에서 다시 통과한 것으로 해석하지 않는다.
자세한 변경·검증 명령·범위는 [ImplementationLog.md](ImplementationLog.md)의 같은 날짜 로캘·입력·단축키 기록을 따른다.

- 중앙 등록: 액션 68개를 대조했다. `Graph.AddNode`(기본 Space), `Graph.Validate`(기본 F7)를 등록하고 그래프 메뉴의 고정 키 문자열을 제거했다.
- 노드 에디터: `EnableShortcuts(false)`로 기본 Delete·F 등 재바인딩 우회를 차단한다. 삭제·정렬·복제·코멘트 액션은 자기 문서의 에디터 문맥으로 실행한다.
- 문맥: 같은 문맥의 바인딩만 충돌로 처리하며 로컬 액션을 우선한다. 비활성 로컬 액션의 키는 Global 명령으로 전달하지 않는다.
  텍스트 입력·키 수집·팝업 등 UI 문맥의 억제는 중앙 평가 경로를 사용한다.
- 입력·표시: InputManager 입력 샘플, 동적 `GetShortcutHint()`와 CSV 원본 번역을 적용한다.
  카메라 연속 이동·텍스트/드래그 취소·키 수집은 상태 처리이며, ImGui 위젯 반환값·호버·포커스는 UI 문맥으로 유지한다.
- 자동 검증: Editor Debug 빌드·`AuditLocalization` 및 관련 CTest 4개 통과. 감사기 회귀 테스트 14개 포함.
- 실제 UI 회귀 확인 대기:
  - 단축키를 변경·해제한 뒤 메뉴·툴팁·실행 키가 일치하는지 확인한다.
  - 그래프·애니메이터·타임라인·프리팹·스프라이트 편집 문맥에서 로컬 명령이 씬 저장/삭제로 넘어가지 않는지 확인한다.
  - 텍스트 입력·팝업·드래그 중 단축키 충돌과 언어 전환 시 편집 상태·ImGui ID 유지를 확인한다.
  - 그래프 캔버스 밖에서 시작하거나 끝난 우클릭이 메뉴를 열지 않는지 확인한다.
  - 분리 창·DPI별 타임라인 클릭/드래그/휠과 프리뷰·그래프·UI·씬의 포커스 대상을 확인한다.
