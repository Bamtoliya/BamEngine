# 프리팹 제작·배치·씬 저장 흐름 점검 및 개선 계획

- 작성일 2026-11-19
- 상태 · 현재 소스와 기존 검사 범위를 대조한 정적 점검. 개선 적용 및 실환경 검증은 대기 중이다.
- 적용 범위 · 모델과 설정한 엔티티 계층에서 프리팹 만들기, 배치, 원본 편집, Apply/Revert/Unpack, 씬 저장·재열기, PIE 복원.

## 1. 평가와 현재 동작

**프리팹 파일과 씬의 저장·배치 기반은 갖춰져 있다. 참조가 있는 캐릭터를 반복 사용하려면 `Revert All`의 객체 수명과 씬·프리팹의 엔티티 참조 직렬화를 먼저 고쳐야 한다.** 현재 구조는 프리팹에서 엔티티를 복제한 뒤 씬에 편집 가능한 사본으로 보관하고, 루트에 원본 경로를 붙이는 방식이다. 이 저장 방식을 기준으로 편집 명령과 표시를 맞춘다.

1. **만들기.** Hierarchy의 `Save As Prefab`은 선택 루트의 하위 트리와 등록 컴포넌트를 파일로 쓴다. 씬의 선택 루트는 기존 상태를 유지한다. Archive는 저장할 하위 트리에 로컬 ID를 부여하고 리소스 참조 목록을 기록한다. [HierarchyPanel](../Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp) · [SceneArchive](../Engine/Source/World/Scene/Private/SceneArchive.cpp)
2. **배치.** 콘텐츠 브라우저·씬 뷰·Hierarchy의 배치는 `SceneManager::InstantiatePrefab()`을 호출한다. Archive는 임시 씬에서 계층과 컴포넌트를 검사하고, 대상 씬에 새 ID로 엔티티를 생성한 다음 부모·자식 관계를 복원한다. 복사나 계층 연결에 실패하면 생성한 엔티티를 되돌린다. 배치가 성공하면 루트에 `PrefabInstanceComponent::prefabAssetPath`를 기록한다. [SceneManager](../Engine/Source/World/Scene/Private/SceneManager.cpp) · [SceneArchive](../Engine/Source/World/Scene/Private/SceneArchive.cpp)
3. **편집.** 콘텐츠 브라우저의 프리팹 열기는 `PrefabModeManager`로 전환하고, `PrefabEditorPanel`은 별도 씬과 로컬 이력을 사용한다. 인스턴스 속성 편집은 씬의 편집 이력에 들어간다. 원본 저장과 인스턴스 편집은 서로 다른 문서 변경이다. [열기 진입점](../Editor/Source/ImGui/Private/AssetFileActions.cpp) · [PrefabModeManager](../Editor/Source/ImGui/PrefabEditor/Private/PrefabModeManager.cpp) · [PrefabEditorPanel](../Editor/Source/ImGui/PrefabEditor/Private/PrefabEditorPanel.cpp)
4. **저장·재열기.** 씬은 각 프리팹 인스턴스의 엔티티와 컴포넌트 전체, 그리고 루트의 원본 경로를 저장한다. 재열기는 저장된 씬 내용으로 복원한다. A를 수정해 `Apply All`하면 원본 파일이 바뀌고 새로 배치한 C는 새 원본을 사용한다. 기존 B는 편집 중이나 씬 재열기 후에도 자체 저장값을 유지한다. B에서 `Revert All`을 해야 새 원본으로 다시 생성된다. [PrefabInstanceComponent](../Engine/Source/World/Scene/Public/PrefabInstanceComponent.h) · [인스펙터의 프리팹 명령](../Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp) · [SceneArchive](../Engine/Source/World/Scene/Private/SceneArchive.cpp)

## 2. 결함과 개선 순서

### 2.1 `Revert All`의 삭제와 그리기 순서

**심각 · 삭제된 엔티티 접근.** `InspectorPanel::Draw()`는 선택 엔티티 포인터를 보관한 채 `DrawPrefabHeaderBar()`를 호출한다. `Revert All`에 성공하면 헤더에서 기존 루트와 자식을 삭제한다. 헤더가 돌아오면 `DrawModelMaterialRefresh()`와 본문 그리기가 같은 포인터를 계속 사용한다. 루트나 자식을 선택한 상태로 되돌리기가 성공하면 해제된 객체에 접근할 수 있다. [InspectorPanel.cpp 115행](../Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp) · [Scene::DestroyEntity](../Engine/Source/World/Scene/Private/Scene.cpp)

