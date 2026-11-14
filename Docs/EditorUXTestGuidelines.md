# 에디터 사용성(UX) 테스트 및 검증 기준

- 작성일: 2026-11-14
- 적용 범위: `Editor/` 전체 패널, 리소스 편집기, 모달·팝업, 공용 UI 위젯(`EditorUI`), 뷰포트 및 단축키/입력/다국어 경로
- 관련 구현 지침: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md) (10·14·16~19절), [ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md), [EditorWorkflowAndInteractionPlan.md](EditorWorkflowAndInteractionPlan.md)

## 1. 문서의 목적과 검증 원칙

`EngineEditorClientGuidelines.md`가 에디터 UI/UX의 **설계·구현 규칙**을 정의한다면, 이 문서는 해당 규칙이 실제 사용 흐름에서 깨지지 않았는지 판정하는 **테스트 계층, 테스트 환경 조건, Pass/Fail 기준 및 회귀 점검 절차**를 정의한다.

1. **구현 확인과 UX 검증의 구분**: 코드·스타일 스택·로캘 키 연결을 읽는 **정적 확인**만으로 UI/UX 검증 완료를 선언하지 않는다.
2. **자동 검사와 수동 확인의 분리**: 레이아웃 사각형 계산, 단축키 문맥 격리, 문서 보호 상태 머신처럼 GPU 없이 판정할 수 있는 불변 조건은 **헤드리스 CTest**로 고정하고, 실환경 수동 검증은 렌더링·OS 창 이벤트·복합 워크플로에 집중한다.
3. **경계 조건 우선 검증**: 기본 창 크기(`1920x1080`, `1.0x`, 단일 언어)만 확인하고 통과로 간주하지 않는다. 언어 전환, UI 배율 확대, 좁은 도킹 폭, 저장 실패·취소 경로를 기본 테스트 조건에 포함한다.

---

## 2. 검증 3계층 (Verification Tiers)

| 계층 | 검증 방식 | 주요 검증 대상 | 관련 타깃 / 도구 |
| :--- | :--- | :--- | :--- |
| **Tier 1. 정적·데이터 무결성 감사** | 빌드 파이프라인 / 스크립트 검사 | - `Editor.csv` / `Engine.csv` 누락 키, 빈 번역, 중복 키, `Editor.csv` 명시적 Group 규칙<br>- `ImGui::*` / `AddNotification` / `AddText` 내 하드코딩 UI 문자열<br>- `ImGui::IsKey*`, `IsMouse*`, `GetMousePos*`, `io.Key*`/`Mouse*` 등 원시 입력 직접 조회<br>- 리터럴 액션 ID 미등록 여부 및 `IImGui` 파생 패널의 `m_DisplayNameKey`/`GetDisplayName` 누락 여부 | `GenerateLocalization`, `AuditLocalization` (`Tools/Audit/LocalizationAuditor/audit_localization.py --strict`), `LocalizationAuditUtility` (CTest) |
| **Tier 2. 헤드리스 UI·상호작용 자동 검사** | 창·GPU 없이 `ImGui::CreateContext()`와 합성 입력(`io.AddMouse*`, `AddKeyEvent`, `InputManager`)으로 실행 (CTest) | - 배율별(`0.75x`~`2.0x`) 및 한·영 전환 후 메뉴바 첫 클릭·드래그 영역 분리<br>- 단축키 문맥 우선순위, 비활성 로컬 명령의 `Global` 폴백 차단, 텍스트 입력/키 캡처 억제, 동적 `GetShortcutHint()`<br>- 무입력 `Draw` 불변성, 언어 전환 후 카테고리 상태 유지, 드래그 앤 드롭 타입 안전성, 읽기 전용 차단<br>- 계층 트리 클릭·드래그 부모 변경·교차 씬 드롭 차단, `StateId` 기반 Undo 기준점·미저장 보호 | `MainMenuBarLayoutCheck`, `ShortcutManagerCheck`, `ReflectionInspectorCheck`, `HierarchyEditorCheck`, `EditorHistoryCheck`, `CanvasViewCheck`, `GraphMessageLocalizationCheck` |
| **Tier 3. 실환경 대화형·GPU·OS 통합 검증** | 실제 `Editor` 실행 후 시각·조작·OS 연동 수동 검증 | - SDL3 네이티브 창 드래그, 최소화/최대화/복원, 다중 모니터 DPI 이동, OS 파일 다이얼로그<br>- 3D 뷰포트/프리뷰어 렌더링(`ViewportDeferredPipeline`, G-Buffer 채널 뷰, `EditorCameraController`, `ImGuizmo`)<br>- 오버레이 클릭 관통 방지, 비동기 썸네일 업로드 예산 및 대량 폴더 프레임 시간<br>- PIE(`F5`/`Shift+F5`) 시점 분리 및 재실행 시 `Workspace.json` / `EditorPreferences.json` / `EditorStyle.json` 복원 | 5절 실환경 회귀 점검 시트 |

