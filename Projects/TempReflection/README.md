# TempReflection

엔진·에디터에 의존하지 않는 C++20 리플렉션 모듈입니다.

사용자가 헤더에 어노테이션을 작성하면 Clang 기반 Generator가
등록용 C++ 파일을 생성합니다. 애플리케이션은 생성된 등록 함수를
호출해 자신이 소유한 Registry에 정보를 등록합니다.

## 구성

- `include/reflection`, `src`: 런타임 정보, 등록, 프로퍼티 접근, 함수 호출
- `generator`: 헤더 분석과 등록 코드 생성
- `cmake/TempReflectionGenerate.cmake`: 빌드 시 코드 생성 연결
- `examples`: 기능과 실패 조건 검증
- `../TempReflectionConsumer`: 별도 소비자 프로젝트

기본 CMake 구성은 Runtime만 빌드합니다.
Generator는 `TEMP_REFLECTION_BUILD_GENERATOR=ON`,
검증 예제는 `TEMP_REFLECTION_BUILD_EXAMPLE=ON`으로 명시적으로 활성화합니다.
예제를 빌드하려면 Generator와 대응하는 Clang resource 경로가 필요합니다.

Runtime은 LLVM에 의존하지 않습니다.
Generator를 빌드할 때 LLVM·Clang 개발 라이브러리가 필요합니다.
소비자는 미리 빌드된 Generator 실행 파일을 사용할 수 있습니다.

## 검증 환경

- Windows x64
- Visual Studio 18 2026
- LLVM·Clang 20.1.8
- CMake 3.21 이상
- C++20, RTTI 사용

LLVM 20에서 기존 예제와 별도 소비자 프로젝트의 실행을 확인했습니다.
소비자에서는 `<string>`을 포함한 헤더 분석과 `std::string` 프로퍼티의
읽기·쓰기를 확인했습니다.

다른 운영체제와 도구 조합은 아직 검증하지 않았습니다.

현재 설치된 MSVC 14.51 STL은 Clang 20 이상을 요구합니다.
이 환경에서 LLVM 18 Generator로 STL 헤더를 분석하는 것은 지원하지 않습니다.

## 어노테이션 작성

분석할 헤더에는 가벼운 어노테이션 헤더를 포함합니다.

```cpp
#pragma once

#include <reflection/Annotations.h>

#define APP_NAME(value) META("DisplayName", value)
#define APP_EDITABLE META("Editable", true)

namespace app
{
    STRUCT(APP_NAME("Settings"))
    struct Settings
    {
        PROPERTY(APP_NAME("Speed"), APP_EDITABLE)
        float Speed = 1.0f;

        FUNCTION()
        void SetSpeed(float value)
        {
            Speed = value;
        }
    };

    ENUM(APP_NAME("State"))
    enum class State
    {
        Stopped,
        Running
    };
}
```

지원하는 어노테이션은 `CLASS`, `STRUCT`, `PROPERTY`, `FUNCTION`, `ENUM`입니다.
각 매크로를 대응하는 선언 바로 앞에 작성합니다.

일반 C++ 컴파일에서는 어노테이션이 제거됩니다.
Generator는 전처리 과정의 호출 위치와 확장된 인자를 수집합니다.

어노테이션 매크로 이름은 전역 이름이므로 다른 라이브러리의 매크로와
충돌하지 않도록 주의해야 합니다.

## 사용자 정의 메타데이터

사용자 매크로는 확장 후 `META("key", value)` 항목이 되어야 합니다.

```cpp
#define APP_NAME(value) META("DisplayName", value)
#define APP_CATEGORY(value) META("Category", value)
#define APP_EDITABLE META("Editable", true)
#define APP_RANGE(min, max) META("RangeMin", min), META("RangeMax", max)
```

사용 예:

```cpp
PROPERTY(APP_NAME("Speed"), APP_CATEGORY("Movement"),
    APP_EDITABLE, APP_RANGE(0.0, 100.0))
float Speed = 1.0f;
```

메타데이터 값은 일반 문자열, bool, 정수, 부동소수점 리터럴을 지원합니다.
Runtime에는 `std::string`, `bool`, `int64_t`, `uint64_t`, `double`로 저장됩니다.

임의의 C++ 식이나 constexpr 변수 값을 평가하지 않습니다.
매크로가 리터럴로 확장되는 것은 허용합니다.
같은 선언의 중복 메타데이터 키는 허용하지 않습니다.

`DisplayName`, `Category`, `Editable` 등의 키에 특별한 동작은 없습니다.
키의 의미와 사용 정책은 소비자가 결정합니다.

