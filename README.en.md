# BamEngine

[![C++](https://img.shields.io/badge/C%2B%2B-23-blue.svg)](https://isocpp.org/)
[![CMake](https://img.shields.io/badge/CMake-3.16%2B-064F8C.svg)](https://cmake.org/)
[![vcpkg](https://img.shields.io/badge/vcpkg-manifest-5C2D91.svg)](https://github.com/microsoft/vcpkg)
[![Vulkan](https://img.shields.io/badge/API-Vulkan-A41E22.svg)](https://www.vulkan.org/)
[![Platform](https://img.shields.io/badge/Platform-Windows-informational.svg)](#environment-and-basic-setup)
[![Status](https://img.shields.io/badge/Status-WIP-orange.svg)](#current-status)

> A modular C++23 game engine + editor project  
> The project separates `Engine` (runtime) and `Editor` (tooling), and focuses on improving productivity through a reflection code generation pipeline.

## Screenshots

> Main editor screenshot is being prepared.  
> Add an image to the path below to display it in this README.

```text
Docs/Images/editor-main.png
```

![BamEngine Editor Screenshot](Docs/Images/editor-main.png)

## Table of Contents

- [Screenshots](#screenshots)
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
- **Vulkan shader flow**: Automatic compilation of `.vert/.frag -> .spv` using `glslc`.
- **World/object system**: Component-based architecture with `GameObject` and `ComponentRegistry`.
- **Resource system**: Safe resource references through `ResourceManager` and `ResourceHandle<T>`.
- **Physics/collision base**: Collider/RigidBody components and collision manager structure.
- **Serialization paths**: Json / Binary / Beve save-load routes.

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

- Automatic reflection code generation through `ReflectionCore`, `ReflectionCodegen`, and `ReflectionBamAdapter`.
- Texture conversion support via `Tools/TextureConverter`.

---

## Tech Stack and Libraries

### Language / Build

- **C++23**
- **CMake (minimum 3.16)**
- **vcpkg manifest mode**

### Main libraries (from `vcpkg.json`)

- `fmt`: formatting/output
- `sdl3` (+ `vulkan`): platform/window/input layer
- `glm`: math library
- `vulkan`: rendering backend
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
├─ Engine/                  # Runtime engine (DLL)
│  └─ Source/
│     ├─ Core/
│     ├─ Render/
│     ├─ Resource/
│     ├─ Physics/
│     ├─ World/
│     ├─ UI/
│     ├─ System/
│     └─ Generated/
├─ Editor/                  # Editor application (EXE)
│  └─ Source/
│     ├─ Application/
│     ├─ ImGui/
│     ├─ AssetManager/
│     ├─ Selection/
│     └─ Generated/
├─ Projects/
│  ├─ ReflectionCore/
│  ├─ ReflectionCodegen/
│  ├─ ReflectionBamAdapter/
│  └─ ReflectionCMake/
├─ Tools/
│  └─ TextureConverter/
├─ Resources/               # Shaders/localization/default resources
└─ CMakeLists.txt
```

---

## Environment and Basic Setup

### Requirements

- Windows (based on current presets)
- Visual Studio (MSVC C++ toolchain)
- CMake 3.16+
- Python 3.x (for reflection/localization scripts)
- Vulkan SDK (`glslc` required)

### Pre-check

`CMakePresets.json` currently contains a hardcoded value:

- `CMAKE_TOOLCHAIN_FILE`: `I:/vcpkg/scripts/buildsystems/vcpkg.cmake`

Update this path to match your local environment.

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
> Based on the root `CMakeLists.txt`, active default targets are `Engine`, `TextureConverter`, and `Editor`.  
> `Client` is currently commented out.

---

## Resource/Asset Pipeline

- Resources are loaded/registered/queried/released via `ResourceManager`.
- The `Handle(index + generation)` layout is used to reduce stale handle issues.
- Editor importers convert and register assets into engine-friendly formats.
- Localization CSV files are converted to JSON through the `GenerateLocalization` target.

---

## Reflection Code Generation

- Engine/editor reflection code is generated at CMake stage with `add_reflection_bundle(...)`.
- Generated files
  - `Engine/Source/Generated/EngineReflection.gen.cpp`
  - `Editor/Source/Generated/EditorReflection.gen.cpp`
- Related projects
  - `Projects/ReflectionCore`
  - `Projects/ReflectionCodegen`
  - `Projects/ReflectionBamAdapter`

---

## Current Status

- This repository is currently **WIP (Work In Progress)**.
- Development is actively focused on rendering, editor tooling, resource handling, and reflection pipeline expansion.

---

## Contribution Guide

- Issues and PRs are welcome.
- Please follow the existing C++ coding style and CMake organization.
- For large changes, open an issue first to align on design and goals.

---

## License

There is no clearly declared root-level project license file at the moment.  
Please define and confirm a license policy before redistribution or commercial use.