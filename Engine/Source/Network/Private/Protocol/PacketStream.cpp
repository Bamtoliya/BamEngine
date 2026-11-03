#include "PacketStream.h"

namespace Engine::Network
{
    // =========================================================================
    // PacketWriter
    // =========================================================================
    PacketWriter::PacketWriter()
    {
        m_Buffer.reserve(256);
    }

    PacketWriter::PacketWriter(EPacketId packetId, std::uint16_t sequence)
    {
        m_Buffer.reserve(256);
        m_HasHeader = true;

        PacketHeader header;
        header.PacketId = static_cast<std::uint16_t>(packetId);
        header.Sequence = sequence;
        header.PayloadSize = 0; // FinalizePacket에서 자동 계산

        WriteBytes(&header, sizeof(PacketHeader));
    }

    void PacketWriter::Clear()
    {
        m_Buffer.clear();
        m_HasHeader = false;
    }

    void PacketWriter::Reserve(std::size_t capacity)
    {
        m_Buffer.reserve(capacity);
    }

    void PacketWriter::WriteBytes(const void* data, std::size_t size)
    {
        if (!data || size == 0) return;
        const std::uint8_t* bytePtr = static_cast<const std::uint8_t*>(data);
        m_Buffer.insert(m_Buffer.end(), bytePtr, bytePtr + size);
    }

    void PacketWriter::WriteString(std::string_view str)
    {
        std::uint16_t length = static_cast<std::uint16_t>(str.length());
        Write<std::uint16_t>(length);
        if (length > 0)
        {
            WriteBytes(str.data(), length);
        }
    }

    void PacketWriter::WriteVec3(const glm::vec3& v)
    {
        Write<float>(v.x);
        Write<float>(v.y);
        Write<float>(v.z);
    }

    void PacketWriter::WriteQuat(const glm::quat& q)
    {
        Write<float>(q.x);
        Write<float>(q.y);
        Write<float>(q.z);
        Write<float>(q.w);
    }

    const std::uint8_t* PacketWriter::FinalizePacket(std::size_t& outTotalSize)
    {
        outTotalSize = m_Buffer.size();
        if (m_HasHeader && m_Buffer.size() >= sizeof(PacketHeader))
        {
            PacketHeader* header = reinterpret_cast<PacketHeader*>(m_Buffer.data());
            header->PayloadSize = static_cast<std::uint16_t>(m_Buffer.size() - sizeof(PacketHeader));
        }
        return m_Buffer.data();
    }

    // =========================================================================
    // PacketReader
    // =========================================================================
    PacketReader::PacketReader(const std::uint8_t* data, std::size_t size)
        : m_Data(data), m_TotalSize(size), m_ReadOffset(0), m_HasError(data == nullptr && size > 0)
    {
    }

    bool PacketReader::ReadBytes(void* outData, std::size_t size)
    {
        if (m_HasError || size == 0) return false;
        if (m_ReadOffset + size > m_TotalSize)
        {
            m_HasError = true; // 버퍼 언더런 (요청 크기 초과)
            return false;
        }

        std::memcpy(outData, m_Data + m_ReadOffset, size);
        m_ReadOffset += size;
        return true;
    }

    bool PacketReader::ReadString(std::string& outStr)
    {
        std::uint16_t length = 0;
        if (!Read<std::uint16_t>(length)) return false;

        if (length == 0)
        {
            outStr.clear();
            return true;
        }

        if (m_ReadOffset + length > m_TotalSize)
        {
            m_HasError = true;
            return false;
        }

        outStr.assign(reinterpret_cast<const char*>(m_Data + m_ReadOffset), length);
        m_ReadOffset += length;
        return true;
    }

    bool PacketReader::ReadVec3(glm::vec3& outVec)
    {
        return Read<float>(outVec.x) && Read<float>(outVec.y) && Read<float>(outVec.z);
    }

    bool PacketReader::ReadQuat(glm::quat& outQuat)
    {
        return Read<float>(outQuat.x) && Read<float>(outQuat.y) && Read<float>(outQuat.z) && Read<float>(outQuat.w);
    }
}