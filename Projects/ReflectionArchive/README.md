# ReflectionArchive

Reflection 메타데이터를 이용해 Archive의 객체 직렬화를 수행하는 C++23 어댑터 모듈입니다.

[Reflection](../Reflection/README.md)과 [Archive](../Archive/README.md)에 의존하며
엔진, 에디터, EnTT, GLM에는 직접 의존하지 않습니다.

## 시작하기

1. 구조체·클래스·enum과 저장 대상 멤버를 Reflection에 등록합니다.
2. 생성 등록 함수를 Registry에 한 번 호출하거나 수동으로 등록합니다.
3. 같은 Registry와 루트 TypeInfo로 `archive_reflection::WriteObject()`와 `ReadObject()`를 호출합니다.

지원 타입에는 타입별 저장·복원 함수나 컨테이너 코덱을 추가할 필요가 없습니다.
리소스 참조 등 프로젝트 고유 의미가 있는 타입에는 사용자 코덱을 연결합니다.

- [공통 빌드 가이드](../../Docs/ReflectionArchiveBuild.md): Release 빌드·검증·설치·외부 소비자 명령
- [소비자 타입 선언](examples/package-consumer/ConsumerTypes.h): 어노테이션·private·상속·컨테이너 예제
- [소비자 실행 코드](examples/package-consumer/main.cpp): 등록·JSON/BEVE 왕복·실패 처리
- [소비자 CMake](examples/package-consumer/CMakeLists.txt): 설치 패키지와 코드 생성 연결

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
최종 반영은 Archive의 대입 정책을 따릅니다. 복사 대입을 지원하면 복사하고 그 밖의 기존 지원 타입은 이동합니다.
포인터나 공유 자원까지 독립적으로 복사하거나 외부 부작용을 되돌리지는 않습니다.

사용 예제는 [`examples/package-consumer/main.cpp`](examples/package-consumer/main.cpp)에 있습니다.
예제는 어노테이션으로 생성한 등록 코드와 공통 API만 사용합니다.
private 멤버, 기반 클래스 멤버, 객체 vector, 스칼라·객체 map, enum과 중첩 컨테이너의 JSON·BEVE 왕복을 검사합니다.
빈 컨테이너와 읽기 실패 시 기존 값 보존도 확인합니다.

## 상속된 멤버

실제 파생 타입의 TypeInfo와 객체를 전달하면 기반 클래스에 등록된 멤버도 공통 API로 저장·복원합니다.
생성 등록은 기반 관계와 안전한 변환을 자동 연결하므로 멤버를 파생 타입에 다시 등록할 필요가 없습니다.
다단계·다중 상속과 가상 상속을 처리합니다.
private·protected 상속에는 파생 클래스의 REFLECT_BODY()가 필요합니다.
기반 클래스의 private·protected 멤버도 기존과 같이 해당 클래스의 REFLECT_BODY()로 등록합니다.

기반 관계가 있으면 Registry와 각 기반 타입의 등록이 필요합니다.
누락하면 UnsupportedType을 반환하고 Field에 해당 기반 타입 이름을 기록합니다.
기반 멤버는 기반 관계의 선언 순서로 먼저 처리하고 파생 멤버를 이어 처리합니다.
문서에 별도 기반 객체를 넣지 않고 같은 객체의 필드로 저장합니다.
기반 타입의 Optional·Transient 등 프로퍼티 정책도 그대로 적용합니다.
타입 자체의 제외 메타데이터를 자동 적용하지 않는 기존 정책은 동일합니다.

상속 경로 전체의 저장 대상 이름이 겹치면 InvalidOperation과 충돌한 필드 이름을 반환합니다.
기반 멤버를 파생 멤버로 덮어쓰거나 한쪽 값을 생략하지 않습니다.
Optional 필드도 저장 대상이면 이름 충돌 검사에 포함하고, 저장 제외 필드는 포함하지 않습니다.
이 검사는 실제 프로퍼티 읽기·쓰기와 사용자 코덱 호출 전에 수행합니다.
가상 상속으로 공유되는 같은 타입·주소의 기반 객체는 한 번만 처리합니다.
같은 타입이더라도 서로 다른 비가상 기반 부분 객체는 별개로 처리하므로 필드 이름이 겹치면 거부합니다.

