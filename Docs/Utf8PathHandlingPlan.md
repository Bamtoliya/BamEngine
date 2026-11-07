# UTF-8 파일 경로 및 다국어 입출력 안정화 계획 (Utf8PathHandlingPlan)

- 작성일: 2026-11-04
- 상태: 열거한 네 리스너와 UI 경로 반영 / AssetHotReloadManager 잔여 경계 발견(2026-11-06, A03)
- 관련 기준: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md), [DevelopmentPriorities.md](DevelopmentPriorities.md)

---

## 1. 배경 및 문제 정의

### 1.1 크래시 증상 및 재현 경로
- **증상**: 에디터 실행 중 파일 다이얼로그나 외부 탐색기에서 "새 폴더" 등 한글 이름의 디렉터리/파일을 생성하거나, 에디터 내부(Content Browser)에서 F2 키를 통해 한글로 이름을 변경할 때 에디터 프로세스가 `std::_Throw_system_error_from_std_win_error` 예외(Win32 오류 코드 1113: `ERROR_NO_UNICODE_TRANSLATION`)와 함께 즉시 비정상 종료(Crash)됨.
- **콜스택**:
  ```text
  Editor.exe!std::_Throw_system_error_from_std_win_error(...) 줄 734
  Editor.exe!std::_Check_convert_result(...) 줄 49
  Editor.exe!std::filesystem::_Convert_narrow_to_wide(...) 줄 47
  Editor.exe!std::filesystem::path::path<std::string, 0>(const std::string& _Source, ...) 줄 648
  Editor.exe!Editor::ContentBrowserFileListener::handleFileAction(...) 줄 31
  efsw.dll!efsw::FileWatcherWin32::handleAction(...)
  efsw.dll!efsw::FileWatcherWin32::run()
  ```

### 1.2 근본 원인 (Root Cause)
1. **외부/서드파티 라이브러리의 UTF-8 규격**:
   - `efsw`(파일 감시자), `ImGui`(UI 텍스트 입력 버퍼), `SDL3`(파일 드롭 이벤트) 등 크로스플랫폼 라이브러리는 문자열을 **UTF-8 인코딩의 `std::string` 또는 `char*`**로 전달함.
2. **MSVC `std::filesystem::path`의 narrow 문자열 해석 방식**:
   - C++20 MSVC STL에서 `std::filesystem::path(const std::string&)` 또는 `path / narrow_str` 연산자는 내부적으로 `std::filesystem::_Convert_narrow_to_wide`를 호출함.
   - 이때 전달된 narrow 문자열은 UTF-8이 아닌 **시스템 ANSI 코드페이지(한국어 Windows 기본값인 CP949 / EUC-KR)**로 간주되어 `MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, ...)`를 호출함.
3. **인코딩 불일치 및 스레드 크래시**:
   - 한글("새 폴더" 등)의 UTF-8 바이트 시퀀스는 CP949에서 유효하지 않은 멀티바이트 시퀀스로 판정되어 변환이 실패함.
   - 변환 실패 시 STL 내부에서 `std::filesystem::filesystem_error` 예외를 던지며, `efsw` 백그라운드 워커 스레드나 UI 루프에 `try-catch`가 없을 경우 `std::terminate()`로 이어져 프로세스가 즉사함.

---

## 2. 해결 목표 및 설계 원칙

1. **표준 C++20 기반 안전한 UTF-8 $\leftrightarrow$ Path 변환 단일 진실의 원천(SSOT) 구축**:
   - `Engine/Source/Core/Public/Functions.h`에 `Engine::Utf8ToPath(std::string_view utf8Str)` 인라인 헬퍼 함수를 구축.
   - C++20 표준 규격(`reinterpret_cast<const char8_t*>`, `std::u8string_view`)을 사용하여 경고 없이 UTF-8을 `std::filesystem::path`로 정확히 변환.
2. **모든 비동기 파일 감시자(`efsw::FileWatchListener`) 전수 교정 및 예외 방어벽 구축**:
   - `ContentBrowserPanel`: `handleFileAction`의 경로 결합을 `Utf8ToPath`로 교체하고, 외부 스레드 예외 방어용 `try-catch` 추가.
   - `ModuleBuildManager`, `ModuleHotReloadManager`, `ShaderHotReloadManager`: `Utf8ToPath` 적용.
