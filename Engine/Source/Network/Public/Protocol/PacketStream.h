#pragma once

#include "Engine_API.h"
#include "PacketHeader.h"
#include <vector>
#include <string>
#include <string_view>
#include <cstdint>
#include <cstring>
#include <concepts>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine::Network
{
    // 패킷 송신용 바이너리 작성기
    class ENGINE_API PacketWriter
    {
    public:
        PacketWriter();
        explicit PacketWriter(EPacketId packetId, std::uint16_t sequence = 0);

        void Clear();
        void Reserve(std::size_t capacity);

        // 원시 바이트 쓰기
        void WriteBytes(const void* data, std::size_t size);

        // C++ 기본 수치형 및 구조체 템플릿 직렬화
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        void Write(const T& value)
        {
            WriteBytes(&value, sizeof(T));
        }

        // 특수 타입 직렬화
        void WriteString(std::string_view str);
        void WriteVec3(const glm::vec3& v);
        void WriteQuat(const glm::quat& q);

        // 패킷 조립 마무리: 버퍼 선두 헤더의 PayloadSize를 확정 갱신하고 전체 데이터 반환
        const std::uint8_t* FinalizePacket(std::size_t& outTotalSize);

        const std::uint8_t* GetData() const { return m_Buffer.data(); }
        std::size_t GetSize() const { return m_Buffer.size(); }
        const std::vector<std::uint8_t>& GetBuffer() const { return m_Buffer; }

    private:
        std::vector<std::uint8_t> m_Buffer;
        bool m_HasHeader = false;
    };

    // 패킷 수신용 바이너리 판독기 (경계 검사 및 안전한 역직렬화)
    class ENGINE_API PacketReader
    {
    public:
        PacketReader(const std::uint8_t* data, std::size_t size);

        bool HasError() const { return m_HasError; }
        std::size_t GetRemainingBytes() const { return (m_ReadOffset <= m_TotalSize) ? (m_TotalSize - m_ReadOffset) : 0; }
        std::size_t GetReadOffset() const { return m_ReadOffset; }

        // 원시 바이트 읽기
        bool ReadBytes(void* outData, std::size_t size);

        // 템플릿 역직렬화
        template <typename T>
            requires std::is_trivially_copyable_v<T>
        bool Read(T& outValue)
        {
            return ReadBytes(&outValue, sizeof(T));
        }

        template <typename T>
            requires std::is_trivially_copyable_v<T>
        T Read()
        {
            T val{};
            Read(val);
            return val;
        }

        // 특수 타입 역직렬화
        bool ReadString(std::string& outStr);
        bool ReadVec3(glm::vec3& outVec);
        bool ReadQuat(glm::quat& outQuat);

    private:
        const std::uint8_t* m_Data = nullptr;
        std::size_t m_TotalSize = 0;
        std::size_t m_ReadOffset = 0;
        bool m_HasError = false;
    };
}