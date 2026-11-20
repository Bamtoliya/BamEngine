# 씬 저장·복원 구조

이 문서는 Engine의 씬 Archive와 Editor의 씬 문서 저장 경계를 설명합니다. 사용자가 메뉴에서 씬을 저장·불러오는 순서는 [에디터 기본 작업](../UserGuide/EditorWorkflow.md)을, 범용 직렬화 계약은 [Reflection·Archive 지침](../ReflectionArchiveGuidelines.md)을 참고하세요.

## 책임 경계

- `Scene::WriteArchive`와 `Scene::CreateFromArchive`는 씬 데이터를 `archive::IArchive`에 기록하고 복원합니다. Engine 계층은 Archive 구현 형식에 직접 결합하지 않습니다.
- Editor의 `SceneDocument`는 씬 문서의 파일 경로, 저장 가능 여부와 편집 이력 기준점을 관리합니다. 현재 Editor 저장 경로는 `archive::JsonArchive`를 사용합니다.
- `SceneManager`는 파일에서 Archive를 읽거나 쓰고, 로드가 성공한 후보 씬을 현재 씬으로 엽니다.
- 씬 데이터 직렬화와 Editor의 Undo/Redo 이력은 별개입니다. 편집 이력은 저장 파일에 포함되지 않습니다.

## 저장 데이터 모양

현재 씬 Archive의 버전은 `1`입니다. 아래는 필드 관계를 보여주는 축약 예시이며, 실제 컴포넌트의 `Data` 내용은 Reflection 등록과 직렬화 정책에 따라 달라집니다.

```json
{
  "Scene": {
    "Version": 1,
    "Name": "Scene",
    "Active": true,
    "Entities": [
      {
        "Id": 1,
        "Hierarchy": {
          "Parent": 0,
          "Children": []
        },
        "Components": [
          {
            "Type": "Engine::TransformComponent",
            "Data": {}
          }
        ]
      }
    ],
    "ResourceReferences": []
  }
}
```

- `Id`는 저장용 엔티티 식별자입니다. 실행 중 사용하는 EnTT 핸들을 영속 ID로 취급하지 않습니다.
- `Hierarchy`가 있는 엔티티는 `Parent`와 순서가 보존되는 `Children` 목록을 별도 저장합니다. `IDComponent`와 `HierarchyComponent`는 일반 `Components` 배열에 중복 저장하지 않습니다.
- `Components`에는 Reflection에 등록되고 직렬화가 허용된 컴포넌트 타입이 `Type` 이름과 `Data` 값으로 들어갑니다. 엔티티는 ID 오름차순, 컴포넌트는 Reflection의 정규 타입 이름 순으로 기록됩니다.
- 리소스 핸들에서 발견한 참조는 씬 객체의 `ResourceReferences`에 별도로 수집됩니다. 키·경로 형식과 URI 해석은 리소스 Archive 어댑터의 계약을 따릅니다.

## Editor 저장 흐름

`ToolBar::SaveScene`은 `EditorDocumentController`에 저장 요청을 전달합니다. `ProcessPendingSave`가 필요하면 파일 대화상자로 경로를 받은 뒤 문서의 `SaveTo`를 호출합니다. `SceneDocument::SaveTo`는 현재 문서가 쓰기 가능하고 현재 편집 씬과 연결되어 있는지 확인한 뒤, 경로를 절대 경로로 정규화합니다. 경계 구현은 [`EditorDocumentFileDialogs.cpp`](../../Editor/Source/Document/Private/EditorDocumentFileDialogs.cpp)를 참고하세요.

저장 단계는 다음과 같습니다.

1. 편집 이력을 반영하고 저장 기준점을 확정할 수 있는지 확인합니다.
2. `Scene::WriteArchive`로 씬을 직렬화하고 `JsonArchive::SaveToFile`로 대상 파일에 씁니다.
3. 저장에 성공하면 이력을 저장 완료 상태로 표시하고 문서의 경로를 갱신합니다.

저장 실패를 성공으로 처리하지 않습니다. 구체적인 디스크 교체 원자성은 `JsonArchive::SaveToFile` 구현의 계약을 확인해야 하며, 이 문서는 원자적 저장을 보장한다고 가정하지 않습니다. 관련 구현은 [`SceneDocument.cpp`](../../Editor/Source/Document/Private/SceneDocument.cpp)와 [`SceneArchive.cpp`](../../Engine/Source/World/Scene/Private/SceneArchive.cpp)에 있습니다.

