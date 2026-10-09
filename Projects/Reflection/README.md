# Reflection

엔진·에디터에 의존하지 않는 C++20 리플렉션 모듈입니다.

사용자가 헤더에 어노테이션을 작성하면 Clang 기반 Generator가
등록용 C++ 파일을 생성합니다. 애플리케이션은 생성된 등록 함수를
호출해 자신이 소유한 Registry에 정보를 등록합니다.

## 구성

- `include/reflection`, `src`: 런타임 정보, 등록, 프로퍼티 접근, 함수 호출
- `generator`: 헤더 분석과 등록 코드 생성
- `cmake/ReflectionGenerate.cmake`: 빌드 시 코드 생성 연결
- `examples`: 기능과 실패 조건 검증
- `../ReflectionConsumer`: 별도 소비자 프로젝트

기본 CMake 구성은 Runtime만 빌드합니다.
Generator는 `REFLECTION_BUILD_GENERATOR=ON`,
검증 예제는 `REFLECTION_BUILD_EXAMPLE=ON`으로 명시적으로 활성화합니다.
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

Reflection 소스 디렉터리를 프로젝트에 포함하고 생성 함수를 호출합니다.

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyApp LANGUAGES CXX)

set(REFLECTION_BUILD_GENERATOR OFF)
set(REFLECTION_BUILD_EXAMPLE OFF)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../Reflection"
    "${CMAKE_CURRENT_BINARY_DIR}/reflection"
)

add_executable(MyApp main.cpp)

reflection_generate(
    TARGET MyApp
    MODULE App
    HEADERS AppTypes.h
)
```

CMake 구성 시 다음 값을 지정합니다.

- `REFLECTION_GENERATOR_EXECUTABLE`: 미리 빌드한 Generator 실행 파일
- `REFLECTION_CLANG_RESOURCE_DIR`: 그 Generator와 대응하는 Clang resource 디렉터리

필요하면 생성 함수에서 직접 `GENERATOR`, `RESOURCE_DIR`을 지정할 수 있습니다.

추가 분석 환경은 `INCLUDE_DIRECTORIES`, `COMPILE_OPTIONS`로 전달합니다.

```cmake
reflection_generate(
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
reflection/<TARGET>/<MODULE>/<CONFIG>/<MODULE>.gen.h
reflection/<TARGET>/<MODULE>/<CONFIG>/<MODULE>.gen.d
```

`.gen.cpp`는 등록 구현입니다.
`.gen.h`는 등록 함수의 두 오버로드 선언입니다.
`.gen.d`는 간접 include를 포함한 의존 파일 목록이며,
관련 헤더 변경 시 코드를 다시 생성하는 데 사용됩니다.

생성 파일은 직접 수정하지 않습니다.

### 라이브러리에서 등록 함수 공개

`reflection_generate()`의 `VISIBILITY` 기본값은 `PRIVATE`입니다.
생성 헤더를 라이브러리 외부에서도 사용하는 경우 `PUBLIC`을 지정합니다.

```cmake
add_library(AppReflection STATIC)

reflection_generate(
    TARGET AppReflection
    MODULE App
    VISIBILITY PUBLIC
    HEADERS AppTypes.h
)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE AppReflection)
```

`PUBLIC`은 생성 헤더의 include 경로와 Runtime 사용 조건을
같은 CMake 빌드의 소비자 타깃에 전달합니다.
MyApp에서는 `App.gen.h`를 포함하고 `Register_App()`을 호출할 수 있습니다.

생성 구현은 라이브러리에서 한 번만 컴파일합니다.
생성 헤더의 자동 설치는 제공하지 않습니다.

## 설치된 패키지 연결

Reflection 소스를 소비자 프로젝트에 포함하지 않고,
설치된 패키지를 `find_package()`로 가져올 수 있습니다.

설치 패키지에는 다음 항목이 포함됩니다.

- `include/reflection`: 공개 헤더
- `lib/<CONFIG>/ReflectionRuntime.lib`: Windows 정적 Runtime 라이브러리
- `lib/cmake/Reflection`: 패키지 설정과 코드 생성 helper

현재 설치 규칙은 Generator 실행 파일과 Clang resource 디렉터리를 포함하지 않습니다.
빌드 중 코드 생성이 필요하면 두 경로를 별도로 제공합니다.

```cmake
cmake_minimum_required(VERSION 3.21)
project(MyApp LANGUAGES CXX)