파생 객체의 컨테이너에도 같은 처리를 적용합니다.
일반 객체 읽기는 복사본의 기존 기반 필드 값을 유지하고, 새 컨테이너 요소는 기반 생성자의 기본값에서 복원합니다.
일반 오류에서 원본을 보존하는 기존 범위는 동일하며 사용자 예외·외부 부작용은 되돌리지 않습니다.
포인터·참조의 동적 타입 식별과 실제 파생 객체의 생성은 이 기능에 포함되지 않습니다.

## 타입 처리

기본 경로는 지원하는 숫자형, bool, std::string과 std::wstring을 처리합니다.

scoped enum은 Registry에 등록된 EnumInfo를 이용해 숫자로 저장·복원합니다.
등록된 중첩 객체는 Registry로 TypeInfo를 찾아 프로퍼티를 처리합니다.

enum과 중첩 객체 처리에는 Registry를 전달해야 합니다.
enum의 등록된 열거자 목록이나 허용할 비트 조합은 검증하지 않습니다.
이 조건은 기본 처리 경로에 대한 설명이며, 등록된 사용자 코덱으로 처리를 대체할 수 있습니다.

기본 처리 경로는 별도 코덱 없이 다음 컨테이너를 처리합니다.

- 지원 스칼라 요소의 `std::vector`, `std::set`, `std::unordered_set` (기본 템플릿 인자)
- 지원 스칼라 요소의 `std::array<T, N>` (빈 배열 포함)
- Registry에 등록된 scoped enum 요소의 `std::vector`, `std::array`, `std::set`, `std::unordered_set`
- Registry에 요소 타입이 등록된 객체의 `std::vector<T, Allocator>`
- Registry에 요소 타입이 등록된 객체의 `std::array<T, N>` (길이 0 포함)
- 지원 스칼라 또는 등록된 scoped enum 키·값의 `std::map`, `std::unordered_map`
- 지원 스칼라 또는 등록된 scoped enum 키와 등록된 객체 값의 `std::map`, `std::unordered_map`
- 지원 컨테이너를 요소로 갖는 `std::vector`, `std::array`와 값으로 갖는 `std::map`, `std::unordered_map`

객체 vector는 PropertyInfo의 요소 C++ 타입으로 Registry에서 TypeInfo를 찾습니다.
따라서 빈 vector도 Registry와 요소 타입 등록이 필요합니다.
복원은 복사한 상위 객체 안에서 vector를 비우고 새 요소를 기본 생성하여 진행합니다.
기본 생성·크기 변경·요소 편집을 지원하지 않는 vector는 복원할 수 없습니다.

객체 고정 배열은 문서의 배열 길이가 정확히 일치해야 합니다.
복사한 상위 객체에서 배열을 기본 생성한 배열 값으로 초기화한 뒤 등록된 멤버를 복원합니다.
따라서 누락된 Optional 필드와 저장 제외 멤버는 요소 타입의 기본값을 따릅니다.
기존 같은 위치의 객체에 필드를 병합하거나 소유 클래스의 배열 멤버 초기화식을 다시 실행하지 않습니다.
배열 자체의 기본 생성·복사 대입·객체 요소 편집을 지원해야 합니다.
길이 0인 객체 배열도 요소 타입 등록이 필요하지만 실제 요소의 생성자는 호출하지 않습니다.

enum 컨테이너는 기본 템플릿 인자 외의 비교자·해시·allocator를 갖는 타입도 Reflection 접근 경로로 처리합니다.
빈 컨테이너와 길이 0인 고정 배열도 해당 enum을 Registry에 등록해야 합니다.
enum 키·값의 map은 사용되는 각 enum의 등록을 모두 요구합니다.
기반 타입의 부호와 표현 범위를 검사하여 숫자로 저장·복원하며, bool 기반 enum은 숫자 0과 1만 허용합니다.
고정 배열은 길이가 정확히 일치해야 하고, set의 중복 값은 대상 컨테이너의 비교·동등성 정책으로 거부합니다.
변환 범위 초과와 set 중복 오류는 `Modes[1]`처럼 인덱스를 포함합니다.
배열 자체를 숫자 배열로 읽는 단계의 형식 오류는 기존 Archive 정책에 따라 필드 이름을 반환합니다.

