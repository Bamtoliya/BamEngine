# Render Pipeline Profile 설계

- 갱신일: 2026-11-13
- 상태: 설계. 프로필 파일, 로더, 패스 재구성과 내보내기 연동은 구현 전이다.
- 관련 문서: [빌드·패키징 계획](BuildAndPackagingPlan.md), [렌더링 개발 계획](RenderingDevelopmentPlan.md), [셰이더 바인딩](Shader/BINDING_LAYOUT.md)

## 목적과 현재 상태

Editor의 Game View와 배포 Client가 동일한 프로필을 읽어 동일한 렌더링 구성을 생성한다. 첫 기준 씬은 `RaytracingScene3`이며 `GBufferMaterial`, 그림자, HDR 조명과 씬의 ACES 설정을 비교한다.

현재 Game View는 Editor의 `ViewportDeferredPipeline`으로 렌더 타깃과 그림자·G-Buffer·조명·Forward 패스를 만들고, 씬의 `PostProcessComponent`를 적용한다. Client는 단일 Forward 패스를 등록한다. 기존 `ViewportDeferredPipeline`에는 독립 프리뷰 조명 처리도 들어 있으므로 Game View에서 공유할 실행 기능부터 추출한다.

## 소유권과 파일 형식

- `ProjectSettings.RenderPipelineProfilePath`는 프로젝트의 기본 프로필 URI를 가리킨다. Editor Game View와 Client가 같은 경로를 사용하고, 빌드 패널은 현재 경로를 표시한다. 배포본의 `Config/Project.json`에 실제로 사용한 경로를 기록한다.
- 프로필에는 버전, 순서가 있는 패스 목록, 패스별 설정, 입력 연결, 최종 색상 출력을 저장한다. 첫 후보 경로는 `Project://RenderPipelines/Deferred.bamrenderpipeline.json`이다.
- 씬은 카메라, 조명, 안개와 `PostProcessComponent::PipelineSettings`를 소유한다. 프로필의 `ScenePostProcess` 패스는 씬의 후처리 설정을 적용한다. 화면 크기와 표시 대상은 호스트가 지정한다.
- Engine은 패스 종류별 입출력 규격과 실행 코드를 소유한다. 인스턴스마다 생성하는 패스 ID, 렌더 타깃과 GPU 객체는 런타임 상태다. 프로필에서 등록된 패스를 다시 조합할 수 있으며, 새로운 실행 동작은 Engine의 패스 구현으로 추가한다.

첫 버전은 `Version`, `Passes[]`, `Output`으로 구성한다. 패스마다 `Id`, `Type`, 필요한 경우 `Inputs`와 `Settings`를 가진다. `Inputs`와 `Output`은 `패스ID.출력슬롯`으로 연결한다. 아래는 초기 Deferred 프로필의 설계 예시다.

```json
{
  "Version": 1,
  "Passes": [
    { "Id": "Shadow", "Type": "DirectionalShadow", "Settings": { "Resolution": 4096 } },
    { "Id": "PointShadow", "Type": "PointShadow", "Settings": { "Resolution": 1024 } },
    { "Id": "Geometry", "Type": "GBuffer" },
    {
      "Id": "Lighting", "Type": "DeferredLighting",
      "Inputs": {
        "GBuffer": "Geometry.GBuffer",
        "DirectionalShadow": "Shadow.Depth",
        "PointShadow": "PointShadow.Depth"
      }
    },
    {
      "Id": "Opaque", "Type": "ForwardOpaque",
      "Inputs": { "Color": "Lighting.HdrColor", "Depth": "Geometry.Depth" }
    },
    {
      "Id": "Transparent", "Type": "ForwardTransparent",
      "Inputs": { "Color": "Opaque.Color", "Depth": "Opaque.Depth" }
    },
    {
      "Id": "PostProcess", "Type": "ScenePostProcess",
      "Inputs": { "Color": "Transparent.Color" }
    }
  ],
  "Output": "PostProcess.Color"
}
```

패스는 배열 순서대로 실행하며 앞에 정의된 출력만 읽을 수 있다. `GBuffer` 출력 묶음의 Diffuse·Normal·PBR·Emission·Position 포맷과 슬롯은 패스 규격으로 고정한다. Forward 패스가 색상·깊이 타깃을 제자리에서 갱신하는 경우 출력은 다음 논리 버전으로 취급한다. 이후 패스가 덮어쓰기 전 버전을 요구하면 검증 오류로 처리한다. 활성 효과가 없는 `ScenePostProcess`는 입력 색상을 최종 출력으로 전달할 수 있다.

