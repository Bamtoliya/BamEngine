# 엔진 ReflectionRegistry 사용

Engine::ReflectionRegistry는 엔진 타입 정보와 ECS 컴포넌트 작업의 소유자다.
내부의 reflection::Registry는 독립 모듈의 타입·멤버 정보를 그대로 사용한다.
일반 모듈의 빌드·설치 방법은 [공통 가이드](ReflectionArchiveBuild.md), 설계 기준은
[엔진 지침](EngineEditorClientGuidelines.md)과 [리플렉션·아카이브 지침](ReflectionArchiveGuidelines.md)을 따른다.

## 소유권과 초기화

- Runtime 초기화가 ReflectionRegistry를 생성하고 종료 시 해제한다.
- 각 Scene의 entt::registry는 실제 엔티티와 컴포넌트를 소유한다.
- 축 잠금·스냅 같은 편집 상태는 월드의 ctx에 있는 Editor::ReflectionSceneEditState가 소유한다.
  이 상태는 컴포넌트 저장소 순회에 포함되지 않는다. 삭제된 엔티티의 항목은 다음 편집 상태 조회 시 정리한다.
- 통합 레지스트리는 타입 정보와 ECS 작업 함수만 소유한다. 특정 월드나 현재 Scene을 보관하지 않는다.
- GetRegistry는 타입 정보 조회, GetComponentEntries는 컴포넌트 추가 메뉴와 작업 목록 조회에 사용한다.
- 반환한 TypeInfo·컴포넌트 항목·목록 뷰는 레지스트리의 Free·Destroy 이후 사용할 수 없다.

기존 EntityComponentRegistry와 별도의 정적 GetEntityComponentRegistry 함수는 제거했다.
GameObject의 클래스형 ComponentRegistry는 별도로 유지한다.

## 컴포넌트 작업 등록

기존 Core·Rendering·Physics의 추가 정책은 ReflectionRegistry.cpp의 RegisterComponentOperations로 옮겼다.
Camera·Light 등 공간 컴포넌트는 Transform·WorldTransform·Flag를 함께 준비하는 정책을 유지한다.
동일한 항목을 Inspector 추가 메뉴와 객체 접근·직렬화에 사용하므로 메뉴에 타입을 다시 나열하지 않는다.

새로 COMPONENT 메타데이터를 붙인 타입은 생성된 ForEach_BamEngine_Type 순회에서 기본 작업을 연결한다.
현재 자동 연결 대상은 기본 생성 가능한 타입이다. 추가 준비가 필요한 종류에는 엔진의 추가 정책을 지정한다.
ID와 WorldTransform은 내부 작업에는 등록하지만 UserAddable=false로 추가 메뉴에서 제외한다.

ReflectionType은 같은 소유자의 Registry에 있는 TypeInfo를 참조한다. 멤버 정보를 복제하지 않는다.
RigidBody·Collider·Joint의 2D/3D 타입도 리플렉션에 등록되어 Inspector에서 접근한다.
편집 지원과 저장 지원은 별도다. Collider의 variant, vec2와 raw 엔티티 참조에는 아직 필요한 저장 연결이 없다.
타입이 등록되어도 멤버 값이 자동 직렬화 지원 범위 밖이면 해당 오류를 그대로 전달한다.

## 이름과 Transform 저장·복원

Runtime 초기화 후 기존 Scene에서 얻은 월드와 그 월드의 엔티티를 전달한다.
다음은 호출부에 넣을 코드 예시이며, Engine::ReflectionRegistry는 이미 생성되어 있어야 한다.

```cpp
#include "Reflection/ReflectionRegistry.h"
#include "Components/Core/TransformComponent.h"
#include <archive/JsonArchive.h>

archive::ArchiveResult SaveTransform(archive::IArchive& writer, const entt::registry& world, entt::entity entity)
{
    return Engine::ReflectionRegistry::Get().WriteComponent<Engine::TransformComponent>(
        writer, "Transform", world, entity);
}

archive::ArchiveResult RestoreTransform(const archive::IArchive& reader, entt::registry& world, entt::entity entity)
{
    return Engine::ReflectionRegistry::Get().ReadComponent<Engine::TransformComponent>(
        reader, "Transform", world, entity);
}
```

