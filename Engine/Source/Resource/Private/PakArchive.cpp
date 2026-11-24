#include "PakArchive.h"
#include "PlatformPath.h"
#include "Functions.h"

#include <archive/ArchiveInterface.h>
#include <algorithm>
#include <fstream>
#include <mutex>
#include <system_error>
#include <glaze/glaze.hpp>

namespace Engine::PakDetail
{
    struct ManifestPakEntry
    {
        std::string Path;
        std::string RelativePath;
        std::string MountScheme;
        uint32_t FormatVersion = 1;
        uint64_t EntryCount = 0;
        uint64_t ByteSize = 0;
        uint64_t TotalBytes = 0;
        uint64_t Hash64 = 0;
        uint64_t FileHash64 = 0;
        bool IntegrityVerified = false;
    };

    struct ManifestConfig
    {
        uint32_t SchemaVersion = 1;
        std::string StartupSceneUri;
        std::string StartupManifestUri;
        std::string RenderPipelineProfileUri;
        std::vector<std::string> BootstrapFiles;
        std::vector<std::string> DirectFileExceptions;
        std::vector<ManifestPakEntry> Paks;
        std::vector<ManifestPakEntry> PakArchives;
        bool IntegrityVerified = false;
    };
}

BEGIN(Engine)

// -----------------------------------------------------------------------------
// PakReader 파일 핸들 및 스레드 동기화 구현체
// -----------------------------------------------------------------------------
struct PakReader::FileHandleImpl
{
    std::ifstream Stream;
    mutable std::mutex Mutex;
};

// =============================================================================
// PakWriter 구현
// =============================================================================
PakWriter::PakWriter() = default;
PakWriter::~PakWriter() = default;

PakWriter::PakWriter(PakWriter&&) noexcept = default;
PakWriter& PakWriter::operator=(PakWriter&&) noexcept = default;

EResult PakWriter::AddEntry(
    std::string_view uri,
    std::span<const uint8_t> data,
    EPakPayloadFormat format,
    EPakCompression compression)
{
    std::string normalized;
    if (!NormalizePakUri(uri, normalized))
    {
        return EResult::InvalidArgument;
    }

    std::string lower = ToLowerPakUri(normalized);
    if (m_lowercaseUris.contains(lower))
    {
        // 중복 URI 거부 (Windows 파일시스템 대소문자 무시 충돌 포함)
        return EResult::InvalidArgument;
    }

    if (data.size() > PAK_MAX_ENTRY_BYTES)
    {
        return EResult::InvalidArgument;
    }

    if (m_entries.size() >= PAK_MAX_ENTRIES)
    {
        return EResult::OutOfMemory;
    }

    PendingEntry entry;
    entry.NormalizedUri = std::move(normalized);
    entry.LowercaseUri = lower;
    entry.Format = format;
    entry.Compression = compression;
    entry.IsFromDisk = false;
    entry.MemoryData.assign(data.begin(), data.end());
    entry.DataSize = entry.MemoryData.size();

    m_lowercaseUris.insert(std::move(lower));
    m_entries.push_back(std::move(entry));
    return EResult::Success;
}

EResult PakWriter::AddFile(
    std::string_view uri,
    const std::filesystem::path& physicalPath,
    EPakPayloadFormat format,
    EPakCompression compression)
{
    std::string normalized;
    if (!NormalizePakUri(uri, normalized))
    {
        return EResult::InvalidArgument;
    }

    std::string lower = ToLowerPakUri(normalized);
    if (m_lowercaseUris.contains(lower))
    {
        return EResult::InvalidArgument;
    }

    std::error_code ec;
    if (!std::filesystem::exists(physicalPath, ec) || !std::filesystem::is_regular_file(physicalPath, ec))
    {
        return EResult::FileNotFound;
    }

    uint64 fileSize = std::filesystem::file_size(physicalPath, ec);
    if (ec || fileSize > PAK_MAX_ENTRY_BYTES)
    {
        return EResult::InvalidArgument;
    }

    if (m_entries.size() >= PAK_MAX_ENTRIES)
    {
        return EResult::OutOfMemory;
    }

    PendingEntry entry;
    entry.NormalizedUri = std::move(normalized);
    entry.LowercaseUri = lower;
    entry.Format = format;
    entry.Compression = compression;
    entry.IsFromDisk = true;
    entry.SourceFilePath = physicalPath;
    entry.DataSize = fileSize;

    m_lowercaseUris.insert(std::move(lower));
    m_entries.push_back(std::move(entry));
    return EResult::Success;
}

