# BamEngine 기술 검토 및 개선 제안

- 평가 기준일: 2026-11-25
- 대상: [Bamtoliya/BamEngine](https://github.com/Bamtoliya/BamEngine), `main`
- 기준 커밋: [`be79eed22ef02b6f115034687998773cc1ac2b84`](https://github.com/Bamtoliya/BamEngine/commit/be79eed22ef02b6f115034687998773cc1ac2b84)
- 문서 성격: 정적 기술 검토 결과와 후속 개선 후보 기록. 구현 작업 지시나 기존 로드맵의 대체 문서가 아니다.
- 관련 문서: [개발 목표 대비 달성률 평가](BamEngineGoalProgressReview-2026-11-25.md)
- 변경 상태: 이 검토로 인한 엔진 소스 수정, 빌드, 테스트 및 커밋 없음.

## 1. 요약

BamEngine은 렌더링 데모나 학습용 프레임워크를 넘어 ECS, Runtime, 리플렉션, 직렬화, 에디터, VFX, 네트워크, 리소스 스트리밍 등을 통합한 고급 개인 개발 엔진으로 평가한다.

강점은 기술의 개수보다 GPU 리소스 수명 관리, 리플렉션 기반 제작 환경, Editor와 Client의 공용 실행 경로 등 시스템 간 연결에 있다. 주요 위험은 일부 RHI 참조 관리와 데이터 보호 문제, 대형 구현부에 집중된 책임, 전역 관리자 결합, 플랫폼 간 기능 격차 및 실행·성능 검증 부족이다.

권장 방향은 결함과 기존 검증 대기를 우선 해소하고, 작은 실제 게임으로 제작·배포 전 과정을 검증하며, 측정된 병목에 따라 Frame Graph, GPU Driven Rendering 등 렌더링 기반을 확장하는 것이다. 강화학습·아바타·제작 도구는 기존 최종 목표에서 제외하지 않고 독립 경로로 진행한다.

## 2. 분석 범위 및 프로젝트 규모

GitHub에서 전체 파일 트리를 조회했으며 응답의 `truncated` 값은 `false`였다. 아래는 소스 계열 확장자(`.cpp`, `.h`, `.hpp`, `.inl`, `.hlsl`, `.hlsli`, `.c`, `.py`) 기준 집계이다. 파일 수는 코드 줄 수나 품질 지표가 아니다.

| 영역 | 파일 수 | 비고 |
|---|---:|---|
| `Engine/Source` | 551 | 23개 최상위 기능 모듈, 약 3.61MB |
| `Editor` | 341 | 에디터 예제·검사 소스 포함 |
| `Engine/Resources/Shader` | 110 | 셰이더 소스 계열 |
| `Engine/examples/*Check.cpp` | 55 | 검사 소스 수, 통과 건수 아님 |

### 2.1 Engine/Source 모듈 분포

| 모듈 | 파일 수 | 모듈 | 파일 수 |
|---|---:|---|---:|
| Render | 171 | Core | 56 |
| UI | 35 | Animation | 32 |
| World | 32 | Graph | 31 |
| Resource | 30 | Physics | 23 |
| Network | 23 | Spline | 18 |
| Sprite | 14 | Audio | 12 |
| VFX | 12 | Gameplay | 9 |
| Simulation | 9 | Terrain | 9 |
| Navigation | 8 | Camera | 7 |
| Input | 7 | Model | 4 |
| Time | 4 | Tilemap | 3 |
| Runtime | 2 | | |

합계 551개. 앞선 평가의 최초 표시(541개, 21개 모듈)는 재집계된 정확한 수치로 정정한다.

## 3. 종합 기술 평가

아래는 독립적인 정성 점수이며 기능 완료율이나 채용 합격 확률이 아니다. 서로 다른 기준의 점수는 단순 평균으로 종합 점수를 산출하지 않는다.

| 항목 | 평가 / 10 |
|---|---:|
| 기술 구현 난이도 | 9.0 |
| 렌더링 및 그래픽스 | 9.0 |
| 기능 다양성 | 9.0 |
| 에디터 및 제작 환경 | 8.5 |
| 엔진 아키텍처 | 8.0 |
| 리소스 관리 | 8.0 |
| 테스트 설계 | 8.0 |
| 코드 유지보수성 | 7.5 |
| 크로스 플랫폼 | 5.5 |
| 취업 포트폴리오 가치 | 9.0 |

| 비교 기준 | 종합 점수 / 10 |
|---|---:|
| 학부생·신입 개발자 포트폴리오 | 9.0 |
| 개인 개발 게임 엔진 | 8.3 |
| 실제 인디 게임 제작용 엔진 | 7.0 |
| 범용 상용 엔진 | 4.5 |

엔진 성숙도 분류상 기초 프레임워크 → 기초 게임 엔진 → 에디터 기반 엔진 → 고급 개인 개발 엔진 → Production-ready 엔진 중 네 번째 단계로 판단한다. 실제 출시, 장시간 안정성 및 성능, 플랫폼 검증 여부는 별도이다.

## 4. 영역별 기술 검토

### 4.1 Runtime 및 ECS

근거: [Runtime.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Runtime/Private/Runtime.cpp), [Scene.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/World/Scene/Private/Scene.cpp), [SystemManager.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/World/System/Private/SystemManager.cpp).

- `Runtime::Initialize()`에서 Reflection, Module, Time, Input, Audio, Resource, Scene, System, Renderer 등의 관리자 생성과 실패 시 정리 경로를 제공한다.
- `Runtime::Free()`는 렌더링 큐 정리, 스트리밍 스레드 종료, 씬 제거, 동적 모듈 언로드, 시스템·리소스 해제, RHI 종료 및 리플렉션 해제를 의존성에 맞춰 수행한다.
- EnTT 기반 씬별 Registry와 Global Registry, Entity 래퍼, Persistent ID, 계층 구조 및 외부 참조 검사가 존재한다.
- FixedUpdate/Update/LateUpdate와 실행 Phase, 렌더링 제출 경로가 확인된다. 확인한 SystemManager 경로는 순차 실행이다.
- Singleton 기반 관리자는 접근이 편리하나 독립 테스트, 복수 Runtime, 시스템 교체에 제약이 있을 수 있다. 기존 전체 구조를 교체하기보다 새 모듈부터 의존성 경계를 명확하게 유지할 것을 권장한다.

### 4.2 DirectX 12 RHI 및 렌더링

근거: [DirectX12RHI.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp), [RHI.h](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Render/RHI/Public/RHI.h), [DeferredSceneRenderer.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Render/Pipeline/Private/DeferredSceneRenderer.cpp), [PipelineManager.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Render/Pipeline/Private/PipelineManager.cpp).

구현 또는 호출 경로 확인 범위:

- Command List/Queue, 프레임별 Fence, Descriptor Heap Allocator, Dynamic Constant/Instance Buffer, GPU Heap Page Allocator.
- Fence 기반 Deferred Release, Descriptor 및 Heap Page 반환 시점 관리.
- PSO 캐싱과 셰이더 변경에 따른 파이프라인 재구축 요청.
- Bindless Index, BLAS/TLAS 인터페이스 및 Raytracing 경로.
- Deferred/Forward, Directional/Point Shadow, Motion Vector/TAA, Probe GI, DXR 그림자·반사, Compute AO, Sky Atmosphere, Deferred Decal, Distortion.

특히 `DeferRelease` / `DeferDescriptorFree`에서 Fence 값을 기록하고 `CollectDeferredReleases`에서 완료된 항목을 반환하는 구조는 DX12의 CPU/GPU 비동기 수명 관리 문제를 해결하려는 중요한 구현이다. 구체적인 동시성 및 실패 경계의 정확성은 별도 검증이 필요하다.

현재 [RenderPassManager.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Render/RenderPass/Private/RenderPassManager.cpp)의 정렬은 Priority 기반이다. 이는 렌더링 패스 구성과 실행에 적합한 기존 구조이나, 확인한 범위에서는 선언적 리소스 의존성으로 컴파일되는 Frame Graph와는 구분된다.

### 4.3 플랫폼 지원

근거: [SDLGPURHI.h](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Render/RHI/Platform/SDLGPU/Public/SDLGPURHI.h), [BuildAndPackagingPlan.md](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/BuildAndPackagingPlan.md).

| 백엔드 | 검토 상태 |
|---|---|
| DirectX 12 | 주력 구현 |
| SDL GPU | 부분 구현; Instanced Draw가 `NotImplemented`, Compute 등 기능 격차 |
| Native Vulkan | 스텁 수준 |
| Native Metal | 스텁 수준 |
| Web/Emscripten | 독립 프리뷰 및 빌드 경계 존재; 실제 Wasm Client·WebGPU 경로는 후속 범위 |

공통 RHI가 존재한다는 사실만으로 각 백엔드가 동일 기능을 지원한다고 평가하지 않는다. 지원 여부를 기능별 Capability와 명시적 폴백 또는 오류로 관리해야 한다.

### 4.4 리플렉션, 직렬화 및 Prefab

근거: [ReflectionBuild.cmake](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/CMake/ReflectionBuild.cmake), [SceneArchive.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/World/Scene/Private/SceneArchive.cpp), [Projects](https://github.com/Bamtoliya/BamEngine/tree/be79eed22ef02b6f115034687998773cc1ac2b84/Projects).

- Reflection, Archive, ReflectionArchive가 독립 프로젝트로 분리되어 있다.
- 어노테이션 헤더 탐색, Clang 파서 설정, 빌드 Configuration별 생성 코드, ECS 메타데이터 등록이 CMake에 연결되어 있다.
- Scene/Prefab Archive는 계층 검증, 버전 검사, 엔티티 참조 재매핑 및 Staging Scene 로드 경로를 갖춘다.
- 동일 메타데이터를 Inspector, 직렬화, 그래프, Undo/Redo에서 활용할 수 있는 구조는 중복 구현을 줄이는 기반이다.

남은 과제로는 데이터 형식 호환성, 누락 리소스 복구, 실패 시 데이터 불변성, 대규모 씬 저장·복원 비용 및 리플렉션 생성 시간의 정량 검증이 있다.

### 4.5 에디터 및 비주얼 스크립팅

근거: [SceneEditHistory.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Editor/Source/Command/Private/SceneEditHistory.cpp), [GraphExecutor.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Graph/Private/GraphExecutor.cpp).

Inspector, Prefab 격리 편집, SceneDocument, AssetManager, 별도 AssetWorker, ThumbnailRenderer, 플러그인, 스프라이트 및 그래프 편집 도구가 구성되어 있다.

`SceneEditHistory`는 ECS Snapshot 캡처·복원, 구조 일치 검증, 연속 편집 병합, 저장 상태 추적 및 에셋 경로 변경 대응을 포함한다. 전체 씬 Snapshot을 사용하는 경로의 비용은 씬 규모에 따라 증가할 수 있으므로 먼저 측정하고, 병목일 때 범위 제한 및 Delta History를 확대한다.

`GraphExecutor`는 실행 예산과 `ExecutionLimit`, 일시정지·재개·Step·진단을 지원한다. 무한 실행을 방어하는 설계는 확인되지만, 모든 노드 내부의 작업 시간이 제한된다는 의미는 아니다.

### 4.6 리소스 스트리밍 및 테스트

근거: [ResourceStreamer.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Resource/Private/ResourceStreamer.cpp), [Checks.cmake](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/CMake/Checks.cmake), [GpuHeapPageAllocatorCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/examples/GpuHeapPageAllocatorCheck.cpp).

비동기 작업 스레드, 조건 변수, 리소스 요청 우선순위, 업로드 예산 및 정리 경로가 확인된다. 테스트 인프라는 CMake/CTest로 구성되어 있으며 GPU Heap Page Allocator의 용량 초과, 단편화, 중복 반환, 잘못된 범위, 무작위 할당 검사 등 구체적인 경계 사례가 존재한다.

기준 커밋의 전체 파일 트리에는 `.github/workflows`가 확인되지 않았다. 다른 CI 시스템의 존재 여부는 확인하지 않았다. 자동화 비용과 GPU 의존성에 맞춰 CPU 검사·Windows 빌드부터 CI를 도입하는 것이 적절하다.

## 5. 구체적인 코드 개선 후보

아래 우선순위는 이전 정적 검토의 권고이며 공식 개발 우선순위(P0~P4)와 동일한 분류가 아니다. 기존 `DevelopmentPriorities.md`의 현재 검증 대기 및 사용자 결정을 우선한다. 모든 항목은 미수정 상태이며 실행 재현 여부를 구분한다.

| ID | 권고 우선순위 | 항목 | 근거 수준 |
|---|---|---|---|
| TR-01 | P0 | Constant Buffer 반복 바인딩 참조 누수 | 코드상 누적 경로 확인, 실행 미재현 |
| TR-02 | P1 | Vertex Buffer 슬롯 범위 검사 | 코드상 비일관 상태 가능 경로 확인 |
| TR-03 | P1 | Entity 삭제 실패 시 참조 원상복구 | 함수 내부 부분 변경 경로 확인 |
| TR-04 | P1 | Persistent ID 검색 비용 | 선형 검색 및 호출 경로 확인, 비용 미측정 |
| TR-05 | P2 | Base 참조 카운트 동시성 | 조건부 위험, 실제 데이터 레이스 미확인 |
| TR-06 | P2 | 대형 구현부 책임 분리 | 규모·책임 집중 관찰, 정량 복잡도 미측정 |
| TR-07 | P2 | Undo/Redo Snapshot 비용 | 전체 씬 캡처 경로 확인, 비용 미측정 |

### TR-01. Constant Buffer 참조 카운트 누수

- 위치: [RHI.cpp — BindConstantBuffer](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Render/RHI/Private/RHI.cpp#L85-L107)
- 상태: 코드상 문제 확인. 실제 메모리 누수 재현 및 호출 빈도 미검증.

기존 정적 버퍼와 새 버퍼가 동일할 때 기존 참조의 Release는 생략하지만 이후 AddRef는 실행하는 구조이다. 동일 버퍼를 반복 바인딩하면 참조가 누적될 수 있다.

제안 코드(검토용, 미적용):

```cpp
EResult RHI::BindConstantBuffer(RHIBuffer* buffer, uint32 slot)
{
    if (slot >= MAX_CONSTANT_BUFFER_SLOTS)
        return EResult::Fail;

    auto& state = m_ConstantBuffers[slot];

    if (!state.isDynamic && state.buffer == buffer)
        return EResult::Success;

    Safe_AddRef(buffer);

    if (!state.isDynamic)
        Safe_Release(state.buffer);

    state.buffer = buffer;
    state.isDynamic = false;
    state.offset = 0;
    return EResult::Success;
}
```

위 방향은 정적 버퍼 바인딩의 소유 참조와 동적 Ring Buffer의 비소유 참조를 구분하는 기존 계약을 전제로 한다. 실제 변경 전 `Safe_AddRef`의 null 처리, `Safe_Release`의 포인터 변경 계약, 파생 백엔드 및 모든 호출부를 대조한다.

완료 기준: 동일 정적 버퍼 반복 바인딩, 다른 버퍼 교체, null 바인딩, Dynamic→Static 및 Static→Dynamic 전환, 프레임 종료·RHI 해제에서 참조 수가 예상대로 유지된다. GPU 사용 중 해제에 대한 기존 Fence 계약도 보존한다.

### TR-02. Vertex Buffer 슬롯 범위 검증

- 위치: [RHI.cpp — BindVertexBuffers](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Render/RHI/Private/RHI.cpp#L60-L75)

반복문에서 슬롯 최대치를 검사하지만 `m_NumVertexBuffersBound`에는 검증되지 않은 `firstSlot + count`를 기록한다. DX12 Draw 경로에는 후속 상한 검사가 있으나 바인딩 단계에서 실패시키는 편이 일관적이다. 덧셈 오버플로도 방지해야 한다.

권고: 기존 입력 계약을 확인한 후 `firstSlot >= MAX_BUFFER_SLOTS` 또는 `count > MAX_BUFFER_SLOTS - firstSlot`을 검사하고, 유효하지 않은 경우 기존 바인딩 상태를 변경하지 않은 채 실패시킨다. 부분 바인딩을 의도한 계약이라면 실제 처리된 개수와 반환값을 명시한다.

완료 기준: 마지막 유효 슬롯, 0개, 최대치 초과, `UINT32_MAX` 등 경계에서 상태 불일치와 잘못된 메모리 접근이 없으며, 정상 Draw 및 Instancing 경로에 회귀가 없다.

### TR-03. Entity 삭제의 트랜잭션 안전성

- 위치: [Scene.cpp — DestroyEntity](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/World/Scene/Private/Scene.cpp)

삭제할 하위 트리와 외부 참조를 수집하고, 읽기 전용 참조가 있으면 삭제를 거부하는 구조는 좋은 설계이다. 다만 쓰기 가능한 참조를 순차적으로 초기화하는 도중 `WriteValue()`가 실패하면 반환하기 전에 이미 변경한 참조를 복구하는 코드가 함수 내에 없다.

권고: 실패 가능한 작업과 실제 변경을 분리하고, 원래 참조값을 보존한 뒤 커밋 또는 롤백하는 방식을 검토한다. 리플렉션 setter의 부수 효과 및 상위 에디터 트랜잭션이 이미 수행하는 복구가 있는지 확인한다. 롤백 자체의 실패 정책도 정의한다.

완료 기준: 외부 참조 두 개 이상 중 후속 쓰기를 의도적으로 실패시켰을 때 삭제되지 않은 엔티티와 계층, 기존 참조가 유지되고 명확한 실패 결과를 반환한다.

### TR-04. Persistent Entity ID 검색 비용

- 위치: [Scene.cpp — FindEntityById / CreateEntity / CreateEntities](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/World/Scene/Private/Scene.cpp)

`FindEntityById`는 IDComponent를 순회하는 O(N) 검색이며, 단일 `CreateEntity`에서도 호출된다. 대량 단일 생성 시 누적 비용이 커질 수 있다. `CreateEntities`에는 별도의 점유 ID 집합을 사용하는 배치 경로가 이미 있다.

권고: 먼저 단일 생성·배치 생성·검색의 비용을 측정한다. 병목이면 씬별 `PersistentId → entt::entity` 역인덱스를 검토한다. 생성, 삭제, 씬 로드, Prefab 인스턴스화, Undo/Redo, ID 변경 및 초기화 시 인덱스 갱신을 한 경로로 묶는다. EnTT handle의 유효성 및 중복 ID 정책을 유지한다.

완료 기준: 1k/10k/100k 등 재현 가능한 규모별 벤치마크 및 ID 무결성 테스트에서 개선과 회귀 여부를 확인한다. 규모와 임계값은 실제 요구에 맞춰 확정한다.

### TR-05. 참조 카운트의 스레드 안전성

- 위치: [Base.h](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Core/Interface/Public/Base.h), [ResourceStreamer.cpp](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Engine/Source/Resource/Private/ResourceStreamer.cpp)

`Base::m_RefCount`는 일반 `int32`로, 스레드 간 동시 AddRef/Release가 허용되는 경우 위험하다. 비동기 스트리밍의 존재 자체가 데이터 레이스 발생을 증명하는 것은 아니다.

권고: 워커 스레드가 Base 객체 소유 참조를 직접 획득·해제하는지 추적하고 스레드 소유권 계약을 정리한다. 실제 공유가 필요하면 atomic intrusive refcount 또는 단일 소유 스레드로 반환하는 큐 등의 설계를 검토한다. atomic 변경만으로 객체 내용의 동시 접근이나 GPU 수명 문제가 해결되지 않는다.

완료 기준: 소유권 경계가 명확하며 동시 참조 변경이 허용된 경로를 스트레스 테스트한다. 스레드 제한이 있는 경로에는 디버그 검증을 고려한다.

### TR-06. 대형 구현 파일의 책임 분리

| 파일 | 기준 커밋 용량 | 검토 방향 |
|---|---:|---|
| DirectX12RHI.cpp | 125,586 B | Frame, upload, descriptor, draw, raytracing 등 변경 경계 검토 |
| SceneViewportPanel.cpp | 119,522 B | UI 상호작용, 렌더 및 도구 책임 경계 검토 |
| DeferredSceneRenderer.cpp | 109,395 B | TAA, Decal, GI, RT, AO 등 효과별 독립 단위 검토 |
| ReflectionPropertyDrawer.cpp | 102,736 B | 타입별 Drawer, 공통 편집 계약 검토 |
| PhysicsSystem.cpp | 97,789 B | 충돌 질의, Trigger, 이동 및 시뮬레이션 책임 검토 |
| Renderer.cpp | 80,882 B | 제출, 큐 관리, 패스 실행 경계 검토 |

파일 길이만으로 결함을 판정하지 않는다. 기존 기능 변경과 테스트를 수반하는 점진적 분리를 우선하고, 규모를 줄이기 위한 일괄 리팩토링은 피한다.

완료 기준: 실제 수정 비용 또는 테스트 독립성이 개선되고, 기존 실행·셰이더 바인딩·렌더 결과에 회귀가 없다.

### TR-07. Undo/Redo Snapshot 비용 검증

전체 씬 Snapshot과 범위 지정 Snapshot을 구분하고, 연속 위젯 변경 병합의 효과를 측정한다. 메모리 사용량과 캡처·복원 p95 시간을 기록한 후 변경 컴포넌트 기반 Delta나 더 작은 Scope를 필요한 경로에 적용한다.

완료 기준: 대규모 씬에서 편집 응답성과 메모리 예산을 확보하고, Undo/Redo·저장 표시·에셋 경로 변경·참조 무결성이 보존된다.

## 6. 기능 확장 및 검증 후보

기존 계획의 우선순위와 중복 여부를 먼저 확인한 후 선택한다. 새로운 기능을 모두 필수 완료 조건으로 추가하지 않는다.

| ID | 후보 | 제안 수준 | 선행 조건 / 완료 방향 |
|---|---|---|---|
| EX-01 | Frame Graph | 신규 설계 후보 | 기존 RenderPass/프로필 보존, Read/Write 선언 및 의존성 정렬부터 점진 도입 |
| EX-02 | GPU Profiler 고도화 | 기존 기능 확장 | 기존 Timestamp·VRAM·MetricsPanel 재활용, 측정 재현성 및 병목 진단 개선 |
| EX-03 | GPU Driven Rendering | 기존 중기 계획 M1 | 가시성·LOD·Indirect Draw의 작은 DX12 비교 경로 |
| EX-04 | Hi-Z Occlusion Culling | M1의 하위 후보 | 기본 가시성 및 Depth Pyramid, 보수적 폴백, 가시성 정확도 검증 |
| EX-05 | ECS Job Scheduler | 조건부 신규 후보 | 의존성 및 read/write 접근 계약, 실제 CPU 병목 측정 후 |
| EX-06 | World Partition/Scene Streaming 고도화 | 기존 기능 확장 | 실제 대규모 콘텐츠, 비동기 로드 및 영속 객체 검증 |
| EX-07 | SDL GPU 기능 동등성 | 필요 기반 확장 | Capability 및 폴백, Instancing/Compute 등 목표 플랫폼 우선 |
| EX-08 | 빌드·테스트 CI | 검증 자동화 | CPU 검사, Windows 빌드, 테스트 로그 및 실패 분리 |
| EX-09 | 실제 게임 제작·배포 검증 | 결과물 검증 | 기존 기능으로 2D/3D 최소 게임의 제작→Shipping Client 실행 |

### 6.1 Frame Graph 제안 범위

| 항목 | 확인한 현재 경로 | 발전 방향 |
|---|---|---|
| 패스 실행 | Priority 정렬 | 명시적 Read/Write 의존성 분석 |
| 리소스 접근 | 패스·RHI에서 직접 지정 | 선언형 리소스 접근 |
| Barrier | RHI 및 호출부 중심 관리 | 의존성 기반 전이·검증 |
| 임시 자원 | 명시적 관리 | Lifetime 분석 및 재사용 |
| 불필요 패스 | 개별 제어 | 필요할 때 Pass Culling |
| 메모리 | 개별 관리 및 기존 Heap Allocator | 필요한 경우 Transient Aliasing |

첫 단계는 기존 Deferred Profile의 작은 패스 집합을 선언형으로 표현하고 결과 및 GPU 시간·CPU 제출 비용을 비교하는 것이다. 완전한 Frame Graph 교체, 다중 큐 스케줄링, Aliasing을 한 번에 도입하지 않는다. 실제 사용 및 검증 결과에 따라 확대한다.

### 6.2 GPU Profiler 관련 정정

앞선 평가에서 GPU Profiler를 신규 구현 후보로 제시했으나, [RenderingDevelopmentPlan.md](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/RenderingDevelopmentPlan.md)의 S1 기록에는 DX12 Timestamp QueryHeap, ReadbackBuffer, 패스별 GPU 시간, VRAM/Descriptor 통계 및 MetricsPanel 구현·과거 실측 검증이 명시되어 있다.

따라서 이 항목은 신규 개발이 아닌 기존 계측의 재현성·검증·고도화로 분류한다. 동일 장면·해상도·장치·옵션에서 CPU/GPU 시간, 프레임 변동, 메모리, 화질을 함께 비교하며, 이전 측정 기록을 최신 검증으로 확대하지 않는다.

## 7. 기존 로드맵과 조화되는 권고 작업 순서

[DevelopmentPriorities.md 11절](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/DevelopmentPriorities.md)을 현재 작업 선택의 기준으로 유지한다. 아래는 이를 대체하는 신규 우선순위가 아니라 검토 결과를 반영할 후보 순서이다.

### 단계 A. 기존 검증 대기 및 결함 해소

- [ ] `RLSimulationCheck` 실패 로그 확인 및 원인 해결 여부 재검증.
- [ ] 기존 검증 대기 CTest, UI/PIE, Terrain/ShaderGraph, Navigation, VFX, 패키지 및 다중 프로젝트 관련 검사 진행.
- [ ] TR-01~TR-03의 실제 호출 계약과 실패 경계 확인 후 필요한 결함 수정 및 회귀 테스트 추가.
- [ ] 기존 Shipping Client 및 `.bampak` 구동 검증 기록 유지, 이후 변경에서 회귀 여부 확인.
- [ ] 누락 리소스 참조 복구 및 데이터 보호 관련 기존 후속 과제와 연계.

### 단계 B. 측정 기반 안정화와 기능 고도화

- [ ] CI 또는 반복 가능한 검증 스크립트로 CPU 테스트·Windows 빌드 결과 기록.
- [ ] TR-04/TR-05/TR-07의 실제 병목 또는 소유권 위반 확인, 필요 시 개선.
- [ ] 기존 GPU Profiler로 대표 장면의 CPU/GPU 시간·메모리 기준 수립.
- [ ] Probe GI 빛샘, 얇은 벽 가시성, DXR 및 기존 렌더 경로의 미검증 항목 확인.
- [ ] Frame Graph/GPU Driven/Hi-Z는 기존 M1 등과 연계하여 작은 실험 경로부터 도입.

### 단계 C. 독립 결과물 제작과 배포 검증

- [ ] 2D 또는 3D 작은 완성 게임으로 편집→플레이→저장→Client 패키징→독립 실행 검증.
- [ ] 강화학습의 외부 학습→평가→정책 재생까지 종단 간 예제 완성.
- [ ] VTuber의 간단한 리그/표정 및 추적→Client→OBS 출력 예제 구축.
- [ ] 자체 모델링·리깅 또는 2D 변형의 편집→저장→변환→재생 예제 구축.
- [ ] 실제 콘텐츠에서 발견한 병목에 따라 확장 기능의 우선순위 재평가.

독립 Shipping Client와 원본 Assets 없는 `.bampak` 실행, HDR 캡처는 2026-11-25 문서에 완료 확인 기록이 있다. 이를 신규 미구현 기능으로 되돌리지 않는다. 현재 커밋과 이후 변경의 회귀 확인은 별도이다.

## 8. 포트폴리오 관점

| 직무 | 프로젝트의 역량 표현 수준 / 10 |
|---|---:|
| 엔진 프로그래머 | 9.0 |
| 그래픽스 프로그래머 | 9.0 |
| 툴 프로그래머 | 8.5 |
| 클라이언트 프로그래머 | 8.0 |
| 게임플레이 프로그래머 | 7.0 |

기술 개수보다 설계 이유, 기존 문제, 수정 전후 비교, 실패 사례와 검증 방법을 중심으로 설명하는 것이 효과적이다.

핵심 사례 후보:

1. **DX12 GPU Resource Lifecycle**: CPU/GPU 비동기 실행에서 발생하는 수명 문제, Fence 기반 Deferred Release, Descriptor 및 Heap Page 반환 계약.
2. **Reflection 기반 제작 환경**: 코드 생성 → ECS 메타데이터 → 직렬화/Prefab → Inspector 및 Undo/Redo의 공통 기반과 생성 비용 개선 사례.
3. **고급 렌더링 파이프라인**: 패스 통합, Temporal Rendering, Raytracing, Probe GI, Tiled Lighting의 실제 측정 및 품질 비교.

수치가 포함된 포트폴리오를 작성할 때는 측정 조건과 날짜를 명시하고 현재 상태에서 재측정한 결과와 과거 기록을 구분한다.

## 9. 검토 한계 및 기록 유지 방식

- 전체 GitHub 파일 트리와 주요 C++/헤더/CMake 구현부를 읽었으나 모든 코드의 호출 관계를 추적하거나 모든 결함을 조사한 것은 아니다.
- GPU, Windows 빌드, CTest, 메모리 프로파일링 및 실제 게임 실행을 새로 수행하지 않았다.
- 구현이 존재한다는 사실과 실제 실행·성능 검증을 구분한다. 구체적인 개선 후보는 기존 테스트 및 문서와 대조하고 재현 가능 여부를 확인한 후 적용한다.
- 우선순위와 평가 점수는 권고이며 사용자 승인이나 기존 로드맵 변경을 의미하지 않는다.
- 향후 각 개선 후보는 `확인 필요 / 재현 / 수정 / 실행 검증 / 성능 검증 / 보류`를 구분하고 근거 커밋 및 테스트 결과를 남긴다. 상세 이력은 기존 `ImplementationLog.md` 및 소유 계획 문서에 연결한다.

### 핵심 참고 문서

- [DevelopmentPriorities.md](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/DevelopmentPriorities.md)
- [EngineDevelopmentPlan.md](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/EngineDevelopmentPlan.md)
- [RenderingDevelopmentPlan.md](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/RenderingDevelopmentPlan.md)
- [ImGuiDevelopmentPlan.md](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/ImGuiDevelopmentPlan.md)
- [BuildAndPackagingPlan.md](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/BuildAndPackagingPlan.md)
- [CodeRefactoringPlan.md](https://github.com/Bamtoliya/BamEngine/blob/be79eed22ef02b6f115034687998773cc1ac2b84/Docs/CodeRefactoringPlan.md): 개선 작업 선택 시 대조할 기존 계획(이 검토에서 전문을 재검토하지 않음).
