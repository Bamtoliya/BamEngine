# 게임 UI 위젯·레이아웃·마스킹 개발 계획

- 범위 · BamEngine의 게임용 Screen Space Canvas, UI 뷰, 관련 프리팹과 독립 Windows Client
- 상태 · 1·2단계와 3단계의 사각 클리핑·휠 스크롤 코드가 있다. 스크롤바와 4단계는 개발 계획이다. 실제 편집·게임 실행은 검증 대기다.
- 기존 계약 · [UI 편집기](UIEditorFoundation.md) · [입력](UIInteraction.md) · [렌더 순서](UIRenderOrder.md) · [엔진 개발 계획](EngineDevelopmentPlan.md)

## 1. 기본 제공 기능 조사

- **Unity uGUI.** Image·Text와 함께 Button·Toggle·Slider·Scrollbar·Dropdown·Input Field·Scroll Rect를 제공한다. Horizontal/Vertical Layout Group으로 한 줄 또는 한 열을 배치하고, Grid Layout Group으로 고정 크기 셀을 채운다. 화면 평면의 사각형 클리핑은 RectMask2D, 스텐실을 쓰는 모양 마스크는 Mask가 맡는다. [UI 레퍼런스](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/UIReference.html) · [입력 위젯](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/comp-UIInteraction.html) · [Grid Layout Group](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/script-GridLayoutGroup.html) · [RectMask2D](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/script-RectMask2D.html) · [Mask](https://docs.unity3d.com/Packages/com.unity.ugui@2.0/manual/script-Mask.html)
- **Unreal UMG.** Button·Check Box·Slider·Progress Bar·Combo Box·Text Box를 제공한다. Canvas Panel은 앵커 기반 자유 배치, Horizontal/Vertical Box는 일렬 배치, Grid/Uniform Grid는 행·열, Overlay는 중첩, Scroll Box는 넘치는 콘텐츠를 다룬다. Grid의 행·열 같은 자식별 배치 값은 Slot이 소유한다. 클리핑은 상속, 교집합 및 필요할 때 적용하는 모드를 제공하며 클리핑 구역이 다르면 배칭에 비용이 생긴다. [UMG 위젯 목록](https://dev.epicgames.com/documentation/en-us/unreal-engine/widget-type-reference?application_version=4.27) · [UMG 슬롯](https://dev.epicgames.com/documentation/unreal-engine/umg-slots-in-unreal-engine) · [클리핑](https://dev.epicgames.com/documentation/unreal-engine/clipping-for-umg-widgets-in-unreal-engine)
- **Godot Control.** 버튼·체크박스·슬라이더·진행률·입력 필드를 Control 트리에 놓는다. HBox/VBox는 가로·세로 배치, GridContainer는 지정된 열 수, Margin/Center/Flow는 패딩·중앙 정렬·줄바꿈을 맡는다. ScrollContainer는 스크롤을 제공하고, Control의 `clip_contents`는 부모 경계 밖 자식의 그리기와 입력을 제한한다. [Control API](https://docs.godotengine.org/en/stable/classes/class_control.html) · [컨테이너 사용법](https://docs.godotengine.org/en/stable/tutorials/ui/gui_containers.html) · [GridContainer](https://docs.godotengine.org/en/stable/classes/class_gridcontainer.html) · [ScrollContainer](https://docs.godotengine.org/en/stable/classes/class_scrollcontainer.html)

## 2. BamEngine의 현재 기반

- `CanvasComponent`와 `RectTransformComponent`는 기준 해상도, 앵커·크기·피벗·변환을 저장한다. `UILayout`은 계층을 순회해 표시 사각형을 계산한다. UI 뷰의 그리드·스냅 기능은 저작 보조 도구다.
- `UIImageComponent`, `UITextComponent`, `UIButtonComponent`, `UIRenderOrderComponent`가 있다. 버튼 클릭과 키보드 실행은 `UIEventSystem`과 `RuntimeUIInput`을 이용한다. 현재 런타임 이벤트의 중심은 버튼 포인터 처리와 키보드 순환이다.
- 캔버스 메뉴의 **UI 프리셋**에서 패널·가로 행·세로 컬럼·그리드, 진행률 바·토글·슬라이더, 클리핑 패널·스크롤 뷰를 생성한다. 레이아웃의 항목 순서는 계층 순서를 사용하고 그리기 Z 순서는 독립적으로 유지한다. [UI 소스](../Engine/Source/UI) · [UI 뷰](../Editor/Source/ImGui/ViewportPanel/Private/UIViewportPanel.cpp)

## 3. 구현 순서와 완료 기준

### 1단계 · Row·Column·Grid와 시각 프리셋

구현 상태 · 기본 배치와 에디터 생성 메뉴를 반영했다. 레이아웃이 배치하는 자식은 UI 뷰에서 선택할 수 있으며, 드래그 이동과 모서리 리사이즈는 컨테이너가 관리하는 동안 제한된다.

- UI 뷰의 생성 메뉴에서 빈 패널, 가로 행, 세로 컬럼, 균일 그리드를 추가한다. 패널은 기존 이미지와 RectTransform을 재사용한다. 컨테이너 옵션은 패딩·간격·정렬이며 첫 Grid는 열 수와 고정 셀 크기를 제공한다. 자식별 Span·비율·자동 줄바꿈은 배치 요구가 확인된 뒤 추가한다.
- `UILayout`에서 자식 배치 결과를 매번 계산한다. 결과 좌표를 저작된 RectTransform에 매 프레임 덮어쓰면 실행·저장·Undo 데이터가 섞이므로, 저장하는 값은 컨테이너 설정과 필요한 자식별 배치 설정으로 둔다. 컨테이너가 관리하는 축은 UI 뷰와 인스펙터에서 표시한다.
- 완료 기준은 설정 메뉴의 세로 버튼 목록과 인벤토리 4열 그리드를 코드 수정 없이 만드는 것이다. 자식 재정렬·패딩 변경·Undo 후 배치가 안정적이고, 프리팹을 인스턴스화해도 같은 순서로 배치되어야 한다.

### 2단계 · Progress Bar·Toggle·Slider

구현 상태 · 진행률 표시, 토글 클릭·키보드 실행, 슬라이더 드래그·키보드 화살표, 런타임 값 변경 이벤트를 구현했다.

- Progress Bar는 게임 값을 표시한다. Toggle은 켬·끔 상태와 변경 이벤트를 제공한다. Slider는 최솟값·최댓값·간격이 있는 연속 또는 단계형 입력이다. 배경·채움·핸들은 기존 Image와 RectTransform을 합성한 생성 프리셋으로 배치한다.
- Slider는 기존 Canvas 좌표 변환과 포인터 캡처를 활용해 드래그한다. 키보드 조정 액션은 InputManager의 GameUI 문맥에 연결한다. Toggle과 Slider의 변경 이벤트는 값이 실제로 바뀔 때 한 번 발생하고, 이전 값을 다시 넣거나 요소가 비활성이면 발생하지 않는다.
- 완료 기준은 오디오 볼륨 슬라이더, 체크박스와 HP 바를 구성하는 것이다. 최솟값·최댓값·간격, 버튼 밖에서 놓기, 비활성화·포커스 이탈, PIE·Windows Client 결과를 확인한다.

### 3단계 · 사각 클리핑과 Scroll View

구현 상태 · 중첩 사각 클리핑과 한 개의 콘텐츠 자식을 이동하는 세로 휠 스크롤을 구현했다. 에디터 UI 뷰는 **상호작용** 모드에서 스크롤을 시도한다. 스크롤바 생성과 드래그는 후속 범위다.

- 부모 클리핑 영역을 자식에게 전달하고 중첩 영역은 교집합으로 계산한다. 첫 범위는 Canvas 축에 정렬된 사각형이다. 같은 유효 클립을 텍스트·이미지 제출, 에디터 프리뷰와 런타임 버튼 히트 테스트에 반영한다. 첫 스크롤 컨테이너는 세로 이동, 휠과 스크롤바 단일 축을 제공한다.
- 렌더러는 Canvas 클립 좌표를 각 뷰포트의 실제 출력 픽셀로 변환한다. 현재 D3D12의 SetViewport는 viewport와 scissor를 함께 바꾸므로, 위젯마다 viewport를 줄이면 위치와 투영이 달라진다. 독립 scissor 또는 UI 패스 셰이더의 클립 사각형을 사용한다. 회전된 마스크는 이 단계에서 지원하지 않는 조건으로 명시한다. 프레임 끝 또는 다른 패스에서 이전 클립이 남지 않아야 한다. 서로 다른 클립 상태는 정렬과 배칭의 경계로 취급한다.
- 완료 기준은 Scroll View 안팎에 걸친 항목에서 화면 경계와 클릭 경계가 일치하는 것이다. 중첩 클립, 공집합, DPI와 레터박스, 스크롤 끝, 바깥에 있는 버튼의 입력 차단을 확인한다.

### 4단계 · 고급 마스킹과 목록 입력

- 원형 얼굴 사진, 모서리가 둥근 패널, 스프라이트 실루엣에 대한 요구를 바탕으로 모양 마스크를 설계한다. 이 단계에서 스텐실의 중첩 깊이, 알파 마스크 합성, CPU 히트 테스트에 사용할 영역 및 백엔드 차이를 결정한다. 얇은 가장자리의 반투명도와 GPU 비용은 사각 클리핑과 별도로 검증한다.
- List/Dropdown의 선택 그룹·팝업 배치, 큰 목록의 가상화, Text Input의 한글 IME·선택·클립보드, 게임패드 폴백 및 포커스 이동은 공통 배치와 스크롤을 기반으로 추가한다.

## 4. 공통 완료 조건

- 생성 도구는 기존 Canvas와 SceneEditHistory를 사용한다. 한 번의 생성은 Undo 한 번으로 복원하고, 저장·재열기와 프리팹 재배치에서 참조를 유지한다.
- 새 컴포넌트는 공통 Enable 상태를 따른다. 기존 씬을 보존해야 하는 신규 직렬화 필드는 빠진 경우의 기본값을 정한다.
- 고정 메뉴·인스펙터 문구는 로캘 CSV를 사용한다. UI 뷰 단축키는 기존 ShortcutManager 액션에 연결하며, 게임 입력은 InputManager를 거친다.
- 헤드리스 검사는 배치·클립·이벤트의 수치 조건을, 실제 Editor·PIE·Client는 해상도·DPI·글꼴·입력과 출력 합성을 확인한다. [에디터 UX 검증](EditorUXTestGuidelines.md)
