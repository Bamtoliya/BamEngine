# BamEngine API Reference

기준: [`770e3adb`](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28) · 2026-11-28 KST.

게임 개발자 및 AI 에이전트가 C++ 코드에서 엔진을 사용하는 방법을 설명한다. 기존 `Docs/UserGuide`(에디터 사용법), `Docs/Technical`(내부 구조)와 분리한다.

## 읽는 방법
- **Preview**: 현재 선언/경로가 존재하는 API. 호환성 보장은 아직 없음.
- **Experimental**: 실험/확장 단계; 실환경 검증 필요.
- **Internal**: public 헤더라도 게임에서 직접 호출하기보다 엔진 소유 경계를 따를 것.
- 등급은 문서 작성자 제안이며 공식 Stable 등급을 부여하지 않았다.
- 선언, 구현, 검사 코드 존재, 검사 실행 결과를 구별한다. 이번 빌드/실행은 하지 않았다.
- C++ 조각은 호출 문맥과 소유권을 설명하기 위한 것으로 독립 실행 예제나 컴파일 검증 완료를 의미하지 않는다.

## 시스템별 목차

| 시스템 | 안정성 제안 |
|---|---|
| [Entity / Scene / Hierarchy](Core/EntityScene.md) | Preview |
| [Transform / 좌표계](Core/Transform.md) | Preview |
| [Reflection / Serialization](Core/ReflectionArchive.md) | Preview |
| [Runtime / System / Time](Core/RuntimeTime.md) | Internal / Preview |
| [EventBus / Delegate](Core/Event.md) | Preview |
| [Project / Virtual Path / Module](Core/ProjectModule.md) | Preview |
| [Native Script](Core/NativeScript.md) | Preview |
| [ResourceManager / ResourceHandle](Resource/ResourceManagement.md) | Preview |
| [InputManager / Input Actions](Input/InputActions.md) | Preview |
| [2D / 3D Physics](Physics/Physics.md) | Preview |
| [Renderer / RHI / Frame submission](Rendering/RendererRHI.md) | Internal / Preview |
| [Material / Shader / Graph](Rendering/MaterialShader.md) | Preview |
| [Animator / Action Timeline / Socket](Animation/AnimatorTimeline.md) | Preview |
| [Sprite / Sprite Animation / Tilemap](Gameplay/SpriteTilemap.md) | Preview |
| [Graph / Visual Scripting](Graph/VisualScripting.md) | Preview |
| [Canvas / UIEventSystem / Widgets](UI/UI.md) | Preview |
| [AudioClip / AudioManager / Mixer](Audio/Audio.md) | Preview |
| [Particle / Composite VFX / Trail](VFX/Particle.md) | Preview |
| [Terrain / Spline](World/TerrainSpline.md) | Preview |
| [SaveGame / DataTable / Gameplay](Gameplay/SaveData.md) | Preview |
| [Navigation / AI](Navigation/Navigation.md) | Preview |
| [Network / RPC / Replication](Network/Network.md) | Experimental |
| [Reinforcement Learning / Simulation](Simulation/RL.md) | Experimental |
| [Editor Plugin / Host Services](Editor/Plugin.md) | Experimental / Internal |

## 관리 문서
- [API 감사 및 문제점](APIAudit.md) — 관찰 근거, 재현 조건, 개선 제안.
- [유지보수 및 검증 규칙](Maintenance.md) — API 변경과 함께 문서 갱신.

## 추가 조사 범위
이번에는 주요 클래스의 공개 선언 및 일부 핵심 구현을 선별했다. 모든 overload 및 전체 헤더, 렌더 패스별 설정, 2D 물리의 모든 query, VFX/terrain/spline/플러그인 상세 ABI, gameplay helper, 플랫폼별 RHI 지원 표는 후속 보강 대상이다. API 목록의 완전성을 주장하지 않는다.
