#include "PakArchive.h"
#include "PlatformPath.h"
#include "TerrainDataFile.h"

#include <archive/JsonArchive.h>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace Engine;

#define EXPECT_TRUE(expr) \
    if (!(expr)) { \
        std::cerr << "[FAIL] Assertion failed: " #expr " at line " << __LINE__ << std::endl; \
        return __LINE__; \
    }

#define EXPECT_FALSE(expr) \
    if (expr) { \
        std::cerr << "[FAIL] Assertion failed (expected false): " #expr " at line " << __LINE__ << std::endl; \
        return __LINE__; \
    }

#define EXPECT_EQ(a, b) \
    if ((a) != (b)) { \
        std::cerr << "[FAIL] Equality failed at line " << __LINE__ << std::endl; \
        return __LINE__; \
    }

#define EXPECT_NE(a, b) \
    if ((a) == (b)) { \
        std::cerr << "[FAIL] Inequality failed (expected not equal) at line " << __LINE__ << std::endl; \
        return __LINE__; \
    }

// -----------------------------------------------------------------------------
// 테스트 1: URI 정규화 및 거부 검증
// -----------------------------------------------------------------------------
static int TestUriNormalization()
{
    std::string out;

    // 정상 정규화 케이스
    EXPECT_TRUE(NormalizePakUri("Project://Textures/Hero.png", out));
    EXPECT_EQ(out, "Project://Textures/Hero.png");

    // 역슬래시 변환 및 스키마 대소문자 정규화
    EXPECT_TRUE(NormalizePakUri("project:\\\\Textures\\Sub\\Hero.dds", out));
    EXPECT_EQ(out, "Project://Textures/Sub/Hero.dds");

    EXPECT_TRUE(NormalizePakUri("ENGINE://Shaders/Standard.dxil", out));
    EXPECT_EQ(out, "Engine://Shaders/Standard.dxil");

    // 중복 슬래시 축약 및 현재 디렉터리(.) 처리
    EXPECT_TRUE(NormalizePakUri("Project://Textures//Sub/./Hero.png", out));
    EXPECT_EQ(out, "Project://Textures/Sub/Hero.png");

    // 디렉터리 탈출(..) 거부
    EXPECT_FALSE(NormalizePakUri("Project://../Secret.txt", out));
    EXPECT_FALSE(NormalizePakUri("Project://Textures/../../etc/passwd", out));
    EXPECT_FALSE(NormalizePakUri("Engine://..", out));

    // 비허용 마운트 및 절대 경로 거부
    EXPECT_FALSE(NormalizePakUri("Editor://Icons/Save.png", out));
    EXPECT_FALSE(NormalizePakUri("C:/Windows/System32/cmd.exe", out));
    EXPECT_FALSE(NormalizePakUri("/usr/bin/bash", out));
    EXPECT_FALSE(NormalizePakUri("Project://C:/Temp/Test.txt", out));

    // 빈 문자열 및 스키마만 있는 경우 거부
    EXPECT_FALSE(NormalizePakUri("", out));
    EXPECT_FALSE(NormalizePakUri("Project://", out));
    EXPECT_FALSE(NormalizePakUri("Engine:///", out));

    std::cout << "[PASS] TestUriNormalization" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 2: 중복 URI 거부 검증
// -----------------------------------------------------------------------------
static int TestDuplicateUriRejection()
{
    PakWriter writer;
    const std::vector<uint8_t> dummyData = { 1, 2, 3, 4 };

    // 최초 등록 성공
    EXPECT_EQ(writer.AddEntry("Project://Textures/Hero.png", dummyData), EResult::Success);

    // 동일 URI 중복 거부
    EXPECT_NE(writer.AddEntry("Project://Textures/Hero.png", dummyData), EResult::Success);

    // Windows 파일시스템 대소문자 무시 중복 거부
    EXPECT_NE(writer.AddEntry("project://textures/HERO.png", dummyData), EResult::Success);

    // 역슬래시 동일 경로 중복 거부
    EXPECT_NE(writer.AddEntry("project:\\textures\\hero.png", dummyData), EResult::Success);

    EXPECT_EQ(writer.GetEntryCount(), 1);

    std::cout << "[PASS] TestDuplicateUriRejection" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 3: 정상 쓰기, 읽기 및 ReadRange 검증
// -----------------------------------------------------------------------------
static int TestNormalWriteAndRead(const fs::path& tempDir)
{
    const fs::path pakPath = tempDir / "TestPackage.bampak";
    const fs::path externalFilePath = tempDir / "ExternalFile.bin";

    // 디스크 파일 준비
    const std::string fileContent = "This is a streamed file content from physical disk!";
    {
        std::ofstream fileOut(externalFilePath, std::ios::binary);
        fileOut.write(fileContent.data(), fileContent.size());
    }

    const std::string sceneJson = "{\"Entities\": [\"Player\", \"Enemy\"]}";
    const std::vector<uint8_t> texBytes = { 0xDD, 0x53, 0x20, 0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66 };
    const std::vector<uint8_t> shaderBytes = { 0x44, 0x58, 0x42, 0x43, 0x01, 0x02, 0x03, 0x04 };

    // 팩 작성
    {
        PakWriter writer;
        EXPECT_EQ(writer.AddEntry("Project://Scenes/Main.bamscene.json",
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(sceneJson.data()), sceneJson.size()),
            EPakPayloadFormat::Json), EResult::Success);

        EXPECT_EQ(writer.AddEntry("Project://Textures/Hero.dds", texBytes, EPakPayloadFormat::Dds), EResult::Success);
        EXPECT_EQ(writer.AddEntry("Engine://Shaders/Quad.dxil", shaderBytes, EPakPayloadFormat::Dxil), EResult::Success);

        EXPECT_EQ(writer.AddFile("Project://Data/DiskFile.bin", externalFilePath, EPakPayloadFormat::Raw), EResult::Success);

        EXPECT_EQ(writer.GetEntryCount(), 4);
        EXPECT_EQ(writer.WriteToFile(pakPath), EResult::Success);
    }

    // 팩 읽기
    {
        PakReader reader;
        EXPECT_FALSE(reader.IsMounted());
        EXPECT_EQ(reader.Mount(pakPath), EResult::Success);
        EXPECT_TRUE(reader.IsMounted());
        EXPECT_EQ(reader.GetEntries().size(), 4);

        // 대소문자 무시 Contains 및 FindEntry 검증
        EXPECT_TRUE(reader.Contains("project://scenes/MAIN.bamscene.json"));
        EXPECT_TRUE(reader.Contains("Project://Textures/Hero.dds"));
        EXPECT_TRUE(reader.Contains("Engine://Shaders/Quad.dxil"));
        EXPECT_TRUE(reader.Contains("Project://Data/DiskFile.bin"));
        EXPECT_FALSE(reader.Contains("Project://NonExistent.png"));

        const PakEntryInfo* sceneEntry = reader.FindEntry("Project://Scenes/Main.bamscene.json");
        EXPECT_TRUE(sceneEntry != nullptr);
        EXPECT_EQ(sceneEntry->PayloadFormat, EPakPayloadFormat::Json);
        EXPECT_EQ(sceneEntry->Size, sceneJson.size());

        // 전체 읽기 및 데이터 일치 검증
        std::vector<uint8_t> readBytes;
        EXPECT_EQ(reader.ReadEntry("Project://Scenes/Main.bamscene.json", readBytes), EResult::Success);
        std::string readScene(reinterpret_cast<const char*>(readBytes.data()), readBytes.size());
        EXPECT_EQ(readScene, sceneJson);

        EXPECT_EQ(reader.ReadEntry("Project://Textures/Hero.dds", readBytes), EResult::Success);
        EXPECT_EQ(readBytes, texBytes);

        EXPECT_EQ(reader.ReadEntry("Engine://Shaders/Quad.dxil", readBytes), EResult::Success);
        EXPECT_EQ(readBytes, shaderBytes);

        EXPECT_EQ(reader.ReadEntry("Project://Data/DiskFile.bin", readBytes), EResult::Success);
        std::string readDisk(reinterpret_cast<const char*>(readBytes.data()), readBytes.size());
        EXPECT_EQ(readDisk, fileContent);

        // ReadRange 부분 범위 읽기 검증
        std::vector<uint8_t> rangeBytes;
        EXPECT_EQ(reader.ReadRange("Project://Textures/Hero.dds", 2, 4, rangeBytes), EResult::Success);
        EXPECT_EQ(rangeBytes.size(), 4);
        EXPECT_EQ(rangeBytes[0], texBytes[2]);
        EXPECT_EQ(rangeBytes[1], texBytes[3]);
        EXPECT_EQ(rangeBytes[2], texBytes[4]);
        EXPECT_EQ(rangeBytes[3], texBytes[5]);

        // 범위 초과 요청 시 적절한 거부 검증
        EXPECT_EQ(reader.ReadRange("Project://Textures/Hero.dds", 5, 100, rangeBytes), EResult::InvalidArgument);

        reader.Unmount();
        EXPECT_FALSE(reader.IsMounted());
    }

    std::cout << "[PASS] TestNormalWriteAndRead" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 4: 결정적(Deterministic) 바이너리 출력 검증
// -----------------------------------------------------------------------------
static int TestDeterminism(const fs::path& tempDir)
{
    const fs::path pakPath1 = tempDir / "Deterministic1.bampak";
    const fs::path pakPath2 = tempDir / "Deterministic2.bampak";

    const std::vector<uint8_t> dataA = { 'A', 'A', 'A' };
    const std::vector<uint8_t> dataB = { 'B', 'B' };
    const std::vector<uint8_t> dataC = { 'C', 'C', 'C', 'C' };

    // 첫 번째 라이터: C, A, B 순으로 등록
    {
        PakWriter writer1;
        writer1.AddEntry("Project://C.txt", dataC);
        writer1.AddEntry("Project://A.txt", dataA);
        writer1.AddEntry("Project://B.txt", dataB);
        EXPECT_EQ(writer1.WriteToFile(pakPath1), EResult::Success);
    }

    // 두 번째 라이터: B, C, A 순으로 등록
    {
        PakWriter writer2;
        writer2.AddEntry("Project://B.txt", dataB);
        writer2.AddEntry("Project://C.txt", dataC);
        writer2.AddEntry("Project://A.txt", dataA);
        EXPECT_EQ(writer2.WriteToFile(pakPath2), EResult::Success);
    }

    // 두 파일 바이너리 비교
    auto readFileBytes = [](const fs::path& path) -> std::vector<uint8_t> {
        std::ifstream in(path, std::ios::binary);
        return std::vector<uint8_t>((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    };

    const auto bytes1 = readFileBytes(pakPath1);
    const auto bytes2 = readFileBytes(pakPath2);

    EXPECT_TRUE(!bytes1.empty());
    EXPECT_EQ(bytes1.size(), bytes2.size());
    EXPECT_EQ(bytes1, bytes2);

    std::cout << "[PASS] TestDeterminism" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 5: 파일 손상 및 변조 방어 검증
// -----------------------------------------------------------------------------
static int TestCorruptionDefense(const fs::path& tempDir)
{
    const fs::path basePakPath = tempDir / "Base.bampak";
    const std::vector<uint8_t> data1 = { 0x10, 0x20, 0x30, 0x40 };
    const std::vector<uint8_t> data2 = { 0xAA, 0xBB, 0xCC, 0xDD };

    {
        PakWriter writer;
        writer.AddEntry("Project://Data1.bin", data1);
        writer.AddEntry("Project://Data2.bin", data2);
        EXPECT_EQ(writer.WriteToFile(basePakPath), EResult::Success);
    }

    std::ifstream in(basePakPath, std::ios::binary);
    std::vector<uint8_t> validBytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();

    auto writeBytesToFile = [](const fs::path& path, const std::vector<uint8_t>& bytes) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    };

    // 5.1 잘못된 매직
    {
        auto corrupt = validBytes;
        corrupt[0] = 'X';
        corrupt[1] = 'X';
        const fs::path p = tempDir / "BadMagic.bampak";
        writeBytesToFile(p, corrupt);

        PakReader reader;
        EXPECT_NE(reader.Mount(p), EResult::Success);
    }

    // 5.2 미지원 포맷 버전
    {
        auto corrupt = validBytes;
        WriteUint32LE(corrupt.data() + 4, 999); // Version 999
        const fs::path p = tempDir / "BadVersion.bampak";
        writeBytesToFile(p, corrupt);

        PakReader reader;
        EXPECT_NE(reader.Mount(p), EResult::Success);
    }

    // 5.3 잘린 파일 (헤더 미만 크기)
    {
        std::vector<uint8_t> corrupt(20, 0);
        const fs::path p = tempDir / "TruncatedSmall.bampak";
        writeBytesToFile(p, corrupt);

        PakReader reader;
        EXPECT_NE(reader.Mount(p), EResult::Success);
    }

    // 5.4 조작된 색인 오프셋 (파일 크기 초과)
    {
        auto corrupt = validBytes;
        WriteUint64LE(corrupt.data() + 16, 10'000'000); // IndexOffset 초과
        const fs::path p = tempDir / "BadIndexOffset.bampak";
        writeBytesToFile(p, corrupt);

        PakReader reader;
        EXPECT_NE(reader.Mount(p), EResult::Success);
    }

    // 5.5 페이로드 변조 (FNV-1a 해시 무결성 검증 실패)
    {
        auto corrupt = validBytes;
        // Data1의 페이로드 바이트 1개 변조 (헤더 48바이트 바로 뒤의 첫 페이로드 데이터)
        corrupt[PAK_HEADER_SIZE] ^= 0xFF;
        const fs::path p = tempDir / "CorruptPayload.bampak";
        writeBytesToFile(p, corrupt);

        PakReader reader;
        // 색인은 유효하므로 Mount는 성공
        EXPECT_EQ(reader.Mount(p), EResult::Success);

        std::vector<uint8_t> outBytes;
        // 변조된 Data1 읽기는 무결성 검증 실패
        EXPECT_NE(reader.ReadEntry("Project://Data1.bin", outBytes), EResult::Success);
        EXPECT_TRUE(outBytes.empty());

        // 변조되지 않은 Data2 읽기는 성공
        EXPECT_EQ(reader.ReadEntry("Project://Data2.bin", outBytes), EResult::Success);
        EXPECT_EQ(outBytes, data2);
    }

    // 5.6 색인 테이블 변조 (색인 무결성 해시 검증 실패)
    {
        auto corrupt = validBytes;
        // 파일 마지막 바이트(색인 영역) 변조
        corrupt.back() ^= 0xFF;
        const fs::path p = tempDir / "CorruptIndex.bampak";
        writeBytesToFile(p, corrupt);

        PakReader reader;
        // 색인 해시 불일치로 Mount 즉시 거부
        EXPECT_NE(reader.Mount(p), EResult::Success);
    }

    std::cout << "[PASS] TestCorruptionDefense" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 6: 다중 팩 마운트 관리자 (PakMountManager) 검증
// -----------------------------------------------------------------------------
static int TestPakMountManager(const fs::path& tempDir)
{
    const fs::path enginePakPath = tempDir / "Engine.bampak";
    const fs::path projectPakPath = tempDir / "Project.bampak";

    const std::vector<uint8_t> engineShader = { 0x11, 0x22 };
    const std::vector<uint8_t> projectScene = { 0x33, 0x44, 0x55 };
    const std::vector<uint8_t> sharedTexEngine = { 0xAA };
    const std::vector<uint8_t> sharedTexProject = { 0xBB };

    // Engine.bampak 생성
    {
        PakWriter writer;
        EXPECT_EQ(writer.AddEntry("Engine://Shaders/Test.dxil", engineShader), EResult::Success);
        EXPECT_EQ(writer.AddEntry("Project://Textures/Shared.dds", sharedTexEngine), EResult::Success);
        EXPECT_EQ(writer.WriteToFile(enginePakPath), EResult::Success);
    }

    // Project.bampak 생성 (동일 URI 오버라이드 포함)
    {
        PakWriter writer;
        EXPECT_EQ(writer.AddEntry("Project://Scenes/Level.bamscene.json", projectScene), EResult::Success);
        EXPECT_EQ(writer.AddEntry("Project://Textures/Shared.dds", sharedTexProject), EResult::Success);
        EXPECT_EQ(writer.WriteToFile(projectPakPath), EResult::Success);
    }

    // PakMountManager 통합 마운트
    PakMountManager mountMgr;
    EXPECT_EQ(mountMgr.MountPak(enginePakPath), EResult::Success);
    EXPECT_EQ(mountMgr.MountPak(projectPakPath), EResult::Success);
    EXPECT_EQ(mountMgr.GetMountedPakCount(), 2);

    // 통합 조회
    EXPECT_TRUE(mountMgr.HasEntry("Engine://Shaders/Test.dxil"));
    EXPECT_TRUE(mountMgr.HasEntry("Project://Scenes/Level.bamscene.json"));
    EXPECT_FALSE(mountMgr.HasEntry("Project://NotReal.txt"));

    // 통합 읽기 및 나중 마운트 팩 우선순위(오버라이드) 검증
    std::vector<uint8_t> readBytes;
    EXPECT_EQ(mountMgr.TryReadBytes("Engine://Shaders/Test.dxil", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, engineShader);

    EXPECT_EQ(mountMgr.TryReadBytes("Project://Scenes/Level.bamscene.json", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, projectScene);

    EXPECT_EQ(mountMgr.TryReadBytes("Project://Textures/Shared.dds", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, sharedTexProject); // Project.bampak 버전이 우선

    // 모든 마운트 해제
    mountMgr.UnmountAll();
    EXPECT_EQ(mountMgr.GetMountedPakCount(), 0);
    EXPECT_FALSE(mountMgr.HasEntry("Engine://Shaders/Test.dxil"));

    std::cout << "[PASS] TestPakMountManager" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 7: 스테이징 2-팩(Engine.bampak + Project.bampak) 패키징 및 마운트 왕복 검증
// -----------------------------------------------------------------------------
static int TestTwoPakStagingRoundTrip(const fs::path& tempDir)
{
    const fs::path stagingDir = tempDir / "TwoPakStaging";
    const fs::path stagedEngine = stagingDir / "Engine" / "Resources";
    const fs::path stagedAssets = stagingDir / "Assets";
    const fs::path stagedConfig = stagingDir / "Config";
    const fs::path contentDir = stagingDir / "Content";

    fs::create_directories(stagedEngine / "Shader" / "bin" / "dxil");
    fs::create_directories(stagedEngine / "Material");
    fs::create_directories(stagedAssets / "Scenes");
    fs::create_directories(stagedAssets / "Textures");
    fs::create_directories(stagedAssets / "Audio");
    fs::create_directories(stagedConfig);
    fs::create_directories(contentDir);

    const std::vector<uint8_t> dxilBytes = { 0x44, 0x58, 0x42, 0x43, 0x10, 0x20, 0x30, 0x40 };
    const std::string uiMatJson = R"({"Material":"UIMaterial","Blend":"Alpha"})";
    const std::string introSceneJson = R"({"Scene":"Intro","Entities":["Camera","Player"]})";
    const std::vector<uint8_t> ddsBytes = { 0x44, 0x44, 0x53, 0x20, 0x7C, 0x00, 0x00, 0x00, 0xAA, 0xBB };
    const std::vector<uint8_t> oggBytes = { 0x4F, 0x67, 0x67, 0x53, 0x00, 0x02, 0x01, 0x02 };

    auto writeBinary = [](const fs::path& path, const void* data, size_t size) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    };

    const fs::path dxilPath = stagedEngine / "Shader" / "bin" / "dxil" / "DefaultPS.dxil";
    const fs::path uiMatPath = stagedEngine / "Material" / "UIMaterial.bammat.json";
    const fs::path introScenePath = stagedAssets / "Scenes" / "Intro.bamscene.json";
    const fs::path heroTexPath = stagedAssets / "Textures" / "Hero.dds";
    const fs::path themeAudioPath = stagedAssets / "Audio" / "Theme.ogg";

    writeBinary(dxilPath, dxilBytes.data(), dxilBytes.size());
    writeBinary(uiMatPath, uiMatJson.data(), uiMatJson.size());
    writeBinary(introScenePath, introSceneJson.data(), introSceneJson.size());
    writeBinary(heroTexPath, ddsBytes.data(), ddsBytes.size());
    writeBinary(themeAudioPath, oggBytes.data(), oggBytes.size());

    // Engine.bampak 작성 (Engine:// 리소스)
    const fs::path enginePakPath = contentDir / "Engine.bampak";
    {
        PakWriter engineWriter;
        EXPECT_EQ(engineWriter.AddFile("Engine://Shader/bin/dxil/DefaultPS.dxil", dxilPath, EPakPayloadFormat::Dxil), EResult::Success);
        EXPECT_EQ(engineWriter.AddFile("Engine://Material/UIMaterial.bammat.json", uiMatPath, EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(engineWriter.GetEntryCount(), 2);
        EXPECT_EQ(engineWriter.WriteToFile(enginePakPath), EResult::Success);
    }

    // Project.bampak 작성 (Project:// 리소스, 스트리밍 오디오 .ogg는 직접 파일 예외로 제외)
    const fs::path projectPakPath = contentDir / "Project.bampak";
    {
        PakWriter projectWriter;
        EXPECT_EQ(projectWriter.AddFile("Project://Scenes/Intro.bamscene.json", introScenePath, EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(projectWriter.AddFile("Project://Textures/Hero.dds", heroTexPath, EPakPayloadFormat::Dds), EResult::Success);
        EXPECT_EQ(projectWriter.GetEntryCount(), 2);
        EXPECT_EQ(projectWriter.WriteToFile(projectPakPath), EResult::Success);
    }

    // 개별 PakReader 사전 무결성 검증
    {
        PakReader engineReader;
        EXPECT_EQ(engineReader.Mount(enginePakPath), EResult::Success);
        EXPECT_EQ(engineReader.GetEntries().size(), 2);
        EXPECT_TRUE(engineReader.Contains("Engine://Shader/bin/dxil/DefaultPS.dxil"));
        EXPECT_TRUE(engineReader.Contains("Engine://Material/UIMaterial.bammat.json"));
        engineReader.Unmount();

        PakReader projectReader;
        EXPECT_EQ(projectReader.Mount(projectPakPath), EResult::Success);
        EXPECT_EQ(projectReader.GetEntries().size(), 2);
        EXPECT_TRUE(projectReader.Contains("Project://Scenes/Intro.bamscene.json"));
        EXPECT_TRUE(projectReader.Contains("Project://Textures/Hero.dds"));
        projectReader.Unmount();
    }

    // PakMountManager 2-팩 통합 마운트 및 TryReadBytes 왕복 검증
    PakMountManager mountMgr;
    EXPECT_EQ(mountMgr.MountPak(enginePakPath), EResult::Success);
    EXPECT_EQ(mountMgr.MountPak(projectPakPath), EResult::Success);
    EXPECT_EQ(mountMgr.GetMountedPakCount(), 2);

    const PakEntryInfo* dxilEntry = mountMgr.FindEntry("Engine://Shader/bin/dxil/DefaultPS.dxil");
    EXPECT_TRUE(dxilEntry != nullptr);
    EXPECT_EQ(dxilEntry->PayloadFormat, EPakPayloadFormat::Dxil);

    const PakEntryInfo* sceneEntry = mountMgr.FindEntry("project://scenes/intro.bamscene.json");
    EXPECT_TRUE(sceneEntry != nullptr);
    EXPECT_EQ(sceneEntry->PayloadFormat, EPakPayloadFormat::Json);

    const PakEntryInfo* ddsEntry = mountMgr.FindEntry("Project://Textures/Hero.dds");
    EXPECT_TRUE(ddsEntry != nullptr);
    EXPECT_EQ(ddsEntry->PayloadFormat, EPakPayloadFormat::Dds);

    std::vector<uint8_t> readBytes;
    EXPECT_EQ(mountMgr.TryReadBytes("Engine://Shader/bin/dxil/DefaultPS.dxil", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, dxilBytes);

    EXPECT_EQ(mountMgr.TryReadBytes("Engine://Material/UIMaterial.bammat.json", readBytes), EResult::Success);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(readBytes.data()), readBytes.size()), uiMatJson);

    EXPECT_EQ(mountMgr.TryReadBytes("Project://Scenes/Intro.bamscene.json", readBytes), EResult::Success);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(readBytes.data()), readBytes.size()), introSceneJson);

    EXPECT_EQ(mountMgr.TryReadBytes("Project://Textures/Hero.dds", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, ddsBytes);

    // 직접 파일 예외(.ogg)는 팩에 포함되지 않고 디스크에 유지됨을 확인
    EXPECT_FALSE(mountMgr.HasEntry("Project://Audio/Theme.ogg"));
    EXPECT_TRUE(fs::is_regular_file(themeAudioPath));

    mountMgr.UnmountAll();
    EXPECT_EQ(mountMgr.GetMountedPakCount(), 0);

    std::cout << "[PASS] TestTwoPakStagingRoundTrip" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 8: Config/PackageManifest.json 기반 패키지 자동 마운트 검증
// -----------------------------------------------------------------------------
static int TestMountFromPackageManifest(const fs::path& tempDir)
{
    const fs::path testRoot = tempDir / "ManifestMountTest";
    const fs::path configDir = testRoot / "Config";
    const fs::path contentDir = testRoot / "Content";

    std::error_code ec;
    fs::create_directories(configDir, ec);
    fs::create_directories(contentDir, ec);

    const fs::path enginePakPath = contentDir / "Engine.bampak";
    const fs::path projectPakPath = contentDir / "Project.bampak";
    const fs::path manifestPath = configDir / "PackageManifest.json";

    const std::vector<uint8_t> shaderDxil = { 0x44, 0x58, 0x42, 0x43, 0xAA, 0xBB, 0xCC, 0xDD };
    const std::string defaultMat = "{\"Material\":\"Default\"}";
    const std::string titleScene = "{\"Scene\":\"TitleScene\",\"Version\":1}";
    const std::vector<uint8_t> titleTex = { 0x44, 0x44, 0x53, 0x20, 0x01, 0x02, 0x03, 0x04 };

    // 1. Engine.bampak 생성
    {
        PakWriter engineWriter;
        EXPECT_EQ(engineWriter.AddEntry("Engine://Shaders/Standard.dxil", shaderDxil, EPakPayloadFormat::Dxil), EResult::Success);
        EXPECT_EQ(engineWriter.AddEntry("Engine://Materials/Default.bammat.json",
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(defaultMat.data()), defaultMat.size()),
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(engineWriter.GetEntryCount(), 2);
        EXPECT_EQ(engineWriter.WriteToFile(enginePakPath), EResult::Success);
    }

    // 2. Project.bampak 생성
    {
        PakWriter projectWriter;
        EXPECT_EQ(projectWriter.AddEntry("Project://Scenes/Title.bamscene.json",
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(titleScene.data()), titleScene.size()),
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(projectWriter.AddEntry("Project://Textures/Title.dds", titleTex, EPakPayloadFormat::Dds), EResult::Success);
        EXPECT_EQ(projectWriter.GetEntryCount(), 2);
        EXPECT_EQ(projectWriter.WriteToFile(projectPakPath), EResult::Success);
    }

    const uint64 enginePakSize = fs::file_size(enginePakPath);
    const uint64 projectPakSize = fs::file_size(projectPakPath);

    // 3. Config/PackageManifest.json 작성
    {
        std::ofstream out(manifestPath, std::ios::binary | std::ios::trunc);
        out << "{\n"
            << "  \"SchemaVersion\": 1,\n"
            << "  \"StartupSceneUri\": \"Project://Scenes/Title.bamscene.json\",\n"
            << "  \"StartupManifestUri\": \"Engine://Data/Startup/Manifest.json\",\n"
            << "  \"RenderPipelineProfileUri\": \"Engine://RenderPipeline/Deferred.json\",\n"
            << "  \"BootstrapFiles\": [\"Config/Project.json\"],\n"
            << "  \"Paks\": [\n"
            << "    {\n"
            << "      \"Path\": \"Content/Engine.bampak\",\n"
            << "      \"MountScheme\": \"Engine://\",\n"
            << "      \"FormatVersion\": 1,\n"
            << "      \"EntryCount\": 2,\n"
            << "      \"ByteSize\": " << enginePakSize << ",\n"
            << "      \"Hash64\": 0,\n"
            << "      \"IntegrityVerified\": true\n"
            << "    },\n"
            << "    {\n"
            << "      \"Path\": \"Content/Project.bampak\",\n"
            << "      \"MountScheme\": \"Project://\",\n"
            << "      \"FormatVersion\": 1,\n"
            << "      \"EntryCount\": 2,\n"
            << "      \"ByteSize\": " << projectPakSize << ",\n"
            << "      \"Hash64\": 0,\n"
            << "      \"IntegrityVerified\": true\n"
            << "    }\n"
            << "  ],\n"
            << "  \"IntegrityVerified\": true\n"
            << "}\n";
    }

    // 4. MountFromPackageManifest 검증
    PakMountManager::Get().UnmountAll();
    EXPECT_EQ(PakMountManager::Get().GetMountedPakCount(), 0);

    std::string startupSceneUri;
    EXPECT_EQ(PakMountManager::Get().MountFromPackageManifest(manifestPath, &startupSceneUri), EResult::Success);
    EXPECT_EQ(startupSceneUri, "Project://Scenes/Title.bamscene.json");
    EXPECT_EQ(PakMountManager::Get().GetMountedPakCount(), 2);

    // 5. 통합 엔트리 조회 및 데이터 검증
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Engine://Shaders/Standard.dxil"));
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Engine://Materials/Default.bammat.json"));
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Project://Scenes/Title.bamscene.json"));
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Project://Textures/Title.dds"));
    EXPECT_FALSE(PakMountManager::Get().HasEntry("Project://NonExistent.png"));

    std::vector<uint8_t> readBytes;
    EXPECT_EQ(PakMountManager::Get().TryReadBytes("Engine://Shaders/Standard.dxil", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, shaderDxil);

    EXPECT_EQ(PakMountManager::Get().TryReadBytes("Engine://Materials/Default.bammat.json", readBytes), EResult::Success);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(readBytes.data()), readBytes.size()), defaultMat);

    EXPECT_EQ(PakMountManager::Get().TryReadBytes("Project://Scenes/Title.bamscene.json", readBytes), EResult::Success);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(readBytes.data()), readBytes.size()), titleScene);

    EXPECT_EQ(PakMountManager::Get().TryReadBytes("Project://Textures/Title.dds", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, titleTex);

    // 6. UnmountAll 검증
    PakMountManager::Get().UnmountAll();
    EXPECT_EQ(PakMountManager::Get().GetMountedPakCount(), 0);
    EXPECT_FALSE(PakMountManager::Get().HasEntry("Project://Scenes/Title.bamscene.json"));

    // 7. PakArchives / RelativePath / TotalBytes 포맷 호환 검증 및 MountFromManifestFile 별칭 검증
    const fs::path altManifestPath = configDir / "AltPackageManifest.json";
    {
        std::ofstream out(altManifestPath, std::ios::binary | std::ios::trunc);
        out << "{\n"
            << "  \"SchemaVersion\": 1,\n"
            << "  \"StartupSceneUri\": \"Project://Scenes/Title.bamscene.json\",\n"
            << "  \"PakArchives\": [\n"
            << "    {\n"
            << "      \"RelativePath\": \"Content/Engine.bampak\",\n"
            << "      \"EntryCount\": 2,\n"
            << "      \"TotalBytes\": " << enginePakSize << ",\n"
            << "      \"FileHash64\": 12345\n"
            << "    },\n"
            << "    {\n"
            << "      \"RelativePath\": \"Content/Project.bampak\",\n"
            << "      \"EntryCount\": 2,\n"
            << "      \"TotalBytes\": " << projectPakSize << ",\n"
            << "      \"FileHash64\": 67890\n"
            << "    }\n"
            << "  ]\n"
            << "}\n";
    }

    startupSceneUri.clear();
    EXPECT_EQ(PakMountManager::Get().MountFromManifestFile(altManifestPath, &startupSceneUri), EResult::Success);
    EXPECT_EQ(startupSceneUri, "Project://Scenes/Title.bamscene.json");
    EXPECT_EQ(PakMountManager::Get().GetMountedPakCount(), 2);
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Engine://Shaders/Standard.dxil"));

    PakMountManager::Get().UnmountAll();
    EXPECT_EQ(PakMountManager::Get().GetMountedPakCount(), 0);

    std::cout << "[PASS] TestMountFromPackageManifest" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 9: 의존성 기반 패키징 검증 (참조된 에셋 포함, 미참조 제외, AdditionalAssets 포함)
// -----------------------------------------------------------------------------
static int TestDependencyBasedPackaging(const fs::path& tempDir)
{
    const fs::path rootDir = tempDir / "DependencyPackagingTest";
    const fs::path projectAssetDir = rootDir / "ProjectSource" / "Assets";
    const fs::path engineResourceDir = rootDir / "EngineSource" / "Resources";
    const fs::path packageStagingDir = rootDir / "Staging";
    const fs::path contentDir = packageStagingDir / "Content";
    const fs::path configDir = packageStagingDir / "Config";
    const fs::path stagedAssets = packageStagingDir / "Assets";

    std::error_code ec;
    fs::create_directories(projectAssetDir / "Scenes", ec);
    fs::create_directories(projectAssetDir / "Textures", ec);
    fs::create_directories(projectAssetDir / "Materials", ec);
    fs::create_directories(projectAssetDir / "Audio", ec);
    fs::create_directories(projectAssetDir / "Data", ec);

    fs::create_directories(engineResourceDir / "Shaders", ec);
    fs::create_directories(engineResourceDir / "Materials", ec);
    fs::create_directories(engineResourceDir / "Textures", ec);

    fs::create_directories(contentDir, ec);
    fs::create_directories(configDir, ec);
    fs::create_directories(stagedAssets / "Audio", ec);

    auto writeBinary = [](const fs::path& path, const void* data, size_t size) {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data), static_cast<std::streamsize>(size));
    };

    // 1. 소스 에셋 준비
    // (A) 참조되는 에셋 (Main -> Hero.dds, Hero.bammat.json -> Standard.dxil)
    const std::string startupSceneJson =
        "{\"Scene\":\"Main\",\"Textures\":[\"Project://Textures/Hero.dds\"],"
        "\"Material\":\"Project://Materials/Hero.bammat.json\"}";
    const std::vector<uint8_t> heroTexDds = { 0x44, 0x44, 0x53, 0x20, 0x01, 0x02, 0x03, 0x04 };
    const std::string heroMatJson = "{\"Material\":\"Hero\",\"Shader\":\"Engine://Shaders/Standard.dxil\"}";
    const std::vector<uint8_t> standardDxil = { 0x44, 0x58, 0x42, 0x43, 0xAA, 0xBB, 0xCC, 0xDD };
    const std::string defaultMatJson = "{\"Material\":\"DefaultFallback\"}";

    // (B) AdditionalAssets로 명시된 에셋
    const std::string additionalConfigJson = "{\"GameMode\":\"HeroChallenge\",\"Version\":2}";
    const std::vector<uint8_t> additionalBgmOgg = { 0x4F, 0x67, 0x67, 0x53, 0x00, 0x02, 0xAA, 0xBB };

    // (C) 미참조 에셋 (패키지에서 제외되어야 함)
    const std::string unusedSceneJson = "{\"Scene\":\"UnusedTestLevel\"}";
    const std::vector<uint8_t> unusedTexDds = { 0x44, 0x44, 0x53, 0x20, 0x99, 0x99, 0x99, 0x99 };
    const std::string unusedMatJson = "{\"Material\":\"UnusedMat\"}";
    const std::vector<uint8_t> unusedAudioOgg = { 0x4F, 0x67, 0x67, 0x53, 0x00, 0x02, 0xFF, 0xFF };
    const std::vector<uint8_t> unusedEngineTex = { 0x89, 0x50, 0x4E, 0x47, 0x00, 0x00, 0x00, 0x00 };

    const fs::path mainScenePath = projectAssetDir / "Scenes" / "Main.bamscene.json";
    const fs::path heroTexPath = projectAssetDir / "Textures" / "Hero.dds";
    const fs::path heroMatPath = projectAssetDir / "Materials" / "Hero.bammat.json";
    const fs::path addConfigPath = projectAssetDir / "Data" / "AdditionalConfig.json";
    const fs::path addBgmPath = projectAssetDir / "Audio" / "AdditionalBGM.ogg";

    const fs::path unusedScenePath = projectAssetDir / "Scenes" / "UnusedScene.bamscene.json";
    const fs::path unusedTexPath = projectAssetDir / "Textures" / "UnusedTex.dds";
    const fs::path unusedMatPath = projectAssetDir / "Materials" / "UnusedMat.bammat.json";
    const fs::path unusedAudioPath = projectAssetDir / "Audio" / "UnusedAudio.ogg";

    const fs::path standardDxilPath = engineResourceDir / "Shaders" / "Standard.dxil";
    const fs::path defaultMatPath = engineResourceDir / "Materials" / "Default.bammat.json";
    const fs::path unusedEngineTexPath = engineResourceDir / "Textures" / "UnusedEngineTex.png";

    writeBinary(mainScenePath, startupSceneJson.data(), startupSceneJson.size());
    writeBinary(heroTexPath, heroTexDds.data(), heroTexDds.size());
    writeBinary(heroMatPath, heroMatJson.data(), heroMatJson.size());
    writeBinary(addConfigPath, additionalConfigJson.data(), additionalConfigJson.size());
    writeBinary(addBgmPath, additionalBgmOgg.data(), additionalBgmOgg.size());

    writeBinary(unusedScenePath, unusedSceneJson.data(), unusedSceneJson.size());
    writeBinary(unusedTexPath, unusedTexDds.data(), unusedTexDds.size());
    writeBinary(unusedMatPath, unusedMatJson.data(), unusedMatJson.size());
    writeBinary(unusedAudioPath, unusedAudioOgg.data(), unusedAudioOgg.size());

    writeBinary(standardDxilPath, standardDxil.data(), standardDxil.size());
    writeBinary(defaultMatPath, defaultMatJson.data(), defaultMatJson.size());
    writeBinary(unusedEngineTexPath, unusedEngineTex.data(), unusedEngineTex.size());

    // 2. 의존성 기반 수집 및 스테이징 시뮬레이션
    // - 의존성 분석 결과:
    //   Referenced: Main.bamscene.json, Textures/Hero.dds, Materials/Hero.bammat.json, Engine Shaders/Standard.dxil
    //   AdditionalAssets: Data/AdditionalConfig.json, Audio/AdditionalBGM.ogg
    //   Essential Engine Fallbacks: Materials/Default.bammat.json
    //   Unreferenced (제외): UnusedScene, UnusedTex, UnusedMat, UnusedAudio, UnusedEngineTex
    const fs::path enginePakPath = contentDir / "Engine.bampak";
    const fs::path projectPakPath = contentDir / "Project.bampak";

    // Engine.bampak 생성 (의존성 필수 리소스만 포함, 미참조 텍스처 제외)
    {
        PakWriter engineWriter;
        EXPECT_EQ(engineWriter.AddFile("Engine://Shaders/Standard.dxil", standardDxilPath,
            EPakPayloadFormat::Dxil), EResult::Success);
        EXPECT_EQ(engineWriter.AddFile("Engine://Materials/Default.bammat.json", defaultMatPath,
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(engineWriter.GetEntryCount(), 2);
        EXPECT_EQ(engineWriter.WriteToFile(enginePakPath), EResult::Success);
    }

    // Project.bampak 생성 (참조 씬/에셋 + AdditionalAssets 포함, 스트리밍 오디오는 직렬 파일 예외로 팩 제외)
    {
        PakWriter projectWriter;
        EXPECT_EQ(projectWriter.AddFile("Project://Scenes/Main.bamscene.json", mainScenePath,
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(projectWriter.AddFile("Project://Textures/Hero.dds", heroTexPath,
            EPakPayloadFormat::Dds), EResult::Success);
        EXPECT_EQ(projectWriter.AddFile("Project://Materials/Hero.bammat.json", heroMatPath,
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(projectWriter.AddFile("Project://Data/AdditionalConfig.json", addConfigPath,
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(projectWriter.GetEntryCount(), 4);
        EXPECT_EQ(projectWriter.WriteToFile(projectPakPath), EResult::Success);
    }

    // AdditionalAssets 중 스트리밍 오디오는 DirectFileExceptions로 디스크에 스테이징
    const fs::path stagedAddBgmPath = stagedAssets / "Audio" / "AdditionalBGM.ogg";
    fs::copy_file(addBgmPath, stagedAddBgmPath, fs::copy_options::overwrite_existing, ec);
    EXPECT_FALSE(ec);

    // 미참조 오디오는 스테이징되지 않음
    const fs::path stagedUnusedAudioPath = stagedAssets / "Audio" / "UnusedAudio.ogg";
    EXPECT_FALSE(fs::exists(stagedUnusedAudioPath));

    // 3. PackageManifest.json 작성
    const uint64 enginePakSize = fs::file_size(enginePakPath);
    const uint64 projectPakSize = fs::file_size(projectPakPath);
    const fs::path manifestPath = configDir / "PackageManifest.json";
    {
        std::ofstream out(manifestPath, std::ios::binary | std::ios::trunc);
        out << "{\n"
            << "  \"SchemaVersion\": 1,\n"
            << "  \"StartupSceneUri\": \"Project://Scenes/Main.bamscene.json\",\n"
            << "  \"StartupManifestUri\": \"\",\n"
            << "  \"RenderPipelineProfileUri\": \"\",\n"
            << "  \"BootstrapFiles\": [\"Config/Project.json\"],\n"
            << "  \"DirectFileExceptions\": [\"Assets/Audio/AdditionalBGM.ogg\"],\n"
            << "  \"Paks\": [\n"
            << "    {\n"
            << "      \"Path\": \"Content/Engine.bampak\",\n"
            << "      \"MountScheme\": \"Engine://\",\n"
            << "      \"FormatVersion\": 1,\n"
            << "      \"EntryCount\": 2,\n"
            << "      \"ByteSize\": " << enginePakSize << ",\n"
            << "      \"Hash64\": 0,\n"
            << "      \"IntegrityVerified\": true\n"
            << "    },\n"
            << "    {\n"
            << "      \"Path\": \"Content/Project.bampak\",\n"
            << "      \"MountScheme\": \"Project://\",\n"
            << "      \"FormatVersion\": 1,\n"
            << "      \"EntryCount\": 4,\n"
            << "      \"ByteSize\": " << projectPakSize << ",\n"
            << "      \"Hash64\": 0,\n"
            << "      \"IntegrityVerified\": true\n"
            << "    }\n"
            << "  ],\n"
            << "  \"IntegrityVerified\": true\n"
            << "}\n";
    }

    // 4. PakMountManager 마운트 및 검증
    PakMountManager::Get().UnmountAll();
    EXPECT_EQ(PakMountManager::Get().GetMountedPakCount(), 0);

    std::string startupSceneUri;
    EXPECT_EQ(PakMountManager::Get().MountFromPackageManifest(manifestPath, &startupSceneUri), EResult::Success);
    EXPECT_EQ(startupSceneUri, "Project://Scenes/Main.bamscene.json");
    EXPECT_EQ(PakMountManager::Get().GetMountedPakCount(), 2);

    // (A) 참조된 에셋 포함 확인
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Project://Scenes/Main.bamscene.json"));
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Project://Textures/Hero.dds"));
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Project://Materials/Hero.bammat.json"));
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Engine://Shaders/Standard.dxil"));
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Engine://Materials/Default.bammat.json"));

    // (B) AdditionalAssets 포함 확인
    EXPECT_TRUE(PakMountManager::Get().HasEntry("Project://Data/AdditionalConfig.json"));
    // 직접 파일 예외인 AdditionalAssets 오디오는 팩에 없고 디스크에 존재
    EXPECT_FALSE(PakMountManager::Get().HasEntry("Project://Audio/AdditionalBGM.ogg"));
    EXPECT_TRUE(fs::is_regular_file(stagedAddBgmPath));

    // (C) 미참조 에셋 제외 확인 (팩 및 스테이징에 미존재)
    EXPECT_FALSE(PakMountManager::Get().HasEntry("Project://Scenes/UnusedScene.bamscene.json"));
    EXPECT_FALSE(PakMountManager::Get().HasEntry("Project://Textures/UnusedTex.dds"));
    EXPECT_FALSE(PakMountManager::Get().HasEntry("Project://Materials/UnusedMat.bammat.json"));
    EXPECT_FALSE(PakMountManager::Get().HasEntry("Project://Audio/UnusedAudio.ogg"));
    EXPECT_FALSE(PakMountManager::Get().HasEntry("Engine://Textures/UnusedEngineTex.png"));
    EXPECT_FALSE(fs::exists(stagedUnusedAudioPath));

    // (D) 데이터 페이로드 검증
    std::vector<uint8_t> readBytes;
    EXPECT_EQ(PakMountManager::Get().TryReadBytes("Project://Textures/Hero.dds", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, heroTexDds);

    EXPECT_EQ(PakMountManager::Get().TryReadBytes("Project://Data/AdditionalConfig.json", readBytes), EResult::Success);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(readBytes.data()), readBytes.size()), additionalConfigJson);

    EXPECT_EQ(PakMountManager::Get().TryReadBytes("Engine://Shaders/Standard.dxil", readBytes), EResult::Success);
    EXPECT_EQ(readBytes, standardDxil);

    PakMountManager::Get().UnmountAll();
    EXPECT_EQ(PakMountManager::Get().GetMountedPakCount(), 0);

    std::cout << "[PASS] TestDependencyBasedPackaging" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 테스트 10: 엔진 로더 통합 경계 (pak 우선/디스크 폴백, Archive 디코딩, TerrainDataFile, 변조 폴백 차단)
// -----------------------------------------------------------------------------
static int TestLoaderPakFallbackAndTerrain(const fs::path& tempDir)
{
    const fs::path rootDir = tempDir / "LoaderIntegrationTest";
    const fs::path assetsDir = rootDir / "Assets";
    const fs::path engineResDir = rootDir / "Engine" / "Resources";
    const fs::path pakPath = rootDir / "Project.bampak";
    const fs::path corruptPakPath = rootDir / "CorruptOverride.bampak";

    std::error_code ec;
    fs::create_directories(assetsDir / "Data", ec);
    fs::create_directories(assetsDir / "Terrains", ec);
    fs::create_directories(engineResDir, ec);

    const fs::path prevProjectRoot = PlatformPath::GetProjectRoot();
    const fs::path prevAssetRoot = PlatformPath::GetProjectAssetRoot();
    const fs::path prevEngineRoot = PlatformPath::GetEngineResourceRoot();

    struct RootRestoreGuard
    {
        fs::path ProjectRoot;
        fs::path AssetRoot;
        fs::path EngineRoot;
        ~RootRestoreGuard()
        {
            PakMountManager::Get().UnmountAll();
            PlatformPath::SetProjectRoot(ProjectRoot);
            PlatformPath::SetProjectAssetRoot(AssetRoot);
            PlatformPath::SetEngineResourceRoot(EngineRoot);
        }
    } restoreGuard{ prevProjectRoot, prevAssetRoot, prevEngineRoot };

    PlatformPath::SetProjectRoot(rootDir);
    PlatformPath::SetProjectAssetRoot(assetsDir);
    PlatformPath::SetEngineResourceRoot(engineResDir);

    // 1. 디스크 전용 파일 및 오버라이드용 디스크 파일 작성
    const std::string diskOnlyJson = R"({"Source":"DiskOnly","Value":42})";
    const std::string diskOverrideJson = R"({"Source":"DiskVersion","Value":1})";
    const std::string pakOverrideJson = R"({"Source":"PakVersion","Value":2})";
    const std::string pakOnlyMatJson = R"({"MaterialName":"PakOnlyMat","Roughness":0.5})";

    {
        std::ofstream out(assetsDir / "Data" / "DiskOnly.json", std::ios::binary | std::ios::trunc);
        out.write(diskOnlyJson.data(), static_cast<std::streamsize>(diskOnlyJson.size()));
    }
    {
        std::ofstream out(assetsDir / "Data" / "Override.json", std::ios::binary | std::ios::trunc);
        out.write(diskOverrideJson.data(), static_cast<std::streamsize>(diskOverrideJson.size()));
    }

    // 2. .bamterrain v2 파일 생성 후 pak에 넣고 디스크 원본 삭제
    const fs::path tempTerrainPath = assetsDir / "Terrains" / "Test_Terrain.bamterrain";
    TerrainDataContent expectedTerrain;
    expectedTerrain.Resolution = 4; // (4+1)^2 = 25 samples
    constexpr size_t sampleCount = 25;
    expectedTerrain.Heights.resize(sampleCount);
    expectedTerrain.Splat.resize(sampleCount * 4);
    expectedTerrain.SplatExtra.resize(sampleCount * 4);
    expectedTerrain.Attributes.resize(sampleCount);
    for (size_t i = 0; i < sampleCount; ++i)
    {
        expectedTerrain.Heights[i] = static_cast<f32>(i) / static_cast<f32>(sampleCount - 1);
        expectedTerrain.Splat[i * 4 + 0] = static_cast<uint8>(i * 10);
        expectedTerrain.Splat[i * 4 + 1] = static_cast<uint8>(255 - i * 10);
        expectedTerrain.Splat[i * 4 + 2] = 0;
        expectedTerrain.Splat[i * 4 + 3] = 0;
        expectedTerrain.SplatExtra[i * 4 + 0] = static_cast<uint8>(i);
        expectedTerrain.SplatExtra[i * 4 + 1] = 0;
        expectedTerrain.SplatExtra[i * 4 + 2] = 0;
        expectedTerrain.SplatExtra[i * 4 + 3] = 0;
        expectedTerrain.Attributes[i] = static_cast<uint8>(i % 3);
    }
    EXPECT_TRUE(TerrainDataFile::Write(tempTerrainPath, expectedTerrain));

    // 3. Project.bampak 작성
    {
        PakWriter writer;
        EXPECT_EQ(writer.AddEntry("Project://Materials/PakOnly.bammat.json",
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pakOnlyMatJson.data()), pakOnlyMatJson.size()),
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(writer.AddEntry("Project://Data/Override.json",
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pakOverrideJson.data()), pakOverrideJson.size()),
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(writer.AddFile("Project://Terrains/Test_Terrain.bamterrain", tempTerrainPath,
            EPakPayloadFormat::Raw), EResult::Success);
        EXPECT_EQ(writer.WriteToFile(pakPath), EResult::Success);
    }

    // 디스크 원본 지형 파일 삭제 -> 오직 pak에만 존재
    fs::remove(tempTerrainPath, ec);
    EXPECT_FALSE(fs::exists(tempTerrainPath));

    PakMountManager::Get().UnmountAll();
    EXPECT_EQ(PakMountManager::Get().MountPak(pakPath), EResult::Success);

    // 4. ExistsOrOnDisk 검증 (가상 URI + 물리 경로 역매핑 + 디스크 폴백)
    EXPECT_TRUE(PakMountManager::Get().ExistsOrOnDisk("Project://Materials/PakOnly.bammat.json"));
    EXPECT_TRUE(PakMountManager::Get().ExistsOrOnDisk(PlatformPath::ResolveUri("Project://Materials/PakOnly.bammat.json")));
    EXPECT_TRUE(PakMountManager::Get().ExistsOrOnDisk("Project://Data/DiskOnly.json"));
    EXPECT_TRUE(PakMountManager::Get().ExistsOrOnDisk("Project://Terrains/Test_Terrain.bamterrain"));
    EXPECT_FALSE(PakMountManager::Get().ExistsOrOnDisk("Project://Missing/DoesNotExist.json"));

    // 5. ReadBytesWithFallback 검증 (pak 우선 오버라이드 + 디스크 폴백)
    std::vector<uint8_t> bytes;
    EXPECT_EQ(PakMountManager::Get().ReadBytesWithFallback("Project://Data/Override.json", bytes), EResult::Success);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()), pakOverrideJson);

    EXPECT_EQ(PakMountManager::Get().ReadBytesWithFallback("Project://Data/DiskOnly.json", bytes), EResult::Success);
    EXPECT_EQ(std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size()), diskOnlyJson);

    // 6. LoadArchiveWithFallback 검증 (pak 전용 JSON + 디스크 폴백 JSON)
    {
        archive::JsonArchive ar;
        EXPECT_TRUE(static_cast<bool>(PakMountManager::Get().LoadArchiveWithFallback(ar, "Project://Materials/PakOnly.bammat.json")));
        std::string matName;
        EXPECT_TRUE(static_cast<bool>(ar.Read("MaterialName", matName)));
        EXPECT_EQ(matName, "PakOnlyMat");
    }
    {
        archive::JsonArchive ar;
        EXPECT_TRUE(static_cast<bool>(PakMountManager::Get().LoadArchiveWithFallback(ar, "Project://Data/DiskOnly.json")));
        int32 value = 0;
        EXPECT_TRUE(static_cast<bool>(ar.Read("Value", value)));
        EXPECT_EQ(value, 42);
    }

    // 7. TerrainDataFile::Read pak 로드 검증 (가상 URI 및 ResolveUri 물리 경로 모두)
    for (const fs::path& queryPath : { fs::path("Project://Terrains/Test_Terrain.bamterrain"),
                                       PlatformPath::ResolveUri("Project://Terrains/Test_Terrain.bamterrain") })
    {
        TerrainDataContent loaded;
        EXPECT_TRUE(TerrainDataFile::Read(queryPath, loaded));
        EXPECT_EQ(loaded.Resolution, expectedTerrain.Resolution);
        EXPECT_EQ(loaded.Heights.size(), expectedTerrain.Heights.size());
        EXPECT_EQ(loaded.Splat, expectedTerrain.Splat);
        EXPECT_EQ(loaded.SplatExtra, expectedTerrain.SplatExtra);
        EXPECT_EQ(loaded.Attributes, expectedTerrain.Attributes);
        for (size_t i = 0; i < sampleCount; ++i)
            EXPECT_TRUE(std::fabs(loaded.Heights[i] - expectedTerrain.Heights[i]) <= (1.0f / 65535.0f + 1e-6f));
    }

    // 8. 변조된 팩 엔트리가 디스크 정상 파일로 조용히 폴백되지 않는지 검증
    {
        PakWriter writer;
        EXPECT_EQ(writer.AddEntry("Project://Data/Override.json",
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(pakOverrideJson.data()), pakOverrideJson.size()),
            EPakPayloadFormat::Json), EResult::Success);
        EXPECT_EQ(writer.WriteToFile(corruptPakPath), EResult::Success);

        std::ifstream in(corruptPakPath, std::ios::binary);
        std::vector<uint8_t> raw((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        in.close();
        raw[PAK_HEADER_SIZE] ^= 0xFF; // 페이로드 1바이트 변조
        std::ofstream out(corruptPakPath, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size()));
    }

    PakMountManager::Get().UnmountAll();
    EXPECT_EQ(PakMountManager::Get().MountPak(corruptPakPath), EResult::Success);

    EXPECT_NE(PakMountManager::Get().ReadBytesWithFallback("Project://Data/Override.json", bytes), EResult::Success);
    {
        archive::JsonArchive ar;
        EXPECT_FALSE(static_cast<bool>(PakMountManager::Get().LoadArchiveWithFallback(ar, "Project://Data/Override.json")));
    }

    PakMountManager::Get().UnmountAll();
    std::cout << "[PASS] TestLoaderPakFallbackAndTerrain" << std::endl;
    return 0;
}

// -----------------------------------------------------------------------------
// 진입점
// -----------------------------------------------------------------------------
int main()
{
    const fs::path tempDir = fs::temp_directory_path() / "BamEngine_PakArchiveCheck";
    std::error_code ec;
    fs::remove_all(tempDir, ec);
    fs::create_directories(tempDir, ec);

    struct CleanupGuard
    {
        fs::path Path;
        ~CleanupGuard()
        {
            std::error_code err;
            fs::remove_all(Path, err);
        }
    } guard{ tempDir };

    std::cout << "=== Running PakArchiveCheck Tests ===" << std::endl;

    if (int res = TestUriNormalization(); res != 0) return res;
    if (int res = TestDuplicateUriRejection(); res != 0) return res;
    if (int res = TestNormalWriteAndRead(tempDir); res != 0) return res;
    if (int res = TestDeterminism(tempDir); res != 0) return res;
    if (int res = TestCorruptionDefense(tempDir); res != 0) return res;
    if (int res = TestPakMountManager(tempDir); res != 0) return res;
    if (int res = TestTwoPakStagingRoundTrip(tempDir); res != 0) return res;
    if (int res = TestMountFromPackageManifest(tempDir); res != 0) return res;
    if (int res = TestDependencyBasedPackaging(tempDir); res != 0) return res;
    if (int res = TestLoaderPakFallbackAndTerrain(tempDir); res != 0) return res;

    std::cout << "=== All PakArchiveCheck Tests Passed Successfully! ===" << std::endl;
    return 0;
}
