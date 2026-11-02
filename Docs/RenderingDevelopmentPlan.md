# 렌더링 개발 목표

- 작성일: 2026-10-24 (최종 갱신: 2026-11-01)
- 상태: 개발 계획 및 2026-11-01 정적 점검·실측 검증 반영 / 아래 장기 목표와 일부 코드 존재 여부는 전체 GPU 실행·성능 측정 완료를 의미하지 않는다.
- 범위: 후처리 효과 추가보다 장면 제출, 가시성, 기하 처리, 재질, 조명과 GPU 자원 관리에 집중한다.
- 관련 지침: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md), [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)
- 연계 계획: [ImGuiDevelopmentPlan.md](ImGuiDevelopmentPlan.md), [BuildAndPackagingPlan.md](BuildAndPackagingPlan.md)
- 분류·이관 기준: [SourceOrganizationGuidelines.md](SourceOrganizationGuidelines.md)
- 통합 우선순위: [DevelopmentPriorities.md](DevelopmentPriorities.md)
- 최신 정적 대조: [2026-11-02 코드·문서 점검](CodeDocumentationAudit-2026-11-02.md) 2.6절·F08·F10. 기본 슬롯 실패와 타일 광원 용량 초과는 잔여 범위다.
- 검증 한계: 아래의 과거 시간·드로우콜 관찰을 최신 동일 조건 비교 실험으로 확대하지 않는다. 이번 갱신에서 새 빌드·GPU 실행·성능 측정은 하지 않았다.

## 1. 목표와 우선순위

기존 래스터라이징 렌더러를 기준 경로로 유지하면서, 측정 가능한 단계로 현대적인 렌더링 기술을 도입한다.
단기에는 정확성과 측정 기반을 확보하고, 중기에는 많은 물체·광원 처리와 하이브리드 레이트레이싱을 확장한다.
장기에는 가상화와 고급 샘플링 기술을 실제 요구에 맞추어 연구한다.

단기·중기·장기는 확정된 주·월 단위 일정이 아니라 선행 조건과 구현 규모에 따른 구분이다.
다른 기능과의 작업 선택은 통합 우선순위 계획을 따른다. S1/S2는 P1, S3/S4/S5는 P2,
M1~M4는 조건부 P3, M5·L 계열은 P4로 대응하되 아래의 실제 선행 조건과 도입 조건을 유지한다.
각 항목은 제안된 목표이며, 모든 기술을 필수 기능으로 일괄 구현하지 않는다.
측정에서 효과가 없거나 현재 콘텐츠에 필요하지 않으면 보류하고 근거를 남긴다.

- 단기: 계측 → 바인딩·기능 지원 범위 정리 → 인스턴싱·LOD → Compute 실행 기반 → 다수 광원 처리.
- 중기: GPU Driven Rendering을 확장하고, 화질 개선 경로로 DXR 그림자·반사와 프로브 GI를 단계적으로 도입한다.
- 장기: Mesh Shader 경험을 바탕으로 가상화 지오메트리를 실험하고, 필요에 따라 VSM·ReSTIR·패스 트레이싱을 검토한다.

GPU Driven과 DXR은 별도의 발전 경로다. Bindless나 Mesh Shader를 DXR의 필수 선행 조건으로 만들지 않는다.
IBL처럼 비교적 독립적인 화질 개선은 측정·바인딩 기반이 마련되면 병행할 수 있다.

## 2. 현재 출발점: 소스에서 확인한 범위 (2026-11-01 갱신)

다음 내용은 2026-11-01 시점 작업 트리의 정적 확인 및 S1·S3·S4 실측 검증 결과다.

- **S1 (CPU/GPU 계측 — 코드 구현 및 실측 확인 완료)**:
  - `RHIFrameMetrics.h`에 패스별 CPU/GPU 시간(`RHIPassMetric`), 하드웨어 지원(`RHICapabilities`), VRAM/디스크립터 통계(`RHIMemoryStats`) 구조 확장이 완료되었다.
  - `DirectX12RHI.cpp`에서 D3D12 Timestamp QueryHeap 및 ReadbackBuffer를 통한 패스별 GPU 시간 지연 회수와 VRAM 예산/사용량/디스크립터 통계 질의(`QueryMemoryStats`)가 구현되었고, 리소스 생성 실패 시 `std::terminate()` 대신 `BAM_LOG_ERROR` 및 `EResult::Fail`을 반환하도록 안정화되었다.
  - `MetricsPanel.cpp`에서 패스별 CPU/GPU 시간 테이블, VRAM/디스크립터 사용량, 하드웨어 기능 지원 여부를 시각화하며 실시간 110~116 FPS 환경에서 계측 동작이 확인되었다.