size_t PakWriter::GetEntryCount() const noexcept
{
    return m_entries.size();
}

void PakWriter::Clear() noexcept
{
    m_entries.clear();
    m_lowercaseUris.clear();
}

EResult PakWriter::WriteToFile(const std::filesystem::path& outputPath)
{
    // 1. 엔트리 사전순 정렬을 통한 결정적(deterministic) 출력 보장
    std::sort(m_entries.begin(), m_entries.end(), [](const PendingEntry& lhs, const PendingEntry& rhs) {
        return lhs.NormalizedUri < rhs.NormalizedUri;
    });

    std::error_code ec;
    if (outputPath.has_parent_path())
    {
        std::filesystem::create_directories(outputPath.parent_path(), ec);
    }

    std::ofstream out(outputPath, std::ios::binary | std::ios::trunc);
    if (!out.is_open())
    {
        return EResult::AccessDenied;
    }

    // 2. 고정 헤더 자리 예약 (48바이트 제로 기록)
    uint8 zeroHeader[PAK_HEADER_SIZE] = {};
    out.write(reinterpret_cast<const char*>(zeroHeader), PAK_HEADER_SIZE);
    if (!out.good())
    {
        return EResult::Fail;
    }

    // 3. 페이로드 순차 스트리밍 기록 및 오프셋/크기/해시 산출
    struct WrittenRecord
    {
        uint64 Offset = 0;
        uint64 Size = 0;
        uint64 Hash64 = 0;
    };

    std::vector<WrittenRecord> writtenRecords;
    writtenRecords.reserve(m_entries.size());

    uint64 currentOffset = PAK_HEADER_SIZE;

    // 스트리밍용 청크 버퍼 (64 KB)
    constexpr size_t CHUNK_SIZE = 64 * 1024;
    std::vector<uint8_t> chunkBuffer(CHUNK_SIZE);

    for (const auto& entry : m_entries)
    {
        WrittenRecord record;
        record.Offset = currentOffset;

        if (entry.IsFromDisk)
        {
            std::ifstream fileIn(entry.SourceFilePath, std::ios::binary);
            if (!fileIn.is_open())
            {
                return EResult::FileNotFound;
            }

            constexpr uint64 FNV_OFFSET_BASIS = 14695981039346656037ULL;
            constexpr uint64 FNV_PRIME = 1099511628211ULL;
            uint64 runningHash = FNV_OFFSET_BASIS;
            uint64 totalBytesRead = 0;

            while (fileIn.good())
            {
                fileIn.read(reinterpret_cast<char*>(chunkBuffer.data()), CHUNK_SIZE);
                std::streamsize bytesRead = fileIn.gcount();
                if (bytesRead <= 0)
                {
                    break;
                }

                out.write(reinterpret_cast<const char*>(chunkBuffer.data()), bytesRead);
                if (!out.good())
                {
                    return EResult::Fail;
                }

                for (std::streamsize i = 0; i < bytesRead; ++i)
                {
                    runningHash ^= static_cast<uint64>(chunkBuffer[i]);
                    runningHash *= FNV_PRIME;
                }

                totalBytesRead += static_cast<uint64>(bytesRead);
            }

            record.Size = totalBytesRead;
            record.Hash64 = runningHash;
            currentOffset += totalBytesRead;
        }
        else
        {
            const size_t memSize = entry.MemoryData.size();
            if (memSize > 0)
            {
                out.write(reinterpret_cast<const char*>(entry.MemoryData.data()), memSize);
                if (!out.good())
                {
                    return EResult::Fail;
                }
            }

            record.Size = static_cast<uint64>(memSize);
            record.Hash64 = ComputePakHash64(entry.MemoryData.data(), memSize);
            currentOffset += record.Size;
        }

        writtenRecords.push_back(record);
    }

    // 4. 색인(Index) 테이블 구성 및 기록
    const uint64 indexOffset = currentOffset;
    std::vector<uint8_t> indexBuffer;

    // 예상 인덱스 버퍼 크기 사전 예약
    size_t estimatedIndexSize = 0;
    for (const auto& entry : m_entries)
    {
        estimatedIndexSize += 2 + entry.NormalizedUri.size() + 2 + 2 + 4 + 8 + 8 + 8;
    }
    indexBuffer.reserve(estimatedIndexSize);

    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        const auto& entry = m_entries[i];
        const auto& record = writtenRecords[i];

        const uint16 uriLen = static_cast<uint16>(entry.NormalizedUri.size());

        uint8 fieldBuf[34];
        WriteUint16LE(fieldBuf, uriLen);
        indexBuffer.insert(indexBuffer.end(), fieldBuf, fieldBuf + 2);

        indexBuffer.insert(indexBuffer.end(), entry.NormalizedUri.begin(), entry.NormalizedUri.end());

        WriteUint16LE(fieldBuf + 0, static_cast<uint16>(entry.Format));
        WriteUint16LE(fieldBuf + 2, static_cast<uint16>(entry.Compression));
        WriteUint32LE(fieldBuf + 4, 0); // Reserved
        WriteUint64LE(fieldBuf + 8, record.Offset);
        WriteUint64LE(fieldBuf + 16, record.Size);
        WriteUint64LE(fieldBuf + 24, record.Hash64);
        indexBuffer.insert(indexBuffer.end(), fieldBuf, fieldBuf + 32);
    }

    const uint64 indexSize = static_cast<uint64>(indexBuffer.size());
    const uint64 indexHash = ComputePakHash64(indexBuffer.data(), indexBuffer.size());

    if (indexSize > 0)
    {
        out.write(reinterpret_cast<const char*>(indexBuffer.data()), indexBuffer.size());
        if (!out.good())
        {
            return EResult::Fail;
        }
    }

    // 5. 파일 시작 위치로 이동하여 최종 48바이트 헤더 덮어쓰기
    uint8 headerBuf[PAK_HEADER_SIZE] = {};
    WriteUint32LE(headerBuf + 0, PAK_MAGIC);
    WriteUint32LE(headerBuf + 4, PAK_FORMAT_VERSION);
    WriteUint32LE(headerBuf + 8, static_cast<uint32>(m_entries.size()));
    WriteUint32LE(headerBuf + 12, 0); // Reserved0
    WriteUint64LE(headerBuf + 16, indexOffset);
    WriteUint64LE(headerBuf + 24, indexSize);
    WriteUint64LE(headerBuf + 32, indexHash);
    WriteUint64LE(headerBuf + 40, 0); // Reserved1

    out.seekp(0, std::ios::beg);
    out.write(reinterpret_cast<const char*>(headerBuf), PAK_HEADER_SIZE);
    out.flush();

    if (!out.good())
    {
        return EResult::Fail;
    }

    out.close();
    return EResult::Success;
}