> 참고: `GenerateLocalization`의 원본 카탈로그는 [Editor.csv](../Editor/Resources/Data/Localization/Editor.csv)와 [Engine.csv](../Engine/Resources/Data/Localization/Engine.csv)이며, 번역본 JSON(`English.json`, `Korean.json`)은 직접 수정하지 않고 빌드 단계에서 동기화한다.

---

## 3. 표준 테스트 환경 매트릭스 (Standard Test Matrix)

모든 신규 패널, 공용 위젯 수정 및 레이아웃 개편은 아래 **표준 경계 조건 조합**을 기준으로 검증한다.

1. **언어 전환 (`Korean` ↔ `English`)**:
   - 단어 길이가 긴 영문(`English`)과 조사·복합어가 결합되는 국문(`Korean`)을 모두 확인한다.
   - 에디터 실행 도중 상단 메뉴 또는 설정에서 언어를 최소 2회 왕복 전환하여 캐시·ID·입력 상태 누수를 확인한다.
2. **UI 배율 및 글꼴/여백 (`Scale & Font`)**:
   - `1.0x` (100%, 기준), `1.5x` (150%, 고해상도 표준), `2.0x` (200%, 고배율 경계).
   - 글꼴 크기와 `FramePadding`을 변경한 상태 및 `EditorStyle.json` 저장 후 재실행 시 Unscaled Base Style SSOT가 유지되는지(배율 중복 곱셈 없음) 확인한다.
3. **패널 도킹 폭 경계 (`Docking Width`)**:
   - **표준 폭 (`400px ~ 800px`)**: 일반 도킹 상태에서 정렬, 열 비율, 툴바 구성을 확인한다.
   - **좁은 도킹 폭 (`240px ~ 300px`)**: 인스펙터, 계층 패널, 리소스 편집기 사이드바의 최소 실사용 폭에서 툴바 줄바꿈, 아이콘 축약, 오버플로 툴팁, 스플리터 클램핑을 확인한다.
   - **극한 축소 폭 (`< 180px`)**: 아이콘 하나보다 좁아진 극한 폭에서도 크래시, 음수 크기 Assert, 가로 무한 팽창이 발생하지 않는지 확인한다(일부 표시 생략은 허용 한계로 기록).

---

## 4. 6대 핵심 UX 영역별 Pass/Fail 기준

### 4.1 레이아웃 적응성 및 시각적 무결성 (지침 17·18절)