- **S2 (바인딩 레이아웃 계약 및 텍스처 슬롯 상태 누수 방지)**:
  - [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)를 `DirectX12RHI::CreateRootSignature()`의 실제 `space0`~`space3`(Root Parameter `[0]`~`[10]`) 구조와 일치하도록 갱신했다.
  - `MaterialInterface::Bind()`에서 G-Buffer 슬롯(0: Diffuse, 1: Normal, 2: PBR, 3: Emission) 점유 여부를 추적하고, 미지정 슬롯을 안전한 기본 더미 텍스처(`white1x1`, `blue1x1`, `black1x1`)로 리셋하여 이전 드로우콜의 텍스처 잔류 누수 현상을 즉시 차단했다 (후속으로 S2-C 플래그 기반 샘플링 분기 계획 수립).
  - `SDLGPURHI`는 `DrawIndexedInstanced`(`NotImplemented`), `BindInstanceBuffer`, Compute 인터페이스가 미구현 상태로, 미지원 시 안전 폴백 경로를 유지한다.
- **S3 (정적 메시 인스턴싱, 다중 패스 독립 LOD 및 GPU 스키닝 제출 — 구현 및 배칭 실측 완료)**:
  - `InstanceData`(`worldMatrix`, `normalMatrix`, `pickingID`), `LODGroupComponent`가 추가되었다.
  - `MeshRenderSystem::OnSubmit()`에서 `PassTarget` 구조체를 통해 활성 뷰포트/패스별 고유 카메라(위치, `projY`) 기준으로 독립적인 화면 덮개 비율(Screen Coverage) + 히스테리시스 LOD를 평가·제출하며, 주 뷰포트(`isPrimaryGeometry`)일 때만 `lodGroup->currentLODIndex`를 갱신하여 보조 패스로 인한 LOD 상태 오염을 차단했다.
  - `Renderer::RenderMeshes()`가 `BindInstanceBuffer()` 및 인덱스/비인덱스 여부에 따라 `DrawIndexedInstanced()` / `DrawInstanced()` 배칭 드로우를 수행하며, 인덱스 31만 개 추가 시에도 드로우콜(56 -> 57회)이 유지됨을 실측 검증했다.
  - `SkinnedMeshRendererComponent`와 `AnimatorComponent::finalBoneMatrices`를 연결한 GPU 스키닝 구조화 버퍼 제출(`SubmitSkinned` / `RenderSkinnedMeshes`)도 구축되었다.
- **S4 (Compute 기반 및 Tiled Light Culling — 구현 및 실측 검증 완료)**:
  - `ShaderBuild.cmake`에 `*CS.hlsl`(`cs_6_0`) 컴파일 추가, `DirectX12Pipeline` Compute PSO, `DirectX12RHI::Dispatch`/`ResourceBarrierUAV`, `LightManager::DispatchLightCulling` 구현 완료.
  - `tiledLightCullingCS.hlsl`에 타일의 4방향 절두체 평면(`s_FrustumPlanes[4]`) 역투영 및 구면 교차 판정이 추가되어 전체 타일 광원 과할당 문제가 해결되었다.
  - `lightingPS.hlsl`에서 선별된 타일 광원만 순회하도록 수정되어 `tileLightCount == 0`일 때의 전체 광원 순회 버그가 해결되었으며, 라이팅 비집중 영역 응시 시 `LightingPass` GPU 시간이 1.10 ms -> 0.25 ms(77% 단축)로 감소함을 실측 확인했다.
  - `SceneViewportPanel`과 `ViewportDeferredPipeline` 모두 `DispatchLightCulling` 및 버퍼 바인딩이 일관되게 연결되었다.
- **S5 (IBL, 그림자 안정화 및 HDR·공통 디퍼드 파이프라인 일원화 — 구현 완료)**:
  - `lightingPS.hlsl`에 반구형 Sky Irradiance·거칠기 기반 수렴(`LIGHT_TYPE_SKY` + `EnvBRDFApprox`), 접촉 그림자 소프트닝, `ShadowStrength` 및 큐브맵 기반 Split-Sum PBR IBL(`gLightingData.HasIBLMaps != 0u`, `gIrradianceMap`, `gPrefilteredEnvMap`, `gBRDFLut`)이 반영되었다.
  - `DirectX12RHI`의 큐브맵 Null SRV 차원(`D3D12_SRV_DIMENSION_TEXTURECUBE`) 안전장치가 보강되었으며, `SceneViewportPanel`의 중복 디퍼드 패스 코드를 `ViewportDeferredPipeline`으로 통합하여 Scene View, Game View, Material Preview 전반의 렌더링 일관성을 확보했다.
  - **선형 HDR 및 포스트프로세스 일원화**: `lightingPS.hlsl`의 내장 Reinhard/감마 보정을 제거하고 `LightingColor` 렌더타깃을 `R16G16B16A16_FLOAT`로 승격하여 선형 HDR 광량을 보존하며, `PostProcessPipeline`(8종 효과 및 `PostProcessComponent` 부재 시 Default ACES 톤매핑 + 감마 2.2 자동 폴백)이 Scene View와 Game View 모두에 일관되게 적용된다.

주요 확인 파일:

