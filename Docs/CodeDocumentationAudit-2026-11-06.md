# 코드·개발 원칙·계획 정합성 점검 (2026-11-06)

## 1. 기준과 범위

- 기준: `main`, HEAD `ecca09da` 및 점검 시작 시 존재하던 미커밋 변경. 소스 변경도 현재 구현으로 읽되 커밋·빌드된 상태로 간주하지 않았다.
- 사용자 승인 범위: 문서 작성·수정. 소스·에셋·빌드 설정은 수정하지 않았고 빌드, CTest, GPU/UI 실행, 패키징은 수행하지 않았다.
- 대조 기준: 루트 AGENTS.md, EngineEditorClientGuidelines 3~6·13~18절, SourceOrganizationGuidelines, ReflectionArchiveGuidelines, Shader/BINDING_LAYOUT 및 관련 기능 계획.
- 조사 범위: 패키징/Client 설정, 파일 감시자, 모델 임포트 변경부, RT 진단·채널 필터·광원 목록, 스플리터 저장, 썸네일 캐시, 로캘·입력, 에셋 분리 계획. 전체 엔진의 모든 코드·실행 경로를 검토한 결과가 아니다.
- 이전 점검의 F 번호와 혼동하지 않도록 이번 항목은 A01~A10으로 구분한다. 심각도와 작업 순서는 별개다.

## 2. 핵심 판단

기능 확대보다 기존 제작 경로의 안전성을 먼저 보완한다. 특히 패키징의 삭제 보호를 완료로 유지할 수 없다. UTF-8 경로 처리와 RT 표시에는 기존 보완 후에도 빠진 경계가 있다. 로캘·입력은 이번 정적 감사에서 위반이 검출되지 않았으므로 전면 재정리 대상으로 올리지 않는다.

