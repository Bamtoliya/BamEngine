# Archive

Glaze를 사용하는 C++23 JSON / BEVE Archive 모듈입니다.
Archive 코어는 엔진, 에디터, Reflection, GLM에 의존하지 않습니다.

## 구성과 의존성

- `IArchive`: 값·컨테이너·객체 직렬화와 문서·파일 입출력의 공통 인터페이스
- `GlazeArchiveBase`: 공통 문서 저장소와 값 변환, 객체·배열 접근 구현
- `JsonArchive`: JSON 문서 분석과 출력
- `BeveArchive`: BEVE 바이너리 문서 분석과 출력
- `ArchiveResult`: 오류 코드와 필드 위치
- `Unicode`: wstring과 UTF-8 변환

CMake 타깃은 `Archive::Json`입니다.
현재 이름과 달리 JSON과 BEVE 구현을 함께 포함하는 정적 라이브러리입니다.
공개 헤더, C++23 요구 조건과 Glaze 의존성을 소비자에게 전달합니다.

기존 엔진의 BinaryArchive 형식은 제공하지 않습니다.
BEVE 지원이 기존 Binary 파일과의 호환성을 의미하지는 않습니다.

## 기본 사용

```cpp
#include <archive/JsonArchive.h>

#include <array>
#include <string>

int main()
{
    archive::JsonArchive writer;
    const std::wstring name = L"플레이어 \U0001F600";
    const std::array<float, 3> position{ 1.0f, 2.0f, 3.0f };

    if (!writer.Write("Name", name) || !writer.WriteArray("Position", position))
    {
        return 1;
    }

    std::string document;

    if (!writer.ToJson(document))
    {
        return 2;
    }

    archive::JsonArchive reader;
    std::wstring restoredName;
    std::array<float, 3> restoredPosition{};

    if (!reader.Parse(document) || !reader.Read("Name", restoredName) ||
        !reader.ReadArray("Position", restoredPosition))
    {
        return 3;
    }

    return restoredName == name && restoredPosition == position ? 0 : 4;
}
```

## 문서 형식과 파일 입출력

JSON과 BEVE는 같은 값·컨테이너·객체 API를 사용합니다.
문서의 최상위 값은 객체여야 합니다.

- JSON: `JsonArchive::Parse()` / `JsonArchive::ToJson()`
- BEVE: `BeveArchive::Parse()` / `BeveArchive::ToBeve()`
- 공통 문서 API: `IArchive::Decode()` / `IArchive::Encode()`
- 공통 파일 API: `SaveToFile()` / `LoadFromFile()`

```cpp
#include <archive/BeveArchive.h>

#include <string>

int main()
{
    archive::BeveArchive writer;
    if (!writer.Write("Level", 7))
    {
        return 1;
    }

    std::string bytes;
    if (!writer.ToBeve(bytes))
    {
        return 2;
    }

    archive::BeveArchive reader;
    int level = 0;
    if (!reader.Parse(bytes) || !reader.Read("Level", level))
    {
        return 3;
    }

    return level == 7 ? 0 : 4;
}
```

BEVE의 std::string 출력은 텍스트가 아닌 바이트 버퍼입니다.
NUL을 포함할 수 있으므로 문자열 길이를 유지하여 전달해야 합니다.

파일 확장자로 형식을 자동 선택하지 않습니다.
생성한 Archive 객체의 형식에 따라 파일을 저장하고 읽습니다.

객체 접근을 시작한 뒤 EndObject()로 닫지 않은 상태에서는
문서 인코딩·디코딩과 파일 입출력을 수행할 수 없습니다.

## 값과 문자열

스칼라 API는 bool, 지원하는 정수·실수 타입, enum,
std::string과 std::wstring을 처리합니다.

JSON 문서와 std::string은 UTF-8을 사용하는 것을 전제로 합니다.
wstring은 Unicode 변환을 거쳐 JSON 문자열로 저장합니다.
std::string 입력의 UTF-8 유효성을 별도로 검사하는 API는 제공하지 않습니다.

enum은 이름 문자열 대신 기반 타입의 숫자로 저장합니다.
숫자 변환은 표현 범위를 검사하지만 등록된 열거자 목록은 검사하지 않습니다.
비트 플래그의 허용 조합과 표시 정책은 소비자가 결정합니다.

## STL 컨테이너

WriteArray는 지원하는 스칼라 요소 범위를 저장합니다.
ReadArray는 다음 컨테이너를 대상으로 제공합니다.

- 기본 allocator의 std::vector
- std::array
- std::set
- std::unordered_set

std::array 읽기는 저장된 배열 길이가 N과 정확히 일치해야 합니다.
set 읽기는 중복 요소가 있으면 실패합니다.

WriteMap과 ReadMap은 키와 값이 지원하는 스칼라 타입인
std::map과 std::unordered_map을 처리합니다.

map은 객체의 멤버 이름으로 키를 변환하지 않고 Key와 Value를 갖는 항목의 배열로 저장합니다.
다음은 JSON으로 출력한 예입니다.

```json
{
    "Values": [
        { "Key": 1, "Value": "첫 번째" },
        { "Key": 2, "Value": "두 번째" }
    ]
}
```

중복 키 읽기는 실패하며 기존 값을 덮어쓰지 않습니다.
unordered 컨테이너의 항목 출력 순서는 보장하지 않습니다.

컨테이너를 지원한다는 것은 임의의 중첩 STL 구성을
자동 직렬화한다는 의미가 아닙니다.

