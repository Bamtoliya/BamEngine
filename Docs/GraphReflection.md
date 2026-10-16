# 그래프 어노테이션과 기본 제어 흐름

## 현재 범위와 책임

리플렉션 메타데이터를 읽어 Logic 그래프의 노드·핀 정의를 등록한다.
GraphPanel은 생성 시 엔진 ReflectionRegistry가 있으면 해당 정의를 가져온다.
메뉴는 Category로 묶고 DisplayName과 Tooltip을 사용한다. 연결에는 고정 타입·핀 키를 유지한다.

GraphExecutor의 Logic 실행과 GraphPanel 추적 미리보기를 작성했다. 상세 범위는 GraphExecution.md에 기록한다.
프로퍼티 Get/Set은 여전히 정의만 생성하며 실행기는 Object 접근을 거부한다.
아래 실행 규칙은 실행기가 따르는 계약이지만, 이번 작업에서 빌드·실행 검증을 마쳤다는 뜻은 아니다.

읽는 순서:

1. `Engine/Source/Graph/Public/GraphAnnotations.h`: 엔진 전용 메타데이터 매크로.
2. `Projects/Reflection/include/reflection/Registry.h`: 정렬된 타입·자유 함수 조회.
3. `Engine/Source/Graph/Private/GraphReflection.cpp`: 메타데이터 검증과 정의 변환.
4. `GraphNode.h`, `GraphPin.h`: 표시 정보, 객체 핀과 리플렉션 원본 조회 정보.
5. `GraphLogicLibrary.h`: 일반 함수에 어노테이션을 적용한 실제 예제.
6. `Engine/examples/GraphReflectionTypes.h`: 생성기 검사용 함수·프로퍼티.

Reflection은 Graph를 참조하지 않는다. 읽기 전용 열거와 정규화된 반환형 정보만 제공한다.
ReturnType은 원본 표기를 유지하고 CanonicalReturnType에는 별칭을 해석한 형식을 생성한다.
매개변수 TypeName은 기존처럼 정규화된 형식이다.
GetTypes/GetFreeFunctions 결과는 레지스트리를 빌려 참조하므로 교체·파괴 후 유지하지 않는다.
열거 중 등록·병합과 동시 읽기·쓰기는 지원하지 않는다.
GraphNodeDefinition에는 포인터 대신 Owner·Member·Signature 문자열을 복사한다.
그래프 등록 배치도 성공 시 저장소를 교체하므로 이전 GraphRegistry 조회 포인터를 다시 얻어야 한다.

## 함수 노출

엔진 헤더의 네임스페이스 안에 넣는 함수 정의 예제다.

```cpp
#include "GraphAnnotations.h"

FUNCTION(GRAPH_PURE("Game.Math.Add"), DISPLAY_NAME("Add"), CATEGORY("Math/Float"),
	GRAPH_PIN_KEY("left", "a"), GRAPH_PIN_DEFAULT("left", 0.0f), GRAPH_PIN_DEFAULT("right", 0.0f))
inline float Add(float left, float right)
{
	return left + right;
}
```

- GRAPH_PURE(id): 데이터 입력과 result 출력. 반환값이 있어야 한다.
- GRAPH_CALLABLE(id): exec 입력, 매개변수 입력, 선택적인 result 출력과 then 실행 출력.
- GRAPH_PIN_KEY("인자 이름", "고정 키"): 인자 이름을 바꾸어도 연결 키를 유지.
- GRAPH_PIN_NAME("인자 이름", "표시 이름"): 키와 별개인 핀 표시 이름.
- GRAPH_PIN_DEFAULT("인자 이름", 값): 연결되지 않은 입력의 기본값.
- DISPLAY_NAME, CATEGORY, TOOLTIP은 기존 매크로를 재사용한다.

자유/static 함수는 target이 없고 일반 멤버 함수는 선언 소유 타입의 target 객체 입력을 가진다.
const만으로 순수성을 추정하지 않는다. GRAPH_PURE는 사용자가 부작용이 없음을 명시하는 계약이다.
이 표기만으로 캐싱 가능성이나 Shader 변환 가능성을 보장하지 않는다.

지원 자료형은 bool, int32/int64 계열, float와 자유/static 함수의 std::array<float, 2/3/4> 값 전달·반환이다.
Graph Int는 int64로 표현하며 int32 기본값과 실제 호출 직전 인자 범위도 검사한다.
구조 검사만으로 네이티브 호출 성공을 보장하지 않는다.
포인터·참조 인자, 출력 참조, enum, double, 문자열과 일반 구조체 반환은 아직 거부한다.
함수 호출 바인딩이 없거나 순수 함수가 void를 반환하는 경우도 오류다.