| 항목 ID | 테스트 조건 및 조작 | 통과(Pass) 기준 | 실패(Fail) 징후 |
| :--- | :--- | :--- | :--- |
| **UX-1.1 버튼·배지 글리프 수용** | 배율 `1.0x`/`1.5x`/`2.0x` 및 한·영 전환 상태에서 `IconButton`, `SmallIconButton`, 타입 배지, 읽기 전용 자물쇠 배지 확인 | 현재 글꼴 측정값(`CalcTextSize`) + 스타일 패딩이 배경 사각형 안에 완전히 포함되고, 시각적 사각형과 클릭 히트 영역이 일치함 | 글리프/텍스트가 캡슐·버튼 밖으로 삐져나오거나(`18x18` 등 고정 크기 잔류), 클릭 판정 위치가 어긋남 |
| **UX-1.2 반응형 툴바 줄바꿈** | `ToolbarLayout` 적용 편집기(12종+)와 뷰포트 상단 바의 패널 폭을 `800px` → `260px`로 축소 | 가용 폭 부족 시 우측 버튼이 잘리지 않고 다음 줄로 개행되거나 아이콘+툴팁으로 전환됨. 줄바꿈 시 하단 캔버스/프리뷰 시작 Y좌표가 늘어난 높이만큼 내려감 | 우측 버튼이 창 밖으로 잘려 클릭할 수 없거나, 툴바가 아래 프리뷰/캔버스 위를 덮음 |
| **UX-1.3 속성 테이블 및 오버플로 툴팁** | `BeginPropertyTable` 패널 폭을 `260px`로 줄이고 긴 라벨에 마우스 호버 | 라벨 열(`WidthFixed`)이 우측 예약 버튼(`↺`, 잠금 등)을 침범하지 않고, 실제 가용 폭 초과(`CalcTextSize().x > availWidth`) 시에만 호버 툴팁이 표시됨. 값 열은 `-FLT_MIN` 또는 예약 폭 차감으로 우측 끝까지 채움 | 라벨과 입력 위젯이 겹치거나, 고정 오프셋(`-65.0f` 등) 때문에 `2.0x` 배율에서 우측 버튼이 잘림 |
| **UX-1.4 스플리터 클램프 및 선호 폭 보존** | 스플리터가 있는 창(`ModelEditor`, `ModelImportModal`, `ConsolePanel`, `UnifiedSettingsPanel`)에서 ① 경계 끝까지 드래그, ② 창 축소 후 다시 확대 | ① 양쪽 영역이 모두 `minSize` 이상을 유지해 사라지지 않음.<br>② 창 축소 중 표시용 클램프가 저장용 선호 폭(`preferredWidth`/`Height`)을 덮어쓰지 않아 창을 다시 넓히면 원래 비율로 복원됨 | 한쪽 영역이 `0px`가 되어 복구할 수 없거나, 창을 잠깐 줄였다 키웠을 때 사이드바가 최소 크기로 고정됨 |

### 4.2 입력·포커스·단축키 일관성 (지침 14절)

| 항목 ID | 테스트 조건 및 조작 | 통과(Pass) 기준 | 실패(Fail) 징후 |
| :--- | :--- | :--- | :--- |
| **UX-2.1 단축키 문맥 우선순위 및 격리** | 로컬 문맥(`GraphEditor`, `AnimatorGraph`, `AnimationTimeline`, `Prefab` 등) 활성 상태에서 공통 키(`Delete`, `Ctrl+S`, `F`, `F2`) 입력 (활성/비활성 상태 모두) | 1) 로컬 액션 `CanExecute == true`: 로컬 액션만 실행되고 `Global` 미실행.<br>2) 로컬 액션 `CanExecute == false`: 입력을 소비하고 **`Global` 명령으로 폴백하지 않음** (예: 그래프 노드 미선택 시 `Delete`로 씬 엔티티가 삭제되지 않음) | 그래프나 타임라인 작업 중 누른 `Delete`나 `Ctrl+S`가 뒤쪽 씬 엔티티를 삭제하거나 씬을 저장함 |
| **UX-2.2 UI 입력 문맥 충돌 억제** | ① `InputText` 편집 중, ② 단축키 설정 키 캡처 중, ③ 모달 팝업 활성 중에 단축키(`F`, `F2`, `Delete`, `Ctrl+D`, `Space` 등) 입력 | `ShortcutManager` 중앙 평가 경로가 `WantTextInput`·키 캡처·모달 문맥을 감지해 에디터 단축키 실행을 억제함 | 엔티티/노드 이름에 `f`나 `w`를 타이핑하는 도중 뷰포트 카메라가 이동하거나 기즈모 모드가 바뀜 |
| **UX-2.3 동적 단축키 힌트 동기화** | `UnifiedSettingsPanel > Shortcuts`에서 단축키 재바인딩 또는 바인딩 해제 후 메뉴·툴팁·컨텍스트 메뉴 확인 | 재시작 없이 변경된 키 문자열이 표시되고, 바인딩 해제 시 빈 괄호 `()` 없이 힌트가 생략됨. 같은 문맥 중복 시 충돌 경고 표시 | 메뉴나 툴팁에 하드코딩된 구형 단축키 문자열이 그대로 남아 있음 |
| **UX-2.4 오버레이·캔버스 경계 차단** | ① 3D 뷰포트/프리뷰어 오버레이 버튼(RGBA, 2D/3D, 조명, 그리드) 위에서 클릭/드래그<br>② 그래프 캔버스 밖에서 시작해 안에서 끝난 우클릭(및 반대) | ① 오버레이 위에서 시작한 입력은 카메라 Orbit/Pan/Fly나 기즈모 조작으로 전달되지 않음.<br>② 캔버스 내부에서 시작하고 끝난 우클릭만 컨텍스트 메뉴를 엶 | 오버레이 버튼 클릭 시 뒤쪽 3D 카메라가 회전하거나, 창 밖 우클릭 드래그 끝에 노드 메뉴가 열림 |