Engine의 `SceneManager::SaveScene`은 `archive::IArchive`를 받아 `Scene::WriteArchive`와 `SaveToFile`을 호출하는 별도 경로입니다. 상대 경로는 `PlatformPath::ResolveProjectRelative`로 해석됩니다. 따라서 Editor 문서 저장 경로와 Engine의 일반 파일 저장 경로를 같은 계층으로 혼동하지 마세요.

## 로드와 검증 흐름

`SceneManager::LoadScene`은 파일을 읽은 뒤 `Scene::CreateFromArchive`로 새 후보 씬을 만듭니다. 후보 생성과 검증이 성공한 뒤에만 `OpenScene`을 요청합니다.

`CreateFromArchive`는 기존 씬에 데이터를 부분 적용하지 않고 임시 후보를 구성합니다.

1. ReflectionRegistry와 Archive의 최상위 객체 상태를 확인하고 버전 `1`인지 검사합니다.
2. `ResourceReferences`를 읽고 가능한 리소스를 미리 불러옵니다. 해결되지 않은 참조는 경고를 남기고 지연 처리합니다.
3. 저장된 ID로 엔티티를 생성하고 Reflection을 통해 컴포넌트 데이터를 읽습니다. 실행 핸들은 새로 만들어집니다.
4. 중복·0 ID, 지원되지 않는 컴포넌트 타입, 잘못된 부모·자식 관계, 순환 계층 및 최대 깊이(256)를 검사합니다.
5. 계층 링크를 새 실행 핸들로 연결하고 Transform 계층을 갱신합니다.
6. 모든 단계가 성공한 경우에만 후보 씬을 호출자에게 반환합니다. 실패한 후보는 해제되며 `SceneManager`는 그 후보를 현재 씬으로 열지 않습니다.

`SceneManager::LoadScene`은 존재하지 않는 `.json` 경로에 대해 같은 이름의 `.bamscene.json` 파일을 한 번 확인합니다. 경로 해석과 해당 호환 동작은 [`SceneManager.cpp`](../../Engine/Source/World/Scene/Private/SceneManager.cpp)에 있습니다.

## 현재 제약과 주의점

- 씬과 Prefab의 Archive 버전은 현재 `1`이며 다른 씬 버전은 거부됩니다. 자동 마이그레이션 경로가 있다고 가정하지 마세요.
- 등록되지 않았거나 실제 저장소 타입과 Reflection 등록이 맞지 않는 컴포넌트는 저장에 실패할 수 있습니다. 로드 시 알 수 없는 컴포넌트 타입도 지원되지 않는 타입 오류입니다.
- 계층의 부모·자식 관계는 영속 ID를 통해 다시 연결합니다. 임의 컴포넌트 필드에 든 `entt::entity`, 포인터 또는 중첩 컨테이너 참조까지 자동으로 새 실행 핸들로 바뀐다고 보장하지 않습니다. 각 타입의 코덱과 참조 정책을 별도로 확인하고, 프리팹 관련 위험은 [Prefab 씬 워크플로 계획](../PrefabSceneWorkflowPlan.md)을 참고하세요.
- Reflection 멤버의 누락 필드·기본값 정책은 [Reflection·Archive 지침](../ReflectionArchiveGuidelines.md)의 계약을 따릅니다. 이는 씬 최상위 버전 불일치 허용을 의미하지 않습니다.
- `SceneDocument::DiscardChanges`는 저장된 문서를 다시 읽거나, 저장 경로가 없는 새 문서라면 빈 씬을 만듭니다. 성공한 씬 교체 뒤 편집 이력 기준점도 새 씬에 맞춰 초기화합니다. Undo/Redo는 별도 [편집 이력 문서](../EditorUndoRedo.md)를 참고하세요.

## 확인 위치

- 씬 직렬화 구현: [`SceneArchive.cpp`](../../Engine/Source/World/Scene/Private/SceneArchive.cpp)
- 파일 저장·로드 및 현재 씬 교체: [`SceneManager.cpp`](../../Engine/Source/World/Scene/Private/SceneManager.cpp)
- Editor 문서 어댑터: [`SceneDocument.cpp`](../../Editor/Source/Document/Private/SceneDocument.cpp)
- 씬 Archive 회귀 검사: [`SceneArchiveCheck.cpp`](../../Engine/examples/SceneArchiveCheck.cpp)

`SceneArchiveCheck`에는 씬·Prefab 데이터와 리소스 참조를 JSON 및 BEVE Archive로 다루는 검사 경로가 있습니다. 이번 문서 변경에서는 빌드와 테스트를 다시 실행하지 않았습니다.

## English

[Scene Persistence Architecture](ScenePersistence.en.md)