find_package(Reflection 0.1 CONFIG REQUIRED)

add_executable(MyApp main.cpp)

reflection_generate(
    TARGET MyApp
    MODULE App
    HEADERS AppTypes.h
)
```

CMake 구성 시 다음 값을 지정합니다.

- `Reflection_DIR`: 설치 경로의 `lib/cmake/Reflection`
- `REFLECTION_GENERATOR_EXECUTABLE`: 미리 빌드한 Generator 실행 파일
- `REFLECTION_CLANG_RESOURCE_DIR`: Generator와 대응하는 Clang resource 디렉터리

`reflection_generate()`는 생성 소스 추가와 `Reflection::Runtime` 링크를 처리합니다.

Runtime만 사용하고 코드 생성은 별도로 처리하는 프로젝트에서는 다음처럼 연결합니다.

```cmake
find_package(Reflection 0.1 CONFIG REQUIRED)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Reflection::Runtime)
```

Runtime 사용에는 LLVM·Clang 라이브러리가 필요하지 않습니다.
Generator와 Clang resource 디렉터리는 빌드 중 헤더 분석에 사용합니다.

설치된 라이브러리와 소비자는 아키텍처, 빌드 구성과 MSVC Runtime 설정을 맞춰야 합니다.

현재 로컬 Reflection 빌드는 CMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded,
즉 /MT를 사용합니다. 이번 PackageCheck도 Windows x64, Release, /MT로 구성합니다.

다른 Runtime 설정이나 Debug 구성을 사용하려면
해당 설정으로 Runtime과 소비자를 함께 빌드해야 합니다.

## 검증 프로젝트의 역할

- `examples`: 내부 기능과 실패 조건 검사
- `../ReflectionConsumer`: 소스 포함 방식의 외부 소비자 검사
- `../ReflectionPackageCheck`: 설치된 패키지 방식의 외부 소비자 검사

PackageCheck는 설치된 헤더와 CMake helper로 등록 코드를 생성하고,
설치된 Runtime과 링크하여 메타데이터 조회와 프로퍼티 읽기·쓰기를 검사합니다.

PackageCheck는 검증용 프로젝트이며 최종 애플리케이션에 포함하지 않습니다.

## 모듈 등록

`MODULE App`이면 `App.gen.h`와 `App.gen.cpp`가 생성됩니다.
생성 헤더에는 `Register_App()`의 기본 호출과 오류 정보 반환 오버로드가 선언됩니다.

소비자는 등록 함수를 직접 선언하지 않고 생성 헤더를 포함합니다.
`reflection_generate()`가 해당 타깃의 생성 헤더 include 경로를 설정합니다.

```cpp
#include <reflection/Reflection.h>
#include "App.gen.h"
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

## 객체·값 뷰를 통한 접근

`ObjectView`는 소유 객체의 타입과 주소를 보관하는 비소유 뷰입니다.
수정 가능한 객체에서 만들면 쓰기 경로를 제공하고,
const 객체에서 만들면 읽기 전용입니다.

`ValueView`는 값의 타입과 주소를 보관하는 읽기 전용 비소유 뷰입니다.
값을 복사해 보관하지 않습니다.

```cpp
#include <reflection/ObjectView.h>
#include <reflection/ValueView.h>

app::Settings settings;
const float replacement = 12.0f;

const auto object = reflection::ObjectView::From(settings);
const auto input = reflection::ValueView::From(replacement);

const auto* type = registry.FindType<app::Settings>();
const auto* speed = type ? type->FindProperty("Speed") : nullptr;

if (speed != nullptr)
{
    const auto error = speed->TryWriteValue(object, input);
    if (error != reflection::PropertyAccessError::None)
    {
        // 호출자가 오류를 처리합니다.
    }

    const auto result = speed->TryReadValue(object);
    if (result)
    {
        const float* value = result.Value.Get<float>();
    }
}
```

