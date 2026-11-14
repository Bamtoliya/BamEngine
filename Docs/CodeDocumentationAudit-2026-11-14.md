# 코드·개발 원칙·계획 정합성 점검 (2026-11-14)

## 1. 기준과 범위

- 기준: `main`, HEAD `fabcf611` 및 점검 시점 작업 트리의 미커밋 변경(183개 변경 파일 diff 및 untracked 항목).
- 사용자 승인 범위: `.md` 문서 작성·수정. C++/CMake/vcpkg/스크립트/임시 파일은 수정·삭제하지 않았으며 빌드, CTest, GPU/UI/브라우저 실행, 패키징은 수행하지 않았다.
- 대조 기준: 루트 `AGENTS.md`, `EngineEditorClientGuidelines.md`, `SourceOrganizationGuidelines.md`, `BuildAndPackagingPlan.md`, `RenderPipelineProfilePlan.md`, `EditorPluginArchitecturePlan.md`, `DevelopmentPriorities.md`.
- 조사 범위: `2026-11-06` 점검(`ecca09da`, A01~A10) 이후 커밋 내역, 현재 미커밋 변경 4갈래(`StartupResourceLoader` 공용화, `ClientGameRuntime` 분리, Emscripten/WGSL 빌드 경계, `ThirdParty/FMOD` 갱신), 관련 셰이더/리소스/패키징/클라이언트 진입 코드와 핵심 계획 문서 정합성. 전체 엔진·에디터의 모든 실행 경로를 전수 검증한 결과가 아니다.
- 이전 점검(`F01~F13`, `A01~A10`)과 구분하기 위해 이번 신규 발견 항목은 `B01~B06`으로 관리한다.

## 2. 핵심 판단

1. `2026-11-06` 점검의 **A01~A10** 중 패키징 경로/생성 보호(A01·A02), 에셋 감시자 UTF-8/예외 경계(A03), RT 큐브맵 차원 방어(A04), 스플리터 선호값 보존(A05), 타일 컬링 디스패치 크기 일치(A06 후속), 비정상 PNG 제거(A07), 패키지 마커 기반 설정 정책(A08), 스키닝 `t0, space2` 바인딩 문서(A09), 가상 URI 선행 후 `Engine/Resources/`·`Assets/` 물리 분리(A10)는 코드와 문서에 반영되었다. 단, 다수 광원 257개 초과 폴백(A06 본체)과 일부 실행 검증은 여전히 남아 있다.
2. 현재 작업 트리는 **Windows Client 실행·패키징 안정화**에서 **WebGPU/Wasm Client 이식 준비**로 넘어가는 과도기 상태다. Windows 네이티브 경로의 `ClientGameRuntime` 분리와 `StartupResourceLoader` 공용화는 방향이 타당하나, 아직 Emscripten/WebGPU 툴체인으로 컴파일·실행 검증되지 않았다.
3. 특히 WebGPU 전환 경로에서 `StartupResourceLoader`↔`Shader` 간 WGSL 경로 단절(**B01**), `ShaderImporter`의 구형 `Resources/Shader/` 하드코딩(**B02**), `ClientGameRuntime`의 `NetworkSystem`/FMOD/DXIL 고정 의존성(**B03**)이 정적 대조로 확인되었으므로 Emscripten Client 타깃 연결 전에 먼저 정리해야 한다.

## 3. 2026-11-06 이후 반영 현황 및 작업 트리 상태

### 3.1 커밋 기준 반영 현황 (`ecca09da..fabcf611`)

