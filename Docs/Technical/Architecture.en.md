# Architecture Overview

This document summarizes the current CMake configuration and major execution paths. Long-term plans and type declarations alone do not establish feature completion. Check the linked source, plans, and implementation records for current implementation and verification status.

## CMake targets and dependency direction

```text
Editor ─────┐
Client ─────┼──> Engine
GameModule ─┘
```

- The root CMake configuration builds `Engine`, converter tools, and the Asset Worker.
- Non-Emscripten builds also add `Editor`, `GameModule`, and `Client`. `BAMENGINE_WITH_EDITOR` is enabled by default.
- The `Editor` target includes editor UI and asset tools.
- `Engine` is defined as a shared library for native configurations such as Windows and as a static library for Emscripten.
- `Editor` and `Client` are Engine consumers. `GameModule` also links to `BamEngine::Engine`. See the [Engine, Editor, and Client guidelines](../EngineEditorClientGuidelines.md) for the runtime/editor dependency boundary.
- The root C++ default is C++20, while some subtargets such as Archive declare a C++23 requirement. Do not describe every target as using one language standard.

## Entry points and lifecycle

### Editor

`Editor/Source/Application/Private/Main.cpp` creates `Application` and calls `Run()`. `Application::Initialize()` sets up logging, the window, Runtime/RHI, built-in resources, ImGui, localization, and systems, then attempts to load the game module and restore the last edited scene. The current Windows Editor Runtime configuration uses DirectX 12.

### Client

`Client/Source/Main.cpp` resolves the project root and reads `Config/Project.json` and startup settings through `ClientGameRuntime`. The Client host creates an SDL window and DirectX 12 RHI, then connects Runtime, systems, the render pipeline, and the startup scene.

The `Client` target and startup path exist in source, but building Client is not equivalent to verifying standalone execution from a deployment folder. See the [Client build and packaging plan](../BuildAndPackagingPlan.md) for remaining package, Release, and platform verification.

### Engine Runtime

`Engine/Source/Runtime/Private/Runtime.cpp` creates the ReflectionRegistry, ModuleManager, TimeManager, InputManager, AudioManager, ResourceManager, SceneManager, SystemManager, and rendering managers. The host calls `FixedUpdate`, `Update`, `LateUpdate`, and `Render`. The render phase coordinates system submissions and the Renderer frame begin, pass rendering, and frame end.

Shutdown releases the scene and modules before systems, resources, and the GPU Renderer, then releases the ReflectionRegistry. This order follows the lifetime dependencies between scenes, runtime modules, and GPU resources.

## Scene and resource data

- `Scene` owns an EnTT registry. Entity IDs, hierarchy, and registered component data are serialized through a Reflection-based Archive path.
- The Editor's `SceneDocument` writes a JSON Archive. The scene Archive currently records version 1 and does not promise automatic restoration of all pointers, unregistered components, or arbitrary container references.
- `ResourceManager` manages typed resource handles, slot generations, and reference counts. File-path resolution and loading follow the ResourceManager and `PlatformPath` contracts.
- Scene serialization and UI edit history are separate responsibilities. The Editor owns scene Undo/Redo; history is not persisted through the file format or scene loading. See [EditorUndoRedo.md](../EditorUndoRedo.md) for details.
- For the Archive field layout, Editor save boundary, and load validation sequence, see [Scene Persistence Architecture](ScenePersistence.en.md).

## Reflection and build outputs

`Engine/CMake/ReflectionBuild.cmake` calls `reflection_generate()` to generate reflection data for Engine headers in the build directory and attach it to the Engine target. Generated files live in the build tree rather than the source tree. The Editor consumes registrations provided by Engine.

Reflection Runtime, Generator, Archive, and the ReflectionArchive Adapter are separate projects/targets. See the [Reflection / Archive build guide](../ReflectionArchiveBuild.md) for setup and independent use, and the [source organization guidelines](../SourceOrganizationGuidelines.md) for file placement and dependency rules.

## Rendering boundary

`Renderer` manages RHI-created resources, per-render-pass submission queues, and cameras. The current default initialization path for the Editor and Windows Client is DirectX 12. The existence of another Backend in an RHI enum or platform folder does not mean the complete platform rendering path is implemented and verified.

See [Shader Binding Layout](../Shader/BINDING_LAYOUT.md) for shader resource contracts and the [Rendering Development Plan](../RenderingDevelopmentPlan.md) for rendering status.

## Verification scope

The implementation log records a user-confirmed `x64-ninja-debug` build and 44/44 CTest pass on 2026-11-20. This record applies to that check set and point in time. It must not be generalized to all recent UI changes, GPU behavior, Release, standalone Client packages, or Emscripten/WebGPU. Builds and tests were not rerun for this documentation change.

## 한국어

[한국어 아키텍처 개요](Architecture.md)