## 로드, 검증과 런타임 수명

1. 기존 `JsonArchive` 계열을 사용해 프로필을 읽고 쓴다. 프로필 스키마 버전은 `ProjectSettings` 버전과 별도로 관리한다.
2. GPU 없이 실행 순서와 연결을 검증한다. 지원 버전, 중복 ID, 알 수 없는 타입·설정, 빠진 입력, 타입·포맷 불일치, 앞에서 정의되지 않은 입력, 덮어쓴 출력 참조, 최종 색상 출력을 검사한다. 오류에는 프로필 경로, 패스 ID, 필드, 실패 원인을 포함한다. 프로필 경로가 비어 있는 기존 프로젝트는 기존 경로로 실행하고, 명시한 프로필이 잘못됐으면 원인을 표시한다.
3. 실행 가능한 구성을 만들 때 기존 `RenderTargetManager`, `RenderPassManager`, `Renderer`에 연결한다. 각 인스턴스가 고유한 렌더 타깃과 패스 ID를 소유한다. 생성 실패 시 이번 인스턴스가 등록한 항목을 역순으로 정리한다.
4. 현재 개별 패스 등록 해제 API를 보강한다. 프레임 경계에서 카메라 등록, 큐, 패스별 콜백을 제거한 뒤 패스와 타깃을 해제한다. GPU 리소스는 RHI의 사용 완료 및 지연 해제 규칙을 따른다. 리사이즈에서는 화면 크기에 종속된 타깃을 갱신한다.

## 호스트와 패키저 연결

- Game View는 프로필 인스턴스에 씬, 카메라, 뷰포트 크기를 전달하고 최종 색상을 패널에 표시한다. 기존 게임 UI 합성은 Game View에 둔다. 프로필이 후처리를 실행하는 경로에서는 기존 Game View 후처리 호출을 함께 실행하지 않는다. Scene View·썸네일·머티리얼 프리뷰 전환은 Game View 검증 이후에 평가한다.
- Windows Client는 배포 설정과 씬을 읽고 필요한 셰이더를 런타임 리소스로 등록한 뒤 프로필 인스턴스를 생성한다. 최종 색상은 Client의 표시 패스로 백버퍼에 출력한다. `FullscreenQuadVS`, `LightingPS`, `PostProcess_ToneMappingPS`처럼 이름으로 찾는 셰이더는 파일 포함과 런타임 키 등록을 둘 다 확인한다.
- 패키저는 프로필을 명시적으로 읽어 공용 검증기를 호출한다. 패스 타입이 선언한 셰이더와 프로필·씬의 애셋 참조를 모아 대상 플랫폼용 산출물을 스테이징에 포함한다. 최종 게시 전 스테이징 루트에서 프로필과 필수 파일을 다시 검사한다. 누락 또는 미지원 기능은 게시 전에 보고한다.
- 첫 실행 검증은 Windows D3D12를 기준으로 한다. Web은 같은 파일 형식을 사용할 수 있으며, WebGPU 패스 구현과 대상 셰이더를 갖춘 뒤 해당 구성을 검증한다.

## 구현 순서와 완료 기준

1. **파일과 공용 검증기.** `Deferred` 프로필을 읽어 다시 저장해도 순서와 연결이 유지된다. 누락된 G-Buffer 입력, 알 수 없는 타입, 잘못된 최종 출력은 GPU 초기화 전에 실패한다.
2. **Game View 적용.** 같은 카메라와 해상도에서 기존 `RaytracingScene3`의 G-Buffer, 그림자, 조명, 투명 처리와 ACES 결과를 비교한다. 인스턴스를 반복 생성·해제해도 패스나 렌더 타깃이 누적되지 않아야 한다.
3. **Windows 내보내기.** 배포본의 설정에 프로필 경로를 기록하고 저장소 밖에서 실행한다. PIE와 Client가 같은 프로필과 패스 목록을 사용하고, 씬 Exposure와 프로필 그림자 해상도를 각각 바꿨을 때 두 호스트에서 결과가 반영되는지 확인한다. 필수 셰이더를 누락한 경우 패키지가 게시되기 전에 실패해야 한다.

2026-11-13에 소스와 기존 배포 로그를 대조했다. 로그는 `RaytracingScene3` 로드와 Client 초기화 성공을 기록했고 Animator 컨트롤러 경로 오류도 남겼다. 현재 씬의 일부 절대 경로 참조 정리, 저장소 밖 실행, 프로필 기반 화면 일치는 검증 대상으로 남는다.
