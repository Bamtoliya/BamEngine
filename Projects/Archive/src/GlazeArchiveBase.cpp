#include <archive/GlazeArchiveBase.h>

namespace archive
{
    template<ArchiveScalar T>
    ArchiveResult GlazeArchiveBase::EncodeScalar(std::string_view key, const T& value, Value& destination)
    {
        if constexpr (std::is_enum_v<T>)
        {
            using Underlying = std::underlying_type_t<T>;
            using Storage = std::conditional_t<std::is_signed_v<Underlying>, std::int64_t, std::uint64_t>;

            static_assert(sizeof(Underlying) <= sizeof(Storage), "Enum storage exceeds 64 bits.");

            destination.data = static_cast<Storage>(static_cast<Underlying>(value));
        }
        else if constexpr (std::same_as<T, std::wstring>)
        {
            std::string utf8;

            if (!detail::WideToUtf8(value, utf8))
            {
                return InvalidValue(key);
            }

            destination.data = std::move(utf8);
        }
        else if constexpr (std::same_as<T, bool> || std::same_as<T, std::string>)
        {
            destination.data = value;
        }
        else if constexpr (std::signed_integral<T>)
        {
            destination.data = static_cast<std::int64_t>(value);
        }
        else if constexpr (std::unsigned_integral<T>)
        {
            destination.data = static_cast<std::uint64_t>(value);
        }
        else
        {
            if (!std::isfinite(value))
            {
                return InvalidValue(key);
            }

            destination.data = static_cast<double>(value);
        }

        return {};
    }

    template<typename Node>
    auto GlazeArchiveBase::FindObject(Node& root, const std::vector<ObjectPathEntry>& path)
    {
        using ObjectPointer = decltype(root.template get_if<Value::object_t>());
        auto* current = &root;

        for (const auto& entry : path)
        {
            auto* object = current->template get_if<Value::object_t>();

            if (!object)
            {
                return ObjectPointer{};
            }

            const auto it = object->find(entry.Key);

            if (it == object->end())
            {
                return ObjectPointer{};
            }

            current = &it->second;

            if (entry.ElementIndex)
            {
                auto* elements = current->template get_if<Value::array_t>();

                if (!elements || *entry.ElementIndex >= elements->size())
                {
                    return ObjectPointer{};
                }

                current = &(*elements)[*entry.ElementIndex];
            }
        }

        return current->template get_if<Value::object_t>();
    }

    class GlazeArchiveBase::ObjectArchive final : public GlazeArchiveBase
    {
    public:
        ObjectArchive() = default;

    private:
        ArchiveResult EncodeDocument(std::string&) const override
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }

        ArchiveResult DecodeDocument(std::string_view) override
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }
    };

#pragma region MyRegion
    GlazeArchiveBase::GlazeArchiveBase()
    {
        m_Root.data = Value::object_t{};
    }

    ArchiveResult GlazeArchiveBase::InvalidValue(std::string_view key)
    {
        return { ArchiveErrorCode::InvalidValue, std::string{ key } };
    }

    std::size_t GlazeArchiveBase::GetObjectDepth() const noexcept
    {
        return m_ObjectPath.size();
    }

    ArchiveResult GlazeArchiveBase::HasField(std::string_view key, bool& exists) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        exists = object->find(key) != object->end();
        return {};
    }

    ArchiveResult GlazeArchiveBase::GetFieldNames(std::vector<std::string>& names) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (object == nullptr)
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }

        std::vector<std::string> candidate;
        candidate.reserve(object->size());

        for (const auto& entry : *object)
        {
            candidate.push_back(entry.first);
        }

        names = std::move(candidate);
        return {};
    }

    ArchiveResult GlazeArchiveBase::BeginWriteObject(std::string_view key)
    {
        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        auto it = object->find(key);

        if (it == object->end())
        {
            Value candidate;
            candidate.data = Value::object_t{};
            it = object->emplace(std::string{ key }, std::move(candidate)).first;
        }

        if (!it->second.is_object())
        {
            return InvalidValue(key);
        }

        m_ObjectPath.push_back({ std::string{ key }, std::nullopt });
        return {};
    }

    ArchiveResult GlazeArchiveBase::BeginReadObject(std::string_view key)
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto it = object->find(key);

        if (it == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        if (!it->second.is_object())
        {
            return InvalidValue(key);
        }

        m_ObjectPath.push_back({ std::string{ key }, std::nullopt });
        return {};
    }

    ArchiveResult GlazeArchiveBase::EndObject()
    {
        if (m_ObjectPath.empty())
        {
            return { ArchiveErrorCode::InvalidOperation, {} };
        }

        m_ObjectPath.pop_back();
        return {};
    }

    ArchiveResult GlazeArchiveBase::GetArraySize(std::string_view key, std::size_t& size) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto it = object->find(key);

        if (it == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        const auto* elements = it->second.get_if<Value::array_t>();

        if (!elements)
        {
            return InvalidValue(key);
        }

        size = elements->size();
        return {};
    }

    ArchiveResult GlazeArchiveBase::BeginWriteObjectElement(std::string_view key)
    {
        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        auto it = object->find(key);

        if (it == object->end())
        {
            Value candidate;
            candidate.data = Value::array_t{};
            it = object->emplace(std::string{ key }, std::move(candidate)).first;
        }

        auto* elements = it->second.get_if<Value::array_t>();

        if (!elements)
        {
            return InvalidValue(key);
        }

        Value element;
        element.data = Value::object_t{};

        const std::size_t index = elements->size();
        elements->push_back(std::move(element));
        m_ObjectPath.push_back({ std::string{ key }, index });
        return {};
    }

    ArchiveResult GlazeArchiveBase::BeginReadObjectElement(std::string_view key, std::size_t index)
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto it = object->find(key);

        if (it == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        const auto* elements = it->second.get_if<Value::array_t>();

        if (!elements)
        {
            return InvalidValue(key);
        }

        if (index >= elements->size())
        {
            return { ArchiveErrorCode::IndexOutOfRange, std::string{ key } };
        }

        if (!(*elements)[index].is_object())
        {
            return InvalidValue(key);
        }

        m_ObjectPath.push_back({ std::string{ key }, index });
        return {};
    }