`TryReadValue()`는 오류와 ValueView를 반환합니다.
`TryWriteValue()`는 PropertyAccessError를 반환합니다.

`ValueView::Get<T>()`는 타입이 일치하면 const 포인터를 반환하고,
일치하지 않으면 nullptr를 반환합니다.
숫자형이나 포인터형의 자동 변환은 수행하지 않습니다.

`CanWrite()` 같은 지원 조회는 프로퍼티에 콜백이 연결돼 있는지를 나타냅니다.
특정 객체가 읽기 전용이거나 전달 값의 타입이 다르면 실제 호출은 실패합니다.

`ObjectView::IsValid()`와 `ValueView::IsValid()`는 주소가 설정돼 있는지만
확인합니다. 대상 객체가 아직 살아 있는지 확인하지 않습니다.
임시 값에서 From()으로 뷰를 만드는 것은 금지하지만,
이미 만든 뷰의 수명을 자동으로 추적하지는 않습니다.

## C++ 타입으로 정보 조회

Registry는 이름 외에도 실제 C++ 타입과 뷰를 이용한 조회를 제공합니다.

- `FindType<T>()`: 클래스·구조체·union 타입 정보
- `FindEnum<T>()`: enum 타입 정보
- `FindType(ObjectView)`, `FindType(ValueView)`: 뷰의 타입 정보
- `FindEnum(ValueView)`: 뷰의 enum 정보

생성 코드는 TypeInfo::For<T>()와 EnumInfo::For<T>()로 실제 타입을 연결합니다.
수동 등록에서도 타입 기반 조회가 필요하면 이 팩토리를 사용합니다.

같은 실제 C++ 타입을 서로 다른 이름으로 중복 등록하는 것은 허용하지 않습니다.
타입 식별에는 RTTI를 사용하며, 이를 파일 저장용 식별자로 사용하지 않습니다.

조회는 뷰에 저장된 타입을 기준으로 합니다.
상속 관계를 따라 변환하거나 동적 타입을 자동으로 찾아주지 않습니다.
포인터 값의 뷰로 대상 객체의 타입 정보를 자동 조회하지도 않습니다.

## 중첩 객체 접근

클래스·구조체·union 값을 가진 프로퍼티는 다음 경로로 접근합니다.

- 읽기: TryReadValue() → FindType(ValueView) → TypeInfo::AsObject()
- 수정: PropertyInfo::TryEditObject()

AsObject()는 타입이 일치할 때 읽기 전용 ObjectView를 만듭니다.
수정 가능한 원본에서 읽은 ValueView도 AsObject()로 변환하면 읽기 전용입니다.

TryEditObject()는 소유 객체와 프로퍼티가 수정 가능할 때
중첩 객체의 수정 가능한 ObjectView를 반환합니다.

프로퍼티 전체 대입이 불가능해도 중첩 객체의 수정 가능한 멤버는
별도로 수정할 수 있습니다. CanWrite()와 CanEditObject()는 다른 기능입니다.

중첩 타입의 내부 정보를 조회하려면 그 타입도 별도로 등록해야 합니다.
등록 여부와 객체 뷰를 얻을 수 있는지는 별개입니다.
포인터 대상 객체를 자동으로 따라가지는 않습니다.
union의 활성 멤버 선택과 관리도 호출자의 책임입니다.

## 벡터 접근과 편집

아래 API와 연결 조건은 `std::vector`의 접근·편집에 대한 설명입니다.
고정 배열, set, map의 지원 범위는 뒤의 각 항목에서 설명합니다.