**높음 · 교체 실패와 이력 누락.** 현재 동작은 새 서브트리를 먼저 배치하고, 기존 루트의 위치·회전·스케일을 복사한 뒤 삭제 결과를 무시한다. 기존 서브트리 삭제가 거부되면 새 인스턴스가 남은 채 성공 알림이 표시될 수 있다. 성공 시에도 대상 서브트리를 가리키는 외부의 쓰기 가능한 직접 참조는 `DestroyEntity()`가 비우며, 형제 순서 보존과 씬 변경 이력 기록이 교체 명령에 연결돼 있지 않다. [Revert All](../Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp) · [삭제 검증과 참조 정리](../Engine/Source/World/Scene/Private/Scene.cpp)

- **첫 수정.** UI에서는 씬과 대상의 식별자를 담은 요청을 만들고, 대상 포인터를 사용하는 그리기가 끝난 뒤 실행한다. 처리 직전에 편집 가능 상태와 씬·대상을 다시 확인한다. 원본이 없거나 유효하지 않을 때 기존 서브트리를 보존하고 실패를 표시한다.
- **안전한 교체.** 기존 Archive의 임시 씬 검증을 사용한다. 부모, 형제 위치, 루트의 배치 Transform과 영향받는 참조를 교체 전에 확인한다. 삭제 실패까지 포함해 한 작업으로 복구할 수 있는 경계에 넣고, 성공한 교체만 `SceneEditHistory`에 한 건으로 기록한다. 참조를 새 노드에 대응시킬 수 없는 현재 단계에서는 참조가 걸린 교체를 차단하고 이유를 표시한다. 안정된 대응을 확보한 뒤에만 참조를 이전한다. [씬 편집 이력](EditorUndoRedo.md)
- **2026-11-19 반영 (정적 확인, 빌드·실행 대기).** 인스펙터는 메뉴에서 씬과 루트 ID만 담은 요청을 남기고, 창 그리기가 끝난 뒤 `InspectorPanel::ExecutePendingPrefabRevert()`에서 실행한다. 실행 직전에 현재 씬·편집 가능 상태(`SceneEditHistory::Enabled`)·루트와 원본 경로를 다시 확인한다. 트리 밖에서 트리 안을 가리키는 엔티티 참조가 있으면 `Scene::CountExternalEntityReferences()`로 세어 교체를 막고 개수를 알린다. 배치 실패 시 기존 트리를 유지하고, 기존 트리 삭제가 거부되면 새 트리를 지워 되돌린다(그마저 실패하면 교체 전 스냅샷 복원). 배치 Transform은 위치·회전만 유지하고 스케일은 원본 값으로 되돌린다. 부모가 있으면 새 루트를 기존 루트 자리에 두어 형제 순서를 유지하고, 성공한 교체만 씬 이력 한 건(`Prefab.RevertAll`)으로 기록한다. 잠긴 인스펙터는 새 루트로 잠금을 옮긴다. 최상위 엔티티는 엔진에 별도 순서가 없어 순서 보존 대상이 아니다. 내부 참조의 새 ID 대응은 2.2에서 다룬다. [구현 기록](ImplementationLog.md)
- [ ] **완료 기준.** 루트 선택, 하위 노드 선택, 잠긴 인스펙터에서 `Revert All`이 안전하게 끝난다. 부모·형제 위치와 배치 Transform이 보존된다. 원본 손상·누락과 삭제 거부에서 기존 트리·선택·참조·이력이 유지되고 새 트리가 남지 않는다. 성공 후 Undo/Redo 한 번씩으로 복원되며, 변경을 저장하기 전 씬의 미저장 표시가 켜진다.

### 2.2 프리팹 내부와 씬 엔티티 참조 복원