- [Renderer.cpp](../Engine/Source/Render/Private/Renderer.cpp), [LightManager.cpp](../Engine/Source/Render/Light/Private/LightManager.cpp)
- [MeshRenderSystem.cpp](../Engine/Source/System/Render/Private/MeshRenderSystem.cpp), [LODGroupComponent.h](../Engine/Source/Core/Public/Components/Render/LODGroupComponent.h)
- [RHI.h](../Engine/Source/Render/RHI/Public/RHI.h), [RHIFrameMetrics.h](../Engine/Source/Render/RHI/Public/RHIFrameMetrics.h)
- [DirectX12RHI.cpp](../Engine/Source/Render/RHI/Platform/DirectX12/Private/DirectX12RHI.cpp), [SDLGPURHI.cpp](../Engine/Source/Render/RHI/Platform/SDLGPU/Private/SDLGPURHI.cpp)
- [tiledLightCullingCS.hlsl](../Resources/Shader/hlsl/tiledLightCullingCS.hlsl), [lightingPS.hlsl](../Resources/Shader/hlsl/lightingPS.hlsl), [ShaderBuild.cmake](../Engine/CMake/ShaderBuild.cmake)

## 3. 도입 원칙

1. 기본 래스터 경로와 신규 경로를 같은 씬에서 비교할 수 있게 유지한다.
2. 고급 기능은 우선 DirectX12에서 검증하고, 공통 RHI에는 실제 사용에 필요한 계약만 추가한다.
   백엔드별 기능 지원을 조회하고, 미지원 시 기본 경로 또는 명확한 실패를 선택한다.
   Vulkan·Metal 등의 이름이나 셰이더 출력 형식이 존재한다는 이유로 동일 지원을 주장하지 않는다.
3. GPU 버퍼·디스크립터·가속 구조의 소유자와 해제 시점을 명확히 한다.
   프레임 슬롯 재사용과 핫 리로드에서는 GPU 완료 전 데이터를 덮어쓰거나 자원을 해제하지 않는다.
4. 렌더링 기능은 Engine에 두고 Editor는 설정·시각화·디버깅을 담당한다.
   Editor 없이 Client에서도 같은 장면 데이터와 실행 경로를 사용할 수 있어야 한다.
   정점·입력 레이아웃·셰이더 공통 정의는 렌더링 소유 영역에서 관리하고, 경로 정리로 바인딩 ABI를 변경하지 않는다.
5. 전체 화면 패스나 Compute 사용 여부로 후처리를 구분하지 않는다.
   가시성·조명 계산도 전체 화면에서 실행할 수 있다.
   RT·GI에 필요한 누적과 디노이징은 품질 확보 범위에 포함하되, 독립적인 후처리 효과 추가는 본 계획의 중심에서 제외한다.
6. 기능마다 CPU 비용·GPU 비용·메모리·화질을 함께 기록한다. 드로우 수 감소만으로 성능 향상을 주장하지 않는다.

## 4. 단기 목표: 기존 렌더러의 측정과 확장 기반

### S1. 재현 가능한 성능 측정과 기능 지원 조회

**목적:** CPU 제출, 그림자, 기하 처리, 조명 중 어떤 부분을 개선할지 근거를 확보한다.

- 기존 `RHIFrameMetrics`를 활용해 프레임·뷰포트·패스 통계의 집계 범위를 명시한다.
- CPU 제출 시간, 패스별 GPU timestamp와 GPU 대기 시간을 측정한다.
  결과는 완료된 프레임에서 지연 회수하고, 측정 때문에 매 프레임 GPU 전체 대기를 추가하지 않는다.
- 제출/컬링 물체 수, PSO·재질 변경, 디스크립터 사용량과 주요 GPU 메모리를 관찰한다.
- Compute, 간접 그리기, Mesh Shader, DXR, 직접 디스크립터 인덱싱의 지원 여부를 분리해 조회한다.
  필요한 OS·SDK·드라이버·Shader Model 조건은 기능 구현 시 공식 명세와 실제 장치로 확정한다.

**완료 기준:** 고정 씬·카메라·해상도에서 CPU/GPU 시간을 구분해 기록하고,
미지원 기능을 선택했을 때 기본 경로로 돌아가거나 이유를 명확히 표시한다.

### S2. 셰이더 바인딩과 렌더 자원 계약 정리

**목적:** 새 버퍼와 패스 추가 시 기존 셰이더·머티리얼·핫 리로드가 깨지는 일을 줄인다.

- Geometry·Lighting·Shadow·Sky·UI의 실제 register/space와 루트 시그니처를 대조한다.
  SDL GPU를 통한 SPIR-V 사용 경로와 GLSL 소스의 유지 범위도 확인한다.
- 기존 번호를 이상적인 순서로 재배치하기보다, 현재 패스별 예외와 새 그룹 배치 규칙을 문서화한다.
- 프레임별 디스크립터 할당·재사용·용량 초과·지연 해제를 검증한다.
- 리소스 읽기/쓰기 전환과 패스 입력·출력을 명시한다.
  자동 Render Graph는 패스 의존성 관리가 실제 문제로 확인될 때 후속으로 검토한다.

