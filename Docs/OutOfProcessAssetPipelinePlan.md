# 독립 워커 프로세스 기반 대용량 에셋 임포트 파이프라인 개발 계획
(Out-of-Process Asset Pipeline & Worker Isolation Plan)

- **작성일**: 2026-11-16 (최근 갱신: 2026-11-18)
- **상태**: 3D/메시/프리팹 썸네일 독립 워커 프로세스 베이킹 구현 완료 (`BamAssetWorker`, `AssetWorkerHost`), 에셋 임포트 파이프라인 확장은 설계 중
- **공통 기준**: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md), [BuildConfigurationAndPreprocessing.md](BuildConfigurationAndPreprocessing.md)
- **연계 문서**:
  - [개발 우선순위 (DevelopmentPriorities.md)](DevelopmentPriorities.md) 11절
  - [크래시 진단 및 복원 계획 (CrashHandlingAndRecoveryPlan.md)](CrashHandlingAndRecoveryPlan.md)
  - [에디터 사용성 테스트 지침 (EditorUXTestGuidelines.md)](EditorUXTestGuidelines.md) 4.6절
- **핵심 목표**: 대용량 3D 모델(수백 MB의 FBX/OBJ/GLTF), 고해상도 텍스처(4K/8K BC7 압축), 복합 에셋 일괄 임포트 시 **에디터 메인 렌더 스레드의 60 FPS 무간섭 유지** 및 **임포트 중 OOM(메모리 부족)·크래시로부터 에디터 프로세스를 100% 완전 격리**하는 독립 워커 프로세스 아키텍처를 구축한다.

---

## 1. 배경 및 문제 정의 (Motivation & Problem Definition)

### 1.1 현재 인-프로세스(In-Process) 임포트의 한계
현재 `AssetManager::ImportAsync`는 `std::async(std::launch::async)`를 통해 에디터 프로세스 내부의 백그라운드 스레드에서 에셋 변환을 수행하고 있다. 그러나 상용 수준의 대규모 프로젝트 에셋을 다룰 때 다음과 같은 구조적 한계에 직면한다:

1. **CPU 코어 경합 및 프레임 드랍 (CPU Starvation & Hitching)**:
   - Assimp 기반 FBX 파싱, 정점 탄젠트 공간 생성(`meshoptimizer`), 텍스처 밉맵 생성 및 BC7/ASTC 블록 압축은 모든 가용 CPU 코어를 100% 풀로드로 소모한다.
   - 백그라운드 스레드가 메인 렌더 루프(DirectX 12 커맨드 큐 제출)와 동일한 코어/L3 캐시를 두고 경합하여 뷰포트 조작 시 심각한 프레임 스파이크(10~50ms 지연)를 유발한다.
2. **에디터 힙 메모리 단편화 및 OOM 크래시 전파**:
   - 수백만 폴리곤의 고밀도 메쉬나 8K 텍스처 파싱 시 일시적으로 수 GB의 메모리 할당/해제가 반복된다.
   - 단일 프로세스 힙을 공유하므로 메모리 단편화가 누적되며, 대형 에셋 파싱 중 메모리 부족(`std::bad_alloc`)이나 C++ 예외/접근 위반이 발생하면 **에디터 본체 프로세스가 작업 저장 기회도 없이 동반 종료(Crash-to-Desktop)**된다.
3. **상용 엔진 표준 패턴과의 격차**:
   - Unreal Engine(`ZenServer`, `ShaderCompileWorker`, `CookWorker`), Unity(`Asset Pipeline v2`) 등 상용 엔진은 무거운 컴파일과 에셋 변환을 에디터 본체와 분리된 **외부 독립 서브프로세스(Worker)**로 완전히 격리하여 운영한다.

---

## 2. 전체 아키텍처 개요 (System Architecture)