전체 배포, 전체 에셋 폴더 이전, 모든 장기 기능을 하나의 선행 조건으로 묶지 않는다. 통합 작업 순서와 사용 조건은 [DevelopmentPriorities 11절](DevelopmentPriorities.md#11-현재-작업-트리-기준-우선순위-재평가-2026-11-06)이 관리한다.

## 3. 결함·위험·문서 불일치

### A01 — 심각 / 패키징 삭제 허용 범위가 소스 디렉터리까지 열려 있음

- 위치: `Tools/Scripts/PackageClient.ps1:13~43, 71~77`, `Get-NormalizedAbsolutePath`, `$ForbiddenDirectories`, 출력 초기화.
- 현재 동작: 드라이브/저장소 루트 및 일부 디렉터리를 거부한 뒤 선택한 출력 경로를 재귀 삭제한다.
- 근거: 금지 목록에 `Client`, `Utility`가 없다. Client.exe를 찾을 수 있는 구성에서 `-OutputDir Client` 또는 `Utility`를 전달하면 삭제 단계에 도달한다. 저장소 밖의 임의 폴더도 허용된다. `GetFullPath`는 문자열 정규화이며 junction 같은 실제 대상 검증을 대신하지 않는다.
- 영향: 잘못 지정한 출력 경로의 소스·사용자 데이터 삭제 위험. 실제 삭제를 실행해 재현하지 않았다.
- 원칙: 공통 지침의 데이터 보호, BuildAndPackagingPlan 6절의 소스 삭제 거부 조건에 어긋난다. 과거 F03의 세 경로 차단 기록만으로 전체 삭제 보호 완료를 판단할 수 없다.
- 최소 개선: 관리하는 패키지 출력 루트 안의 전용 하위 디렉터리만 허용하고, 입력·바이너리 경로와 겹치거나 실제 경로를 확정할 수 없는 대상은 거부한다. 외부 출력 지원은 소유 표식 등 별도 명시적 정책으로 다룬다. 금지 이름만 추가하는 방식은 새 폴더가 생길 때 다시 누락된다.
- 완료 조건: 삭제 없는 경로 판정 검사로 루트·상위·Client·Utility·입력 바이너리·외부 경로·junction 사례를 거부한다. 실제 삭제 검사는 소스가 아닌 폐기 가능한 전용 fixture에서만 수행한다.

### A02 — 높음 / 출력 삭제 후 지원하지 않는 PowerShell 인자로 중단

- 위치: `Tools/Scripts/PackageClient.ps1:76~77, 105, 130`의 `New-Item -LiteralPath` 호출.
- 근거: 현재 호스트의 `(Get-Command New-Item).Parameters.Keys`에 `LiteralPath`가 없다. `Remove-Item`/`Copy-Item`의 지원 인자를 `New-Item`에 동일하게 적용했다. 메타데이터만 조회했고 스크립트는 실행하지 않았다.
- 영향: 새 출력에서는 폴더 생성 실패, 기존 출력에서는 삭제 이후 생성 실패. 따라서 현행 스크립트의 패키징 성공을 과거 기록으로 보증할 수 없다.
- 최소 개선: 검증한 경로로 지원되는 디렉터리 생성 API를 사용하고, 모든 필수 입력을 먼저 확인한다. 임시 staging 작성 성공 후 기존 패키지를 교체하도록 순서를 정한다.
- 완료 조건: 빈 출력·기존 출력·공백/한글 경로·필수 DLL/설정 누락에서 기존 정상 패키지 보존 및 명확한 실패를 확인한다. A01을 먼저 해결한다.

### A03 — 높음 / 에셋 감시자에 다국어 변환과 예외 경계가 남음

- 위치: `Editor/Source/HotReload/Private/AssetHotReloadManager.cpp:20~61, 120~133`, 확장자 판정 함수, `TextureFileListener::handleFileAction`, `Initialize`.
- 현재 동작: 콜백 입력은 `u8path`로 변환하지만, 뒤의 판정 함수는 `path.filename().string()`으로 전체 파일명을 narrow 문자열로 다시 변환한다. 감시 등록에도 `m_WatchDirectory.string()`을 사용한다. 콜백을 감싼 예외 경계는 없다.
- 발생 조건: Windows 시스템 코드페이지로 표현할 수 없는 파일명(예: 이모지·일부 다국어 문자) 또는 UTF-8 계약과 다른 바이트의 감시 경로. 모든 한글 파일이 반드시 실패한다고 단정하지 않는다.
- 영향: 변환 예외가 외부 감시 스레드 밖으로 나가거나 잘못된 경로로 감시할 위험. 실제 충돌은 미재현이다.
- 원칙: Utf8PathHandlingPlan의 모든 감시자 교정·외부 스레드 예외 방어 목표와 불일치. 해당 계획은 네 리스너만 열거하고 이 리스너를 빠뜨렸다.
- 최소 개선: 기존 `Utf8ToPath`와 `WStrToStr`를 재사용하고 ASCII 확장자 비교도 안전한 UTF-8 또는 wide 표현에서 처리한다. 콜백에서 예외를 차단·기록하고 감시 등록 실패를 보고한다.
- 완료 조건: 한글·이모지·혼합 문자 이름의 PNG/머티리얼/메시 수정과 다국어 프로젝트 루트에서 감시 등록을 확인한다. 정적 변환 점검과 실행 결과를 구분한다.

### A04 — 높음 / 큐브맵을 2D RT 진단 경로에 그대로 전달

- 위치: `BaseViewportPanel.cpp:110~163, 183~205`, `SceneViewportPanel.cpp:366~376`, `ChannelFilter.cpp::SubmitChannelPreviewPass`, `ViewportDeferredPipeline.cpp:132~139`, `Resources/Shader/hlsl/viewport_channelPS.hlsl:14`.
- 근거: 전체 RT 메뉴는 큐브맵도 선택할 수 있다. PointShadowDepth는 `D32_FLOAT / TextureCube / DepthStencilTarget`인데 채널 필터는 `Texture2D`로 샘플링한다. `ImGuiManager::GetImGuiTextureID`도 차원 변환 없이 기존 SRV를 돌려준다.
- 영향: 큐브맵 선택 후 채널 진단에서 리소스 차원 계약 위반. 실제 화면 오류·D3D12 검증 오류·장치 실패의 형태는 실행 확인이 필요하다.
- 이미 개선된 범위: 씬의 grid/debug overlay는 RenderTarget usage 검사로 보호한다. 기존 오버레이 결함 전체가 미수정이라고 보고하지 않는다. 이 검사는 채널 필터 입력을 보호하지 않는다.
- 최소 개선: 지원하지 않는 차원의 진단을 차단하고 이유를 표시한다. 큐브맵 진단이 필요하면 면 선택과 2D 변환 출력을 기존 공통 진단 경로에 연결한다. 새 안내는 Editor.csv, 새 명령 단축키는 ShortcutManager를 사용한다.
- 완료 조건: 모든 RT 선택, RGBA와 단일 채널 전환, 별도 RT 창 열기, 깊이 큐브 6면에서 디버그 레이어 오류 없이 지원 또는 명시적 미지원이 표시된다.

### A05 — 보통 / 창 축소가 스플리터의 저장 선호값을 덮어씀

- 위치: `ModelEditor.cpp:110~127, 285~294`, `ModelImportModal.cpp:543~559, 841~850`.
- 근거: 매 Draw에서 `m_LeftPaneWidth`/`m_SplitterWidth`에 clamp 결과를 대입하고 같은 멤버를 workspace에 저장한다. 넓은 창에서 설정한 값이 좁은 창으로 바뀌는 것만으로 작아져 다시 넓혀도 복구되지 않는다.
- 원칙: EngineEditorClientGuidelines 17절의 설정 너비 유지 및 workspace 소유권과 불일치.
- 최소 개선: 표시용 clamp 값과 사용자 저장값을 분리하고 실제 splitter 드래그 때만 선호값을 갱신한다. 두 최소 영역보다 작은 창의 배치도 별도로 정의한다.
- 추가 확인. `ConsolePanel::DrawLog()`는 `activeDetailHeight`를 계산한 뒤 `m_DetailViewHeight`에도 대입한다. 콘솔 창 높이를 줄일 때 같은 저장값 덮어쓰기가 발생한다. 통합 설정의 `DrawSidebarLayout`이 표시값 분리와 실제 드래그 감지를 이미 구현하고 있다.
- 완료 조건. 창을 넓혔다 줄인 뒤 다시 넓히거나 좁은 상태에서 종료·재실행해도 선호 크기가 유지된다. 모델 임포트 모달을 다시 열 때도 같은 값을 복원한다.

### A06 — 보통 / 타일 광원 초과 폴백이 여전히 없음 (기존 F10)

- 위치: `tiledLightCullingCS.hlsl:5, 100~102, 133~146`, `lightingPS.hlsl:664~666`.
- 근거: atomic 카운트는 증가하지만 256개까지만 저장하고 최종 개수도 clamp한다. Lighting도 최대 256개만 순회한다.
- 영향: 같은 타일에 대상 광원이 257개 이상 교차하면 일부 기여가 제외된다. 용량 내 장면의 결함으로 확대하지 않는다.
- 원칙: RenderingDevelopmentPlan S4의 초과 계측·정확한 대체 경로 및 기본 경로와 결과 일치 기준 미충족.
- 개선/검증: overflow 표시와 전체 순회 등 정확성을 보존하는 대체 경로를 연결하고 255/256/257개와 기준 영상을 비교한다. 다수 광원 장면을 사용할 때 신규 조명 확장보다 우선한다.

2026-11-07 재확인. 현행 `LightManager`는 씬 광원을 최대 32개까지만 업로드하므로, 257개 타일 초과는 씬 광원 용량을 확장할 때 검증할 조건이다. 현재 실행 경로에서는 `EnsureTileBuffers()`가 큰 버퍼를 재사용한 뒤 `DispatchLightCulling()`이 할당용 타일 수를 디스패치에 사용한다. 위치 타깃 폭이 줄면 셰이더가 현재 폭으로 계산한 같은 타일 인덱스에 여러 그룹이 쓸 수 있다. 디스패치 그룹 수를 현재 위치 타깃 크기에서 계산한 뒤 해상도 축소·복원과 서로 다른 크기의 씬·게임 뷰를 비교한다.

### A07 — 보통 / 메모리·프레임 성능 완료 표현이 구현 근거를 넘음

- 위치: DevelopmentPriorities 10.6, 기존 점검 F12, ImGuiDevelopmentPlan 23.4, RenderingDevelopmentPlan S2-D; `AssetCache.cpp:596~678`, `AssetCache.h:35~43`.
- 근거: 썸네일은 원본 RGBA를 디코딩한 뒤 축소한다. 128×128 RGBA는 65,536바이트(64KiB)다. 축소 결과 보관 크기와 원본 디코드 피크는 다르다. 현재는 워커 2개, 입력 32MiB, 원본 RGBA 64MiB 등의 상한도 존재한다.
- 판단: 예산이 없다는 과거 지적은 현행에 적용하지 않는다. 반대로 축소만으로 디코드 메모리 99% 절감·프레임 hitch 제거가 입증되지도 않는다. 메트릭스 UI 60% 절감과 BC 압축 VRAM 비율 역시 현재 측정값이 아닌 목표/조건부 추정이다.
- 개선/검증: 구현된 상한과 효과를 구분하고 원본 크기·동시 작업·CPU 피크·GPU 사용량·프레임 p95/p99·해상도·빌드 구성을 기록한다. 전면 캐시 재작성은 우선하지 않는다.

### A08 — 보통 / Client 설정 정책에 대한 문서가 서로 다름

- 위치: `Client/Source/Main.cpp:356~384`, BuildAndPackagingPlan 2·6절, DevelopmentPriorities 10.4.
- 사실: `NotFound`는 경고 후 기본값으로 성공한다. 파싱 실패는 실패, SchemaVersion 0/지원 초과는 InvalidArgument다. 설정 결과·버전 검사가 없다는 설명도, 설정 누락이 안전 종료된다는 설명도 현재 코드와 다르다.
- 영향: 배포 실패 조건과 테스트 기대값을 잘못 정하게 된다. 코드의 기본값 정책 자체를 사용자 결정 없이 결함으로 확정하지 않는다.
- 개선: 현행 정책을 정확히 기술하고, 배포에서는 설정 누락을 거부할지 개발 실행과 구분할지 배포 재개 시 결정한다. 시작 매니페스트·에셋 루트 연결의 잔여 범위는 별도로 유지한다.
- 완료 조건: 누락/손상/0/미지원 버전/정상 설정의 실제 종료·폴백 결과를 정책과 대조한다.

### A09 — 보통 / 스키닝 바인딩 문서가 현재 구현 이전 상태

- 위치: `Docs/Shader/BINDING_LAYOUT.md`의 Skinning note; `skinningVS.hlsl:26`, `DirectX12RHI.cpp:204~205`, `Renderer.cpp:646~660`.
- 사실: 본 행렬은 현재 `t0, space2`에 선언되고 `BindInstanceBuffer(..., sizeof(mat4))`로 제출한다. 문서는 과거 `t0, space0` 미연결 설명을 유지했다.
- 영향: 후속 셰이더 작업에서 같은 슬롯을 비어 있는 것으로 오해하거나 별도 슬롯을 중복 도입할 수 있다.
- 조치: 현재 슬롯 공유와 드로우별 데이터 해석을 문서에 반영한다. 정적 메시 인스턴스와 본 행렬은 같은 슬롯의 서로 다른 사용이며 둘의 동시 사용 지원을 의미하지 않는다. 백엔드 동등성은 이번에 검증하지 않았다.

### A10 — 보통 / 에셋 분리 계획이 호환 해석보다 물리 이동을 앞세움

- 위치: ResourceAndAssetSeparationPlan 4절, `PlatformPath.cpp::ResolveProjectRelative`, BuildAndPackagingPlan 4절.
- 근거: 기존 계획 Phase 1은 실제 파일 이동, Phase 2는 가상 URI 및 기존 경로 해석이다. 현재 경로 함수는 절대 경로 또는 프로젝트 루트 결합이며 URI 마운트 구현이 아니다.
- 위험: 순서대로 구현할 경우 중간 상태에서 기존 씬/머티리얼/시작 설정/감시자/패키징 참조가 끊길 수 있다. 아직 파일 이동을 실행한 결함은 아니다.
- 추천: 분리 목표는 유지하되 경로 계약·기존 참조 호환 → 기존 물리 위치에서 마운트 검증 → 도구 소비자 연결 → 작은 에셋 묶음 이관 순서로 조정한다. 롤백·읽기 전용·외부 경로 거부·동적 참조 포함 정책을 먼저 정한다.
- 완료 조건: 이동 전후 같은 씬을 열고 저장/재열기·재임포트·핫리로드·피커·썸네일에서 참조가 유지된다. 전체 이관을 당장 진행 중인 2D/3D 기능의 전역 선행 조건으로 만들지 않는다.

## 4. 이번에 확인한 개선과 미검증 범위

- `python -B Utility/LocalizationAuditor/audit_localization.py --root . --strict`: 종료 코드 0. 파일 쓰기 없는 정적 감사로 634개 파일/2,219개 키 확인. 누락·빈 번역·중복 키·검사 대상 하드코딩 UI·빈 Editor 그룹·고정 패널 미번역·리터럴 액션 미등록·검사 대상 ImGui 입력 직접 조회가 없었다. 동적 문자열·실제 UI 충돌·번역 품질까지 증명하지 않는다.
- 직접 KEY 조회 검색 결과의 카메라 연속 이동·보조 키와 텍스트/드래그 취소를 명령 단축키 위반으로 일괄 분류하지 않았다. 공통 지침 14절의 예외 경계를 적용했다.
- 썸네일의 디코드 제한·무효화 요청·ImGui 프레임 텍스처 참조 보존이 존재한다. 이번에 GPU 수명과 성능을 실행 검증하지 않았다.
- Animator 저장 실패 반환·미저장 보호 코드와 현재 임포터 변경부를 확인했다. 기존 저장 보호가 전부 없다는 결론을 반복하지 않는다. 다중 출력 재임포트의 중간 실패 롤백·인메모리 갱신은 실패 주입 검증이 남는다.
- Joint의 비어 있지 않은 ConnectedEntity를 UnsupportedType으로 거부하는 현재 미커밋 변경은 미지원 참조를 조용히 성공 처리하지 않는 방향이다. 일반 엔티티 관계 저장의 완성으로 해석하지 않는다.

## 5. 기존 실행 기록의 한계와 다음 확인

- Debug `out/build/x64-debug/Testing/Temporary/LastTest.log`: 11월 6일 11:33 GraphExecutionCheck 한 건 통과. 29개 전체 통과가 아니다. `LastTestsFailed.log`에는 같은 이름이 남아 있어 단독 현재 실패 근거로 사용하지 않는다.
- Release LastTest.log는 10월 11일 시작·종료뿐이다. 현행 Release 전체 검증 근거가 아니다.
- `Logs/Editor.log` 11월 6일 10:14에는 `rgba2x2.bamsprite`, `rgba2x2.bamtex`, `rgba2x2test.bamtex` archive read 실패가 남아 있다. 이전 실행의 기록이며 현재 작업 트리에서 재현한 결과가 아니다. 2D 제작 예제 착수 시 테스트 데이터/구형 포맷/현재 로더를 먼저 구분한다.
- 이번 정적 감사와 과거 실행 기록, 앞으로 수행할 완료 조건을 섞지 않는다. 기존 기록의 검증 사실 자체를 취소하거나 현재 전체의 성공으로 확대하지 않는다.

사용자 실행용 명령(이번에 실행하지 않음):

```powershell
cmake --build out/build/x64-debug --config Debug
ctest --test-dir out/build/x64-debug -C Debug -R "EditorSettingsCheck|SceneArchiveCheck|EditorHistoryCheck" --output-on-failure --no-tests=error
```

관련 검사가 현재 구성에 등록·빌드되어 있는지 먼저 확인한다. 이 CTest가 패키징 경로 판정, 감시 스레드 예외, GPU 타입 오류, 실제 UI 복원을 검증하는 것은 아니다. 각 A 항목의 경계 시나리오는 별도 검증한다. 현행 PackageClient.ps1은 A01/A02 보완 전 실행 검증을 권하지 않는다.