**완료 기준:** 바인딩 문서로 각 패스의 리소스를 추적할 수 있고,
리사이즈·셰이더 교체 실패·씬 교체 후에도 기존 유효 경로를 유지한다.

### S2-B. 셰이더 리플렉션 기반 머티리얼 상수 버퍼 자동 레이아웃 (중장기 목표)

**목적:** 머티리얼과 후처리 파라미터의 오프셋 하드코딩을 제거하고, 셰이더 바이너리로부터 상수 버퍼 구조를 자동 추출하여 안전성과 생산성을 극대화한다.

- DXC Shader Reflection API(`ID3D12ShaderReflection`) 및 Vulkan용 `SPIRV-Reflect`를 셰이더 컴파일/로딩 단계에 연결한다.
- 셰이더 바이너리(`.cso`, `.spv`) 파싱 시 대상 ConstantBuffer(`b0, space3` 등)의 변수 이름, 타입(Float, Vec4 등), 크기, 바이트 오프셋 메타데이터를 자동 수집한다.
- `MaterialInterface` 생성 시 수집된 리플렉션 정보를 기반으로 `m_Parameters` 테이블을 스스로 구성한다.
- 프로그래머나 아티스트가 오프셋을 전혀 몰라도 `mat->SetFloat("Name", value)` 호출 시 정확한 바이트 위치에 값이 쓰이도록 보장한다.

**완료 기준:** 셰이더 코드에서 변수 순서나 타입을 바꾸더라도 C++ 코드의 수동 수정 없이 런타임에서 오프셋 불일치나 화면 왜곡 없이 값이 올바르게 전달된다.

### S2-C. 플래그 기반 텍스처 샘플링 분기 및 머티리얼 그래프 입력 유연화

실행 순서: 현재 기본 텍스처 확보/바인딩 실패(F08)와 관련 리소스 오류를 정리한 뒤,
유효성 플래그·기본값 분기(3-A) → 그래프 상수/채널 입력(3-B)을 별도 작업으로 진행한다.
타일 용량 초과(F10)는 해당 장면을 사용하기 전에 해결하며 전체 배포 완료를 이 기능의 필수 조건으로 두지 않는다.
통합 선택·검증 기준은 [DevelopmentPriorities.md 10절](DevelopmentPriorities.md)을 따른다.

**목적:** 텍스처를 무조건 샘플링하는 고정 파이프라인에서 탈피하여, 슬롯 미지정 시의 상태 누수(오염) 방지 및 머티리얼 그래프를 통한 유연한 상수/벡터/채널 분리 입력을 지원한다.

- **1. 텍스처 유효성 비트 플래그(`MaterialFeatureFlags`) 도입**:
  - 머티리얼 상수 버퍼에 텍스처 슬롯별 유효성 플래그(`FLAG_HAS_DIFFUSE_MAP`, `FLAG_HAS_NORMAL_MAP`, `FLAG_HAS_PBR_MAP`, `FLAG_HAS_EMISSIVE_MAP` 등)를 정의한다.
  - G-Buffer 셰이더(`gbufferPS.hlsl`)에서 텍스처가 없는 슬롯은 샘플링을 수행하지 않고 안전한 수학적 기본값(Emission: 0.0, Normal: 정점 법선, PBR: 상수 거칠기/금속도/차폐 1.0)으로 즉시 분기한다.
  - 슬롯을 1x1 더미 텍스처로 채우지 않고 순수하게 비우는(Unbound/Null SRV) 방식을 취하더라도, 드로우콜 간 텍스처 잔류로 인한 PBR/Emission 오염 및 화면 폭발 버그를 원천 차단한다.
- **2. 머티리얼 그래프 기반 상수/벡터/컬러 분리 입력 연동**:
  - 텍스처 에셋 기반 샘플링뿐만 아니라, 노드 그래프에서 특정 변수나 단일 Color(RGBA), 스칼라 파라미터(Float), 분리된 채널(Split RGBA / Swizzle)을 G-Buffer의 각 속성에 직접 공급할 수 있는 평가 경로를 구축한다.
  - Albedo에 단색(Color Uniform) 직결, Roughness/Metallic에 스칼라 슬라이더 직결, Emission에 HDR Color 직결 등 텍스처 없는 절차적/상수 기반 머티리얼 작성을 완전하게 지원한다.
- **3. 샘플링 대역폭 최적화**:
  - 텍스처가 바인딩되지 않은 슬롯의 GPU 메모리 텍스처 페치(Texture Fetch) 대역폭을 0으로 절감한다.

**완료 기준:** 텍스처가 없는 머티리얼에서 이전 드로우콜의 텍스처 오염이나 하얗게 새는 현상이 발생하지 않고, 머티리얼 그래프에서 연결된 상수 컬러 및 채널 분리 데이터가 G-Buffer와 최종 라이팅에 정확히 반영된다.

