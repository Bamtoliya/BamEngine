#include "PlatformPath.h"
#include "ProjectDescriptor.h"
#include "ModelSubfolderHelper.h"
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
        PlatformPath::ClearVirtualSchemes();
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

        // 12. 범용 가상 스키마 등록(Editor://) 및 해제, 역변환, 읽기 전용 가드 검증
        const fs::path editorResRoot = PlatformPath::Normalize(PlatformPath::GetEngineRoot() / "Editor/Resources");
        PlatformPath::RegisterVirtualScheme({ L"Editor", editorResRoot, true, true });
        EXPECT_EQ(PlatformPath::GetVirtualSchemeRoot(L"Editor"), editorResRoot);
        EXPECT_TRUE(PlatformPath::IsVirtualSchemeUri(L"Editor://Data/Localization/English.json", L"Editor"));
        EXPECT_TRUE(PlatformPath::IsVirtualSchemeUri("editor://data/localization/english.json", "editor"));
        EXPECT_TRUE(PlatformPath::IsVirtualUri(L"Editor://Data/Localization/English.json"));
        EXPECT_TRUE(PlatformPath::IsVirtualUri("editor://data/localization/english.json"));

        const auto resolvedEditorLoc = PlatformPath::ResolveUri(L"Editor://Data/Localization/English.json");
        EXPECT_TRUE(fs::exists(resolvedEditorLoc));
        EXPECT_EQ(PlatformPath::ToVirtualUri(resolvedEditorLoc), L"Editor://Data/Localization/English.json");
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Editor://Data/Localization/English.json"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(resolvedEditorLoc));

        PlatformPath::UnregisterVirtualScheme(L"Editor");
        EXPECT_TRUE(PlatformPath::GetVirtualSchemeRoot(L"Editor").empty());
        EXPECT_FALSE(PlatformPath::IsVirtualUri(L"Editor://Data/Localization/English.json"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(L"Editor://Data/Localization/English.json"));

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

    // 13. Phase 6: ModelSubfolderHelper 하이브리드 에셋 서브폴더 구조 및 이름 정리(Sanitizer) 검증
    {
        // (1) SanitizeName 검증: 특수문자 치환, 공백/마침표 트리밍, 윈도우 예약어 회피, 빈 문자열 폴백
        EXPECT_EQ(ModelSubfolderHelper::SanitizeName(L"Barbarian:01*?<>|"), L"Barbarian_01_____");
        EXPECT_EQ(ModelSubfolderHelper::SanitizeName(L"  Knight.Mesh.  "), L"Knight.Mesh");
        EXPECT_EQ(ModelSubfolderHelper::SanitizeName(L"CON"), L"_CON");
        EXPECT_EQ(ModelSubfolderHelper::SanitizeName(L"aux"), L"_aux");
        EXPECT_EQ(ModelSubfolderHelper::SanitizeName(L"   "), L"Model");
        EXPECT_EQ(ModelSubfolderHelper::SanitizeName(L""), L"Model");

        // (2) ResolveLayout 검증: 기본 하이브리드 레이아웃 (새 폴더 생성)
        const fs::path targetDir = mockAssetRoot / "Models" / "Characters";
        const auto layout1 = ModelSubfolderHelper::ResolveLayout(targetDir, L"Barbarian", true, true, true, true, true);
        EXPECT_TRUE(layout1.UsesDedicatedModelFolder);
        EXPECT_EQ(layout1.RootDirectory, PlatformPath::Normalize(targetDir / "Barbarian"));
        EXPECT_EQ(layout1.ModelFilePath, PlatformPath::Normalize(targetDir / "Barbarian/Barbarian.bammodel"));
        EXPECT_EQ(layout1.SkeletonFilePath, PlatformPath::Normalize(targetDir / "Barbarian/Barbarian_Skeleton.bamskel"));
        EXPECT_EQ(layout1.MeshesDirectory, PlatformPath::Normalize(targetDir / "Barbarian/Meshes"));
        EXPECT_EQ(layout1.MaterialsDirectory, PlatformPath::Normalize(targetDir / "Barbarian/Materials"));
        EXPECT_EQ(layout1.TexturesDirectory, PlatformPath::Normalize(targetDir / "Barbarian/Textures"));
        EXPECT_EQ(layout1.AnimationsDirectory, PlatformPath::Normalize(targetDir / "Barbarian/Animations"));
        EXPECT_EQ(layout1.SidecarFilePath, PlatformPath::Normalize(targetDir / "Barbarian/Barbarian.bamimport.json"));

        // (3) ResolveLayout 재임포트 중복 방지 검증: 이미 모델 전용 폴더 안인 경우 Barbarian/Barbarian 중복 방지
        const fs::path dedicatedDir = targetDir / "Barbarian";
        const auto layout2 = ModelSubfolderHelper::ResolveLayout(dedicatedDir, L"Barbarian", true, true, true, true, true);
        EXPECT_TRUE(layout2.UsesDedicatedModelFolder);
        EXPECT_EQ(layout2.RootDirectory, PlatformPath::Normalize(dedicatedDir));
        EXPECT_EQ(layout2.ModelFilePath, PlatformPath::Normalize(dedicatedDir / "Barbarian.bammodel"));

        // (4) ResolveLayout 평탄화(Flat) 옵션 검증: 서브폴더 비활성화 시 모든 산출물이 RootDirectory에 배치
        const auto layoutFlat = ModelSubfolderHelper::ResolveLayout(targetDir, L"Prop_Box", false, false, false, false, false);
        EXPECT_FALSE(layoutFlat.UsesDedicatedModelFolder);
        EXPECT_EQ(layoutFlat.RootDirectory, PlatformPath::Normalize(targetDir));
        EXPECT_EQ(layoutFlat.MeshesDirectory, PlatformPath::Normalize(targetDir));
        EXPECT_EQ(layoutFlat.MaterialsDirectory, PlatformPath::Normalize(targetDir));
        EXPECT_EQ(layoutFlat.TexturesDirectory, PlatformPath::Normalize(targetDir));
        EXPECT_EQ(layoutFlat.AnimationsDirectory, PlatformPath::Normalize(targetDir));

        // (5) 파생 에셋 경로 생성 (LOD, 머티리얼, 텍스처, 애니메이션) 검증
        EXPECT_EQ(ModelSubfolderHelper::BuildMeshPath(layout1, L"Body", 0),
            PlatformPath::Normalize(targetDir / "Barbarian/Meshes/Body.bammesh"));
        EXPECT_EQ(ModelSubfolderHelper::BuildMeshPath(layout1, L"Body", 1),
            PlatformPath::Normalize(targetDir / "Barbarian/Meshes/Body_LOD1.bammesh"));
        EXPECT_EQ(ModelSubfolderHelper::BuildMeshPath(layout1, L"Body", 2),
            PlatformPath::Normalize(targetDir / "Barbarian/Meshes/Body_LOD2.bammesh"));
        EXPECT_EQ(ModelSubfolderHelper::BuildMaterialPath(layout1, L"M_Barbarian"),
            PlatformPath::Normalize(targetDir / "Barbarian/Materials/M_Barbarian.bammatinst"));
        EXPECT_EQ(ModelSubfolderHelper::BuildTexturePath(layout1, L"T_Albedo", L".png"),
            PlatformPath::Normalize(targetDir / "Barbarian/Textures/T_Albedo.png"));
        EXPECT_EQ(ModelSubfolderHelper::BuildAnimationPath(layout1, L"Walk"),
            PlatformPath::Normalize(targetDir / "Barbarian/Animations/Walk.bamanim"));

        // (6) 충돌 처리 (ResolveUniqueDirectory, ResolveUniqueFilePath) 검증
        const fs::path testCollisionDir = tempRoot / "CollisionTest";
        fs::create_directories(testCollisionDir, ec);
        fs::create_directories(testCollisionDir / "ModelA", ec);
        const auto uniqueDir1 = ModelSubfolderHelper::ResolveUniqueDirectory(testCollisionDir / "ModelA");
        EXPECT_EQ(uniqueDir1, PlatformPath::Normalize(testCollisionDir / "ModelA_1"));

        fs::create_directories(testCollisionDir / "ModelA_1", ec);
        const auto uniqueDir2 = ModelSubfolderHelper::ResolveUniqueDirectory(testCollisionDir / "ModelA");
        EXPECT_EQ(uniqueDir2, PlatformPath::Normalize(testCollisionDir / "ModelA_2"));

        const fs::path dummyFile = testCollisionDir / "Item.bammodel";
        std::ofstream(dummyFile) << "dummy";
        const auto uniqueFile1 = ModelSubfolderHelper::ResolveUniqueFilePath(dummyFile);
        EXPECT_EQ(uniqueFile1, PlatformPath::Normalize(testCollisionDir / "Item_1.bammodel"));
    }

    // 14. Phase 1: SetEngineRoot / GetEngineRoot vs GetProjectRoot 분리 검증
    {
        const fs::path customEngineRoot = PlatformPath::Normalize(tempRoot / "CustomEngineRoot");
        const fs::path customProjectRoot1 = PlatformPath::Normalize(tempRoot / "CustomProjectRoot1");
        const fs::path customProjectRoot2 = PlatformPath::Normalize(tempRoot / "CustomProjectRoot2");
        fs::create_directories(customEngineRoot, ec);
        fs::create_directories(customProjectRoot1, ec);
        fs::create_directories(customProjectRoot2, ec);

        // (1) EngineRoot 미설정 시 ProjectRoot로 폴백
        PlatformPath::SetEngineRoot({});
        PlatformPath::SetProjectRoot(customProjectRoot1);
        EXPECT_EQ(PlatformPath::GetProjectRoot(), customProjectRoot1);
        EXPECT_EQ(PlatformPath::GetEngineRoot(), customProjectRoot1);

        // (2) EngineRoot 명시 설정 후 독립성 검증
        PlatformPath::SetEngineRoot(customEngineRoot);
        EXPECT_EQ(PlatformPath::GetEngineRoot(), customEngineRoot);
        EXPECT_EQ(PlatformPath::GetProjectRoot(), customProjectRoot1);

        // (3) ProjectRoot 전환 시 EngineRoot는 유지됨을 검증
        PlatformPath::SetProjectRoot(customProjectRoot2);
        EXPECT_EQ(PlatformPath::GetEngineRoot(), customEngineRoot);
        EXPECT_EQ(PlatformPath::GetProjectRoot(), customProjectRoot2);
    }

    // 15. Phase 1: 외부 콘텐츠 폴더 가상 마운트 (Mount://<Alias>/...) 등록, 해석, 보호 검증
    {
        PlatformPath::ClearContentMounts();
        EXPECT_TRUE(PlatformPath::GetContentMounts().empty());
        EXPECT_FALSE(PlatformPath::HasContentMount("SharedKit"));

        const fs::path externalKitRoot = PlatformPath::Normalize(tempRoot / "ExternalKit");
        const fs::path externalAudioRoot = PlatformPath::Normalize(tempRoot / "ExternalAudio");
        fs::create_directories(externalKitRoot / "Props", ec);
        fs::create_directories(externalAudioRoot / "SFX", ec);

        const fs::path mockBarrelFile = externalKitRoot / "Props/Barrel.bammodel";
        const fs::path mockAudioFile = externalAudioRoot / "SFX/Explosion.wav";
        std::ofstream(mockBarrelFile) << "model data";
        std::ofstream(mockAudioFile) << "audio data";

        // (1) 마운트 등록 및 조회 검증
        ContentMountDesc kitMount;
        kitMount.Name = "SharedKit";
        kitMount.RootPath = externalKitRoot;
        kitMount.ReadOnly = true;
        kitMount.ShowUnderAssets = false;
        EXPECT_TRUE(PlatformPath::RegisterContentMount(kitMount));
        EXPECT_TRUE(PlatformPath::HasContentMount("SharedKit"));
        EXPECT_TRUE(PlatformPath::HasContentMount("sharedkit"));

        ContentMountDesc audioMount;
        audioMount.Name = "AudioLib";
        audioMount.RootPath = externalAudioRoot;
        audioMount.ReadOnly = false;
        audioMount.ShowUnderAssets = true;
        EXPECT_TRUE(PlatformPath::RegisterContentMount(audioMount));
        EXPECT_TRUE(PlatformPath::HasContentMount("AudioLib"));

        const auto mounts = PlatformPath::GetContentMounts();
        EXPECT_TRUE(mounts.size() == 2);

        // (2) 가상 URI 판별 검증
        EXPECT_TRUE(PlatformPath::IsMountUri(L"Mount://SharedKit/Props/Barrel.bammodel"));
        EXPECT_TRUE(PlatformPath::IsMountUri("mount://audiolib/sfx/explosion.wav"));
        EXPECT_TRUE(PlatformPath::IsVirtualUri(L"Mount://SharedKit/Props/Barrel.bammodel"));
        EXPECT_TRUE(PlatformPath::IsVirtualUri("mount://audiolib/sfx/explosion.wav"));
        EXPECT_FALSE(PlatformPath::IsMountUri(L"Project://Props/Barrel.bammodel"));
        EXPECT_FALSE(PlatformPath::IsMountUri(L"Engine://Shader/test.hlsl"));

        // (3) URI 해석 (ResolveMountUri & ResolveUri) 검증
        const auto resolvedKit = PlatformPath::ResolveMountUri(L"Mount://SharedKit/Props/Barrel.bammodel");
        EXPECT_EQ(resolvedKit, mockBarrelFile);
        const auto resolvedKitGeneric = PlatformPath::ResolveUri(L"Mount://SharedKit/Props/Barrel.bammodel");
        EXPECT_EQ(resolvedKitGeneric, mockBarrelFile);

        const auto resolvedAudio = PlatformPath::ResolveMountUri("mount://AudioLib/SFX/Explosion.wav");
        EXPECT_EQ(resolvedAudio, mockAudioFile);

        // (4) 디렉터리 탈출 방어 검증
        const auto escapedMount = PlatformPath::ResolveMountUri(L"Mount://SharedKit/../../Windows/System32/cmd.exe");
        EXPECT_EQ(escapedMount, externalKitRoot);

        // (5) ToVirtualUri 역변환 검증
        const auto virtualKit = PlatformPath::ToVirtualUri(mockBarrelFile);
        EXPECT_EQ(virtualKit, L"Mount://SharedKit/Props/Barrel.bammodel");
        const auto virtualAudio = PlatformPath::ToVirtualUri(mockAudioFile);
        EXPECT_EQ(virtualAudio, L"Mount://AudioLib/SFX/Explosion.wav");

        // (6) 읽기 전용 가드 (IsReadOnlyTarget & IsMountReadOnly) 검증
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(L"Mount://SharedKit/Props/Barrel.bammodel"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget("Mount://SharedKit/Props/Barrel.bammodel"));
        EXPECT_TRUE(PlatformPath::IsReadOnlyTarget(mockBarrelFile));
        EXPECT_TRUE(PlatformPath::IsMountReadOnly("SharedKit"));
        EXPECT_TRUE(PlatformPath::IsMountReadOnly(L"Mount://SharedKit/Props/Barrel.bammodel"));

        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(L"Mount://AudioLib/SFX/Explosion.wav"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget("Mount://AudioLib/SFX/Explosion.wav"));
        EXPECT_FALSE(PlatformPath::IsReadOnlyTarget(mockAudioFile));
        EXPECT_FALSE(PlatformPath::IsMountReadOnly("AudioLib"));
        EXPECT_FALSE(PlatformPath::IsMountReadOnly(L"Mount://AudioLib/SFX/Explosion.wav"));

        // (7) 마운트 해제 검증
        PlatformPath::UnregisterContentMount("SharedKit");
        EXPECT_FALSE(PlatformPath::HasContentMount("SharedKit"));
        EXPECT_TRUE(PlatformPath::ResolveMountUri(L"Mount://SharedKit/Props/Barrel.bammodel").empty());
        PlatformPath::ClearContentMounts();
        EXPECT_TRUE(PlatformPath::GetContentMounts().empty());
    }

    // 16. Phase 1: ProjectDescriptor .bamproject 저장/로드 및 ScaffoldNewProject 격리 생성 검증
    {
        const fs::path testProjDir = PlatformPath::Normalize(tempRoot / "TestGameProj");
        fs::create_directories(testProjDir, ec);
        const fs::path testProjFile = testProjDir / "TestGame.bamproject";

        // (1) .bamproject 직렬화 및 역직렬화 라운드트립 검증
        ProjectDescriptor origDesc;
        origDesc.ProjectName = "TestGame";
        origDesc.AssetDirectory = "CustomAssets";
        origDesc.ContentMounts.push_back({ "SharedLibrary", "D:/SharedLib", true, false });
        origDesc.ContentMounts.push_back({ "LocalAudio", "../Audio", false, true });
        origDesc.Modules.push_back({ "TestGameModule", "Runtime" });

        std::string saveErr;
        EXPECT_TRUE(ProjectDescriptor::SaveToFile(testProjFile, origDesc, &saveErr));
        EXPECT_TRUE(fs::exists(testProjFile));

        ProjectDescriptor loadedDesc;
        std::string loadErr;
        EXPECT_TRUE(ProjectDescriptor::LoadFromFile(testProjFile, loadedDesc, &loadErr));
        EXPECT_TRUE(loadedDesc.SchemaVersion == ProjectDescriptor::CurrentSchemaVersion);
        EXPECT_EQ(loadedDesc.ProjectName, "TestGame");
        EXPECT_EQ(loadedDesc.AssetDirectory, "CustomAssets");
        EXPECT_TRUE(loadedDesc.ContentMounts.size() == 2);
        EXPECT_EQ(loadedDesc.ContentMounts[0].Name, "SharedLibrary");
        EXPECT_EQ(loadedDesc.ContentMounts[0].Path, "D:/SharedLib");
        EXPECT_TRUE(loadedDesc.ContentMounts[0].ReadOnly);
        EXPECT_FALSE(loadedDesc.ContentMounts[0].ShowUnderAssets);
        EXPECT_EQ(loadedDesc.ContentMounts[1].Name, "LocalAudio");
        EXPECT_EQ(loadedDesc.ContentMounts[1].Path, "../Audio");
        EXPECT_FALSE(loadedDesc.ContentMounts[1].ReadOnly);
        EXPECT_TRUE(loadedDesc.ContentMounts[1].ShowUnderAssets);
        EXPECT_TRUE(loadedDesc.Modules.size() == 1);
        EXPECT_EQ(loadedDesc.Modules[0].Name, "TestGameModule");
        EXPECT_EQ(loadedDesc.Modules[0].Type, "Runtime");

        // (1-b) Phase 5: 프로젝트별 독립 C++ 모듈 소스/빌드/바이너리 경로 해석 검증
        const auto resolvedModules = loadedDesc.ResolveModules(testProjDir);
        EXPECT_TRUE(resolvedModules.size() == 1);
        EXPECT_EQ(resolvedModules[0].Name, "TestGameModule");
        EXPECT_EQ(resolvedModules[0].Type, "Runtime");
        EXPECT_EQ(resolvedModules[0].CMakeSourceRoot, PlatformPath::Normalize(testProjDir / "Source"));
        EXPECT_EQ(resolvedModules[0].SourceDirectory, PlatformPath::Normalize(testProjDir / "Source" / "TestGameModule"));

        const auto editorDebug = loadedDesc.ResolveModules(testProjDir,
            { EProjectModuleRole::Editor, true });

        const auto editorRelease = loadedDesc.ResolveModules(testProjDir,
            { EProjectModuleRole::Editor, false });

        const auto clientDebug = loadedDesc.ResolveModules(testProjDir,
            { EProjectModuleRole::Client, true });

        const auto clientRelease = loadedDesc.ResolveModules(testProjDir,
            { EProjectModuleRole::Client, false });

        const auto shippingRelease = loadedDesc.ResolveModules(testProjDir,
            { EProjectModuleRole::Shipping, false });

        EXPECT_TRUE(editorDebug.size() == 1u);
        EXPECT_TRUE(editorRelease.size() == 1u);
        EXPECT_TRUE(clientDebug.size() == 1u);
        EXPECT_TRUE(clientRelease.size() == 1u);
        EXPECT_TRUE(shippingRelease.size() == 1u);

        const auto binaryRoot = testProjDir / "Binaries" / "Win64";

        EXPECT_EQ(editorDebug[0].BinaryPath,
            PlatformPath::Normalize(binaryRoot / "Editor" / "Debug" / "TestGameModule.dll"));

        EXPECT_EQ(editorRelease[0].BinaryPath,
            PlatformPath::Normalize(binaryRoot / "Editor" / "Release" / "TestGameModule.dll"));

        EXPECT_EQ(clientDebug[0].BinaryPath,
            PlatformPath::Normalize(binaryRoot / "Client" / "Debug" / "TestGameModule.dll"));

        EXPECT_EQ(clientRelease[0].BinaryPath,
            PlatformPath::Normalize(binaryRoot / "Client" / "Release" / "TestGameModule.dll"));

        EXPECT_EQ(shippingRelease[0].BinaryPath,
            PlatformPath::Normalize(binaryRoot / "Shipping" / "Release" / "TestGameModule.dll"));

        EXPECT_EQ(editorDebug[0].BuildDirectory,
            PlatformPath::Normalize(testProjDir / "Build" / "Editor" / "Debug"));

        EXPECT_EQ(clientRelease[0].BuildDirectory,
            PlatformPath::Normalize(testProjDir / "Build" / "Client" / "Release"));

        EXPECT_TRUE(editorDebug[0].BinaryPath != editorRelease[0].BinaryPath);
        EXPECT_TRUE(editorRelease[0].BinaryPath != clientRelease[0].BinaryPath);
        EXPECT_TRUE(clientRelease[0].BinaryPath != shippingRelease[0].BinaryPath);

        const auto primaryRuntime = loadedDesc.ResolvePrimaryRuntimeModule(testProjDir);
        EXPECT_TRUE(primaryRuntime.has_value());
        EXPECT_EQ(primaryRuntime->Name, "TestGameModule");

        ProjectDescriptor emptyModDesc;
        EXPECT_FALSE(emptyModDesc.ResolvePrimaryRuntimeModule(testProjDir).has_value());

        // (2) 상대 경로 해석 도우미 검증
        const auto resolvedAssetRoot = loadedDesc.ResolveAssetRoot(testProjDir);
        EXPECT_EQ(resolvedAssetRoot, PlatformPath::Normalize(testProjDir / "CustomAssets"));

        const auto resolvedMounts = loadedDesc.ResolveContentMounts(testProjDir);
        EXPECT_TRUE(resolvedMounts.size() == 2);
        EXPECT_EQ(resolvedMounts[0].Name, "SharedLibrary");
        EXPECT_EQ(resolvedMounts[0].RootPath, PlatformPath::Normalize("D:/SharedLib"));
        EXPECT_TRUE(resolvedMounts[0].ReadOnly);
        EXPECT_EQ(resolvedMounts[1].Name, "LocalAudio");
        EXPECT_EQ(resolvedMounts[1].RootPath, PlatformPath::Normalize(testProjDir / "../Audio"));
        EXPECT_FALSE(resolvedMounts[1].ReadOnly);

        // (3) FindProjectFile 검증
        EXPECT_EQ(ProjectDescriptor::FindProjectFile(testProjDir), PlatformPath::Normalize(testProjFile));
        EXPECT_EQ(ProjectDescriptor::FindProjectFile(testProjFile), PlatformPath::Normalize(testProjFile));

        // (4) ScaffoldNewProject 격리 디렉터리 및 필수 파일 생성 검증
        const fs::path scaffoldDir = PlatformPath::Normalize(tempRoot / "ScaffoldedGame");
        fs::path createdProjFile;
        std::string scaffoldErr;
        EXPECT_TRUE(ProjectDescriptor::ScaffoldNewProject(scaffoldDir, "ScaffoldedGame", &createdProjFile, &scaffoldErr, EProjectTemplate::Empty));
        EXPECT_TRUE(fs::exists(createdProjFile));
        EXPECT_EQ(createdProjFile, PlatformPath::Normalize(scaffoldDir / "ScaffoldedGame.bamproject"));
        EXPECT_TRUE(fs::exists(scaffoldDir / "Config" / "Project.json"));
        EXPECT_TRUE(fs::exists(scaffoldDir / "Assets" / "Scenes" / "Main.bamscene.json"));

        ProjectDescriptor scaffoldDesc;
        EXPECT_TRUE(ProjectDescriptor::LoadFromFile(createdProjFile, scaffoldDesc, &loadErr));
        EXPECT_EQ(scaffoldDesc.ProjectName, "ScaffoldedGame");
        EXPECT_EQ(scaffoldDesc.AssetDirectory, "Assets");
    }

    std::cout << "[VirtualPathCheck] All virtual path and URI tests passed successfully!" << std::endl;
    return 0;
}
