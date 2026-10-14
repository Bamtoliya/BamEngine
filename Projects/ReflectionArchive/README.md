# ReflectionArchive

Reflection 메타데이터를 이용해 Archive의 객체 직렬화를 수행하는 C++23 어댑터 모듈입니다.

[Reflection](../Reflection/README.md)과 [Archive](../Archive/README.md)에 의존하며
엔진, 에디터, EnTT, GLM에는 직접 의존하지 않습니다.

## 구성과 의존성

- `ReflectionArchiveAdapter.h`: Reflection 기반 객체 저장·복원과 객체 컨테이너 코덱
- `PropertyArchiveCodecs.h`: 실제 C++ 타입별 사용자 직렬화 코덱 등록
- CMake 타깃: `ReflectionArchive::Adapter`

어댑터 자체는 INTERFACE 라이브러리입니다.
Reflection::Runtime과 Archive::Json의 사용 조건을 소비자에게 전달합니다.

JSON과 BEVE는 같은 어댑터를 사용할 수 있습니다.

## 객체 저장과 복원

`archive_reflection::WriteObject()`는 TypeInfo와 읽을 객체의 ObjectView를 받습니다.
`archive_reflection::ReadObject()`는 TypeInfo와 복원할 실제 객체를 받습니다.

대상 객체의 C++ 타입과 TypeInfo의 C++ 타입은 일치해야 합니다.
ReadObject의 대상은 복사 생성과 대입이 가능한 클래스 또는 union 타입이어야 합니다.

ReadObject는 기존 객체를 복사하여 프로퍼티를 읽고 성공할 때 대상에 대입합니다.
포인터나 공유 자원까지 독립적으로 복사하거나 외부 부작용을 되돌리지는 않습니다.

사용 예제는 [`examples/package-consumer/main.cpp`](examples/package-consumer/main.cpp)에 있습니다.
예제는 수동 타입 등록, vector 코덱, 객체 왕복과 읽기 실패 시 기존 값 보존을 검사합니다.

## 타입 처리

기본 경로는 지원하는 숫자형, bool, std::string과 std::wstring을 처리합니다.

scoped enum은 Registry에 등록된 EnumInfo를 이용해 숫자로 저장·복원합니다.
등록된 중첩 객체는 Registry로 TypeInfo를 찾아 프로퍼티를 처리합니다.

enum과 중첩 객체 처리에는 Registry를 전달해야 합니다.
enum의 등록된 열거자 목록이나 허용할 비트 조합은 검증하지 않습니다.
이 조건은 기본 처리 경로에 대한 설명이며, 등록된 사용자 코덱으로 처리를 대체할 수 있습니다.

STL 컨테이너와 사용자 전용 타입은 필요한 코덱을 명시적으로 등록합니다.
Reflection에서 컨테이너에 접근할 수 있다는 사실만으로 자동 직렬화되는 것은 아닙니다.

## 코덱 등록

- `PropertyCodecs::Register<T>(write, read)`
	- 사용자 타입의 저장·복원 콜백 등록
- `PropertyCodecs::RegisterArray<Container>()`
	- 스칼라 요소를 갖는 지원 컨테이너의 전체 타입 지정
- `PropertyCodecs::RegisterMap<Map>()`
	- 스칼라 키와 값을 갖는 map의 전체 타입 지정
- `archive_reflection::RegisterObjectArray<Element>()`
	- vector의 객체 요소 타입과 해당 TypeInfo 지정
- `archive_reflection::RegisterObjectMap<Map>()`
	- map의 전체 타입과 mapped 타입의 TypeInfo 지정

같은 C++ 타입의 코덱을 중복 등록하면 false를 반환합니다.
일반 Register<T>는 기본 생성과 지원하는 복사 생성이 가능한 타입을 대상으로 하며,
저장·복원 콜백을 모두 제공해야 합니다. 콜백이 비어 있어도 등록은 false를 반환합니다.

코덱 조회는 이름 문자열이 아닌 정확한 C++ 타입을 기준으로 합니다.
프로퍼티의 직렬화 정책과 접근 조건을 검사한 뒤 등록된 코덱을 기본 타입 처리보다 먼저 적용합니다.
복원은 코덱 호출 전에도 PropertyInfo::CanWrite()가 true여야 하므로
코덱을 등록해도 const 또는 대입 불가능한 프로퍼티의 복원을 허용하지 않습니다.
코덱 실행이 실패하면 기본 처리로 우회하지 않고 오류를 반환합니다.