map은 사용자 지정 비교자·해시·동등성 비교자·allocator를 갖는 타입도 같은 접근 경로로 처리합니다.
저장 형식은 `[{"Key": ..., "Value": ...}]`이며 기존 명시적 map 코덱과 호환됩니다.
복원은 상위 객체의 복사본에서 map을 비우고 새 항목을 삽입합니다.
객체 값은 기본 생성하므로 누락된 Optional 필드는 새 객체의 기본값을 유지합니다.
대상의 비교·해시·allocator 정책은 복사본의 컨테이너 연산을 따르며 파일에 저장하지 않습니다.
사용자 정의 복사·대입 동작이나 allocator 전파 규칙을 별도로 대체하지 않습니다.
중복 키는 대상 map의 비교·동등성 정책으로 판단하며 해당 `[index].Key`에서 InvalidValue를 반환합니다.
빈 객체 map도 Registry와 값 타입 등록이 필요합니다.

일반 enum과 그 밖의 사용자 전용 타입은 필요한 코덱을 명시적으로 등록합니다.
중첩 컨테이너는 위 지원 범위를 재귀적으로 조합합니다.
예를 들어 `vector<vector<int>>`, `array<vector<float>, N>`, `map<string, vector<Settings>>`를 처리합니다.
배열 안의 배열은 `[[1, 2], []]`로 저장하며 별도 객체 포장을 넣지 않습니다.
map의 `Value`에는 해당 컨테이너의 기존 저장 형식을 사용합니다.
내부 컨테이너에는 별도 Reflection 등록이 필요 없으며 객체·enum 요소의 등록은 여전히 필요합니다.
바깥 컨테이너가 비어 있어도 내부 타입의 등록과 지원 여부를 검사합니다.
복원은 상위 객체의 복사본에서 각 컨테이너를 새 값으로 구성하고, 고정 배열 길이를 검사합니다.
새로 기본 생성되는 내부 컨테이너의 비교·해시·allocator는 해당 타입의 기본 생성 정책을 따릅니다.
중첩한 값의 코덱도 기본 처리보다 먼저 적용하고 오류를 그대로 전파합니다.
오류 경로는 `Matrix[1]`, `EnumGroups[0][1]`, `Objects[0].Value[1].Speed`처럼 중첩 위치를 유지합니다.
스칼라 배열 자체를 읽는 단계의 오류는 내부 배열 필드 단위이며 모든 오류에 최하위 인덱스를 붙이지는 않습니다.
컨테이너·객체를 요소로 갖는 set과 지원 목록 밖의 STL 컨테이너 조합은 자동 처리하지 않습니다.
지원하지 않는 타입은 UnsupportedType 오류를 반환하며 조용히 생략하지 않습니다.

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
CMAKE_PREFIX_PATH에 모듈 설치 경로와 Glaze 설치 경로를 함께 지정합니다.
소비자의 아키텍처·빌드 구성·MSVC Runtime 설정은 설치 라이브러리와 맞춥니다.
Debug와 Release는 별도 경로에 설치합니다.

이 모듈은 Reflection 등록 코드를 자동 생성하지 않습니다.
소비자가 수동 등록하거나 Reflection의 reflection_generate()를 별도로 연결합니다.

## 검증 예제

- [`examples/package-consumer`](examples/package-consumer): 설치된 패키지 연결과 객체 직렬화 검사
- Archive 프로젝트의 Reflection 예제: 수동 등록, 생성 등록과 타입별 코덱 검사

Archive의 생성 예제는 수동 등록 예제와 독립적으로 선택할 수 있습니다.
엔진 컴포넌트 예제는 ARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE을 별도로 켤 때만 포함합니다.
CTest와 BUILD_TESTING으로 활성화한 모듈 검증 프로그램을 실행할 수 있습니다.
설치 패키지 소비자도 `ctest --test-dir <빌드 경로> -C Release --output-on-failure --no-tests=error`로 실행합니다.

빌드 설정만 바꾸는 것으로 생성기가 배포되지는 않습니다.
설치 패키지에는 Generator와 Clang resource가 없으므로 생성 등록 소비자에 두 경로를 별도로 제공합니다.
수동 등록 소비자와 이미 생성한 등록 코드를 컴파일하는 프로젝트에는 빌드 중 Generator 호출이 필요하지 않습니다.

Windows x64 Debug·Release의 생성 등록·설치 소비자 실행 결과는
[공통 빌드 가이드의 실행 기록](../../Docs/ReflectionArchiveBuild.md#실행-검증-기록)에 정리되어 있습니다.
다른 환경과 지원 목록 밖의 타입까지 검증했다는 의미는 아닙니다.
