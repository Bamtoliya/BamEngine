# 애니메이션

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

**파일 확인:** AnimatorComponent, AnimationSystem, SkinnedMeshRendererComponent, BlendTree1D/2D, BoneMask, RootMotion, AnimationIK, AnimatorGraphPanel, AnimationTimelinePanel.

권장 제작 절차:

- 스켈레톤이 포함된 모델과 애니메이션 클립을 임포트한다. 본 이름, 계층, 스케일이 호환되는지 확인한다.
- SkinnedMeshRenderer와 Animator를 연결하고 단일 Idle 클립부터 미리보기한다.
- Animator Graph에서 Idle/Walk/Run/Jump의 상태와 전환을 구성한다(실제 패널의 지원 설정을 확인).
- 이동 속도 및 지면 상태를 그래프 또는 C++에서 연동한다.
- 이동의 책임을 Root Motion 또는 Controller 중 하나에 명확히 두어 이중 이동을 방지한다.
- 공격 모션의 이벤트/타임라인은 실제 지원되는 Action Timeline 기능을 확인 후 연결한다.

관련 헤더는 존재하지만 리타게팅, IK 편집 UI, 모든 Blend Tree 동작의 완성은 이번 검토에서 판정하지 않았다.

2D 애니메이션 클립 생성은 [2D 게임 제작](GameDevelopment2D.md)을 참고한다.

## 관련 문서 및 소스

- [전체 근거 및 검토 범위](Sources.md)

