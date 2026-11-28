# C++ 프로젝트 개발

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 생성된 모듈

Project Hub에서 C++ 모듈 생성 옵션을 선택한다. 스캐폴드의 진입점은 `IModule`과 `IMPLEMENT_MODULE`이다. 아래 코드는 생성기에서 확인한 구조를 일반화한 예시다.

```cpp
#include "ModuleInterface.h"
#include "Logger.h"

class SandboxModule final : public Engine::IModule
{
public:
    void OnLoad() override
    {
        BAM_LOG(Info, "SandboxModule", "Lifecycle", "Loaded.");
        // 필요한 프로젝트 시스템/타입을 등록한다.
    }

    void OnUnload() override
    {
        // 등록한 시스템/이벤트/타입을 정리한다.
        BAM_LOG(Info, "SandboxModule", "Lifecycle", "Unloaded.");
    }
};

IMPLEMENT_MODULE(SandboxModule)
```

이 예시는 모듈 로드/언로드 로그를 확인하기 위한 것이며 매 프레임 갱신을 자동 등록하지 않는다. 실제 프로젝트 descriptor, CMake 타깃, DLL 이름, 클래스 이름을 생성된 파일과 맞춘다.

`IModule`은 `OnPreHotReload` / `OnPostHotReload` 훅도 제공한다. DLL 코드 주소를 참조하는 이벤트, 그래프 함수, 객체 인스턴스는 모듈 언로드 전에 정리한다.

## 모듈 빌드

생성된 `Source/CMakeLists.txt`는 `find_package(BamEngine REQUIRED PATHS "${BAMENGINE_ROOT}/Engine/CMake")`를 사용한다. 외부 모듈의 `bamengine_configure_game_module`은 `BamEngine::Engine`을 링크하고 `Binaries/Win64/<역할>/<구성>`에 출력한다.

아래는 SDK에 정의된 변수를 사용하는 별도 프로젝트 configure/build 예시다. 실제 실행은 하지 않았으며 MSVC 개발 환경 및 빌드된 엔진이 필요하다.

```powershell
# 프로젝트 루트에서 실행 (경로는 로컬 설치에 맞게 변경)
$env:BAMENGINE_ROOT = 'D:/Dev/BamEngine'
cmake -S Source -B .bam/module-build -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DBAMENGINE_ROOT=D:/Dev/BamEngine `
  -DBAMENGINE_BUILD_DIR=D:/Dev/BamEngine/out/build/x64-ninja-debug
cmake --build .bam/module-build
```

엔진과 모듈의 ABI, 플랫폼, Debug/Release, MSVC 런타임 및 엔진 SDK 의존성을 맞춘다. SDK가 찾는 Engine.lib/DLL을 확인한다. Debug/Release 출력 폴더가 동일할 수 있으므로 서로 다른 구성을 동시에 덮어쓰지 않는다.

에디터 하단에는 모듈 빌드 및 자동 컴파일, 모듈 리로드, 셰이더 관련 제어 경로가 있다. 빌드/리로드 상태 및 Console 로그를 확인한다. 구조체 레이아웃 변경 등 위험한 변경은 PIE를 멈추고 필요시 에디터를 재시작한다.

## 엔티티 API 및 네이티브 스크립트

`Entity`, `Scene`, `NativeScriptComponent`, `ScriptableEntity` 헤더에서 확인한 API를 사용한 최소 예시(별도 빌드 검증 없음):

```cpp
#include "Scene.h"
#include "NativeScriptComponent.h"

class ExampleScript final : public Engine::ScriptableEntity
{
public:
    void OnCreate() override { elapsed = 0.0f; }
    void OnUpdate(float dt) override { elapsed += dt; }
    void OnDestroy() override {}
private:
    float elapsed = 0.0f;
};

void AttachExample(Engine::Scene& scene)
{
    auto& entity = scene.CreateEntity();
    auto& native = scene.AddComponent<Engine::NativeScriptComponent>(entity);
    native.Bind<ExampleScript>();
}
```

위 함수는 유효한 실행용 씬을 얻은 호출자에서 사용한다. `Bind<T>()`는 스크립트 인스턴스 팩토리를 설정하는 C++ 경로다. 엔진이 실제로 이 씬의 스크립트 갱신을 수행하는 시점과 등록 위치를 확인해야 한다. 팩토리나 인스턴스가 씬 JSON으로 자동 영속화된다고 가정하지 않는다. 필요한 경우 프로젝트의 모듈 등록/그래프/씬 초기화 흐름으로 다시 연결한다.

ECS의 레지스트리 접근은 `Scene::GetRegistry()`를 사용할 수 있다. 엔티티 핸들은 해당 씬과 수명이 끝나면 유효하지 않을 수 있다. 외부 저장 파일에는 실행 핸들 자체를 영구 ID처럼 저장하지 않는다.

## Reflection을 이용한 확장

엔진의 컴포넌트 선언 패턴을 참고한다.

```cpp
#include "ComponentAnnotations.h"

namespace Sandbox
{
    STRUCT(COMPONENT)
    struct PlayerStats
    {
        REFLECT_BODY()
        PROPERTY(EDITABLE)
        float MoveSpeed = 5.0f;
        PROPERTY(EDITABLE)
        int Health = 100;
    };
}
```

위 코드는 어노테이션 패턴 설명이다. 실제 프로젝트에서 컴포넌트 등록·Inspector 표시·직렬화를 위해서는 해당 헤더를 Reflection Generator 입력에 포함하고 생성 결과를 모듈 빌드 및 등록/해제 과정에 연결해야 한다. 생성된 기본 모듈이 사용자 타입의 모든 Reflection 연동을 자동으로 수행한다고 보장하지 않는다. `Engine/CMake/BamEngineConfig.cmake`, `Docs/EngineReflectionIntegration.md`, `Docs/ReflectionArchiveBuild.md`를 참고한다.

리플렉션 가능 여부, Inspector 편집 가능 여부, 직렬화 가능 여부, Graph 노드 노출은 별도 정책이다. 새로운 구조체는 저장 후 복원, 누락 필드 기본값, Undo/Redo, 모듈 리로드를 확인한다.

## 관련 문서 및 소스

- [EngineEditorClientGuidelines.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/EngineEditorClientGuidelines.md)
- [`ProjectDescriptor.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Private/ProjectDescriptor.cpp) — `Engine/Source/Core/Path/Private/ProjectDescriptor.cpp`
- [`Scene.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Scene/Public/Scene.h) — `Engine/Source/World/Scene/Public/Scene.h`
- [`Entity.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/Entity.h) — `Engine/Source/World/Entity/Public/Entity.h`
- [`NativeScriptComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/NativeScriptComponent.h) — `Engine/Source/World/Entity/Public/NativeScriptComponent.h`
- [`ScriptableEntity.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/ScriptableEntity.h) — `Engine/Source/World/Entity/Public/ScriptableEntity.h`
- [`ModuleInterface.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Module/Public/ModuleInterface.h) — `Engine/Source/Core/Module/Public/ModuleInterface.h`
- [`BamEngineConfig.cmake`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/CMake/BamEngineConfig.cmake) — `Engine/CMake/BamEngineConfig.cmake`
- [전체 근거 및 검토 범위](Sources.md)