같은 호출을 NameComponent 등 등록 타입에 사용할 수 있다. JSON·BEVE는 같은 경로를 사용한다.
glm::vec3·quat·mat4 코덱은 엔진 레지스트리에서 한 번 준비한다.
선택 인자인 PropertyCodecs 포인터를 지정하면 해당 호출에서는 전달한 코덱 집합을 사용한다.
이 집합은 엔진 기본 코덱과 자동 병합되지 않으므로 필요한 값 타입의 코덱을 함께 준비해야 한다.

ReadComponent는 임시 값에서 읽기와 유효성 검사를 완료한 뒤 기존 컴포넌트에 반영한다.
없는 컴포넌트는 읽기가 성공한 후 추가 정책을 실행하여 생성한다.
Transform은 위치·크기를 검증하고 RestoreLocalRotation으로 회전·Euler 힌트·캐시를 맞춘다.
성공한 반영 후 TransformSystem이 월드 행렬을 갱신한다.
Camera·Light처럼 InvalidateCache를 제공하는 타입은 복원 후보의 캐시를 무효화한다.
일반적인 읽기 실패에서는 기존 값과 의존 컴포넌트를 보존한다. 사용자 코드의 예외·외부 부작용은 별도 범위다.

## 순회와 엔티티 컴포넌트 출력

Inspector는 Scene::ForEachReflectedComponent를 통해 타입 정보와 ObjectView를 받는다.
const Scene은 읽기 전용 뷰를 전달하고, 다른 Scene에 속한 Entity는 거부한다.
등록 여부·존재 확인과 실제 저장소 타입을 검사한 후 객체에 접근한다.

WriteComponents는 한 엔티티의 컴포넌트를 TypeInfo::QualifiedName을 키로 갖는 객체에 출력한다.
타입의 Transient 등 저장 제외 정책을 확인하므로 WorldTransform은 출력에서 제외한다.
미등록 저장소나 리플렉션 정보가 없는 컴포넌트는 오류로 거부한다. 실패하면 해당 쓰기의 기존 문서를 유지한다.

월드와 raw entt::entity의 쌍을 받는 API는 호출자가 올바른 쌍을 제공해야 한다.
다른 월드의 엔티티가 같은 숫자 핸들을 갖는지로 소유권을 식별할 수는 없다.
StorageId와 std::type_index는 실행 중 연결에 사용하며 영구 파일 식별자로 사용하지 않는다.

## 씬 파일 저장·복원

에디터의 Scene 메뉴에서 기존 Save/Load Scene 명령을 사용할 수 있다.
현재는 이름·로컬 Transform·Flag·Hierarchy로 구성한 빈 엔티티 씬을 기준으로 검증했다.
SceneManager::SaveScene은 현재 씬을 WriteArchive로 출력한 후 파일에 기록한다.
LoadScene은 파일을 읽고 Scene::CreateFromArchive로 임시 씬을 만든 뒤, 전체 복원이 성공했을 때 OpenScene을 호출한다.
JSON과 BEVE는 같은 씬 형식과 리플렉션 처리 경로를 사용한다. 에디터 파일 대화상자는 현재 JSON을 선택한다.

- Scene::CreateEntity는 0이 아닌 씬 내부 영구 ID를 가진 IDComponent를 자동으로 추가한다.
  IDComponent를 다시 추가할 필요가 없다. FindEntityById로 해당 씬의 Entity를 찾는다.
  ID는 씬 파일 안에서 참조를 연결하는 식별자이며, 서로 다른 씬을 아우르는 UUID는 아니다.