// =============================================================================
// PakReader 구현
// =============================================================================
PakReader::PakReader() = default;

PakReader::~PakReader()
{
    Unmount();
}

PakReader::PakReader(PakReader&&) noexcept = default;
PakReader& PakReader::operator=(PakReader&&) noexcept = default;

EResult PakReader::Mount(const std::filesystem::path& pakPath)
{
    Unmount();

    std::error_code ec;
    if (!std::filesystem::exists(pakPath, ec) || !std::filesystem::is_regular_file(pakPath, ec))
    {
        return EResult::FileNotFound;
    }

    const uint64 fileSize = std::filesystem::file_size(pakPath, ec);
    if (ec || fileSize < PAK_HEADER_SIZE || fileSize > PAK_MAX_FILE_SIZE)
    {
        return EResult::Fail;
    }

    auto handle = std::make_unique<FileHandleImpl>();
    handle->Stream.open(pakPath, std::ios::binary);
    if (!handle->Stream.is_open())
    {
        return EResult::AccessDenied;
    }

    // 1. 헤더 48바이트 읽기 및 검증
    uint8 headerBuf[PAK_HEADER_SIZE];
    handle->Stream.read(reinterpret_cast<char*>(headerBuf), PAK_HEADER_SIZE);
    if (handle->Stream.gcount() != PAK_HEADER_SIZE)
    {
        return EResult::Fail;
    }

    const uint32 magic = ReadUint32LE(headerBuf + 0);
    if (magic != PAK_MAGIC)
    {
        return EResult::Fail;
    }

    const uint32 version = ReadUint32LE(headerBuf + 4);
    if (version != PAK_FORMAT_VERSION)
    {
        return EResult::Fail;
    }

    const uint32 entryCount = ReadUint32LE(headerBuf + 8);
    if (entryCount > PAK_MAX_ENTRIES)
    {
        return EResult::Fail;
    }

    const uint64 indexOffset = ReadUint64LE(headerBuf + 16);
    const uint64 indexSize = ReadUint64LE(headerBuf + 24);
    const uint64 expectedIndexHash = ReadUint64LE(headerBuf + 32);

    // 인덱스 경계 검증
    if (indexOffset < PAK_HEADER_SIZE || indexOffset > fileSize)
    {
        return EResult::Fail;
    }
    if (indexSize > fileSize || indexOffset + indexSize > fileSize)
    {
        return EResult::Fail;
    }

    // 2. 색인 테이블 바이트 읽기 및 무결성 해시 검증
    std::vector<uint8_t> indexBuffer(static_cast<size_t>(indexSize));
    if (indexSize > 0)
    {
        handle->Stream.seekg(static_cast<std::streamoff>(indexOffset), std::ios::beg);
        handle->Stream.read(reinterpret_cast<char*>(indexBuffer.data()), static_cast<std::streamsize>(indexSize));
        if (static_cast<uint64>(handle->Stream.gcount()) != indexSize)
        {
            return EResult::Fail;
        }

        const uint64 actualIndexHash = ComputePakHash64(indexBuffer.data(), indexBuffer.size());
        if (actualIndexHash != expectedIndexHash)
        {
            return EResult::Fail;
        }
    }
    else if (entryCount > 0)
    {
        // 엔트리가 있는데 인덱스 크기가 0인 비정상 케이스 방어
        return EResult::Fail;
    }

    // 3. 색인 테이블 파싱 및 개별 엔트리 검증
    size_t readPos = 0;
    std::vector<PakEntryInfo> parsedEntries;
    parsedEntries.reserve(entryCount);

    std::unordered_map<std::string, size_t> parsedMap;

    for (uint32 i = 0; i < entryCount; ++i)
    {
        if (readPos + 2 > indexSize)
        {
            return EResult::Fail;
        }

        const uint16 uriLen = ReadUint16LE(indexBuffer.data() + readPos);
        readPos += 2;

        if (uriLen == 0 || uriLen > PAK_MAX_URI_LENGTH || readPos + uriLen + 32 > indexSize)
        {
            return EResult::Fail;
        }

        std::string rawUri(reinterpret_cast<const char*>(indexBuffer.data() + readPos), uriLen);
        readPos += uriLen;

        std::string normalized;
        if (!NormalizePakUri(rawUri, normalized))
        {
            return EResult::Fail;
        }

        const uint16 formatVal = ReadUint16LE(indexBuffer.data() + readPos);
        readPos += 2;
        const uint16 compVal = ReadUint16LE(indexBuffer.data() + readPos);
        readPos += 2;
        // Reserved 4 bytes
        readPos += 4;

        const uint64 dataOffset = ReadUint64LE(indexBuffer.data() + readPos);
        readPos += 8;
        const uint64 dataSize = ReadUint64LE(indexBuffer.data() + readPos);
        readPos += 8;
        const uint64 dataHash = ReadUint64LE(indexBuffer.data() + readPos);
        readPos += 8;

        // 페이로드 영역 범위 검증 (헤더와 인덱스 테이블 사이 구간에 위치해야 함)
        if (dataOffset < PAK_HEADER_SIZE || dataOffset > indexOffset)
        {
            return EResult::Fail;
        }
        if (dataSize > PAK_MAX_ENTRY_BYTES || dataOffset + dataSize > indexOffset)
        {
            return EResult::Fail;
        }

        // 중복 URI 검증 (Windows 대소문자 무시 기준)
        std::string lower = ToLowerPakUri(normalized);
        if (parsedMap.contains(lower))
        {
            return EResult::Fail;
        }

        PakEntryInfo entry;
        entry.Uri = std::move(normalized);
        entry.PayloadFormat = static_cast<EPakPayloadFormat>(formatVal);
        entry.Compression = static_cast<EPakCompression>(compVal);
        entry.Offset = dataOffset;
        entry.Size = dataSize;
        entry.Hash64 = dataHash;

        const size_t newIndex = parsedEntries.size();
        parsedEntries.push_back(std::move(entry));
        parsedMap.emplace(std::move(lower), newIndex);
    }

    if (readPos != indexSize)
    {
        // 인덱스 테이블 끝에 알 수 없는 잉여 데이터가 남아있는 비정상 케이스 방어
        return EResult::Fail;
    }

    // 4. 페이로드 구간 간 중첩(Overlap) 방어 검증
    struct RangeCheck
    {
        uint64 Begin;
        uint64 End;
    };
    std::vector<RangeCheck> ranges;
    ranges.reserve(parsedEntries.size());

    for (const auto& entry : parsedEntries)
    {
        if (entry.Size > 0)
        {
            ranges.push_back({ entry.Offset, entry.Offset + entry.Size });
        }
    }

    std::sort(ranges.begin(), ranges.end(), [](const RangeCheck& a, const RangeCheck& b) {
        return a.Begin < b.Begin;
    });

    for (size_t i = 1; i < ranges.size(); ++i)
    {
        if (ranges[i].Begin < ranges[i - 1].End)
        {
            // 중첩된 엔트리 구간 검출 -> 거부
            return EResult::Fail;
        }
    }

    m_entries = std::move(parsedEntries);
    m_uriToIndex = std::move(parsedMap);
    m_pakPath = pakPath;
    m_fileSize = fileSize;
    m_fileHandle = std::move(handle);
    m_isMounted = true;
    return EResult::Success;
}

