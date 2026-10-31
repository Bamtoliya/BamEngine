# 엔진·에디터·클라이언트 빌드 구성과 디버그 전처리기 지침

- 작성일: 2026-10-29
- 상태: 지침 / 클라이언트 빌드 및 패키징 선행 기준
- 관련 문서:
  - [BuildAndPackagingPlan.md](BuildAndPackagingPlan.md) (빌드·패키징 순서 및 실행 계약)
  - [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md)의 3절, 12절 (Engine/Editor/Client 책임 및 패키징)
  - [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md) (의존성 및 소스 배치)

---

## 1. 목적 및 배경

클라이언트 빌드 및 패키징(P2-E)을 본격적으로 진행하기 전, 엔진 코드베이스 전반에 걸쳐 **최적화 수준(`Debug` vs `Release`)**과 **타깃 역할(`Editor` vs `Client`)**의 구분을 명확히 하고, **디버그 전용 코드의 전처리기 처리 표준**을 수립한다.

현재 코드베이스는 에디터에 필요한 도구 기능(디버그 렌더링, 그래프 실행 추적, 피킹 등)이 컴파일러의 `_DEBUG` 플래그에 의존하거나, 전처리기 없이 표준 스트림(`std::cerr`, `std::cout`)으로 노출되어 있어 다음과 같은 위험을 안고 있다:

1. **에디터 릴리스 빌드 파손**: 에디터를 빠른 속도로 사용하기 위해 Release 모드로 빌드할 때, 기즈모/디버그 렌더링 코드가 함께 증발하여 빌드 오류가 발생함.
2. **배포 바이너리 오염**: 최종 클라이언트 배포판에 불필요한 디버그 콘솔 출력, 디버그용 메모리 스냅샷, 피킹 버퍼 오버헤드가 그대로 잔존함.
3. **CRT/ABI 불일치 크래시**: Debug 실행 파일과 Release DLL의 혼용으로 인한 힙 손상 위험.

---

## 2. 핵심 설계 원칙: 두 축의 분리

빌드 구성을 판단할 때는 반드시 **최적화 수준**과 **타깃 역할**이라는 서로 다른 두 축을 분리해야 한다.

```mermaid
flowchart TD
    subgraph BuildAxis["축 1: 최적화 수준 (Optimization Level)"]
        DebugOpt["Debug (/Od, /MDd)<br>C++ 런타임 검사, 메모리 누수 추적, 느림"]
        ReleaseOpt["Release (/O2, /MD)<br>인라인 최적화, STL 반복자 검사 해제, 빠름"]
    end

    subgraph TargetAxis["축 2: 타깃 역할 (Target Role)"]
        EditorTarget["Editor Target<br>씬 편집, 기즈모, 피킹, 디버그 라인, 그래프 펄스"]
        ClientTarget["Client / Standalone Target<br>순수 게임 실행, 편집 도구 및 디버그 라인 제외"]
    end

    BuildAxis -.-> Matrix["실제 빌드 구성 매트릭스"]
    TargetAxis -.-> Matrix

    Matrix --> C1["Debug Editor (엔진 코어 메모리 디버깅용)"]
    Matrix --> C2["Development Editor (실무 일상 작업용 기본 구성 - Release + 툴 기능 ON)"]
    Matrix --> C3["Development Client (개발용 클라이언트 - 프로파일링/치트 ON)"]
    Matrix --> C4["Shipping Client (최종 배포용 - 순수 최적화 및 모든 디버그 툴 스트립)"]
```

### "에디터도 릴리스 DLL을 사용해야 하는가?"에 대한 답
**네, 에디터도 일상적인 작업(레벨 디자인, 씬 편집, PIE 플레이)에서는 반드시 릴리스(Release 또는 RelWithDebInfo) 엔진 DLL을 사용해야 합니다.**

- **성능 이유**: Debug 빌드는 STL 반복자 검증과 최적화 해제로 인해 프레임레이트가 심각하게 낮아집니다.
- **실무 엔진 표준**: 언리얼 엔진의 `Development Editor`, 유니티의 에디터 구동 방식과 동일하게, 최적화(`/O2`)가 켜진 상태에서 에디터 도구(기즈모, 그래프 디버깅 등)가 정상 작동해야 합니다.
- **CRT 일치 규칙**: Windows/MSVC 환경에서는 **Debug EXE와 Release DLL을 섞어서 링크하면 즉시 힙 손상 크래시**가 발생합니다. 따라서 에디터를 Release로 구동할 때는 엔진 DLL도 반드시 Release 빌드여야 합니다.

---

## 3. 전처리기 매크로 정의 표준

엔진 내부 코드는 다음 매크로를 명확히 구분하여 적용한다.

| 전처리기 매크로 | 정의 조건 | 허용되는 사용처 | 금지되는 사용처 |
| :--- | :--- | :--- | :--- |
| **`_DEBUG`**<br>(또는 `DEBUG`) | 컴파일러 디버그 빌드 시 자동 정의 | - C 런타임 메모리 누수 점검 (`_CrtSetDbgFlag`)<br>- GPU RHI 디버그 레이어 활성화<br>- 실패 시 `__debugbreak()` 호출 | - 기즈모/디버그 라인 구조체 정의<br>- 에디터용 디버그 실행기 조회 API<br>- 에디터 패널에 노출되는 편의 기능 |
| **`BAM_EDITOR`**<br>(또는 `BAM_WITH_EDITOR`) | Editor 타깃 빌드 시 정의<br>(Debug, Release 모두) | - 뷰포트 기즈모/디버그 라인 (`DebugVertex`)<br>- 그래프 노드 펄스 실행기 조회 (`GetDebugExecutor`)<br>- 마우스 피킹 ID 버퍼 계산 (`pickingId`)<br>- 리소스 메모리 스냅샷 (`GetDebugSnapshot`) | - 독립형 Client 전용 런타임 로직 |
| **`BAM_SHIPPING`** | 최종 배포용 Client 타깃 빌드 시 정의 | - 모든 치트, 콘솔 커맨드, 디버그 로깅 완전 스트립 | - 에디터 또는 개발용 클라이언트 |

