# 빌트인 리소스와 사용자 애셋 분리 보관 계획 (Resource & Asset Separation Plan)

- 작성일: 2026-11-05
- 상태: 계획 수립 (차기 개발 목표)
- 관련 지침: [EngineEditorClientGuidelines.md](EngineEditorClientGuidelines.md), [BuildAndPackagingPlan.md](BuildAndPackagingPlan.md)
- 우선순위: [DevelopmentPriorities.md](DevelopmentPriorities.md) P3 / 에셋 아키텍처

---

## 1. 배경 및 문제 정의

현재 BamEngine은 모든 리소스(엔진 기본 구동 자원, 에디터 UI 자원, 프로젝트 사용자 에셋, 테스트용 다운로드 데이터)를 단일 루트 폴더인 `Resources/`에 혼재하여 관리하고 있다.

### 현재 구조의 한계
1. **버전 관리(Git) 및 협업 오염**:
   - 엔진 구동에 필수적인 기본 셰이더, 기본 머티리얼, 에디터 로컬라이제이션 파일과 사용자의 대용량 테스트 모델/텍스처/씬이 뒤섞여 있어 커밋 분리 및 이력 관리가 어렵다.
2. **프로젝트 격리(Multi-Project) 불가**:
   - 엔진 코어와 사용자 프로젝트의 경계가 모호하여, 여러 프로젝트를 전환하거나 새 프로젝트를 생성할 때 엔진 리소스와 프로젝트 리소스를 깔끔하게 분리할 수 없다.
3. **에디터 UI(Content Browser) 혼선**:
   - 에셋 브라우저 탐색 시 엔진 내부 전용 파일(`FullscreenQuadVS.hlsl`, `Editor.csv`, 내장 더미 텍스처 등)이 사용자 콘텐츠와 섞여 노출되어 작업 가독성을 저해하고 실수로 엔진 필수 자원을 수정·삭제할 위험이 있다.
4. **빌드 및 패키징(Cook / Stage) 복잡성**:
   - 독립 실행형 클라이언트 패키징 시 전체 `Resources/`를 무차별 복사하거나 일일이 수동 제외해야 하며, 런타임에 필요한 최소 엔진 자원과 게임 콘텐츠의 경계가 불명확하다.

---

## 2. 목표 및 핵심 설계 원칙

1. **저장소 물리 디렉터리 분리**:
   - **엔진 빌트인 리소스 (`Engine/Resources/` 또는 `Resources/Engine/`)**: 엔진 및 에디터 구동에 필수적인 불변(Immutable) 시스템 자원.
   - **사용자 프로젝트 애셋 (`Assets/` 또는 `Resources/Project/`)**: 사용자가 저작·임포트·편집하는 프로젝트 고유 가변(Mutable) 자원.
2. **가상 경로(Virtual URI / Mount Point) 시스템 도입**:
   - 물리 경로에 의존하지 않는 스킴 기반 식별자(`Engine://`, `Project://` / `Asset://`)를 정의하여 코드와 직렬화 데이터의 이식성을 보장.
3. **하위 호환성 유지 (Graceful Fallback)**:
   - 기존의 `Resources/...` 상대 경로 참조가 즉시 파괴되지 않도록 다단계 해석 및 마이그레이션 경로 제공.
4. **에디터 및 패키징 파이프라인 연계**:
   - 콘텐츠 브라우저의 "엔진 콘텐츠 표시" 토글 옵션 및 빌트인 자원 읽기 전용(Read-Only) 보호.
   - 패키징 시 엔진 런타임 최소 리소스와 프로젝트 에셋을 각각 최적화 수집.

---

## 3. 세부 설계

### 3.1 디렉터리 구조

```text
BamEngine/
├─ Engine/
│  └─ Resources/                   # [엔진 빌트인 리소스 (Engine://)]
│     ├─ Shaders/                  # FullscreenQuadVS, LightingPS, PBR, Gizmo, Sky 등
│     ├─ Materials/                # DefaultMaterial, ErrorMaterial, SkyMaterial 등
│     ├─ Textures/                 # White1x1, Black1x1, FlatNormal1x1, DefaultGrid 등
│     ├─ Meshes/                   # Primitive Meshes (Cube, Sphere, Cylinder, Capsule, Quad)
│     ├─ Fonts/                    # 기본 UI 폰트, 아이콘 폰트 (FontAwesome 등)
│     └─ Localization/             # Engine.csv, Editor.csv (엔진/에디터 표준 번역 데이터)
│
├─ Assets/                         # [사용자 프로젝트 애셋 (Project:// 또는 Asset://)]
│  ├─ Scenes/                      # 사용자 저장 씬 (.json)
│  ├─ Models/                      # 임포트된 모델 (.bammodel, .bammesh)
│  ├─ Textures/                    # 프로젝트 텍스처 (.png, .dds)
│  ├─ Materials/                   # 사용자 머티리얼 및 머티리얼 인스턴스 (.bammat.json, .bammatinst.json)
│  ├─ Animations/                  # 애니메이션 클립, .animator 상태 머신 에셋
│  ├─ Audio/                       # 배경음, 효과음 에셋
│  └─ Prefabs/                     # 엔티티 프리팹 에셋
│
└─ Config/
   └─ Project.json                 # 프로젝트 설정 (AssetDirectory: "Assets", EngineResourceDirectory: "Engine/Resources")
```