- 파일의 Scene 객체에는 Version=1, Name, Active, Entities를 기록한다.
  엔티티에는 Id, 선택적인 Hierarchy 객체, Components 배열을 기록한다.
  각 컴포넌트는 Type에 QualifiedName, Data에 공통 리플렉션 API의 결과를 담는다.
  기존 WriteComponents의 타입 이름을 키로 갖는 출력 형식과 씬의 컴포넌트 배열 형식은 구분한다.
- Hierarchy의 Parent와 Children은 영구 ID를 사용하며, Parent=0은 부모 없음을 뜻한다.
  모든 엔티티를 만든 후 ID를 새 EnTT 핸들로 연결하고 자식 순서를 보존한다.
  IDComponent와 HierarchyComponent는 씬 구조에서 한 번만 처리한다.
  HierarchyComponent는 COMPONENT로 등록하지만 NOSERIALIZE로 raw 핸들의 일반 값 직렬화를 막는다.
  일반 컴포넌트의 멤버를 씬 코드에서 다시 나열하지 않는다.
- 중복·0 ID, 알 수 없는 컴포넌트, 중복 컴포넌트, 누락 필드, 잘못된 값과 참조를 오류로 처리한다.
  부모·자식 양방향 불일치, 중복 자식, 순환과 256단계를 넘는 깊이를 거부한다.
  현재 연결된 부모와 자식에는 Transform이 필요하다. 복원이 끝난 후 월드 행렬을 다시 계산한다.
- Scene::CreateFromArchive의 출력 Scene*은 호출 전에 nullptr이어야 한다.
  성공하면 참조 수 1인 새 씬을 반환하므로 호출자가 Release하거나 SceneManager에 전달한 뒤 자신의 참조를 해제한다.
  일반적인 복원 실패에서는 출력은 nullptr이며, 기존 씬·엔티티·편집 상태를 변경하지 않는다.
  씬 교체 후 이전 월드의 Entity 포인터·핸들·ctx 참조를 새 씬에 사용하지 않는다.
- Paused, WorldTransform 캐시와 월드 ctx의 편집 설정은 씬 파일에 저장하지 않는다.
  불러온 씬의 Paused는 false다. 성공한 에디터 파일 로딩 후 선택 상태와 Undo/Redo 기록을 비운다.
  실패한 로딩에서는 이를 유지한다.
- 씬 관리는 렌더러 초기화 없이도 실행할 수 있다. 씬을 닫을 때 기존 Light·Collision·Camera 관리자가
  생성되어 있으면 기존 정리 작업을 수행한다.

저장할 수 없는 리소스 핸들·Collider variant·vec2·Joint 엔티티 참조 또는 미등록 컴포넌트를 가진 씬은 저장에 실패한다.
지원하지 않는 데이터를 조용히 생략하여 저장 성공으로 처리하지 않는다.
직렬화 실패에서는 기존 문서와 파일을 유지한다. 파일 기록 중 I/O 실패에 대한 원자적 교체는 아직 제공하지 않는다.
파일 버전은 현재 1만 읽으며, 타입·필드 이름 변경에 대한 마이그레이션은 아직 없다.
직접 entt::registry::create로 만든 엔티티는 Scene의 Entity 래퍼와 ID가 없으므로 씬 저장 대상에 사용할 수 없다.
이 형식은 ECS 씬 데이터 대상이며, 기존 GameObject·EventSystem·UICanvas 데이터는 포함하지 않는다.

## Inspector 속성 편집

Inspector는 컴포넌트마다 멤버를 다시 나열하지 않고 ReflectionPropertyDrawer를 사용한다.
PROPERTY의 Category를 기준으로 배경 없는 트리 그룹을 만들고, Category가 없으면 UI_GENERAL 그룹을 사용한다.
컴포넌트는 배경이 있는 헤더로 구분한다. 속성·값의 제목 행과 표 배경·구분선은 표시하지 않고 2열 정렬만 유지한다.
AddComponent 버튼은 패널의 사용 가능한 가로 너비를 채운다.
그룹 ID는 번역 전 키와 타입·프로퍼티 경로로 구분한다. 언어를 바꾸어도 접힘 상태를 유지한다.
컴포넌트 헤더의 바깥에서는 Scene과 엔티티별로 ID를 구분한다.

