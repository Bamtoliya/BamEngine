# 수학 노드와 로직 실행 추적

## 구현 범위

Engine의 GraphExecutor가 Logic 그래프의 복사본을 실행한다. Editor는 같은 실행기를 사용하며
별도의 연산 규칙을 구현하지 않는다. 실행기는 UI 좌표·ImGui·씬 포인터를 소유하지 않는다.
이번 작업은 코드 작성과 정적 검토까지 수행했다. 빌드·CTest·실제 UI 조작 결과는 아직 없다.

공통 데이터·등록은 GraphFoundation.md와 GraphReflection.md, 화면 편집은 GraphPanel.md를 참고한다.
저장·불러오기, PIE 자동 연결, 셰이더 코드 생성, 객체 Get/Set 실행은 이번 범위에 포함되지 않는다.

## 실제 수학 함수

GraphMathLibrary.h에는 스칼라·정수 함수를, GraphVectorLibrary.h에는 벡터 함수를 작성했다.
GraphLogicLibrary.h에서 포함하며 각 함수의 GRAPH_PURE 어노테이션으로 Logic 노드를 생성한다.
기존 Math.AddFloat/MultiplyFloat/GreaterFloat/LessFloat는 고정 정의를 유지하고 실행기에서 계산한다.
정의와 실행기의 타입 키를 임의로 변경하면 기존 그래프와 호환되지 않는다.

- Float: 사칙연산·나머지·부호·절댓값, Min/Max, Clamp/Saturate, Floor/Ceil/Round/Trunc/Fract.
- 비교: 정확한 비교와 절대 오차 허용 NearlyEqual. 정수 비교는 float로 변환하지 않는다.
- 보간: Lerp, InverseLerp, Remap, SmoothStep, Step.
- 수학: Sqrt, Pow, Exp, Log/Log2, 삼각·역삼각함수, Atan2, Pi와 도/라디안 변환.
- Int: int64 사칙연산·나머지, 절댓값·부호 반전, Min/Max/Clamp, 비교와 Float 변환.
- Float2/3/4: 생성·성분 추출, 덧셈·뺄셈·스케일·내적·길이·거리·정규화·보간.
- Float3: 외적 추가. 벡터는 std::array<float, N> 값이며 GraphValue의 기존 벡터 표현과 같다.

Lerp와 Remap은 범위 밖에서도 외삽한다. SmoothStep은 edge0 < edge1을 요구하고 보간 계수를 0~1로 제한한다.
삼각함수의 각도는 라디안이다. Float/Int 나머지는 피제수의 부호를 따르며 양수 모듈러와 다르다.
Int 나눗셈은 0 방향으로 절삭한다. FloatToInt도 절삭하며 IntToFloat는 큰 정수의 정밀도를 잃을 수 있다.

0으로 나누기, 잘못된 정의역, 잘못된 Clamp 구간, 정수 오버플로와 길이 0의 정규화는 예외로 보고한다.
INT64_MIN % -1은 0으로 처리하고 INT64_MIN / -1은 오버플로로 거부한다.
넓은 중간 계산이 필요한 곳은 double을 사용하고 Float 범위 초과·NaN·무한대 결과를 거부한다.
벡터 정규화는 길이를 먼저 float로 축소하지 않으므로 큰 벡터와 작은 벡터에서도 계산할 수 있다.
실행기는 입력과 결과의 자료형·유한성을 다시 검사한다. 기본값만 검사하고 호출을 신뢰하지 않는다.

## 리플렉션 호출 연결

FunctionInfo::InvokeDynamicWithoutObject는 std::any 인자 목록과 소유하는 반환값을 제공한다.
인자 개수·정확한 네이티브 자료형을 확인하고 사용자 함수 예외를 결과 오류로 변환한다.
자유/static 함수의 scalar 및 숫자 std::array 값 전달을 지원한다. 기존 타입 지정 호출 API는 유지한다.
숫자 배열 반환은 생성기에서도 인식하며, 일반 사용자 구조체·참조 반환까지 허용한 것은 아니다.
배열 반환값은 동적 호출 API로 읽는다. 멤버 함수의 배열 반환 지원은 아직 없다.