### 4.3 공용 UI 패턴 동작 일관성 (지침 19절)

| 항목 ID | 테스트 조건 및 조작 | 통과(Pass) 기준 | 실패(Fail) 징후 |
| :--- | :--- | :--- | :--- |
| **UX-3.1 검색 바 (`DrawSearchBar`)** | 적용 화면(플러그인, 계층, 콘솔, 리소스 검사, 에셋 참조, 노드 추가, 엔티티/리소스 피커, 콘텐츠 브라우저, 통합 설정)에서 입력·`[X]`·`Escape` 확인 | ① 팝업 오픈 시 첫 포커스가 아이콘이 아닌 입력창에 위치함.<br>② 빈 문자열일 때 `[X]` 버튼은 비활성화되나 레이아웃 자리를 유지함.<br>③ 입력창 포커스 중 `Escape` 1회 입력 시 팝업을 닫지 않고 검색어만 비우고 포커스를 해제함 | `Escape`를 눌렀을 때 검색어 초기화와 함께 리소스 피커/노드 추가 팝업까지 한꺼번에 닫힘 |
| **UX-3.2 모달 대화상자 (`DialogButtonBar`)** | 19개 모달/팝업에서 버튼 배치·색상·`Enter`/`Escape` 확인 (좁은 창 줄바꿈 포함) | ① 하단 우측 `[취소] ... [확인]` 순서 및 파괴적 동작의 적색 강조 일치.<br>② `CancelIndex = -1`(크래시 복원 모달 등)에서는 `Escape`가 파괴적 첫 버튼을 실행하지 않음.<br>③ 확인 버튼 비활성(`Enabled == false`) 또는 `InputText` 편집 중일 때 `Enter`가 모달을 확정하지 않음 | 복원 창에서 누른 `Escape`로 백업 스냅샷이 폐기되거나, 잘못된 이름/경로로 확인 버튼이 비활성인데 `Enter`로 통과됨 |
| **UX-3.3 리스트·트리·그리드 다중 선택** | `HierarchyPanel`, `ContentBrowserPanel`, `ResourcePicker`에서 클릭, 더블클릭, `Ctrl`/`Shift` 클릭, 박스 드래그, 우클릭 확인 | - 이미 선택된 다중 항목 위에서 우클릭/드래그 시 **다중 선택 전체를 유지**함.<br>- 선택 밖 항목에서 우클릭/드래그 시 **해당 단일 항목으로 선택 전환**.<br>- 스크롤로 기준 항목이 클리퍼 밖으로 벗어나도 `Shift+클릭` 범위 선택이 유지됨 | 다중 선택 후 우클릭 메뉴를 열 때 선택이 1개로 풀리거나, 가상 스크롤 밖 기준 항목과의 범위 선택이 깨짐 |
| **UX-3.4 빈 상태 안내 (`EmptyState`)** | 빈 폴더, 검색 결과 0건, 지형 미선택, 빈 필름스트립 조회 | 가용 영역 정중앙에 비활성 회색 안내 문구가 표시되고 좁은 폭에서 줄바꿈됨. 검색 결과 0건일 때 임의로 전체 목록으로 복귀하지 않음 | 로딩 실패인지 결과 0건인지 알 수 없는 빈 공간만 남거나, 검색 결과 0건인데 폴더 전체 목록이 대신 표시됨 |

### 4.4 편집 데이터 안전성 및 문서 수명 보호 (지침 10절 · P1-A)