타입의 HIDDEN(VISIBLE(false)) 메타데이터는 Inspector 본문과 AddComponent 메뉴에서 숨기는 정책이다.
HierarchyComponent에 적용하여 부모·자식 관계는 기존 HierarchyPanel에서 표시한다.
타입별 Inspector 특수화는 필요하지 않으며, 리플렉션 등록·엔진 컴포넌트 순회·씬 관계 저장은 유지한다.
PROPERTY의 HIDDEN은 해당 속성만 숨긴다. UI 표시 정책과 NOSERIALIZE 저장 정책은 별개다.

- DisplayName·Category는 LocalizationManager의 키로 해석한다. 번역이 없거나 빈 값이면 원래 키를 표시한다.
  AddComponent의 카테고리와 항목 이름도 같은 메타데이터와 번역 경로를 사용한다.
  엔진 타입의 키는 ExportReflectionLocalization이 Engine.csv에 병합하고 GenerateLocalization이 JSON을 만든다.
- 문자열·wstring, 정수·실수, bool, enum·BITMASK, vec2/3/4, 색상, Quaternion과 행렬을 표시한다.
  Editable과 ReadOnly, 실제 멤버 쓰기 가능 여부 및 const ObjectView를 함께 검사한다.
  실수 입력은 유한값만 허용하고 RangeMin/RangeMax가 있으면 범위 안으로 제한한다.
  Color 메타데이터가 있는 벡터는 색상 편집기를 사용한다.
- 등록된 중첩 값은 공통 Drawer로 편집한다. Collider의 Shape는 대안을 선택하고 내부 프로퍼티를 편집한다.
  ColliderComponent·Collider2DComponent는 임시 복사본에서 편집하고 기존 SetOffset/SetSettings/SetShape 검증 후 반영한다.
  잘못된 반지름·크기·마찰·반발력은 원본을 보존하며, 성공하면 Dirty를 표시한다.
  Camera·Light 편집은 캐시를 무효화하고 Transform 편집은 기존 회전 동기화·월드 행렬 갱신을 사용한다.
- Tag의 unordered_set<wstring>은 추가·삭제를 지원한다. 입력 버퍼는 월드 ctx의 엔티티별 편집 상태에 둔다.
  누적 힘·역관성과 Joint의 raw ConnectedEntity는 읽기 전용이다.
  엔티티 관계를 숫자로 직접 고치는 UI는 제공하지 않는다.
- ResourceHandle<Texture/Sprite/Mesh/Material/MaterialInstance/MaterialInterface/Skeleton/Shader/Animation>은
  Content Browser 파일 드래그앤드롭과 현재 로드된 리소스 선택·해제를 지원한다.
  페이로드의 바이트 길이와 종료 문자를 검사하고 ResourceManager로 로드한 실제 객체 타입을 확인한다.
  MaterialInterface에는 Material·MaterialInstance를 허용하며 다른 타입은 기존 값을 보존하고 오류를 표시한다.
  Content Browser는 CONTENT_BROWSER_ITEM 페이로드 하나를 보낸다. 확장자만으로 타입을 판단하지 않는다.
  잘못된 타입을 거부해도 로더가 만든 리소스는 ResourceManager에 남을 수 있다.