```mermaid
flowchart TD
    subgraph EditorHost ["에디터 본체 프로세스 (Editor.exe) - 60 FPS 유지"]
        CB["ContentBrowser (드래그 앤 드롭 / 임포트 요청)"]
        AWM["AssetWorkerHost (워커 프로세스 풀 관리자)"]
        JobObj["Windows Job Object (에디터 종료 시 워커 자동 정리)"]
        FooterBar["하단 푸터 바 (실시간 진행률 프로그레스 / 취소 버튼)"]
        
        CB -->|"1. ImportRequest (JSON/IPC)"| AWM
        AWM --> JobObj
        AWM -.->|"4. 실시간 상태 브로드캐스트"| FooterBar
    end

    subgraph IPCChannel ["프로세스 간 통신 (Anonymous Pipes / Stdio Streaming)"]
        PipeIn["표준 입력 (stdin): 변환 작업 요청"]
        PipeOut["표준 출력 (stdout): 진행률(Progress) / 결과 JSON"]
        PipeErr["표준 에러 (stderr): 변환 실패 로그 스트리밍"]
    end

    subgraph WorkerPool ["독립 워커 프로세스 (BamAssetWorker.exe)"]
        Worker1["Worker #1 (Model Converter: Assimp + meshopt)"]
        Worker2["Worker #2 (Texture Converter: DirectXTK + BC7)"]
        WorkerN["Worker #N (Audio / Font / Shader Converter)"]
    end

    subgraph DiskOutput ["디스크 산출물 (바이너리 캐시)"]
        MeshBin[".bammesh (최적화 기하 버퍼)"]
        TexBin[".bamtex (DDS / 압축 밉맵)"]
    end

    AWM --> PipeIn
    PipeIn --> WorkerPool
    WorkerPool --> PipeOut
    WorkerPool --> PipeErr
    PipeOut --> AWM
    Worker1 --> MeshBin
    Worker2 --> TexBin
    AWM -->|"5. 렌더 스레드 안전 자원 등록"| EngineAssetCache[("AssetCache / ResourceManager")]
```

---

## 3. 세부 설계 (Detailed Design)

### 3.1 독립 CLI 워커 바이너리 (`BamAssetWorker.exe`)
- **프로젝트 위치**: `Tools/AssetWorker/`
- **의존성**: `BamTools::ModelConverter`, `TextureConverter`, `Core`, `Archive` (렌더러 및 ImGui 제외한 순수 헤드리스).
- **실행 모드**:
  1. **단발성 CLI 모드 (Batch/One-shot)**:
     `BamAssetWorker.exe --type model --source Hero.fbx --dest Content/Hero.bammesh --preset Default`
  2. **상주형 데몬 모드 (Daemon / Pipe Worker)**:
     `BamAssetWorker.exe --daemon`
     - 프로세스를 띄워둔 채 `stdin`으로 JSON 작업 명세를 연속 수신하고, 완료 시 `stdout`으로 결과를 응답하여 프로세스 스폰 비용(Spawn Overhead) 최소화.

### 3.2 IPC 프로토콜 명세 (JSON over Standard I/O)
- **작업 의뢰 (Editor -> Worker, stdin)**:
  ```json
  {
    "taskId": 1042,
    "type": "Model",
    "sourcePath": "I:/Assets/Dragon.fbx",
    "outputPath": "Content/Models/Dragon.bammesh",
    "options": {
      "generateLOD": true,
      "lodCount": 3,
      "optimizeVertexCache": true,
      "scale": 1.0
    }
  }
  ```
- **진행률 피드백 (Worker -> Editor, stdout)**:
  ```json
  { "taskId": 1042, "status": "Progress", "progress": 0.45, "step": "Generating Tangents" }
  ```
- **완료 보고 (Worker -> Editor, stdout)**:
  ```json
  {
    "taskId": 1042,
    "status": "Success",
    "outputPath": "Content/Models/Dragon.bammesh",
    "generatedSubmeshes": 8,
    "vertexCount": 124500,
    "elapsedMs": 412.5
  }
  ```