| 항목 ID | 테스트 조건 및 조작 | 통과(Pass) 기준 | 실패(Fail) 징후 |
| :--- | :--- | :--- | :--- |
| **UX-4.1 무입력 Draw 불변성** | 모든 컴포넌트 인스펙터와 리소스 편집기를 열고 입력 없이 렌더링 및 언어 전환 수행 | 단순 표시나 언어 전환만으로 컴포넌트 값이나 문서의 `HasUnsavedChanges()`가 `true`가 되지 않음 | 편집기를 열기만 했는데 반올림 오차나 초기 대입 때문에 `*`(미저장) 표시가 켜짐 |
| **UX-4.2 연속 조작 단일 Undo 병합** | 기즈모 드래그, 슬라이더 드래그, 스플라인 제어점 이동, 타일맵 브러시 드래그 후 `Ctrl+Z` 1회 실행 | 누름~뗌 사이의 연속 변경이 단일 제스처(`Flush`)로 묶여 **`Ctrl+Z` 1회**로 조작 전 상태까지 복원됨 | 드래그 1회에 수십 개의 Undo 명령이 쌓여 `Ctrl+Z`를 여러 번 눌러야 복원됨 |
| **UX-4.3 저장 기준점(`StateId`) 일치** | ① 편집(`*`) → `Ctrl+Z`(`*` 해제)<br>② 편집 후 저장 → `Ctrl+Z`(`*`) → `Ctrl+Y`(`*` 해제)<br>③ Undo 후 새 편집 분기 | 명령 개수가 아니라 고유 `StateId`를 저장 기준점과 비교해, 저장 상태와 정확히 일치할 때만 `*` 표시와 종료 확인이 사라짐 | Undo 후 분기 편집으로 명령 개수만 같아졌을 때 저장된 상태로 오판함 |
| **UX-4.4 미저장 교체 차단 및 저장 실패 보존** | 씬, 프리팹, 애니메이터, 그래프 등 미저장 상태에서 `New`/`Open`/`Close`/종료 시도 후 ① `Cancel`, ② 저장 실패(읽기 전용/디렉터리 경로) 유도 | ① `Cancel` 시 교체·종료가 즉시 중단됨.<br>② `Save` 실패 시 오류를 알리고 **교체·종료를 진행하지 않은 채** 기존 초안·파일·`*` 상태를 보존함 | 저장이 실패했는데도 창이 닫히거나 다음 파일로 넘어가 편집 내용이 유실됨 |
| **UX-4.5 읽기 전용 보호** | `Engine://` 빌트인 리소스, `const` 객체, `ReadOnly` 메타데이터, `EditCondition` 미충족 속성에 편집·드롭·리네임·삭제 시도 | UI에서 잠금 배지/비활성 상태로 표시되며, 드롭 페이로드나 강제 입력이 와도 원본 데이터가 변경되지 않음 | 드래그 앤 드롭이나 `F2`/`Delete`로 읽기 전용 엔진 리소스 또는 잠긴 속성이 변경/삭제됨 |

### 4.5 문맥 전환 및 영속 상태 일관성

| 항목 ID | 테스트 조건 및 조작 | 통과(Pass) 기준 | 실패(Fail) 징후 |
| :--- | :--- | :--- | :--- |
| **UX-5.1 실시간 언어 전환 상태 유지** | 엔티티 선택, 인스펙터 카테고리 접힘, 리소스 편집기 도킹, 설정 미적용 초안 유지 상태에서 언어 전환 | 첫 클릭에 메뉴가 열리고, 도킹 배치·열린 창·카테고리 접힘·엔티티 선택·초안 값이 모두 유지됨 | 창 제목이나 라벨 문자열 변경으로 ImGui ID가 달라져 도킹이 풀리거나 UI 상태가 초기화됨 |
| **UX-5.2 PIE 진입·디버깅·종료 복귀** | `F5`(Play) → `Scene View` 탭 전환 후 카메라/기즈모 조작 → `Shift+F5`(Stop) | Play 시 `Game View`가 전면화되고, PIE 중에도 `Scene View` 에디터 카메라와 기즈모가 동작하며, Stop 시 `Scene View`로 복귀하고 런타임 변경이 편집 씬에 남지 않음 | Play 시 `Scene View` 시점이 잠기거나, Stop 후 런타임 상태가 편집 씬·Undo 이력에 잔류함 |
| **UX-5.3 계층 선택↔인스펙터 연계** | 인스펙터가 다른 탭 뒤에 숨겨진 상태에서 `HierarchyPanel`의 엔티티 A(스크롤 하단) → B → A 순으로 클릭 | 클릭 즉시 `InspectorPanel` 탭이 최상위로 전면화(`Focus()`)되고, 엔티티별 이전 스크롤 위치(`m_EntityScrollMap`)가 유지됨 | 엔티티를 클릭해도 가려진 인스펙터 탭이 올라오지 않거나 스크롤이 매번 최상단으로 초기화됨 |
| **UX-5.4 재실행 설정·레이아웃 복원** | 언어, UI 배율, 창 크기/전체화면(15초 확인 포함), 뷰포트 해상도·패스 토글, 단축키, `EditorStyle.json` 변경 후 에디터 재실행 | 모든 설정이 각 소유자(`EditorPreferences.json`, `Workspace.json`, `EditorShortcuts.json`, `EditorStyle.json`) 경로에서 누락 없이 복원됨 | 재실행 시 배율이 누적 왜곡되거나 뷰포트 해상도·도킹·단축키가 기본값으로 초기화됨 |

