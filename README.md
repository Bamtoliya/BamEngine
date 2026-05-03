# BamEngine

[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C.svg)](https://cmake.org/)
[![vcpkg](https://img.shields.io/badge/vcpkg-manifest-5C2D91.svg)](https://github.com/microsoft/vcpkg)
[![Vulkan](https://img.shields.io/badge/API-Vulkan-A41E22.svg)](https://www.vulkan.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows-informational.svg)](#개발-환경-및-기본-셋팅)
[![Status](https://img.shields.io/badge/Status-WIP-orange.svg)](#현재-상태)

> C++23 기반의 모듈형 게임 엔진 + 에디터 프로젝트  
> `Engine`(런타임)와 `Editor`(툴링)를 분리하고, Reflection 코드 생성 파이프라인으로 생산성을 높이는 것을 목표로 합니다.

## 스크린샷

> 에디터 메인 화면 이미지를 준비 중입니다.  
> 아래 경로에 이미지를 추가하면 README에서 바로 노출됩니다.

```text
Docs/Images/editor-main.png
```

![BamEngine Editor Screenshot](Docs/Images/editor-main.png)

## 목차

- [스크린샷](#스크린샷)
- [프로젝트 소개](#프로젝트-소개)
- [핵심 기능](#핵심-기능)
- [기술 스택 및 라이브러리](#기술-스택-및-라이브러리)
- [프로젝트 구조](#프로젝트-구조)
- [개발 환경 및 기본 셋팅](#개발-환경-및-기본-셋팅)
- [빌드 및 실행](#빌드-및-실행)
- [리소스/에셋 파이프라인](#리소스에셋-파이프라인)
- [Reflection 코드 생성](#reflection-코드-생성)
- [현재 상태](#현재-상태)
- [기여 가이드](#기여-가이드)

---

## 프로젝트 소개

`BamEngine`은 다음 목표를 가진 엔진 프로젝트입니다.

- **엔진/에디터 분리 구조**: 런타임(`Engine`)과 제작 도구(`Editor`)를 독립적으로 관리
- **데이터 중심 리소스 관리**: `Handle(index + generation)` 기반 리소스 참조
- **자동 Reflection 파이프라인**: 코드 생성 기반 메타데이터/직렬화 확장
- **확장 가능한 모듈 아키텍처**: Render, Resource, Physics, UI, World, System 모듈화

---

## 핵심 기능

### 1) 엔진 런타임 (`Engine`)

- **렌더링 파이프라인**: RenderPass / RenderTarget / RHI 계층 분리
- **Vulkan 기반 셰이더 처리**: `glslc`로 `.vert/.frag -> .spv` 자동 컴파일
- **월드/오브젝트 시스템**: `GameObject`, `ComponentRegistry` 기반 컴포넌트 관리
- **리소스 시스템**: `ResourceManager`, `ResourceHandle<T>`를 통한 안전한 리소스 참조 관리
- **기본 물리/충돌 구성**: Collider/RigidBody 계열 컴포넌트 및 충돌 매니저 구조
- **직렬화 지원**: Json / Binary / Beve 저장/로드 경로 제공

### 2) 에디터 (`Editor`)

- **ImGui 기반 툴 UI**
- **핵심 패널 구성**
	- Viewport
	- Hierarchy
	- Inspector
	- Content Browser
	- ToolBar / SceneControlBar
- **선택 시스템**: `SelectionManager` 기반 다중 선택/선택 해제/레이 피킹
- **에셋 임포터**: Model / Texture / Shader / Sprite / Animation 임포트 구조
- **로컬라이징 파이프라인**: CSV -> JSON 자동 변환

### 3) 코드 생성/도구 체인

- `ReflectionCore`, `ReflectionCodegen`, `ReflectionBamAdapter`를 통한 리플렉션 코드 자동 생성
- `Tools/TextureConverter`를 통한 텍스처 변환 지원

---

## 기술 스택 및 라이브러리

### 언어/빌드

- **C++23**
- **CMake (최소 3.16)**
- **vcpkg manifest 모드**

### 주요 라이브러리 (`vcpkg.json` 기준)

- `fmt`: 포맷팅/출력
- `sdl3` (+ `vulkan`): 플랫폼/윈도우/입력 계층
- `glm`: 수학 라이브러리
- `vulkan`: 렌더링 백엔드
- `imgui`, `imguizmo`: 에디터 UI/기즈모
- `assimp`: 모델 임포트
- `stb`: 이미지 유틸리티
- `glslang`, `spirv-tools`, `spirv-cross`: 셰이더/SPIR-V 툴체인
- `efsw`: 파일 시스템 감시
- `cppcodec`: 인코딩 유틸리티
- `directxtex`: 텍스처 처리
- `lz4`: 압축

---

## 프로젝트 구조

```text
BamEngine/
├─ Engine/                  # 런타임 엔진 (DLL)
│  └─ Source/
│     ├─ Core/
│     ├─ Render/
│     ├─ Resource/
│     ├─ Physics/
│     ├─ World/
│     ├─ UI/
│     ├─ System/
│     └─ Generated/
├─ Editor/                  # 에디터 애플리케이션 (EXE)
│  └─ Source/
│     ├─ Application/
│     ├─ ImGui/
│     ├─ AssetManager/
│     ├─ Selection/
│     └─ Generated/
├─ Projects/
│  ├─ ReflectionCore/
│  ├─ ReflectionCodegen/
│  ├─ ReflectionBamAdapter/
│  └─ ReflectionCMake/
├─ Tools/
│  └─ TextureConverter/
├─ Resources/               # 셰이더/로컬라이제이션/기본 리소스
└─ CMakeLists.txt
```

---

## 개발 환경 및 기본 셋팅

### 요구사항

- Windows (현재 프리셋 기준)
- Visual Studio (MSVC C++ 툴체인)
- CMake 3.16+
- Python 3.x (Reflection/Localization 스크립트 실행)
- Vulkan SDK (`glslc` 필요)

### 사전 확인

`CMakePresets.json`에 아래 항목이 하드코딩되어 있습니다.

- `CMAKE_TOOLCHAIN_FILE`: `I:/vcpkg/scripts/buildsystems/vcpkg.cmake`

환경에 맞게 경로를 수정해야 합니다.

---

## 빌드 및 실행

### 1) Configure

```powershell
cmake --preset x64-debug
```

### 2) Build (`Editor` 기준)

```powershell
cmake --build .\out\build\x64-debug --config Debug --target Editor
```

### 3) 실행 파일 확인

```powershell
Get-ChildItem -Path .\out\build\x64-debug -Recurse -Filter Editor*.exe
```

> 참고  
> 루트 `CMakeLists.txt` 기준 현재 기본 타깃은 `Engine`, `TextureConverter`, `Editor`입니다.  
> `Client`는 주석 처리되어 있습니다.

---

## 리소스/에셋 파이프라인

- 리소스는 `ResourceManager`를 통해 로드/등록/조회/해제
- `Handle(index + generation)` 구조로 stale handle 문제를 완화하도록 설계
- 에디터 임포터를 통해 에셋을 엔진 포맷으로 변환/등록
- Localization CSV를 JSON으로 변환하는 자동화 타깃 포함 (`GenerateLocalization`)

---

## Reflection 코드 생성

- CMake 단계에서 `add_reflection_bundle(...)`로 엔진/에디터 반사 코드 생성
- 생성 파일
	- `Engine/Source/Generated/EngineReflection.gen.cpp`
	- `Editor/Source/Generated/EditorReflection.gen.cpp`
- 관련 프로젝트
	- `Projects/ReflectionCore`
	- `Projects/ReflectionCodegen`
	- `Projects/ReflectionBamAdapter`

---

## 현재 상태

- 본 저장소는 **WIP(Work In Progress)** 상태입니다.
- 렌더링/에디터/리소스/리플렉션 파이프라인을 중심으로 지속 확장 중입니다.

---

## 기여 가이드

- 이슈/PR 환영
- 코딩 스타일은 기존 C++ 코드 스타일과 CMake 구조를 따릅니다.
- 대규모 변경 전에는 이슈로 설계/목표를 먼저 공유해 주세요.

---

## 라이선스

저장소 루트에 별도 라이선스 파일이 명시되어 있지 않습니다.  
배포/상업적 이용 전 라이선스 정책을 먼저 확정하세요.