#pragma endregion


#pragma region Write
    ArchiveResult GlazeArchiveBase::WriteValue(std::string_view key, const ArchiveScalarValue& value)
    {
        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        Value candidate;
        const auto result = std::visit([key, &candidate](const auto& source)
            {
                return EncodeScalar(key, source, candidate);
            }, value);

        if (!result)
        {
            return result;
        }

        object->insert_or_assign(std::string{ key }, std::move(candidate));
        return {};
    }

    ArchiveResult GlazeArchiveBase::WriteArrayValues(std::string_view key, ArchiveScalarType, const std::vector<ArchiveScalarValue>& values)
    {
        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        Value candidate;
        candidate.data = Value::array_t{};

        auto& elements = candidate.get_array();
        elements.reserve(values.size());

        for (const auto& value : values)
        {
            Value element;
            const auto result = std::visit([key, &element](const auto& source)
                {
                    return EncodeScalar(key, source, element);
                }, value);

            if (!result)
            {
                return result;
            }

            elements.push_back(std::move(element));
        }

        object->insert_or_assign(std::string{ key }, std::move(candidate));
        return {};
    }

    ArchiveResult GlazeArchiveBase::WriteMapValues(std::string_view key, ArchiveScalarType, ArchiveScalarType, const std::vector<ArchiveScalarMapEntry>& entries)
    {
        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        Value candidate;
        candidate.data = Value::array_t{};
        auto& elements = candidate.get_array();
        elements.reserve(entries.size());

        for (std::size_t index = 0; index < entries.size(); ++index)
        {
            const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
            Value encodedKey;
            Value encodedValue;

            const auto keyResult = std::visit([&path, &encodedKey](const auto& source)
                {
                    return EncodeScalar(path + ".Key", source, encodedKey);
                }, entries[index].Key);

            if (!keyResult)
            {
                return keyResult;
            }

            const auto valueResult = std::visit([&path, &encodedValue](const auto& source)
                {
                    return EncodeScalar(path + ".Value", source, encodedValue);
                }, entries[index].Value);

            if (!valueResult)
            {
                return valueResult;
            }

            Value entry;
            entry.data = Value::object_t{};
            entry.get_object().emplace("Key", std::move(encodedKey));
            entry.get_object().emplace("Value", std::move(encodedValue));
            elements.push_back(std::move(entry));
        }

        object->insert_or_assign(std::string{ key }, std::move(candidate));
        return {};
    }

    ArchiveResult GlazeArchiveBase::WriteObjectValue(std::string_view key, const ObjectWriter& writeObject)
    {
        if (!FindObject(m_Root, m_ObjectPath))
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        ObjectArchive temporary;
        const auto result = writeObject(temporary);

        if (!result)
        {
            return result;
        }

        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        object->insert_or_assign(std::string{ key }, std::move(temporary.m_Root));
        return {};
    }
    ArchiveResult GlazeArchiveBase::WriteObjectArrayValues(
        std::string_view key, std::size_t size, const ObjectArrayWriter& writeObject)
    {
        if (!FindObject(m_Root, m_ObjectPath))
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        Value candidate;
        candidate.data = Value::array_t{};

        auto& elements = candidate.get_array();
        elements.reserve(size);

        for (std::size_t index = 0; index < size; ++index)
        {
            ObjectArchive temporary;
            const auto result = writeObject(temporary, index);

            if (!result)
            {
                return result;
            }

            elements.push_back(std::move(temporary.m_Root));
        }

        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        object->insert_or_assign(std::string{ key }, std::move(candidate));
        return {};
    }
    ArchiveResult GlazeArchiveBase::WriteObjectMapValues(std::string_view key, std::size_t size, ArchiveScalarType,
        const ObjectMapKeyWriter& writeKey, const ObjectArrayWriter& writeValue)
    {
        if (!FindObject(m_Root, m_ObjectPath))
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        Value candidate;
        candidate.data = Value::array_t{};
        auto& elements = candidate.get_array();
        elements.reserve(size);

        for (std::size_t index = 0; index < size; ++index)
        {
            const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
            ArchiveScalarValue keySource;
            const auto keyResult = writeKey(index, keySource);

            if (!keyResult)
            {
                return keyResult;
            }

            Value encodedKey;
            const auto encodingResult = std::visit([&path, &encodedKey](const auto& source)
                {
                    return EncodeScalar(path + ".Key", source, encodedKey);
                }, keySource);

            if (!encodingResult)
            {
                return encodingResult;
            }

            ObjectArchive temporary;
            const auto valueResult = writeValue(temporary, index);

            if (!valueResult)
            {
                return valueResult;
            }

            Value entry;
            entry.data = Value::object_t{};
            entry.get_object().emplace("Key", std::move(encodedKey));
            entry.get_object().emplace("Value", std::move(temporary.m_Root));
            elements.push_back(std::move(entry));
        }

        auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        object->insert_or_assign(std::string{ key }, std::move(candidate));
        return {};
    }