- `TryGetSize(object)`: 현재 크기 조회
- `TryReadElement(object, index)`: 요소의 읽기 전용 ValueView
- `TryWriteElement(object, index, value)`: 기존 요소에 값 대입
- `TryEditElementObject(object, index)`: 객체 요소의 수정 가능한 ObjectView
- `TryResize(object, size)`: 크기 변경
- `TryClear(object)`: 모든 요소 제거
- `TryAppend(object, value)`: 끝에 값 복사 추가
- `TryInsert(object, index, value)`: 지정 위치에 값 복사 삽입
- `TryErase(object, index)`: 지정 위치의 요소 제거

각 기능의 연결 여부는 대응하는 CanReadElement(), CanWriteElement(),
CanEditElementObject(), CanResize(), CanClear(), CanAppend(), CanInsert(),
CanErase()로 조회합니다. 컨테이너 여부는 IsContainer()로 확인합니다.

실제 호출에서는 소유 객체 타입, 읽기 전용 여부, 값 타입과 위치를 검사합니다.
입력 값 타입은 요소 타입과 정확히 일치해야 합니다.

기존 요소 접근과 삭제는 index < size 조건을 사용합니다.
삽입은 index <= size를 허용하며, index == size이면 끝에 추가합니다.
빈 벡터에는 위치 0으로 삽입할 수 있지만 조회나 삭제는 할 수 없습니다.

현재 콜백 연결 조건은 다음과 같습니다.

- 요소 쓰기: 요소의 복사 대입 지원
- 객체 요소 수정: 요소가 클래스 또는 union
- 크기 변경: 요소의 기본 생성과 이동 생성 지원
- 비우기: const 벡터가 아니면 지원
- 끝에 추가: 요소의 복사 생성과 이동 생성 지원
- 삽입: 요소의 복사 생성·복사 대입·이동 생성·이동 대입 지원
- 삭제: 요소의 이동 대입 지원

수정 콜백은 const 벡터 프로퍼티에 연결하지 않습니다.
const 소유 객체를 통한 수정도 거부합니다.

이 조건은 현재 구현의 지원 범위입니다.
위치나 요청 크기에 따라 일부 조건을 완화하지 않습니다.
예를 들어 기본 생성이 불가능한 요소는 TryResize(object, 0)도 지원하지 않지만,
TryClear()로 비울 수 있습니다.

복사 지원 검사에서는 지원하는 STL 컨테이너의 내부 요소 타입도 확인합니다.
대상은 `std::vector`, `std::array`, `std::set`, `std::unordered_set`,
`std::map`, `std::unordered_map`입니다.

이 판별이 중첩 컨테이너의 자동 순회나 자동 직렬화를 제공하는 것은 아닙니다.
사용자 정의 타입의 생성자·대입 연산자 본문까지 분석하여
컴파일 가능 여부를 보장하지도 않습니다.

std::vector<bool>은 실제 bool 요소 주소를 제공하지 않으므로
TryReadElement()와 TryWriteElement()를 지원하지 않습니다.
크기 조회, 크기 변경, 비우기, 추가, 삽입과 삭제는 지원합니다.

TryAppend()와 TryInsert()는 읽기 전용 ValueView에서 값을 복사합니다.
이동 전용 값을 소비하는 API는 제공하지 않습니다.
이동 전용 요소도 TryErase()와 TryClear()로 제거할 수 있습니다.

## 고정 배열 접근

`std::array<T, N>` 프로퍼티는 다음 기능을 제공합니다.

- `TryGetSize`: 고정 길이 조회
- `TryReadElement`: 요소의 읽기 전용 ValueView
- `TryWriteElement`: 복사 대입 가능한 요소 수정
- `TryEditElementObject`: 클래스·union 요소의 ObjectView
- `TryWriteEnumElement`: scoped enum 요소에 숫자 쓰기

const 배열과 const 요소에는 수정 콜백을 연결하지 않습니다.
const 소유 객체를 통한 수정도 거부합니다.

고정 배열은 Resize, Clear, Append, Insert, Erase를 지원하지 않습니다.
Clear를 요소 초기화 작업으로 해석하지 않습니다.

`std::array<bool, N>`은 실제 bool 요소 주소를 제공하므로 요소 읽기·쓰기를 지원합니다.
길이가 0인 배열도 컨테이너로 인식하지만, 모든 요소 인덱스는 범위를 벗어납니다.

