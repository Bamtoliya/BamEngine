# 그래프 기초: 데이터와 연결 검증

## 1. 이번 단계의 범위

로직 그래프와 셰이더 그래프가 공유할 노드·핀·연결 데이터를 구현했다.
공통 기반은 C++ API로 그래프를 구성하고 연결 규칙을 검사한다.
Editor의 `GraphPanel`이 이 API에 연결되었으며 화면 편집의 구현 내용과 확인 방법은 `Docs/GraphPanel.md`에 기록한다.
Logic 실행기와 추적 미리보기는 `Docs/GraphExecution.md`에 기록한다. 셰이더 코드 생성은 아직 없다.
`Validate()` 성공은 구성 규칙을 만족한다는 뜻이며 실행 가능하다는 보장이 아니다.

`EngineEditorClientGuidelines.md`의 Engine/Editor 책임과 Visual Scripting 원칙을 따른다.
공통 데이터에 실행 콜백, ImGui 좌표, 패널 선택 상태나 GPU 자원을 넣지 않았다.
로직 실행기와 셰이더 코드 생성은 각 도메인에서 별도로 다룬다.

## 2. 코드를 읽는 순서

1. `Engine/Source/Graph/Public/GraphTypes.h`: 식별자, 도메인, 자료형, 값과 진단.
2. `GraphPin.h`와 `GraphLink.h`: 노드 안의 핀과 노드 사이의 연결.
3. `GraphNode.h`: 재사용하는 노드 정의와 그래프 안에 배치한 노드의 구분.
4. `GraphRegistry.h`와 `Private/GraphRegistry.cpp`: 정의 등록, 검증과 기본 노드 목록.
5. `Graph.h`와 `Private/Graph.cpp`: 그래프 편집과 전체 구성 검증.
6. `Engine/examples/GraphFoundationCheck.cpp`: 정상 구성과 거부 조건의 검사 예제.

핀은 노드 정의에 속하며, 배치한 노드는 타입 키와 입력 기본값 재정의만 가진다.
예를 들어 `Math.AddFloat` 정의는 입력 `a`, `b`와 출력 `result`를 설명한다.
같은 정의로 노드 두 개를 만들더라도 각 노드의 입력 재정의는 독립적이다.
등록 정의 자체에는 실행 콜백이 없다. GraphExecutor가 기본 정의와 리플렉션 함수의 실행을 연결한다.

## 3. 식별자와 소유권

- `Graph`는 노드·연결·입력 재정의와 다음 ID를 소유한다.
- 노드와 연결 ID는 각각 그래프 안에서 증가하는 64비트 정수다. 0은 무효 값이며 삭제한 ID는 재사용하지 않는다.
- 노드 타입과 핀 키는 문자열이다. 연결은 메모리 주소나 벡터 인덱스 대신 `(노드 ID, 핀 키)`를 저장한다.
- ID는 다른 그래프 사이의 전역 식별자가 아니다. 그래프 복사본도 같은 로컬 ID를 가질 수 있다.
- `GraphRegistry`는 노드 정의를 소유하며 전역 싱글턴을 사용하지 않는다.
- `Graph`는 레지스트리 포인터를 보관하지 않는다. 편집·검증할 때 호환되는 정의를 가진 레지스트리를 전달한다.
- 다른 레지스트리를 사용하면 `Validate()`로 다시 확인해야 한다. 동일한 타입 키의 의미와 버전 호환성은 아직 관리하지 않는다.
- `GetNodes()`, `GetLinks()`, `FindNode()` 결과는 빌린 읽기 전용 뷰다. 그래프 변경 후에는 다시 조회한다.
- 레지스트리의 조회 포인터도 등록 변경 후 다시 조회한다. 특히 `RegisterBuiltins()` 성공은 내부 저장소를 교체한다.
- 같은 그래프나 레지스트리에 대한 동시 읽기·쓰기를 동기화하지 않는다. 호출자가 수명을 관리하고 접근을 직렬화한다.

`Register()`는 기존 타입을 덮어쓰지 않는다. 잘못된 정의와 타입 중복은 오류로 반환한다.
`RegisterBuiltins()`는 후보 레지스트리에 전체 기본 정의를 등록한 후 반영하므로,
사용자 정의와 이름이 충돌해도 기본 노드 일부만 추가된 상태를 남기지 않는다.

## 4. 연결과 기본값 규칙

공통 규칙:

- 출력에서 입력으로 연결한다. 노드 안에서 핀 키는 방향과 관계없이 고유해야 한다.
- `Bool`, `Int`, `Float`, `Float2`, `Float3`, `Float4`와 `Execution`을 구분한다.
- `Object`는 연결 전용이며 ObjectType 이름까지 정확히 일치해야 한다. 기본값과 Shader 사용은 거부한다.
  객체 핸들·씬 접근과 실행은 아직 없다.