추가 타입의 일반 멤버 편집에는 PROPERTY 등록을 사용한다.
엔진 특유의 리소스 해석과 Collider 검증은 Editor에서 처리하며 독립 Reflection 모듈은 Editor에 의존하지 않는다.
현재 일반 컨테이너는 크기 표시만 지원하며, 모든 컨테이너 요소 편집은 구현하지 않았다.
속성·기즈모·구조 편집의 Undo/Redo 범위와 제약은 [EditorUndoRedo.md](EditorUndoRedo.md)를 따른다.
편집 가능해진 물리 타입이 모두 씬 저장 가능한 것은 아니다.
특히 Joint의 ConnectedEntity를 NOSERIALIZE로 감추지 않으며 참조 처리가 준비되지 않은 저장은 오류로 처리한다.

ReflectionInspectorCheck는 생성된 영어·한국어 JSON에서 실제 번역 키를 확인한다.
헤드리스 ImGui 프레임에서 속성 표시와 클릭, Category 접기·펼치기,
언어 변경 후 상태 유지와 const 입력 거부를 확인한다.
리소스 페이로드 검증·상속 타입 호환·실제 드롭 전달 및 Collider의 잘못된 후보 보존과 Dirty 갱신도 검사한다.
이는 실제 Editor 창의 시각적 배치나 GPU 리소스 생성까지 검증한 것은 아니다.

## HierarchyPanel 계층 표시와 부모 변경

HierarchyPanel은 Scene의 실제 Entity 래퍼와 HierarchyComponent를 사용한다.
루트는 씬 내부 영구 ID 순서로 표시하며, 자식은 children에 저장된 순서를 유지한다.
이름이 없는 엔티티도 Entity ID로 표시한다. Scene에 등록되지 않은 raw EnTT 엔티티는 표시하지 않는다.
잘못된 부모·자식 연결, 중복, 순환이나 깊이 초과가 있으면 표시용 트리에서만 연결을 끊고 경고한다.
이 과정에서 원본 HierarchyComponent를 자동으로 고치거나 엔티티를 숨기지 않는다.

- 노드 클릭은 Entity 선택, Ctrl+클릭은 다중 선택 토글이다. 화살표 클릭은 접기·펼치기다.
  Viewport 등에서 기본 선택 엔티티가 바뀌면 그 조상 노드를 펼치고 선택 행으로 스크롤한다.
  같은 선택을 유지하는 동안 사용자가 접은 노드를 매 프레임 다시 펼치지 않는다.
- 엔티티를 다른 노드에 드롭하면 그 노드의 마지막 자식으로 이동한다.
  드래그 중 나타나는 루트 드롭 영역에 놓으면 부모를 해제한다. 한 번에 드래그한 엔티티 하나를 이동한다.
  선택 잠금 중에는 클릭 변경과 관계 변경을 하지 않는다.
- 드래그 데이터는 원본 Scene, 실행 핸들과 영구 ID를 확인한다. 다른 씬이나 삭제·교체된 엔티티는 거부한다.
  드래그 중에는 원본 Scene 참조를 유지한다. 관계 변경은 트리 그리기가 끝난 후에 실행한다.

실제 변경은 Engine의 공통 Scene::ReparentEntity가 담당한다. Editor 전용 저장 구조는 추가하지 않는다.
다음은 유효한 child·parent가 같은 Scene에 속한 경우의 호출 예시다.

```cpp
const auto moveResult = scene->ReparentEntity(child, &parent);
const auto detachResult = scene->ReparentEntity(child);
```

양쪽 parent·children을 함께 갱신하고 자식 및 후손의 월드 변환을 유지한다.
기존 형제의 상대 순서는 보존한다. 같은 부모 지정은 Unchanged이며 중복 자식을 추가하지 않는다.
공간 관계 변경에는 관련 엔티티의 TransformComponent와 WorldTransformComponent가 필요하다.
CanvasComponent·RectTransformComponent를 사용하는 UI 엔티티의 부모 변경은 현재 거부한다.
UI의 앵커·크기·레이아웃을 유지하는 처리는 3D TRS 변환과 별도로 연결해야 한다.
자기 자신·자손으로의 이동, 다른 씬, 이미 깨진 관계와 씬 형식의 깊이 256 제한 초과를 거부한다.
부모 행렬이 특이하거나 변환 결과를 TRS로 정확히 다시 구성할 수 없는 전단 등도 거부한다.
실패하면 일반적인 검증 경로에서는 관계·로컬 변환·기존 월드 캐시를 보존한다.
메모리 할당 예외나 사용자 코드의 동시 변경까지 포함하는 트랜잭션은 보장하지 않는다.
성공하면 TransformSystem으로 월드 캐시를 갱신하며, 저장은 기존 Scene JSON·BEVE 경로를 사용한다.