void PakReader::Unmount()
{
    if (m_fileHandle)
    {
        std::lock_guard lock(m_fileHandle->Mutex);
        if (m_fileHandle->Stream.is_open())
        {
            m_fileHandle->Stream.close();
        }
    }
    m_fileHandle.reset();
    m_entries.clear();
    m_uriToIndex.clear();
    m_pakPath.clear();
    m_fileSize = 0;
    m_isMounted = false;
}

bool PakReader::IsMounted() const noexcept
{
    return m_isMounted;
}

bool PakReader::Contains(std::string_view uri) const
{
    return FindEntry(uri) != nullptr;
}

const PakEntryInfo* PakReader::FindEntry(std::string_view uri) const
{
    if (!m_isMounted)
    {
        return nullptr;
    }

    std::string normalized;
    if (!NormalizePakUri(uri, normalized))
    {
        return nullptr;
    }

    std::string lower = ToLowerPakUri(normalized);
    auto it = m_uriToIndex.find(lower);
    if (it == m_uriToIndex.end())
    {
        return nullptr;
    }

    return &m_entries[it->second];
}

EResult PakReader::ReadEntry(std::string_view uri, std::vector<uint8_t>& outBytes) const
{
    outBytes.clear();

    if (!m_isMounted || !m_fileHandle)
    {
        return EResult::Fail;
    }

    const PakEntryInfo* entry = FindEntry(uri);
    if (!entry)
    {
        return EResult::FileNotFound;
    }

    if (entry->Size > PAK_MAX_ENTRY_BYTES)
    {
        return EResult::OutOfMemory;
    }

    if (entry->Size == 0)
    {
        return EResult::Success;
    }

    outBytes.resize(static_cast<size_t>(entry->Size));

    {
        std::lock_guard lock(m_fileHandle->Mutex);
        m_fileHandle->Stream.clear();
        m_fileHandle->Stream.seekg(static_cast<std::streamoff>(entry->Offset), std::ios::beg);
        m_fileHandle->Stream.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(entry->Size));

        if (static_cast<uint64>(m_fileHandle->Stream.gcount()) != entry->Size)
        {
            outBytes.clear();
            return EResult::Fail;
        }
    }

    // FNV-1a 64비트 무결성 해시 검증
    const uint64 actualHash = ComputePakHash64(outBytes.data(), outBytes.size());
    if (actualHash != entry->Hash64)
    {
        outBytes.clear();
        return EResult::Fail;
    }

    return EResult::Success;
}