### 3.2 가상 경로(URI) 및 마운트 시스템

`ResourceManager`와 `PlatformPath`에 다중 마운트 포인트를 지원하는 가상 경로 해석 계층을 도입한다.

| 가상 스킴 / 접두사 | 대상 루트 디렉터리 | 성격 | 권한 |
| :--- | :--- | :--- | :--- |
| `Engine://` (또는 `/Engine/`) | `<EngineRoot>/Engine/Resources/` | 시스템 빌트인 자원 | 읽기 전용 (Read-Only) |
| `Project://` (또는 `Asset://`, `/Game/`) | `<ProjectRoot>/Assets/` | 프로젝트 전용 사용자 에셋 | 읽기/쓰기 (Read-Write) |
| 상대 경로 (Legacy) | 마운트 순서대로 검색 (`Assets/` 우선 -> `Engine/Resources/` 폴백) | 하위 호환 폴백 | 조회 후 자동 정규화 경고 |

#### 경로 해석 알고리즘 (Pseudo-Code)
```cpp
std::filesystem::path ResolvePath(const std::wstring& virtualKey)
{
    if (virtualKey.starts_with(L"Engine://"))
    {
        return GetEngineResourceRoot() / virtualKey.substr(9);
    }
    if (virtualKey.starts_with(L"Project://"))
    {
        return GetProjectAssetRoot() / virtualKey.substr(10);
    }
    
    // [하위 호환 폴백]: 기존 Resources/... 또는 상대 경로 해석
    auto assetPath = GetProjectAssetRoot() / virtualKey;
    if (std::filesystem::exists(assetPath))
        return assetPath;
        
    auto enginePath = GetEngineResourceRoot() / virtualKey;
    if (std::filesystem::exists(enginePath))
        return enginePath;
        
    return GetProjectAssetRoot() / virtualKey; // 기본 대상
}
```

### 3.3 에디터 콘텐츠 브라우저(Content Browser) 및 UI 연동

1. **콘텐츠 브라우저 뷰 트리 분리**:
   - 루트에 `Project Content`(`Assets/`)와 `Engine Content`(`Engine/Resources/`)를 명확한 폴더 아이콘과 함께 분리 표시.
   - 상단 메뉴 또는 뷰 설정에 `[ ] Show Engine Content` 체크박스를 기본 해제(Unchecked) 상태로 두어 사용자의 작업 시야 분산 방지.
2. **에셋 보호 정책**:
   - `Engine://` 스키마 하위의 리소스는 에디터 인스펙터나 파일 탐색기에서 이름 변경(Rename), 삭제(Delete), 드래그 이동(Move)이 차단된다.
   - 빌트인 머티리얼을 수정하려는 경우, "Create Material Instance in Project" 또는 "Copy to Project Assets" 기능으로 유도.
3. **ResourcePicker 및 인스펙터**:
   - 기본적으로 프로젝트 애셋(`Project://`)을 상단에 노출.
   - 기본 기하 형상(Cube, Sphere 등)이나 기본 셰이더 선택 시에는 `Engine://` 범주를 탭이나 필터로 선택 가능.

### 3.4 빌드 및 패키징 (Cook / Stage / Package) 파이프라인 연동

`Tools/Scripts/PackageClient.ps1` 및 에디터 패키징 시스템에서의 수집 정책:

1. **Engine Built-in Resource 수집**:
   - 런타임에 필요한 항목만 추출: `Engine/Resources/Shaders/`, `Engine/Resources/Textures/` (1x1 등 기본 텍스처), `Engine/Resources/Localization/Engine.csv`.
   - 에디터 전용 자원(에디터 아이콘, `Editor.csv`, 개발용 Gizmo 셰이더 등)은 배포 패키지에서 자동 제외.
2. **Project Asset 수집**:
   - `Config/Project.json`에 지정된 `StartupScenePath`를 루트로 하여 씬에서 직렬화된 에셋 참조 그래프(Dependency Traversal)를 순회.
   - 사용되지 않는 미참조 대용량 원본 파일(`.zip`, `.blend`, 미사용 모델 등)은 배포 폴더에서 제외하여 패키지 경량화 달성.

---

## 4. 단계별 구현 순서

