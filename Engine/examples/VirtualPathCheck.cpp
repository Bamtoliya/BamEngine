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
        std::cerr << "Equality failed: " << (a) << " != " << (b) << " at line " << __LINE__ << std::endl; \
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

    std::cout << "[VirtualPathCheck] All virtual path and URI tests passed successfully!" << std::endl;
    return 0;
}
