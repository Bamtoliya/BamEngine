# GraphPanel: ImGui 노드 편집 패널

## 구현 범위와 검증 상태

`ImGuiInterface`를 상속한 `GraphPanel`을 기본 패널에 등록했다.
기존 Console·Metrics 패널처럼 `m_Open`, `m_Focused`, `m_Hovered`, `m_Name`과
번역 제목 뒤의 고정 `###GraphPanel` ID를 사용한다.
창 메뉴의 `Panels → Graph`에서 다시 열 수 있다.

화면 편집에는 [imnodes](https://github.com/Nelarius/imnodes)를 사용한다.
노드·핀·연결선 렌더링, 노드 이동, 화면 이동과 선택은 확장이 담당한다.
자료형, 연결 개수, 데이터 순환과 기본값 검증은 기존 Engine API가 담당한다.
공통 기반과 실행 제한은 `Docs/GraphFoundation.md`를 참고한다.

이번 작업은 소스 작성과 정적 검토까지 수행했다.
프로젝트 지침에 따라 의존성 다운로드, CMake 구성, 빌드·테스트와 실제 Editor 조작은 실행하지 않았다.
따라서 아래 사용 흐름은 실행으로 확인해야 할 항목이며, 검증 완료 기록이 아니다.

## 의존성

- `Editor/CMake/NodeEditor.cmake`에서 CMake FetchContent로 공식 소스를 가져온다.
- 고정 커밋: `eb36902c892548ef94f88f51ad7e7c9c7058a71c`.
- ImGui 1.92 대응이 들어간 소스를 선택했으며 실제 프로젝트의 ImGui 1.92.5와 빌드 검증은 남아 있다.
- upstream 예제는 비활성화하고 submodule은 가져오지 않는다. 기존 `imgui::imgui`를 재사용한다.
- imnodes의 경고는 유지하되 upstream의 MSVC `/WX`는 이 의존성에서 해제한다.
- Editor만 imnodes를 링크한다. Engine 그래프 자료형에 확장 라이브러리의 ID나 헤더를 추가하지 않았다.
- 첫 CMake 구성에는 Git과 GitHub 접근이 필요하다. 소스는 빌드 디렉터리의 `_deps`에 준비된다.

## 파일별 책임

- `Editor/Source/ImGui/GraphPanel/Public/GraphPanel.h`: 패널과 두 도메인의 편집 문서 상태.
- `Private/GraphPanel.cpp`: 창, 도구 모음, 속성, 진단과 그래프 편집 API 호출.
- `Private/GraphPanelNodeEditor.cpp`: imnodes 컨텍스트, 노드·핀 표시와 연결·선택 이벤트 변환.
- `Public/GraphNodePalette.h`: UI와 독립적인 호환 후보 조회·검색 및 노드 생성과 연결의 일괄 적용.
- `Private/GraphPanelExecution.cpp`: 실행 미리보기와 추적, 노드에 표시할 최근 관측값 조회.
- `ImGuiManager.cpp`: 기존 패널과 같은 생성·등록 경로.
- `Resources/Data/Localization/Editor.csv`: `GraphPanel.*` 한국어·영어 문구.
- `Engine::GraphRegistry::GetTypes()`: 도메인 필터와 정렬을 적용한 노드 추가 목록.

각 문서는 Engine 그래프와 imnodes 컨텍스트를 소유한다. Logic과 Shader의 데이터·위치·선택·화면 이동은 독립적이다.
Engine의 노드·연결 ID는 64비트이며 imnodes는 `int` ID를 사용한다.
Engine ID를 잘라서 변환하지 않고, 노드·핀·연결마다 고유한 편집용 ID를 별도 발급하여 대응시킨다.
편집용 ID 공간이 부족하면 모델을 변경하기 전에 오류로 거부한다. 삭제한 편집용 ID도 재사용하지 않는다.

패널이 숨겨지면 마우스 해제 프레임을 놓칠 수 있다.
다시 표시할 때 위치·화면 이동을 메모리 문자열로 옮긴 뒤 UI 컨텍스트를 재생성하여 드래그 상태를 정리한다.
선택은 모델과 ID 대응이 남아 있는 항목에 복원한다. 삭제 후에도 같은 방식으로 UI 내부 참조를 정리한다.
이 과정은 파일 저장이 아니며 그래프 문서 자체를 교체하지 않는다.
이전 imnodes 컨텍스트는 패널을 그린 뒤 복구하며, 종료 시 패널의 `Free()`에서 소유 컨텍스트를 해제한다.
기존 ImGuiManager는 패널을 해제한 다음 ImGui를 종료한다.

## 조작과 확인할 결과

1. `Logic`을 선택하고 `노드 추가`에서 `Core.Float`, `Math.AddFloat`, `Logic.FloatResult`를 추가한다.
   목록은 현재 도메인의 레지스트리 정의에서 생성된다. Shader 전용 노드는 Logic 목록에 없어야 한다.
2. 노드 몸체를 드래그해 겹치지 않게 배치하고, 가운데 버튼 드래그로 화면을 이동한다.
   `화면 초기화`는 이동만 초기화하며 노드 배치는 유지해야 한다.
3. 선택하지 않은 `Core.Float` 노드 안에서 `value`를 2로 편집한다. 오른쪽 속성에서도 편집할 수 있다.
   `Math.AddFloat.b`를 3으로 편집한다. 입력하지 않은 필수 값은 `값 지정`으로 0 등 표시된 값을 명시할 수 있다.
4. `Core.Float.result → Math.AddFloat.a` 연결을 만들고,
   `Math.AddFloat.result → Logic.FloatResult.value`를 연결한다.
   입력 핀에서 출력 핀 방향으로 드래그해도 모델에는 출력→입력으로 저장해야 한다.
5. 연결된 입력은 노드 안에서 기본값 편집 대신 관측값을 표시하고 오른쪽 속성의 편집은 비활성화되어야 한다.
   연결 편집만으로 계산하지 않는다. Start.exec를 FloatResult.exec에 연결한 뒤 실행하면 값을 계산한다.
   실행 조작과 추적은 GraphExecution.md의 절차를 따른다.
6. 연결선을 선택하고 `Delete` 또는 속성의 `연결 해제`를 누른다.
   연결이 제거되고 이전 입력 재정의나 정의의 기본값을 다시 편집할 수 있어야 한다.
7. `Flow.Branch.condition`에 Float 출력을 연결하거나, 입력에 두 번째 소스를 연결하거나,
   Add 노드들로 데이터 순환을 만든다. 후보 핀에 머무를 때 오류가 표시되고,
   놓았을 때 기존 연결을 보존하면서 새 연결을 거부해야 한다.
8. `Core.Float.result`를 빈 캔버스에 놓으면 검색창이 있는 호환 노드 목록이 열려야 한다.
   `add`를 검색하면 `Math / AddFloat / a`와 `Math / AddFloat / b`가 표시되고,
   선택한 입력에 새 노드가 자동 연결되어야 한다. 입력 핀에서 시작하면 연결 가능한 출력 후보를 표시한다.
   다른 노드 몸체·연결선·잘못된 핀이나 캔버스 밖에 놓으면 추가 팝업을 열지 않는다.
   팝업에서 Esc 또는 바깥 클릭으로 취소하면 노드와 연결이 생기지 않아야 한다.
9. Ctrl 또는 드래그 상자로 여러 노드·연결을 선택하고 삭제한다.
   선택 노드의 부속 연결도 정리되며, 숫자 입력 중 Delete가 그래프 삭제로 전달되지 않아야 한다.
10. Shader로 바꾸어 `Shader.ColorOutput.color`의 Float4 값을 편집한다.
    로직 전용 노드는 목록에 없어야 하며, Float 출력을 color에 연결하면 거부해야 한다.
11. Logic으로 돌아오거나 패널을 닫았다 다시 연다. 데이터·배치·화면 이동·선택이 보존되고,
    이전에 잡고 있던 드래그가 남지 않아야 한다. 언어를 바꿔도 같은 패널·속성 ID를 사용해야 한다.
12. 패널 밖의 창에서 Delete를 누르거나 다른 창에서 시작한 마우스 드래그를 캔버스 위로 옮긴다.
    그래프가 임의로 삭제되거나 이동하지 않아야 한다.
13. 상단 추가 버튼과 우클릭 메뉴에서도 이름·타입 키·분류·설명으로 검색한다.
    영문 대소문자를 구분하지 않으며 공백으로 구분한 모든 단어가 일치해야 한다.
    검색 결과는 도메인 제한을 유지하고 핀에서 시작한 경우 호환 핀의 키·표시 이름도 검색한다.
14. Bool·Int·Float·Float2/3/4의 미연결 입력을 노드 안에서 편집한다.
    필수 입력은 경고와 `값 지정`을 표시하며, 보이는 0이 자동으로 저장되어서는 안 된다.
    숫자를 편집하는 동안 노드가 함께 이동하거나 Delete가 노드를 삭제하지 않아야 한다.
15. Start → FloatResult를 연결하고 실행하여 데이터 연결의 입력·출력에 `= 값` 표시를 확인한다.
    미평가 핀은 `관측값 없음`이며 Shader에는 실행 결과가 없다.
    값을 편집하거나 실행 상태를 초기화하면 이전 관측값을 지워야 한다.

편집에 성공할 때 전체 구조 진단을 갱신하며, `검사` 버튼으로도 다시 확인할 수 있다.
마지막 편집 오류는 하단에 노드 ID·핀 키와 함께 표시한다. Engine의 상세 오류 문구는 현재 영어다.

## 사용자 노드

사용자 코드가 패널의 `RegisterNode(GraphNodeDefinition)`를 호출하면 해당 도메인의 추가 메뉴에 반영된다.
엔진 어노테이션 노드는 생성 시 리플렉션에서 추가한다. 별도 모듈은 `RegisterReflectionNodes`로 등록한다.
목록은 Category / DisplayName으로 표시하며 Tooltip과 고정 타입·핀 키도 검색할 수 있다.
핀에서 시작한 목록은 방향·정확한 자료형·ObjectType·연결 개수 제한을 Engine::Graph::CanConnect로 확인한다.
Float와 Float3 같은 암묵적 변환이나 기존 연결의 자동 교체는 없다.
호환 핀이 여러 개면 핀별로 항목을 표시하여 사용자가 연결할 곳을 선택한다.
후보 조회는 팝업을 열 때 수행하며 검색 입력은 준비된 후보를 필터링한다.
선택하면 그래프 복사본에서 생성과 연결을 검증한 뒤 함께 적용한다. 실패하면 원본 데이터와 ID를 보존한다.
Object 입력은 타입이 맞는 연결만 허용하고 기본값을 입력할 수 없다.
어노테이션과 기본 제어 흐름의 정의는 `Docs/GraphReflection.md`를 참고한다.
등록 결과의 `Succeeded()`를 반드시 확인한다. 별도의 메뉴 항목 등록은 필요 없다.
속성 창과 노드 내부 편집은 Bool·Int·Float·Float2·Float3·Float4를 지원한다.
실행 함수나 셰이더 생성 함수의 등록까지 제공하는 API는 아니다.

노드의 읽기 전용 값은 최근 2,048개 실행 기록에 남은 마지막 관측값이다.
실행하지 않은 분기의 현재 값이나 실시간 계산 결과를 보장하지 않는다.
추적을 한 번 훑어 프레임의 표시용 값을 구성하며 핀마다 기록 전체를 검색하지 않는다.
과거 기록을 선택해도 노드 값이 그 시점으로 되감기지는 않는다. 선택 기록의 값은 오른쪽 추적에서 확인한다.

## 사용자 실행 명령

기존 의존성 설정이 있는 빌드 디렉터리에서 실행한다.
Editor 빌드는 기존 `GenerateLocalization` 작업을 통해 CSV의 번역 JSON도 갱신한다.

```powershell
cmake -S . -B out/build/engine-reflection-unified -DBAMENGINE_BUILD_GRAPH_CHECK=ON -DBAMENGINE_BUILD_REFLECTION_CHECK=ON -DBUILD_TESTING=ON
cmake --build out/build/engine-reflection-unified --config Debug --target Editor GraphPaletteCheck GraphFoundationCheck GraphReflectionCheck
ctest --test-dir out/build/engine-reflection-unified -C Debug -R '^Graph(Palette|Foundation|Reflection)Check$' --output-on-failure
```

`GraphFoundationCheck`에 도메인별 목록, 정렬과 사용자 노드 노출 검사를 추가했다.
`GraphPaletteCheck`에는 양방향 후보, 여러 호환 핀, 실행·데이터 연결 개수, 정확한 객체 타입·벡터 크기,
검색과 실패 시 생성·연결·ID 보존 검사를 작성했다. 두 검사는 작성 상태이며 이번 작업에서 실행하지 않았다.
이는 위의 실제 UI 입력·배치·도킹 검증을 대신하지 않는다.

## 남은 범위

Logic 실행·추적 미리보기의 사용법은 `Docs/GraphExecution.md`에 기록한다.
파일 저장·불러오기, Undo/Redo, 씬 연결과 셰이더 코드 생성은 아직 없다.
현재 편집 내용은 프로세스를 종료하면 사라진다.
기존 `GraphComponent`와 Python GraphCompiler 실험 코드는 이번 패널의 실행 경로가 아니다.
