# BamEngine

[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.21%2B-064F8C.svg)](https://cmake.org/)
[![vcpkg](https://img.shields.io/badge/vcpkg-manifest-5C2D91.svg)](https://github.com/microsoft/vcpkg)
[![Graphics](https://img.shields.io/badge/API-DirectX12%20%2F%20SDL_GPU-A41E22.svg)](#2-렌더링-아키텍처-rhi--renderpass)
[![Platform](https://img.shields.io/badge/Platform-Windows-informational.svg)](#개발-환경-및-기본-셋팅)
[![Status](https://img.shields.io/badge/Status-WIP-orange.svg)](#현재-상태)

> C++23 기반의 모듈형 게임 엔진 + 에디터 프로젝트  
> `Engine`(런타임)와 `Editor`(툴링)를 분리하고, Reflection 코드 생성 파이프라인으로 생산성을 높이는 것을 목표로 합니다.

현재는 ECS 씬 편집·저장·불러오기, UI 캔버스 편집, 씬 Undo/Redo와 Logic 그래프 실행 미리보기를 개발하고 있습니다.
아래 구현 범위는 **현재 작업 공간의 코드 기준**이며, 실행 검증 범위와 남은 작업은 [현재 상태](#현재-상태)에 구분합니다.

## 스크린샷

에디터 메인 화면 이미지는 아직 등록되지 않았습니다. 이미지 추가 위치와 규칙은 [이미지 가이드](Docs/Images/README.md)를 참고합니다.

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
		- [9) 그래프 시스템 (Logic / Shader)](#9-그래프-시스템-logic--shader)
	- [프로젝트 구조](#프로젝트-구조)
	- [개발 환경 및 기본 셋팅](#개발-환경-및-기본-셋팅)
		- [요구사항](#요구사항)
		- [사전 확인](#사전-확인)
	- [빌드 및 실행](#빌드-및-실행)
		- [1) Configure](#1-configure)
		- [2) Build (`Editor` 기준)](#2-build-editor-기준)
		- [3) 실행 파일 확인](#3-실행-파일-확인)
		- [4) 선택 검사 타깃](#4-선택-검사-타깃)
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
	- Reflection 코드 생성, Localization CSV->JSON 변환, HLSL의 DXIL/SPIR-V 컴파일을 CMake 단계에 통합해 반복 작업을 줄입니다.
	- 사람 손으로 관리하던 메타코드/리소스 변환을 자동화해 유지보수 비용을 낮추는 것이 목적입니다.

- **장기 확장을 위한 모듈 경계 유지**
	- Render/Resource/World/System/Physics를 분리해 기능 추가 시 영향 범위를 국소화합니다.
	- 특정 렌더링 백엔드나 임포터 파이프라인 교체를 고려한 구조를 지향합니다.

---

## 핵심 기능

### 1) 엔진 런타임 (`Engine`)

- **렌더링 파이프라인**: RenderPass / RenderTarget / RHI 계층 분리
- **셰이더 처리**: DXC로 HLSL을 DXIL과 SPIR-V로 자동 컴파일
- **월드/오브젝트 시스템**: EnTT 기반 `Entity`·컴포넌트·계층 관리와 기존 `GameObject` 경로 병존
- **리소스 시스템**: `ResourceManager`, `ResourceHandle<T>`를 통한 세대 기반 핸들 관리
- **기본 물리/충돌 구성**: Collider/RigidBody 계열 컴포넌트 및 충돌 매니저 구조
- **직렬화 지원**: 공통 `IArchive` 인터페이스를 통한 JSON / BEVE 저장·로드 API 제공
- **Reflection 연동**: 생성된 컴포넌트 메타데이터를 Inspector와 컴포넌트 직렬화에서 활용
- **씬 파일**: 영구 ID와 부모·자식 순서를 보존하는 ECS 씬 JSON / BEVE 저장·복원
- **프리팹 기반 API**: 엔티티 하위 트리 저장과 새 ID를 부여하는 인스턴스 생성 경로
- **Logic 그래프 실행기**: 수학 함수·제어 흐름·단계 실행·실행 예산과 추적 기록

### 2) 에디터 (`Editor`)

- **ImGui 기반 툴 UI**
- **핵심 패널 구성**
	- Viewport
	- Hierarchy
	- Inspector
	- Content Browser
	- ToolBar / SceneControlBar
	- Graph / Console / Metrics
- **선택 시스템**: `SelectionManager` 기반 다중 선택, ECS 정적 메시·Simple Sprite의 GPU ID 피킹과 레이 경계 상자 피킹
- **씬 편집**: Hierarchy 트리·부모 변경·하위 트리 삭제, Reflection Inspector와 Transform 기즈모
- **Undo/Redo**: 속성·컴포넌트 추가·생성/삭제·계층 변경·UI 편집의 씬 이력과 드래그 병합
- **ECS UI 편집**: Canvas / Image / Button 생성, 팬·줌·스냅·이동·크기 조절과 Z-Index 편집
- **UI 상호작용 미리보기**: 버튼의 호버·누름·클릭·취소와 상태 색상
- **그래프 패널**: imnodes 기반 Logic / Shader 편집, 호환 노드 검색·자동 연결, Logic 실행·핀 값·추적 표시
- **에셋 임포터**: Model / Texture / Shader / Sprite / Animation 임포트 구조
- **로컬라이징 파이프라인**: CSV -> JSON 자동 변환

현재 실행 진입점은 SDL3 + ImGui 에디터입니다. Qt6 Widgets는 빌드 의존성에 추가되었지만,
`QtManager`는 기본 창·도크 구성 단계이며 엔진 뷰포트·씬·선택 연결과 기본 실행 경로 전환은 아직 없습니다.

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
| CMake 3.21+ | 전체 | 멀티 타깃(Engine DLL, Editor EXE, Tools) 빌드 오케스트레이션 |
| vcpkg manifest | 전체 | 의존성 목록과 toolchain 기반 패키지 구성 |
| Unity Build / PCH | Editor / Engine | Editor의 Unity Build와 Engine의 PCH 사용. Engine의 Unity Build는 현재 비활성화 |

### 런타임/렌더링

| 기술 | 적용 위치 | 채택 의도 |
|---|---|---|
| SDL3 | Engine | 플랫폼 윈도우/입력 계층 표준화 |
| DirectX12 / SDL GPU | Engine | 네이티브 D3D12와 SDL GPU RHI 경로. 현재 Editor 기본 선택은 DirectX12 |
| Vulkan | Engine | SDL GPU의 Vulkan 백엔드와 관련 개발 의존성 |
| DXC / glslang / SPIRV-Tools / SPIRV-Cross | 셰이더 빌드·Engine | DXC 기반 HLSL -> DXIL / SPIR-V 생성과 셰이더 분석·변환 도구 |
| EnTT | Engine | ECS 엔티티·컴포넌트 저장소와 시스템 처리 |
| glm | Engine | 수학 연산(벡터/행렬/쿼터니언) 표준화 |

### 에디터/콘텐츠 제작

| 기술 | 적용 위치 | 채택 의도 |
|---|---|---|
| Dear ImGui + ImGuizmo | Editor | 빠른 툴 UI 제작과 트랜스폼 기즈모 제공 |
| imnodes | Editor | Logic / Shader 그래프의 노드·핀·연결 편집. CMake FetchContent로 고정 커밋 사용 |
| Qt6 Widgets | Editor | 기본 창·도크 구성의 초기 연결. 기존 ImGui 편집 기능의 Qt 이전은 미완료 |
| assimp | Editor | 모델 임포트 파이프라인 구축 |
| efsw | Editor | 파일 변경 감지 기반 에셋 갱신 자동화 |
| Python / LLVM·Clang | 도구 체인 | Python으로 Localization 변환, C++ LLVM/Clang Generator로 Reflection 코드 생성 |

### 데이터/유틸리티

| 기술 | 적용 위치 | 채택 의도 |
|---|---|---|
| glaze | Archive | JSON / BEVE 직렬화·역직렬화 처리 |
| lz4 | Engine | 빠른 압축 경로 구성 |
| fmt | 전체 | 안전하고 일관된 문자열 포맷팅 |
| stb, directxtex | 리소스/도구 | 이미지/텍스처 처리 유틸리티 |
| cppcodec | 리소스/유틸 | 인코딩 처리 보조 |

---

## 프로젝트 기술 전체 정리 (엔진 내부 포함)

이 섹션은 단순 외부 라이브러리 목록이 아니라, 코드베이스에 구현된 내부 기술(아키텍처/런타임 정책/에디터 파이프라인)까지 포함해 정리합니다.

### 1) 엔진 아키텍처/런타임 모델

- **모듈 분리**: Core, Runtime, World, Render, Resource, Physics, UI, Graph, System, Utility
- **ECS 모델**: `Scene`의 EnTT 저장소와 `Entity` 래퍼, 영구 `IDComponent`, `HierarchyComponent`
- **기존 오브젝트 모델**: `GameObject + Component` 경로 유지. 새 ECS 씬 저장 형식과는 별도
- **매니저 중심 서브시스템**: Scene/Layer/Resource/RenderPass/RenderTarget/Physics/Input 등
- **핸들 기반 리소스 참조**: 세대(generation) 기반 stale handle 완화

### 2) 렌더링 아키텍처 (RHI + RenderPass)

- **RHI 추상화 계층**
	- 공통 인터페이스: Buffer / Texture / Sampler / Shader / Pipeline 생성 및 바인딩
	- 구현: 네이티브 `DirectX12`, `SDLGPU`와 그 Vulkan 백엔드 경로
- **RenderPass + RenderTarget 분리 설계**
	- 패스 등록/정렬/로드-스토어 정책/블렌드 모드 관리
	- 뷰포트 단위로 카메라-패스 매핑 및 커스텀 커맨드 제출
- **셰이더 파이프라인**
	- `Resources/Shader`의 HLSL 중 이름이 VS/PS로 끝나는 파일을 DXC로 컴파일
	- DXIL은 `Resources/Shader/bin/dxil`, SPIR-V는 `Resources/Shader/bin/spirv`에 생성
	- GLSL 대응 소스는 남아 있지만 현재 CMake의 자동 컴파일 대상은 HLSL
- **SceneView 피킹**: GBuffer의 임시 Entity ID를 한 픽셀 readback으로 조회. 레이 모드는 메시 AABB 검사
	- GPU ID 지원 대상·동기 readback 비용·커스텀 셰이더 조건: [ScenePicking.md](Docs/ScenePicking.md)

### 3) UI 시스템 (Canvas / RectTransform / Pivot)

- **ECS UI 편집기 기반**
	- `Core/Public/Components/UI`의 Canvas / RectTransform / Image / Button / RenderOrder 컴포넌트
	- Engine UILayout과 UIRenderSystem을 사용한 캔버스 이미지 렌더링
	- UI View에서 Canvas/Image/Button 생성, 화면 영역·그리드 표시, 팬·줌, 선택·이동·크기 조절과 스냅
	- 사용법·검증 상태·현재 제한: [UIEditorFoundation.md](Docs/UIEditorFoundation.md)
	- UI 버튼·포인터 이벤트와 Interact 미리보기: [UIInteraction.md](Docs/UIInteraction.md)
	- Z-Index와 앞/뒤 배치, 그리기·선택 순서: [UIRenderOrder.md](Docs/UIRenderOrder.md)

현재 ECS 편집기의 범위는 최상위 ScreenSpace Canvas와 이미지·버튼입니다.
기존 GameObject의 `UICanvas`·`RectTransform` 경로는 남아 있지만 새 ECS UI와 별도이며,
그 경로의 WorldSpace 등 설정을 ECS 편집기의 지원 범위로 해석하지 않습니다.

### 4) 에디터 기술 (ImGui 툴 프레임워크)

- **Docking/Viewports 활성화된 Dear ImGui 환경**
- **핵심 패널**: Hierarchy, Inspector, ContentBrowser, Scene/Game/UI Viewport, ToolBar, Graph, Console, Metrics
- **ImGuizmo 기반 편집 기즈모**
	- Translate/Rotate/Scale, Local/World, Snap 옵션
- **Selection/피킹/입력 연동**
	- 엔진 Input 시스템과 에디터 조작 흐름 통합
- **Reflection Inspector**: 메타데이터 기반 속성 표시·편집과 컴포넌트 추가, 한국어·영어 표시 연결
- **씬 Undo/Redo**: 메모리 스냅샷 기반 이력, 영구 ID로 선택·관계 복원, 기본 100건 보관
	- `Ctrl+Z`, `Ctrl+Y`, `Ctrl+Shift+Z`와 Edit 메뉴 사용
	- 그래프·에셋/파일 작업·기존 GameObject 편집은 이력 연결 범위 밖
	- 지원 작업·실패 조건·검증 기록: [EditorUndoRedo.md](Docs/EditorUndoRedo.md)

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
	- Engine 헤더의 어노테이션을 탐색해 `BamEngine` 모듈 생성
	- `Engine::ReflectionRegistry`가 타입 정보·컴포넌트 작업·직렬화 코덱을 소유
	- `COMPONENT` 메타데이터의 타입 목록으로 기본 작업 등록, 필요한 컴포넌트에 별도 추가 정책 적용
	- Inspector에서 엔티티의 등록된 컴포넌트를 순회하고 프로퍼티 편집

- **ReflectionArchive — `Projects/ReflectionArchive`**
	- `ReflectionArchive::Adapter` 타깃으로 Reflection과 Archive 연결
	- 프로퍼티 메타데이터와 타입별 코덱을 이용해 저장·복원 처리
	- 등록 객체·scoped enum·지원 컨테이너의 재귀 조합과 상속 멤버 저장·복원
	- 일반 등록 타입은 멤버별 저장 함수를 반복 작성하지 않고 공통 `WriteObject()` / `ReadObject()` 사용
	- GLM 등 소비자 전용 타입의 직렬화는 별도 어댑터로 연결
	- 상세 지원 타입·제약·외부 소비 예제: [ReflectionArchive README](Projects/ReflectionArchive/README.md)

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
	- 리소스의 `WriteArchive()` / `ReadArchive()`를 Reflection 속성 처리에 연결
	- ResourceManager에서 `.json`과 `.beve` 리소스 로드
	- ECS 씬의 영구 ID·컴포넌트·계층 저장·복원, 프리팹 하위 트리 저장·인스턴스 생성 API
	- 리소스 핸들은 실행 중 index/generation 대신 에셋 키·경로를 기록하고 ResourceManager로 해석
	- GLM 벡터·회전·행렬과 2D/3D Collider shape 코덱 연결
	- 기존 Binary 리소스 저장·로드 경로는 현재 지원하지 않음

- **압축**: `lz4` 기반 압축·해제

### 7) 콘텐츠 제작/임포트 파이프라인

- **모델 임포트**: assimp
- **텍스처/이미지 처리**: stb, DirectXTex, TextureConverter 도구
- **파일 변경 감시**: efsw (에셋 갱신/브라우저 연동)
- **로컬라이제이션**: CSV -> JSON 자동 변환 스크립트

### 8) 자동화/빌드 파이프라인

- **CMake 타깃 구조**: Engine(Shared), Editor(Executable), TextureConverter(Tool)
- **빌드 구성**: Editor Unity Build, Engine PCH, MSVC 병렬 컴파일 옵션
- **의존성 관리**: vcpkg manifest + CMake Toolchain, imnodes 고정 커밋 FetchContent
- **코드 생성 통합**
	- Reflection bundle 생성
	- Localization 생성
	- HLSL의 DXIL / SPIR-V 컴파일

### 9) 그래프 시스템 (Logic / Shader)

- **공통 데이터**: 노드·핀·연결·입력값과 도메인별 정의를 Engine에서 관리
	- 자료형·방향·연결 개수·데이터 순환·필수 입력 검증
- **Reflection 연결**: `GRAPH_PURE` / `GRAPH_CALLABLE` 함수와 프로퍼티 노출 메타데이터로 노드 정의 생성
- **Logic 실행**: 스칼라·벡터 수학, Branch / Sequence / Loop / DoOnce / Gate / FlipFlop
	- 실행 복사본과 인스턴스 상태, 실행·일시정지·중지·한 단계 진행
	- 기본 이벤트 예산 10,000, 데이터 평가 깊이 128, 최근 추적 2,048건 보관
- **GraphPanel**: Logic / Shader 문서 전환, 노드 검색, 핀을 끌어 호환 노드 생성·자동 연결
	- 노드 안의 입력값 편집, 실행 경로 강조와 핀의 최근 관측값 표시

현재 그래프는 메모리 편집·실행 미리보기 단계입니다. 저장·불러오기, PIE 자동 연결,
씬 객체 Get/Set 실행과 Shader 코드 생성·컴파일 연결은 아직 없습니다.
Shader 도메인의 노드 편집·연결 검증을 완성된 셰이더 제작 기능으로 간주하지 않습니다.

관련 문서: [공통 기반](Docs/GraphFoundation.md), [어노테이션](Docs/GraphReflection.md),
[패널 편집](Docs/GraphPanel.md), [Logic 실행·추적](Docs/GraphExecution.md).

---

## 프로젝트 구조

```text
BamEngine/
├─ Engine/                  # 런타임 엔진 (DLL)
│  └─ Source/
│     ├─ Core/
│     ├─ Runtime/
│     ├─ Graph/
│     ├─ Render/
│     ├─ Resource/
│     ├─ Physics/
│     ├─ World/
│     ├─ UI/
│     ├─ System/
│     └─ Utility/
├─ Editor/                  # 에디터 애플리케이션 (EXE)
│  └─ Source/
│     ├─ Application/
│     ├─ ImGui/
│     ├─ AssetManager/
│     ├─ Command/           # 씬 편집 이력과 스냅샷
│     ├─ Qt/                # 기본 Qt 창·도크 구성
│     ├─ Rendering/
│     ├─ Selection/
│     └─ Generated/
├─ Projects/
│  ├─ Reflection/           # Runtime, LLVM/Clang 기반 Generator, CMake 생성 함수
│  ├─ Archive/              # 공통 Archive API와 JSON / BEVE 구현
│  ├─ ReflectionArchive/    # Reflection과 Archive 연결 어댑터
│  ├─ ReflectionConsumer/   # 소비 프로젝트 예제
│  ├─ ReflectionPackageCheck/
│  └─ ReflectionEnTT/       # 향후 성능 비교용 별도 구현
├─ Tools/
│  └─ TextureConverter/
├─ Resources/               # 셰이더/로컬라이제이션/기본 리소스
├─ Docs/                    # 설계 지침·기능별 범위·구현 기록
└─ CMakeLists.txt
```

---

## 개발 환경 및 기본 셋팅

### 요구사항

- Windows (현재 Windows 프리셋 기준)
- Visual Studio 18 2026 및 MSVC C++ 툴체인
- Windows SDK와 DirectX12 개발 환경
- CMake 3.21 이상 (프로젝트 선언 기준)
- C++23 지원 컴파일러
	- Reflection Runtime과 Generator는 C++20 사용
	- Archive와 ReflectionArchive는 C++23 요구
- LLVM/Clang 20.1.8 개발 라이브러리와 대응하는 Clang resource 디렉터리
- Python 3.x (Localization 스크립트 실행)
- Vulkan SDK와 DXC (`dxc.exe`, HLSL -> DXIL / SPIR-V 컴파일)
- Qt6 Widgets (`vcpkg.json`의 `qtbase[widgets]`, 현재 Editor 구성의 필수 의존성)
- Git 및 첫 구성 시 GitHub 접근 (imnodes FetchContent)

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
Linux/macOS 프리셋은 남아 있지만 현재 엔진의 Windows·DirectX12 의존성까지 이식된 상태는 아닙니다.

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

기본 출력 위치는 `out/build/x64-debug/bin/Debug/Editor.exe`입니다.
리소스 경로가 저장소 루트 기준이므로 루트에서 실행합니다.

```powershell
.\out\build\x64-debug\bin\Debug\Editor.exe
```

> 참고  
> 루트 `CMakeLists.txt` 기준 현재 기본 타깃은 `Engine`, `TextureConverter`, `Editor`입니다.  
> `Client`는 주석 처리되어 있습니다.

### 4) 선택 검사 타깃

아래는 사용자가 실행할 검증 명령입니다. 이번 README 갱신에서는 구성·빌드·테스트를 실행하지 않았습니다.
기존 Debug 프리셋에 검사 타깃을 추가하려면 다음과 같이 구성합니다.

```powershell
cmake --preset x64-debug -DBAMENGINE_BUILD_REFLECTION_CHECK=ON -DBAMENGINE_BUILD_GRAPH_CHECK=ON -DBUILD_TESTING=ON
$targets = @('SceneReflectionCheck', 'SceneArchiveCheck', 'UILayoutCheck', 'UIInteractionCheck',
    'EditorHistoryCheck', 'GraphFoundationCheck', 'GraphReflectionCheck', 'GraphExecutionCheck', 'GraphPaletteCheck')
cmake --build .\out\build\x64-debug --config Debug --target $targets
ctest --test-dir .\out\build\x64-debug -C Debug -R '^(Scene|UI|EditorHistory|Graph).*Check$' --output-on-failure
```

검사 대상은 등록 컴포넌트, 씬·리소스·프리팹, UI 레이아웃·입력, 편집 이력과 그래프입니다.
GPU 렌더링·피킹과 실제 창 조작은 Editor에서 별도로 확인합니다.
독립 모듈의 구성·설치·외부 사용은 [Reflection / Archive 빌드 가이드](Docs/ReflectionArchiveBuild.md)를 참고합니다.

---

## 리소스/에셋 파이프라인

- 리소스는 `ResourceManager`를 통해 로드/등록/조회/해제
- `Handle(index + generation)` 구조로 stale handle 문제를 완화하도록 설계
- 파일에는 에셋 키·경로를 저장하고 로드 시 참조를 해결. 필요한 리소스와 ResourceManager 초기화가 전제
- 에디터 임포터를 통해 에셋을 엔진 포맷으로 변환/등록
- Localization CSV를 JSON으로 변환하는 자동화 타깃 포함 (`GenerateLocalization`)

---

## Reflection 코드 생성

엔진은 `Engine/CMake/ReflectionBuild.cmake`에서 `reflection_generate()`를 호출합니다.
분석 대상은 `Engine/Source` 아래에서 `CLASS`, `STRUCT`, `ENUM`, `PROPERTY`, `FUNCTION` 마커가 발견된 헤더입니다.
생성 모듈 이름은 `BamEngine`이며, `Engine::ReflectionRegistry`를 등록 대상으로 사용합니다.

생성 파일은 소스 디렉터리가 아닌 빌드 디렉터리에 배치됩니다.
`x64-debug` 프리셋의 Debug 구성에서는 다음 경로를 사용합니다.

```text
out/build/x64-debug/Engine/Generated/Debug/
├─ BamEngine.gen.cpp
├─ BamEngine.gen.h
└─ BamEngine.gen.d
```

생성된 C++ 파일은 Engine 타깃에 포함되어 컴파일됩니다.
엔진은 생성된 `reflection_generated::Register_BamEngine()` 경로로 타입을 등록하고,
`ForEach_BamEngine_Type`의 컴포넌트 목록으로 ECS 작업을 구성합니다.
프로퍼티 타입 순회는 리소스 핸들 코덱 등록에도 사용합니다.

에디터 Inspector는 엔진이 제공하는 Registry와 컴포넌트 바인딩을 사용합니다.
현재 Editor 타깃은 별도의 Reflection 코드 생성 단계를 실행하지 않습니다.

관련 모듈:

- `Projects/Reflection`: Runtime과 Generator
- `Projects/Archive`: JSON / BEVE 직렬화
- `Projects/ReflectionArchive`: Reflection 기반 직렬화 어댑터
- `Projects/ReflectionEnTT`: 별도 비교용 구현. 현재 Reflection을 대체한 상태가 아님

---

## 현재 상태

본 저장소는 **WIP(Work In Progress)** 상태입니다. 아래는 2026-10-16 현재 작업 공간 기준입니다.

**코드에 연결된 범위**

- Reflection Runtime·Generator와 통합 ReflectionRegistry, ECS Inspector·컴포넌트 작업·직렬화 연결
- JSON / BEVE 기반 씬 저장·불러오기, 영구 ID·계층 복원, 리소스 참조 코덱과 프리팹 API
- ECS UI 캔버스 편집·버튼 상호작용 미리보기·Z-Index와 그리기/선택 순서
- Hierarchy 편집, Transform 기즈모, 씬 Undo/Redo와 GPU ID / 레이 경계 피킹 경로
- Logic / Shader 공통 그래프·패널 편집, Logic 수학·제어 흐름 실행과 추적 미리보기

**아직 남은 범위와 제한**

- PIE 종료 시 `Application::RestoreScene()`은 `NotImplemented` 반환. 실행 전 편집 상태 복원 미완료
- 그래프 파일 저장·불러오기, 씬 객체 Get/Set 실행, PIE 자동 연결과 Shader 코드 생성 미구현
- 프리팹은 엔진 API 단계이며 전용 에디터 제작·배치 워크플로우는 미연결
- Qt 창은 초기 구성 단계. 기본 ImGui 에디터 기능과 엔진 뷰포트를 이전한 상태가 아님
- 씬 형식은 Version 1이며 마이그레이션과 파일의 원자적 교체는 미구현
- 임의 포인터·객체 참조의 자동 복원, 모든 컴포넌트·리소스의 왕복 저장을 보장하지 않음
- 기존 Binary 리소스 경로 미지원. Client 기본 빌드와 배포·패키징 흐름은 미완성
- Visual Scripting의 전체 제작·실행 흐름, 핫 리로드, 아바타·강화학습 연동은 장기 목표

**검증 기록과 한계**

- [구현 기록](Docs/ImplementationLog.md)과 [Undo/Redo 문서](Docs/EditorUndoRedo.md)에는
  일부 작업의 Debug·Release 빌드 및 검사 통과 기록이 있습니다.
- 그래프 패널·실행·호환 노드 검색 등에는 소스 작성·정적 검토만 기록된 항목도 있습니다.
  과거 기록을 현재 작업 공간 전체의 빌드·테스트 통과로 해석하지 않습니다.
- 이번 README 갱신은 소스·설정·문서 대조만 수행했습니다. 최신 변경의 빌드·CTest와 GPU·UI 조작 검증은 별도입니다.
- 일부 상세 문서에는 최신 리소스 핸들·Collider 코덱과 프리팹 코드가 아직 반영되지 않았습니다.
  해당 지원 범위는 현재 코드와 실제 왕복 검사 결과를 함께 확인해야 합니다.

---

## 기여 가이드

- 이슈/PR 환영
- 코딩 스타일은 기존 C++ 코드 스타일과 CMake 구조를 따릅니다.
- 대규모 변경 전에는 이슈로 설계/목표를 먼저 공유해 주세요.

---

## 구현 세부 기록

- 상세 구현 히스토리 및 기술 부채 추적 문서: [ImplementationLog.md](Docs/ImplementationLog.md)
- 장기 목표와 역할 경계: [엔진·에디터·클라이언트 지침](Docs/EngineEditorClientGuidelines.md)
- 독립 모듈 목표: [Reflection / Archive 지침](Docs/ReflectionArchiveGuidelines.md)
- 엔진 통합·씬 파일·Inspector: [EngineReflectionIntegration.md](Docs/EngineReflectionIntegration.md)
- 독립 모듈 구성·검증·설치: [ReflectionArchiveBuild.md](Docs/ReflectionArchiveBuild.md)
- 기능 단위 변경 시 템플릿 기반으로 기록을 누적하는 것을 권장합니다.

---

## 라이선스

저장소 루트에 별도 라이선스 파일이 명시되어 있지 않습니다.  
배포/상업적 이용 전 라이선스 정책을 먼저 확정하세요.
