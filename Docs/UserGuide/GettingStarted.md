# 시작하기

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

## 엔진 구성과 제작 흐름

| 구성 | 역할 |
|---|---|
| Engine | ECS/Scene, 리소스, 그래픽스, 물리, 입력, 애니메이션, UI 등 공통 런타임 |
| Editor | 콘텐츠 제작, Inspector, 그래프, 리소스 편집, PIE, 빌드 프로필 |
| Client | 에디터 없이 프로젝트를 실행하는 애플리케이션 |
| 프로젝트 모듈 | 개별 게임의 C++ 규칙 및 확장 기능을 담는 DLL |
| Reflection / Archive | 타입 정보와 속성 편집, 저장·복원, 그래프 연동의 기반 |

일반 제작 흐름:

```text
엔진 준비 → Project Hub → 새 프로젝트 → 에셋 임포트
→ 씬 구성 → 컴포넌트 추가 → 그래프 또는 C++ 로직
→ PIE 테스트 → 프리팹 및 콘텐츠 확장 → Build Profile
→ Client 독립 실행 검증
```

게임별 코드를 `Engine/Source`에 바로 추가하지 말고 프로젝트 모듈에 배치하는 방향을 우선한다. `Editor` 의존 기능은 게임의 독립 Client에서 실행될 것을 전제로 하지 않는다.

## 의존성 준비

기존 GettingStarted 문서 및 CMakePresets 기준:

- Visual Studio 2026 MSVC x64 도구, Windows SDK, CMake 3.21 이상(프로젝트 모듈 생성 CMake는 3.24 이상), Ninja, Git, Python 3
- vcpkg manifest 및 저장소의 `vcpkg.json` 의존성
- LLVM/Clang 20.1.8 개발 파일, Clang resource directory
- DXC, Vulkan SDK 및 Qt6 Widgets
- FMOD SDK(저장소에 포함되지 않는 바이너리), 최초 의존성 다운로드용 네트워크 연결

```powershell
git clone https://github.com/Bamtoliya/BamEngine.git
cd BamEngine
```

재현을 원할 때만 별도 작업 디렉터리에서 기준 커밋을 checkout 한다. 평상시 최신 main을 사용하는 개발 환경에서는 작업 중 변경을 먼저 확인한다.

```powershell
git status
git rev-parse HEAD
```

`CMakePresets.json`의 `windows-base`, `windows-ninja-base`에는 로컬 절대 경로가 들어 있다. 특히 `I:/vcpkg/scripts/buildsystems/vcpkg.cmake`와 Visual Studio Community의 Ninja 경로를 실제 설치 위치에 맞춘다. 개인 경로를 공유 설정에 무심코 커밋하지 않는다. `LLVM_DIR`, `Clang_DIR`, `REFLECTION_CLANG_RESOURCE_DIR`도 확인한다.

FMOD 파일을 `ThirdParty/FMOD/lib/x64/`에 준비한다.

| 빌드 구성 | 파일 |
|---|---|
| Debug | `fmodL_vc.lib`, `fmodL.dll` |
| Release | `fmod_vc.lib`, `fmod.dll` |

## Visual Studio 사용

1. Visual Studio 2026에서 저장소 루트를 CMake 프로젝트로 연다.
2. `x64-ninja-debug` 또는 `x64-debug` 프리셋을 선택한다.
3. configure 결과를 확인하고 시작 대상에 `Editor`를 선택한다.
4. 저장소 루트 기준으로 실행한다. 리소스나 프로젝트 허브 초기화에 문제가 있으면 작업 디렉터리와 로그를 확인한다.

## 명령행 사용

MSVC x64 개발 환경이 활성화된 터미널에서:

```powershell
cmake --preset x64-ninja-debug
cmake --build --preset x64-ninja-debug --target Editor
.\out\build\x64-ninja-debug\bin\Editor.exe
```

테스트 타깃까지 빌드한 후:

```powershell
cmake --build --preset x64-ninja-debug
ctest --preset x64-ninja-debug --output-on-failure --no-tests=error
```

Ninja Release 및 Visual Studio 프리셋도 존재한다. Linux/macOS 프리셋의 존재만으로 모든 플랫폼 실행이 검증되었다고 보지 않는다.

## 관련 문서 및 소스

- [GettingStarted.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/UserGuide/GettingStarted.md)
- [EngineEditorClientGuidelines.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/EngineEditorClientGuidelines.md)
- [`CMakePresets.json`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/CMakePresets.json) — `CMakePresets.json`
- [전체 근거 및 검토 범위](Sources.md)