**높음 · 잘못된 엔티티 바인딩.** Archive의 `entt::entity` 코덱은 런타임 핸들을 정수로 저장한다. 프리팹 배치에서는 새 핸들을 발급하고 부모·자식만 로컬 ID에서 변환한다. `SpringArmComponent::TargetEntity`, `ThirdPersonControllerComponent::SpringArmEntity`, `CharacterControllerComponent::TargetAnimatorEntity` 같은 필드는 원래 숫자를 가진 채 복사된다. 두 번째 프리팹이 첫 번째 캐릭터나 다른 엔티티를 가리키거나, 참조가 무효가 될 수 있다. 씬 재열기 역시 새 레지스트리를 만들므로 저장 전후 핸들이 우연히 일치하는 데이터로는 이 문제를 판별하기 어렵다. [엔티티 코덱](../Engine/Source/Core/Reflection/Private/ReflectionRegistry.cpp) · [씬·프리팹 복원](../Engine/Source/World/Scene/Private/SceneArchive.cpp) · [Spring Arm](../Engine/Source/Camera/Public/SpringArmComponent.h)

- 씬은 이미 있는 `IDComponent::id`, 프리팹은 파일 내부 로컬 ID로 지원하는 엔티티 참조를 저장한다. 엔티티를 모두 생성한 뒤 참조를 해석한다. 씬과 프리팹 문서에만 해당하는 Archive 문맥에서 이 규칙을 적용하고, 에디터 Undo/Redo가 이미 별도로 보관하는 참조의 동작을 함께 확인한다. 공용 리플렉션 코덱의 다른 소비자도 보존한다. [Reflection·Archive 지침](ReflectionArchiveGuidelines.md) · [씬 편집 이력](../Editor/Source/Command/Private/SceneEditHistory.cpp)
- 프리팹에서 서브트리 밖을 가리키는 참조는 저장 전에 필드와 대상을 진단한다. 다른 씬에도 배치할 수 있도록, 사용자가 배치 후 재지정할 참조와 저장을 막아야 할 참조의 정책을 지원 범위에 맞춰 정한다. 기존 파일의 숫자 핸들은 의미가 달라질 수 있으므로 새 ID 표기와 구별하고, 올바른 대상을 검증할 수 없는 값은 수정 안내를 제공한다. 중첩 구조와 컨테이너 참조 지원은 실제 사용처를 확인하며 확장한다.
- [ ] **완료 기준.** 핸들이 서로 다른 대상 씬에 같은 캐릭터를 두 번 배치한다. A·B의 카메라, Spring Arm, Animator가 각자의 엔티티만 참조한다. 다른 엔티티를 만들었다 지운 뒤 저장·재열기와 PIE 종료를 반복해 결과를 비교한다. 이전 파일 호환, 프리팹 밖 참조 진단과 누락 ID 처리도 확인한다. [PIE 스냅샷 경로](../Editor/Source/PIE/Private/PIEManager.cpp)

### 2.3 `Apply All`과 `Unpack`의 편집 계약

**높음 · 원본 저장 범위와 원본 충돌.** `Apply All`은 현재 인스턴스의 서브트리 전체를 원본에 덮어쓴다. 직렬화 가능한 `PrefabInstanceComponent`도 일반 컴포넌트로 수집하므로 원본 파일에 자신의 연결 경로가 들어갈 수 있다. 루트의 씬 배치값도 함께 기록된다. 속성별 변경값과 원본 변경 여부를 비교하지 않으므로 오래된 B에서 실행하면 최신 원본을 B의 값으로 덮어쓸 수 있다. [Apply All](../Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp) · [컴포넌트 직렬화](../Engine/Source/World/Scene/Private/SceneArchive.cpp)