GraphReflection은 지원하는 함수의 실제 C++ 타입 정보를 사용하여 별칭과 벡터 핀을 연결한다.
Graph Int를 네이티브 int32로 전달할 때는 실행 직전 범위를 검사한다. 암묵적인 Float/Int 변환은 없다.
벡터 전체의 어노테이션 기본값 문법은 아직 없으므로 연결 또는 속성 창의 벡터 값을 사용한다.

## 실행 상태와 제어 흐름

Load는 그래프·정의와 필요한 함수 바인딩을 복사한다. 원본 편집은 실행 중인 복사본에 영향을 주지 않는다.
함수 코드가 있는 모듈의 수명까지 소유하지는 않으므로 실행 인스턴스를 유지한 채 네이티브 DLL을 언로드하지 않는다.
같은 실행기에 대한 동시 접근과 네이티브 함수 호출 도중 실행기 API 재호출은 지원하지 않는다.

Begin은 Start 또는 Update 진입점을 지정하여 일시정지 상태로 시작한다.
Start는 인스턴스당 한 번이고 Update는 이전 이벤트가 완료되거나 중지된 뒤 다시 호출할 수 있다.
delta는 호출자가 공급하는 초 단위 값이다. 그래프 상태는 Update 사이에 보존하고 Load/Clear로 초기화한다.
현재 UI는 Update를 버튼으로 한 번씩 호출한다. 실제 씬의 매 프레임 Update를 자동 구독하지 않는다.

Resume/Pause/Stop과 Step/Tick을 제공한다. Step은 실행 작업 하나를 처리하며,
노드 진입뿐 아니라 복귀·반복 제어 작업도 한 단계다. 그 작업이 요구하는 순수 데이터 계산은 함께 처리한다.
Tick의 maxTasks로 한 호출의 실행 작업 수를 제한한다. 순수 데이터 평가는 결과를 무조건 캐싱하지 않는다.
네이티브 함수가 반환한 결과는 해당 이벤트에서 생성된 뒤 읽을 수 있고, 새 이벤트에서는 지운다.
Loop의 index는 body에서만 유효하며 completed에서는 읽을 수 없다.
전체 제어 흐름 지배 관계를 정적으로 분석하는 컴파일러는 아직 없다.

Branch, Sequence, ForLoop, WhileLoop, DoOnce, Gate, FlipFlop은 GraphReflection.md의 계약을 실행한다.
Sequence와 Loop는 작업 스택으로 진행하고 실행 경로 자체에 C++ 재귀를 사용하지 않는다.
같은 활성 노드에 다시 진입하는 실행 순환은 오류로 거부한다.
DoOnce·Gate·FlipFlop 상태는 실행 인스턴스별 노드 ID에 속하며 다른 실행과 공유하지 않는다.
순수하다고 표시한 사용자 함수가 외부 전역 상태를 읽거나 변경하는 것은 별개의 사용자 코드 계약이다.

기본 이벤트 실행 예산은 10,000이며 실행 작업과 데이터 노드 평가가 같은 예산을 소비한다.
데이터 평가 깊이는 128로 제한한다. 조건이 계속 true인 While도 예산 소진 시 노드 위치를 포함해 실패한다.
이 제한은 호출한 네이티브 함수 내부의 무한 반복이나 블로킹을 선점하지 않는다.
오류 발생 시 이후 그래프 작업은 중단하지만 이미 수행된 네이티브 부작용을 롤백하지는 않는다.

Object 핀, 지원하지 않는 노드·바인딩, Shader 도메인은 Load 단계에서 거부한다.
Load의 구조 검사는 사용하지 않는 노드의 필수 입력도 확인한다.
씬 객체 Get/Set을 안전하게 실행하려면 객체 핸들 수명·검증·Dirty 처리의 연결을 먼저 구현해야 한다.

## 추적과 패널 조작