- **선행 결함 해소 및 CTest 회귀 복구**: CTest 29/29 통과 복구(2026-11-07 사용자 확인 기록), A01~A05, A06 후속, A07, A08, A09 반영.
- **트랙 A·B·C 완결 (2026-11-07~08)**:
  - **S2-D 텍스처 밉맵**: `stb_image_resize2` 기반 CPU 밉맵 체인 생성, D3D12 다중 서브리소스 업로드, `TextureMipmapCheck` 추가. (오프라인 BC7/DDS 베이킹은 후속 범위)
  - **자동 메시 LOD**: `meshoptimizer`(`meshopt_simplify`) 기반 임포트 시 자동 LOD 생성 및 `ModelEditor` 실시간 LOD 검사기 연동.
  - **리소스·애셋 분리(Phase 1~3-B)**: `Engine://`, `Editor://`, `Project://` 가상 URI 해석 계층(`PlatformPath::ResolveUri`) 적용 후 `Engine/Resources/`와 `Assets/` 물리 분리 및 에디터 읽기 전용 보호 완료.
- **트랙 D·E·F·G 확장 및 완료 (2026-11-08~11)**:
  - **2D 제작 파이프라인**: 스프라이트 3대 슬라이서·원클릭 `.spriteanim` 생성, 9-Slice/Tiled/Filled 모드, 픽셀 퍼펙트 카메라 및 1:1 뷰포트 표시, 16×16 청크 타일맵·4방향 자동 타일(AutoTile) 규칙/브러시, MaxRects 아틀라스 배치 기반(`SpriteAtlas`, `SpriteBatcher`). (실제 드로우 배칭 연결·계측은 Phase 5 잔여)
  - **공용 에디터 DLL 플러그인(`PluginHost`)**: `Plugins.txt` 개발 타깃, `BuiltinPlugins`/`Plugins`의 `plugin.json` 발견, `PluginApprovals.json` SHA-256 승인 및 섀도 복사 핫리로드, `GameplayScore.dll` 런타임 예제 연결.
  - **복합 비주얼 이펙트(`CompositeEffect`) 및 타임라인**: Phase 1~4(스프라이트, 파티클 물리 충돌/컬 노이즈, 라이트, 메쉬 FX, 사운드 길이/페이드아웃, 트레일 리본, 데칼, 카메라 셰이크) 및 공용 타임라인 위젯 연동.
  - **비정상 종료 진단·복원**: `CrashHandler_Win32.cpp`(SEH/CRT, 미니덤프, `.crash.json` 콜스택)와 `RecoveryManager`(하트비트 스냅샷, `Session.lock`, 주 뷰포트 중앙 복원 팝업) 연결.
- **에디터 공용 UI·렌더 프로필·패키징·Web 프리뷰 (2026-11-09~13)**:
  - **공용 UI/리소스 편집기**: `ResourceEditorInterface` 3줄 공통 헤더(12개 편집기 전환), `EditorUI` 공통 검색 바·모달 버튼 바·반응형 툴바·테이블 확대.
  - **Deferred Render Pipeline Profile**: `Engine/Resources/RenderPipelines/Deferred.bamrenderpipeline.json`, 공용 검증기(`RenderPipelineProfile`), Game View 및 Windows Client(`ClientRenderPipeline`) 연결, 스키닝 그림자 및 하늘 앞 파티클 합성 보강.
  - **에디터 내장 Windows 패키저**: `Editor/Source/Build/Private/ClientPackage.cpp`에서 씬 참조 검증, 스테이징·백업·원자적 게시, `.bam-client-package` 마커, `[Package inventory]` 비교 로그, 빌드 워커 SDL TLS 정리(`SDL_CleanupTLS()`) 연결.
  - **독립 WebGPU 씬 프리뷰**: `Client/Web/`에 `.bamscene.json` v1 정적 프리뷰 및 `Tools/WebGPUProbe/main.cpp` 추가.

### 3.2 현재 작업 트리 미커밋 변경 분류 (4갈래)

1. **`StartupResourceLoader` 공용화 및 패키저 연동**:
   - `Editor/Source/AssetManager/.../StartupResourceLoader.*` 삭제 -> `Engine/Source/Resource/Public|Private/StartupResourceLoader.*` 신설.
   - `Compute` 셰이더 타입 지원, `CollectPipelineFiles()` 검증 함수 추가, `Application.cpp`·`ClientGameRuntime.cpp`·`ClientPackage.cpp` 연결.
