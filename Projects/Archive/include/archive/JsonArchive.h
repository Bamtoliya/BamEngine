#pragma once

#include <archive/GlazeArchiveBase.h>

#include <cstddef>
#include <string>
#include <string_view>

namespace archive
{
    class JsonArchive final : public GlazeArchiveBase
    {
    public:
        JsonArchive() = default;

        [[nodiscard]] ArchiveResult Parse(std::string_view json);
        [[nodiscard]] ArchiveResult ToJson(std::string& json) const;

    protected:
        ArchiveResult EncodeDocument(std::string& document) const override;
        ArchiveResult DecodeDocument(std::string_view document) override;

    private:
        static constexpr std::size_t IndentationWidth = 4;
        static constexpr std::size_t NumericElementsPerLine = 4;

        static void AppendIndentation(std::string& output, std::size_t depth);

        template<typename T>
        static bool AppendEncodedValue(const T& value, std::string& output);

        static bool AppendFormattedValue(const Value& value, std::string& output, std::size_t depth);
    };
}