객체 컨테이너 코덱은 전달받은 TypeInfo를 값으로 보관합니다.
전달한 TypeInfo의 C++ 타입이 객체 요소 또는 mapped 타입과 다르면 등록은 false를 반환합니다.
Registry와 내부 요소 코덱은 포인터로 빌려 사용하므로
해당 코덱을 사용하는 동안 살아 있어야 합니다.

## 프로퍼티 메타데이터 정책

- `Serialize = false`: 저장·복원에서 제외
- `NoSerialize = true`: 저장·복원에서 제외
- `Transient = true`: 저장·복원에서 제외
- `Optional = true`: 읽을 필드가 누락된 경우 허용

Serialize, NoSerialize, Transient 중 하나라도 제외 조건에 해당하면 제외합니다.
Serialize, NoSerialize, Transient 값이 bool이 아니면 InvalidValue를 반환합니다.
읽기에서 직렬화 대상 프로퍼티의 Optional 값이 bool이 아니어도 InvalidValue를 반환합니다.

Optional은 필드 누락만 허용합니다.
필드가 존재하지만 값이 잘못된 경우의 오류를 무시하지 않습니다.

일반 객체 읽기에서 누락된 Optional 필드는 기존 값을 유지합니다.
객체 컨테이너 읽기는 새 요소를 만들므로 해당 요소의 기본값을 유지합니다.

필드 이름은 PropertyInfo::Name을 사용합니다.
DisplayName, Editable, ReadOnly는 이 어댑터의 직렬화 정책으로 해석하지 않습니다.
복원 가능 여부에는 프로퍼티의 실제 쓰기 지원 조건이 적용됩니다.

타입 자체의 Serialize, NoSerialize, Transient 메타데이터를
WriteObject와 ReadObject가 자동으로 적용하지는 않습니다.
타입 단위 제외가 필요하면 소비자가 IsSerializationEnabled()로 판단합니다.

## 지원 경계

객체 컨테이너 복원은 새 컨테이너를 구성하여 기존 컨테이너를 교체합니다.
기존 같은 인덱스나 키의 객체에 필드를 병합하지 않습니다.

포인터 대상의 자동 순회, 객체 생성 정책, 영구 ID와 파일 버전 마이그레이션은 제공하지 않습니다.
GLM 등 소비자 전용 타입은 별도 코덱이나 어댑터로 연결합니다.

메모리 할당과 사용자 코드의 예외를 ArchiveResult로 변환하는 일반적인 예외 처리 계층은 없습니다.
최종 대입에서 예외가 발생한 경우 대상 객체의 원래 상태를 보장하지 않습니다.

## CMake 연결

소스 포함 방식에서는 Reflection::Runtime과 Archive::Json을 먼저 준비한 뒤
ReflectionArchive를 add_subdirectory로 포함합니다.
각 의존 타깃이 없으면 해당하는 설치 패키지를 찾습니다.
소스 포함 예제는 소비자와 ReflectionArchive 디렉터리가 나란히 있는 배치를 가정합니다.

```cmake
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../ReflectionArchive" "${CMAKE_CURRENT_BINARY_DIR}/reflection-archive")

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE ReflectionArchive::Adapter)
```

설치 패키지 방식:

```cmake
find_package(ReflectionArchive 0.1 CONFIG REQUIRED COMPONENTS Adapter)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE ReflectionArchive::Adapter)
```

설치 패키지는 Reflection과 Archive를 의존 패키지로 찾습니다.
Archive가 사용하는 Glaze 패키지도 검색할 수 있어야 합니다.

이 모듈은 Reflection 등록 코드를 자동 생성하지 않습니다.
소비자가 수동 등록하거나 Reflection의 reflection_generate()를 별도로 연결합니다.

## 검증 예제

- [`examples/package-consumer`](examples/package-consumer): 설치된 패키지 연결과 객체 직렬화 검사
- Archive 프로젝트의 Reflection 예제: 수동 등록, 생성 등록과 타입별 코덱 검사

검사 코드가 존재한다는 사실이 현재 구성에서 실행 검증을 완료했다는 뜻은 아닙니다.
