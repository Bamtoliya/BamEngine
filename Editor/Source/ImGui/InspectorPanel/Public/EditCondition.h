#pragma once

#include <reflection/EnumInfo.h>
#include <reflection/ObjectView.h>
#include <reflection/PropertyInfo.h>
#include <reflection/Registry.h>
#include <reflection/TypeInfo.h>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace Editor
{
    struct EditConditionResult
    {
        // 조건을 해석하지 못하면 속성을 숨기지 않는다.
        bool Met = true;
        std::string Error;
    };

    // EDITCONDITION 식을 평가한다.
    //   식 := 그리고 ('||' 그리고)*        그리고 := 단항 ('&&' 단항)*
    //   단항 := '!' 단항 | '(' 식 ')' | 항  항 := bool속성 | 속성 ('==' | '!=') 값 | 플래그속성 '&' 플래그
    class EditConditionParser
    {
    public:
        EditConditionParser(std::string_view text, const reflection::TypeInfo& type,
            const reflection::ObjectView& object, const reflection::Registry* registry)
            : m_Text(text), m_Type(type), m_Object(object), m_Registry(registry)
        {
        }

        EditConditionResult Run()
        {
            const bool met = ParseOr();
            SkipSpaces();
            if (m_Error.empty() && m_Position != m_Text.size()) Fail("unexpected text");
            if (!m_Error.empty()) return { true, std::move(m_Error) };
            return { met, {} };
        }

    private:
        // 오류를 찾을 수 있도록 결과가 정해진 뒤에도 나머지 항을 모두 읽는다.
        bool ParseOr()
        {
            bool met = ParseAnd();
            while (m_Error.empty() && Consume("||"))
            {
                const bool right = ParseAnd();
                met = met || right;
            }
            return met;
        }

        bool ParseAnd()
        {
            bool met = ParseUnary();
            while (m_Error.empty() && Consume("&&"))
            {
                const bool right = ParseUnary();
                met = met && right;
            }
            return met;
        }

        bool ParseUnary()
        {
            if (Consume("!")) return !ParseUnary();
            if (!Consume("(")) return ParseTerm();

            const bool met = ParseOr();
            if (m_Error.empty() && !Consume(")")) Fail("missing ')'");
            return met;
        }

        bool ParseTerm()
        {
            const auto name = ReadToken();
            if (name.empty()) return Fail("expected a property name");
            const auto* property = m_Type.FindProperty(name);
            if (!property) return Fail("unknown property '" + std::string(name) + "'");
            const auto read = property->TryReadValue(m_Object);
            if (!read) return Fail("cannot read '" + std::string(name) + "'");

            if (Consume("==")) return Compare(read.Value, ReadToken());
            if (Consume("!=")) return !Compare(read.Value, ReadToken());
            SkipSpaces();
            if (!m_Text.substr(m_Position).starts_with("&&") && Consume("&")) return HasFlag(read.Value, ReadToken());

            const auto* value = read.Value.Get<bool>();
            return value ? *value : Fail("'" + std::string(name) + "' is not a bool");
        }

        bool Compare(const reflection::ValueView& value, std::string_view literal)
        {
            if (literal.empty()) return Fail("expected a value");
            if (const auto* enumeration = m_Registry ? m_Registry->FindEnum(value) : nullptr)
            {
                const auto current = enumeration->ReadValue(value);
                const auto* entry = enumeration->FindEntry(EnumeratorName(literal));
                if (!current || !entry) return Fail("unknown enumerator '" + std::string(literal) + "'");
                return std::visit([](auto left, auto right) { return std::cmp_equal(left, right); }, entry->Value, *current);
            }
            if (const auto* boolean = value.Get<bool>())
            {
                if (literal == "true" || literal == "1") return *boolean;
                if (literal == "false" || literal == "0") return !*boolean;
                return Fail("invalid bool value '" + std::string(literal) + "'");
            }

            const auto number = ReadNumber<short, unsigned short, int, unsigned int, long, unsigned long, long long,
                unsigned long long, signed char, unsigned char, float, double>(value);
            double expected = 0.0;
            const auto* end = literal.data() + literal.size();
            const auto parsed = std::from_chars(literal.data(), end, expected);
            if (!number || parsed.ec != std::errc{} || parsed.ptr != end)
                return Fail("cannot compare with '" + std::string(literal) + "'");
            return *number == expected;
        }

        bool HasFlag(const reflection::ValueView& value, std::string_view literal)
        {
            const auto* enumeration = m_Registry ? m_Registry->FindEnum(value) : nullptr;
            if (!enumeration) return Fail("'&' requires a registered enum");
            const auto current = enumeration->ReadValue(value);
            const auto* entry = enumeration->FindEntry(EnumeratorName(literal));
            if (!current || !entry) return Fail("unknown flag '" + std::string(literal) + "'");

            const auto bits = [](const reflection::EnumValue& number)
                {
                    return std::visit([](auto raw) { return static_cast<std::uint64_t>(raw); }, number);
                };
            return (bits(*current) & bits(entry->Value)) != 0;
        }

        template<typename... Types>
        static std::optional<double> ReadNumber(const reflection::ValueView& value)
        {
            std::optional<double> number;
            const auto read = [&]<typename T>(std::type_identity<T>)
                {
                    if (const auto* current = value.Get<T>()) number = static_cast<double>(*current);
                };
            (read(std::type_identity<Types>{}), ...);
            return number;
        }

        static std::string_view EnumeratorName(std::string_view literal)
        {
            // ELightType::Spot 같은 형태에서 식별자("Spot")만 추출
            const auto colon = literal.rfind("::");
            return colon == std::string_view::npos ? literal : literal.substr(colon + 2);
        }

        std::string_view ReadToken()
        {
            SkipSpaces();
            const auto begin = m_Position;
            while (m_Position < m_Text.size() && IsTokenCharacter(m_Text[m_Position])) ++m_Position;
            return m_Text.substr(begin, m_Position - begin);
        }

        static bool IsTokenCharacter(char value)
        {
            return (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z') || (value >= '0' && value <= '9') ||
                value == '_' || value == ':' || value == '.' || value == '-';
        }

        bool Consume(std::string_view symbol)
        {
            SkipSpaces();
            if (!m_Text.substr(m_Position).starts_with(symbol)) return false;
            m_Position += symbol.size();
            return true;
        }

        void SkipSpaces()
        {
            const auto next = m_Text.find_first_not_of(" \t\r\n", m_Position);
            m_Position = next == std::string_view::npos ? m_Text.size() : next;
        }

        bool Fail(std::string error)
        {
            if (m_Error.empty()) m_Error = std::move(error);
            return true;
        }

        std::string_view m_Text;
        std::size_t m_Position = 0;
        const reflection::TypeInfo& m_Type;
        reflection::ObjectView m_Object;
        const reflection::Registry* m_Registry = nullptr;
        std::string m_Error;
    };

    inline EditConditionResult EvaluateEditCondition(const reflection::PropertyInfo& property, const reflection::TypeInfo& type,
        const reflection::ObjectView& object, const reflection::Registry* registry)
    {
        const auto* metadata = property.Metadata.Find("EditCondition");
        const auto* text = metadata ? std::get_if<std::string>(metadata) : nullptr;
        if (!text || text->empty()) return {};
        return EditConditionParser(*text, type, object, registry).Run();
    }
}
