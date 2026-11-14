# 셰이더 툴체인 다변화 및 Slang 도입 검토

- 작성일: 2026-11-14
- 관련 지침: [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md), [RenderingDevelopmentPlan.md](RenderingDevelopmentPlan.md), [BuildAndPackagingPlan.md](BuildAndPackagingPlan.md)
- 적용 대상: Engine 셰이더 컴파일러(`ShaderCompiler`), 오프라인 셰이더 빌드(`ShaderBuild.cmake`), 다중 RHI 백엔드(DirectX 12, Vulkan, WebGPU, 향후 Metal)

---

## 1. 배경 및 목적

BamEngine은 HLSL을 표준 셰이더 언어로 채택하고 DirectX 12(DXIL)와 Vulkan/SDLGPU(SPIR-V)를 지원해왔다.
최근 WebGPU(WGSL) 지원 및 플랫폼 확장이 진행되면서 단일 셰이더 소스로부터 서로 다른 바이너리/텍스트 포맷을 효율적이고 안정적으로 생성해야 하는 과제가 대두되었다.

현재는 `HLSL -> DXC -> SPIR-V -> Google Tint -> WGSL` 2단계 파이프라인을 구축 중이나, 장기적으로 복잡해지는 머티리얼 변종(Über-shader 매크로 관리), 다중 타깃 컴파일 일관성, 그리고 향후 Metal(macOS) 및 AI/신경망 렌더링 확장을 고려하여 **Slang(슬랭) 툴체인**의 도입 타당성을 분석하고 로드맵을 정의한다.

---

## 2. 툴체인 대안 비교

| 비교 항목 | 현행: DXC + Google Tint | 검토안: NVIDIA Slang | 대안: Naga (wgpu 생태계) |
| :--- | :--- | :--- | :--- |
| **파이프라인 구조** | $\text{HLSL} \to \text{SPIR-V} \to \text{WGSL}$ (2단계) | $\text{Slang/HLSL} \to \text{DXIL / SPIR-V / WGSL / MSL}$ (1단계) | $\text{HLSL} \to \text{WGSL}$ (1단계 직접 변환) |
| **지원 타깃** | DXIL, SPIR-V, WGSL (Tint 경유) | **DXIL, SPIR-V, WGSL, MSL(Metal), C++, CUDA** | WGSL, SPIR-V, MSL |
| **HLSL 문법 호환성** | 100% (Microsoft 공식 컴파일러) | **99% 호환 (HLSL 상위 집합 문법 지원)** | 보통 (최신 SM6+, 템플릿, 매크로 파싱 오류 빈번) |
| **모던 언어 기능** | 제한적 (기존 HLSL/C 전처리기 중심) | **제네릭(Generics), 인터페이스, 모듈/임포트, Auto-diff** | 없음 (변환기 역할에 집중) |
| **바인딩 매핑** | `space` 키워드 기반 수동 매핑 및 검증 필요 | 플랫폼별 레이아웃 규칙 자동 생성 및 리플렉션 내장 | 제한적 |
| **툴체인 복잡도** | `dxc.exe` + `tint.exe` 2개 도구 체인 의존 | `slangc.exe` 또는 `slang.dll` 단일 라이브러리 의존 | Rust 도구 또는 독립 실행 바이너리 필요 |

---

## 3. Slang 도입의 핵심 기대 효과

1. **단일 소스 다중 백엔드 완전 일원화 (Write-Once, Target-All)**:
   - Slang 소스 하나로 DirectX 12(DXIL), Vulkan(SPIR-V), WebGPU(WGSL)를 중간 경유 없이 직접 출력한다.
   - 향후 macOS/iOS 진출 시 Metal Shading Language(MSL) 변환기를 별도로 붙일 필요 없이 즉시 대응이 가능하다.
2. **머티리얼 Über-shader 매크로 지옥 해소**:
   - 현재의 `#ifdef USE_NORMAL_MAP`, `#ifdef HAS_ROUGHNESS` 등 C 전처리기 매크로 조합은 셰이더 변종 수가 증가할수록 유지보수가 불가능해진다.
   - Slang의 `interface IMaterial`과 `struct StandardPBR : IMaterial` 같은 정적 인터페이스/제네릭을 도입하면 가상 함수 비용 없이 컴파일 타임 최적화 코드로 분기할 수 있다.
3. **C++ API 직접 임베딩 지원 (`slang.dll`)**:
   - CLI 실행 파일(`slangc.exe`) 호출 방식뿐만 아니라, `slang.dll` C++ API를 엔진의 `ShaderCompiler`에 직접 링크하여 메모리 상에서 직접 컴파일 및 리플렉션 데이터를 획득할 수 있다.
4. **AI 및 신경 렌더링 확장성 (Automatic Differentiation)**:
   - Slang은 셰이더 연산의 전방/후방 미분(Auto-diff)을 언어 차원에서 지원한다. 향후 3D Gaussian Splatting, NeRF, 차분 가능 렌더링(Differentiable Rendering)이나 강화학습 물리 시뮬레이션 연동 시 강력한 이점을 제공한다.

---

## 4. 도입 시 고려사항 및 전환 비용 (위험 분석)

1. **외부 의존성 및 패키지 관리**:
   - `slang.dll` 및 컴파일러 런타임 배포가 필요하며, vcpkg 포트 또는 사전 빌드된 바이너리 아티팩트 관리가 수반된다.
2. **기존 빌드 파이프라인 재작업**:
   - [ShaderBuild.cmake](../Engine/CMake/ShaderBuild.cmake), [ShaderCompiler.cpp](../Engine/Source/Render/Shader/Private/ShaderCompiler.cpp), 그리고 셰이더 리플렉션 계층([ShaderReflection.cpp](../Engine/Source/Render/Shader/Private/ShaderReflection.cpp)의 `spirv-cross-c`)을 Slang C++ API에 맞추어 개편해야 한다.
3. **바인딩 레이아웃 계약 검증**:
   - [Shader/BINDING_LAYOUT.md](Shader/BINDING_LAYOUT.md)에 정의된 `space0`~`space3` 규약이 Slang의 자동 레이아웃 합성에서도 기존 D3D12 Root Signature 및 Vulkan Descriptor Set과 정확히 1:1 일치하는지 정밀 검증이 필요하다.

---

## 5. 단계별 도입 로드맵

- **1단계: 단기 — 현행 파이프라인 완결 (DXC + Tint)**
  - 현재 진행 중인 WebGPU Client 이식 선결 과제(`B01~B03`)는 기존 `HLSL -> DXC -> Tint` 파이프라인으로 안정화한다. 작업 도중 컴파일러를 전면 교체하는 무리한 리팩터링을 차단한다.
- **2단계: 중기 — Slang 툴체인 독립 검증 (PoC)**
  - 별도 브랜치에서 `slangc` CLI를 활용해 핵심 셰이더(`defaultVS/PS`, `gbufferVS/PS`, `lightingPS`, `tiledLightCullingCS`)를 DXIL, SPIR-V, WGSL로 컴파일하고 바이트코드 크기, 컴파일 시간, 바인딩 호환성을 비교 계측한다.
- **3단계: 장기 — 엔진 셰이더 컴파일러 통합**
  - PoC 결과에서 바인딩 일치와 안정성이 확인되면, `ShaderCompiler.cpp`에 Slang 백엔드를 옵션(`BAMENGINE_USE_SLANG=ON`)으로 통합하고 점진적으로 기존 컴파일러를 대체한다.