EResult PakReader::ReadRange(
    std::string_view uri,
    uint64 offset,
    uint64 size,
    std::vector<uint8_t>& outBytes) const
{
    outBytes.clear();

    if (!m_isMounted || !m_fileHandle)
    {
        return EResult::Fail;
    }

    const PakEntryInfo* entry = FindEntry(uri);
    if (!entry)
    {
        return EResult::FileNotFound;
    }

    if (offset > entry->Size || size > entry->Size - offset)
    {
        return EResult::InvalidArgument;
    }

    if (size == 0)
    {
        return EResult::Success;
    }

    outBytes.resize(static_cast<size_t>(size));

    {
        std::lock_guard lock(m_fileHandle->Mutex);
        m_fileHandle->Stream.clear();
        m_fileHandle->Stream.seekg(static_cast<std::streamoff>(entry->Offset + offset), std::ios::beg);
        m_fileHandle->Stream.read(reinterpret_cast<char*>(outBytes.data()), static_cast<std::streamsize>(size));

        if (static_cast<uint64>(m_fileHandle->Stream.gcount()) != size)
        {
            outBytes.clear();
            return EResult::Fail;
        }
    }

    return EResult::Success;
}

const std::vector<PakEntryInfo>& PakReader::GetEntries() const noexcept
{
    return m_entries;
}

