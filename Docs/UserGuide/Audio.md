# 오디오

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

**파일 확인 단계.** AudioSourceComponent, AudioListenerComponent, AudioMixer 등의 헤더를 확인했다. 구체적인 UI와 파일 형식, 재생 API는 추가 소스 조사 및 실행 확인이 필요하다.

## 권장 검증 순서 (제안)

1. [시작하기](GettingStarted.md)의 FMOD SDK 의존성을 준비한다.
2. 테스트 씬에서 AudioSourceComponent와 AudioListenerComponent의 등록 및 설정을 확인한다.
3. 짧은 효과음 재생부터 시작하여 반복 BGM, 위치 기반 재생, 볼륨 제어를 각각 검증한다.
4. PIE 반복 시작·종료 시 이전 음원이 남지 않는지 확인한다.
5. 독립 Client에서도 동일 리소스와 FMOD 런타임을 확인한다.

메뉴 이름이나 지원 코덱 및 믹서 편집 기능을 확정한 문서가 아니다.

## 관련 문서 및 소스

- [전체 근거 및 검토 범위](Sources.md)