#pragma endregion

#pragma region Read
    ArchiveResult GlazeArchiveBase::ReadValue(std::string_view key, ArchiveScalarType, ArchiveScalarValue& value) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto it = object->find(key);

        if (it == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        return ExtractScalarValue(key, it->second, value);
    }

    ArchiveResult GlazeArchiveBase::ExtractScalarValue(std::string_view key, const Value& source, ArchiveScalarValue& value)
    {
        if (const auto* stored = source.get_if<bool>())
        {
            value = *stored;
            return {};
        }

        if (const auto* stored = source.get_if<std::int64_t>())
        {
            value = *stored;
            return {};
        }

        if (const auto* stored = source.get_if<std::uint64_t>())
        {
            value = *stored;
            return {};
        }

        if (const auto* stored = source.get_if<double>())
        {
            value = *stored;
            return {};
        }

        if (const auto* stored = source.get_if<std::string>())
        {
            value = *stored;
            return {};
        }

        return InvalidValue(key);
    }

    ArchiveResult GlazeArchiveBase::ReadArrayValues(std::string_view key, ArchiveScalarType, std::vector<ArchiveScalarValue>& values) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto it = object->find(key);

        if (it == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        const auto* elements = it->second.get_if<Value::array_t>();

        if (!elements)
        {
            return InvalidValue(key);
        }

        std::vector<ArchiveScalarValue> candidate;
        candidate.reserve(elements->size());

        for (const auto& source : *elements)
        {
            ArchiveScalarValue element;
            const auto result = ExtractScalarValue(key, source, element);

            if (!result)
            {
                return result;
            }

            candidate.push_back(std::move(element));
        }

        values.swap(candidate);
        return {};
    }

    ArchiveResult GlazeArchiveBase::GetMapSize(std::string_view key, std::size_t& size) const
    {
        return GetArraySize(key, size);
    }

    ArchiveResult GlazeArchiveBase::FindMapEntry(std::string_view key, std::size_t index, const Value::object_t*& entry) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto it = object->find(key);

        if (it == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        const auto* elements = it->second.get_if<Value::array_t>();

        if (!elements)
        {
            return InvalidValue(key);
        }

        const auto path = std::string{ key } + "[" + std::to_string(index) + "]";

        if (index >= elements->size())
        {
            return { ArchiveErrorCode::IndexOutOfRange, path };
        }

        const auto* candidate = (*elements)[index].get_if<Value::object_t>();

        if (!candidate)
        {
            return InvalidValue(path);
        }

        if (candidate->find("Key") == candidate->end())
        {
            return { ArchiveErrorCode::MissingField, path + ".Key" };
        }

        if (candidate->find("Value") == candidate->end())
        {
            return { ArchiveErrorCode::MissingField, path + ".Value" };
        }

        entry = candidate;
        return {};
    }

    ArchiveResult GlazeArchiveBase::ReadMapKey(std::string_view key, std::size_t index, ArchiveScalarType, ArchiveScalarValue& value) const
    {
        const Value::object_t* entry = nullptr;
        const auto result = FindMapEntry(key, index, entry);

        if (!result)
        {
            return result;
        }

        const auto path = std::string{ key } + "[" + std::to_string(index) + "].Key";
        return ExtractScalarValue(path, entry->find("Key")->second, value);
    }

    ArchiveResult GlazeArchiveBase::ReadMapValue(std::string_view key, std::size_t index, ArchiveScalarType, ArchiveScalarValue& value) const
    {
        const Value::object_t* entry = nullptr;
        const auto result = FindMapEntry(key, index, entry);

        if (!result)
        {
            return result;
        }

        const auto path = std::string{ key } + "[" + std::to_string(index) + "].Value";
        return ExtractScalarValue(path, entry->find("Value")->second, value);
    }
    ArchiveResult GlazeArchiveBase::ReadObjectValue(std::string_view key, const ObjectReader& readObject) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto found = object->find(key);

        if (found == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        if (!found->second.is_object())
        {
            return InvalidValue(key);
        }

        ObjectArchive temporary;
        temporary.m_Root = found->second;
        return readObject(temporary);
    }
    ArchiveResult GlazeArchiveBase::ReadObjectArrayValues(std::string_view key, const ObjectArrayReader& readObject) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto found = object->find(key);

        if (found == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        const auto* elements = found->second.get_if<Value::array_t>();

        if (!elements)
        {
            return InvalidValue(key);
        }

        for (std::size_t index = 0; index < elements->size(); ++index)
        {
            const auto& source = (*elements)[index];

            if (!source.is_object())
            {
                const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
                return InvalidValue(path);
            }

            ObjectArchive temporary;
            temporary.m_Root = source;

            const auto result = readObject(temporary, index);

            if (!result)
            {
                return result;
            }
        }

        return {};
    }
    ArchiveResult GlazeArchiveBase::ReadObjectMapValues(std::string_view key, ArchiveScalarType,
        const ObjectMapKeyReader& readKey, const ObjectArrayReader& readValue) const
    {
        const auto* object = FindObject(m_Root, m_ObjectPath);

        if (!object)
        {
            return { ArchiveErrorCode::InvalidOperation, std::string{ key } };
        }

        const auto found = object->find(key);

        if (found == object->end())
        {
            return { ArchiveErrorCode::MissingField, std::string{ key } };
        }

        const auto* elements = found->second.get_if<Value::array_t>();

        if (!elements)
        {
            return InvalidValue(key);
        }

        for (std::size_t index = 0; index < elements->size(); ++index)
        {
            const auto path = std::string{ key } + "[" + std::to_string(index) + "]";
            const auto* entry = (*elements)[index].get_if<Value::object_t>();

            if (!entry)
            {
                return InvalidValue(path);
            }

            const auto keyField = entry->find("Key");
            const auto valueField = entry->find("Value");

            if (keyField == entry->end())
            {
                return { ArchiveErrorCode::MissingField, path + ".Key" };
            }

            if (valueField == entry->end())
            {
                return { ArchiveErrorCode::MissingField, path + ".Value" };
            }

            if (!valueField->second.is_object())
            {
                return InvalidValue(path + ".Value");
            }

            ArchiveScalarValue keySource;
            const auto extractionResult = ExtractScalarValue(path + ".Key", keyField->second, keySource);

            if (!extractionResult)
            {
                return extractionResult;
            }

            const auto keyResult = readKey(index, keySource);

            if (!keyResult)
            {
                return keyResult;
            }

            ObjectArchive temporary;
            temporary.m_Root = valueField->second;
            const auto valueResult = readValue(temporary, index);

            if (!valueResult)
            {
                return valueResult;
            }
        }

        return {};
    }
#pragma endregion


}