2. **`ClientGameRuntime` 실행 계층 분리**:
   - `Client/Source/Execution/Public|Private/ClientGameRuntime.*` 신설 및 `Client/CMakeLists.txt`, `Client/Source/Main.cpp` 분리(Windows SDL/HWND/D3D12 호스트와 게임 실행 계층 분리).
3. **Emscripten 빌드 경계 및 WGSL sidecar 경로 초안**:
   - `CMakeLists.txt`, `Engine/CMakeLists.txt`, `Engine/CMake/Dependencies.cmake`, `Engine/CMake/ReflectionBuild.cmake`, `Projects/Reflection/CMakeLists.txt`, `vcpkg.json`: `EMSCRIPTEN` 조건에서 Engine 정적 라이브러리 전환 및 `DirectX12`/`SDLGPU`/`Vulkan`/`CrashHandler_Win32`/`ShaderCompiler`/`Network`/`RLServer` 제외, 호스트 `ReflectionGenerator` 강제.
   - `Engine/CMake/ShaderBuild.cmake`, `Shader.h/.cpp`, `ShaderCompiler.h/.cpp`, `ResourceManager.cpp`, `ShaderImporter.cpp`, `ShaderHotReloadManager.cpp`: `ERHIType::WebGPU`, `wgslPath`, 호스트 Tint(`TINT_EXECUTABLE`) 기반 SPIR-V → WGSL sidecar 변환 초안.
   - `Tools/WebGPUProbe/CMakeLists.txt`: `emdawnwebgpu` 포트 단독 프로브 CMake 추가.
4. **FMOD SDK 갱신 및 임시/테스트 파일**:
   - `ThirdParty/FMOD/inc/` 헤더가 2.03.15(`FMOD_VERSION 0x00020315`, `FMOD_BUILDNUMBER 168126`)로 갱신되고 `ThirdParty/FMOD/examples/`가 삭제되었으며, `ThirdParty/FMOD/lib/`(`arm64`, `js`, `w32`, `wasm`, `x64`, `x86`)가 추가(untracked)됨.
   - `.gitignore`에 CTest/테스트 부산물(`managed-scene.json`, `test-*.json` 등) 추가.
   - 저장소 루트에 0바이트 untracked 파일 `cd`, `emcc`, `emcmake` 존재.

## 4. 신규 결함·위험·문서 불일치 (`B01~B06`)

### B01 — 보통 / `StartupResourceLoader`와 `Shader` 간의 WebGPU(WGSL) 경로 연결 단절

- 위치:
  - `Engine/Source/Resource/Private/StartupResourceLoader.cpp:30~40, 138~143, 172~176, 319~328`
  - `Engine/Source/Render/Shader/Private/Shader.cpp:84~86, 111~139`
- 현재 동작:
  - `ShaderItemConfig`에 `DxilPath`, `SpirvPath`만 존재하며 `WgslPath` 필드가 없다.
  - `GetStartupShaderPath(item, rhiType)`는 `rhiType == ERHIType::SDLGPU && !item.SpirvPath.empty()`일 때만 `SpirvPath`를 반환하고, 그 외 백엔드(신규 `ERHIType::WebGPU` 포함)에서는 `DxilPath`(`.cso`)를 반환한다.
  - `LoadShaders()`는 `ShaderDesc`에 `desc.Path`만 채우고 `desc.dxilPath`, `desc.spirvPath`, `desc.wgslPath`를 채우지 않는다.
  - `Shader::ResolveLegacyBinaryPaths()`는 `m_Path` 확장자가 `.spv`일 때만 `m_SpirvPath`를 채우고 그로부터 `bin/wgsl/*.wgsl`을 자동 유도한다.