## 비공개 멤버

비공개 프로퍼티 접근과 비공개 함수 바인딩을 허용하려면
소유 타입 내부에 `REFLECT_BODY()`를 작성합니다.

```cpp
CLASS()
class Counter
{
    REFLECT_BODY()

private:
    PROPERTY()
    int m_value = 0;
};
```

이 매크로는 생성 등록 코드에 friend 접근을 허용합니다.

현재 생성기는 접근을 지원하는 비공개 프로퍼티에 `REFLECT_BODY()`가 없으면
오류를 보고합니다. 비공개 함수는 바인딩하지 못하는 경우
접근 불가 상태를 가진 정보로 등록합니다.

## 소비자 CMake 연결

TempReflection 소스 디렉터리를 프로젝트에 포함하고 생성 함수를 호출합니다.

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyApp LANGUAGES CXX)

set(TEMP_REFLECTION_BUILD_GENERATOR OFF)
set(TEMP_REFLECTION_BUILD_EXAMPLE OFF)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../TempReflection"
    "${CMAKE_CURRENT_BINARY_DIR}/temp-reflection"
)

add_executable(MyApp main.cpp)

temp_reflection_generate(
    TARGET MyApp
    MODULE App
    HEADERS AppTypes.h
)
```

CMake 구성 시 다음 값을 지정합니다.

- `TEMP_REFLECTION_GENERATOR_EXECUTABLE`: 미리 빌드한 Generator 실행 파일
- `TEMP_REFLECTION_CLANG_RESOURCE_DIR`: 그 Generator와 대응하는 Clang resource 디렉터리

필요하면 생성 함수에서 직접 `GENERATOR`, `RESOURCE_DIR`을 지정할 수 있습니다.

추가 분석 환경은 `INCLUDE_DIRECTORIES`, `COMPILE_OPTIONS`로 전달합니다.

```cmake
temp_reflection_generate(
    TARGET MyApp
    MODULE App
    HEADERS AppTypes.h MoreTypes.h
    INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/external/include"
    COMPILE_OPTIONS -DAPP_FEATURE=1
)
```

현재 생성 함수는 소비자 타깃의 모든 컴파일 옵션·정의·include 경로를
자동으로 복제하지 않습니다. 헤더 분석에 필요한 설정은 명시적으로 전달하고,
실제 컴파일 환경과 일치시켜야 합니다.

같은 타깃에 같은 MODULE 이름을 두 번 사용할 수 없습니다.
같은 실행 파일에 포함되는 모듈 이름도 서로 다르게 지정해야 합니다.

생성 파일은 빌드 디렉터리 아래에 배치됩니다.

```text
reflection/<TARGET>/<MODULE>/<CONFIG>/<MODULE>.gen.cpp
reflection/<TARGET>/<MODULE>/<CONFIG>/<MODULE>.gen.d
```

`.gen.cpp`는 등록 구현입니다.
`.gen.d`는 간접 include를 포함한 의존 파일 목록이며,
관련 헤더 변경 시 코드를 다시 생성하는 데 사용됩니다.

생성 파일은 직접 수정하지 않습니다.

## 모듈 등록

`MODULE App`이면 다음 등록 함수가 생성됩니다.
현재 등록 함수 선언 헤더는 사용자가 작성합니다.

```cpp
#include <reflection/Registry.h>

namespace reflection_generated
{
    bool Register_App(reflection::Registry& registry);
    bool Register_App(
        reflection::Registry& registry, reflection::RegistrationResult& failure);
}
```

사용 예:

```cpp
reflection::Registry registry;
reflection::RegistrationResult failure;

if (!reflection_generated::Register_App(registry, failure))
{
    std::cerr << reflection::ToString(failure.Error)
        << ": " << failure.Owner << " / " << failure.Member << '\n';
    return 1;
}
```

모듈 등록은 임시 Registry에서 수행한 뒤 대상 Registry에 병합합니다.
등록 검증 실패 시 대상 Registry에 일부 항목만 남기지 않습니다.
메모리 할당 예외는 RegistrationResult로 변환하지 않고 전달합니다.

같은 이름의 타입·enum이나 같은 시그니처의 자유 함수를 다시 등록하면 실패합니다.

Registry는 전역 싱글턴이 아닙니다. 애플리케이션이 생성하고 수명을 관리합니다.
등록과 조회를 동시에 수행하기 위한 동기화는 제공하지 않습니다.

## 프로퍼티 접근

```cpp
const auto* type = registry.FindType("app::Settings");
const auto* speed = type ? type->FindProperty("Speed") : nullptr;