### 3.3 에디터 측 호스트 (`AssetWorkerHost`)
- **수명주기 및 프로세스 풀 관리**:
  - 시스템 CPU 코어 수에 맞추어 워커 프로세스 상한 관리 (`MaxWorkers = max(1, CoreCount - 2)`).
  - 에디터 메인 스레드와 렌더 스레드를 보호하기 위해 최소 2개 코어는 비워둠.
- **Windows Job Object 연동 (좀비 프로세스 원천 방지)**:
  - `CreateJobObjectW`를 생성하고 `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE` 플래그를 설정.
  - 에디터가 정상 종료되거나 의도치 않게 크래시되어도, OS가 자식 워커 프로세스들을 100% 즉시 강제 회수.
- **스레드/프로세스 우선순위 격하**:
  - `CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS` 플래그로 스폰하여 에디터 반응성 절대 우선 보장.

### 3.4 장애 격리 및 안전장치 (Fault Isolation & Safety Guard)
1. **OOM / Crash 격리**:
   - 워커 프로세스가 대형 에셋 파싱 중 크래시(`0xC0000005`) 또는 OOM으로 비정상 종료되어도 에디터 프로세스는 살아남으며, `AssetWorkerHost`가 비정상 종료 코드(`ExitCode != 0`)를 수신하여 UI에 `"임포트 실패 (프로세스 오류)"` 알림만 표시.
2. **타임아웃 감지 (Hang Guard)**:
   - 3분 이상 응답이 없는 불량 에셋(무한 루프 등)은 에디터 측에서 워커 프로세스를 안전하게 강제 종료(`TerminateProcess`)하고 타임아웃 처리.
3. **인-프로세스 폴백(Fallback) 보존**:
   - 워커 바이너리가 빌드되지 않았거나 시스템 환경상 서브프로세스 스폰이 제한되는 경우 기존의 `AssetManager::ImportAsync` 인-프로세스 경로로 투명하게 폴백.

### 3.5 대용량 3D 에셋 썸네일 & 프리뷰 베이킹 확장 (3D Thumbnail & Preview Baking — 구현 완료)
- **필요성**:
  - `Sponza.obj`(21MB), `Sponza.bammodel`(1,143개 서브메시 참조), `dragon.bammesh`(198MB), 1,023개 개별 `.bammesh`, 프리팹(`.bamprefab.json`) 등을 에디터 메인 스레드에서 즉시 파싱/렌더링하면 UI가 프리징됨.
  - 이를 에디터 프로세스와 완전히 격리된 독립 워커 프로세스로 위임하여 에디터 메인 스레드 부하 0.0ms를 달성함.
- **독립 헤드리스 썸네일 워커 (`BamAssetWorker.exe`)**:
  - 위치: `Tools/AssetWorker/` (`main.cpp`, `HeadlessThumbnailRasterizer.h/.cpp`)
  - 지원 포맷: `.bammesh` / `.bammesh.json`, `.bammodel` / `.bammodel.json`, `.obj` / `.fbx` / `.gltf` / `.glb`, `.bamprefab` / `.prefab` / `.json`
  - 단발성 CLI(`--mode thumbnail --source <src> --output <png> [--resolution <dim>] [--project-root <dir>]`) 및 파이프 기반 상주형 데몬(`--daemon`) 지원 (`THUMB\t<id>\t<res>\t<src>\t<dst>` -> `RESULT\t<id>\t<ok>`).
  - RHI/SwapChain 의존성 없는 헤드리스 2x SSAA (`256x256` -> `128x128`) 소프트웨어 PBR 래스터라이저 + 3점 스튜디오 조명 + ACES 톤매핑 + 알베도 텍스처 샘플링(`.bamtex`, `.dds`, `.png`, `.tga`, `.jpg`).
  - 대용량 복합 모델은 최경량 LOD(`.bammesh` LOD2) 우선 선택, 198MB 초대형 단일 메시는 정점 분포/AABB를 유지한 삼각형 스텝 샘플링으로 수십 ms 내에 처리.