- 근거 및 영향: `ERHIType::WebGPU`로 `StartupResourceLoader::LoadPipeline()`이나 `CollectPipelineFiles()`를 호출하면 `.wgsl`이나 `.spv`가 아니라 `.cso` 경로를 읽고, `Shader::BuildRHIShaderDesc()`는 빈 `m_WgslPath`를 반환해 WebGPU 셰이더 로드/사전 검증이 실패한다.
- 최소 개선: `ShaderItemConfig`에 선택적 `WgslPath`를 추가하고 `GetStartupShaderPath()`에 `ERHIType::WebGPU` 분기(`WgslPath` 우선, 없으면 `SpirvPath` 기반 `bin/wgsl/*.wgsl` 유도 또는 `SpirvPath` 반환)를 연결한다. `LoadShaders()`에서는 `desc.dxilPath`, `desc.spirvPath`, `desc.wgslPath`를 함께 채워 `Shader::BuildRHIShaderDesc()`와 일관되게 맞춘다.
- 완료 조건: `DirectX12`, `SDLGPU`, `WebGPU` 각 `ERHIType`에 대해 `CollectPipelineFiles()`와 `LoadShaders()`가 의도한 바이너리 확장자(`.cso`, `.spv`, `.wgsl`)와 `Shader` 경로 필드를 올바르게 채우는지 확인한다.

### B02 — 보통 / `ShaderImporter.cpp`의 구형 `Resources/Shader/...` 경로 하드코딩 잔류

- 위치: `Editor/Source/AssetManager/Importer/Private/ShaderImporter.cpp:64~73` (참고: `Client/Source/ClientRenderPipeline.cpp:71` 구형 키 `L"Resources/Material/UIMaterial.bammat.json"`).
- 현재 동작: `ShaderHotReloadManager.cpp:318~336`은 `PlatformPath::ToVirtualUri(hlslPath)`(폴백 `Engine://Shader/...`)와 `result.OutputPath`를 사용하지만, `ShaderImporter::Import()`의 `.hlsl` 분기는 `desc.Key = L"Resources/Shader/" + stemW`, `desc.dxilPath = L"Resources/Shader/bin/dxil/" + ...`, `desc.spirvPath = L"Resources/Shader/bin/spirv/" + ...`, `desc.wgslPath = L"Resources/Shader/bin/wgsl/" + ...`로 구형 물리 경로를 하드코딩한다. (`rhiType == ERHIType::WebGPU`일 때만 `desc.wgslPath`를 `compileResult.OutputPath`로 덮어쓴다.)
- 영향: Phase 3-B 이후 실제 셰이더 파일은 `Engine/Resources/Shader/`에 위치하므로, `.hlsl` 직접 임포트 시 키와 출력 경로가 `Engine://Shader/...` 및 실제 `Engine/Resources/Shader/bin/...` 산출물 경로와 어긋난다.
- 최소 개선: `ShaderImporter::Import()`에서도 `PlatformPath::ToVirtualUri(sourcePath)`(폴백 `Engine://Shader/` + `stemW`)와 `compileResult.OutputPath`(및 동일 `bin/` 루트 상대 경로)를 사용하도록 정규화한다.
- 완료 조건: HLSL 임포트와 핫 리로드가 생성하는 `Shader` 키 및 `dxilPath`/`spirvPath`/`wgslPath`가 동일한 `Engine://Shader/...` 규약과 실제 출력 파일을 가리키는지 확인한다.

### B03 — 보통 / `ClientGameRuntime`의 시스템·셰이더 의존성과 Emscripten 빌드 경계 불일치

- 위치:
  - `Client/Source/Execution/Private/ClientGameRuntime.cpp:200~223`
  - `Client/Source/ClientRenderPipeline.cpp:44~54`
  - `Engine/CMakeLists.txt:23~33, 129~147`
