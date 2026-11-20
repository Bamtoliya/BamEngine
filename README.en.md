# BamEngine

[![C++](https://img.shields.io/badge/C%2B%2B-20%20%2F%2023-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.21%2B-064F8C.svg)](https://cmake.org/)
[![vcpkg](https://img.shields.io/badge/vcpkg-manifest-5C2D91.svg)](https://github.com/microsoft/vcpkg)
[![Graphics](https://img.shields.io/badge/API-DirectX12%20%2F%20SDL_GPU-A41E22.svg)](#tech-stack-and-libraries)
[![Platform](https://img.shields.io/badge/Platform-Windows-informational.svg)](#environment-and-basic-setup)
[![Status](https://img.shields.io/badge/Status-WIP-orange.svg)](#current-status)

> A modular game engine and editor project using C++20 by default, with selected targets requiring C++23
> The project separates `Engine` (runtime) and `Editor` (tooling), and focuses on improving productivity through a reflection code generation pipeline.

## Screenshots

The main editor screenshot has not been added yet. See [the image guide](Docs/Images/README.md) for the expected filename.

## Documentation

- [User and developer documentation index](Docs/README.en.md)
- [Getting Started](Docs/UserGuide/GettingStarted.en.md)
- [Basic Editor Workflow](Docs/UserGuide/EditorWorkflow.en.md)
- [Architecture Overview](Docs/Technical/Architecture.en.md)

## Table of Contents

- [Screenshots](#screenshots)
- [Documentation](#documentation)
- [Overview](#overview)
- [Core Features](#core-features)
- [Tech Stack and Libraries](#tech-stack-and-libraries)
- [Project Structure](#project-structure)
- [Environment and Basic Setup](#environment-and-basic-setup)
- [Build and Run](#build-and-run)
- [Resource/Asset Pipeline](#resourceasset-pipeline)
- [Reflection Code Generation](#reflection-code-generation)
- [Current Status](#current-status)
- [Contribution Guide](#contribution-guide)

---

## Overview

`BamEngine` is designed around the following goals:

- **Engine/Editor separation**: Runtime (`Engine`) and content creation tools (`Editor`) are managed independently.
- **Data-oriented resource management**: Resource references are handled through `Handle(index + generation)`.
- **Automated reflection pipeline**: Metadata and serialization are extended through generated code.
- **Modular architecture**: Render, Resource, Physics, UI, World, and System modules are organized by responsibility.

---

## Core Features

### 1) Runtime Engine (`Engine`)

- **Rendering pipeline**: Layered structure with RenderPass / RenderTarget / RHI.
- **Shader flow**: HLSL compilation through DXC into DXIL / SPIR-V, with shader analysis and conversion tools.
- **World/object system**: EnTT-based ECS scenes and components.
- **Resource system**: Safe resource references through `ResourceManager` and `ResourceHandle<T>`.
- **Physics/collision base**: Collider/RigidBody components and collision manager structure.
- **Scene serialization**: Reflection-backed scene archives; the Editor scene document currently writes JSON.

### 2) Editor (`Editor`)

- **ImGui-based tool UI**
- **Main panels**
  - Viewport
  - Hierarchy
  - Inspector
  - Content Browser
  - ToolBar / SceneControlBar
- **Selection system**: Multi-select, deselect, and ray picking through `SelectionManager`.
- **Asset importers**: Model / Texture / Shader / Sprite / Animation import flow.
- **Localization pipeline**: Automatic CSV -> JSON conversion.

### 3) Codegen / Toolchain

- Engine reflection code generation through `Projects/Reflection`, with Archive and ReflectionArchive support in their own targets.
- Texture conversion support via `Tools/Converters/TextureConverter`.

---

## Tech Stack and Libraries

### Language / Build

- **C++20 baseline**, with C++23 requirements on selected targets
- **CMake 3.21+**
- **vcpkg manifest mode**

### Main libraries (from `vcpkg.json`)

- `fmt`: formatting/output
- `sdl3`: platform/window/input layer
- `glm`: math library
- DirectX 12 and SDL_GPU RHI paths; Vulkan SDK and shader tools are build dependencies
- `imgui`, `imguizmo`: editor UI and gizmos
- `assimp`: model import
- `stb`: image utilities
- `glslang`, `spirv-tools`, `spirv-cross`: shader/SPIR-V toolchain
- `efsw`: filesystem watching
- `cppcodec`: encoding utilities
- `directxtex`: texture processing
- `lz4`: compression

---

## Project Structure

```text
BamEngine/
├─ Engine/                  # Runtime engine
├─ Editor/                  # Editor application
├─ Client/                  # Standalone execution host
├─ Projects/                # Reflection, Archive, GameModule, Plugins, etc.
├─ Tools/                   # AssetWorker, converters, and developer tools
├─ Docs/                    # User guides, technical docs, design and verification records
└─ CMakeLists.txt
```

---

## Environment and Basic Setup

### Requirements

- Windows (based on current presets)
- Visual Studio (MSVC C++ toolchain)
- CMake 3.21+
- Python 3.x, LLVM/Clang 20.1.8, DXC, Vulkan SDK, Qt6 Widgets, and the matching FMOD SDK files
- GitHub access for the first configure (imnodes FetchContent)

### Pre-check

`CMakePresets.json` currently contains a hardcoded local value:

- `CMAKE_TOOLCHAIN_FILE`: `I:/vcpkg/scripts/buildsystems/vcpkg.cmake`

Update `CMAKE_TOOLCHAIN_FILE` in the Windows presets to match your vcpkg installation. The presets also expect the Reflection LLVM/Clang resource path and local FMOD SDK files; see the [Getting Started guide](Docs/UserGuide/GettingStarted.en.md).

---

## Build and Run

### 1) Configure

```powershell
cmake --preset x64-debug
```

### 2) Build (`Editor` target)

```powershell
cmake --build .\out\build\x64-debug --config Debug --target Editor
```

### 3) Locate executable

```powershell
Get-ChildItem -Path .\out\build\x64-debug -Recurse -Filter Editor*.exe
```

> Note  
> Non-Emscripten root configurations also add `Client` and `GameModule`; `Editor` is added when `BAMENGINE_WITH_EDITOR` is enabled (ON by default).
> The existence of the Client target does not mean standalone package deployment has been fully verified. See the [Client build and packaging plan](Docs/BuildAndPackagingPlan.md).

---

## Resource/Asset Pipeline

- Resources are loaded/registered/queried/released through `ResourceManager`.
- The `Handle(index + generation)` layout is used to reduce stale handle issues.
- Editor importers convert and register assets into engine-friendly formats.
- Localization CSV files are converted to JSON through the `GenerateLocalization` target.

---

## Reflection Code Generation

- Engine reflection code is generated at CMake stage through `reflection_generate()` in `Engine/CMake/ReflectionBuild.cmake`.
- Generated C++ files are placed in the build directory and compiled into the Engine target; the Editor consumes the Engine registry.
- Related projects include `Projects/Reflection`, `Projects/Archive`, and `Projects/ReflectionArchive`.

---

## Current Status

- This repository is **WIP (Work In Progress)**. Implemented source paths and runtime verification are tracked separately.
- The implementation log records a user-confirmed `x64-ninja-debug` build and full 44/44 CTest pass on 2026-11-20. This applies to that check set and point in time, not every current change, UI workflow, GPU path, or standalone Client package.
- See the [documentation index](Docs/README.en.md) and [implementation log](Docs/ImplementationLog.md) for feature scope and verification limits.

---

## Contribution Guide

- Issues and PRs are welcome.
- Please follow the existing C++ coding style and CMake organization.
- For large changes, open an issue first to align on design and goals.

---

## License

There is no clearly declared root-level project license file at the moment.  
Please define and confirm a license policy before redistribution or commercial use.