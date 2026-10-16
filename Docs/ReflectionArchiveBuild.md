# Reflection·Archive 빌드와 설치

Reflection, Archive, ReflectionArchive의 독립 모듈을 빌드하고 설치 패키지로 검증하는 안내다.
설계 목표는 [개발 지침](ReflectionArchiveGuidelines.md), 실제 수행 결과는 [구현 기록](ImplementationLog.md)을 따른다.
엔진의 ECS 연결과 컴포넌트 저장·복원은 [엔진 ReflectionRegistry 안내](EngineReflectionIntegration.md)를 참조한다.

## 사용할 모듈 선택

- [Reflection](../Projects/Reflection/README.md): 타입 등록·조회·멤버 접근. C++20과 RTTI가 필요하다.
- [Archive](../Projects/Archive/README.md): JSON·BEVE의 값·객체·파일 처리. C++23과 Glaze가 필요하다.
- [ReflectionArchive](../Projects/ReflectionArchive/README.md): 등록한 멤버를 공통 API로 저장·복원하는 연결 계층이다.

Reflection Runtime과 수동 등록에는 LLVM이 필요하지 않다.
어노테이션에서 등록 코드를 만들 때 Generator와 대응하는 Clang resource 디렉터리가 필요하다.
Generator를 소스에서 빌드할 때만 LLVM·Clang 개발 SDK가 필요하다.
아래 모듈 검증은 Engine·Editor·GLM·EnTT를 포함하지 않는다.

## 환경과 경로 준비

명령은 저장소 루트의 PowerShell에서 실행한다.
Windows x64, Visual Studio 18 2026, MSVC 19.51, CMake 3.21 이상을 기준으로 한다.
헤더 분석에는 현재 MSVC STL과 호환되는 LLVM·Clang 20.1.8을 사용한다.
다른 환경의 지원을 보장하는 목록은 아니다.

```powershell
$repoRoot = (Resolve-Path '.').Path
$glazePrefix = Join-Path $repoRoot 'out/build/x64-debug/vcpkg_installed/x64-windows'
$llvmRoot = Join-Path $repoRoot 'Projects/Reflection/dependencies/llvm-20.1.8'
$buildRoot = Join-Path $repoRoot 'out/build/reflection-archive-release'
$installPrefix = Join-Path $repoRoot 'out/install/reflection-archive-release'
$consumerRoot = Join-Path $repoRoot 'out/build/reflection-archive-release-consumers'
```

Glaze와 LLVM은 자동으로 내려받지 않는다. 두 의존성을 준비한 뒤 실제 위치에 맞게 경로를 바꾼다.
glazeConfig.cmake를 포함하는 설치 prefix와 LLVMConfig.cmake·ClangConfig.cmake를 포함하는 SDK를 사용한다.
glazePrefix의 예제 경로에 있는 `x64-debug`는 기존 의존성 배치 이름이며 이번 빌드 구성을 결정하지 않는다.

## 생성 등록 모듈의 Release 빌드

코어 예제, 생성 등록 직렬화 예제와 Reflection 검증 예제를 함께 빌드한다.
Generator도 같은 빌드에서 새로 만든다. 수동 등록 예제와 엔진 예제는 끈다.

```powershell
cmake -S Projects/Archive -B "$buildRoot/generated" -G 'Visual Studio 18 2026' -A x64 `
    -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
    "-DCMAKE_PREFIX_PATH=$glazePrefix" -DBUILD_TESTING=ON `
    -DARCHIVE_BUILD_EXAMPLE=ON -DARCHIVE_BUILD_GENERATED_EXAMPLE=ON `
    -DARCHIVE_BUILD_REFLECTION_EXAMPLE=OFF -DARCHIVE_BUILD_GLM_EXAMPLE=OFF `
    -DARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE=OFF `
    -DREFLECTION_BUILD_GENERATOR=ON -DREFLECTION_BUILD_EXAMPLE=ON `
    "-DLLVM_DIR=$llvmRoot/lib/cmake/llvm" "-DClang_DIR=$llvmRoot/lib/cmake/clang" `
    "-DREFLECTION_CLANG_RESOURCE_DIR=$llvmRoot/lib/clang/20"