## Set 접근과 편집

`std::set`과 `std::unordered_set` 프로퍼티는 다음 기능을 제공합니다.

- `IsSet`: set 계열 여부 확인
- `TryGetSize`: 요소 수 조회
- `TryReadElement`: 순회 위치의 요소 읽기
- `TryFindSetElement`: 값으로 요소 검색
- `TryInsertSetElement`: 요소 복사 삽입
- `TryEraseSetElement`: 값으로 요소 삭제
- `TryClear`: 모든 요소 제거

검색과 중복 판정은 컨테이너의 comparator 또는 hash·equality 정책을 따릅니다.
이미 존재하는 값의 삽입은 ElementAlreadyExists,
없는 값의 검색·삭제는 ElementNotFound를 반환합니다.

set 요소는 수정 가능한 뷰를 제공하지 않습니다.
값을 바꾸려면 기존 요소 삭제와 새 요소 삽입을 별도로 수행합니다.
두 작업을 하나의 원자적 교체로 보장하지 않습니다.

## Map 접근과 편집

`std::map`과 `std::unordered_map` 프로퍼티는 다음 기능을 제공합니다.

- `IsMap`: map 계열 여부 확인
- `GetMapKeyType`, `GetMapValueType`: 키·값의 C++ 타입 조회
- `TryReadMapKey`, `TryReadMapValue`: 순회 위치의 키·값 읽기
- `TryFindMapValue`: 키로 값 검색
- `TryWriteMapValue`: 기존 키의 값 수정
- `TryEditMapValueObject`: 기존 클래스·union 값의 ObjectView
- `TryInsertMapEntry`: 키·값 복사 삽입
- `TryEraseMapEntry`: 키로 항목 삭제
- `TryClear`: 모든 항목 제거

키와 값의 타입은 등록된 타입과 정확히 일치해야 합니다.
숫자나 문자열의 자동 변환을 제공하지 않습니다.

TryWriteMapValue는 없는 키를 생성하지 않습니다.
TryInsertMapEntry는 기존 키의 값을 덮어쓰지 않습니다.
없는 키는 KeyNotFound, 중복 삽입은 KeyAlreadyExists를 반환합니다.

키는 읽기 전용입니다.
객체 값 편집은 해당 값의 수명이나 소유권을 관리하지 않습니다.

## 컨테이너 전체 순회

`TryForEachElement(object, visitor)`는 다음 형태의 콜백을 받습니다.

`bool visitor(std::size_t index, const reflection::ValueView& value)`

`TryForEachMapEntry(object, visitor)`는 다음 형태의 콜백을 받습니다.

`bool visitor(std::size_t index, const reflection::ValueView& key,
const reflection::ValueView& value)`

true를 반환하면 계속 읽고, false를 반환하면 정상적으로 중단합니다.
정상 중단도 PropertyAccessError::None을 반환합니다.
빈 컨테이너에서는 콜백을 호출하지 않습니다.

전달하는 뷰는 실제 요소를 빌려 읽으며, 요소를 복사하지 않습니다.
읽기 전용 객체에서도 순회를 사용할 수 있습니다.
콜백과 콜백 상태를 함수 반환 이후까지 보관하지 않습니다.

콜백 실행 중 대상 컨테이너를 삽입·삭제·비우기·크기 변경하지 않습니다.
외부 별칭을 통한 변경에도 같은 제약이 적용됩니다.

map의 일반 요소 순회는 pair<const Key, Mapped>를 전달합니다.
키와 값을 구분해서 사용하려면 TryForEachMapEntry를 사용합니다.

map·set의 전체 순회는 반복자를 한 번 진행하므로 O(n)입니다.
인덱스 접근을 반복하면 매번 begin부터 이동하므로 전체 탐색은 O(n²)가 됩니다.
vector·array는 기존 인덱스 접근으로 O(n)에 순회합니다.

unordered 컨테이너의 index는 현재 순회 순번입니다.
항목의 영구 ID나 정렬 순서를 의미하지 않습니다.