- 자동 형변환은 없다. 예를 들어 `Float`에서 `Float4`로 연결할 수 없다.
- `Int` 값은 `std::int64_t`, 벡터 값은 해당 크기의 `std::array<float, N>`을 사용한다.
- 데이터 입력은 소스 하나, 데이터 출력은 목적지 여러 개를 허용한다.
- 같은 연결의 중복 추가는 오류다. 연결 교체는 기존 연결을 해제하고 새로 연결한다.
- 데이터 연결의 자기 순환과 간접 순환을 거부한다. 현재는 노드 단위 의존성을 기준으로 판단한다.
- 재귀 호출 대신 방문 집합과 명시적 작업 목록으로 경로를 탐색한다.

로직 도메인:

- 실행 핀을 허용한다. 실행 출력 하나는 목적지 하나를 가지며 실행 입력은 여러 경로를 받을 수 있다.
- 분기는 `Flow.Branch`의 `true`, `false`처럼 서로 다른 출력 핀으로 표현한다.
- 실행 순환은 데이터로 표현할 수 있다. GraphExecutor는 활성 노드 재진입을 거부하고 실행 예산을 검사한다.
- 실행량 제한, 데이터 평가와 인스턴스 상태의 실제 규칙은 GraphExecution.md에 기록한다.

셰이더 도메인:

- 셰이더를 지원한다고 등록한 노드는 실행 핀을 선언할 수 없다.
- 셰이더 그래프에는 해당 도메인을 지원하는 노드만 배치한다.
- GPU 자료형 변환, 셰이더 단계, 출력 루트 개수, 리소스 바인딩은 아직 검증하지 않는다.
- 향후 코드 생성기는 `Docs/Shader/BINDING_LAYOUT.md`의 기존 바인딩 규칙을 따라야 한다.

기본값:

- 입력 데이터 핀에만 기본값을 지정한다. `std::monostate`는 기본값이 없다는 뜻이다.
- 기본값이 없는 데이터 입력은 연결이나 노드별 재정의가 필요하다.
- 입력 값은 정확한 자료형이어야 하며 NaN과 무한대는 거부한다.
- 연결과 입력 재정의를 함께 보관할 수 있다. 실행기는 연결, 노드 재정의, 정의의 기본값 순서로 읽는다.
  Graph의 편집 API 자체는 값을 평가하지 않는다.
- `ResetInputValue()`는 노드 재정의를 지운다. 필수 입력이라면 다시 연결 또는 값 지정이 필요하다.
- 실행 입력의 미연결과 노드의 도달 가능성은 구성 오류로 처리하지 않는다.

`RemoveNode()`는 해당 노드의 모든 입·출력 연결도 제거한다.
연결이나 값을 검증하다 오류를 반환하면 해당 편집은 반영하지 않는다.
메모리 할당 예외까지 포함한 전체 트랜잭션 복구를 제공하는 것은 아니다.

## 5. 기본 노드

- 공통: `Core.Bool/Int/Float`, `Math.AddFloat/MultiplyFloat/GreaterFloat/LessFloat`, `Logic.And/Or/Not/SelectFloat`.
- 로직 전용: `Event.Start/Update`, `Flow.Branch/Sequence/ForLoop/WhileLoop/DoOnce/Gate/FlipFlop`, `Logic.FloatResult`.
- 셰이더 전용: `Shader.ColorOutput`.

어노테이션 등록과 각 제어 흐름의 실행 계약은 `Docs/GraphReflection.md`에 기록한다.
GraphLogicLibrary의 Math.AbsFloat와 Logic.Xor는 리플렉션 변환으로 Logic에 추가된다.
RegisterBuiltins 자체는 리플렉션 레지스트리를 조회하지 않는다.

출력 핀 이름과 자료형은 `GraphRegistry.cpp`의 정의가 기준이다.
노드를 추가하려면 사용자 코드에서 `GraphNodeDefinition`을 만들어 `Register()`에 전달한다.
엔진의 기본 노드 목록을 수정할 필요는 없다. 자유/static 함수는 어노테이션으로 실행을 연결할 수 있다.
사용자 제어 흐름 실행 핸들러와 Shader 생성 핸들러의 등록 API는 아직 없다.
`GetTypes(domain)`은 해당 도메인을 지원하는 타입 키를 정렬한 복사본으로 반환한다.
Editor의 노드 추가 메뉴는 이 API를 사용하므로 사용자 노드도 같은 목록에 표시할 수 있다.
`Nodes/*.h`와 `GrapEdge.h`는 과거의 빈 준비 파일이며 이번 API에서 사용하지 않는다.
`GraphExecutor.h`는 별도의 Logic 실행 인스턴스 API다. 구성 검사의 범위와 실행 검사의 범위를 구분한다.

## 6. 사용 예제

다음은 기존 C++ 소스에서 호출할 수 있는 함수 전체다.
타입을 등록한 레지스트리를 전달하면 `2 + 3`의 결과를 받도록 구성한 로직 그래프를 반환한다.
반환 시점에 5를 계산하거나 이벤트를 실행하지는 않는다.