3. **에디터 UI 입력 및 드래그 앤 드롭 안전화**:
   - Content Browser 인라인 이름 변경(`DrawRename`): `renameBuffer` 변환 시 `Utf8ToPath` 적용.
   - SDL3 외부 드롭 처리(`OnExteranalDropped`): `droppedFiles` 변환 시 `Utf8ToPath` 적용.
4. **불필요한 Narrow 문자열 왕복 변환 제거**:
   - `path.stem().string() + "..."` 형태로 Wide $\rightarrow$ Narrow $\rightarrow$ Wide로 불필요하게 왕복 변환하던 코드를 `path.stem().wstring() + L"..."`로 교체하여 인코딩 변환 비용 및 실패 가능성 원천 차단.

---

## 3. 단계별 작업 항목

- [x] **Phase 1: UTF-8 변환 유틸리티 표준화**
  - `Engine/Source/Core/Public/Functions.h`에 `Engine::Utf8ToPath` 함수 추가.
- [x] **Phase 2: 파일 감시자(efsw) 리스너 4종 전수 교정**
  - `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp`
  - `Editor/Source/HotReload/Private/ModuleBuildManager.cpp`
  - `Editor/Source/HotReload/Private/ModuleHotReloadManager.cpp`
  - `Editor/Source/HotReload/Private/ShaderHotReloadManager.cpp`
- [x] **Phase 3: Content Browser UI 입력 및 파일 드롭 교정**
  - `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp`
  - `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserTreeItem.cpp`
  - `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp` (`OnExteranalDropped`)
- [x] **Phase 4: 썸네일 캐시 및 파일 결합 Wide String 유지 보완**
  - `Editor/Source/AssetManager/AssetCache/Private/AssetCache.cpp`
  - `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserGridItem.cpp` (스프라이트/JSON 생성 경로)
- [ ] **Phase 4-B: 에셋 감시자 경계 보완 (2026-11-06 추가)**
  - `AssetHotReloadManager.cpp`의 `TextureFileListener`도 대상에 포함한다. `u8path` 이후 `filename().string()`으로 되돌리는 판정 함수와 `addWatch(m_WatchDirectory.string())`를 교정한다.
  - 기존 `Utf8ToPath`/`WStrToStr`를 재사용하고 콜백 예외 방어·감시 등록 실패 보고를 연결한다. 상세 근거는 [A03](CodeDocumentationAudit-2026-11-06.md#a03--높음--에셋-감시자에-다국어-변환과-예외-경계가-남음).
  - `IHotReloader::NotifyFileModified`의 `filePath.string()` 변환도 제거한다. 디바운스 키와 수집 결과를 `std::filesystem::path`로 유지하고 에셋 유형 판정과 리로드 로그의 경로 변환을 함께 확인한다.
  - `Texture::Reload`는 이미 path로 파일을 읽는다. 읽기 전 로그용 `targetPath.string()`을 UTF-8로 바꾼다. 로캘 CSV 변환기의 CWD·외부 프로세스 실행 경계는 별도 검증 대상으로 기록한다.
  - 한글뿐 아니라 시스템 코드페이지로 표현 불가능한 문자·이모지 파일명과 다국어 프로젝트 루트를 검증한다.
- [ ] **Phase 5: 빌드 검증 및 문서화, 커밋**
  - 전체 빌드 통과 확인
  - `Docs/DevelopmentPriorities.md`, `Docs/ImplementationLog.md` 기록 및 Git 커밋

---

## 4. 검증 기준

1. **정적 검증**:
   - 모든 수정 대상 파일에서 `std::filesystem::path(dir) / filename` 형태의 미변환 narrow 결합이 제거되었는지 확인.
2. **빌드 검증**:
   - MSVC C++20 컴파일러에서 경고 및 에러 없이 클린 빌드되는지 확인.
3. **시나리오 검증**:
   - 파일 다이얼로그나 탐색기에서 "새 폴더", "새 텍스트 문서.txt" 등 다국어 파일 생성 시 크래시 없이 리프레시되는지 확인.
   - Content Browser에서 F2를 눌러 한글 이름으로 변경할 때 정상 반영되는지 확인.
   - 바탕 화면 등 한글 경로에서 파일을 에디터로 드래그 앤 드롭 시 정상 임포트되는지 확인.