### S3. 정적 메시 인스턴싱과 기본 LOD

**목적:** 반복되는 메시의 CPU 제출과 불필요한 기하 처리 비용을 줄인다.

- 실제 인스턴스 수·시작 위치·인덱스 범위를 전달할 수 있는 RHI 드로우 계약을 정의한다.
- 같은 메시·호환 재질·패스 상태를 가진 불투명 정적 메시부터 배칭한다.
- 월드 행렬과 Picking ID를 인스턴스 데이터로 공급한다.
  음수 스케일, 비균일 스케일의 노멀 변환과 머티리얼별 차이를 처리한다.
- 화면 크기 기반의 단순 메시 LOD를 작은 예제로 추가하고, 경계 진동을 완화한다.
  투명 정렬, 지형 변위, 스키닝을 처음부터 같은 배칭 경로에 강제하지 않는다.

**완료 기준:** 반복 메시 씬에서 단일 드로우 경로와 화면·피킹·그림자가 일치하고,
드로우 수와 CPU/GPU 시간의 변화를 각각 기록한다. LOD 경계에서 물체가 사라지지 않는다.

### S4. Compute 실행 기반과 Tiled/Clustered Lighting

**목적:** 전체 광원 순회 비용을 줄이고 GPU 컬링·GI의 공통 기반을 마련한다.

- Compute PSO·Dispatch·UAV 버퍼·카운터·리소스 전환·메모리 가시성 처리를 연결한다.
  셰이더 자동 빌드와 런타임 컴파일·리플렉션 경로도 확장한다.
- **내장 시스템 셰이더 식별자 정리:**
  `TiledLightCullingCS`, `LightingPS` 등 엔진 내장 셰이더 경로의 하드코딩 매직 스트링을 심볼릭 논리 키 및 C++ `constexpr` 상수로 일원화하여 에셋 셰이더와의 참조 경계를 정돈한다.
- 기존 Deferred Lighting에 타일별 광원 목록을 먼저 연결한다.
  깊이 구분이 필요하거나 타일별 목록이 커지는 경우 Clustered 방식으로 확장한다.
- Directional·Sky Light와 범위가 있는 Point·Spot Light를 구분한다.
- 목록 용량 초과를 계측하고, 안전한 전체 순회 등 정확성을 유지하는 대체 경로를 제공한다.
  투명 Forward 경로에서 불투명 깊이만으로 광원을 제거하지 않는다.
- 초기에는 단일 큐로 검증한다. 비동기 Compute는 겹칠 작업과 이득이 측정될 때 추가한다.