- 현재 동작 및 근거:
  1. `Engine/CMakeLists.txt:28`은 `EMSCRIPTEN` 빌드에서 `/Network/` 소스를 제외하지만, 플랫폼 중립 계층으로 분리한 `ClientGameRuntime::InitializeSystems()`(line 217)는 `Engine::NetworkSystem`을 무조건 등록한다.
  2. `Engine/CMakeLists.txt`는 `Audio/`(`AudioManager`, `AudioClip`, `AudioSystem`) 및 `CompositeVFXSystem`의 FMOD 호출을 Emscripten에서 제외하지 않으면서, FMOD 라이브러리 링크는 `if(WIN32)` 블록(lines 144~145)에만 두고 `ThirdParty/FMOD/lib/w32/`나 `wasm/` 링크를 연결하지 않았다.
  3. `ClientRenderPipeline::Initialize()`(lines 44~54)는 렌더 프로필을 사용할 때 `Shader/bin/dxil` 아래의 `.cso` 파일을 고정 로드한다.
- 영향: 현 상태 그대로 Emscripten용 Client 타깃을 연결하면 `NetworkSystem` 심볼 누락, FMOD 링크 오류 또는 오디오 미정의 심볼, DXIL 고정 셰이더 로드 실패가 순차적으로 발생한다.
- 최소 개선: Emscripten Client 구성 전에 (1) `NetworkSystem` 등록의 조건부 제외 또는 플랫폼 중립 null/stub 제공, (2) Emscripten용 FMOD Wasm 라이브러리 링크 또는 오디오 백엔드 경계 분리, (3) `ClientRenderPipeline`의 RHI 타입별 셰이더 경로 선택을 결정한다.
- 완료 조건: Emscripten 환경에서 `Engine` 정적 라이브러리와 최소 Web Client 타깃이 링크 오류 없이 구성·빌드되는지 확인한다.

### B04 — 낮음 / 작업 트리 내 0바이트 임시 파일 잔존 및 다목적 변경 혼재

- 위치: 저장소 루트 `cd`, `emcc`, `emcmake` (각 0바이트 untracked), 및 현재 미커밋 변경 전체(183개 파일 diff + untracked 항목).
- 근거: 쉘 작업 중 생성된 0바이트 파일 3개가 루트에 남아 있고, FMOD 2.03.15 SDK 교체, `StartupResourceLoader` 공용화, `ClientGameRuntime` 분리, Emscripten/WGSL 빌드 경계 초안이 커밋 없이 한 작업 트리에 섞여 있다.
- 영향: `FMOD_COMMON_H`의 콜백 플래그(`FMOD_SYSTEM_CALLBACK_MIDMIX` 제거 등) 및 구조체 필드(`FMOD_ADVANCEDSETTINGS`) 변경이 Windows 기존 빌드에 미치는 영향을 다른 변경과 분리해 검증하기 어려워진다.
- 최소 개선: 루트 0바이트 파일(`cd`, `emcc`, `emcmake`)을 삭제하고, 기능/SDK 변경을 논리 단위(FMOD 2.03.15 갱신, StartupResourceLoader 공용화, ClientGameRuntime 분리, Emscripten/WGSL 초안)로 나누어 빌드 확인 후 커밋한다.

### B05 — 낮음 / 독립 Client의 `CrashHandler` 미연결 및 구형 `PackageClient.ps1` 병존

- 위치: `Client/Source/Main.cpp`, `Editor/Source/Build/Private/ClientPackage.cpp`, `Tools/Scripts/PackageClient.ps1`.
- 현재 동작:
  - `CrashHandlingAndRecoveryPlan.md` 8절의 미연결 항목처럼 `Client/Source/Main.cpp`에는 `CrashHandler::Initialize()`/`Free()` 호출이 아직 연결되어 있지 않다.
  - 에디터 내장 패키저(`ClientPackage.cpp`)는 `Assets/`와 `Engine/Resources/` 분리 구조, `.bam-client-package`, Render Pipeline Profile 검증을 사용하지만, `Tools/Scripts/PackageClient.ps1:120~148`은 여전히 구형 `Resources/` 하위 폴더 구조를 복사한다.
