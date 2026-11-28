# 아키텍처 개요

이 문서는 현재 CMake 구성과 주요 실행 경로를 요약합니다. 장기 계획이나 타입 선언만으로 기능 완료를 판정하지 않습니다. 현재 구현과 검증 상태는 각 링크의 코드·계획·구현 기록을 함께 확인하세요.

## CMake 타깃과 의존 방향

```text
Editor ─────┐
Client ─────┼──> Engine
GameModule ─┘
```

- 루트 CMake는 `Engine`, 변환 도구와 Asset Worker를 구성합니다.
- 비-Emscripten 빌드에서는 `Editor`, `GameModule`, `Client`도 추가합니다. `BAMENGINE_WITH_EDITOR`는 기본적으로 활성화됩니다.
- `Editor` 타깃은 에디터 UI와 에셋 도구를 포함합니다.
- `Engine`은 Windows 등 네이티브 구성에서 공유 라이브러리이며 Emscripten 구성에서는 정적 라이브러리로 정의됩니다.
- `Editor`와 `Client`는 각각 엔진 소비자입니다. `GameModule`도 `BamEngine`에 링크됩니다. 런타임·에디터 의존 경계는 [엔진·에디터·클라이언트 지침](../EngineEditorClientGuidelines.md)을 따릅니다.
- 루트의 C++ 기본값은 C++20이며, Archive 등 일부 하위 타깃은 C++23 요구를 선언합니다. 모든 타깃을 한 언어 표준으로 설명하지 않습니다.

## 실행 진입점과 생명주기

### Editor

`Editor/Source/Application/Private/Main.cpp`는 `Application`을 생성하고 `Run()`을 호출합니다. `Application::Initialize()`는 로그·창·Runtime/RHI·기본 리소스·ImGui·로캘·시스템을 초기화하고, 게임 모듈과 마지막 작업 씬을 복원합니다. Windows Editor의 현재 Runtime 설정은 DirectX 12를 사용합니다.

### Client

`Client/Source/Main.cpp`는 프로젝트 루트를 결정하고 `ClientGameRuntime`을 통해 `Config/Project.json`과 시작 설정을 읽습니다. Client 호스트는 SDL 창과 DirectX 12 RHI를 만들고, Runtime·시스템·렌더 파이프라인·시작 씬을 연결합니다.

`Client` 타깃과 시작 경로는 소스에 존재하지만, Client를 만드는 것과 배포 폴더에서 독립 실행을 검증하는 것은 다릅니다. 현재 남은 패키지·Release·플랫폼 검증은 [Client 빌드·패키징 계획](../BuildAndPackagingPlan.md)을 참조하세요.

### Engine Runtime

`Engine/Source/Runtime/Private/Runtime.cpp`의 `Runtime`은 ReflectionRegistry, ModuleManager, TimeManager, InputManager, AudioManager, ResourceManager, SceneManager, SystemManager와 렌더링 관리자를 구성합니다. 호스트는 `FixedUpdate`, `Update`, `LateUpdate`, `Render`를 호출합니다. 렌더 단계는 시스템 제출과 Renderer의 프레임 시작·패스 렌더링·프레임 종료를 조정합니다.

종료는 씬과 모듈을 먼저 해제하고 시스템·리소스·GPU Renderer를 정리한 뒤 ReflectionRegistry를 해제합니다. 이 순서는 씬이 참조하는 런타임 모듈과 GPU 리소스의 수명 관계에 기반합니다.

## 씬·리소스 데이터

- `Scene`은 EnTT registry를 소유하며 엔티티 ID, 계층과 등록 컴포넌트 데이터가 Reflection 기반 Archive 경로로 직렬화됩니다.
- Editor의 `SceneDocument`는 JSON Archive로 저장합니다. 씬 Archive는 현재 버전 1을 기록하며, 모든 포인터·미등록 컴포넌트·임의 컨테이너 참조의 자동 복원을 보장하지 않습니다.
- `ResourceManager`는 타입별 리소스 핸들, slot generation, 참조 수를 관리합니다. 파일 경로·참조 해석과 로딩은 ResourceManager 및 `PlatformPath` 경로 계약을 따릅니다.
- 씬 파일의 직렬화 및 UI 편집 이력은 서로 다른 책임입니다. 씬 Undo/Redo는 Editor가 관리하며 파일 포맷이나 씬 로딩으로 이력을 저장하지 않습니다. 상세 범위는 [EditorUndoRedo.md](../EditorUndoRedo.md)를 참고하세요.
- Archive의 실제 필드 구성, Editor 저장 경계와 로드 검증 순서는 [씬 저장·복원 구조](ScenePersistence.md)를 참고하세요.

## Reflection과 빌드 산출물

`Engine/CMake/ReflectionBuild.cmake`가 `reflection_generate()`를 호출해 Engine 헤더의 반영 정보를 빌드 디렉터리에 생성하고 Engine 타깃에 연결합니다. 생성 파일은 소스 트리 대신 빌드 디렉터리에 놓입니다. Editor는 Engine이 제공하는 등록 정보를 사용합니다.

Reflection Runtime, Generator, Archive 및 ReflectionArchive Adapter는 별도 프로젝트/타깃입니다. 구성과 독립 사용은 [Reflection / Archive 빌드 안내](../ReflectionArchiveBuild.md), 파일 배치·의존 규칙은 [소스 조직 지침](../SourceOrganizationGuidelines.md)을 참고하세요.

## 렌더링 경계

`Renderer`는 RHI 생성 리소스, 렌더 패스별 제출 큐와 카메라를 관리합니다. 현재 Editor와 Windows Client의 기본 초기화 경로는 DirectX 12입니다. RHI 열거형 또는 플랫폼 폴더에 다른 Backend 코드가 있다는 사실만으로 해당 플랫폼의 전체 렌더링 경로가 완성·검증되었다고 볼 수 없습니다.

셰이더 리소스의 바인딩 계약은 [Shader Binding Layout](../Shader/BINDING_LAYOUT.md), 전체 렌더링 개발 상태는 [Rendering Development Plan](../RenderingDevelopmentPlan.md)을 참고하세요.

## 검증 기준

2026-11-20 구현 기록에는 사용자가 `x64-ninja-debug` 빌드와 CTest 44/44 통과를 확인한 것으로 기록되어 있습니다. 이 결과는 해당 검사 집합과 시점의 확인입니다. 최신 UI, GPU, Release, Client 패키지 외부 실행 및 Emscripten/WebGPU에 대한 포괄적 검증으로 확대 해석하지 않습니다. 문서 작성 작업 자체에서도 빌드와 테스트는 다시 실행하지 않았습니다.

## English

[Architecture Overview in English](Architecture.en.md)