### 4.6 반응성 및 프레임 비용 예산 (ImGui 계획 14-D)

| 항목 ID | 테스트 조건 및 조작 | 통과(Pass) 기준 | 실패(Fail) 징후 |
| :--- | :--- | :--- | :--- |
| **UX-6.1 유휴 상태 I/O 제로** | `ResourcePicker` 팝업 또는 `ContentBrowserPanel`을 열어두고 마우스만 이동 | 팝업 진입 1회(또는 파일 감시 이벤트 시)만 디스크 스캔/캐싱하며, 유휴 프레임마다 디스크 순회나 Archive 헤더 파싱(`PeekResourceType`)을 반복하지 않음 | 팝업을 열어둔 채로 프레임 시간이 급증하거나 디스크 I/O가 매 프레임 발생함 |
| **UX-6.2 클리핑 및 업로드 예산** | 200개 이상 스프라이트/텍스처 폴더 탐색 및 수천 줄 `ConsolePanel` 스크롤 | `ImGuiListClipper`로 가시 영역만 제출, 썸네일 프레임당 최대 4개 업로드·캐시 상한(256개 / 64MB) 유지. 콘솔 멀티라인 로그가 행 높이를 깨지 않고 하단 상세 뷰에서만 줄바꿈 전문 표시 | 대량 폴더 진입 시 수백 개 썸네일 동시 업로드로 에디터가 멈칫하거나(Hitch), 긴 로그로 콘솔 스크롤바가 튐 |

---

## 5. 미검증 공용 UI 실환경 회귀 점검 시트

2026-11-05~11-09에 정적 반영된 공용 UI·단축키 변경을 Windows Debug/Release `Editor` 실행 환경에서 확인하는 표준 점검표다.

### 5.1 빌드 및 Tier 1·2 사전 확인 명령 (사용자 실행)

```powershell
cmake --preset x64-debug -DBUILD_TESTING=ON
cmake --build out/build/x64-debug --config Debug --target Editor AuditLocalization ReflectionInspectorCheck MainMenuBarLayoutCheck ShortcutManagerCheck HierarchyEditorCheck EditorHistoryCheck CanvasViewCheck GraphMessageLocalizationCheck
ctest --test-dir out/build/x64-debug -C Debug --output-on-failure -R "LocalizationAuditUtility|MainMenuBarLayoutCheck|ShortcutManagerCheck|ReflectionInspectorCheck|HierarchyEditorCheck|EditorHistoryCheck|CanvasViewCheck|GraphMessageLocalizationCheck"
```

### 5.2 Tier 3 대화형 UI 점검 체크리스트

- **A. 타이틀바 · 주 메뉴 · 하단 푸터 바**
  - [ ] 주 메뉴 폭을 줄였을 때 메뉴 접힘과 창 제어 3버튼(최소화·최대화·닫기)의 축약 메뉴가 겹치지 않고, 제목 표시줄 빈 영역 드래그·더블클릭 최대화가 정확히 동작한다.
  - [ ] 한국어↔영어 전환 및 배율 `1.0x`/`1.5x`/`2.0x` 변경 직후 `Help` 등 상단 메뉴가 첫 클릭에 열린다.
  - [ ] 상단 Play Bar에는 `Play`/`Pause`/`Stop` 3버튼만 남고, 하단 Footer Bar 5종 버튼(컴파일·자동 컴파일·핫리로드·자동 리로드·셰이더 리로드)과 우하단 토스트 알림이 겹치지 않는다.
- **B. `ResourceEditorInterface` 3줄 공통 헤더 (리소스 편집기 12종)**
  - [ ] 머티리얼·모델·파티클·VFX·타임라인·스프라이트·애니메이터·그래프 등 편집기를 좁은 도킹 폭(`260px`)으로 줄였을 때 메뉴 접힘과 퀵액세스 줄바꿈이 겹침 없이 동작하고 하단 프리뷰가 가려지지 않는다.
  - [ ] 리소스 전환 및 언어 전환 뒤에도 도킹 탭 위치가 풀리지 않으며, 파일 경로가 없는 리소스에서는 경로 복사 버튼이 비활성화된다.
  - [ ] 뷰포트/프리뷰어 우측 상단 RGBA·2D/3D 오버레이 클릭 시 뒤쪽 3D 카메라나 기즈모가 움직이지 않는다.