cmake --build "$buildRoot/generated" --config Release -j 2
ctest --test-dir "$buildRoot/generated" -C Release --output-on-failure --no-tests=error -j 2
```

CTest에는 ArchiveExample, ArchiveGeneratedReflectionExample, ReflectionExample의 3개 검사가 등록된다.
검사 프로그램은 빌드 폴더의 개별 작업 디렉터리에서 실행되어 출력 파일을 분리한다.

모듈과 소비자는 `MultiThreadedDLL`(`/MD`)로 맞춘다.
배치된 LLVM SDK는 Generator의 `/MT` 설정을 사용한다. Generator는 별도 프로세스로 실행된다.
실제 Runtime 라이브러리와 소비자의 Runtime 설정이 같은지가 링크 호환성의 기준이다.

이미 빌드한 Generator를 사용하려면 새 빌드 디렉터리에서 REFLECTION_BUILD_GENERATOR를 OFF로 바꾸고
REFLECTION_GENERATOR_EXECUTABLE에 실행 파일을 지정한다. Clang resource 경로도 계속 제공해야 한다.
이 경우 LLVM_DIR과 Clang_DIR은 필요하지 않다.

## LLVM 없는 코어·수동 등록 검사

다음은 코어만 사용하는 구성과 수동 Reflection 등록을 사용하는 구성을 각각 검사한다.
생성 예제와 Generator를 끄므로 LLVM·Clang SDK를 검색하지 않는다.

```powershell
foreach ($mode in @('core', 'manual')) {
    $manualRegistration = if ($mode -eq 'manual') { 'ON' } else { 'OFF' }
    cmake -S Projects/Archive -B "$buildRoot/$mode" -G 'Visual Studio 18 2026' -A x64 `
        -DCMAKE_CONFIGURATION_TYPES=Release -DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL `
        "-DCMAKE_PREFIX_PATH=$glazePrefix" -DBUILD_TESTING=ON -DARCHIVE_BUILD_EXAMPLE=ON `
        "-DARCHIVE_BUILD_REFLECTION_EXAMPLE=$manualRegistration" -DARCHIVE_BUILD_GENERATED_EXAMPLE=OFF `
        -DARCHIVE_BUILD_GLM_EXAMPLE=OFF -DARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE=OFF `
        -DREFLECTION_BUILD_GENERATOR=OFF -DREFLECTION_BUILD_EXAMPLE=OFF
    if ($LASTEXITCODE -ne 0) { throw "$mode configure failed" }
    cmake --build "$buildRoot/$mode" --config Release -j 2
    if ($LASTEXITCODE -ne 0) { throw "$mode build failed" }
    ctest --test-dir "$buildRoot/$mode" -C Release --output-on-failure --no-tests=error -j 2
    if ($LASTEXITCODE -ne 0) { throw "$mode tests failed" }
}
```

코어 구성은 ArchiveExample 1개, 수동 등록 구성은 ArchiveExample과 ArchiveReflectionExample 2개를 실행한다.
각 구성은 서로 다른 빌드 디렉터리를 사용한다.

## 설치

앞서 생성 등록 구성을 빌드했다면 세 모듈을 하나의 새 prefix에 설치할 수 있다.

```powershell
cmake --install "$buildRoot/generated" --config Release --prefix "$installPrefix"
$generatorExe = Join-Path $buildRoot 'generated/reflection/Release/ReflectionGenerator.exe'
```

설치 결과에는 공개 헤더, ArchiveJson·ReflectionRuntime 정적 라이브러리와 CMake 패키지가 들어간다.
ReflectionArchive는 INTERFACE 라이브러리이므로 별도 정적 라이브러리 파일을 만들지 않는다.
Glaze, Generator 실행 파일과 Clang resource는 이 설치에 포함되지 않는다.
생성 등록 소비자에는 별도 Generator와 resource 경로를 제공한다.

ArchiveJson.lib는 현재 설치 규칙에서 구성별 하위 디렉터리를 사용하지 않는다.
Debug와 Release를 같은 prefix에 차례로 설치하면 라이브러리를 덮어쓸 수 있으므로 설치 경로를 분리한다.
아키텍처와 MSVC Runtime 설정도 다른 라이브러리를 같은 설치 경로에 섞지 않는다.

## 설치된 패키지의 외부 소비자 검사

각 소비자는 설치된 모듈을 find_package로 가져온다.
Archive·ReflectionArchive에는 Glaze prefix도 제공하며, Reflection Runtime만 사용하는 소비자에는 필요하지 않다.

```powershell
$consumerOptions = @(
    '-G', 'Visual Studio 18 2026', '-A', 'x64', '-DCMAKE_CONFIGURATION_TYPES=Release',
    '-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreadedDLL', '-DBUILD_TESTING=ON'
)

cmake -S Projects/Archive/examples/package-consumer -B "$consumerRoot/archive" @consumerOptions `
    "-DCMAKE_PREFIX_PATH=$installPrefix;$glazePrefix"
if ($LASTEXITCODE -ne 0) { throw 'Archive consumer configure failed' }

cmake -S Projects/ReflectionPackageCheck -B "$consumerRoot/runtime" @consumerOptions `
    "-DCMAKE_PREFIX_PATH=$installPrefix" -DREFLECTION_PACKAGE_CHECK_GENERATED=OFF
if ($LASTEXITCODE -ne 0) { throw 'Runtime consumer configure failed' }

cmake -S Projects/ReflectionArchive/examples/package-consumer -B "$consumerRoot/adapter" @consumerOptions `
    "-DCMAKE_PREFIX_PATH=$installPrefix;$glazePrefix" "-DREFLECTION_GENERATOR_EXECUTABLE=$generatorExe" `
    "-DREFLECTION_CLANG_RESOURCE_DIR=$llvmRoot/lib/clang/20"
if ($LASTEXITCODE -ne 0) { throw 'Adapter consumer configure failed' }

cmake -S Projects/ReflectionPackageCheck -B "$consumerRoot/reflection-generated" @consumerOptions `
    "-DCMAKE_PREFIX_PATH=$installPrefix" -DREFLECTION_PACKAGE_CHECK_GENERATED=ON `
    "-DREFLECTION_GENERATOR_EXECUTABLE=$generatorExe" "-DREFLECTION_CLANG_RESOURCE_DIR=$llvmRoot/lib/clang/20"
if ($LASTEXITCODE -ne 0) { throw 'Generated Reflection consumer configure failed' }

foreach ($consumer in @('archive', 'runtime', 'adapter', 'reflection-generated')) {
    cmake --build "$consumerRoot/$consumer" --config Release -j 2
    if ($LASTEXITCODE -ne 0) { throw "$consumer build failed" }
    ctest --test-dir "$consumerRoot/$consumer" -C Release --output-on-failure --no-tests=error -j 2
    if ($LASTEXITCODE -ne 0) { throw "$consumer tests failed" }
}
```

예상하는 CTest 개수는 Archive 단독 1개, Reflection Runtime 단독 1개,
ReflectionArchive 1개, 생성 Reflection 구성 2개다.
소비자 예제는 검증용이며 최종 애플리케이션에 포함할 필요가 없다.

## 실행 검증 기록

2026-10-16에 Engine과 LLVM SDK 폴더를 제외한 별도 소스 배치에서 검증했다.
환경은 Windows x64, Visual Studio 18 2026, MSVC 19.51, CMake 4.2.3이다.

- Debug·Release 각각 코어 CTest 1/1, 수동 등록 2/2, 생성 등록 3/3 통과.
- Debug·Release 각각 설치 Archive 소비자 1/1, Runtime 전용 1/1, ReflectionArchive 1/1, 생성 Reflection 구성 2/2 통과.
- Release 검증에는 명시한 외부 LLVM·Clang 20.1.8 SDK로 새로 빌드한 Release Generator를 사용했다.
- 코어·수동 등록 구성과 설치 소비자는 LLVM·Clang 패키지 검색을 비활성화한 상태로 검증했다.
- 설치 헤더 30개와 정적 라이브러리 2개의 내용이 각 검증 소스·빌드 산출물과 일치했다.

Release Generator 링크에서는 LLVM SDK의 LLVMSupport 링크 설정으로 LNK4199 경고가 발생했다.
해당 SDK가 delay-load 옵션과 `-INCLUDE:malloc`을 하나의 항목으로 전달한다.
Generator의 코드 생성과 그 결과를 사용하는 소비자 실행은 통과했으며, 이 경고도 구현 기록에 남겼다.

## 검증 범위와 다른 구성

검사에는 JSON·BEVE 왕복, 지원 컨테이너 조합과 상속, 필드 정책, 타입·const 접근 오류,
일반적인 실패에서 원본 객체·기존 문서 보존이 포함된다. Release에서도 오류 반환으로 검사한다.
포인터 소유권·동적 파생 객체 생성·객체 그래프 복원·예외의 롤백·파일 저장 원자성은 보장하지 않는다.

Debug 검증은 별도 빌드·설치·소비자 디렉터리에서 수행한다.
CMAKE_CONFIGURATION_TYPES, --config와 CTest의 -C를 모두 Debug로 바꾸고,
모듈과 소비자의 CMAKE_MSVC_RUNTIME_LIBRARY를 MultiThreadedDebugDLL(`/MDd`)로 맞춘다.
외부 Generator 경로도 실제 빌드 산출물의 위치에 맞춘다.

엔진·GLM 소비자 예제는 각각 ARCHIVE_BUILD_ENGINE_COMPONENT_EXAMPLE과 ARCHIVE_BUILD_GLM_EXAMPLE을 켠다.
이때는 Engine 헤더와 해당 소비자 의존성을 별도로 준비해야 한다.
이 문서의 독립 모듈 검증은 엔진·에디터 전체 빌드나 ReflectionEnTT 성능 비교를 의미하지 않는다.