const std::filesystem::path& PakReader::GetPakPath() const noexcept
{
    return m_pakPath;
}

// =============================================================================
// PakMountManager 구현
// =============================================================================
PakMountManager::PakMountManager() = default;
PakMountManager::~PakMountManager() = default;

PakMountManager::PakMountManager(PakMountManager&&) noexcept = default;
PakMountManager& PakMountManager::operator=(PakMountManager&&) noexcept = default;

EResult PakMountManager::MountPak(const std::filesystem::path& pakPath)
{
    auto reader = std::make_unique<PakReader>();
    EResult res = reader->Mount(pakPath);
    if (res != EResult::Success)
    {
        return res;
    }

    m_readers.push_back(std::move(reader));
    return EResult::Success;
}

void PakMountManager::UnmountAll()
{
    m_readers.clear();
}

bool PakMountManager::HasEntry(std::string_view uri) const
{
    return FindEntry(uri) != nullptr;
}

const PakEntryInfo* PakMountManager::FindEntry(std::string_view uri) const
{
    // 나중에 마운트된 팩이 먼저 오버라이드할 수 있도록 역순 순회
    for (auto it = m_readers.rbegin(); it != m_readers.rend(); ++it)
    {
        if (const auto* entry = (*it)->FindEntry(uri))
        {
            return entry;
        }
    }
    return nullptr;
}

EResult PakMountManager::TryReadBytes(std::string_view uri, std::vector<uint8_t>& outBytes) const
{
    for (auto it = m_readers.rbegin(); it != m_readers.rend(); ++it)
    {
        if ((*it)->Contains(uri))
        {
            return (*it)->ReadEntry(uri, outBytes);
        }
    }
    return EResult::FileNotFound;
}

EResult PakMountManager::TryReadRange(
    std::string_view uri,
    uint64 offset,
    uint64 size,
    std::vector<uint8_t>& outBytes) const
{
    for (auto it = m_readers.rbegin(); it != m_readers.rend(); ++it)
    {
        if ((*it)->Contains(uri))
        {
            return (*it)->ReadRange(uri, offset, size, outBytes);
        }
    }
    return EResult::FileNotFound;
}

size_t PakMountManager::GetMountedPakCount() const noexcept
{
    return m_readers.size();
}

const std::vector<std::unique_ptr<PakReader>>& PakMountManager::GetReaders() const noexcept
{
    return m_readers;
}