```cpp
#include "Graph.h"
#include <stdexcept>

Engine::Graph MakeExampleGraph(const Engine::GraphRegistry& registry)
{
	auto require = [](Engine::GraphDiagnostic result)
	{
		if (!result.Succeeded()) throw std::runtime_error(result.Message);
		return result;
	};

	Engine::Graph graph(Engine::EGraphDomain::Logic);
	const auto start = require(graph.AddNode(registry, "Event.Start")).Node;
	const auto number = require(graph.AddNode(registry, "Core.Float")).Node;
	const auto add = require(graph.AddNode(registry, "Math.AddFloat")).Node;
	const auto result = require(graph.AddNode(registry, "Logic.FloatResult")).Node;

	require(graph.SetInputValue(registry, { number, "value" }, 2.0f));
	require(graph.SetInputValue(registry, { add, "b" }, 3.0f));
	require(graph.Connect(registry, { number, "result" }, { add, "a" }));
	require(graph.Connect(registry, { add, "result" }, { result, "value" }));
	require(graph.Connect(registry, { start, "exec" }, { result, "exec" }));

	const auto errors = graph.Validate(registry);
	if (!errors.empty()) throw std::runtime_error(errors.front().Message);
	return graph;
}
```

호출하는 쪽은 먼저 `GraphRegistry registry;`를 만들고 `registry.RegisterBuiltins()`의 성공을 확인한다.
이후 `MakeExampleGraph(registry)`를 호출한다. 구체적인 호출 코드는 검사 예제의 `main()`을 참고한다.
`CanConnect()`는 편집 UI의 연결 미리보기 등에 사용할 읽기 전용 검사다.
`GraphDiagnostic`의 `Code`, `Node`, `Pin`, `Link`, `Message`로 실패 위치를 표시할 수 있다.
성공한 `AddNode()`는 `Node`, 성공한 `Connect()`는 `Link` 필드에 새 식별자를 반환한다.
현재 그래프 자체의 전역 ID나 에셋 경로는 없으므로 호출자가 대상 그래프 문맥을 함께 보관한다.

## 7. 확인 방법과 현재 검증 상태

프로젝트 작업 지침에 따라 이번 작업에서 빌드와 실행은 수행하지 않았다.
다음 명령은 의존성이 구성된 기존 빌드 디렉터리에서 사용자가 실행할 명령이다.

```powershell
cmake -S . -B out/build/engine-reflection-unified -DBAMENGINE_BUILD_GRAPH_CHECK=ON -DBUILD_TESTING=ON
cmake --build out/build/engine-reflection-unified --config Debug --target GraphFoundationCheck
ctest --test-dir out/build/engine-reflection-unified -C Debug -R '^GraphFoundationCheck$' --output-on-failure
```

Release도 사용하는 경우 `--config Release`로 빌드하고 CTest에 `-C Release`를 지정한다.
성공 시 검사 프로그램은 `Graph foundation checks passed.`로 시작하는 메시지를 출력한다.
검사 대상은 다음과 같다.

- 기본·사용자 노드 등록, 잘못된 정의 거부와 등록 충돌 시 부분 추가 방지.
- 정상 로직·셰이더 구성, 입력 기본값과 필수 입력 검사.
- 연결 방향·자료형·개수·중복·존재하지 않는 노드와 핀 검사.
- 자기·간접 데이터 순환 거부와 실행 순환 표현의 구분.
- 연결 미리보기의 읽기 전용 동작과 실패한 편집의 기존 상태 보존.
- 삭제 시 연결 정리, ID 비재사용과 그래프 복사본의 독립성.
- NaN·무한대 거부, 53비트를 넘는 정수 보존과 레지스트리 불일치 보고.

이 검사는 Engine DLL을 링크하지만 렌더러, 씬이나 Editor를 초기화하지 않는다.
빌드 의존성 자체는 기존 Engine 빌드와 같다. GPU 출력이나 Editor UI를 검증하는 검사는 아니다.
성능 측정은 하지 않았으며 작은 그래프를 위한 선형 조회로 시작했다.
큰 그래프의 반복 검증 비용은 실제 편집·컴파일 경로가 생기면 측정한 뒤 개선한다.

## 8. 후속 구현 순서

1. Editor 그래프 패널: `GraphPanel`과 imnodes 연결 코드를 작성했다. 빌드와 실제 조작 검증은 아직 수행하지 않았다.
2. 저장 형식: 버전과 타입·핀 키 변경 정책을 정한 뒤 Reflection/Archive에 연결한다.
3. 로직 실행기: 초기 해석기·실행량 제한·추적을 작성했다. 빌드·실행 검증과 씬 객체 바인딩은 남아 있다.
4. 셰이더 생성기: 순서 정렬, 단계별 검증, 코드 생성과 기존 셰이더 컴파일·머티리얼 경로를 연결한다.

현재 `GraphComponent.h`는 네이티브 콜백을 보관하는 과거 실험 코드다.
이번 `Graph`의 실행이나 씬 직렬화와 연결되지 않았다.
`Tools/GraphCompiler/GraphCompiler.py`도 입력 연결을 해석하지 않는 기존 고정 코드 생성 예제로 남아 있다.
해당 JSON은 이번 그래프의 저장 형식이 아니며 새로운 로직·셰이더 컴파일러로 간주하지 않는다.
