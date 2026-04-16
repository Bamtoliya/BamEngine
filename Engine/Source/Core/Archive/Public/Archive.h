#pragma once

#include "Engine_API.h"
#include "Types.h"

#include <cstddef>
#include <filesystem>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string_view>

namespace Engine
{
    enum class EArchiveMode : uint8
    {
        Read,
        Write
    };

    class ENGINE_API Archive
    {
    public:
        explicit Archive(EArchiveMode mode)
            : m_Mode(mode)
        {
        }

        virtual ~Archive() = default;

    public:
        bool IsReading() const { return m_Mode == EArchiveMode::Read; }
        bool IsWriting() const { return m_Mode == EArchiveMode::Write; }
        EArchiveMode GetMode() const { return m_Mode; }

    public:
        virtual bool PushScope(string_view key) = 0;
        virtual void PopScope() = 0;

    public:
        template<typename T>
        void Process(string_view key, T& type);

        virtual void Process(string_view key, bool& v) = 0;
        virtual void Process(string_view key, int8& v) = 0;
        virtual void Process(string_view key, int16& v) = 0;
        virtual void Process(string_view key, int32& v) = 0;
        virtual void Process(string_view key, int64& v) = 0;
        virtual void Process(string_view key, uint8& v) = 0;
        virtual void Process(string_view key, uint16& v) = 0;
        virtual void Process(string_view key, uint32& v) = 0;
        virtual void Process(string_view key, uint64& v) = 0;
        virtual void Process(string_view key, f32& v) = 0;
        virtual void Process(string_view key, f64& v) = 0;
        virtual void Process(string_view key, string& v) = 0;
        virtual void Process(string_view key, wstring& v) = 0;

    public:
        virtual void Process(string_view key, glm::vec2& v) = 0;
        virtual void Process(string_view key, glm::vec3& v) = 0;
        virtual void Process(string_view key, glm::vec4& v) = 0;
        virtual void Process(string_view key, glm::quat& v) = 0;
        virtual void Process(string_view key, glm::mat3& v) = 0;
        virtual void Process(string_view key, glm::mat4& v) = 0;

    public:
        virtual void ProcessRaw(string_view key, const void* data, size_t size) = 0;
        virtual void ProcessEnum(string_view key, void* enum_ptr, size_t size) = 0;

    public:
        virtual size_t BeginArray(string_view key) = 0;
        virtual void EndArray() = 0;
        virtual void BeginArrayElement() = 0;
        virtual void EndArrayElement() = 0;
        virtual size_t GetScopeDepth() const = 0;

    public:
        virtual size_t BeginMap(string_view key) = 0;
        virtual void EndMap() = 0;
        virtual void BeginMapElement(string& out_key) = 0;
        virtual void EndMapElement() = 0;

    public:
        virtual bool SaveToFile(string_view file_path) { return false; }
        virtual bool LoadFromFile(string_view file_path) { return false; }

        virtual bool SaveToFile(wstring_view file_path)
        {
            auto utf8_path = std::filesystem::path(file_path).u8string();
            return SaveToFile(string(utf8_path.begin(), utf8_path.end()));
        }

        virtual bool LoadFromFile(wstring_view file_path)
        {
            auto utf8_path = std::filesystem::path(file_path).u8string();
            return LoadFromFile(string(utf8_path.begin(), utf8_path.end()));
        }

    protected:
        EArchiveMode m_Mode;
    };
}