PakMountManager& PakMountManager::Get() noexcept
{
    static PakMountManager s_instance;
    return s_instance;
}

EResult PakMountManager::MountFromPackageManifest(
    const std::filesystem::path& manifestPath,
    std::string* outStartupSceneUri)
{
    std::error_code ec;
    if (!std::filesystem::exists(manifestPath, ec) || !std::filesystem::is_regular_file(manifestPath, ec))
    {
        return EResult::FileNotFound;
    }

    std::ifstream file(manifestPath, std::ios::binary);
    if (!file.is_open())
    {
        return EResult::AccessDenied;
    }

    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (text.starts_with("\xef\xbb\xbf"))
    {
        text.erase(0, 3);
    }

    PakDetail::ManifestConfig manifest;
    if (glz::read_json(manifest, text))
    {
        return EResult::Fail;
    }

    if (manifest.SchemaVersion == 0)
    {
        return EResult::Fail;
    }

    if (outStartupSceneUri)
    {
        *outStartupSceneUri = manifest.StartupSceneUri;
    }

    std::vector<PakDetail::ManifestPakEntry> allEntries;
    allEntries.reserve(manifest.Paks.size() + manifest.PakArchives.size());
    for (auto& entry : manifest.Paks)
    {
        allEntries.push_back(std::move(entry));
    }
    for (auto& entry : manifest.PakArchives)
    {
        allEntries.push_back(std::move(entry));
    }

    if (allEntries.empty())
    {
        return EResult::Fail;
    }

    const std::filesystem::path manifestDir = manifestPath.parent_path();
    const std::filesystem::path packageRoot = (manifestDir.filename() == "Config")
        ? manifestDir.parent_path()
        : manifestDir;

    const size_t initialReaderCount = m_readers.size();

    for (const auto& pakEntry : allEntries)
    {
        const std::string& relPathStr = !pakEntry.RelativePath.empty() ? pakEntry.RelativePath : pakEntry.Path;
        if (relPathStr.empty())
        {
            while (m_readers.size() > initialReaderCount)
            {
                m_readers.pop_back();
            }
            return EResult::InvalidArgument;
        }

        std::filesystem::path pakFilePath = packageRoot / std::filesystem::u8path(relPathStr);
        if (!std::filesystem::exists(pakFilePath, ec))
        {
            if (std::filesystem::exists(manifestDir / std::filesystem::u8path(relPathStr), ec))
            {
                pakFilePath = manifestDir / std::filesystem::u8path(relPathStr);
            }
            else
            {
                while (m_readers.size() > initialReaderCount)
                {
                    m_readers.pop_back();
                }
                return EResult::FileNotFound;
            }
        }

        const EResult mountRes = MountPak(pakFilePath);
        if (mountRes != EResult::Success)
        {
            while (m_readers.size() > initialReaderCount)
            {
                m_readers.pop_back();
            }
            return mountRes;
        }

        if (pakEntry.EntryCount > 0)
        {
            if (m_readers.empty() || m_readers.back()->GetEntries().size() != pakEntry.EntryCount)
            {
                while (m_readers.size() > initialReaderCount)
                {
                    m_readers.pop_back();
                }
                return EResult::Fail;
            }
        }

        const uint64 expectedBytes = pakEntry.TotalBytes != 0 ? pakEntry.TotalBytes : pakEntry.ByteSize;
        if (expectedBytes > 0)
        {
            const uint64 actualBytes = std::filesystem::file_size(pakFilePath, ec);
            if (ec || actualBytes != expectedBytes)
            {
                while (m_readers.size() > initialReaderCount)
                {
                    m_readers.pop_back();
                }
                return EResult::Fail;
            }
        }
    }

    return EResult::Success;
}

