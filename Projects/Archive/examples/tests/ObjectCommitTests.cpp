#include "TestSupport.h"

#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>

#include <cstdint>
#include <string>
#include <vector>

namespace archive_tests
{
    namespace
    {
        struct Shared
        {
            std::string Label = "Initial";
            std::vector<std::int32_t> Values{ 1 };
            bool operator==(const Shared&) const = default;
        };
        struct Left : virtual Shared
        {
            std::int32_t LeftValue = 3;
            bool operator==(const Left&) const = default;
        };
        struct Right : virtual Shared
        {
            std::int32_t RightValue = 5;
            bool operator==(const Right&) const = default;
        };
        struct Diamond : Left, Right { bool operator==(const Diamond&) const = default; };
        struct MoveCommit
        {
            std::int32_t Value = 0;
            bool Moved = false;
            MoveCommit() = default;
            MoveCommit(const MoveCommit&) = default;
            MoveCommit(MoveCommit&&) = default;
            MoveCommit& operator=(const MoveCommit&) = delete;
            MoveCommit& operator=(MoveCommit&& other) noexcept
            {
                Value = other.Value;
                Moved = true;
                return *this;
            }
        };

        template<typename Archive>
        bool CheckCommit()
        {
            Archive writer;
            Diamond expected;
            expected.Label = "Shared base content";
            expected.Values = { -7, 19 };
            expected.LeftValue = 23;
            expected.RightValue = 29;
            const auto write = writer.WriteObject("Diamond", expected,
                [](archive::IArchive& ar, const Diamond& value) -> archive::ArchiveResult
                {
                    auto result = ar.Write("Label", value.Label);
                    if (result) result = ar.WriteArray("Values", value.Values);
                    if (result) result = ar.Write("Left", value.LeftValue);
                    if (result) result = ar.Write("Right", value.RightValue);
                    return result;
                });
            Archive reader;
            Diamond actual;
            std::string document;
            if (!write || !writer.Encode(document) || !reader.Decode(document))
            {
                return false;
            }
            const auto read = reader.ReadObject("Diamond", actual,
                [](archive::IArchive& ar, Diamond& value) -> archive::ArchiveResult
                {
                    auto result = ar.Read("Label", value.Label);
                    if (result) result = ar.ReadArray("Values", value.Values);
                    if (result) result = ar.Read("Left", value.LeftValue);
                    if (result) result = ar.Read("Right", value.RightValue);
                    return result;
                });
            if (!read || actual != expected)
            {
                return false;
            }
            const auto failure = reader.ReadObject("Diamond", actual,
                [](archive::IArchive&, Diamond& value) -> archive::ArchiveResult
                {
                    value.Label = "Discard this candidate";
                    value.Values.clear();
                    return { archive::ArchiveErrorCode::InvalidValue, "Label" };
                });
            if (failure.Code != archive::ArchiveErrorCode::InvalidValue || actual != expected)
            {
                return false;
            }
            MoveCommit moveTarget;
            return reader.ReadObject("Diamond", moveTarget,
                [](archive::IArchive& ar, MoveCommit& value) { return ar.Read("Right", value.Value); }) &&
                moveTarget.Value == expected.RightValue && moveTarget.Moved;
        }
    }

    int CheckObjectCommit()
    {
        return CheckCommit<archive::JsonArchive>() && CheckCommit<archive::BeveArchive>()
            ? 0 : Fail("Object commit lost shared virtual base data or changed assignment-only compatibility.");
    }
}
