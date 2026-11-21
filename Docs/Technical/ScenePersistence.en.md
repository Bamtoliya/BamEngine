# Scene Persistence Architecture

This document describes the Engine scene Archive and the Editor scene-document boundary. For the user-facing menu workflow, see [Basic Editor Workflow](../UserGuide/EditorWorkflow.en.md). For the general serialization contract, see [Reflection / Archive guidelines](../ReflectionArchiveGuidelines.md).

## Responsibility boundaries

- `Scene::WriteArchive` and `Scene::CreateFromArchive` write and restore scene data through `archive::IArchive`. The Engine layer does not depend directly on one Archive format.
- The Editor's `SceneDocument` owns the document path, save eligibility, and edit-history save point. The current Editor save path uses `archive::JsonArchive`.
- `SceneManager` reads or writes Archives from files and opens a candidate scene only after loading succeeds.
- Scene serialization and Editor Undo/Redo history are separate. Edit history is not stored in the scene file.

## Saved data shape

The current scene Archive version is `1`. This abbreviated example shows the field relationships; component `Data` depends on Reflection registration and serialization policy.

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

- `Id` is the persistent entity identifier. Do not treat the runtime EnTT handle as a persistent ID.
- When present, `Hierarchy` stores `Parent` and an order-preserving `Children` list separately. `IDComponent` and `HierarchyComponent` are not duplicated in the regular `Components` array.
- `Components` contains types registered with Reflection and enabled for serialization, represented by a `Type` name and `Data` value. Entities are written in ascending ID order; components are written by Reflection qualified type name.
- A disabled component keeps its `Data`. A component using the common activation state writes an optional `Enabled: false` beside `Type`. Records without the field keep the enabled default. Components with an existing activation property keep it in `Data`. Prefabs use the same component record.
- Resource references discovered while writing resource handles are collected separately under the scene object's `ResourceReferences`. Key/path formats and URI resolution follow the resource Archive adapter contract.

## Editor save flow

`ToolBar::SaveScene` forwards a save request to `EditorDocumentController`. `ProcessPendingSave` obtains a path through the file dialog when needed, then calls the document's `SaveTo`. `SceneDocument::SaveTo` checks that the document is writable and bound to the current editable scene, then normalizes the target to an absolute path. See [`EditorDocumentFileDialogs.cpp`](../../Editor/Source/Document/Private/EditorDocumentFileDialogs.cpp) for that boundary.

The save sequence is:

1. Flush edit history and confirm that a saved baseline can be established.
2. Serialize through `Scene::WriteArchive` and write the target with `JsonArchive::SaveToFile`.
3. On success, mark the history as saved and update the document path.

Save failures are not reported as success. The exact on-disk replacement atomicity depends on the `JsonArchive::SaveToFile` implementation; this document does not promise atomic writes. See [`SceneDocument.cpp`](../../Editor/Source/Document/Private/SceneDocument.cpp) and [`SceneArchive.cpp`](../../Engine/Source/World/Scene/Private/SceneArchive.cpp).

The Engine's `SceneManager::SaveScene` is a separate path that accepts `archive::IArchive`, then calls `Scene::WriteArchive` and `SaveToFile`. Relative paths are resolved through `PlatformPath::ResolveProjectRelative`. Do not conflate this general Engine path with the Editor document save path.

## Load and validation flow

`SceneManager::LoadScene` reads the file and constructs a candidate scene through `Scene::CreateFromArchive`. It requests `OpenScene` only after candidate construction and validation succeed.

`CreateFromArchive` builds a temporary candidate rather than partially applying data to the existing scene.

1. Validate that the ReflectionRegistry exists, the Archive is at the expected object depth, and the version is `1`.
2. Read `ResourceReferences` and preload resources that can be resolved. Unresolved references are logged and deferred.
3. Create entities from the stored IDs and read components through Reflection. Runtime handles are newly created.
4. Validate nonzero, unique IDs; supported component types; parent/child consistency; hierarchy cycles; and the maximum hierarchy depth of 256.
5. Reconnect hierarchy links to the new runtime handles and update the Transform hierarchy.
6. Return the candidate only if every step succeeds. Failed candidates are released and are not opened as the current scene by `SceneManager`.

If a requested `.json` path does not exist, `SceneManager::LoadScene` checks once for a same-name `.bamscene.json` file. See [`SceneManager.cpp`](../../Engine/Source/World/Scene/Private/SceneManager.cpp) for path resolution and this compatibility behavior.

## Current constraints

- The scene and Prefab Archive version is currently `1`; other scene versions are rejected. Do not assume an automatic migration path.
- Components without a valid Reflection registration or with a storage type that does not match that registration can cause saving to fail. Unknown serialized component types are unsupported during loading.
- Hierarchy parent/child relationships are restored through persistent IDs. Arbitrary `entt::entity` fields, pointers, or references nested in containers are not guaranteed to be remapped to new runtime handles. Check each type's codec and reference policy; see the [Prefab Scene Workflow Plan](../PrefabSceneWorkflowPlan.md) for related risks.
- Missing reflected member fields and defaults follow the contract in the [Reflection / Archive guidelines](../ReflectionArchiveGuidelines.md). That does not make a mismatched top-level scene version acceptable.
- `SceneDocument::DiscardChanges` reloads the saved document or creates an empty scene when the document has no saved path. After a successful scene replacement, the edit-history baseline is reset for the new scene. See the separate [Editor history document](../EditorUndoRedo.md) for Undo/Redo behavior.

## Where to verify

- Scene serialization: [`SceneArchive.cpp`](../../Engine/Source/World/Scene/Private/SceneArchive.cpp)
- File save/load and current-scene replacement: [`SceneManager.cpp`](../../Engine/Source/World/Scene/Private/SceneManager.cpp)
- Editor document adapter: [`SceneDocument.cpp`](../../Editor/Source/Document/Private/SceneDocument.cpp)
- Scene Archive regression checks: [`SceneArchiveCheck.cpp`](../../Engine/examples/SceneArchiveCheck.cpp)

`SceneArchiveCheck` includes paths that exercise scene/Prefab data and resource references with JSON and BEVE Archives. Builds and tests were not rerun for this documentation change.

## 한국어

[씬 저장·복원 구조](ScenePersistence.md)
