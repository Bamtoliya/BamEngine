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

- [BamEngine](#bamengine)
	- [스크린샷](#스크린샷)
	- [목차](#목차)
	- [프로젝트 소개](#프로젝트-소개)
	- [개발 의도](#개발-의도)
	- [핵심 기능](#핵심-기능)
		- [1) 엔진 런타임 (`Engine`)](#1-엔진-런타임-engine)
		- [2) 에디터 (`Editor`)](#2-에디터-editor)
		- [3) 코드 생성/도구 체인](#3-코드-생성도구-체인)
	- [사용 기술과 채택 의도](#사용-기술과-채택-의도)
		- [핵심 기반](#핵심-기반)
		- [런타임/렌더링](#런타임렌더링)
		- [에디터/콘텐츠 제작](#에디터콘텐츠-제작)
		- [데이터/유틸리티](#데이터유틸리티)
	- [프로젝트 기술 전체 정리 (엔진 내부 포함)](#프로젝트-기술-전체-정리-엔진-내부-포함)
		- [1) 엔진 아키텍처/런타임 모델](#1-엔진-아키텍처런타임-모델)
		- [2) 렌더링 아키텍처 (RHI + RenderPass)](#2-렌더링-아키텍처-rhi--renderpass)
		- [3) UI 시스템 (Canvas / RectTransform / Pivot)](#3-ui-시스템-canvas--recttransform--pivot)
		- [4) 에디터 기술 (ImGui 툴 프레임워크)](#4-에디터-기술-imgui-툴-프레임워크)
		- [5) Reflection 시스템 (Core + Codegen + Adapter)](#5-reflection-시스템-core--codegen--adapter)
		- [6) 리소스/직렬화/압축 파이프라인](#6-리소스직렬화압축-파이프라인)
		- [7) 콘텐츠 제작/임포트 파이프라인](#7-콘텐츠-제작임포트-파이프라인)
		- [8) 자동화/빌드 파이프라인](#8-자동화빌드-파이프라인)
	- [프로젝트 구조](#프로젝트-구조)
	- [개발 환경 및 기본 셋팅](#개발-환경-및-기본-셋팅)
		- [요구사항](#요구사항)
		- [사전 확인](#사전-확인)
	- [빌드 및 실행](#빌드-및-실행)
		- [1) Configure](#1-configure)
		- [2) Build (`Editor` 기준)](#2-build-editor-기준)
		- [3) 실행 파일 확인](#3-실행-파일-확인)
	- [리소스/에셋 파이프라인](#리소스에셋-파이프라인)
	- [Reflection 코드 생성](#reflection-코드-생성)
	- [현재 상태](#현재-상태)
	- [기여 가이드](#기여-가이드)
	- [구현 세부 기록](#구현-세부-기록)
	- [라이선스](#라이선스)

---

## 프로젝트 소개

`BamEngine`은 다음 목표를 가진 엔진 프로젝트입니다.

- **엔진/에디터 분리 구조**: 런타임(`Engine`)과 제작 도구(`Editor`)를 독립적으로 관리
- **데이터 중심 리소스 관리**: `Handle(index + generation)` 기반 리소스 참조
- **자동 Reflection 파이프라인**: 코드 생성 기반 메타데이터/직렬화 확장
- **확장 가능한 모듈 아키텍처**: Render, Resource, Physics, UI, World, System 모듈화

---

## 개발 의도

이 프로젝트는 "작동하는 엔진"을 넘어, **실제 게임 제작 워크플로우를 견딜 수 있는 개발 기반**을 만드는 데 초점을 둡니다.

- **런타임/툴링의 분리**
	- `Engine`은 게임 실행 책임에 집중하고, `Editor`는 제작 경험(씬 구성, 인스펙터, 에셋 파이프라인)에 집중합니다.
	- 결과적으로 런타임 안정성과 툴 확장성을 독립적으로 개선할 수 있습니다.

- **데이터/자동화 중심 개발**
	- Reflection 코드 생성, Localization CSV->JSON 변환, Shader SPIR-V 컴파일을 CMake 단계에 통합해 반복 작업을 줄입니다.
	- 사람 손으로 관리하던 메타코드/리소스 변환을 자동화해 유지보수 비용을 낮추는 것이 목적입니다.

- **장기 확장을 위한 모듈 경계 유지**
	- Render/Resource/World/System/Physics를 분리해 기능 추가 시 영향 범위를 국소화합니다.
	- 특정 렌더링 백엔드나 임포터 파이프라인 교체를 고려한 구조를 지향합니다.

---

## 핵심 기능

### 1) 엔진 런타임 (`Engine`)

- **렌더링 파이프라인**: RenderPass / RenderTarget / RHI 계층 분리
- **Vulkan 기반 셰이더 처리**: `glslc`로 `.vert/.frag -> .spv` 자동 컴파일
- **월드/오브젝트 시스템**: `GameObject`, `ComponentRegistry` 기반 컴포넌트 관리
- **리소스 시스템**: `ResourceManager`, `ResourceHandle<T>`를 통한 안전한 리소스 참조 관리
- **기본 물리/충돌 구성**: Collider/RigidBody 계열 컴포넌트 및 충돌 매니저 구조
- **직렬화 지원**: 공통 `IArchive` 인터페이스를 통한 JSON / BEVE 저장·로드 API 제공
- **Reflection 연동**: 생성된 컴포넌트 메타데이터를 Inspector와 컴포넌트 직렬화에서 활용

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

- **Reflection Generator**: LLVM/Clang 전처리기와 AST를 이용해 어노테이션이 있는 헤더를 분석하고 등록용 C++ 코드 생성
- **CMake 통합**: `reflection_generate()`로 코드 생성과 대상 타깃의 컴파일 연결
- **ReflectionArchive**: Reflection 메타데이터와 Archive의 직렬화 API 연결
- `Tools/TextureConverter`를 통한 텍스처 변환 지원

---

## 사용 기술과 채택 의도

아래 항목은 루트 `CMakeLists.txt`, `Engine/CMakeLists.txt`, `Editor/CMakeLists.txt`, `vcpkg.json` 기준입니다.

### 핵심 기반

| 기술 | 적용 위치 | 채택 의도 |
|---|---|---|
| C++23 | 전체 | 성능 중심 런타임 + 현대 C++ 문법/타입 안정성 확보 |
| CMake 3.16+ | 전체 | 멀티 타깃(Engine DLL, Editor EXE, Tools) 빌드 오케스트레이션 |
| vcpkg manifest | 전체 | 팀 단위 의존성 버전 고정 및 재현 가능한 개발 환경 구성 |
| Unity Build + PCH | Engine, Editor | 빌드 시간 단축 및 반복 개발 속도 개선 |

### 런타임/렌더링

| 기술 | 적용 위치 | 채택 의도 |
|---|---|---|
| SDL3 | Engine | 플랫폼 윈도우/입력 계층 표준화 |
| Vulkan | Engine | 저수준 그래픽 제어와 확장 가능한 렌더링 백엔드 구성 |
| glslc + glslang + SPIRV-Tools + SPIRV-Cross | Engine | GLSL -> SPIR-V 컴파일 및 크로스 백엔드 대응 가능성 확보 |
| glm | Engine | 수학 연산(벡터/행렬/쿼터니언) 표준화 |

### 에디터/콘텐츠 제작

| 기술 | 적용 위치 | 채택 의도 |
|---|---|---|
| Dear ImGui + ImGuizmo | Editor | 빠른 툴 UI 제작과 트랜스폼 기즈모 제공 |
| assimp | Editor | 모델 임포트 파이프라인 구축 |
| efsw | Editor | 파일 변경 감지 기반 에셋 갱신 자동화 |
| Python 스크립트 | Editor, Reflection | Localization 변환/Reflection 코드 생성 자동화 |

### 데이터/유틸리티

| 기술 | 적용 위치 | 채택 의도 |
|---|---|---|
| glaze | Engine 계열 직렬화 경로 | 경량 JSON 직렬화/역직렬화 처리 |
| lz4 | Engine | 빠른 압축 경로 구성 |
| fmt | 전체 | 안전하고 일관된 문자열 포맷팅 |
| stb, directxtex | 리소스/도구 | 이미지/텍스처 처리 유틸리티 |
| cppcodec | 리소스/유틸 | 인코딩 처리 보조 |

---

## 프로젝트 기술 전체 정리 (엔진 내부 포함)

이 섹션은 단순 외부 라이브러리 목록이 아니라, 코드베이스에 구현된 내부 기술(아키텍처/런타임 정책/에디터 파이프라인)까지 포함해 정리합니다.

### 1) 엔진 아키텍처/런타임 모델

- **모듈 분리**: Core, World, Render, Resource, Physics, UI, System
- **컴포넌트 기반 오브젝트 모델**: `GameObject + Component` 조합
- **매니저 중심 서브시스템**: Scene/Layer/Resource/RenderPass/RenderTarget/Physics/Input 등
- **핸들 기반 리소스 참조**: 세대(generation) 기반 stale handle 완화

### 2) 렌더링 아키텍처 (RHI + RenderPass)

- **RHI 추상화 계층**
	- 공통 인터페이스: Buffer / Texture / Sampler / Shader / Pipeline 생성 및 바인딩
	- 플랫폼 구현: `SDLGPU` 백엔드, `Vulkan` 경로(확장 구조 포함)
- **RenderPass + RenderTarget 분리 설계**
	- 패스 등록/정렬/로드-스토어 정책/블렌드 모드 관리
	- 뷰포트 단위로 카메라-패스 매핑 및 커스텀 커맨드 제출
- **셰이더 파이프라인**
	- GLSL 소스 -> `glslc` -> SPIR-V 자동 컴파일
	- `spirv-cross` 기반 리플렉션/크로스 타깃 대응 기반

### 3) UI 시스템 (Canvas / RectTransform / Pivot)

- **ECS UI 편집기 기반**
	- `Core/Public/Components/UI`의 CanvasComponent / RectTransformComponent / UIImageComponent
	- Engine UILayout과 UIRenderSystem을 사용한 캔버스 이미지 렌더링
	- UI View에서 Canvas/Image 생성, 화면 영역·그리드 표시, 팬·줌, 선택·이동·크기 조절과 스냅
	- 사용법·검증 상태·현재 제한: [UIEditorFoundation.md](Docs/UIEditorFoundation.md)
	- UI 버튼·포인터 이벤트와 Interact 미리보기: [UIInteraction.md](Docs/UIInteraction.md)
	- Z-Index와 앞/뒤 배치, 그리기·선택 순서: [UIRenderOrder.md](Docs/UIRenderOrder.md)

아래 항목은 기존 GameObject UI 구현이다. ECS 편집기는 위의 새 데이터 경로를 사용한다.

- **UICanvas 정책 기반 레이아웃**
	- RenderMode: ScreenSpace / WorldSpace / ScreenSpaceCamera
	- LayoutSource: InheritParent / ReferenceResolution / ExternalOverride
	- ScaleMode: ConstantPixelSize / ScaleWithScreenSize / ConstantPhysicalSize
- **RectTransform 2D 레이아웃 시스템**
	- AnchorMin/AnchorMax, AnchoredPosition, Size, Scale, Rotation
	- Pivot preset/Anchor preset, Lock/IgnoreLayout/RaycastTarget 플래그
- **에디터 UI 뷰포트 합성 기술**
	- VirtualCanvas RenderTarget에 UI를 먼저 렌더링
	- 최종 뷰포트 이미지에 Canvas Overlay를 AddImageQuad로 합성
	- 기존 VirtualCanvas 오버레이는 ECS 캔버스 편집 경로로 전환되었다.

### 4) 에디터 기술 (ImGui 툴 프레임워크)

- **Docking/Viewports 활성화된 Dear ImGui 환경**
- **핵심 패널**: Hierarchy, Inspector, ContentBrowser, Scene/Game/UI Viewport, ToolBar
- **ImGuizmo 기반 편집 기즈모**
	- Translate/Rotate/Scale, Local/World, Snap 옵션
- **Selection/피킹/입력 연동**
	- 엔진 Input 시스템과 에디터 조작 흐름 통합

### 5) Reflection 시스템 (Core + Codegen + Adapter)

- **Reflection Runtime — `Projects/Reflection`**
	- 엔진·에디터에 의존하지 않는 C++20 리플렉션 모듈
	- `Registry`, `TypeInfo`, `PropertyInfo`, `FunctionInfo`, `EnumInfo` 제공
	- `ObjectView`와 `ValueView`를 통한 객체·값 접근
	- 프로퍼티 접근, 함수 호출, enum 정보와 지원하는 STL 컨테이너 접근 제공
	- 사용자 정의 메타데이터를 저장하며, 편집·직렬화 정책은 소비자가 해석

- **Reflection Generator**
	- `CLASS`, `STRUCT`, `PROPERTY`, `FUNCTION`, `ENUM` 어노테이션 분석
	- LLVM/Clang 전처리기와 AST를 이용해 선언과 메타데이터 수집
	- 모듈별 `*.gen.cpp`, `*.gen.h`와 의존성 파일 `*.gen.d` 생성
	- 생성된 등록 함수를 호출해 소비자가 소유한 Registry에 정보 등록

- **엔진·에디터 연결**
	- 코어 컴포넌트 헤더에서 `BamCoreComponents` 모듈 생성
	- `ComponentReflectionBindings`로 Reflection 타입과 EnTT 컴포넌트 접근 연결
	- Inspector에서 엔티티의 등록된 컴포넌트를 순회하고 프로퍼티 편집

- **ReflectionArchive — `Projects/ReflectionArchive`**
	- `ReflectionArchive::Adapter` 타깃으로 Reflection과 Archive 연결
	- 프로퍼티 메타데이터와 타입별 코덱을 이용해 저장·복원 처리
	- GLM 등 소비자 전용 타입의 직렬화는 별도 어댑터로 연결

### 6) 리소스/직렬화/압축 파이프라인

- **리소스 타입**: Texture, Shader, Material, Mesh, Skeleton, Sprite 등

- **Archive — `Projects/Archive`**
	- 엔진·에디터·Reflection·GLM에 의존하지 않는 C++23 직렬화 모듈
	- 공통 `IArchive` 인터페이스와 `GlazeArchiveBase` 기반 구현
	- `JsonArchive`: JSON 문서 분석과 출력
	- `BeveArchive`: BEVE 바이너리 문서 분석과 출력
	- `ArchiveResult`: 오류 코드와 오류가 발생한 필드 위치 제공
	- 스칼라, 문자열, enum, 지원하는 STL 컨테이너와 콜백 기반 객체 직렬화
	- CMake 타깃 `Archive::Json`은 JSON과 BEVE 구현을 포함하는 정적 라이브러리

- **엔진 연결**
	- 리소스의 `WriteArchive()` / `ReadArchive()`로 데이터 저장·복원
	- ResourceManager에서 `.json`과 `.beve` 리소스 로드
	- Reflection 기반 컴포넌트 저장·복원과 엔티티 컴포넌트 저장 경로 제공
	- 기존 Binary 리소스 저장·로드 경로는 현재 지원하지 않음

- **압축**: `lz4` 기반 압축·해제

### 7) 콘텐츠 제작/임포트 파이프라인

- **모델 임포트**: assimp
- **텍스처/이미지 처리**: stb, DirectXTex, TextureConverter 도구
- **파일 변경 감시**: efsw (에셋 갱신/브라우저 연동)
- **로컬라이제이션**: CSV -> JSON 자동 변환 스크립트

### 8) 자동화/빌드 파이프라인

- **CMake 타깃 구조**: Engine(Shared), Editor(Executable), TextureConverter(Tool)
- **빌드 생산성 최적화**: Unity Build, PCH, MSVC 병렬 컴파일 옵션
- **의존성 재현성**: vcpkg manifest + CMake Toolchain 기반 고정
- **코드 생성 통합**
	- Reflection bundle 생성
	- Localization 생성
	- Shader SPIR-V 컴파일

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
│  ├─ Reflection/           # Runtime, LLVM/Clang 기반 Generator, CMake 생성 함수
│  ├─ Archive/              # 공통 Archive API와 JSON / BEVE 구현
│  └─ ReflectionArchive/    # Reflection과 Archive 연결 어댑터
├─ Tools/
│  └─ TextureConverter/
├─ Resources/               # 셰이더/로컬라이제이션/기본 리소스
└─ CMakeLists.txt
```

---

## 개발 환경 및 기본 셋팅

### 요구사항

- Windows (현재 Windows 프리셋 기준)
- Visual Studio 18 2026 및 MSVC C++ 툴체인
- CMake 3.21 이상 (프로젝트 선언 기준)
- C++23 지원 컴파일러
	- Reflection Runtime과 Generator는 C++20 사용
	- Archive와 ReflectionArchive는 C++23 요구
- LLVM/Clang 20.1.8 개발 라이브러리와 대응하는 Clang resource 디렉터리
- Python 3.x (Localization 스크립트 실행)
- Vulkan SDK (`glslc` 필요)

### 사전 확인

현재 빌드 설정은 다음 경로를 사용합니다.

- `CMakePresets.json`
	- `CMAKE_TOOLCHAIN_FILE`: `I:/vcpkg/scripts/buildsystems/vcpkg.cmake`
	- `REFLECTION_CLANG_RESOURCE_DIR`: `${sourceDir}/Projects/Reflection/dependencies/llvm-20.1.8/lib/clang/20`
- `Projects/Reflection/cmake/ReflectionGenerator.cmake`
	- LLVM 패키지: `Projects/Reflection/dependencies/llvm-20.1.8/lib/cmake/llvm`
	- Clang 패키지: `Projects/Reflection/dependencies/llvm-20.1.8/lib/cmake/clang`

다른 개발 환경에서는 vcpkg와 LLVM/Clang SDK 경로를 확인하고 조정해야 합니다.
현재 Windows 프리셋의 생성기는 `Visual Studio 18 2026`입니다.

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

엔진은 `Engine/CMake/ReflectionBuild.cmake`에서 `reflection_generate()`를 호출합니다.
분석 대상은 `Engine/Source/Core/Public/Components/*.h`이며,
생성 모듈 이름은 `BamCoreComponents`입니다.

생성 파일은 소스 디렉터리가 아닌 빌드 디렉터리에 배치됩니다.
`x64-debug` 프리셋의 Debug 구성에서는 다음 경로를 사용합니다.

```text
out/build/x64-debug/Engine/reflection/Engine/BamCoreComponents/Debug/
├─ BamCoreComponents.gen.cpp
├─ BamCoreComponents.gen.h
└─ BamCoreComponents.gen.d
```

생성된 C++ 파일은 Engine 타깃에 포함되어 컴파일됩니다.
엔진은 `reflection_generated::Register_BamCoreComponents()`를 호출해
Registry에 정보를 등록하고, 생성된 타입 목록으로 컴포넌트 바인딩을 구성합니다.

에디터 Inspector는 엔진이 제공하는 Registry와 컴포넌트 바인딩을 사용합니다.
현재 Editor 타깃은 별도의 Reflection 코드 생성 단계를 실행하지 않습니다.

관련 모듈:

- `Projects/Reflection`: Runtime과 Generator
- `Projects/Archive`: JSON / BEVE 직렬화
- `Projects/ReflectionArchive`: Reflection 기반 직렬화 어댑터

---

## 현재 상태

- 본 저장소는 **WIP(Work In Progress)** 상태입니다.
- 새 Reflection Runtime과 Generator가 엔진 코어 컴포넌트 및 에디터 Inspector에 연결되어 있습니다.
- Archive의 JSON / BEVE 구현과 Reflection 기반 컴포넌트 직렬화 경로가 있습니다.
- 씬 저장 경로는 구현되어 있으며, 씬 로드와 플레이 종료 후 씬 복원은 아직 미구현입니다.
- 기존 Binary 리소스 직렬화 경로는 현재 지원하지 않습니다.

---

## 기여 가이드

- 이슈/PR 환영
- 코딩 스타일은 기존 C++ 코드 스타일과 CMake 구조를 따릅니다.
- 대규모 변경 전에는 이슈로 설계/목표를 먼저 공유해 주세요.

---

## 구현 세부 기록

- 상세 구현 히스토리 및 기술 부채 추적 문서: `Docs/ImplementationLog.md`
- 기능 단위 변경 시 템플릿 기반으로 기록을 누적하는 것을 권장합니다.

---

## 라이선스

저장소 루트에 별도 라이선스 파일이 명시되어 있지 않습니다.  
배포/상업적 이용 전 라이선스 정책을 먼저 확정하세요.
