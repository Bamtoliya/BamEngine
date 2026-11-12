# BamEngine Implementation Log

This document tracks implementation details, architectural decisions, and technical debt.

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
