# BamEngine Documentation

This index links user and developer documentation based on the current source tree and recorded verification. Code being present and a feature passing runtime verification are distinct claims.

## User guides

- [Getting Started](UserGuide/GettingStarted.en.md): development environment, dependencies, and building/running the editor
- [Basic Editor Workflow](UserGuide/EditorWorkflow.en.md): scenes, entities, asset browsing, and Undo/Redo

## Developer documentation

- [Architecture Overview](Technical/Architecture.en.md): CMake targets, Editor/Client entry points, Runtime, scene/resource/Reflection flows
- [Scene Persistence Architecture](Technical/ScenePersistence.en.md): scene Archive data, Editor save boundary, load validation, and constraints

## Existing design and verification references

- [Engine, Editor, and Client guidelines](EngineEditorClientGuidelines.md)
- [Source organization guidelines](SourceOrganizationGuidelines.md)
- [Reflection / Archive build guide](ReflectionArchiveBuild.md)
- [Shader binding layout](Shader/BINDING_LAYOUT.md)
- [Scene Undo/Redo scope and constraints](EditorUndoRedo.md)
- [Editor UX verification criteria](EditorUXTestGuidelines.md)
- [Client build and packaging plan, including remaining verification](BuildAndPackagingPlan.md)
- [Implementation log](ImplementationLog.md)
- [Development priorities](DevelopmentPriorities.md)

## Status labels

- **Implemented in source** means the path exists in the current source; it does not imply UI, GPU, or deployment validation.
- **Automated verification** means the named CTest or automated check covered that scope.
- **User-run verification** means an execution result is recorded from the user's environment.
- **Unverified / planned** means there is no recorded execution result or the work remains a design/development plan.

The implementation log records a user-confirmed `x64-ninja-debug` build and 44/44 CTest pass on 2026-11-20. That record applies to that point in time; it does not certify every change in the current worktree, GPU rendering, all UI workflows, or standalone deployment. Check the linked plan and implementation records for each feature's current status.

## 한국어

[한국어 문서 색인](README.md)