---

## 4. 엔진 소스 내 디버그 코드 현황 및 정비 과제

클라이언트 빌드 및 패키징 작업 전에 정비해야 할 엔진(`Engine/Source`) 내의 세부 항목 목록이다.

### 4.1. 콘솔 직접 출력 스트림 제거 (`std::cerr`, `std::cout`)
엔진 공식 로거(`BAM_LOG`, `ENGINE_LOG_ERROR`) 대신 C++ 표준 스트림을 직접 호출하는 코드는 에디터 콘솔 창에 수집되지 않으며, 배포판 GUI 실행 시 불필요한 I/O를 유발한다.

- `Engine/Source/Core/Localization/Private/LocalizationManager.cpp` (21, 29, 42, 58줄):
  `std::cerr` 호출부를 `BAM_LOG(Error, "Localization", ...)` 또는 `ENGINE_LOG_ERROR`로 전환.
- `Engine/Source/Core/Private/Reflection/ReflectionRegistry.cpp` (302줄):
  `std::cerr` 호출부를 `BAM_LOG(Fatal, "Reflection", ...)`로 전환.
- `Engine/Source/Graph/Public/GraphGameplayLibrary.h` (21, 28, 34줄):
  블루프린트 `Debug.PrintFloat` 등의 `std::cout`을 `BAM_LOG(Info, "BlueprintLog", ...)`로 전환.

### 4.2. 저수준 디버그 API 및 강제 종료 개선
- `Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp` (922, 928줄):
  - `OutputDebugStringA` 및 `std::terminate()` 호출을 제거하고, `ENGINE_LOG_ERROR`로 원인을 남긴 뒤 `EResult::Fail`을 정상 반환하도록 개선.

### 4.3. 디버그 전용 인터페이스 전처리기 정비
- `Engine/Source/System/Graph/Public/GraphSystem.h` (51줄):
  - 비표준 문법인 `#ifdef(_DEBUG)`를 `#if defined(BAM_EDITOR)` (또는 `#if defined(_DEBUG)`)로 수정.
- `Engine/Source/System/Graph/Private/GraphSystem.cpp` (232줄):
  - `GetDebugExecutor` 구현부에도 헤더와 동일하게 전처리기 매크로를 일치시킴.
- `Engine/Source/System/Graph/Public/GraphSystem.h` (58~60줄):
  - `NodeExecuteHook`, `TriggerNodeExecuteHook` 등 에디터 중단점용 훅 인터페이스를 `#if defined(BAM_EDITOR)`로 격리.

### 4.4. 릴리스 에디터 빌드 파손 방지 (최우선 과제)
- `Engine/Source/Core/Public/Structs/Vertex.h` (218줄) 및 `Vertex.cpp` (67줄):
  - `DebugVertex`가 `#ifdef _DEBUG`로 감싸져 있어 에디터를 Release로 빌드하면 링크 에러가 발생함.
  - `#ifdef _DEBUG`를 제거하여 항상 정의되도록 하거나, `#if defined(BAM_EDITOR)`로 전환.

### 4.5. 단언 매크로 및 디버그 스냅샷 격리
- `Engine/Source/Core/Public/Macro.h` (43줄):
  - 원시 C `assert()`를 엔진 표준 단언 체계로 정비.
- `Engine/Source/Resource/Public/ResourceManager.h` (66줄):
  - `GetDebugSnapshot()` 함수를 `#if defined(BAM_EDITOR)`로 감싸 최종 Client 배포 바이너리에서 제외.

---

## 5. 클라이언트 빌드 및 패키징(P2-E) 진행 시 작업 절차

1. **CMake 타깃별 컴파일 정의 추가**:
   - `Editor` 타깃: `target_compile_definitions(Editor PRIVATE BAM_EDITOR=1)`
   - `Engine` 타깃 (에디터용 빌드 시): `BAM_EDITOR=1` 전달
   - `Client` 타깃: `BAM_EDITOR`를 정의하지 않음 (Shipping 구성 시 `BAM_SHIPPING=1`)
2. **위 4절의 엔진 디버그 코드 정비 완료**:
   - `DebugVertex` 및 `GetDebugExecutor`의 매크로를 `BAM_EDITOR`로 전환.
   - 표준 스트림 및 저수준 디버그 호출 정리.
3. **독립형 Client 릴리스 빌드 검증**:
   - 에디터 의존성이 완전히 제거된 상태에서 Client 실행 파일이 온전히 링크되는지 확인.
4. **패키징 검증**:
   - [BuildAndPackagingPlan.md](BuildAndPackagingPlan.md)의 4단계(Stage와 Package) 절차에 따라 배포 폴더를 구성하고 개발 저장소 외부에서 단독 실행 확인.