app::Settings settings;

if (speed != nullptr && speed->CanWrite())
{
    if (!speed->Write<float>(settings, 10.0f))
    {
        return 2;
    }
}

if (speed != nullptr)
{
    const float* value = speed->Read<float>(settings);
}
```

`Read<T>()`는 원본 멤버의 주소를 반환합니다.
반환 포인터의 수명은 대상 객체와 멤버의 수명에 달려 있습니다.

소유 객체와 값의 자료형이 일치해야 합니다.
상속 관계나 숫자형·포인터형의 자동 변환은 지원하지 않습니다.

- const 멤버: 읽기 가능, 쓰기 불가
- 대입 불가능한 멤버: 읽기 가능, 쓰기 불가
- 비트 필드·참조 멤버·volatile 멤버: 현재 접근 미지원
- 포인터 멤버: 포인터 값 접근만 수행하며 대상 객체 수명을 관리하지 않음

지원 여부는 `CanRead()`, `CanWrite()`로 조회합니다.
지원하지 않는 이유는 `GetReadStatus()`, `GetWriteStatus()`로 조회합니다.

상태 조회는 프로퍼티 자체의 지원 여부입니다.
자료형이 다른 호출까지 성공한다는 의미는 아닙니다.

프로퍼티가 사용자 정의 타입이어도 그 타입의 내부 정보를 자동 등록하거나
자동 순회하지 않습니다. 내부 정보를 조회하려면 해당 타입도 별도로 표시하고
생성 입력에 포함해야 합니다.

## 함수 조회와 호출

오버로드는 함수 이름과 Clang의 canonical 시그니처로 구분합니다.

```cpp
const auto* type = registry.FindType("app::Settings");
const auto* setter = type
    ? type->FindFunction("SetSpeed", "void (float)") : nullptr;

app::Settings settings;

if (setter != nullptr && setter->CanInvoke())
{
    if (!setter->Invoke(settings, 5.0f))
    {
        return 3;
    }
}
```

현재 호출 API:

- `Invoke`: void를 반환하는 멤버 함수
- `InvokeValue<Result>`: 스칼라 값을 반환하는 멤버 함수
- `InvokeWithoutObject`: void를 반환하는 정적·자유 함수
- `InvokeValueWithoutObject<Result>`: 스칼라 값을 반환하는 정적·자유 함수

스칼라는 숫자, enum, 포인터 등을 포함합니다.
문자열·사용자 정의 객체의 값 반환과 참조 반환은 현재 지원하지 않습니다.

참조 매개변수, 가변 인자, noexcept를 포함한 예외 명세,
ref-qualified 멤버 함수 등은 현재 호출 바인딩 대상에서 제외합니다.
생성자·소멸자의 리플렉션 호출도 지원하지 않습니다.

호출 미지원 함수도 정보를 등록할 수 있습니다.
`CanInvoke()`, `GetBindingStatus()`로 실제 호출 가능 여부를 확인합니다.

인자와 반환 자료형은 저장된 함수 포인터 타입과 일치해야 합니다.
자동 변환은 제공하지 않습니다.
호출된 사용자 함수에서 발생한 예외는 그대로 전달됩니다.

시그니처 표기는 Generator 출력으로 확인합니다.
현재 인자가 없는 const 멤버 함수의 예시는 `float (void) const`입니다.

## Enum 조회

`FindEnum("qualified::Name")`으로 enum 정보를 찾습니다.
`FindEntry("Enumerator")`로 열거자를 조회합니다.

열거자 값은 기반 타입의 부호에 따라 `int64_t` 또는 `uint64_t`로 저장됩니다.
열거자 별칭과 동일한 숫자 값을 가진 서로 다른 열거자 이름을 허용합니다.

## 수명과 지원 경계

Registry가 소유한 정보는 const 포인터로 조회합니다.
현재 등록·병합 과정에서는 기존 등록 정보의 주소를 유지합니다.
Registry를 파괴하거나 대입해 내용을 교체한 이후에는 기존 포인터를 사용하지 않습니다.

프로퍼티와 함수의 대상 객체는 사용자가 소유합니다.
객체 생성·삭제, 포인터 소유권, 직렬화, 에디터 UI는 제공하지 않습니다.

템플릿 선언, 상속 정보 활용, 중첩 타입 자동 순회 등을
일반적으로 지원하는 것으로 보장하지 않습니다.
지원 범위는 구현과 검증 예제를 기준으로 합니다.