현재 vector<bool>은 요소 ValueView를 제공하지 않으므로
TryForEachElement도 ElementUnavailable을 반환합니다.

## 접근 오류와 예외

Try 계열 함수는 입력과 지원 여부 검사 실패를 PropertyAccessError로 반환합니다.
조회 결과 구조체의 bool 변환은 성공 여부를 나타냅니다.

여러 조건이 동시에 잘못된 경우 함수의 검사 순서에서 처음 발견한 오류를 반환합니다.
따라서 읽기 전용 객체이면서 위치도 잘못된 경우
항상 ReadOnlyObject가 반환되는 것으로 가정하지 않습니다.

메모리 할당, 사용자 타입의 생성·대입·이동에서 발생한 예외는
PropertyAccessError로 변환하지 않고 호출자에게 전달합니다.
Try라는 이름이 예외가 발생하지 않는다는 의미는 아닙니다.

입력·지원 여부 검사나 enum 숫자 변환의 범위 검사에서 실패하면 대상 값을 수정하지 않습니다.
수정 콜백 실행 중 예외가 발생한 경우에는 대상 값이나 컨테이너가
반드시 원래 상태로 유지된다고 보장하지 않습니다.

## 뷰와 요소 주소의 수명

ObjectView, ValueView와 Read<T>()의 반환 포인터는 대상 값을 소유하지 않습니다.
대상 객체를 파괴하거나 이동해 주소가 바뀌면 기존 뷰를 사용하지 않습니다.


벡터 편집 후에는 요소 뷰와 포인터를 다시 조회하는 방식을 권장합니다.

- 재할당: 모든 기존 요소 주소가 무효화됩니다.
- 삽입: 재할당이 없어도 삽입 위치와 그 뒤의 요소 주소가 무효화됩니다.
- 삭제: 삭제 위치와 그 뒤의 요소 주소가 무효화됩니다.
- 크기 축소: 제거된 요소 주소가 무효화됩니다.
- 비우기: 모든 요소 주소가 무효화됩니다.

무효화된 뷰도 IsValid()가 true일 수 있습니다.
Registry에 보관된 타입 정보의 수명과 대상 객체·요소의 수명은 별개입니다.

리플렉션 API 밖에서 벡터를 수정하거나 프로퍼티 전체를 대입한 경우에도
같은 수명 규칙을 적용해야 합니다.

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

## Enum 조회와 숫자 접근

`Registry::FindEnum("qualified::Name")` 또는 `Registry::FindEnum<T>()`으로 등록된 enum 정보를 찾습니다.
`EnumInfo::FindEntry("Enumerator")`로 열거자 이름을 조회합니다.

`EnumValue`는 `std::variant<int64_t, uint64_t>`입니다.
열거자 값은 기반 타입의 부호에 따라 저장됩니다.
열거자 별칭과 같은 숫자 값을 가진 여러 이름을 허용합니다.

### 숫자 읽기와 열거자 검색

`EnumInfo::ReadValue(ValueView)`는 enum 값을 숫자로 복사하여
`std::optional<EnumValue>`로 반환합니다.
유효하지 않은 뷰, 다른 자료형 또는 읽기 바인딩이 없는 정보에서는 `std::nullopt`를 반환합니다.

`EnumInfo::FindEntryByValue(EnumValue)`는 숫자가 같은 열거자를 찾습니다.
signed와 unsigned 값도 숫자 자체를 비교하므로 `int64_t{1}`과 `uint64_t{1}`은 같습니다.
음수와 큰 unsigned 값은 부호 변환으로 같은 값이 되지 않습니다.

같은 값의 열거자가 여러 개이면 등록 순서상 첫 번째 항목을 반환합니다.
일치하는 열거자가 없으면 `nullptr`를 반환합니다.
`ReadValue()`는 열거자 목록에 없는 enum 값도 읽을 수 있습니다.

### 프로퍼티 숫자 쓰기

`PropertyInfo::TryWriteEnumValue(ObjectView, EnumValue)`로 enum 프로퍼티에 숫자를 씁니다.
지원 여부는 `CanWriteEnumValue()`로 확인합니다.

