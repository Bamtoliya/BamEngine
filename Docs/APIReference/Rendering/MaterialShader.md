# Material / Shader / Graph

[API 목차](../README.md) · [API 감사](../APIAudit.md)

- 기준 커밋: `770e3adb81e3a4e9ea692996df09942e74587f28` (2026-11-28 KST)
- API 안정성: **Preview** (문서 작성자 제안 등급, 엔진의 공식 호환성 보증 아님)
- 검증: 공개 선언 및 일부 구현/예제 정적 확인. 신규 빌드·실행 미수행. 코드 조각은 별도 명시가 없으면 컴파일 미검증.

## 리소스 사용 순서
Material/MaterialInstance, shader, texture를 에셋으로 준비하고 static/skinned/sprite renderer에 올바른 material handle을 지정한다. `MaterialInterface`에 MaterialConstants(roughness, metallic, baseColor, emissiveColor, ao 등), MaterialTextureBinding, MaterialPassShader 및 MaterialBufferLayout이 존재한다. 구조체를 작성했다고 자동 GPU upload가 발생한다고 가정하지 않는다. material별 파라미터 변경 API는 추가 확인 후 이용한다.

| API | 의미 |
|---|---|
| `Material::Bind(uint32 slot) -> EResult` | 렌더 문맥에서 바인딩 |
| `Shader::GetRHIShader() -> RHIShader*` | 비소유 native abstraction 조회 |
| `Shader::UpdateBytecode(const vector<uint8>&) -> EResult` | 바이트코드 갱신; 파이프라인 호환 검증 |
| `Shader::GetShaderType()` / `GetEntryPoint()` | shader stage 및 entry 정보 |
| `GetUniformBuffers`, `GetStorageBuffers`, `GetSamplers`, `GetStorageTextures` | 리플렉션 바인딩 정보 |
| `FindUniformBuffer(set,binding)` / `FindUniformBuffer(name)` | null 처리 |

```cpp
#include "Shader.h"
void InspectShader(const Engine::Shader& shader) {
    for (const auto& binding : shader.GetUniformBuffers()) {
        (void)binding; // inspect reflected binding
    }
}
```

## 주의
`Shader::Bind` 기본 구현은 NotImplemented 반환이다. Material 경로를 우선 사용한다. DXIL/SPIR-V/WGSL 경로 필드가 있지만 모든 backend에 자동 번역·실행을 보장하지 않는다. ShaderGraphCompiler는 로직 그래프와 별도 도메인이다. 새 코드 실패 시 기존 material/pipeline 유지 여부, shader stage IO와 binding 변경 후 재컴파일을 검증한다. [BINDING_LAYOUT](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/Shader/BINDING_LAYOUT.md)와 [MaterialGraph 계획](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/MaterialShaderGraphIntegrationPlan.md) 참조.

## 관련 소스
- [Material.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Render/Material/Public/Material.h) — `Engine/Source/Render/Material/Public/Material.h`
- [MaterialInterface.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Render/Material/Public/MaterialInterface.h) — `Engine/Source/Render/Material/Public/MaterialInterface.h`
- [Shader.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Render/Shader/Public/Shader.h) — `Engine/Source/Render/Shader/Public/Shader.h`
- [MaterialInstance.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Render/Material/Public/MaterialInstance.h) — `Engine/Source/Render/Material/Public/MaterialInstance.h`
- [ShaderGraphCompiler.h](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Shader/Public/ShaderGraphCompiler.h) — `Engine/Source/Graph/Shader/Public/ShaderGraphCompiler.h`

## 검증 및 참고 예제
- [ShaderGraphCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/ShaderGraphCheck.cpp)
- [IBLShadingCheck.cpp](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/examples/IBLShadingCheck.cpp)
