# Particle / Composite VFX / Trail

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 사용 모델
ParticleEmitterComponent는 개별 emitter, ParticleEffectComponent는 effect asset 기반, CompositeVFXComponent는 복합 이펙트, TrailComponent는 이동 흔적의 구성 경계다. 에디터에서 에셋과 컴포넌트를 준비하고 게임에서는 활성/비활성, 위치 및 프로젝트 이벤트를 연결한다. 필드별 제어 API는 각 헤더 본문을 추가 확인한다.

`ParticleSystem::SimulateSceneParticles(Scene*,dt)`, `SubmitSceneParticles(Scene*,const CameraBuffer&,uint32 targetPassId)`, `UpdateEffectComponent(scene,effectComp,worldTransform,dt)`, `UpdateEmitter(scene,emitter,worldTransform,dt)`, `SpawnParticle(emitter,worldTransform)` 등의 공개 선언은 시스템 및 독립 preview 경계이다. 일반 게임 프레임에서 직접 추가 호출하면 중복 시뮬레이션/제출 위험이 있다. Presentation phase로 등록되는 시스템을 기본 사용한다.

## 예제 흐름 (개념; 컴파일 코드 아님)
1. 에셋 에디터에서 emitter/effect 정의 및 material/texture 연결.
2. 씬에 이펙트 엔티티 생성, effect component와 위치 연결.
3. 공격 hit/notify에서 원하는 효과 실행 또는 prefab 생성.
4. 완료 후 반환/삭제; persistent callback과 리소스 정리.

## 검증 및 제한
GPU particle simulation, 대규모 emitter pooling, 모든 blend mode의 동일 성능은 이번 조사로 확정하지 않았다. burst, looping, pause, transform local/world, transparent sort, camera 변경, PIE 반복을 확인한다. game-facing Play/Stop/Spawn API와 완료 알림 통합은 추가 감사 후보(A15).

## 관련 소스
- [ParticleSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/VFX/Public/ParticleSystem.h) — `Engine/Source/VFX/Public/ParticleSystem.h`
- [ParticleEffectComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/VFX/Public/ParticleEffectComponent.h) — `Engine/Source/VFX/Public/ParticleEffectComponent.h`
- [ParticleEmitterComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/VFX/Public/ParticleEmitterComponent.h) — `Engine/Source/VFX/Public/ParticleEmitterComponent.h`
- [CompositeVFXComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/VFX/Public/CompositeVFXComponent.h) — `Engine/Source/VFX/Public/CompositeVFXComponent.h`
- [TrailComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/VFX/Public/TrailComponent.h) — `Engine/Source/VFX/Public/TrailComponent.h`

## 검증 및 참고 예제
- [VFXBlendAndPassCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/VFXBlendAndPassCheck.cpp)
