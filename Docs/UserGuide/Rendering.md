# 렌더링

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 재질 및 조명

콘텐츠 브라우저에서 Material 또는 Material Instance를 생성하고 Material Editor에서 현재 지원되는 셰이더와 파라미터를 선택한다. StaticMeshRenderer/SkinnedMeshRenderer에 메시 및 재질을 할당한다. Texture 리소스 로드 및 색 공간 등 실제 제공 옵션을 확인한다. 재질 편집 후 Preview와 Scene/Game View를 비교한다.

Directional / Point / Spot / Sky Light의 생성 함수가 존재한다. 기본 실험장은 Directional Light 하나와 필요할 경우 Sky Light를 사용하고, 노출·환경광을 조정한다. 엔진에 Deferred/Forward 계열 프로필, IBL, AO, CSM 관련 코드가 존재하나 각 backend/profile의 모든 조합이 동일한 결과를 제공한다고 간주하지 않는다.

## 고급 렌더 기능 탐색

아래는 해당 커밋 파일 목록에서 확인한 기능군이다. 구체적인 메뉴/토글 및 실제 렌더 성공은 추가 검증 대상이다.

| 기능군 | 관련 코드 | 제작 시 확인할 것 |
|---|---|---|
| 안개·대기 | FogComponent, SkyAtmosphereComponent | 카메라, 광원, 밀도 및 색 |
| 구름 | VolumetricCloudComponent | 노이즈/렌더 프로필/성능 |
| 후처리 | BloomEffect, DepthOfFieldEffect, AutoExposureEffect, OutlineEffect | 활성화, 적용 순서, 카메라/씬 설정 |
| 데칼 | DecalComponent | 표면 투영 및 머티리얼 |
| GI | ProbeGIVolumeComponent | 프로브 범위 및 지원 프로필 |
| 시간 누적 | TemporalAA, TemporalHistory | 카메라 이동 및 잔상 |
| 레이 트레이싱 | RaytracedShadowSettings, DirectX12AccelerationStructure | GPU 기능과 backend별 지원 |
| RHI | DirectX12RHI, SDLGPURHI, VulkanRHI, MetalRHI | 구현/플랫폼별 기능 차이, 실제 빌드 |

RHI 클래스 파일의 존재는 해당 백엔드가 완성되었음을 의미하지 않는다. 현재 GPU 및 플랫폼에서 지원되는 렌더러부터 사용한다. Metrics와 렌더 타겟 디버그 뷰를 통해 비용과 중간 결과를 확인한다.

## 관련 문서 및 소스

- [전체 근거 및 검토 범위](Sources.md)