- 현재 저장 구조에 맞춰 동작명을 **인스턴스로 원본 덮어쓰기**로 명확히 하고 확인창에 대상 경로와 새 배치에 반영되는 범위를 보여준다. 씬 배치값을 원본 기본값에 반영하는 정책을 정한다. 기본 동작은 원본 루트 배치를 보존하는 쪽을 우선 검토한다. 프리팹 문서 작성 시 루트의 씬 인스턴스 연결 정보는 제외하며, 씬 저장에는 계속 보존한다.
- `SceneManager::SavePrefab()`, 프리팹 모드와 독립 편집기의 저장을 같은 저장 정책으로 묶는다. 공통 읽기 전용 검사, 동일 원본을 연 문서의 미저장 변경과 파일 변경 확인, 성공 시 썸네일 무효화를 적용한다. Archive가 이미 임시 파일 작성 후 이름 변경으로 게시하므로 그 경로를 재사용한다. 원본 파일 덮어쓰기는 씬의 Undo 범위와 구별해 확인창과 결과에 표시한다. [Archive 저장](../Projects/Archive/src/ArchiveInterface.cpp) · [읽기 전용 검사](../Engine/Source/World/Scene/Private/SceneManager.cpp)
- **보통 · `Unpack`의 진입점 차이.** Hierarchy 메뉴는 `SceneEditHistory`에 기록하고 인스펙터 메뉴는 루트 컴포넌트만 직접 제거한다. 두 진입점을 기존 씬 편집 명령 경로에 연결한다. 성공 시 대상의 프리팹 연결만 해제하며 계층·컴포넌트 값과 참조를 보존한다. [Hierarchy 메뉴](../Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp) · [Inspector 메뉴](../Editor/Source/ImGui/InspectorPanel/Private/InspectorPanel.cpp)
- [ ] **완료 기준.** A만 수정해 원본을 갱신하면 B는 독립 상태를 유지하고 C는 갱신된 원본을 사용한다. 프리팹 파일에는 루트 자기 참조가 없고, 저장·재열기 후에도 A·B·C의 상태가 계약과 일치한다. 다른 창에서 수정한 원본과 저장 충돌이 나면 원본과 각 편집 초안이 유지된다. 두 `Unpack` 메뉴에서 같은 Undo/Redo와 미저장 표시가 나온다.

### 2.4 프리팹 편집 모드 전환과 이력

**보통 · 실패 시 선택 유실과 메인 씬 Undo 소실.** 프리팹 모드는 대상 파일을 읽기 전에 메인 씬 선택을 비운다. 파일이나 인스턴스 로드가 실패하면 이 실패 경로에서 선택을 복구하지 않는다. `OpenScene()`의 반환값을 모드 전환 성공 조건으로 사용하지 않는 경우도 있다. 반면 독립 `PrefabEditorPanel`은 후보 로드가 성공한 뒤 기존 문서를 교체한다. 프리팹 모드에서 정상 복귀하더라도 `ResetDocument()`가 이전 메인 씬 Undo/Redo 목록을 지우고, 기존 미저장 여부만 되돌린다. [PrefabModeManager](../Editor/Source/ImGui/PrefabEditor/Private/PrefabModeManager.cpp) · [PrefabEditorPanel](../Editor/Source/ImGui/PrefabEditor/Private/PrefabEditorPanel.cpp) · [SceneEditHistory](../Editor/Source/Command/Private/SceneEditHistory.cpp)