- 영향: 사용자가 구형 PowerShell 스크립트를 현행 패키징 경로로 오인할 수 있고, 배포 Client 크래시 시 미니덤프가 남지 않는다.
- 최소 개선: 문서에서 Windows 공식 배포 경로를 Editor의 `ClientPackage.cpp`로 명시하고 `PackageClient.ps1`의 상태를 구형 참고용/정리 대상으로 구분한다. Client `CrashHandler` 연결은 별도 작은 작업으로 추적한다.

### B06 — 보통 / 핵심 계획 문서의 기준일 및 최신 현황 미갱신 (이번 작업에서 해소)

- 위치: `Docs/DevelopmentPriorities.md`, `Docs/RenderingDevelopmentPlan.md`, `Docs/EngineDevelopmentPlan.md`, `Docs/ImGuiDevelopmentPlan.md`, `Docs/BuildAndPackagingPlan.md`.
- 근거: `DevelopmentPriorities.md` 상단 기준일이 `2026-11-06`에 머물러 있어 11월 7~14일에 반영된 에셋 분리, 밉맵, 자동 LOD, 2D 시스템, 복합 VFX, 크래시 복원, DLL 플러그인, 렌더 프로필, WebGPU/Emscripten 초안이 진입 문서와 기준 문서 목록에 빠져 있었다.
- 조치: 이번 문서 점검 작업에서 위 문서들의 상단 상태, 기준 문서 목록, 우선순위(11절) 및 구현 로그(`ImplementationLog.md`)를 2026-11-14 기준으로 동기화했다.

## 5. 미검증 범위 및 사용자 확인 안내

이번 점검은 정적 코드·설정·문서 대조에 한정되며, 아래 항목은 사용자 환경에서 직접 실행·검증이 필요하다.

1. **작업 트리 임시 파일 정리 (선택 사항)**:
   ```powershell
   Remove-Item -LiteralPath "cd", "emcc", "emcmake" -WhatIf
   ```
   `-WhatIf`로 0바이트 임시 파일 대상을 먼저 확인한 뒤 필요 시 `-WhatIf`를 제거하여 삭제한다.

2. **Windows Debug/Release 빌드 및 CTest (FMOD 2.03.15·`StartupResourceLoader`·`ClientGameRuntime` 회귀 확인)**:
   ```powershell
   cmake --preset x64-debug -DBUILD_TESTING=ON
   cmake --build out/build/x64-debug --config Debug --parallel -- /p:UseMultiToolTask=true /p:EnforceProcessCountAcrossBuilds=true
   ctest --test-dir out/build/x64-debug -C Debug --parallel --output-on-failure
   ```
   - **2026-11-14 Debug 확인 완료 (사용자 실행)**: `Engine/CMake/Dependencies.cmake`의 `spirv_cross_core`~`reflect`→`spirv_cross_c` 순서 및 `if(NOT EMSCRIPTEN)` 스코프 복구 후 `x64-debug` CMake 구성·멀티코어 빌드 및 CTest **36/36 통과(100% tests passed, 0 failed, 5.03 sec)** 확인 완료.
   - **남은 Windows 검증**: `x64-release` 빌드 확인, 실행 시 FMOD 2.03.15 오디오 초기화/재생, `RenderPipelineProfilePlan.md` 2·3단계 수동 검증(Game View vs 저장소 밖 Client 패키지 `RaytracingScene3` 렌더링 비교, Fox 스키닝 그림자 및 하늘 앞 파티클, `SDL_CleanupTLS()` 종료 시 누수 덤프 해소)은 별도 수동 확인이 필요하다.

3. **Emscripten / Tint 툴체인 검증 (도구 설치 후)**:
   - Emscripten SDK(`emcc`, `emcmake`)와 호스트 `Tint`(`TINT_EXECUTABLE`)를 준비한 뒤, `Tools/WebGPUProbe` 독립 빌드/브라우저 실행과 호스트 WGSL 변환(`BAMENGINE_BUILD_WGSL_SHADERS=ON`), 호스트 `ReflectionGenerator`를 지정한 `Engine` Emscripten 정적 라이브러리 구성을 순서대로 검증한다.
