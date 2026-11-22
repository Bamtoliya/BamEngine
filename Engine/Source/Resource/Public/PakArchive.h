#pragma once

#include "PakFormat.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <span>

BEGIN(Engine)

// -----------------------------------------------------------------------------
// 팩 내 개별 엔트리 메타데이터
// -----------------------------------------------------------------------------
struct PakEntryInfo
{
    std::string Uri; // 정규화된 가상 URI (예: "Project://Textures/Hero.png")
    EPakPayloadFormat PayloadFormat = EPakPayloadFormat::Raw;
    EPakCompression Compression = EPakCompression::None;
    uint64 Offset = 0; // 파일 시작 기준 페이로드 데이터 바이트 오프셋
    uint64 Size = 0;   // 페이로드 바이트 크기
    uint64 Hash64 = 0; // 페이로드 데이터의 FNV-1a 64비트 무결성 해시
};

// -----------------------------------------------------------------------------
// 리소스 팩 작성기 (결정적 순서 정렬 및 순차 스트리밍 쓰기)
// -----------------------------------------------------------------------------
class ENGINE_API PakWriter
{
public:
    PakWriter();
    ~PakWriter();

    NO_COPY(PakWriter)
    PakWriter(PakWriter&&) noexcept;
    PakWriter& operator=(PakWriter&&) noexcept;

    // 메모리 바이트로부터 엔트리 추가
    [[nodiscard]] EResult AddEntry(
        std::string_view uri,
        std::span<const uint8_t> data,
        EPakPayloadFormat format = EPakPayloadFormat::Raw,
        EPakCompression compression = EPakCompression::None);

    // 디스크 파일 경로로부터 엔트리 추가 (WriteToFile 시 순차 스트리밍)
    [[nodiscard]] EResult AddFile(
        std::string_view uri,
        const std::filesystem::path& physicalPath,
        EPakPayloadFormat format = EPakPayloadFormat::Raw,
        EPakCompression compression = EPakCompression::None);

    // 등록된 항목 수 조회
    [[nodiscard]] size_t GetEntryCount() const noexcept;

    // 대기 중인 엔트리 목록 초기화
    void Clear() noexcept;

    // 파일로 팩 작성 (엔트리 사전순 정렬을 통한 결정적 산출물 생성)
    [[nodiscard]] EResult WriteToFile(const std::filesystem::path& outputPath);

private:
    struct PendingEntry
    {
        std::string NormalizedUri;
        std::string LowercaseUri;
        EPakPayloadFormat Format = EPakPayloadFormat::Raw;
        EPakCompression Compression = EPakCompression::None;

        bool IsFromDisk = false;
        std::vector<uint8_t> MemoryData;
        std::filesystem::path SourceFilePath;
        uint64 DataSize = 0;
    };

    std::vector<PendingEntry> m_entries;
    std::unordered_set<std::string> m_lowercaseUris;
};

// -----------------------------------------------------------------------------
// 리소스 팩 리더 (색인 로드, URI 조회, 무결성 검증 읽기, 부분 범위 읽기)
// -----------------------------------------------------------------------------
class ENGINE_API PakReader
{
public:
    PakReader();
    ~PakReader();

    NO_COPY(PakReader)
    PakReader(PakReader&&) noexcept;
    PakReader& operator=(PakReader&&) noexcept;

    // 팩 파일 마운트 (헤더 및 색인 로드 + 무결성 검증)
    [[nodiscard]] EResult Mount(const std::filesystem::path& pakPath);

    // 마운트 해제 및 파일 핸들 해제
    void Unmount();

    // 마운트 상태 확인
    [[nodiscard]] bool IsMounted() const noexcept;

    // URI 포함 여부 확인 (Windows 대소문자 무시 검색 지원)
    [[nodiscard]] bool Contains(std::string_view uri) const;

    // URI로 엔트리 메타데이터 조회
    [[nodiscard]] const PakEntryInfo* FindEntry(std::string_view uri) const;

    // 전체 엔트리 바이트 읽기 (FNV-1a 64비트 무결성 해시 검증)
    [[nodiscard]] EResult ReadEntry(std::string_view uri, std::vector<uint8_t>& outBytes) const;

    // 부분 바이트 범위 읽기 (오디오 스트리밍, 대형 에셋 부분 청크 읽기용)
    [[nodiscard]] EResult ReadRange(
        std::string_view uri,
        uint64 offset,
        uint64 size,
        std::vector<uint8_t>& outBytes) const;

    // 전체 색인 엔트리 목록 조회
    [[nodiscard]] const std::vector<PakEntryInfo>& GetEntries() const noexcept;

    // 마운트된 팩 파일 경로 조회
    [[nodiscard]] const std::filesystem::path& GetPakPath() const noexcept;

private:
    std::filesystem::path m_pakPath;
    std::vector<PakEntryInfo> m_entries;
    std::unordered_map<std::string, size_t> m_uriToIndex; // lowercase URI -> index in m_entries
    bool m_isMounted = false;
    uint64 m_fileSize = 0;

    struct FileHandleImpl;
    std::unique_ptr<FileHandleImpl> m_fileHandle;
};

// -----------------------------------------------------------------------------
// 다중 팩 마운트 관리자 (다중 .bampak 등록 및 통합 URI 조회/읽기)
// -----------------------------------------------------------------------------
class ENGINE_API PakMountManager
{
public:
    PakMountManager();
    ~PakMountManager();

    NO_COPY(PakMountManager)
    PakMountManager(PakMountManager&&) noexcept;
    PakMountManager& operator=(PakMountManager&&) noexcept;

    // 팩 파일 마운트 추가
    [[nodiscard]] EResult MountPak(const std::filesystem::path& pakPath);

    // 패키지 매니페스트 기반 자동 마운트 및 검증
    [[nodiscard]] EResult MountFromPackageManifest(
        const std::filesystem::path& manifestPath,
        std::string* outStartupSceneUri = nullptr);

    // 패키지 매니페스트 기반 자동 마운트 및 검증 (편의 별칭)
    [[nodiscard]] inline EResult MountFromManifestFile(
        const std::filesystem::path& manifestPath,
        std::string* outStartupSceneUri = nullptr)
    {
        return MountFromPackageManifest(manifestPath, outStartupSceneUri);
    }

    // 전역 싱글턴 인스턴스 접근
    [[nodiscard]] static PakMountManager& Get() noexcept;

    // 모든 마운트 해제
    void UnmountAll();

    // 전체 마운트된 팩 중 URI 존재 여부 확인
    [[nodiscard]] bool HasEntry(std::string_view uri) const;

    // 통합 엔트리 메타데이터 조회
    [[nodiscard]] const PakEntryInfo* FindEntry(std::string_view uri) const;

    // URI 바이트 읽기
    [[nodiscard]] EResult TryReadBytes(std::string_view uri, std::vector<uint8_t>& outBytes) const;

    // URI 부분 범위 읽기
    [[nodiscard]] EResult TryReadRange(
        std::string_view uri,
        uint64 offset,
        uint64 size,
        std::vector<uint8_t>& outBytes) const;

    // 현재 마운트된 팩 수
    [[nodiscard]] size_t GetMountedPakCount() const noexcept;

    // 마운트된 리더 목록 접근
    [[nodiscard]] const std::vector<std::unique_ptr<PakReader>>& GetReaders() const noexcept;

private:
    std::vector<std::unique_ptr<PakReader>> m_readers;
};

END
