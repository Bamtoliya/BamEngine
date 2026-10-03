#include "MetadataParser.h"

#include <clang/AST/ASTContext.h>
#include <clang/Lex/Lexer.h>
#include <clang/Lex/LiteralSupport.h>

#include <llvm/ADT/ArrayRef.h>

#include <charconv>
#include <cstdint>
#include <system_error>
#include <unordered_set>
#include <utility>

namespace reflection_codegen
{
    MetadataParseResult ParseMetadata(
        std::span<const clang::Token> tokens,
        clang::ASTContext& context
    )
    {
        MetadataParseResult result;
        std::unordered_set<std::string> keys;
        std::size_t position = 0;

        const auto fail = [&](std::string message)
            {
                result.Error = std::move(message);
                result.Entries.clear();

                if (!tokens.empty())
                {
                    const std::size_t index =
                        position < tokens.size()
                        ? position
                        : tokens.size() - 1;

                    result.ErrorLocation = tokens[index].getLocation();
                }
            };

        const auto spelling = [&](const clang::Token& token)
            {
                return clang::Lexer::getSpelling(
                    token,
                    context.getSourceManager(),
                    context.getLangOpts()
                );
            };

        const auto expect = [&](
            clang::tok::TokenKind kind,
            const char* message
            )
            {
                if (position >= tokens.size() ||
                    !tokens[position].is(kind))
                {
                    fail(message);
                    return false;
                }

                ++position;
                return true;
            };

        const auto parseString = [&](std::string& output)
            {
                const std::size_t start = position;

                while (position < tokens.size() &&
                    tokens[position].is(clang::tok::string_literal))
                {
                    ++position;
                }

                if (start == position)
                {
                    fail("Expected an ordinary string literal");
                    return false;
                }

                clang::StringLiteralParser parser(
                    llvm::ArrayRef<clang::Token>(
                        tokens.data() + start,
                        position - start
                    ),
                    context.getSourceManager(),
                    context.getLangOpts(),
                    context.getTargetInfo()
                );

                if (parser.hadError ||
                    !parser.isOrdinary() ||
                    !parser.getUDSuffix().empty())
                {
                    fail("Invalid or unsupported string literal");
                    return false;
                }

                output = parser.GetString().str();
                return true;
            };

        while (position < tokens.size())
        {
            if (spelling(tokens[position]) != "META")
            {
                fail("Expected META(key, value)");
                return result;
            }

            ++position;

            if (!expect(clang::tok::l_paren, "Expected '(' after META"))
            {
                return result;
            }

            std::string key;

            if (!parseString(key))
            {
                return result;
            }

            if (key.empty())
            {
                fail("Metadata key must not be empty");
                return result;
            }

            if (!keys.insert(key).second)
            {
                fail("Duplicate metadata key: " + key);
                return result;
            }

            if (!expect(clang::tok::comma, "Expected ',' after key"))
            {
                return result;
            }

            if (position >= tokens.size())
            {
                fail("Expected a metadata value");
                return result;
            }

            reflection::MetadataValue value;

            if (tokens[position].is(clang::tok::string_literal))
            {
                std::string text;

                if (!parseString(text))
                {
                    return result;
                }

                value = std::move(text);
            }
            else if (spelling(tokens[position]) == "true" ||
                spelling(tokens[position]) == "false")
            {
                value = spelling(tokens[position]) == "true";
                ++position;
            }
            else
            {
                std::string number;

                if (tokens[position].is(clang::tok::minus))
                {
                    number = "-";
                    ++position;
                }
                else if (tokens[position].is(clang::tok::plus))
                {
                    ++position;
                }

                if (position >= tokens.size() ||
                    !tokens[position].is(clang::tok::numeric_constant))
                {
                    fail("Expected a string, bool, or numeric literal");
                    return result;
                }

                const std::string digits = spelling(tokens[position]);
                number += digits;
                ++position;

                const char* begin = number.data();
                const char* end = begin + number.size();

                if (digits.find_first_of(".eE") != std::string::npos)
                {
                    double parsed = 0.0;

                    const auto conversion = std::from_chars(
                        begin,
                        end,
                        parsed,
                        std::chars_format::general
                    );

                    if (conversion.ec != std::errc{} ||
                        conversion.ptr != end)
                    {
                        fail("Invalid or out-of-range floating literal");
                        return result;
                    }

                    value = parsed;
                }
                else
                {
                    // C++의 8진수 표기와 혼동되지 않도록 거부합니다.
                    if (digits.size() > 1 && digits.front() == '0')
                    {
                        fail("Only decimal integers without leading zeros are supported");
                        return result;
                    }

                    std::int64_t parsed = 0;

                    const auto conversion = std::from_chars(
                        begin,
                        end,
                        parsed,
                        10
                    );

                    if (conversion.ec != std::errc{} ||
                        conversion.ptr != end)
                    {
                        fail("Invalid or out-of-range integer literal");
                        return result;
                    }

                    value = parsed;
                }
            }

            if (!expect(clang::tok::r_paren, "Expected ')' after value"))
            {
                return result;
            }

            result.Entries.push_back(
                reflection::MetadataEntry{
                    std::move(key),
                    std::move(value)
                }
            );

            if (position == tokens.size())
            {
                break;
            }

            if (!expect(clang::tok::comma, "Expected ',' between META entries"))
            {
                return result;
            }

            if (position == tokens.size())
            {
                fail("Trailing comma is not allowed");
                return result;
            }
        }

        return result;
    }
}