- 후보 생성·Archive 검증·씬 전환 성공을 확인한 뒤 선택과 이력 대상을 바꾼다. 프리팹 A를 편집 중 B를 열다가 B 로드에 실패할 때도 A의 편집 문서를 유지할 수 있는 순서를 검토한다. 공용 미저장 확인의 저장·버리기·취소 의미를 유지한다.
- 메인 씬 이력 보존은 전환 실패 수정 뒤에 다룬다. 메인 씬을 메모리에 유지하는 기존 구조에서 이력과 저장 기준점을 문서별로 보관할 수 있는지 검토한다. 이력의 씬 참조, 보류 중 명령, 에셋 이동 후 이력 갱신을 함께 확인한다. [기존 프리팹 이력 계약](EditorUndoRedo.md#프리팹-변경-표시-후속-연동-2026-11-01)
- [ ] **완료 기준.** 잘못된 원본 열기, 저장 실패, 전환 취소에서 현재 씬·선택·편집 초안과 미저장 상태가 유지된다. 정상 진입·복귀 후 프리팹 편집은 메인 씬 이력에 섞이지 않는다. 이력 보존을 적용하면 진입 전 씬 편집을 복귀 후 Undo/Redo할 수 있다.

## 3. 원본 변경 전파의 범위

첫 검증은 현재 저장 방식의 **독립 인스턴스 값 보존과 명시적 원본 작업**을 기준으로 한다. 이 방식은 씬에 배치한 상태가 원본 갱신과 무관하게 저장된다는 장점이 있다. 원본을 수정할 때 이미 배치한 인스턴스 여러 개에도 반영해야 한다면, 안정된 프리팹 노드 ID와 씬 엔티티 ID의 대응부터 정의한다. 현재 `WritePrefabArchive()`의 로컬 ID는 순회 순서에 따라 부여되므로, 노드 추가·삭제·재정렬 이후까지 이어지는 노드 식별자로 사용하려면 형식 변경이 필요하다.

자동 갱신이 필요한 경우에만 원본 버전, 변경 필드, 삭제·추가 충돌, 수동 갱신 명령을 설계한다. 우선 대표 캐릭터로 갱신 빈도와 작업 중 불편을 확인한다. 중첩 프리팹과 Variant는 식별·복원 기준이 확정된 뒤 검토한다. [개발 우선순위](DevelopmentPriorities.md) · [프리팹 편집 흐름](ImGuiDevelopmentPlan.md#10-프리팹-관련-패널과-작업-흐름)

## 4. 통합 검증 시나리오

1. **샘플 준비.** 모델·Animator Controller와 Action Timeline이 연결된 캐릭터를 만든다. 프리팹 내부 엔티티 참조 하나와, 같은 씬의 별도 엔티티에서 프리팹 자식을 가리키는 참조 하나를 준비한다. 두 인스턴스 A·B를 같은 부모 아래에 배치하고, 이름과 위치를 다르게 설정한다. [Action Timeline 계획](AnimationTimelineEditorPlan.md)
2. **원본과 인스턴스.** A의 자식 값만 바꾸고 씬을 저장·재열기한다. A·B의 값과 내부 참조가 독립적으로 유지되는지 확인한다. A에서 원본 덮어쓰기를 실행하고 C를 새로 배치해 원본과 기존 인스턴스 정책을 확인한다. 프리팹 모드·독립 편집기의 저장과도 비교한다.
3. **되돌리기와 연결 해제.** B에 `Revert All`을 실행해 배치 Transform·형제 위치·내부 참조를 확인한다. 유지할 수 없는 외부 참조가 있는 경우 교체를 차단한다. `Unpack`은 인스펙터와 Hierarchy 두 곳에서 확인하고, 각 명령 직후와 Undo/Redo 후에 씬 변경 표시를 확인한다.
4. **실패와 재실행.** 원본 누락·손상, 읽기 전용 저장, 두 창에서 같은 원본 수정, 한글 경로 이름 변경을 각각 확인한다. 씬을 재열고 PIE를 시작·종료한 뒤 모델·Controller·타임라인과 카메라 참조가 유지되는지 본다. 실패 시 수정 전 씬과 원본 파일을 비교한다. [에디터 UX 검증 기준](EditorUXTestGuidelines.md)

## 5. 회귀 검사와 검증 상태

- `SceneArchiveCheck`는 기존 프리팹 왕복, 새 ID, 계층·자식 순서와 잘못된 입력의 배치 롤백을 검사한다. `EditorHistoryCheck`와 `AssetReferenceCheck`에는 프리팹·그래프 경로 이동 및 되돌리기 사례가 있다. 이 검사들을 바탕으로 두 인스턴스의 내부 참조, 교체의 외부 참조 보호, 원본 파일 충돌, 두 메뉴에서 같은 이력을 남기는 경우를 추가한다. [SceneArchiveCheck](../Engine/examples/SceneArchiveCheck.cpp) · [EditorHistoryCheck](../Editor/examples/EditorHistoryCheck.cpp) · [AssetReferenceCheck](../Editor/examples/AssetReferenceCheck.cpp)
- 변경할 고정 문구는 `Editor.Prefab.csv`·`Editor.Notification.csv` 등 기능별 기존 CSV의 한·영 키로 관리한다. 확인창의 버튼 순서와 파괴적 작업 표시, 키보드 입력은 기존 `EditorUI`, `InputManager`·`ShortcutManager` 규칙을 따른다. [에디터·클라이언트 지침 14·19절](EngineEditorClientGuidelines.md)

구현 후 사용자가 저장소 루트에서 실행할 기존 검사 명령이다.

```powershell
cmake --preset x64-debug -DBUILD_TESTING=ON
cmake --build --preset x64-debug --config Debug --target Editor AuditLocalization SceneArchiveCheck AssetReferenceCheck EditorHistoryCheck HierarchyEditorCheck
ctest --preset x64-debug --output-on-failure -R "SceneArchiveCheck|AssetReferenceCheck|EditorHistoryCheck|HierarchyEditorCheck|LocalizationAuditUtility"
```

이번 점검에서는 코드와 기존 검사 내용을 대조했다. 개선 코드, 에디터 UI 재현, 빌드·CTest와 PIE 실행 결과는 이후 구현 기록에 남긴다.