**완료 기준:** 광원 수 0·1·다수, 목록 초과, 카메라 근접 경계에서 기본 경로와 결과가 일치한다.
적은 광원과 많은 광원 모두 측정해 새 방식의 손익 조건을 기록한다.
개념 참고: [Clustered Deferred and Forward Shading](https://www.cse.chalmers.se/~uffe/clustered_shading_preprint.pdf).

### S5. 환경맵 기반 IBL과 기존 그림자 안정화

**목적:** 동적 GI 전에 재질의 환경광 응답과 기본 그림자 품질을 확보하고, 다중 광원 환경에서의 그림자 농도·조도 밸런스를 개선한다.

- Diffuse irradiance, roughness별 specular prefilter, BRDF LUT로 환경맵 IBL을 구성한다.
  먼저 고정 환경맵 하나로 시작하고, 동적 캡처·다수 Reflection Probe는 필요에 따라 확장한다.
- 색 공간, 선형 HDR 조명과 노출 조건을 고정해 금속도·거칠기별 구체를 비교한다.
- 기존 CSM·Spot·Point 그림자의 바이어스, 캐스케이드 경계, 필터 비용과 카메라 이동 안정성을 점검한다.
  기존 코드에 있는 CSM·PCF·PCSS 계열 처리를 신규 구현 성과로 중복 기록하지 않는다.
- **다중 광원 그림자 농도 균형 및 제어 기능 검토:**
  - Direct Light와 Point/Spot Light가 중첩될 때 발생하는 그림자 농도(대비) 불균일 현상을 분석하고 조도 밸런싱 가이드라인을 수립한다.
  - Directional Light에만 독점 적용된 접촉 그림자(`ContactShadow`)로 인한 그림자 간 이질감을 완화한다.
  - 광원별 그림자 강도(`ShadowStrength`, 0.0~1.0) 파라미터 또는 앰비언트 최솟값 보정 도입을 검토하여 과도한 칠흑 그림자(Pitch Black)를 방지한다.

**완료 기준:** 재질·환경맵 변경이 재현 가능하고, 직접광을 꺼도 환경광 반응을 확인할 수 있다.
그림자 품질과 비용은 고정 카메라 및 이동 카메라에서 각각 기록하며, 다중 광원 씬에서 그림자 농도 왜곡이나 과도한 간섭 없이 균형 잡힌 명암이 출력된다.

## 5. 중기 목표: GPU 중심 제출과 하이브리드 조명

### M1. GPU Driven Rendering과 Hi-Z 오클루전 컬링

**선행 조건:** S1~S3, S4의 Compute 실행 기반.

- GPU 장면 버퍼에 인스턴스·경계·메시·재질 참조를 구성하고 생성·수정·삭제를 갱신한다.
- GPU 프러스텀·LOD 선택 → 가시 명령 생성 → 간접 그리기 순서로 구현한다.
  DirectX12에서는 `ExecuteIndirect`를 우선 검토한다.
- 이후 Hi-Z 깊이 피라미드로 가려진 물체를 제거한다.
  이전 프레임 깊이 사용 시 카메라 컷·이동·새 물체·가림 해제에서 보수적인 재노출 정책을 둔다.
- 뷰포트와 그림자 뷰의 가시 목록을 구분한다. 카메라 밖의 그림자 캐스터를 잘못 제거하지 않는다.
- 프레임마다 가시 목록을 CPU로 읽어 드로우를 재구성하는 경로를 필수로 만들지 않는다.

**완료 기준:** CPU 기준 경로와 가시성·피킹·그림자가 일치하며, 갑작스러운 카메라 이동에서 누락이 없다.
가림이 많은 씬과 거의 없는 씬의 총비용을 비교한다.
참고: [Indirect Drawing and GPU Culling](https://learn.microsoft.com/en-us/windows/win32/direct3d12/indirect-drawing-and-gpu-culling-).

### M2. 선택적 Bindless 리소스 접근

**도입 조건:** 다수 재질의 바인딩 비용이나 GPU Driven 명령 구성 제약이 실제로 확인된 경우.

- GPU 재질 테이블에서 텍스처·샘플러를 인덱스로 참조한다.
- 직접 힙 인덱싱을 사용한다면 SM 6.6과 리소스 바인딩 지원 조건을 조회한다.
  셰이더 프로파일·루트 시그니처·컴파일 캐시·리플렉션과 기존 머티리얼 ABI를 함께 다룬다.
- 무효 인덱스의 기본 리소스, 슬롯 재사용과 GPU 완료 전 해제 방지 정책을 정의한다.
- 기존 바인딩 경로를 유지한다. Bindless를 모든 백엔드나 DXR의 필수 조건으로 삼지 않는다.

**완료 기준:** 많은 서로 다른 재질에서 슬롯 오염이 없고, 핫 리로드·삭제·씬 교체가 안전하다.
디스크립터 메모리와 CPU/GPU 비용까지 비교해 기본 사용 여부를 결정한다.
참고: [HLSL Dynamic Resources](https://microsoft.github.io/DirectX-Specs/d3d/HLSL_SM_6_6_DynamicResources.html).

### M3. DXR 그림자 → 반사

**선행 조건:** 기능 지원 조회, 자원 수명·배리어 관리와 패스 계측. M1·M2 완료는 필수가 아니다.

- 정적 삼각형 메시 BLAS와 인스턴스 TLAS를 생성하고 Transform·추가·삭제 변경을 반영한다.
- 첫 단계는 불투명 정적 메시의 단일 광원 그림자다.
  단순 차폐 조회에는 RayQuery를 검토하되, 전체 DXR 파이프라인과의 선택 이유를 기록한다.
- 다음 단계로 반사에 필요한 메시 속성·재질·텍스처 조회를 연결한다.
  기존 픽셀 셰이더를 이름만 바꾸어 재사용할 수 있다고 가정하지 않는다.
- Masked, 스키닝, 지형 변위, 투명 재질의 지원 정책을 따로 정의한다.
  초기 미지원 형상은 누락 사실을 표시하고 기존 그림자·반사 경로의 대체 정책을 명시한다.
- 정적 메시에 충분한 샘플을 사용하는 기준 영상을 먼저 만들고,
  이후 실시간 샘플 수를 위한 누적·디노이징과 카메라 컷 시 이력 초기화를 추가한다.

**완료 기준:** 화면 밖 물체의 차폐·반사가 확인되고, 인스턴스 이동·삭제·씬 교체 후 잔여 기하가 없다.
추적뿐 아니라 BLAS/TLAS 갱신, 누적·디노이징과 메모리까지 포함해 측정한다.
참고: [DirectX Raytracing 명세](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html).

### M4. 프로브 기반 동적 GI

**선행 조건:** S5의 간접광 합성 기준과 M3의 광선 조회 기반. DDGI는 우선 검토할 구현 방식이다.

- 제한된 실내 공간에 프로브를 배치하고 irradiance와 가시성 정보를 갱신한다.
- 프레임당 갱신량을 제한하고, 프로브의 벽 내부 배치·빛샘·보간·시간적 수렴을 처리한다.
- IBL·직접광·GI의 기여를 분리해 중복 조명을 방지한다.
- 먼저 확산 간접광을 지원한다. 거울 반사나 모든 재질의 간접 반사를 해결했다고 주장하지 않는다.

**완료 기준:** 직접광이 닿지 않는 면의 색 번짐과 조명 이동 반응을 확인한다.
벽 두께·프로브 간격·급격한 광원 변화에 따른 누출과 지연, 갱신 예산을 기록한다.
참고: [RTX Global Illumination / DDGI](https://developer.nvidia.com/blog/rtx-global-illumination-part-i/).

### M5. Mesh Shader와 Meshlet 처리 실험

**선행 조건:** 일반 메시의 GPU Driven 기준 경로와 재현 가능한 고밀도 메시 씬.

- 정적 메시를 Meshlet으로 전처리하고 저장·로딩 경로에 연결한다.
- Mesh Shader부터 비교한 뒤 필요할 때 Amplification Shader와 묶음 단위 컬링을 추가한다.
- 경계·노멀 콘 컬링의 전제를 확인한다. 양면 재질·음수 스케일·변형 메시를 별도로 처리한다.
- 기존 Vertex Shader 경로를 유지하고 GPU·장면별 손익을 기록한다.

**완료 기준:** 같은 모델에서 형상·재질·그림자가 일치하고, 전처리·메모리·GPU 비용을 함께 비교한다.
Mesh Shader 지원 자체를 Nanite 구현으로 표현하지 않는다.
참고: [Mesh Shader 명세](https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html).

## 6. 장기 목표: 요구가 확인된 기술부터 연구

### L1. 가상화 지오메트리

- **도입 이유:** 고밀도 정적 메시를 기존 LOD와 스트리밍만으로 다루기 어려울 때 검토한다.
- **범위:** 클러스터 계층·오차 기준·페이지 스트리밍·상주 예산·GPU 선택과 래스터 경로를 연결한다.
  먼저 제한된 정적 불투명 모델만 대상으로 한다.
- **비용과 제약:** 에셋 변환·파일 형식·런타임 메모리·그림자·피킹·RT 기하 표현까지 영향이 크다.
  소프트웨어 래스터라이징은 별도 손익 검토 대상이며 필수 출발점으로 두지 않는다.
- **완료 기준:** 카메라 이동과 페이지 미상주 상태에서 구멍이 없고, 원본 대비 오차·메모리·I/O 비용을 기록한다.

Nanite는 참고할 시스템이다. 첫 목표는 제한된 가상화 지오메트리 실험이며 동등한 기능을 약속하지 않는다.
참고: [Nanite 기술 발표](https://advances.realtimerendering.com/s2021/Karis_Nanite_SIGGRAPH_Advances_2021_final.pdf).

### L2. Virtual Shadow Maps

- **도입 이유:** 기존 CSM·아틀라스의 해상도 분배나 다수 그림자 광원의 비용이 실제 한계가 될 때 검토한다.
- **범위:** 가상 페이지·물리 페이지 풀·요청 수집·페이지별 캐스터 컬링·캐시 무효화를 구현한다.
- **비용과 제약:** 움직이는 광원·스키닝·지형·정점 변위가 많은 경우 캐시 재생성 비용이 커질 수 있다.
  가상화 지오메트리 전체를 필수 조건으로 만들지는 않는다.
- **완료 기준:** 페이지 예산 초과에서도 정의된 대체 동작을 유지하고, 정적/동적 씬에서 품질·메모리·시간을 비교한다.

참고: [Epic Virtual Shadow Maps](https://dev.epicgames.com/documentation/unreal-engine/virtual-shadow-maps-in-unreal-engine).

### L3. ReSTIR 기반 다수 광원·간접광 샘플링

- **도입 이유:** Clustered Lighting과 기존 RT 샘플링만으로 많은 광원을 처리하기 어려울 때 검토한다.
- **범위:** ReSTIR DI부터 시작해 후보 샘플, reservoir, 공간·시간 재사용과 가시성 검사를 구현한다.
  이후 필요할 때 GI·경로 재사용으로 확장한다.
- **비용과 제약:** 움직임·가림 해제에서 잘못된 샘플 재사용과 잔상, 분산·편향의 조건을 검토해야 한다.
  MegaLights는 참고 시스템이며 ReSTIR와 동일한 알고리즘이라고 가정하지 않는다.
- **완료 기준:** 충분한 샘플의 기준 영상과 비교하고, 급격한 광원·카메라 변화에서 오차와 총 실행 비용을 기록한다.

참고: [NVIDIA RTXDI](https://github.com/NVIDIA-RTX/RTXDI).

### L4. 패스 트레이싱 기준 렌더러

- **도입 이유:** 실시간 조명 결과를 비교할 기준 영상과 빛 전달 학습 도구가 필요할 때 추가한다.
- **범위:** 먼저 정적 씬의 누적 렌더링, 다중 반사, 직접광 중요도 샘플링과 MIS를 구현한다.
  충분히 수렴한 영상을 저장하고, 카메라·기하·재질·광원 변경 시 누적을 초기화한다.
- **비용과 제약:** 초기 목표는 실시간 게임 경로 전체의 교체가 아니다.
  실시간 전환은 샘플링·디노이징·지원 하드웨어 예산이 확보된 뒤 별도로 판단한다.
- **완료 기준:** 단순 분석 장면에서 조명·재질을 검증하고, 샘플 수에 따른 수렴과 기준 영상의 오차를 기록한다.

### L5. 추가 연구 후보

- **Work Graphs:** GPU 내부 작업 생성·분류가 기존 Dispatch·간접 실행으로 병목이 될 때 검토한다.
  기존 경로를 대체할 필요, 장치·SDK 지원과 메모리 비용을 먼저 확인한다.
- **Virtual Texturing:** 실제 텍스처 상주 메모리와 로딩이 한계일 때 검토한다.
  페이지 요청·미상주 대체 mip·비동기 업로드를 작은 예제로 검증한다.
- **Visibility Buffer:** G-buffer 대역폭 또는 다수 재질 처리 비용이 주요 병목일 때 검토한다.
  재질 복원·미분·Masked 처리 복잡성을 포함해 기존 Deferred와 비교한다.
- **Neural Rendering·3D Gaussian Splatting:** 학습·실사 캡처 콘텐츠라는 별도 요구가 생길 때 독립 실험으로 다룬다.
  일반 메시·재질·동적 조명의 기본 경로를 대체하는 필수 목표로 두지 않는다.

## 7. 공통 검증과 완료 기록

### 비교 장면과 측정 조건

- 작은 기본 씬: 신규 경로의 고정 비용과 기존 기능 회귀 확인.
- 반복 메시 씬: 예를 들어 1천·1만 개로 늘리며 인스턴싱·GPU 제출 비용 확인.
- 다양한 재질 씬: 바인딩 비용, 디스크립터 예산과 Bindless 비교.
- 가림이 많은 실내/거의 없는 야외: CPU 컬링과 GPU·Hi-Z 컬링 손익 비교.
- 다수 광원 씬: 광원 수와 겹침 정도를 나누어 목록 길이·오버플로·조명 비용 확인.
- 고밀도 모델·이동 물체 씬: LOD·Meshlet·스트리밍·가속 구조 갱신 확인.
- 단순 조명 기준 씬: 금속·거칠기 구체, 얇은 벽과 색상 벽으로 반사·GI 오차 확인.

위 물체 수는 검증용 예시이며 달성 성능이나 지원 한도를 뜻하지 않는다.
같은 GPU·드라이버·빌드 구성·내부 해상도·카메라·광원·품질로 비교한다.
성능 비교 시 VSync·프레임 제한 상태를 기록하고, 워밍업 뒤 여러 구간의 중앙값과 상위 지연을 남긴다.
평균 FPS 외에 CPU 제출·패스별 GPU ms·메모리·업로드량과 화질 차이를 함께 남긴다.

### 실행 흐름과 경계 조건

- 사용자가 Visual Studio에서 대상 구성을 빌드하고 F5로 실행하는 경로를 기본 검증 흐름으로 삼는다.
- Scene View·다중 뷰포트·머티리얼 미리보기·PIE 반복 시작/종료·Client에서 기능 범위와 회귀를 확인한다.
- 씬 교체, 창·뷰포트 리사이즈, 카메라 컷, 에셋 삭제와 셰이더 핫 리로드 실패를 확인한다.
- 2D Sprite·UI 정렬과 투명 재질, 지형·스키닝처럼 지원 경계가 다른 경로를 별도로 기록한다.
- 지원하지 않는 GPU, 빈 장면, 최대 목록 초과와 자원 할당 실패에서 정의된 동작을 확인한다.
- GPU 디버그 계층·검증 도구를 활용하되, 검증 모드의 성능을 일반 실행 수치와 섞지 않는다.

### 설정·도구·완료 판정

- 품질·디버그 옵션은 실제 구현된 기능만 노출한다. 미지원 옵션에는 이유와 대체 동작을 표시한다.
- 설정 UI는 `ImGuiDevelopmentPlan.md`의 GraphicsSettingPanel 방향과 연결한다.
  UI 구현과 엔진 실행 로직을 분리하고, 필요한 사용자 설정만 기존 저장 체계에 연결한다.
- 고정 표시 문구는 기존 로캘 키를 먼저 확인하고 Engine.csv·Editor.csv의 영어·한국어 번역을 함께 등록한다.
  시각화 토글·단축키·카메라 조작은 InputManager 경로를 따른다.
- 기능별 기록에는 변경 파일, 지원 백엔드·형상·재질, 정상/실패 동작, 검증 환경과 미검증 항목을 남긴다.
- 상태는 계획·구현 중·정적 확인·실행 검증 완료를 구분한다.
  코드나 API 선언의 존재, 문서의 체크 표시, 한 번의 FPS 관찰을 완료 근거로 삼지 않는다.

이 문서 작성에 대한 직접 수정 허가는 렌더러 구현이나 빌드·테스트 실행 허가로 확대하지 않는다.
후속 작업은 루트 AGENTS.md와 해당 요청에서 허용된 범위를 따른다.