std::string PakMountManager::ToPakUri(const std::filesystem::path& pathOrUri)
{
    if (pathOrUri.empty())
        return {};

    const std::wstring virtualUri = PlatformPath::ToVirtualUri(pathOrUri);
    if (!virtualUri.empty() && PlatformPath::IsVirtualUri(virtualUri))
    {
        std::string normalized;
        if (NormalizePakUri(WStrToStr(virtualUri), normalized))
            return normalized;
    }

    // When loose files are pruned (.bampak-only packaged Client), PlatformPath::ResolveUri falls back to
    // <ProjectRoot>/<relative> for scheme-less paths like "Config/Startup/01_Textures.json" or "Resources/...".
    // Probe mounted paks directly against Project:// and Engine:// (and stripped Resources/ prefix).
    const auto& mountManager = PakMountManager::Get();
    if (mountManager.GetMountedPakCount() == 0)
        return {};

    std::filesystem::path rel = pathOrUri.is_absolute() ? PlatformPath::ToProjectRelative(pathOrUri) : PlatformPath::Normalize(pathOrUri);
    if (rel.empty() || rel.is_absolute())
        return {};

    std::string relStr = WStrToStr(rel.generic_wstring());
    while (relStr.starts_with("./"))
        relStr.erase(0, 2);
    while (!relStr.empty() && relStr.front() == '/')
        relStr.erase(0, 1);
    if (relStr.empty())
        return {};

    const auto tryCandidate = [&mountManager](std::string_view candidate) -> std::string
    {
        std::string normalized;
        if (NormalizePakUri(candidate, normalized) && mountManager.HasEntry(normalized))
            return normalized;
        return {};
    };

    if (auto found = tryCandidate("Project://" + relStr); !found.empty())
        return found;
    if (auto found = tryCandidate("Engine://" + relStr); !found.empty())
        return found;

    if (relStr.size() > 10 && ToLowerStr(relStr.substr(0, 10)) == "resources/")
    {
        const std::string stripped = relStr.substr(10);
        if (auto found = tryCandidate("Project://" + stripped); !found.empty())
            return found;
        if (auto found = tryCandidate("Engine://" + stripped); !found.empty())
            return found;
    }

    return {};
}

bool PakMountManager::ExistsOrOnDisk(const std::filesystem::path& pathOrUri) const
{
    if (pathOrUri.empty())
        return false;

    if (!m_readers.empty())
    {
        const std::string uri = ToPakUri(pathOrUri);
        if (!uri.empty() && HasEntry(uri))
            return true;
    }

    std::error_code ec;
    return std::filesystem::is_regular_file(PlatformPath::ResolveUri(pathOrUri), ec);
}

EResult PakMountManager::ReadBytesWithFallback(const std::filesystem::path& pathOrUri, std::vector<uint8_t>& outBytes) const
{
    outBytes.clear();
    if (pathOrUri.empty())
        return EResult::InvalidArgument;

    if (!m_readers.empty())
    {
        const std::string uri = ToPakUri(pathOrUri);
        if (!uri.empty() && HasEntry(uri))
            return TryReadBytes(uri, outBytes); // 팩 엔트리 무결성 실패 시 디스크로 폴백하지 않음
    }

    const std::filesystem::path physical = PlatformPath::ResolveUri(pathOrUri);
    std::ifstream file(physical, std::ios::binary | std::ios::ate);
    if (!file.is_open())
        return EResult::FileNotFound;

    const std::streamsize size = file.tellg();
    if (size < 0)
        return EResult::Fail;
    if (size == 0)
        return EResult::Success;

    outBytes.resize(static_cast<size_t>(size));
    file.seekg(0, std::ios::beg);
    if (!file.read(reinterpret_cast<char*>(outBytes.data()), size))
    {
        outBytes.clear();
        return EResult::Fail;
    }
    return EResult::Success;
}

archive::ArchiveResult PakMountManager::LoadArchiveWithFallback(archive::IArchive& ar, const std::filesystem::path& pathOrUri) const
{
    if (m_readers.empty())
        return ar.LoadFromFile(PlatformPath::ResolveUri(pathOrUri));

    std::vector<uint8_t> bytes;
    const EResult res = ReadBytesWithFallback(pathOrUri, bytes);
    if (res == EResult::FileNotFound)
        return { archive::ArchiveErrorCode::FileOpenFailed, {} };
    if (res != EResult::Success)
        return { archive::ArchiveErrorCode::FileReadFailed, {} };

    return ar.Decode(std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()));
}

END
