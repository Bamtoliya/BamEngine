# Getting Started

This guide targets the Windows development presets currently in the repository. Presets for other platforms exist, but their presence does not establish that the full engine and editor have been ported and verified there.

## Requirements

- Windows x64, MSVC tools and Windows SDK from Visual Studio 18 2026
- CMake 3.21 or later, Git, and Python 3
- vcpkg manifest mode and the packages declared by `vcpkg.json`
- LLVM/Clang 20.1.8 development files and a Clang resource directory for the Reflection Generator
- DXC and the Vulkan SDK for the shader toolchain
- Qt6 Widgets, used by the current Editor configuration
- FMOD SDK files: libraries and runtime DLLs excluded from the repository
- GitHub access during the first configure to fetch imnodes

FMOD files are not included in this repository. Prepare files matching the headers under `ThirdParty/FMOD/inc` in `ThirdParty/FMOD/lib/x64/`.

| Configuration | Required files |
|---|---|
| Debug | `fmodL_vc.lib`, `fmodL.dll` |
| Release | `fmod_vc.lib`, `fmod.dll` |

LLVM/Clang files may also be excluded from the repository. Prepare the SDK and verify that the Reflection paths in `CMakePresets.json` match its actual location.

## Set the local vcpkg path

The current Windows presets in `CMakePresets.json` contain this absolute local path:

```text
I:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

If vcpkg is installed elsewhere, update `CMAKE_TOOLCHAIN_FILE` in `windows-base` and `windows-ninja-base` to your installation path. This is a current repository configuration constraint; do not assume another developer has the same path.

## Configure and build the Editor

From the repository root, use the x64 Ninja Debug preset. An active MSVC x64 developer environment and the SDKs and local paths above are required.

```powershell
cmake --preset x64-ninja-debug
cmake --build --preset x64-ninja-debug --target Editor
```

The single-configuration Ninja preset places the executable here:

```powershell
.\out\build\x64-ninja-debug\bin\Editor.exe
```

Run the Editor from the repository root because resource paths are resolved relative to the project root. Run the automated checks after building:

```powershell
ctest --preset x64-ninja-debug --output-on-failure --no-tests=error
```

The implementation log records a user-confirmed `x64-ninja-debug` build and a full 44/44 CTest pass on 2026-11-20. That record does not guarantee a fresh build or a pass on another environment.

## Other executable targets

The root CMake configuration also adds `Client` and `GameModule` for non-Emscripten builds. `Client` is a separate execution path that uses `Config/Project.json`, startup scene/resource paths, and a project module. Building the Editor does not verify Client packaging. Check the [Client build and packaging plan](../BuildAndPackagingPlan.md) for standalone package execution, Release configuration, and platform-specific rendering verification.

## Troubleshooting

- If CMake cannot find the vcpkg toolchain, check the actual `CMAKE_TOOLCHAIN_FILE` path.
- If the Reflection Generator cannot find LLVM/Clang, check `LLVM_DIR`, `Clang_DIR`, `REFLECTION_CLANG_RESOURCE_DIR`, and the SDK installation.
- If FMOD linking fails, check that the `.lib` matching the Debug or Release configuration exists under `ThirdParty/FMOD/lib/x64/`.
- If resources fail to load after launching the Editor, verify that the working directory is the repository root.

The environment-specific installation flow was not rerun as part of this documentation change. If setup still fails, capture the complete error and the preset used.

## 한국어

[한국어 시작하기](GettingStarted.md)
