# Project / Virtual Path / Module

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 경로와 프로젝트
`Engine://`, `Editor://`, `Project://`, `Plugin://`, `Mount://`를 실제 OS 절대 경로 대신 저장 참조로 사용한다. `PlatformPath::ResolveUri(pathOrUri)` 및 `ToVirtualUri(...)`로 경계 변환한다. `RegisterContentMount(const ContentMountDesc&) -> bool`, `UnregisterContentMount(name)`, `HasContentMount(name)`, `IsMountReadOnly(nameOrUri)`를 제공한다. 활성 프로젝트 전환은 Editor의 ProjectSessionManager가 담당한다. 직접 root를 바꾸면 리소스·씬·감시자 정리 절차를 누락할 수 있다.

## C++ 모듈
2026-11-28 변경에서 외부 CMake 소비자는 `BamEngine::Engine`을 사용한다. `bamengine_configure_game_module`은 Windows 모듈 산출물 경로를 `Binaries/Win64/<Editor|Client|Shipping>/<CONFIG>`로 지정한다.

Project Hub의 Create C++ Module을 선택하면 `Source/CMakeLists.txt`, 모듈 소스 및 `.bamproject`의 Modules 항목이 생성된다. 산출물은 `Binaries/Win64`에 위치한다.
```cpp
#include "ModuleInterface.h"
class MyModule final : public Engine::IModule {
public:
    void OnLoad() override { /* register owned systems/types */ }
    void OnUnload() override { /* unregister and release */ }
};
IMPLEMENT_MODULE(MyModule)
```
`OnPreHotReload`와 `OnPostHotReload`는 선택적 hook이다. `ModuleManager::LoadModule(name,dllPath)`, `ReloadModule(name,newDllPath)`, `UnloadModule(name)`의 결과를 확인한다. 게임에서는 Editor가 관리하는 자동 reload와 중복으로 호출하지 않는다.

외부 모듈 구성 예(경로 예시이며 실행 미검증):
```powershell
cmake -S D:/Games/MyGame/Source -B D:/Games/MyGame/Build -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug -DBAMENGINE_ROOT=D:/BamEngine `
  -DBAMENGINE_BUILD_DIR=D:/BamEngine/out/build/x64-ninja-debug
cmake --build D:/Games/MyGame/Build
```
`find_package(BamEngine REQUIRED PATHS "${BAMENGINE_ROOT}/Engine/CMake")`와 `bamengine_configure_game_module(target, projectRoot)`를 scaffold가 사용한다. 같은 compiler/ABI/CRT/config를 사용한다. `BamEngineConfig.cmake`가 Debug를 우선 선택하는 구조(A04), projectTemplate 인자가 무시되는 문제(A05), C++20 scaffold와 실제 엔진 공개 헤더 요구 수준의 호환성은 확인 필요.

## 관련 소스
- [PlatformPath.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Public/PlatformPath.h) — `Engine/Source/Core/Path/Public/PlatformPath.h`
- [ProjectDescriptor.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Public/ProjectDescriptor.h) — `Engine/Source/Core/Path/Public/ProjectDescriptor.h`
- [ModuleInterface.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Module/Public/ModuleInterface.h) — `Engine/Source/Core/Module/Public/ModuleInterface.h`
- [ModuleManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Module/Public/ModuleManager.h) — `Engine/Source/Core/Module/Public/ModuleManager.h`
- [ProjectDescriptor.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Private/ProjectDescriptor.cpp) — `Engine/Source/Core/Path/Private/ProjectDescriptor.cpp`
- [BamEngineConfig.cmake](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/CMake/BamEngineConfig.cmake) — `Engine/CMake/BamEngineConfig.cmake`
- [ProjectSessionManager.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Project/Private/ProjectSessionManager.cpp) — `Editor/Source/Project/Private/ProjectSessionManager.cpp`

## 검증 및 참고 예제
- [VirtualPathCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/VirtualPathCheck.cpp)