기본값은 bool에는 bool, 정수에는 정수 리터럴, float에는 0.0f 같은 실수 리터럴을 쓴다.
NaN·무한대와 float 표현 범위 초과는 거부한다. C++ 기본 인자는 아직 자동 추출하지 않는다.
기본값을 생략한 데이터 입력은 연결 또는 입력 재정의가 필요하다.
생성된 exec·then·target·result와 겹치는 핀 키도 중복으로 거부한다.
GRAPH_PURE와 GRAPH_CALLABLE은 하나만 지정한다. id는 전체 목록에서 고유해야 하며 오버로드도 구분한다.
FUNCTION 자체를 다른 선언 매크로로 감싸지 않는다. 기존 생성기와 CMake는 FUNCTION 마커를 찾는다.

## 프로퍼티 노출

등록한 클래스 내부의 일부 삽입 예제다. 기존 REFLECT_BODY 선언이 필요하다.

```cpp
PROPERTY(GRAPH_READONLY, GRAPH_ID("Game.Character.Health"), DISPLAY_NAME("Health"))
float m_Health = 100.0f;

PROPERTY(GRAPH_READWRITE, GRAPH_ID("Game.Character.Speed"), DISPLAY_NAME("Speed"))
float m_Speed = 1.0f;
```

GRAPH_READONLY는 `<id>.Get`, GRAPH_READWRITE는 `<id>.Get`과 `<id>.Set`을 만든다.
GRAPH_ID는 프로퍼티에서도 필수다. Inspector의 READONLY/EDITABLE과 그래프 접근 정책은 독립적이다.
Get에는 target·result, Set에는 exec·target·value·then이 있다.
읽기 바인딩이 없거나 쓰기를 요청한 const 프로퍼티, 두 접근 정책의 동시 지정은 거부한다.
어노테이션이 없으면 노출하지 않는다.

Object 핀은 정확한 소유 타입 이름까지 일치해야 한다. 상속·const 객체 자동 변환은 아직 없다.
연결만 허용하며 주소·숫자·null을 기본값으로 입력할 수 없다. Shader에도 노출할 수 없다.
씬 객체 조회 노드와 객체 핸들의 수명 관리는 아직 없다. 현재 실제 씬 객체에 연결하여 실행할 수는 없다.
예제 검사에서는 타입이 있는 임의 출력 정의로 연결 규칙만 확인한다.

Set 실행 바인딩을 만들 때는 검증 함수, Dirty/캐시 무효화와 실패 복구를 연결해야 한다.
그런 정책이 필요한 값은 검증된 setter 함수를 GRAPH_CALLABLE로 노출하는 것이 우선이다.
이번 변환 계층은 단순 대입이나 setter, ONCHANGED를 자동 실행하지 않는다.

## 등록과 실패

생성한 모듈의 리플렉션 레지스트리를 `RegisterReflectedGraphNodes(graphRegistry, reflectionRegistry)`에
전달하고 반환한 GraphDiagnostic의 Succeeded()를 확인한다.
별도 프로젝트 모듈은 자신의 헤더를 reflection_generate에 넣고 생성된 등록 함수를 호출해야 한다.
어노테이션만 추가했다고 로딩되지 않은 모듈까지 검색하지 않는다.
Engine/Source 헤더는 기존 ReflectionBuild.cmake의 FUNCTION/PROPERTY 스캔을 따른다.
패널에는 GraphPanel::RegisterReflectionNodes로 별도 레지스트리의 정의를 추가할 수 있다.

배치는 후보 GraphRegistry에서 전체 성공한 뒤 반영한다. 중복 ID, 존재하지 않는 인자를 가리키는
핀 메타데이터, 미지원 형식이나 잘못된 플래그가 있으면 기존 정의를 유지한다.
오류 Message는 원본 타입/함수와 이유를 포함한다. 같은 레지스트리의 반복 등록은 중복 오류다.
이 API는 핫 리로드가 아니다. 갱신·삭제와 저장 형식의 마이그레이션은 후속 범위다.

## 기본 노드와 실행 계약

일반 함수 시그니처에서 유도할 수 없는 흐름은 RegisterBuiltins에서 직접 정의한다.
현재 실행 출력 하나에는 목적지 하나만 허용하므로 순차 실행은 Sequence로 표현한다.

