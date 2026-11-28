# 파티클 및 VFX

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

Content Browser의 Create Effect 및 ParticleEditorPanel/VFXEditorPanel 파일이 확인된다. ParticleEmitterComponent, ParticleEffectComponent, CompositeVFXComponent, TrailComponent를 이용하는 제작 경로를 탐색할 수 있다.

권장 실습: 작은 파티클 효과 에셋 하나를 만들고, 수명/방출량/색/크기/속도 등 실제 UI가 제공하는 설정을 조절한 다음 씬 엔티티에 연결한다. 단일 효과 검증 후 다중 효과 및 타임라인을 합친다. 효과 편집 후 저장, 재열기 및 Client 출력까지 확인한다.

## 관련 문서 및 소스

- [전체 근거 및 검토 범위](Sources.md)

