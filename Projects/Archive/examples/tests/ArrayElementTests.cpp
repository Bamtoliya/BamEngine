#include "TestSupport.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace archive_tests
{
    namespace
    {
        template<typename Archive>
        bool CheckElements()
        {
            using Code = archive::ArchiveErrorCode;
            Archive writer;
            const auto write = writer.WriteArrayElements("Mixed", 4,
                [](archive::IArchive& entry, std::string_view key, std::size_t index) -> archive::ArchiveResult
                {
                    if (index == 0)
                    {
                        return entry.Write(key, std::int32_t{ 7 });
                    }
                    if (index == 1)
                    {
                        return entry.WriteArray(key, std::vector<std::int32_t>{ 1, 2 });
                    }
                    if (index == 2)
                    {
                        return entry.WriteObject(key, std::int32_t{ 3 },
                            [](archive::IArchive& object, std::int32_t value)
                            {
                                return object.Write("Number", value);
                            });
                    }
                    return entry.WriteArrayElements(key, 1,
                        [](archive::IArchive& child, std::string_view field, std::size_t)
                        {
                            return child.Write(field, std::string{ "Nested" });
                        });
                });
            Archive reader;
            std::string document;
            if (!write || !writer.Encode(document) || !reader.Decode(document))
            {
                return false;
            }
            std::size_t calls = 0;
            const auto read = reader.ReadArrayElements("Mixed",
                [&](archive::IArchive& entry, std::string_view key, std::size_t index) -> archive::ArchiveResult
                {
                    ++calls;
                    if (index == 0)
                    {
                        std::int32_t value = 0;
                        const auto result = entry.Read(key, value);
                        return !result || value == 7 ? result : archive::ArchiveResult{ Code::InvalidValue, {} };
                    }
                    if (index == 1)
                    {
                        std::vector<std::int32_t> values;
                        const auto result = entry.ReadArray(key, values);
                        return !result || values == std::vector<std::int32_t>{ 1, 2 }
                            ? result : archive::ArchiveResult{ Code::InvalidValue, {} };
                    }
                    if (index == 2)
                    {
                        const auto begin = entry.BeginReadObject(key);
                        if (!begin)
                        {
                            return begin;
                        }
                        std::int32_t number = 0;
                        const auto result = entry.Read("Number", number);
                        const auto end = entry.EndObject();
                        return !result || number == 3 ? (result ? end : result)
                            : archive::ArchiveResult{ Code::InvalidValue, {} };
                    }
                    return entry.ReadArrayElements(key,
                        [](archive::IArchive& child, std::string_view field, std::size_t) -> archive::ArchiveResult
                        {
                            std::string value;
                            const auto result = child.Read(field, value);
                            return !result || value == "Nested" ? result
                                : archive::ArchiveResult{ Code::InvalidValue, {} };
                        });
                });
            std::string after;
            if (!read || calls != 4 || !reader.Encode(after) || after != document)
            {
                return false;
            }
            for (int failure = 0; failure < 4; ++failure)
            {
                const auto result = writer.WriteArrayElements("Mixed", 2,
                    [failure](archive::IArchive& entry, std::string_view key,
                        std::size_t index) -> archive::ArchiveResult
                    {
                        if (index == 0)
                        {
                            return entry.Write(key, std::int32_t{ 99 });
                        }
                        if (failure == 0)
                        {
                            return {}; // Missing the requested field.
                        }
                        if (failure == 1)
                        {
                            const auto value = entry.Write(key, std::int32_t{ 1 });
                            return value ? entry.Write("Extra", std::int32_t{ 2 }) : value;
                        }
                        if (failure == 2)
                        {
                            return entry.BeginWriteObject(key); // Unbalanced object depth.
                        }
                        return { Code::InvalidValue, std::string{ key } + "[2].Number" };
                    });
                const auto expected = failure == 3 ? Code::InvalidValue : Code::InvalidOperation;
                const auto path = failure == 3 ? "Mixed[1][2].Number" : "Mixed[1]";
                if (result.Code != expected || result.Field != path || !writer.Encode(after) || after != document)
                {
                    return false;
                }
            }
            const auto unbalanced = reader.ReadArrayElements("Mixed",
                [](archive::IArchive& entry, std::string_view key, std::size_t index) -> archive::ArchiveResult
                {
                    if (index == 2)
                    {
                        return entry.BeginReadObject(key);
                    }
                    return {};
                });
            const auto failed = reader.ReadArrayElements("Mixed",
                [](archive::IArchive&, std::string_view key, std::size_t) -> archive::ArchiveResult
                {
                    return { Code::InvalidValue, std::string{ key } + "[1]" };
                });
            const auto absent = reader.ReadArrayElements("Absent",
                [](archive::IArchive&, std::string_view, std::size_t) -> archive::ArchiveResult { return {}; });
            return unbalanced.Code == Code::InvalidOperation && unbalanced.Field == "Mixed[2]" &&
                failed.Code == Code::InvalidValue && failed.Field == "Mixed[0][1]" &&
                absent.Code == Code::MissingField && absent.Field == "Absent" &&
                writer.WriteArrayElements("Mixed", 0, {}).Code == Code::InvalidOperation &&
                reader.ReadArrayElements("Mixed", {}).Code == Code::InvalidOperation &&
                reader.GetObjectDepth() == 0 && reader.Encode(after) && after == document;
        }
    }

    int CheckArrayElements()
    {
        return CheckElements<archive::JsonArchive>() && CheckElements<archive::BeveArchive>()
            ? 0 : Fail("Arbitrary array values, callback validation or document preservation failed.");
    }
}