- Event.Start: 인스턴스 활성화 시 한 번 exec를 발생시킬 진입점.
- Event.Update: 외부 업데이트마다 exec와 해당 호출의 delta(초)를 제공할 진입점.
- Flow.Branch: condition에 따라 true/false 중 한 경로만 실행.
- Flow.Sequence: then0의 동기 실행 사슬이 반환된 뒤 then1 실행. 출력은 현재 두 개로 고정.
- Flow.ForLoop: 진입 시 first/last를 평가하고 `[first, last)`의 각 index에서 body 실행.
  first >= last면 body 없이 completed 실행. 정상 종료 시 completed는 한 번이다.
  index는 해당 반복의 body 문맥에서만 유효하다. body가 반환되면 다음 반복으로 진행한다.
- Flow.WhileLoop: 매 반복 직전 condition을 다시 평가하여 true면 body 실행.
  body가 반환되면 조건을 다시 읽고 false일 때 completed를 한 번 실행한다.
- Flow.DoOnce: 첫 통과 전에 잠금을 설정하고 then 실행. reset은 잠금만 해제한다.
  startClosed는 인스턴스 첫 사용 시 초기 상태를 정하며 이후 변경은 reset을 대신하지 않는다.
- Flow.Gate: 열린 상태에서만 enter를 exit로 전달. open/close/toggle은 상태만 변경.
  startOpen은 인스턴스 첫 사용 시 초기 상태를 정한다.
- Flow.FlipFlop: a부터 시작해 a/b를 번갈아 실행하고 경로 실행 전에 isA를 갱신한다.
  reset은 다음 경로를 a로 돌리고 isA를 false로 초기화한다. 첫 실행 전 isA도 false다.

순수 데이터 정의는 Core.Bool/Int/Float, Math.AddFloat/MultiplyFloat/GreaterFloat/LessFloat,
Logic.And/Or/Not/SelectFloat다. And/Or는 두 입력을 평가하며 SelectFloat는 선택한 값 경로만 평가한다.
Logic/Shader 공통 정의 중 Logic 평가는 GraphExecutor에서 제공하며 Shader 코드 생성은 아직 없다.
Logic의 Bool/Int/Float/Float2/Float3/Float4 결과 노드와 Shader.ColorOutput을 제공한다.
어노테이션으로 등록하는 수학·벡터 함수와 Logic.Xor는 현재 Logic 전용이다.

상태는 Registry나 편집 문서가 아닌 실행 인스턴스가 노드 ID별로 소유해야 한다.
PIE 재시작·독립 실행은 상태를 공유하지 않는다. 중첩 Sequence/Loop도 같은 실행량 제한을 소비하고,
예산 소진 시 노드 위치를 포함한 오류로 중단해야 한다. 같은 활성 노드의 재진입은 우선 오류로 거부한다.
순수 노드도 상태를 읽는 getter일 수 있으므로 이벤트 전체에 무조건 캐싱하지 않는다.

현재 Validate는 자료형·연결·필수 입력·데이터 순환만 검사한다.
실행기는 ForLoop.index의 유효 범위·아직 생성되지 않은 결과·재진입·실행 예산을 추가 검사한다.
전체 실행 문맥의 정적 분석은 아직 없다.
Break/Continue, 가변 핀 Sequence/Switch, Delay/비동기 작업과 사용자 이벤트는 실행 문맥 구현 후 추가한다.

## 검증 방법

사용자가 실행할 명령이다. 이번 작업에서는 빌드·테스트·실제 UI 실행을 수행하지 않았다.

```powershell
cmake -S . -B out/build/engine-reflection-unified -DBAMENGINE_BUILD_GRAPH_CHECK=ON -DBUILD_TESTING=ON
cmake --build out/build/engine-reflection-unified --config Debug --target Editor GraphFoundationCheck GraphReflectionCheck
ctest --test-dir out/build/engine-reflection-unified -C Debug -R '^Graph(Foundation|Reflection)Check$' --output-on-failure
```

GraphReflectionCheck는 실제 생성기의 매크로 확장, 반환형 별칭, 자유/static/멤버 함수,
프로퍼티 정책, 표시 정보, 핀 키와 객체 타입 일치를 확인하도록 작성했다.
잘못된 어노테이션과 배치 후반 충돌에서 기존 목록을 보존하는 경로도 포함한다.
GraphFoundationCheck에는 기본 흐름의 핀·기본값·도메인과 Object 정의 거부 조건을 추가했다.
이 검사들은 실행기나 제어 흐름의 동작을 검증하지 않는다.

Editor에서는 Math/Float의 Abs (Float), Logic/Bool의 Xor와 Flow 메뉴를 확인한다.
표시 이름과 고정 타입 키가 구분되고 Object 입력에는 값 편집기 대신 연결 안내가 나와야 한다.
Reflection 공용 기반을 변경했으므로 기존 ReflectionExample 검사도 재실행해야 한다.
