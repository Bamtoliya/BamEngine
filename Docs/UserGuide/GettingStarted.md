# 시작하기

이 안내는 현재 저장소에 있는 Windows 개발 프리셋을 기준으로 합니다. 저장소에는 다른 플랫폼용 프리셋도 있지만, 프리셋이 존재한다는 사실만으로 전체 엔진·에디터 이식이 검증된 것은 아닙니다.

## 필요한 환경

- Windows x64, Visual Studio 18 2026의 MSVC 도구와 Windows SDK
- CMake 3.21 이상, Git, Python 3
- vcpkg manifest mode 및 `vcpkg.json`이 요구하는 패키지
- LLVM/Clang 20.1.8 개발 파일과 Reflection Generator용 Clang resource directory
- DXC와 Vulkan SDK: 셰이더 컴파일 도구 체인
- Qt6 Widgets: 현재 Editor 구성에서 사용하는 의존성
- FMOD SDK 파일: 저장소에서 제외되는 라이브러리·런타임 DLL
- 첫 configure 시 imnodes를 가져오기 위한 GitHub 접근

FMOD 파일은 이 저장소에 포함되지 않습니다. `ThirdParty/FMOD/inc`의 헤더 버전과 맞는 파일을 `ThirdParty/FMOD/lib/x64/`에 준비해야 합니다.

| 구성 | 필요한 파일 |
|---|---|
| Debug | `fmodL_vc.lib`, `fmodL.dll` |
| Release | `fmod_vc.lib`, `fmod.dll` |

LLVM/Clang 파일도 저장소에서 제외될 수 있습니다. SDK를 준비하고 `CMakePresets.json`의 Reflection 경로가 실제 설치와 일치하는지 확인하세요.

## 로컬 vcpkg 경로 설정

현재 `CMakePresets.json`의 Windows preset에는 다음 로컬 절대 경로가 들어 있습니다.

```text
I:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

vcpkg가 다른 위치에 있으면 `windows-base`와 `windows-ninja-base`의 `CMAKE_TOOLCHAIN_FILE`을 설치 경로에 맞춰야 합니다. 이 값은 현재 저장소 설정의 제약이며, 다른 개발자의 경로를 그대로 사용할 수 있다고 가정하지 마세요.

## Editor 구성·빌드

저장소 루트에서 x64 Ninja Debug preset을 사용합니다. MSVC x64 개발 환경이 활성화되어 있고 앞의 SDK 및 로컬 경로가 준비되어 있어야 합니다.

```powershell
cmake --preset x64-ninja-debug
cmake --build --preset x64-ninja-debug --target Editor
```

실행 파일은 단일 구성 Ninja preset에서 다음 경로에 생성됩니다.

```powershell
.\out\build\x64-ninja-debug\bin\Editor.exe
```

저장소의 리소스 경로가 프로젝트 루트를 기준으로 해석되므로 저장소 루트에서 Editor를 실행하세요. 빌드 후 자동 검사는 다음처럼 실행할 수 있습니다.

```powershell
ctest --preset x64-ninja-debug --output-on-failure --no-tests=error
```

2026-11-20 구현 기록에는 사용자가 `x64-ninja-debug` 빌드와 CTest 전체 44/44 통과를 확인한 기록이 있습니다. 이는 새 configure/build나 다른 환경에서의 통과를 보증하지 않습니다.

## 별도 실행 대상

루트 CMake는 비-Emscripten 구성에서 `Client`와 `GameModule` 타깃도 추가합니다. `Client`는 `Config/Project.json`, 시작 씬·리소스 경로와 프로젝트 모듈을 사용하는 별도 실행 경로입니다. Editor를 빌드할 수 있다는 사실만으로 Client 패키징이 검증되는 것은 아닙니다. 패키지 외부 실행, Release 구성 및 플랫폼별 렌더링 검증은 [Client 빌드·패키징 계획](../BuildAndPackagingPlan.md)을 먼저 확인하세요.

## 문제가 생겼을 때

- CMake가 vcpkg toolchain을 찾지 못하면 `CMAKE_TOOLCHAIN_FILE`의 실제 경로를 확인합니다.
- Reflection Generator가 LLVM/Clang을 찾지 못하면 `LLVM_DIR`, `Clang_DIR`, `REFLECTION_CLANG_RESOURCE_DIR`과 SDK 설치 상태를 확인합니다.
- FMOD 링크 오류가 나면 Debug/Release에 맞는 `.lib`가 `ThirdParty/FMOD/lib/x64/`에 있는지 확인합니다.
- Editor 실행 후 리소스 로드가 실패하면 현재 작업 디렉터리가 저장소 루트인지 확인합니다.

정확한 환경별 설치 성공을 이번 문서 작업에서 새로 실행해 확인하지는 않았습니다. 설정·빌드 오류가 지속되면 오류 전체와 사용한 preset을 함께 기록하세요.

## English

[Getting Started in English](GettingStarted.en.md)