## 사용자 정의 객체

WriteObject와 ReadObject는 사용자 콜백으로 객체의 필드를 처리합니다.
WriteObjectArray와 ReadObjectArray는 기본 allocator의 vector<T>를 처리합니다.
WriteObjectMap과 ReadObjectMap은 스칼라 키와 객체 값을 갖는 map을 처리합니다.

객체 배열의 요소는 필드가 들어 있는 객체로 저장합니다.
객체 map의 항목은 Key와 객체 형태의 Value를 갖습니다.

객체 컨테이너 읽기는 새 요소를 생성하여 컨테이너를 교체합니다.
기존 같은 인덱스나 키의 객체를 수정하는 방식이 아닙니다.
콜백에서 생략한 필드는 새 요소의 기본값을 유지합니다.

객체 배열 콜백 오류에는 `Steps[1].Speed`,
객체 map 콜백 오류에는 `Settings[1].Value.Speed`처럼 요소 위치를 붙입니다.
스칼라 배열의 오류 위치는 현재 필드 이름 단위입니다.

## 실패와 반영 범위

ArchiveResult의 bool 변환은 성공 여부를 나타냅니다.
Code는 오류 종류, Field는 오류가 발생한 필드 위치입니다.

Parse와 컨테이너 읽기·쓰기는 임시 값을 준비한 후 성공할 때 반영합니다.
ReadObject는 대상 객체를 복사하여 콜백을 실행한 뒤 성공할 때 대입합니다.

객체 복사가 포인터나 공유 자원까지 독립 복사하는 것은 아닙니다.
콜백이 외부 상태를 변경하면 그 변경은 Archive가 되돌리지 않습니다.

메모리 할당이나 사용자 타입·콜백의 예외를
ArchiveResult로 변환하는 일반적인 예외 처리 계층은 제공하지 않습니다.

메모리상의 임시 값 반영과 파일 저장의 원자성은 별개입니다.

SaveToFile은 문서 인코딩이 성공한 뒤 대상 파일을 열어 기존 내용을 덮어씁니다.
임시 파일 저장 후 교체하는 방식은 제공하지 않습니다.
파일 쓰기 실패 시 기존 파일 내용의 보존을 보장하지 않습니다.

사용자 타입의 대입이나 콜백에서 예외가 발생한 경우에도
대상과 외부 상태의 완전한 복원을 보장하지 않습니다.

## Reflection과 GLM 연결

Reflection 기반 직렬화는 별도 [ReflectionArchive](../ReflectionArchive/README.md) 모듈이 제공합니다.
소비자는 `ReflectionArchive::Adapter` 타깃을 연결합니다.

ReflectionArchiveAdapter와 PropertyArchiveCodecs는
Archive 코어의 설치 패키지에 포함되지 않습니다.
코덱 등록, 메타데이터 정책과 객체 복원 규칙은 ReflectionArchive 문서에서 설명합니다.

GLM 지원은 엔진의 소비자 측 GlmArchiveAdapter로 제공합니다.
Archive 코어가 GLM 타입을 직접 처리하지는 않습니다.

## CMake 연결

소스 포함 방식:

```cmake
set(ARCHIVE_BUILD_EXAMPLE OFF)

add_subdirectory(
    "${CMAKE_CURRENT_SOURCE_DIR}/../Archive"
    "${CMAKE_CURRENT_BINARY_DIR}/archive"
)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Archive::Json)
```

설치 패키지 방식:

```cmake
find_package(Archive 0.1 CONFIG REQUIRED)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE Archive::Json)
```

두 방식 모두 Glaze의 CMake 패키지를 찾을 수 있어야 합니다.
Archive::Json은 C++23 요구 조건을 소비자에게 전달합니다.

## 검증 예제

- `ArchiveExample`: JSON / BEVE, 파일 입출력, 값·컨테이너와 실패 조건 검사
- `ArchiveReflectionExample`: 수동 Reflection 등록과 어댑터 연결 검사
- `ArchiveGeneratedReflectionExample`: 생성 Reflection 등록과 어댑터 연결 검사
- `ArchiveGlmExample`: 소비자 측 GLM 어댑터 연결 검사
- `ArchiveEngineComponentExample`: 실제 엔진 컴포넌트 헤더의 생성 등록과 직렬화 검사
- [`examples/package-consumer`](examples/package-consumer): 설치된 Archive 패키지 연결 검사

예제 활성화 옵션:

- `ARCHIVE_BUILD_EXAMPLE`: 코어 예제
- `ARCHIVE_BUILD_GLM_EXAMPLE`: GLM 예제
- `ARCHIVE_BUILD_REFLECTION_EXAMPLE`: 수동 Reflection 예제
- `ARCHIVE_BUILD_GENERATED_EXAMPLE`: 생성 Reflection 및 엔진 컴포넌트 예제

생성 Reflection 및 엔진 컴포넌트 예제를 구성하려면
ARCHIVE_BUILD_REFLECTION_EXAMPLE과 ARCHIVE_BUILD_GENERATED_EXAMPLE을 모두 켜야 합니다.

현재 Reflection 예제 구성은 Reflection::Runtime 타깃이 없으면 Reflection 소스를 포함합니다.
이 경우 LLVM·Clang SDK가 필요합니다.
생성 예제에는 대응하는 Clang resource 디렉터리도 필요합니다.

예제와 검사 코드가 존재한다는 사실이 현재 구성에서 실행 검증을 완료했다는 뜻은 아닙니다.
