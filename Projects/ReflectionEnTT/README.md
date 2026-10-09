# ReflectionEnTT

선언 앞의 매크로와 사용자 메타데이터 문법을 유지하고, EnTT meta 등록 코드를 생성하는 독립 C++20 모듈입니다.
엔진·에디터·GameObject·GetTypeID()·리플렉션 기반 클래스가 필요하지 않습니다.
기존 엔진과 Reflection 프로젝트의 파일/빌드 연결은 변경하지 않았습니다.

## 구조

- Clang 전처리기: 사용자 매크로를 확장하고 META 토큰을 수집합니다.
- Clang AST: 마커를 실제 선언에 연결하고 타입/멤버/함수/enum 정보를 수집합니다.
- Generator: EnTT 등록 .gen.cpp/.gen.h와 의존성 .gen.d를 만듭니다.
- Runtime: EnTT meta와 문자열 키/typed value 메타데이터만 사용합니다.
- Context: 소비자가 소유하는 entt::meta_ctx에 등록합니다.

파서와 depfile 로직은 Projects/Reflection의 현재 구현을 기반으로 새 폴더에 독립적으로 구성했습니다.
지원 대상은 EnTT 3.16.x와 LLVM/Clang 20.1.8 SDK입니다. EnTT 4는 아직 검증하지 않았습니다.

## 선언

~~~cpp
#include <reflection_entt/Annotations.h>
#define NAME(value) META("DisplayName", value)
#define EDITABLE META("Editable", true)
#define RANGE(minimum, maximum) META("RangeMin", minimum), META("RangeMax", maximum)

namespace app
{
    STRUCT(NAME("Movement Settings"))
    struct Settings
    {
        PROPERTY(NAME("Speed"), EDITABLE, RANGE(0.0, 100.0))
        float Speed = 1.0f;
    };

    CLASS(NAME("Player"))
    class Player
    {
        REFLECT_BODY()

    public:
        FUNCTION()
        void Move(float distance) { m_position += distance; }

    private:
        PROPERTY(NAME("Position"), EDITABLE)
        float m_position = 0.0f;
    };
}
~~~

CLASS/STRUCT/PROPERTY/FUNCTION/ENUM은 일반 C++ 컴파일에서 지워집니다.
META는 토큰 문법입니다. META를 빈 매크로로 정의하면 안 됩니다.
NAME, EDITABLE, RANGE 같은 매크로는 소비자가 정의하며 모듈이 특정 편집기 키를 강제하지 않습니다.
REFLECT_BODY()는 생성 코드의 private/protected 접근을 위한 friend만 추가합니다.
가상 함수나 타입 ID를 추가하거나 접근 지정자를 변경하지 않습니다.

메타데이터 값은 bool, int64_t, uint64_t, double, std::string입니다.
UTF-8/이스케이프/NUL을 보존하며, 중복 키와 잘못된 리터럴은 생성 오류입니다.
일반 C++ 표현식/객체는 평가하지 않으며 정수는 십진수 리터럴을 사용합니다.

## 빌드

Runtime은 header-only이며 EnTT만 필요합니다. LLVM은 Generator를 빌드할 때만 필요합니다.
vcpkg.json은 EnTT만 요청하므로 LLVM 소스를 vcpkg로 빌드하지 않습니다.

현재 저장소에 이미 설치된 EnTT와 LLVM SDK를 사용하는 프리셋:

~~~powershell
$repoRoot = "I:/BamtoliyaGithub/BamEngine"
cmake -S "$repoRoot/Projects/ReflectionEnTT" --preset windows-dev
cmake --build "$repoRoot/Projects/ReflectionEnTT/build" --config Release
ctest --test-dir "$repoRoot/Projects/ReflectionEnTT/build" -C Release --output-on-failure
~~~

프리셋은 out/build/x64-debug/vcpkg_installed 아래 EnTT와
Projects/Reflection/dependencies/llvm-20.1.8 SDK를 참조합니다.
다른 환경에서는 EnTT_DIR, LLVM_DIR, Clang_DIR, REFLECTION_ENTT_CLANG_RESOURCE_DIR을 지정합니다.
공식 LLVM SDK의 CRT에 맞춰 Generator 개발 프리셋은 /MT를 사용합니다.
header-only Runtime은 소비자의 /MD 또는 /MT를 강제하지 않습니다.
실패한 구성의 캐시를 정리하려면 첫 명령에 --fresh를 추가합니다.

## 소비 프로젝트

~~~cmake
add_subdirectory(path/to/ReflectionEnTT)
add_executable(App main.cpp)
reflection_entt_generate(
    TARGET App MODULE App HEADERS AppTypes.h
    GENERATOR "/path/to/ReflectionEnTTGenerator.exe"
    RESOURCE_DIR "/path/to/llvm/lib/clang/20"
)
~~~