- **C. `EditorUI::DialogButtonBar` 모달 및 팝업**
  - [ ] 크래시 복원 모달(`DrawRecoveryModal`)에서 `Escape`를 눌렀을 때 첫 번째 버튼(스냅샷 폐기)이 실행되지 않는다.
  - [ ] 파일 충돌 모달의 `[건너뛰기] [덮어쓰기(적색)] [복사본 만들기]` 및 일괄 처리 흐름이 유지된다.
  - [ ] 디스플레이 해상도 변경 시 15초 프로그레스 바와 자동 복구가 동작하고, 미저장 씬/프리팹/애니메이터 닫기에서 저장 실패 시 창과 초안이 유지된다.
- **D. `DrawSearchBar` · `ToolbarLayout` · 행 내부 `SmallIconButton`**
  - [ ] 노드 추가·리소스 피커·엔티티 선택 팝업을 열었을 때 입력창에 첫 포커스가 잡히고, 검색창에서 `Escape` 1회 입력 시 팝업은 열린 채 검색어만 비워진다.
  - [ ] 스프라이트 프레임·애니메이터 조건·머티리얼 슬롯 행에서 `SmallIconButton` 패딩이 배율 `2.0x`에서도 유지되고, 첫/마지막 프레임 이동 버튼이 비활성화된다.
  - [ ] 좁은 도킹 폭에서 `EmptyState` 문구가 중앙 정렬·줄바꿈되며 데이터가 생기면 즉시 목록으로 복귀한다.
- **E. 콘텐츠 브라우저 (`Assets/` vs `Engine/Resources/`) 및 다중 조작**
  - [ ] `Engine://` 항목에 글꼴 기반 크기의 자물쇠 배지가 표시되고 수정·삭제·리네임·드롭이 차단된다.
  - [ ] 다중 타일 선택 후 우클릭 경로 복사가 줄 단위로 복사되고, 씬 뷰포트 다중 드롭 시 단일 Undo(`Ctrl+Z`)로 일괄 제거된다.
  - [ ] `Tilemap` 등 200개 이상 스프라이트 폴더를 연 상태에서 스크롤·검색·범위 전환 시 체감 프레임 저하가 없고, `Logs/Editor.log`에 오류나 누락 키 경고가 남지 않는다.

---

## 6. Tier 2 헤드리스 자동 검사 확장 후보 (`EditorUICheck`)

현재 `Editor/examples/`의 검사는 메뉴바·단축키·인스펙터·계층·문서 이력을 다루며, 공용 위젯 집합(`EditorUI`) 자체의 불변 조건을 검증하는 전용 타깃은 아직 없다. 향후 `Editor/examples/EditorUICheck.cpp`를 신설할 때는 다음 4가지 계약을 CTest로 고정한다.

1. **`DialogButtonBar` 계약**: 우측 정렬 및 좁은 폭 세로 줄바꿈 시 `GetDialogButtonBarHeight()`와 실제 렌더링 높이 일치 여부, `CancelIndex = -1` 시 `Escape` 무반응, 비활성 확인 버튼 및 `InputText` 활성 중 `Enter` 무시.
2. **`DrawSearchBar` 계약**: `FocusOnDraw = true` 첫 프레임 입력 포커스 획득, 활성 상태에서 `Escape` 입력 시 `Cleared == true` 반환 및 버퍼 초기화, 빈 버퍼 시 `[X]` 비활성 상태 유지.
3. **`IconButton` / `SmallIconButton` 배율 계약**: 배율 `1.0x`/`1.5x`/`2.0x`에서 `GetItemRectSize()`가 글리프 측정 크기 + 최소 패딩 이상을 항상 만족하는지 검사.
4. **`ToolbarLayout` 폭 경계 계약**: 좁은 창 폭(`220px`)에서도 모든 자식 위젯의 `GetItemRectMax().x`가 창 오른쪽 경계(`WindowPos.x + WindowWidth`)를 초과하지 않고 다음 줄로 개행되는지 검사.