### Phase 1: 기반 경로 계약 및 디렉터리 분리 (준비 단계)
- [x] `ProjectSettings`(`Engine/Source/Core/Config/Public/Config.h`)에 `EngineResourceDirectory` 경로 설정 항목 정립. [완료: 2026-11-08]
- [x] URI 식별자·물리 경로·직렬화 표현을 구분하고 기존 `Resources/...` 대응, 외부 경로 거부, 빌트인 쓰기 금지, 실패 시 롤백 계약을 먼저 정한다. [완료: 2026-11-08]
- [x] 실제 파일을 이동하지 않고 참조 목록과 대표 씬/머티리얼/컨트롤러를 확보한다. 물리 이동은 Phase 3-B 이후다(2026-11-06 A10). [완료: 2026-11-08]

### Phase 2: `ResourceManager` 및 가상 마운트 해석 계층 구현
- [x] 먼저 기존 물리 위치에 마운트를 연결해 새 해석과 기존 참조가 같은 리소스를 가리키는지 확인한다. 현재 PlatformPath의 경로 결합을 URI 구현으로 간주하지 않는다. [완료: 2026-11-08]
- [x] `PlatformPath` 및 `ResourceManager`에 가상 스킴(`Engine://`, `Project://`) 해석 로직 구현. [완료: 2026-11-08]
- [x] 기존 상대 경로를 올바른 가상 스킴으로 변환하는 하위 호환 폴백 및 경고 로깅 추가. [완료: 2026-11-08]
- [x] 직렬화/역직렬화(`archive_reflection`) 시 가상 URI 포맷으로 저장/복원되도록 정규화. [완료: 2026-11-08]

### Phase 3: 에디터 툴 및 콘텐츠 브라우저 연동
- [x] `ContentBrowserPanel`에 `Assets/` 단독 뷰 및 `Show Engine Content` 토글 옵션 적용. [완료: 2026-11-08]
- [x] `Engine://` 리소스에 대한 읽기 전용 보호(삭제/이동 방지) 및 복사/인스턴스 생성 편의 기능 추가. [완료: 2026-11-08]
- [x] `ResourcePicker`의 카테고리 필터(`Project` / `Engine Built-in`) 분리. [완료: 2026-11-08]

### Phase 3-B: 소비자 검증 후 작은 묶음의 물리 이관
- **세부 실행 계획서**: [AssetMigrationPlan-Phase3B.md](AssetMigrationPlan-Phase3B.md)
- [x] Step 1: 감시자·임포터·피커·썸네일·시작 설정의 경로 해석을 먼저 연결한다 (`PlatformPath::ResolveUri` 일원화). [완료: 2026-11-08]
- [x] Step 2 (배치 1): `Engine/Resources/Shader/` 및 기본 머티리얼 이관, 참조 변환·저장/재열기·재임포트·핫리로드 확인. [완료: 2026-11-08]
- [x] Step 3 (배치 2): `Engine/Resources/Fonts/`, `Data/Localization/` 원본/JSON 및 도구 체인 이관 및 로캘 감사 통과. [완료: 2026-11-08]
- [ ] Step 4 (배치 3): 사용자 프로젝트 에셋(`Assets/`) 분리 정책 수립 및 시작 씬/동적 로드 자원 참조 무결성 검증.

이 순서는 [통합 계획 11절](DevelopmentPriorities.md)의 안전성 작업 이후에 적용한다. 전체 물리 이관을 다른 기능의 필수 선행 조건으로 두지 않는다. 근거는 [이번 점검 A10](CodeDocumentationAudit-2026-11-06.md)이다.

### Phase 4: 배포 스크립트 및 패키징 연동 검증
- [ ] 배포 재개 시 A01·A02의 출력 보호·생성 실패를 먼저 보완한다. 이번 계획 변경으로 배포 보류를 자동 해제하지 않는다.
- [ ] `PackageClient.ps1` 스크립트를 갱신하여 `Engine/Resources/` 런타임 필수 리소스와 `Assets/` 프로젝트 에셋을 각각 분리 수집하도록 개선.
- [ ] 배포 폴더에서 Client가 `Engine://` 및 `Project://`를 정상적으로 마운트하여 독립 실행되는지 검증.

---

## 5. 완료 기준 (Acceptance Criteria)

1. **물리적 분리**: 저장소 루트에 `Engine/Resources/`와 `Assets/`가 분리되어, Git 커밋 시 사용자 에셋과 엔진 코어 리소스의 변경이 명확히 구분된다.
2. **참조 불변성**: 가상 URI(`Engine://`, `Project://`)를 통해 프로젝트 파일의 위치가 변경되어도 리소스 간 참조가 깨지지 않는다.
3. **에디터 안전성**: 에디터에서 엔진 빌트인 리소스를 실수로 삭제하거나 덮어쓸 수 없으며, 일반 작업 시에는 프로젝트 에셋만 깔끔하게 노출된다.
4. **패키징 검증**: `PackageClient.ps1` 실행 시 에디터 전용 리소스와 미사용 테스트 에셋이 배포본에 포함되지 않고 경량 패키지가 정상 동작한다.
