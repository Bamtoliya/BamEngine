#pragma once

#include <archive/GlazeArchiveBase.h>
#include <glaze/json.hpp>

#include <string>
#include <string_view>
#include <utility>
#include <algorithm>
#include <cstddef>

namespace archive
{
    class JsonArchive final : public GlazeArchiveBase
    {
    public:
        JsonArchive() = default;

        [[nodiscard]] ArchiveResult Parse(std::string_view json)
        {
            if (GetObjectDepth() != 0)
            {
                return { ArchiveErrorCode::InvalidOperation, {} };
            }

            std::string text{ json };

            if (glz::validate_json(text))
            {
                return { ArchiveErrorCode::InvalidJson, {} };
            }

            Value candidate;

            if (glz::read_json(candidate, text) || !candidate.is_object())
            {
                return { ArchiveErrorCode::InvalidJson, {} };
            }

            m_Root = std::move(candidate);
            return {};
        }

        [[nodiscard]] ArchiveResult ToJson(std::string& json) const
        {
            if (GetObjectDepth() != 0)
            {
                return { ArchiveErrorCode::InvalidOperation, {} };
            }

            std::string candidate;

            if (!AppendFormattedValue(m_Root, candidate, 0))
            {
                return { ArchiveErrorCode::SerializationFailed, {} };
            }

            json = std::move(candidate);
            return {};
        }

    protected:
        ArchiveResult EncodeDocument(std::string& document) const override
        {
            return ToJson(document);
        }

        ArchiveResult DecodeDocument(std::string_view document) override
        {
            return Parse(document);
        }
    private:
    private:
        static constexpr std::size_t IndentationWidth = 4;
        static constexpr std::size_t NumericElementsPerLine = 4;

        static void AppendIndentation(std::string& output, std::size_t depth)
        {
            output.append(depth * IndentationWidth, ' ');
        }

        template<typename T>
        static bool AppendEncodedValue(const T& value, std::string& output)
        {
            constexpr auto options = glz::opt_true <
                glz::opts{ .prettify = true, .indentation_width = 4, .new_lines_in_arrays = false },
                glz::escape_control_characters_opt_tag{} > ;

            std::string encoded;

            if (glz::write<options>(value, encoded))
            {
                return false;
            }

            output += encoded;
            return true;
        }

        static bool AppendFormattedValue(const Value& value, std::string& output, std::size_t depth)
        {
            if (const auto* object = value.get_if<Value::object_t>())
            {
                output += '{';
                std::size_t index = 0;

                for (const auto& [key, member] : *object)
                {
                    output += index++ == 0 ? "\n" : ",\n";
                    AppendIndentation(output, depth + 1);

                    if (!AppendEncodedValue(key, output))
                    {
                        return false;
                    }

                    output += ": ";

                    if (!AppendFormattedValue(member, output, depth + 1))
                    {
                        return false;
                    }
                }

                if (!object->empty())
                {
                    output += '\n';
                    AppendIndentation(output, depth);
                }

                output += '}';
                return true;
            }

            if (const auto* elements = value.get_if<Value::array_t>())
            {
                const bool numericOnly = std::all_of(elements->begin(), elements->end(), [](const Value& element)
                    {
                        return element.is_number();
                    });

                const bool containsContainers = std::any_of(elements->begin(), elements->end(), [](const Value& element)
                    {
                        return element.is_object() || element.is_array();
                    });

                const bool wrapNumbers = numericOnly && elements->size() > NumericElementsPerLine;

                if (!wrapNumbers && !containsContainers)
                {
                    return AppendEncodedValue(value, output);
                }

                output += "[\n";

                for (std::size_t index = 0; index < elements->size(); ++index)
                {
                    const bool startsLine = !wrapNumbers || index % NumericElementsPerLine == 0;

                    if (index != 0)
                    {
                        output += startsLine ? ",\n" : ", ";
                    }

                    if (startsLine)
                    {
                        AppendIndentation(output, depth + 1);
                    }

                    if (!AppendFormattedValue((*elements)[index], output, depth + 1))
                    {
                        return false;
                    }
                }

                output += '\n';
                AppendIndentation(output, depth);
                output += ']';
                return true;
            }

            return AppendEncodedValue(value, output);
        }
    };
}