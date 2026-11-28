# API 사용성 및 결함 감사 (정적 분석)

[API 목차](README.md)

기준: `770e3adb81e3a4e9ea692996df09942e74587f28` · 2026-11-28 KST. 실제 재현/빌드 미수행. P0는 사용 전 안전성 확인 우선, P1은 기능 작업 전 또는 병행, P2는 개선 후보. 심각도는 작성자 제안이며 사용자 승인 전 공식 우선순위가 아니다. “코드상 결함 경로”는 정적 코드에서 위험한 경로를 직접 확인한 의미로, 실행 중 발생을 입증하지는 않았다. “검증 필요”는 미확인 위험/요구다.

| ID | 우선순위 | 분류 | 항목 |
|---|---|---|---|
| [A01](#a01) | P0 | 코드상 결함 경로 | 리소스 타입 혼동 |
| [A02](#a02) | P0 | 코드상 결함 경로 | EventBus 발행 중 Reset 수명 |
| [A03](#a03) | P1 | 코드상 결함 및 사용성 | Timer 생성·0 FPS·스텝 정책 |
| [A04](#a04) | P1 | 코드상 결함 경로 | 외부 모듈 Debug/Release 혼합 |
| [A05](#a05) | P2 | 코드상 미구현 | 프로젝트 템플릿 미반영 |
| [A06](#a06) | P1 | API 구조상 제약 | Native Script 영속 바인딩 |
| [A07](#a07) | P1 | API 구조상 제약 | 물리 전역 콜백 개별 해제 부재 |
| [A08](#a08) | P2 | 비용 분석 | MulticastDelegate 발행 비용 |
| [A09](#a09) | P2 | API 구조상 제약 | 오토타일 규칙/연결 그룹 |
| [A10](#a10) | P2 | API 사용성 | FMOD raw 포인터 결합 |
| [A11](#a11) | P1 | 검증 필요 | 씬별 내비게이션 데이터 정리 |
| [A12](#a12) | P1 | 검증 필요 | 네트워크 실행 및 권한 경계 |
| [A13](#a13) | P2 | API 사용성 후보 | 이름 기반 애니메이션 참조 |
| [A14](#a14) | P1 | 이전 사용자 보고 / 재검증 | 드롭다운 중첩 및 프리팹 열기 |
| [A15](#a15) | P2 | 추가 설계 검토 | VFX gameplay 제어 경계 |
| [A16](#a16) | P2 | 계약 확인 필요 | Hitstop의 시간 경계 |
| [A17](#a17) | P2 | API 사용성 후보 | 스프라이트 선택 영역 슬라이스 |

<a id="a01"></a>
## A01 — 리소스 타입 혼동
- 우선순위: P0 · 코드상 결함 경로
- 근거: [Engine/Source/Resource/Public/ResourceManager.inl](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Resource/Public/ResourceManager.inl) · [API 문서](Resource/ResourceManagement.md)
- 관찰: LoadResource<T>의 cache hit 및 GetResourceHandle<T>는 key hash로 찾은 handle을 타입 확인 없이 반환한다. ResourceHandle<T>::Get()은 static_cast<T*>를 사용한다.
- 재현/검증 방법: 같은 키에 Texture 등록 후 AudioClip으로 조회; 역참조 전에 실패하는지 검사. 실제 crash를 실행하지 않았다.
- 개선 제안: 슬롯 타입 ID 및 상속 호환 검사, 안전한 typed resolve/TryGet; 잘못된 타입은 invalid handle과 진단으로 반환. 캐시 키 충돌도 별도 검증.

<a id="a02"></a>
## A02 — EventBus 발행 중 Reset 수명
- 우선순위: P0 · 코드상 결함 경로
- 근거: [Engine/Source/Core/Event/Public/EventBus.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Event/Public/EventBus.h) · [API 문서](Core/Event.md)
- 관찰: Publish는 storage 소유권을 유지하지 않은 채 Broadcast 호출. callback에서 Reset하면 unique_ptr storage가 삭제되고 Broadcast가 m_Listeners에 다시 접근할 수 있다. Delegate.h의 snapshot은 callable만 보호한다.
- 재현/검증 방법: 리스너 2개 등록, 첫 콜백에서 bus.Reset(); ASan/디버거에서 UAF 여부 검사. 소유자 자체 파괴도 별도 검사.
- 개선 제안: 발행 중 reset 지연, storage shared ownership 또는 dispatch-depth 가드와 reset 정책. 재진입/자기 삭제 테스트.

<a id="a03"></a>
## A03 — Timer 생성·0 FPS·스텝 정책
- 우선순위: P1 · 코드상 결함 및 사용성
- 근거: [Engine/Source/Time/Public/Timer.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Time/Public/Timer.h) · [API 문서](Core/RuntimeTime.md)
- 관찰: 생성자와 소멸자가 private이고 자체 public 생성 함수가 없다. SetTargetFPS(0)은 1.0f/0 연산. Update는 누적 시간이 여러 간격이어도 한 번만 소비.
- 재현/검증 방법: 외부 모듈 생성 가능 여부 확인, FPS=0 및 NaN/음수 dt, 큰 dt 입력 테스트.
- 개선 제안: Timer를 명시적 생성 가능한 값 타입 또는 팩토리로 정리. 유효한 interval/Hz 설정 및 bool/소비 횟수 계약 명시.

<a id="a04"></a>
## A04 — 외부 모듈 Debug/Release 혼합
- 우선순위: P1 · 코드상 결함 경로
- 근거: [Engine/CMake/BamEngineConfig.cmake](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/CMake/BamEngineConfig.cmake) · [API 문서](Core/ProjectModule.md)
- 관찰: 자동 build dir은 Ninja Debug 우선. Engine.lib 후보에서 $<CONFIG>는 제외하며 Debug, Release 순 탐색. IMPORTED_IMPLIB/LOCATION은 구성별이 아닌 단일 속성이다. 같은 Binaries/Win64에 Debug/Release DLL 출력.
- 재현/검증 방법: 양쪽 Engine 구성 준비 후 Release 모듈 configure, 실제 link 경로 및 CRT 확인.
- 개선 제안: IMPORTED_CONFIGURATIONS 및 IMPORTED_IMPLIB_DEBUG/RELEASE, IMPORTED_LOCATION_* 명시. config 별 출력 및 일치 검증.

<a id="a05"></a>
## A05 — 프로젝트 템플릿 미반영
- 우선순위: P2 · 코드상 미구현
- 근거: [Engine/Source/Core/Path/Private/ProjectDescriptor.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Private/ProjectDescriptor.cpp) · [API 문서](Core/ProjectModule.md)
- 관찰: ProjectHub에는 Empty/2D/3D 콤보가 있으나 ScaffoldNewProject에서 (void)projectTemplate; 이후 동일 빈 Main 씬을 기록한다.
- 재현/검증 방법: 템플릿별 새 프로젝트 생성 후 초기 씬 비교.
- 개선 제안: 템플릿별 startup camera/light/2D 설정 구현 또는 UI 선택을 비활성화하고 미지원 표시.

<a id="a06"></a>
## A06 — Native Script 영속 바인딩
- 우선순위: P1 · API 구조상 제약
- 근거: [Engine/Source/World/Entity/Public/NativeScriptComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/NativeScriptComponent.h) · [API 문서](Core/NativeScript.md)
- 관찰: Instance unique_ptr와 InstantiateScript std::function만 확인되며 저장 가능한 script type ID/이름 필드가 없다. Bind<T>는 C++ factory를 연결한다.
- 재현/검증 방법: 사용자 스크립트 바인딩 후 저장/재열기, PIE, module reload, Client에서 재바인딩 확인.
- 개선 제안: 타입 ID 기반 ScriptRegistry/Factory 등록, 직렬화 가능한 binding, 모듈 수명과 연동. 자동 저장 복원 여부를 명문화.

<a id="a07"></a>
## A07 — 물리 전역 콜백 개별 해제 부재
- 우선순위: P1 · API 구조상 제약
- 근거: [Engine/Source/Physics/Public/PhysicsSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/PhysicsSystem.h) · [API 문서](Physics/Physics.md)
- 관찰: RegisterGlobalTriggerCallback/CollisionCallback은 void이고 ClearGlobal*Callbacks만 선언. 개별 등록의 소유권과 해제 API가 드러나지 않는다.
- 재현/검증 방법: 두 모듈이 등록한 뒤 하나를 unload하여 상대 구독을 보존하는지 확인.
- 개선 제안: 구독 token/RAII, owner module별 정리, publish 중 삭제/씬 교체 테스트.

<a id="a08"></a>
## A08 — MulticastDelegate 발행 비용
- 우선순위: P2 · 비용 분석
- 근거: [Engine/Source/Core/Event/Public/Delegate.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Event/Public/Delegate.h) · [API 문서](Core/Event.md)
- 관찰: Broadcast는 vector snapshot 복사 후 각 listener의 연결 여부를 전체 목록 선형 검사한다. N개 listener 기준 최악 O(N²).
- 재현/검증 방법: 1/100/1000 listener, 60Hz 및 빈번한 remove 상황에서 프로파일링.
- 개선 제안: 유효 token hash/index나 generation 기반 검사를 고려. 재진입 동작 보존 후 측정.

<a id="a09"></a>
## A09 — 오토타일 규칙/연결 그룹
- 우선순위: P2 · API 구조상 제약
- 근거: [Engine/Source/Tilemap/Public/TilemapComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Tilemap/Public/TilemapComponent.h) · [API 문서](Gameplay/SpriteTilemap.md)
- 관찰: isAutotile boolean 및 단일 autotileSprites 배열; 이웃도 isAutotile이면 같은 그룹으로 연결. mask0 필수. 다중 지형 규칙 그룹을 같은 Tilemap에 섞는 모델은 이 선언에서 확인되지 않음.
- 재현/검증 방법: 물/땅 등 다른 규칙을 같은 레이어에 섞고 경계, 빈 mask, undo/reopen 확인.
- 개선 제안: 룰셋 리소스 또는 group ID, rule validation, mask 시각화, fallback 명문화. 기존 저장 마이그레이션.

<a id="a10"></a>
## A10 — FMOD raw 포인터 결합
- 우선순위: P2 · API 사용성
- 근거: [Engine/Source/Audio/Public/AudioManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Audio/Public/AudioManager.h) · [API 문서](Audio/Audio.md)
- 관찰: PlayOneShot/3D/BGM/Crossfade는 FMOD::Sound*를 요구하고 EResult만 반환한다. handle 기반 재생, 개별 재생 종료 제어는 이 경로에서 드러나지 않음.
- 재현/검증 방법: AudioClip 소유 해제 후 재생 종료, 반복/scene 전환 및 backend 분리 테스트.
- 개선 제안: ResourceHandle<AudioClip> overload, playback token, 실제 channel lifetime 계약을 추가.

<a id="a11"></a>
## A11 — 씬별 내비게이션 데이터 정리
- 우선순위: P1 · 검증 필요
- 근거: [Engine/Source/Navigation/Public/NavigationSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Navigation/Public/NavigationSystem.h) · [API 문서](Navigation/Navigation.md)
- 관찰: Set/Get/RemoveSceneNavGrid/Mesh는 Scene*를 key로 받는다. 실제 unload hook과 주소 재사용 방어는 아직 확인하지 못했다.
- 재현/검증 방법: A/B scene 교체, 반복 PIE, 동일 주소 재사용 시 nav data 격리 확인.
- 개선 제안: 씬 ID/소유권 또는 destruction hook으로 보장; 기존 hook이 있으면 문서와 테스트 추가.

<a id="a12"></a>
## A12 — 네트워크 실행 및 권한 경계
- 우선순위: P1 · 검증 필요
- 근거: [Engine/Source/Network/Public/NetworkSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Network/Public/NetworkSystem.h) · [API 문서](Network/Network.md)
- 관찰: GetDriver가 m_Driver를 직접 역참조하고 FindEntityByNetId는 entt::entity 반환. 초기화/씬 소속/권한 계약을 추가 확인해야 함.
- 재현/검증 방법: 초기화 실패, stale ID, 다중 scene, invalid RPC, reconnect, WAN/payload fuzz 테스트.
- 개선 제안: 유효 상태 조회, scene-aware identity, authority 정책, 실패 진단 명문화. 보안 기능 완료 주장 금지.

<a id="a13"></a>
## A13 — 이름 기반 애니메이션 참조
- 우선순위: P2 · API 사용성 후보
- 근거: [Engine/Source/Animation/Public/AnimatorComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Animation/Public/AnimatorComponent.h) · [API 문서](Animation/AnimatorTimeline.md)
- 관찰: SetFloat/Int/Bool/Trigger는 string 이름을 받고 void 반환. 호출자에게 missing/type mismatch 결과가 직접 전달되지 않는다. 내부 logging 여부 미확인.
- 재현/검증 방법: 없는 parameter, 잘못된 type, controller reload/rename에서 진단 확인.
- 개선 제안: TrySet 계열 또는 parameter handle, 타입 검사 및 에디터 validation.

<a id="a14"></a>
## A14 — 드롭다운 중첩 및 프리팹 열기
- 우선순위: P1 · 이전 사용자 보고 / 재검증
- 근거: [Engine/Source/UI/Public/UIDropdownComponent.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/UI/Public/UIDropdownComponent.h) · [API 문서](UI/UI.md)
- 관찰: 이전 대화의 텍스트 중첩·프리팹 미열림 보고. 최신 커밋에서 같은 오류가 남았는지는 조사하지 못했다.
- 재현/검증 방법: 문제 prefab을 최신 main에서 열고 Console/로그/옵션별 clipping, z-order, layout 및 재열기 확인.
- 개선 제안: 재현 후 원인에 따라 layout 수정 또는 오류 reporting; 해결 여부 확인 전 결함 단정 금지.

<a id="a15"></a>
## A15 — VFX gameplay 제어 경계
- 우선순위: P2 · 추가 설계 검토
- 근거: [Engine/Source/VFX/Public/ParticleSystem.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/VFX/Public/ParticleSystem.h) · [API 문서](VFX/Particle.md)
- 관찰: public 시스템 helper는 simulation/update/submit/spawn 단위. game-facing 통합 재생/종료 API는 이번 조사에서 확인하지 못했다.
- 재현/검증 방법: 게임 모듈에서 effect 1회 재생/loop/정지/완료/풀 반환 API 찾고 최소 예제 작성.
- 개선 제안: 이미 있다면 문서화, 없으면 공통 VFX playback facade/완료 이벤트 설계.

<a id="a16"></a>
## A16 — Hitstop의 시간 경계
- 우선순위: P2 · 계약 확인 필요
- 근거: [Engine/Source/Runtime/Private/Runtime.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Runtime/Private/Runtime.cpp) · [API 문서](Core/RuntimeTime.md)
- 관찰: Update는 CombatEffectService::Update(dt)로 scaledDt를 시스템에 전달하나 FixedUpdate 및 LateUpdate에는 이 함수의 스케일 적용이 없다. 외부 루프 스케일 여부는 미확인.
- 재현/검증 방법: hitstop 중 rigid body, animation, timeline, audio, late camera의 시간 추적.
- 개선 제안: 의도된 도메인별 시간 정책 명문화, 필요 시 공통 scaled/unscaled clock.

<a id="a17"></a>
## A17 — 스프라이트 선택 영역 슬라이스
- 우선순위: P2 · API 사용성 후보
- 근거: [Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp) · [API 문서](Gameplay/SpriteTilemap.md)
- 관찰: SpriteEditor의 slicing modal에는 grid by size/count 및 automatic이 확인되지만 선택 영역 제한 경로는 이번 검색에서 확인하지 못했다. 개별 region 편집은 존재.
- 재현/검증 방법: 선택 사각형 내부만 grid/auto slicing 및 기존 외부 slice 유지 여부 확인.
- 개선 제안: 지원되지 않으면 region-limited slicing 및 기존 slice merge/replace 옵션 제공.

## 권장 작업 순서
1. A01/A02 안전성 회귀 테스트 및 수정.
2. A04/A06/A07 외부 프로젝트와 모듈 생명주기.
3. A03 시간 API, A11/A12/A14 실제 환경 확인.
4. 나머지 사용성 항목은 게임 제작을 통해 필요성을 검증.
5. 각 수정과 함께 API 문서·테스트·기존 UserGuide 갱신.

이 문서는 코드 변경을 수행하지 않는다. 기존 개발 계획에 중복 이슈가 있다면 새 작업을 생성하지 말고 기존 항목과 통합한다.