추적은 실행 진입·복귀, 통과한 연결 ID, 입출력 값, 반복과 오류를 순서 번호와 함께 기록한다.
최근 2,048개만 보관하고 오래된 기록부터 제거한다. GetTrace는 빌린 읽기 전용 참조이며 다음 실행 전 읽는다.
예산과 프레임 대기 시간이 다르므로 현재 기록을 성능 프로파일링 시간으로 사용하지 않는다.

1. Logic에서 Start → Branch → FloatResult 실행 경로와 수학 노드의 데이터 연결을 만든다.
2. 상단 진입점 목록에서 Start/Update의 노드 ID를 선택한다.
3. 실행은 자동 진행, 일시정지는 현재 위치 유지, 한 단계는 실행 작업 하나, 중지는 남은 작업 취소다.
4. 오른쪽 작업 간격을 조절하면 흐름을 천천히 볼 수 있다. 기본 0.15초이며 UI 프레임당 최대 네 작업을 진행한다.
5. 노드는 주황색, 마지막 연결은 굵은 노란색으로 강조한다. 실패 노드는 빨간색이다.
6. 기록을 클릭하면 일시정지하고 해당 노드를 선택한다. 실행 순서·핀 값·오류를 검사할 수 있다.
   연결된 데이터 입력·출력은 선택하지 않아도 노드 안에 최근 보관된 기록의 마지막 관측값을 표시한다.
   핀에 마우스를 올려도 같은 값을 확인할 수 있다. 기록에 없는 핀은 `관측값 없음`으로 표시한다.
   노드 표시는 과거 기록을 선택해도 그 시점으로 되감기지 않는다. 선택 시점의 값은 추적 항목에서 확인한다.
7. 최신 흐름 따라가기를 켜거나 실행을 계속하면 다시 최신 기록을 강조한다.
8. 실행 상태 초기화는 복사본·상태·기록을 지운다. 다음 실행에서 다시 Load하므로 Start도 다시 실행할 수 있다.

Logic의 노드·연결·값을 편집하거나 정의를 등록하면 실행 상태를 초기화한다.
노드 이동·선택·화면 이동은 실행을 초기화하지 않는다. 숨김·접힘·도메인 전환에서는 일시정지한다.
Shader 문서를 편집해도 일시정지한 Logic 실행 기록은 유지한다.
현재 강조는 실행기의 실제 기록이며 임의로 만든 애니메이션 경로가 아니다.
PIE의 외부 그래프 인스턴스 연결, 중단점, 호출 스택 UI와 비동기 노드는 후속 작업이다.

간단한 보간 예제:

- Start.exec → FloatResult.exec.
- Math.LerpFloat.result → FloatResult.value.
- LerpFloat의 a=2, b=6, t=0.25.
- 실행 완료 시 마지막 결과 3과 해당 데이터 연결·값 기록을 확인한다.

벡터 예제는 MakeFloat3(3,4,0) → NormalizeFloat3 → Float3Result로 구성한다.
결과는 대략 (0.6,0.8,0)이어야 한다. Normalize 입력을 0 벡터로 바꾸면 해당 노드에서 오류가 나야 한다.

## 사용자 검증 명령

```powershell
cmake -S . -B out/build/engine-reflection-unified -DBAMENGINE_BUILD_GRAPH_CHECK=ON -DBUILD_TESTING=ON -DREFLECTION_BUILD_EXAMPLE=ON
cmake --build out/build/engine-reflection-unified --config Debug --target Editor GraphExecutionCheck GraphReflectionCheck GraphFoundationCheck ReflectionExample
ctest --test-dir out/build/engine-reflection-unified -C Debug -R '^(Graph(Execution|Reflection|Foundation)Check|ReflectionExample)$' --output-on-failure
```

GraphExecutionCheck에는 수학 경계 조건, 실제 생성기의 벡터 반환, 동적 호출의 인자 검사,
실제 연산 그래프·분기·Sequence·Loop·상태·중지·재진입·예산·추적 용량 검사를 작성했다.
작성한 검사이지 실행하여 통과를 확인한 결과는 아니다.
공용 Reflection 변경이 있으므로 기존 ReflectionExample도 재빌드·재실행해야 한다.
화면 강조·도킹·언어 변경·기록 선택은 Editor에서 별도로 확인한다.
