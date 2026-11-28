# 물리와 충돌

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 3D 물리 설정

3D `ColliderComponent`의 지원 형태는 Box/Sphere/Capsule/Mesh다. Capsule 높이는 양끝 반구를 포함하며 지름 이상이어야 한다. `RigidBodyComponent`에는 질량, 댐핑, 중력 배율, 회전 고정, CCD, 선속도, 각속도 설정이 있다. 2D 전용 컴포넌트와 혼용하지 않는다. 외부 PhysX SDK가 이 물리 기능의 필수라는 가정을 하지 않는다.

## 실습 및 확인

[3D 게임 제작](GameDevelopment3D.md)의 낙하 실험과 [2D 게임 제작](GameDevelopment2D.md)의 플랫폼 실습을 사용한다.

- Static: 고정 환경에 사용한다. Dynamic: 중력 및 물리 반응을 확인한다.
- Kinematic의 세부 이동 및 접촉 규칙, Joint, 물리 재질, 쿼리, 레이어 설정은 별도 최신 코드 검토 후 확장한다.
- 빠른 물체는 CCD 설정과 고정 스텝을 함께 검증한다.
- Collider 크기 및 오프셋, 회전 고정, 저장 후 복원, PIE 종료 후 상태 복원을 확인한다.

위 내용은 확인된 API 및 실습 제안이며 물리 엔진 전체 기능의 검증 결과는 아니다.

## 관련 문서 및 소스

- [`RigidBodyComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/RigidBodyComponent.h) — `Engine/Source/Physics/Public/RigidBodyComponent.h`
- [`ColliderComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/ColliderComponent.h) — `Engine/Source/Physics/Public/ColliderComponent.h`
- [전체 근거 및 검토 범위](Sources.md)