같은 빌드에서 Generator를 만들었다면 GENERATOR를 생략합니다.
Runtime만 포함한 소비 프로젝트는 REFLECTION_ENTT_GENERATOR_EXECUTABLE로 기본 실행 파일을 지정할 수 있습니다.
resource 기본값은 REFLECTION_ENTT_CLANG_RESOURCE_DIR입니다.
INCLUDE_DIRECTORIES/COMPILE_OPTIONS로 헤더 의존성과 소비자의 전처리 정의를 생성기에도 전달합니다.
VISIBILITY PUBLIC은 정적 라이브러리의 include/링크 전파에 사용할 수 있습니다.
생성 헤더 설치와 Windows DLL export는 자동 처리하지 않습니다.
각 타입은 하나의 생성 모듈이 관리하며 모듈명은 프로그램 전체에서 고유하게 정합니다.

## 사용

~~~cpp
#include "App.gen.h"
#include "AppTypes.h"
#include <reflection_entt/Reflection.h>

entt::meta_ctx context;
reflection_entt::RegistrationResult result;
if (!reflection_entt_generated::Register_App(context, result))
{
    // result.Error, result.Subject 확인
}

app::Settings settings;
auto object = entt::forward_as_meta(context, settings);
auto type = entt::resolve<app::Settings>(context);
auto property = type.data(entt::hashed_string::value("Speed"));
property.set(object, 25.0f);
const auto* metadata = reflection_entt::GetMetadata(property);
const auto* name = metadata ? metadata->FindAs<std::string>("DisplayName") : nullptr;
~~~

프로퍼티는 as_ref_t로 등록하여 원본을 참조합니다.
FUNCTION은 as_is_t로 등록하여 값 반환은 소유하고 참조 반환은 참조를 유지합니다.
Context는 meta_type/meta_any보다, 원본 객체는 이를 참조하는 meta_any보다 오래 살아 있어야 합니다.
컨테이너 재할당이나 원본 파괴로 무효화된 참조를 사용하지 않습니다.

EnTT의 숫자/상속 변환 규칙을 따릅니다. 이전 Runtime의 exact-type-only 쓰기 정책과 다릅니다.
Editable/ReadOnly/Range는 설명 데이터입니다. UI·직렬화 정책은 소비자가 해석합니다.
C++ const 멤버와 const 원본의 쓰기는 EnTT가 제한합니다.
문자열을 소유한 Metadata 하나를 각 meta object의 custom()에 붙입니다.

자유 함수는 정규 이름으로 별도의 모듈 그룹에서 조회합니다.

~~~cpp
auto functions = reflection_entt_generated::FreeFunctions_App(context);
auto value = functions.invoke(entt::hashed_string::value("app::DoubleValue"), {}, 2.0f);
~~~

등록 전 기존 타입/ID를 검사하며 중복을 거절합니다. 사전 검사 실패는 context를 변경하지 않습니다.
등록 중 메모리 할당 예외까지 전체 롤백하는 트랜잭션은 아닙니다.
등록/해제와 동시에 조회/호출하지 않습니다.
native type hash와 등록 ID는 저장 파일용 영구 ID가 아닙니다. 안정된 저장 키 정책을 별도로 정합니다.

## 검사와 제한

예제는 메타데이터 확장, private 속성/함수, wstring, vector/map/set 원본 편집,
const 쓰기 거절, enum/alias/uint64 최댓값, public 상속, 함수 오버로드,
static/const/noexcept 함수, 문자열 반환, 참조 인자/반환, 동적 인자 배열 호출,
자유 함수, 기본 생성, 중복 등록 거절과 context 격리를 검사하도록 작성했습니다.
tests/GeneratorFailures.cmake는 잘못된 입력에서 기존 출력 보존을 검사합니다.
tests/package는 설치된 패키지만 사용하는 소비 예제입니다.
검사 코드가 존재한다는 사실은 실행 검증을 완료했다는 뜻이 아닙니다.

현재 범위는 비템플릿 class/struct, enum, 비템플릿 함수와 public 상속입니다.
bit-field/참조 데이터 멤버/volatile 멤버/ref-qualified·variadic 함수/생성자·소멸자 마커는 거절합니다.
private/protected 멤버는 REFLECT_BODY()가 필요합니다.
추상 타입이나 기본 생성 불가 타입은 등록할 수 있으나 기본 생성 기능이 제공되지 않을 수 있습니다.
추가 함수 바인딩 제약은 EnTT의 인자·반환 타입 요구에 따릅니다.

기존 Reflection/ReflectionCore 마커 헤더와 새 Annotations.h는 같은 전역 매크로 이름을 정의하므로
한 번역 단위에서 함께 포함하지 않습니다. 엔진 연결은 모듈 검증 이후 별도 작업입니다.
