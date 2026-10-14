#include <archive/JsonArchive.h>

#include <glaze/json.hpp>

#include <algorithm>
#include <utility>

namespace archive
{
    ArchiveResult JsonArchive::Parse(std::string_view json)
    {
        return Decode(json);
    }

    ArchiveResult JsonArchive::ToJson(std::string& json) const
    {
        return Encode(json);
    }

    ArchiveResult JsonArchive::EncodeDocument(std::string& document) const
    {
        if (!AppendFormattedValue(m_Root, document, 0))
        {
            return { ArchiveErrorCode::SerializationFailed, {} };
        }

        return {};
    }

    ArchiveResult JsonArchive::DecodeDocument(std::string_view document)
    {
        std::string text{ document };

        if (glz::validate_json(text))
        {
            return { ArchiveErrorCode::InvalidDocument, {} };
        }

        Value candidate;

        if (glz::read_json(candidate, text) || !candidate.is_object())
        {
            return { ArchiveErrorCode::InvalidDocument, {} };
        }

        m_Root = std::move(candidate);
        return {};
    }

    void JsonArchive::AppendIndentation(std::string& output, std::size_t depth)
    {
        output.append(depth * IndentationWidth, ' ');
    }

    template<typename T>
    bool JsonArchive::AppendEncodedValue(const T& value, std::string& output)
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

    bool JsonArchive::AppendFormattedValue(const Value& value, std::string& output, std::size_t depth)
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
}