숫자 쓰기는 수정 가능한 scoped enum, 즉 `enum class`와 `enum struct`를 지원합니다.
일반 enum은 이 숫자 쓰기 경로에서 제외합니다.
기존 자료형 기반 읽기·쓰기 API의 지원 조건은 별도로 적용됩니다.

대상 기반 타입의 표현 범위를 검사한 뒤 변환합니다.
범위를 벗어나면 `ValueOutOfRange`를 반환하고 기존 값을 유지합니다.
읽기 전용 객체와 const 프로퍼티에는 쓸 수 없습니다.

등록된 열거자 목록에 포함되는지는 검사하지 않습니다.
표현 범위 안의 미등록 값과 비트 조합 값도 쓸 수 있습니다.
일반 `TryWriteValue()`에는 자동 숫자 변환이 추가되지 않습니다.

### 벡터 요소 숫자 편집

scoped enum을 요소로 갖는 수정 가능한 `std::vector` 프로퍼티에는 다음 API를 제공합니다.

- `TryWriteEnumElement(ObjectView, index, EnumValue)`: 기존 요소 수정
- `TryAppendEnumElement(ObjectView, EnumValue)`: 마지막에 요소 추가
- `TryInsertEnumElement(ObjectView, index, EnumValue)`: 지정 위치에 요소 삽입

지원 여부는 각각 `CanWriteEnumElement()`, `CanAppendEnumElement()`,
`CanInsertEnumElement()`로 확인합니다.
이 결과는 프로퍼티의 바인딩 지원 여부이며, 전달한 객체의 수정 가능 여부까지 보장하지 않습니다.

요소 수정은 `index < size`, 삽입은 `index <= size`인 위치를 허용합니다.
빈 벡터의 0번 위치에도 삽입할 수 있습니다.
숫자 변환에는 프로퍼티 숫자 쓰기와 같은 범위 검사를 적용합니다.

추가·삽입 후 기존 요소 뷰와 포인터는 벡터의 주소 무효화 규칙에 따라 다시 조회해야 합니다.

scoped enum을 요소로 갖는 수정 가능한 `std::array`도
TryWriteEnumElement로 기존 요소를 수정할 수 있습니다.
고정 배열에는 enum 요소 추가·삽입 기능을 제공하지 않습니다.

### 비트 플래그 정책

Reflection은 엔진의 비트 연산자나 플래그 정책에 의존하지 않습니다.
`META("Bitmask", true)` 같은 메타데이터는 저장하지만 특별한 동작을 부여하지 않습니다.

비트 연산자 제공, 허용할 비트 조합 검사, 체크박스 UI와 표시 이름 조합은 소비자가 처리합니다.
`FindEntryByValue()`도 여러 플래그로 값을 분해하지 않고 숫자가 같은 항목만 찾습니다.

## 수명과 지원 경계

Registry가 소유한 정보는 const 포인터로 조회합니다.
현재 등록·병합 과정에서는 기존 등록 정보의 주소를 유지합니다.
Registry를 파괴하거나 대입해 내용을 교체한 이후에는 기존 포인터를 사용하지 않습니다.

프로퍼티와 함수의 대상 객체는 사용자가 소유합니다.
객체 생성·삭제, 포인터 소유권, 직렬화, 에디터 UI는 제공하지 않습니다.

등록된 중첩 객체와 vector, array, set, unordered_set, map, unordered_map의
접근 경로를 제공합니다.
전체 객체 그래프를 자동 순회하거나 포인터를 따라가지는 않습니다.

일반적인 템플릿 선언의 코드 생성, 상속 관계를 이용한 접근,
객체 생성·소멸과 포인터 소유권 관리는 현재 제공하지 않습니다.

직렬화는 Reflection Runtime의 책임이 아닙니다.
별도 Archive 모듈과 소비자 측 어댑터가 Reflection 정보를 사용하여 처리합니다.

지원 범위는 공개 API, 콜백 연결 조건과 검증 예제를 기준으로 합니다.