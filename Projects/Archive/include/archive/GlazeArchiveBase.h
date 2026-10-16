#pragma once

#include <archive/ArchiveInterface.h>

#include <optional>
#include <functional>

#include <glaze/json/generic.hpp>

namespace archive
{
	class GlazeArchiveBase : public IArchive
    {
    public:
        virtual ~GlazeArchiveBase() = default;

        [[nodiscard]] std::size_t GetObjectDepth() const noexcept override;
        [[nodiscard]] ArchiveResult GetMapSize(std::string_view key, std::size_t& size) const override;
        [[nodiscard]] ArchiveResult HasField(std::string_view key, bool& exists) const override;
        [[nodiscard]] ArchiveResult GetFieldNames(std::vector<std::string>& names) const override;
        [[nodiscard]] ArchiveResult BeginWriteObject(std::string_view key) override;
        [[nodiscard]] ArchiveResult BeginReadObject(std::string_view key) override;
        [[nodiscard]] ArchiveResult EndObject() override;
        [[nodiscard]] ArchiveResult GetArraySize(std::string_view key, std::size_t& size) const override;
        [[nodiscard]] ArchiveResult BeginWriteObjectElement(std::string_view key) override;
        [[nodiscard]] ArchiveResult BeginReadObjectElement(std::string_view key, std::size_t index) override;

    protected:
        using Value = glz::generic_u64;

        ArchiveResult WriteArrayElementValues(std::string_view key, std::size_t size,
            const ArrayElementCallback& writeElement) override;
        ArchiveResult ReadArrayElementValues(std::string_view key,
            const ArrayElementCallback& readElement) const override;

        virtual ArchiveResult EncodeDocument(std::string& document) const = 0;
        virtual ArchiveResult DecodeDocument(std::string_view document) = 0;
        ArchiveResult WriteValue(std::string_view key, const ArchiveScalarValue& value) override;
        ArchiveResult WriteArrayValues(std::string_view key, ArchiveScalarType elementType, const std::vector<ArchiveScalarValue>& values) override;
        ArchiveResult WriteMapValues(std::string_view key, ArchiveScalarType keyType, ArchiveScalarType valueType, const std::vector<ArchiveScalarMapEntry>& entries) override;
        ArchiveResult WriteObjectValue(std::string_view key, const ObjectWriter& writeObject) override;
        ArchiveResult WriteObjectArrayValues(std::string_view key, std::size_t size, const ObjectArrayWriter& writeObject) override;
        ArchiveResult WriteObjectMapValues(std::string_view key, std::size_t size, ArchiveScalarType keyType, const ObjectMapKeyWriter& writeKey, const ObjectArrayWriter& writeValue) override;
        ArchiveResult ReadValue(std::string_view key, ArchiveScalarType requestedType, ArchiveScalarValue& value) const override;
        ArchiveResult ReadArrayValues(std::string_view key, ArchiveScalarType requestedType, std::vector<ArchiveScalarValue>& values) const override;
        ArchiveResult ReadMapKey(std::string_view key, std::size_t index, ArchiveScalarType requestedType, ArchiveScalarValue& value) const override;
        ArchiveResult ReadMapValue(std::string_view key, std::size_t index, ArchiveScalarType requestedType, ArchiveScalarValue& value) const override;
        ArchiveResult ReadObjectValue(std::string_view key, const ObjectReader& readObject) const override;
        ArchiveResult ReadObjectArrayValues(std::string_view key, const ObjectArrayReader& readObject) const override;
        ArchiveResult ReadObjectMapValues(std::string_view key, ArchiveScalarType keyType, const ObjectMapKeyReader& readKey, const ObjectArrayReader& readValue) const override;

        GlazeArchiveBase();

        Value m_Root;

    private:
        class ObjectArchive;
        struct ObjectPathEntry
        {
            std::string Key;
            std::optional<std::size_t> ElementIndex;
        };

        static ArchiveResult InvalidValue(std::string_view key);
        template<ArchiveScalar T>
        static ArchiveResult EncodeScalar(std::string_view key, const T& value, Value& destination);
        template<typename Node>
        static auto FindObject(Node& root, const std::vector<ObjectPathEntry>& path);
        static ArchiveResult ExtractScalarValue(std::string_view key, const Value& source, ArchiveScalarValue& value);
        ArchiveResult FindMapEntry(std::string_view key, std::size_t index, const Value::object_t*& entry) const;

        std::vector<ObjectPathEntry> m_ObjectPath;
    };
}