- **에디터 호스트 (`AssetWorkerHost`) 및 `AssetCache` 통합**:
  - 위치: `Editor/Source/AssetManager/Worker/` (`AssetWorkerHost.h/.cpp`)
  - `Windows Job Object`(`JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`) + `CREATE_NO_WINDOW | BELOW_NORMAL_PRIORITY_CLASS` + 2개 데몬 프로세스 풀 운영.
  - `AssetCache`는 `_v2` 디스크 캐시 시드를 사용하며, 캐시 미스 시 3D 모델·메시·프리팹 베이킹을 `AssetWorkerHost`로 위임하고, 완료된 PNG를 `m_DecodeWorkers`로 비동기 로드해 GPU 텍스처로 업로드. 워커 미사용 시 인-프로세스 폴백(`ThumbnailRenderer::SubmitPrefab` 포함) 유지.
  - **대량 썸네일 프레임 방어 최적화**: 2048×2048 썸네일 공용 아틀라스(256슬롯/페이지), `Renderer::GetPreRenderDelegate()` 기반 렌더 프레임 기록 중 무대기(Fence Stall 0회) 부분 텍스처 업로드, LIFO 스케줄링 및 콘텐츠 브라우저 3-채널 `ImDrawListSplitter` 일괄 배칭 연동.

---

## 4. 단계별 개발 로드맵 (Phased Implementation Roadmap)

| 단계 | 목표 | 주요 작업 내용 | 산출물 |
| :--- | :--- | :--- | :--- |
| **Phase 1** | 독립 CLI 워커 및 썸네일 베이커 구축 *(완료)* | • `Tools/AssetWorker/` CMake 프로젝트 및 `BamAssetWorker.exe` 구축<br>• 헤드리스 PBR 래스터라이저(`HeadlessThumbnailRasterizer`) 구현<br>• 모델/메시/원시모델/프리팹 썸네일 단독 CLI & 데몬 모드 구현 | `BamAssetWorker.exe` |
| **Phase 2** | 에디터 호스트 및 썸네일 파이프라인 연동 *(완료)* | • 에디터 내 `AssetWorkerHost` 구축 (Job Object + 데몬 파이프 풀 2개)<br>• `AssetCache` 비동기 워커 위임/폴링 연동 및 프리팹 저장 무효화 연동<br>• 레거시 `Resources/` -> `Assets/` 리소스 참조 폴백 보강 | `AssetWorkerHost.h/.cpp`, `AssetCache.cpp` |
| **Phase 3** | 대용량 임포트 파이프라인 확장 *(예정)* | • `AssetManager::ImportAsync` 의 `ModelConverter`/`TextureConverter` 작업을 `BamAssetWorker` 파이프로 이전<br>• 하단 푸터 바 실시간 진행률(%) 및 취소(`X`) 버튼 연동 | 임포트 진행률 UI, 변환 워커 |
| **Phase 4** | 결함 격리 및 60FPS 무간섭 검증 *(진행 전)* | • 고의적 OOM/크래시 에셋 주입 시 에디터 무영향 검증<br>• 대용량 에셋 일괄 처리 도중 60FPS 유지 실측 | 검증 리포트, 단위 테스트 |

---

## 5. 검증 기준 (Pass/Fail Criteria)

1. **프레임 무간섭 불변성 (Pass)**:
   - 500MB 이상의 대형 FBX 및 8K 텍스처를 임포트하는 동안, 에디터 3D 뷰포트 조작 시 프레임 드랍이 16.6ms(60FPS) 이하로 유지되어야 함 (`Stuttering 0건`).
2. **프로세스 격리성 (Pass)**:
   - 워커 프로세스 강제 강제종료(`taskkill /F /IM BamAssetWorker.exe`) 시 에디터가 다운되지 않고 적절한 오류 안내 팝업을 띄워야 함.
3. **좀비 프로세스 제로 (Pass)**:
   - 에디터가 작업 도중 강제 종료되더라도 OS 백그라운드에 `BamAssetWorker.exe`가 1개도 잔류하지 않고 즉시 자동 정리되어야 함.