현재 형제 순서 드래그 정렬과 여러 선택 항목의 일괄 이동은 제공하지 않는다.
관계 변경 Undo/Redo는 Editor의 메모리 이력으로 처리한다. 저장 경로와는 별개다.
HierarchyEditorCheck는 관계·월드 변환 보존, 실패 보존, 계층 저장·복원, 비정상 트리 표시,
실제 ImGui 클릭·드롭과 다른 씬 드롭 거부 및 드래그 중 원본 Scene 수명을 검사한다.

## 검사와 현재 범위

의존성이 준비된 저장소 루트 CMake 구성에서 BAMENGINE_BUILD_REFLECTION_CHECK=ON과 BUILD_TESTING=ON을 지정한다.

```powershell
cmake --build out/build/engine-reflection-unified --config Debug `
    --target SceneReflectionCheck SceneArchiveCheck TransformEditorCheck `
        PickingMathCheck ReflectionInspectorCheck HierarchyEditorCheck Editor
ctest --test-dir out/build/engine-reflection-unified -C Debug --output-on-failure --no-tests=error
```

Release 검사는 같은 빌드·CTest 명령의 구성을 Release로 바꾼다.
엔진 리플렉션 생성 파일은 빌드 디렉터리의 Engine/Generated/<Config>에 둔다.
_DEBUG 같은 조건부 멤버가 있으므로 구성 사이에 생성 파일을 공유하지 않는다.
Localization 키 추출도 현재 구성의 REFLECTION_METADATA_FILES를 사용한다.
빌드 디렉터리는 Visual Studio 18 2026 x64와 필요한 vcpkg·LLVM 경로로 구성되어 있어야 한다.

SceneReflectionCheck는 기존 추가 정책, 월드 독립성, const·타입 검사, 이름과 Transform의 저장·복원,
실패 시 원본 보존과 Transient 제외를 확인한다.
TransformEditorCheck는 월드 행렬의 로컬 변환, 축 잠금과 잘못된 편집 거부를 확인한다.
편집 상태의 월드 독립성, 엔티티 삭제 후 정리와 편집 상태 생성 후의 컴포넌트 출력도 검사한다.
SceneArchiveCheck는 JSON·BEVE 파일 왕복, 빈 씬, Unicode 이름, 53비트를 넘는 ID,
실행 핸들이 바뀐 부모 참조·형제 순서·월드 행렬 복원과 로딩 후 ID 할당을 검사한다.
잘못된 버전·ID·컴포넌트·필드·회전·계층과 손상·누락 파일에서 현재 씬 보존을 확인한다.
지원하지 않는 컴포넌트의 쓰기 실패에서 기존 문서·파일 보존을 확인한다.
잘못된 Transform·계층·ID의 쓰기 실패에서 문서 보존과 깊이 256/257의 경계도 검사한다.
검사 통과는 에디터 창에서의 수동 조작이나 GPU 출력까지 검증했다는 뜻은 아니다.

PIE 복원은 아직 미구현 상태이며 Play 진입은 비활성 상태다.
실행 상태·타이머·외부 연결의 초기화까지 검증한 뒤 연결해야 한다.
리소스 핸들의 영구 ID 변환, 물리 값 타입의 저장 연결과 Hierarchy 밖의 엔티티 참조도 별도로 준비해야 한다.
