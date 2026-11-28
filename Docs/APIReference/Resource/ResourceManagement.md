# ResourceManager / ResourceHandle

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 리소스 로딩과 소유권
`ResourceManager::LoadResource<T>(...)`는 첫 인자로 wide key/path 또는 `ResourceCreateDesc` 파생 descriptor 포인터를 사용한다. 구현은 key 정규화/해시 조회 → URI 해석 → Pak/JSON/BEVE archive 분기 → source Create 순서다. 첫 인자를 생략하거나 타입을 추측하지 않는다.
`GetResourceHandle<T>(key)`는 이미 등록된 키를 찾는 함수이며 파일을 로드하는 함수가 아니다. `AddResource<T>(key, resource)`는 직접 생성한 리소스의 등록 경로다. 중복 등록·소유권 이전의 구체적 결과는 AddResourceInternal 구현과 테스트를 추가 확인해야 한다.

```cpp
#include "ResourceManager.h"
#include "ResourceHandle.h"
#include "AudioClip.h"
void InspectLoadedClip(const std::wstring& key) {
    auto clip = Engine::ResourceManager::Get().GetResourceHandle<Engine::AudioClip>(key);
    if (clip && clip.Get()) {
        const auto seconds = clip->GetDurationSeconds();
        (void)seconds;
    }
}
```
전제: key가 실제 AudioClip에 대응한다. 현재 구현은 잘못된 타입을 방어하지 못할 수 있다(A01).

## 핸들 계약
복사 시 AddRef, 이동 시 원본 핸들 초기화, 소멸/Reset 시 Release. `IsValid()`는 슬롯 유효성과 접근 계층을 확인한다. `Get()`은 raw 포인터를 반환하므로 핸들보다 오래 보관하지 않는다. 프로젝트 범위 정리(`UnloadProjectScopedResources`) 이후 핸들을 재확인한다. `IsOwnerThread()`를 제공하므로 등록, 파괴, GPU 업로드를 무조건 작업 스레드에서 호출하지 않는다. 작업 큐와 resource streamer 경계를 따른다.

## 중요 개선
아래 타입 혼동 지적은 기준 커밋에 대한 기록이다. 2026-11-28 변경에서는 `TryAddRefResource`가 핸들 슬롯과 타입 검증기를 확인한 뒤 참조를 추가한다. `Engine/examples/ResourceHandleCheck.cpp`에 관련 검증 코드가 있다.

`GetResourceHandle<T>` 및 cache-hit에서 T 검증 없이 생성하고 `Get()`이 static_cast한다. 동일 key를 다른 T로 조회하면 타입 혼동 위험이 있다. 실제 재현은 독립 테스트로 수행하고 타입 ID 검사/안전한 변환 API를 추가한다(A01). Pak 의존성만으로 동적 로딩 리소스가 자동 포함된다고 가정하지 않는다.

## 관련 소스
- [ResourceManager.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Resource/Public/ResourceManager.h) — `Engine/Source/Resource/Public/ResourceManager.h`
- [ResourceHandle.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Resource/Public/ResourceHandle.h) — `Engine/Source/Resource/Public/ResourceHandle.h`
- [ResourceManager.inl](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Resource/Public/ResourceManager.inl) — `Engine/Source/Resource/Public/ResourceManager.inl`
- [ResourceHandle.inl](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Resource/Public/ResourceHandle.inl) — `Engine/Source/Resource/Public/ResourceHandle.inl`

## 검증 및 참고 예제
- [ResourcePreloaderCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/ResourcePreloaderCheck.cpp)
- [StreamingPolicyCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/StreamingPolicyCheck.cpp)
- [PakArchiveCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/PakArchiveCheck.cpp)
