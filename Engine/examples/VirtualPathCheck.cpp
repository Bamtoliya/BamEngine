#include "PlatformPath.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <cassert>

namespace fs = std::filesystem;
using namespace Engine;

#define EXPECT_TRUE(expr) \
    if (!(expr)) { \
        std::cerr << "Assertion failed: " #expr " at line " << __LINE__ << std::endl; \
        return __LINE__; \
    }

#define EXPECT_FALSE(expr) \
    if (expr) { \
        std::cerr << "Assertion failed (expected false): " #expr " at line " << __LINE__ << std::endl; \
        return __LINE__; \
    }

#define EXPECT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "Equality failed: " << fs::path(a).string() << " != " << fs::path(b).string() << " at line " << __LINE__ << std::endl; \
        return __LINE__; \
    }

int main()
{
    // 임시 격리 테스트 디렉터리 구성
    const fs::path tempRoot = PlatformPath::Normalize(fs::temp_directory_path() / "BamEngine_VirtualPathCheck");
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
    fs::create_directories(tempRoot, ec);

    struct CleanupGuard
    {
        fs::path path;
        ~CleanupGuard()
        {
            std::error_code ec;
            fs::remove_all(path, ec);
        }
    } cleanup{ tempRoot };

    const fs::path mockEngineRoot = PlatformPath::Normalize(tempRoot / "Engine" / "Resources");
    const fs::path mockAssetRoot = PlatformPath::Normalize(tempRoot / "Assets");
    fs::create_directories(mockEngineRoot / "Shaders", ec);
    fs::create_directories(mockAssetRoot / "Textures", ec);

    // 1. 마운트 루트 설정 및 조회 검증
    PlatformPath::SetProjectRoot(tempRoot);
    PlatformPath::SetEngineResourceRoot(mockEngineRoot);
    PlatformPath::SetProjectAssetRoot(mockAssetRoot);

    EXPECT_EQ(PlatformPath::GetEngineResourceRoot(), mockEngineRoot);
    EXPECT_EQ(PlatformPath::GetProjectAssetRoot(), mockAssetRoot);

    // 2. URI 스키마 판별 (대소문자 무시)
    EXPECT_TRUE(PlatformPath::IsEngineUri(L"Engine://Shaders/Test.hlsl"));
    EXPECT_TRUE(PlatformPath::IsEngineUri("engine://shaders/test.hlsl"));
    EXPECT_TRUE(PlatformPath::IsEngineUri(L"ENGINE://SHADERS/TEST.HLSL"));
    EXPECT_FALSE(PlatformPath::IsEngineUri(L"Project://Shaders/Test.hlsl"));

    EXPECT_TRUE(PlatformPath::IsProjectUri(L"Project://Textures/Test.png"));
    EXPECT_TRUE(PlatformPath::IsProjectUri("project://textures/test.png"));
    EXPECT_TRUE(PlatformPath::IsProjectUri(L"Asset://Textures/Test.png"));
    EXPECT_TRUE(PlatformPath::IsProjectUri("asset://textures/test.png"));
    EXPECT_FALSE(PlatformPath::IsProjectUri(L"Engine://Textures/Test.png"));

    EXPECT_TRUE(PlatformPath::IsVirtualUri(L"Engine://a"));
    EXPECT_TRUE(PlatformPath::IsVirtualUri(L"Project://b"));
    EXPECT_TRUE(PlatformPath::IsVirtualUri(L"Asset://c"));
    EXPECT_FALSE(PlatformPath::IsVirtualUri(L"Resources/Textures/Test.png"));

    // 3. URI 경로 해석 (ResolveUri)
    {
        const auto resolvedEngine = PlatformPath::ResolveUri(L"Engine://Shaders/FullscreenQuadVS.hlsl");
        const auto expectedEngine = PlatformPath::Normalize(mockEngineRoot / "Shaders/FullscreenQuadVS.hlsl");
        EXPECT_EQ(resolvedEngine, expectedEngine);

        const auto resolvedProject = PlatformPath::ResolveUri(L"Project://Textures/Knight.png");
        const auto expectedProject = PlatformPath::Normalize(mockAssetRoot / "Textures/Knight.png");
        EXPECT_EQ(resolvedProject, expectedProject);

        const auto resolvedAsset = PlatformPath::ResolveUri(L"Asset://Textures/Knight.png");
        EXPECT_EQ(resolvedAsset, expectedProject);
    }

    // 4. 경로 탈출(Directory Traversal) 방어 검증
    {
        const auto escapedEngine = PlatformPath::ResolveUri(L"Engine://../../Windows/System32/cmd.exe");
        EXPECT_EQ(escapedEngine, mockEngineRoot);

        const auto escapedProject = PlatformPath::ResolveUri(L"Project://../../etc/passwd");
        EXPECT_EQ(escapedProject, mockAssetRoot);
    }

    // 5. ToVirtualUri 역변환 검증
    {
        const fs::path engineFile = mockEngineRoot / "Shaders/LightingPS.hlsl";
        const auto engineUri = PlatformPath::ToVirtualUri(engineFile);
        EXPECT_EQ(engineUri, L"Engine://Shaders/LightingPS.hlsl");

        const fs::path assetFile = mockAssetRoot / "Textures/Knight.png";
        const auto assetUri = PlatformPath::ToVirtualUri(assetFile);
        EXPECT_EQ(assetUri, L"Project://Textures/Knight.png");
    }

    // 6. 읽기 전용 검사 (IsReadOnlyTarget) 검증
    {
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Engine://Shaders/Test.hlsl"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget("Engine://Shaders/Test.hlsl"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(mockEngineRoot / "Shaders/Test.hlsl"));

        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(L"Project://Textures/Knight.png"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget("Project://Textures/Knight.png"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(mockAssetRoot / "Textures/Knight.png"));
    }

    // 7. 하위 호환 폴백 (디스크 실존 파일 기준 우선순위 검색) 검증
    {
        const fs::path dummyEngineShader = mockEngineRoot / "Shaders/BaseVS.hlsl";
        const fs::path dummyProjectTexture = mockAssetRoot / "Textures/Hero.png";

        std::ofstream(dummyEngineShader) << "// engine shader";
        std::ofstream(dummyProjectTexture) << "// hero texture";

        // "Shaders/BaseVS.hlsl" 상대 경로 -> mockEngineRoot에서 탐지되어야 함
        const auto resolvedShader = PlatformPath::ResolveUri(L"Shaders/BaseVS.hlsl");
        EXPECT_EQ(resolvedShader, dummyEngineShader);

        // "Textures/Hero.png" 상대 경로 -> mockAssetRoot에서 탐지되어야 함
        const auto resolvedTexture = PlatformPath::ResolveUri(L"Textures/Hero.png");
        EXPECT_EQ(resolvedTexture, dummyProjectTexture);

        // "Resources/Textures/Hero.png" 상대 경로 -> mockAssetRoot에서 stripped 경로로 탐지되어야 함
        const auto resolvedResTexture = PlatformPath::ResolveUri(L"Resources/Textures/Hero.png");
        EXPECT_EQ(resolvedResTexture, dummyProjectTexture);
    }

    // 8. 과도기 단일 폴더(engineRoot == projectRoot) 환경에서의 빌트인 읽기 전용 판별 검증
    {
        const fs::path sharedRoot = PlatformPath::Normalize(tempRoot / "Resources");
        fs::create_directories(sharedRoot / "Shader", ec);
        fs::create_directories(sharedRoot / "Textures", ec);
        PlatformPath::SetEngineResourceRoot(sharedRoot);
        PlatformPath::SetProjectAssetRoot(sharedRoot);

        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(sharedRoot / "Shader"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(sharedRoot / "Shader/LightingPS.hlsl"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(sharedRoot / "Material/DefaultMaterial.bammat.json"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(sharedRoot / "Textures/Hero.png"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(sharedRoot / "Material/CustomUserMat.bammat.json"));
    }

    // 9. 실제 물리 이관(Shader, Material, Fonts, Localization) 자동 감지 및 하위호환 검증
    {
        // 커스텀 목 루트 초기화 -> 실제 디스크 환경 자동 감지
        PlatformPath::SetProjectRoot({});
        PlatformPath::SetEngineResourceRoot({});
        PlatformPath::SetEditorResourceRoot({});
        PlatformPath::SetProjectAssetRoot({});

        const fs::path engineRoot = PlatformPath::GetEngineResourceRoot();
        const fs::path projectRoot = PlatformPath::GetProjectAssetRoot();

        EXPECT_TRUE(fs::exists(engineRoot / "Shader/hlsl/defaultVS.hlsl"));
        EXPECT_TRUE(fs::exists(engineRoot / "Material/DefaultMaterial.bammat.json"));
        EXPECT_TRUE(fs::exists(engineRoot / "Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Solid-900.otf"));
        EXPECT_TRUE(fs::exists(engineRoot / "Data/Localization/English.json"));
        EXPECT_TRUE(fs::exists(engineRoot / "Data/Localization/Korean.json"));

        // 가상 URI 해석 검증
        const auto resolvedEngineShader = PlatformPath::ResolveUri(L"Engine://Shader/hlsl/defaultVS.hlsl");
        EXPECT_EQ(resolvedEngineShader, PlatformPath::Normalize(engineRoot / "Shader/hlsl/defaultVS.hlsl"));

        // 레거시 Resources/ 상대 경로 -> Engine/Resources 이관 위치로 자동 해석 검증
        const auto resolvedLegacyShader = PlatformPath::ResolveUri(L"Resources/Shader/hlsl/defaultVS.hlsl");
        EXPECT_EQ(resolvedLegacyShader, PlatformPath::Normalize(engineRoot / "Shader/hlsl/defaultVS.hlsl"));

        const auto resolvedLegacyMat = PlatformPath::ResolveUri(L"Resources/Material/DefaultMaterial.bammat.json");
        EXPECT_EQ(resolvedLegacyMat, PlatformPath::Normalize(engineRoot / "Material/DefaultMaterial.bammat.json"));

        const auto resolvedEngineFont = PlatformPath::ResolveUri(L"Engine://Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Solid-900.otf");
        EXPECT_EQ(resolvedEngineFont, PlatformPath::Normalize(engineRoot / "Fonts/fontawesome-free-7.1.0-desktop/otfs/Font Awesome 7 Free-Solid-900.otf"));

        const auto resolvedLegacyLoc = PlatformPath::ResolveUri(L"Resources/Data/Localization/English.json");
        EXPECT_EQ(resolvedLegacyLoc, PlatformPath::Normalize(engineRoot / "Data/Localization/English.json"));

        // 10. 빌트인 텍스처 및 확장 머티리얼 해석 검증
        auto resolvedWhite = PlatformPath::ResolveUri(L"Engine://Texture/white1x1.png");
        EXPECT_TRUE(fs::exists(resolvedWhite));
        auto resolvedChecker = PlatformPath::ResolveUri(L"Engine://Texture/DefaultChecker.png");
        EXPECT_TRUE(fs::exists(resolvedChecker));
        auto resolvedSpriteMat = PlatformPath::ResolveUri(L"Engine://Material/SpriteMaterial.bammat.json");
        EXPECT_TRUE(fs::exists(resolvedSpriteMat));

        // 11. Assets/ 프로젝트 에셋 루트 및 레거시 상대 경로 해석 검증
        EXPECT_TRUE(fs::exists(projectRoot / "Scenes/RaytracingScene.bamscene.json"));
        const auto resolvedLegacyScene = PlatformPath::ResolveUri(L"Resources/Scenes/RaytracingScene.bamscene.json");
        EXPECT_EQ(resolvedLegacyScene, PlatformPath::Normalize(projectRoot / "Scenes/RaytracingScene.bamscene.json"));
        const auto resolvedProjectScene = PlatformPath::ResolveUri(L"Project://Scenes/RaytracingScene.bamscene.json");
        EXPECT_EQ(resolvedProjectScene, PlatformPath::Normalize(projectRoot / "Scenes/RaytracingScene.bamscene.json"));

        // 12. Editor:// 가상 마운트 URI 및 에디터 로캘 해석 검증
        const auto resolvedEditorLoc = PlatformPath::ResolveUri(L"Editor://Data/Localization/English.json");
        EXPECT_TRUE(fs::exists(resolvedEditorLoc));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Editor://Data/Localization/English.json"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(resolvedEditorLoc));

        // 읽기 전용 판별 검증
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Engine://Shader/hlsl/defaultVS.hlsl"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Resources/Shader/hlsl/defaultVS.hlsl"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Resources/Material/DefaultMaterial.bammat.json"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Resources/Material/SpriteMaterial.bammat.json"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Resources/Texture/white1x1.png"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Resources/Texture/DefaultChecker.png"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Engine://Data/Localization/English.json"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Resources/Data/Localization/English.json"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(L"Assets/Material/GBufferMaterial.bammat.json"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(L"Resources/Material/GBufferMaterial.bammat.json"));
    }

    std::cout << "[VirtualPathCheck] All virtual path and URI tests passed successfully!" << std::endl;
    return 0;
}
