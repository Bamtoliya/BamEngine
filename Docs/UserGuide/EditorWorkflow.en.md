# Basic Editor Workflow

This guide describes basic workflows connected in the current source. A feature being implemented and its latest interactive validation are different claims; do not infer that every UI step has passed in the current environment unless a recent result says so.

## Open and save scenes

Use the top-level `Scene` menu to create a scene, save it, save it under another name, or request a scene load. The scene file dialog accepts `.bamscene.json` and `.json`. The Editor's `SceneDocument` saves scenes through a JSON Archive.

Saving or loading is rejected when the current execution state does not allow saving or another document operation is in progress. Follow the confirmation dialog when replacing a scene with unsaved changes.

## Add and edit entities

1. Right-click an empty area of the `Hierarchy` panel to open the creation menu.
2. Choose an empty entity, a built-in 3D primitive, a light, a Sprite, a Canvas, or Terrain.
3. Select an entity in the Hierarchy to edit its registered components and properties in the `Inspector`. Use the Scene View Transform gizmo to edit position, rotation, and scale.
4. Save the changes from the `Scene` menu.

Supported entity types and operations are grounded in the current source and existing design documents. Prefab engine APIs and the Editor workflow may have different completion states; check the [Prefab Scene Workflow Plan](../PrefabSceneWorkflowPlan.md).

## Scene Undo/Redo

Use Undo/Redo from the `Edit` menu, or use `Ctrl+Z` for Undo and `Ctrl+Y` or `Ctrl+Shift+Z` for Redo in a scene-editing context. Text-edit history takes precedence while editing text.

The supported changes and constraints are listed in [Scene Undo/Redo scope](../EditorUndoRedo.md). Actions that do not modify scene data, such as selection, camera movement, and panel docking, are not part of scene history.

## Browse assets

Use the `Content Browser` to browse project assets and engine resources. Resource opening and importing depend on the file type and the active editor path. Do not treat an unverified editor or conversion path as a universal import feature.

File moves, renames, and deletion are connected to reference-impact checks and file history. Review the impact confirmation before applying a change; consult the Undo/Redo guide for recovery behavior and constraints.

## Verification scope

- Scene Archive and Editor history have automated check paths.
- On 2026-11-20, the user confirmed a full `x64-ninja-debug` build and 44/44 CTest pass.
- This does not establish interactive validation of every panel, DPI/language layouts, GPU picking, PIE, or the latest uncommitted UI. Use the [Editor UX verification guidelines](../EditorUXTestGuidelines.md) for interactive acceptance criteria.

## 한국어

[한국어 에디터 기본 작업](EditorWorkflow.md)
