# Renderer / RHI / Frame submission

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Internal / Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 사용 경계
일반 게임 개발은 렌더링 컴포넌트와 머티리얼로 시작한다. Renderer/RHI 직접 호출은 커스텀 렌더 패스·엔진 확장용이다. Runtime이 frame begin/end와 시스템 제출을 관리하므로 게임 업데이트에서 프레임을 다시 열지 않는다.

| 공개 API | 사용 의미 |
|---|---|
| `Renderer::BeginFrame()`, `EndFrame()`, `Render(f32)` | Runtime 소유; 결과 EResult 확인 |
| `SubmitStatic(command, passID)` / `SubmitSkinned(command, passID)` | 패스별 draw command 제출 |
| `SubmitCustomCommand(command, passID)` | 사용자 렌더 명령 경계 |
| `ClearRenderQueue(passID)` / `ForgetRenderPass(passID)` | 패스 제거 및 큐 정리 |
| `RenderDynamicGeometry(commands, renderPass)` | 동적 기하 렌더 경로 |
| `RHI::CreateVertexBuffer(data,size,stride)` / `CreateIndexBuffer(...)` | backend buffer 생성 |
| `RHI::SupportsVSync(bool)` / `SetVSync(bool)` | capability 확인 후 결과 확인 |

```cpp
#include "Renderer.h"
void QueueMesh(const Engine::StaticDrawCommand& command,
               Engine::RenderPassID pass) {
    Engine::Renderer::Get().SubmitStatic(command, pass);
}
```
전제: 유효한 render pass 및 렌더 시스템 제출 문맥; command가 참조하는 GPU 자원은 실행 완료 전까지 살아 있어야 한다. 이 조각의 외부 모듈 컴파일 여부는 미검증이다.

## 백엔드 및 GPU 수명
DX12/SDL GPU 외 백엔드 파일이 존재한다고 기능 동등성을 보장하지 않는다. RHI 기본 `SetVSync`는 NotImplemented를 반환한다. backend별 포맷, 바인딩, barrier, fence, shader variant, raytracing 지원 검증이 필요하다. GPU에 제출된 자원을 CPU scope 종료만으로 즉시 제거하지 않는다. 렌더 타겟 resize 및 패스 삭제 이후 옛 포인터를 재사용하지 않는다. `RenderPipelineProfileCheck` 등 이름은 회귀 검사 후보이지 이번 커밋 테스트 통과 기록이 아니다.

## 관련 소스
- [Renderer.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Render/Public/Renderer.h) — `Engine/Source/Render/Public/Renderer.h`
- [RHI.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Render/RHI/Public/RHI.h) — `Engine/Source/Render/RHI/Public/RHI.h`
- [Runtime.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Runtime/Private/Runtime.cpp) — `Engine/Source/Runtime/Private/Runtime.cpp`

## 검증 및 참고 예제
- [RenderPipelineProfileCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/RenderPipelineProfileCheck.cpp)
- [GpuHeapPageAllocatorCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/GpuHeapPageAllocatorCheck.cpp)
- [MeshInstancingCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/MeshInstancingCheck.cpp)
- [TemporalAACheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/TemporalAACheck.cpp)
- [RaytracingBuildPolicyCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/RaytracingBuildPolicyCheck.cpp)
